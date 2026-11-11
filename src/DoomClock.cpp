#include "MatrixClock.h"
#include "StarWarsHud.h"
#include "IPSClock.h"

// Shared with Backlights: 0 = Normal E1M1 patrol, 1 = Red Berserk Alert (56-59s), 2 = BFG 9000 Green Plasma Blast (00-02s)
uint8_t doomState = 0;

namespace {

enum DoomPal {
    P_WHITE = 0,
    P_BLACK,
    P_RUST_DARK,    // UAC rusted tech-base steel
    P_RUST_MID,     // UAC bulkhead brown-grey
    P_BRONZE,       // Metallic bronze bezel
    P_PANEL_BG,     // Deep recessed HUD background
    P_HAZARD_YEL,   // Industrial hazard yellow
    P_DOOM_RED,     // Classic DOOM font red
    P_DOOM_ORANGE,  // DOOM font bright orange gradient
    P_DOOM_YELLOW,  // DOOM font golden-yellow highlight
    P_SKIN_BASE,    // Doomguy skin base tone
    P_SKIN_SHADOW,  // Doomguy cheek/jaw shadow
    P_HAIR_BROWN,   // Doomguy brown crew-cut
    P_BLOOD_RED,    // Doomguy blood & wound red
    P_BFG_GREEN,    // BFG 9000 emerald plasma ball
    P_BFG_AURA,     // BFG green ambient aura
    P_ARMOR_BLUE,   // MegaArmor / SoulSphere cyan
    P_COUNT
};

const uint16_t DOOM_RAW[P_COUNT] = {
    0xFFFF,                 // P_WHITE
    0x0000,                 // P_BLACK
    swC565(36, 32, 28),     // P_RUST_DARK
    swC565(76, 68, 60),     // P_RUST_MID
    swC565(148, 126, 96),   // P_BRONZE
    swC565(18, 14, 12),     // P_PANEL_BG
    swC565(255, 204, 0),    // P_HAZARD_YEL
    swC565(210, 20, 16),    // P_DOOM_RED
    swC565(255, 110, 18),   // P_DOOM_ORANGE
    swC565(255, 220, 40),   // P_DOOM_YELLOW
    swC565(235, 175, 140),  // P_SKIN_BASE
    swC565(170, 110, 85),   // P_SKIN_SHADOW
    swC565(85, 45, 25),     // P_HAIR_BROWN
    swC565(185, 10, 14),    // P_BLOOD_RED
    swC565(40, 255, 75),    // P_BFG_GREEN
    swC565(10, 95, 30),     // P_BFG_AURA
    swC565(30, 180, 240)    // P_ARMOR_BLUE
};

// E1M1 Hangar / Phobos industrial toxic waste tech-base backdrop
__attribute__((noinline)) void drawDoomBackdrop(TFT_eSprite &s, int screen, unsigned long nowMs,
                                                uint8_t state, uint16_t stateMs, const uint16_t *pal) {
    s.fillSprite(pal[P_PANEL_BG]);

    // 1. Rusted industrial tech-base steel wall (riveted panels)
    for (int y = 0; y < 192; y += 4) {
        s.drawFastHLine(0, y, TFT_WIDTH, pal[(y % 8 == 0) ? P_RUST_DARK : P_RUST_MID]);
    }
    s.drawRect(2, 2, TFT_WIDTH - 4, 190, pal[P_BRONZE]);
    s.drawRect(4, 4, TFT_WIDTH - 8, 186, pal[P_BLACK]);

    // Top warning chevron stripe (hazard stripes)
    for (int hx = 6; hx < 128; hx += 16) {
        s.fillTriangle(hx, 6, hx + 8, 6, hx + 16, 18, pal[P_HAZARD_YEL]);
        s.fillTriangle(hx, 6, hx + 8, 18, hx, 18, pal[P_BLACK]);
    }

    // 2. Toxic Slime / Nukage bubbling at the bottom of the screens (y = 192..239)
    s.fillRect(0, 192, TFT_WIDTH, 48, pal[P_BLACK]);
    s.drawFastHLine(0, 192, TFT_WIDTH, pal[P_BFG_GREEN]);
    for (int ny = 196; ny < 240; ny += 3) {
        int shift = (int)((nowMs / 45u + ny * 7 + screen * 23) % 20) - 10;
        uint16_t nCol = pal[((ny / 3) & 1) ? P_BFG_GREEN : P_BFG_AURA];
        s.drawFastHLine(14 + shift, ny, 107 - abs(shift), nCol);
    }
    // Nukage bubbling sparks
    for (int b = 0; b < 4; ++b) {
        int bx = 16 + ((screen * 31 + b * 29 + (int)(nowMs / 80u)) % 103);
        int by = 230 - ((int)(nowMs / 50u + b * 19) % 32);
        s.drawPixel(bx, by, pal[P_BFG_GREEN]);
        s.drawPixel(bx + 1, by, pal[P_WHITE]);
    }

    // 3. Sec 56-59 (State 1): BERSERK PACK RED ALERT! Red flash on bulkheads
    if (state == 1) {
        bool flash = ((nowMs / 90u + screen) & 1) != 0;
        if (flash) {
            s.drawRect(0, 0, TFT_WIDTH, TFT_HEIGHT, pal[P_DOOM_RED]);
            s.drawRect(1, 1, TFT_WIDTH - 2, TFT_HEIGHT - 2, pal[P_DOOM_ORANGE]);
        }
    }

    // 4. Sec 00-02 (State 2): BFG 9000 GREEN PLASMA BALL tearing across all 6 screens!
    if (state == 2) {
        int gBallX = (int)((stateMs * 1150u) / 3000u) - 60;
        int scrX0 = screen * 150;
        int localX = gBallX - scrX0;
        int trailStart = localX - 260;
        if (localX >= 0 && trailStart < TFT_WIDTH) {
            int x0 = trailStart < 0 ? 0 : trailStart;
            int x1 = localX >= TFT_WIDTH ? TFT_WIDTH - 1 : localX;
            // Giant green BFG plasma shockwave trail across the center (y = 80..130)
            for (int dy = -16; dy <= 16; ++dy) {
                uint16_t tCol = pal[(abs(dy) <= 4) ? P_WHITE : (abs(dy) <= 10 ? P_BFG_GREEN : P_BFG_AURA)];
                s.drawFastHLine(x0, 105 + dy, x1 - x0 + 1, tCol);
            }
        }
        // Plasma ball head with chaotic green lightning arcs
        if (localX >= -25 && localX < TFT_WIDTH + 25) {
            s.fillCircle(localX, 105, 22, pal[P_BFG_AURA]);
            s.fillCircle(localX, 105, 14, pal[P_BFG_GREEN]);
            s.fillCircle(localX, 105, 6, pal[P_WHITE]);
            for (int sp = 0; sp < 6; ++sp) {
                int ax = localX + ((int)((nowMs / 20u + sp * 27) % 50) - 25);
                int ay = 105 + ((int)((nowMs / 25u + sp * 19) % 50) - 25);
                s.drawLine(localX, 105, ax, ay, pal[P_WHITE]);
            }
        }
    }
}

// DOOM 1994 Heavy Chamfered Red/Yellow Gradient Digit
__attribute__((noinline)) void drawDoomDigit(TFT_eSprite &s, int digit, unsigned long nowMs,
                                             uint8_t state, const uint16_t *pal) {
    if (digit < 0 || digit > 9) return;
    static const uint8_t segMask[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
    static const uint8_t segs[7][5] = {
        {26, 26, 83, 16, 1},  // A
        {93, 32, 16, 76, 0},  // B
        {93, 110, 16, 76, 0}, // C
        {26, 174, 83, 16, 1}, // D
        {26, 110, 16, 76, 0}, // E
        {26, 32, 16, 76, 0},  // F
        {28, 101, 79, 16, 1}  // G
    };

    uint8_t m = segMask[digit];

    // Pass 1: Heavy black drop shadow
    for (int b = 0; b < 7; ++b) {
        if (!(m & (1 << b))) continue;
        swFillChamfer(s, segs[b][0] + 4, segs[b][1] + 4, segs[b][2], segs[b][3], 4, pal[P_BLACK]);
    }

    // Pass 2: Classic DOOM Font Red-to-Yellow Gradient
    for (int b = 0; b < 7; ++b) {
        if (!(m & (1 << b))) continue;
        int x = segs[b][0], y = segs[b][1], w = segs[b][2], h = segs[b][3], horiz = segs[b][4];
        uint16_t col = pal[(state == 2) ? P_BFG_GREEN : ((y < 80) ? P_DOOM_YELLOW : ((y < 140) ? P_DOOM_ORANGE : P_DOOM_RED))];
        swFillChamfer(s, x, y, w, h, 4, col);

        // Chiseled metallic highlight on upper-left edges
        if (horiz) {
            s.drawFastHLine(x + 4, y + 2, w - 8, pal[(state == 2) ? P_WHITE : P_DOOM_YELLOW]);
            s.drawFastHLine(x + 4, y + h - 2, w - 8, pal[P_BLACK]);
        } else {
            s.drawFastVLine(x + 2, y + 4, h - 8, pal[(state == 2) ? P_WHITE : P_DOOM_YELLOW]);
            s.drawFastVLine(x + w - 2, y + 4, h - 8, pal[P_BLACK]);
        }
    }
    (void)nowMs;
}

// Screen 2 (Center): Animated DOOMGUY FACE Status Bar + Health Eye-Darts + Date
__attribute__((noinline)) void drawDoomguyFaceCenter(TFT_eSprite &s, struct tm &now, suseconds_t subsec,
                                                     unsigned long nowMs, uint8_t state, const uint16_t *pal) {
    const int cx = 67;

    // Header DOOM logo banner
    swDrawTxt(s, state == 2 ? "BFG 9000!" : (state == 1 ? "BERSERK!" : "DOOM // 1994"),
              cx, 16, 1, 1, TC_DATUM, pal[(state == 2) ? P_BFG_GREEN : P_DOOM_YELLOW]);
    s.drawFastHLine(20, 27, 95, pal[P_DOOM_RED]);

    // Heavy riveted steel bezel around the Doomguy face box (x = 22..112, y = 35..155)
    swFillChamfer(s, 22, 35, 91, 120, 8, pal[P_RUST_MID]);
    s.drawRect(24, 37, 87, 116, pal[P_BRONZE]);
    s.fillRect(26, 39, 83, 112, pal[P_PANEL_BG]);

    // Doomguy Face (cx = 67, cy = 95)
    const int fy = 92;

    // Brown Crew Cut Hair
    s.fillCircle(cx, fy - 18, 26, pal[P_HAIR_BROWN]);
    s.fillRect(cx - 25, fy - 18, 51, 14, pal[P_HAIR_BROWN]);

    // Square muscular jaw & ears
    swFillChamfer(s, cx - 24, fy - 12, 49, 44, 6, pal[P_SKIN_BASE]);
    s.fillCircle(cx - 24, fy - 2, 5, pal[P_SKIN_BASE]); // Left ear
    s.fillCircle(cx + 24, fy - 2, 5, pal[P_SKIN_BASE]); // Right ear

    // Hairline spikes on forehead
    for (int hx = -18; hx <= 18; hx += 6) {
        s.fillTriangle(cx + hx - 3, fy - 14, cx + hx + 3, fy - 14, cx + hx, fy - 9, pal[P_HAIR_BROWN]);
    }

    // Shadow on cheekbones and chin cleft
    s.drawFastHLine(cx - 18, fy + 22, 37, pal[P_SKIN_SHADOW]);
    s.drawFastVLine(cx, fy + 24, 6, pal[P_SKIN_SHADOW]);

    // Angry angled eyebrows
    s.drawLine(cx - 18, fy - 8, cx - 4, fy - 4, pal[P_HAIR_BROWN]);
    s.drawLine(cx - 18, fy - 7, cx - 4, fy - 3, pal[P_HAIR_BROWN]);
    s.drawLine(cx + 18, fy - 8, cx + 4, fy - 4, pal[P_HAIR_BROWN]);
    s.drawLine(cx + 18, fy - 7, cx + 4, fy - 3, pal[P_HAIR_BROWN]);

    // Doomguy Eyes (dart left/right periodically!)
    int lookDir = ((nowMs / 2200u) % 3) - 1; // -1 = looking left, 0 = center, +1 = right
    if (state >= 1) lookDir = ((nowMs / 180u) & 1) ? 1 : -1; // Rapid panic darting
    s.fillRect(cx - 16, fy - 4, 11, 7, pal[P_WHITE]);
    s.fillRect(cx + 6, fy - 4, 11, 7, pal[P_WHITE]);
    s.fillRect(cx - 13 + lookDir * 2, fy - 3, 5, 5, pal[(state == 1) ? P_DOOM_RED : P_BLACK]);
    s.fillRect(cx + 9 + lookDir * 2, fy - 3, 5, 5, pal[(state == 1) ? P_DOOM_RED : P_BLACK]);

    // Broad nose
    s.drawLine(cx, fy - 4, cx - 3, fy + 8, pal[P_SKIN_SHADOW]);
    s.drawFastHLine(cx - 4, fy + 8, 9, pal[P_SKIN_SHADOW]);

    // Gritted teeth / Bloody mouth (grimace)
    s.fillRect(cx - 12, fy + 14, 25, 6, pal[P_BLACK]);
    for (int tx = cx - 10; tx <= cx + 10; tx += 4) {
        s.drawFastVLine(tx, fy + 14, 6, pal[P_WHITE]);
    }
    // Blood dripping from mouth & forehead in high-intensity states!
    if (now.tm_sec > 30 || state >= 1) {
        s.fillRect(cx - 8, fy + 19, 4, 7, pal[P_BLOOD_RED]);
        s.fillRect(cx + 14, fy - 10, 3, 11, pal[P_BLOOD_RED]);
    }

    // Bottom Date Display formatted as DOOM Status Bar Ammo Readout (y = 172..218)
    swFillChamfer(s, 16, 172, 103, 44, 4, pal[P_RUST_MID]);
    s.fillRect(18, 174, 99, 40, pal[P_PANEL_BG]);
    swDrawTxt(s, "DATE", cx, 177, 1, 1, TC_DATUM, pal[P_DOOM_YELLOW]);
    char dateStr[6] = {
        char('0' + now.tm_mday / 10), char('0' + now.tm_mday % 10), '.',
        char('0' + now.tm_mon / 10), char('0' + now.tm_mon % 10), 0
    };
    swDrawTxt(s, dateStr, cx, 198, 4, 1, MC_DATUM, pal[P_DOOM_ORANGE]);
    (void)subsec;
}

// Screen 5 (Rightmost): Classic DOOM Status Bar (100% HEALTH, 100% ARMOR, AMMO, Weather)
__attribute__((noinline)) void drawDoomHudScreen(TFT_eSprite &s, WeatherService &weather,
                                                 struct tm &now, unsigned long nowMs,
                                                 uint8_t state, const uint16_t *pal) {
    // 1. HEALTH DISPLAY (y = 18..78)
    swFillChamfer(s, 10, 18, 115, 62, 5, pal[P_RUST_MID]);
    s.fillRect(12, 20, 111, 58, pal[P_PANEL_BG]);
    swDrawTxt(s, "HEALTH", 67, 23, 1, 1, TC_DATUM, pal[P_DOOM_YELLOW]);

    char hpBuf[16];
    int hp = 100 - (now.tm_sec * 35) / 59;
    if (state == 1) hp = 200; // MegaHealth Berserk
    snprintf(hpBuf, sizeof(hpBuf), "%d%%", hp);
    swDrawTxt(s, hpBuf, 67, 52, 4, 1, MC_DATUM, pal[(hp > 50) ? P_DOOM_ORANGE : P_DOOM_RED]);

    // 2. ARMOR & WEATHER DISPLAY (y = 86..156)
    swFillChamfer(s, 10, 86, 115, 72, 5, pal[P_RUST_MID]);
    s.fillRect(12, 88, 111, 68, pal[P_PANEL_BG]);
    swDrawTxt(s, "ARMOR / ENV", 67, 91, 1, 1, TC_DATUM, pal[P_ARMOR_BLUE]);

    char unit = (WeatherService::getUnits().value == "imperial") ? 'F' : 'C';
    float vals[3] = {weather.getNowTemp(), weather.getHigh(5), weather.getLow(5)};
    int ints[3];
    for (int k = 0; k < 3; ++k) ints[k] = (int)(vals[k] + (vals[k] >= 0.0f ? 0.5f : -0.5f));

    char buf[16];
    snprintf(buf, sizeof(buf), "%d%c", ints[0], unit);
    swDrawTxt(s, buf, 42, 124, 4, 1, MC_DATUM, pal[P_WHITE]);

    snprintf(buf, sizeof(buf), "HI:%d", ints[1]);
    swDrawTxt(s, buf, 94, 116, 2, 1, MC_DATUM, pal[P_DOOM_YELLOW]);
    snprintf(buf, sizeof(buf), "LO:%d", ints[2]);
    swDrawTxt(s, buf, 94, 134, 2, 1, MC_DATUM, pal[P_ARMOR_BLUE]);

    // 3. AMMO COUNTER (y = 164..232)
    swFillChamfer(s, 10, 164, 115, 68, 5, pal[P_RUST_MID]);
    s.fillRect(12, 166, 111, 64, pal[P_PANEL_BG]);
    swDrawTxt(s, state >= 1 ? "CELLS" : "AMMO", 67, 170, 1, 1, TC_DATUM, pal[P_HAZARD_YEL]);

    snprintf(buf, sizeof(buf), state >= 1 ? "BFG: %02d" : "SHELLS: %02d", now.tm_sec);
    swDrawTxt(s, buf, 67, 202, 2, 1, MC_DATUM, pal[(state == 2) ? P_BFG_GREEN : P_DOOM_ORANGE]);
    (void)nowMs;
}

} // namespace

void MatrixClock::renderDoom(IPSClock &clock, TimeSync &time, WeatherService &weather) {
    unsigned long nowMs = millis();
    if (nowMs - lastFrame < 45) return;
    lastFrame = nowMs;
    ++frame;

    struct tm now;
    suseconds_t subsecond;
    time.getLocalTime(&now, &subsecond);

    uint16_t stateMs = 0;
    if (now.tm_sec <= 2) {
        doomState = 2;
        stateMs = (uint16_t)(now.tm_sec * 1000u + (uint32_t)(subsecond / 1000));
    } else if (now.tm_sec >= 56) {
        doomState = 1;
        stateMs = (uint16_t)((now.tm_sec - 56) * 1000u + (uint32_t)(subsecond / 1000));
    } else {
        doomState = 0;
    }

    int hour = now.tm_hour;
    if (IPSClock::getHourFormat()) {
        hour %= 12;
        if (hour == 0) hour = 12;
    }
    int digits[6] = {hour / 10, hour % 10, -1, now.tm_min / 10, now.tm_min % 10, -1};

    tfts->claim();
    tfts->setDimming(clock.getBrightness());
    tfts->enableAllDisplays();
    TFT_eSprite &sprite = tfts->getSprite();

    uint16_t pal[P_COUNT];
    swDimPalette(DOOM_RAW, pal, P_COUNT);

    for (int i = 0; i < 6; ++i) {
        drawDoomBackdrop(sprite, i, nowMs, doomState, stateMs, pal);

        if (i == 2) {
            drawDoomguyFaceCenter(sprite, now, subsecond, nowMs, doomState, pal);
        } else if (i == 5) {
            drawDoomHudScreen(sprite, weather, now, nowMs, doomState, pal);
        } else {
            bool blankHour = (i == 0 && hour < 10 && !IPSClock::getLeadingZero().value);
            if (!blankHour) {
                drawDoomDigit(sprite, digits[i], nowMs, doomState, pal);
            }
        }

        tfts->chip_select.setDigit(SW_SCREENS[i]);
        sprite.pushSprite(0, 0);
    }
    tfts->release();
}
