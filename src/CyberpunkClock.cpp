#include "MatrixClock.h"
#include "StarWarsHud.h"
#include "IPSClock.h"

// Shared with Backlights: 0 = Night City Kiroshi HUD, 1 = Sandevistan Overdrive (57-59s), 2 = Relic Malfunction Glitch (00-02s)
uint8_t cyberpunkState = 0;

namespace {

enum CyberPal {
    P_WHITE = 0,
    P_BLACK,
    P_BG_VOID,      // Deep Night City carbon black (#08090F)
    P_BG_GRID,      // Dark Kiroshi HUD grid (#141828)
    P_CYBER_YELLOW, // Signature Cyberpunk 2077 Yellow (#FCEE09)
    P_YELLOW_DIM,   // Olive-yellow border accent (#8A8000)
    P_NEON_CYAN,    // Electric Kiroshi Cyan (#00F0FF)
    P_CYAN_DARK,    // Deep neural circuit cyan (#005868)
    P_GLITCH_RED,   // Arasaka / Relic Crimson (#FF003C)
    P_RED_DARK,     // Dark crimson shadow (#580018)
    P_SANDE_GREEN,  // Sandevistan Emerald Afterimage (#00FF88)
    P_CHROME,       // Samurai Oni metallic silver (#C8D4E0)
    P_COUNT
};

const uint16_t CYBER_RAW[P_COUNT] = {
    0xFFFF,                 // P_WHITE
    0x0000,                 // P_BLACK
    swC565(8, 9, 16),       // P_BG_VOID
    swC565(20, 26, 42),     // P_BG_GRID
    swC565(252, 238, 10),   // P_CYBER_YELLOW
    swC565(138, 128, 0),    // P_YELLOW_DIM
    swC565(0, 240, 255),    // P_NEON_CYAN
    swC565(0, 88, 104),     // P_CYAN_DARK
    swC565(255, 0, 60),     // P_GLITCH_RED
    swC565(88, 0, 24),      // P_RED_DARK
    swC565(0, 255, 136),    // P_SANDE_GREEN
    swC565(200, 212, 224)   // P_CHROME
};

// Night City Kiroshi Optics HUD frame, PCB neural traces, acid rain & Relic Glitch Tear
__attribute__((noinline)) void drawCyberpunkBackdrop(TFT_eSprite &s, int screen, unsigned long nowMs,
                                                     uint8_t state, uint16_t stateMs, const uint16_t *pal) {
    s.fillSprite(pal[state == 2 ? P_RED_DARK : P_BG_VOID]);

    // 1. Top & Bottom Signature Cyberpunk 2077 Yellow Chamfered Header/Footer Bars
    // Top angled bar (y = 0..17) with 45-degree corner notch
    s.fillRect(0, 0, TFT_WIDTH, 16, pal[state == 2 ? P_GLITCH_RED : P_CYBER_YELLOW]);
    s.fillTriangle(115, 0, 134, 0, 134, 16, pal[P_BG_VOID]);
    // Technical barcode & Kiroshi header text in black inside the yellow bar
    for (int bx = 6; bx < 34; bx += 3) {
        s.drawFastVLine(bx, 3, 10, pal[P_BLACK]);
    }
    swDrawTxt(s, state == 2 ? "RELIC ERR!" : (state == 1 ? "SANDEVISTAN" : "NC // 2077"),
              74, 4, 1, 1, TC_DATUM, pal[P_BLACK]);

    // 2. Neural PCB Circuit Traces & Kiroshi Corner Crosshairs
    for (int cy = 44; cy <= 188; cy += 48) {
        s.drawFastHLine(8, cy, 22, pal[P_BG_GRID]);
        s.drawLine(30, cy, 38, cy + 8, pal[P_BG_GRID]);
        s.fillCircle(8, cy, 2, pal[P_CYAN_DARK]);

        s.drawFastHLine(105, cy + 12, 22, pal[P_BG_GRID]);
        s.drawLine(105, cy + 12, 97, cy + 4, pal[P_BG_GRID]);
        s.fillCircle(127, cy + 12, 2, pal[P_RED_DARK]);
    }
    // Left/Right vertical HUD bracket rails
    s.drawFastVLine(4, 22, 196, pal[P_GLITCH_RED]);
    s.drawFastVLine(130, 22, 196, pal[P_NEON_CYAN]);

    // 3. Diagonal Neon Acid Rain falling across Night City
    for (int r = 0; r < 12; ++r) {
        uint32_t h = (uint32_t)(screen + 4) * 2654435761u ^ (uint32_t)(r + 9) * 2246822519u;
        int ry = (int)(((nowMs * 95u) / 1000u + (h & 0xFF)) % 200u) + 20;
        int rx = 10 + ((int)((h >> 8) % 114));
        uint16_t rCol = pal[(r & 1) ? P_CYAN_DARK : P_RED_DARK];
        s.drawLine(rx, ry, rx - 3, ry + 9, rCol);
    }

    // 4. Bottom Cyberpunk Yellow/Cyan Status Strip (y = 224..239)
    s.drawFastHLine(0, 224, TFT_WIDTH, pal[P_NEON_CYAN]);
    s.fillRect(0, 226, TFT_WIDTH, 14, pal[P_BG_GRID]);
    for (int sx = 8; sx < 128; sx += 12) {
        s.drawLine(sx, 239, sx + 6, 227, pal[P_CYBER_YELLOW]);
    }

    // 5. Sec 00-02 (State 2): RELIC MALFUNCTION Digital Glitch Bars tearing across all 6 screens!
    if (state == 2) {
        for (int g = 0; g < 5; ++g) {
            int gy = 30 + ((int)(nowMs / 25u + g * 43 + screen * 29) % 175);
            int gh = 3 + (g & 3);
            uint16_t gCol = pal[(g & 1) ? P_NEON_CYAN : P_GLITCH_RED];
            s.fillRect(0, gy, TFT_WIDTH, gh, gCol);
        }
    }
    (void)stateMs;
}

// Cyberpunk 2077 Digit with Chromatic Aberration (Red/Cyan RGB Split) + Sandevistan Trail + PCB Cuts!
__attribute__((noinline)) void drawCyberpunkDigit(TFT_eSprite &s, int digit, unsigned long nowMs,
                                                  uint8_t state, const uint16_t *pal) {
    if (digit < 0 || digit > 9) return;
    static const uint8_t segMask[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
    static const uint8_t segs[7][5] = {
        {26, 30, 83, 16, 1},  // A
        {93, 36, 16, 76, 0},  // B
        {93, 114, 16, 76, 0}, // C
        {26, 180, 83, 16, 1}, // D
        {26, 114, 16, 76, 0}, // E
        {26, 36, 16, 76, 0},  // F
        {28, 105, 79, 16, 1}  // G
    };

    uint8_t m = segMask[digit];

    // Periodic glitch jitter offset (intense during State 2 Relic Malfunction, subtle micro-glitch normally)
    int glitchDX = 0;
    if (state == 2) {
        glitchDX = ((int)(nowMs / 35u) % 7) - 3;
    } else if (((nowMs / 180u) & 15) == 0) {
        glitchDX = 2;
    }

    // Pass 1: Sandevistan Emerald Afterimage Ghost (active during Sec 57-59 State 1)
    if (state == 1) {
        for (int b = 0; b < 7; ++b) {
            if (!(m & (1 << b))) continue;
            swFillChamfer(s, segs[b][0] - 8, segs[b][1] - 3, segs[b][2], segs[b][3], 3, pal[P_SANDE_GREEN]);
        }
    }

    // Pass 2: RGB Chromatic Aberration Split (Left shift = Glitch Red #FF003C, Right shift = Neon Cyan #00F0FF)
    int shift = (state >= 1) ? 5 : 3;
    for (int b = 0; b < 7; ++b) {
        if (!(m & (1 << b))) continue;
        int x = segs[b][0] + glitchDX, y = segs[b][1], w = segs[b][2], h = segs[b][3];
        swFillChamfer(s, x - shift, y - 1, w, h, 3, pal[P_GLITCH_RED]);
        swFillChamfer(s, x + shift, y + 2, w, h, 3, pal[P_NEON_CYAN]);
    }

    // Pass 3: Main Cyberpunk Yellow Body (#FCEE09) with Black PCB Tech Cutouts & White Edge Highlight
    for (int b = 0; b < 7; ++b) {
        if (!(m & (1 << b))) continue;
        int x = segs[b][0] + glitchDX, y = segs[b][1], w = segs[b][2], h = segs[b][3], horiz = segs[b][4];
        uint16_t mainCol = pal[(state == 1) ? P_NEON_CYAN : P_CYBER_YELLOW];
        swFillChamfer(s, x, y, w, h, 3, mainCol);

        // Iconic Cyberpunk 2077 black circuit trace line + 45-degree notch carved inside each segment!
        if (horiz) {
            s.drawFastHLine(x + 6, y + 3, w - 12, pal[P_WHITE]);
            s.drawFastHLine(x + 10, y + h / 2, w - 20, pal[P_BLACK]);
            s.fillRect(x + w / 2 - 2, y + h / 2 - 2, 5, 5, pal[P_GLITCH_RED]);
        } else {
            s.drawFastVLine(x + 3, y + 6, h - 12, pal[P_WHITE]);
            s.drawFastVLine(x + w / 2, y + 10, h - 20, pal[P_BLACK]);
            s.fillRect(x + w / 2 - 2, y + h / 2 - 2, 5, 5, pal[P_GLITCH_RED]);
        }
    }
}

// Screen 2 (Center): SAMURAI Cyber-Oni Mask + Sandevistan Neural Spine + Date Block
__attribute__((noinline)) void drawCyberpunkCenter(TFT_eSprite &s, struct tm &now, suseconds_t subsec,
                                                   unsigned long nowMs, uint8_t state, const uint16_t *pal) {
    const int cx = 67, cy = 88;

    // 1. Background Cyan/Red Neural Hex Halo
    s.drawCircle(cx, cy, 44, pal[P_CYAN_DARK]);
    s.drawLine(cx - 46, cy, cx + 46, cy, pal[P_RED_DARK]);

    // 2. SAMURAI Cybernetic Oni Mask (y = 32..142)
    // Twin curved chrome/crimson Oni Horns (top-left & top-right)
    s.fillTriangle(cx - 18, cy - 28, cx - 38, cy - 54, cx - 28, cy - 22, pal[P_GLITCH_RED]);
    s.fillTriangle(cx + 18, cy - 28, cx + 38, cy - 54, cx + 28, cy - 22, pal[P_GLITCH_RED]);
    s.drawLine(cx - 18, cy - 28, cx - 38, cy - 54, pal[P_WHITE]);
    s.drawLine(cx + 18, cy - 28, cx + 38, cy - 54, pal[P_WHITE]);

    // Angular Chrome/Black Oni Cranium & Cheekplates
    swFillChamfer(s, cx - 28, cy - 28, 57, 46, 8, pal[P_BG_GRID]);
    s.drawRect(cx - 26, cy - 26, 53, 42, pal[P_CHROME]);
    // Yellow Samurai forehead crest
    s.fillTriangle(cx - 12, cy - 26, cx + 12, cy - 26, cx, cy - 10, pal[P_CYBER_YELLOW]);

    // Blazing Electric Cyan Kiroshi Cyber-Eyes (slanted menacingly)
    s.fillTriangle(cx - 24, cy - 12, cx - 5, cy - 5, cx - 18, cy - 1, pal[P_NEON_CYAN]);
    s.fillTriangle(cx + 24, cy - 12, cx + 5, cy - 5, cx + 18, cy - 1, pal[P_NEON_CYAN]);
    s.drawLine(cx - 22, cy - 9, cx - 7, cy - 5, pal[P_WHITE]);
    s.drawLine(cx + 7, cy - 5, cx + 22, cy - 9, pal[P_WHITE]);

    // Mechanical Oni Jaw & Chrome Fangs (y = cy + 8..cy + 42)
    s.fillTriangle(cx - 26, cy + 12, cx + 26, cy + 12, cx, cy + 44, pal[P_GLITCH_RED]);
    s.drawTriangle(cx - 26, cy + 12, cx + 26, cy + 12, cx, cy + 44, pal[P_CYBER_YELLOW]);
    // 4 sharp white/chrome Oni tusks
    s.fillTriangle(cx - 18, cy + 14, cx - 12, cy + 14, cx - 15, cy + 28, pal[P_WHITE]);
    s.fillTriangle(cx + 12, cy + 14, cx + 18, cy + 14, cx + 15, cy + 28, pal[P_WHITE]);
    s.fillTriangle(cx - 8,  cy + 26, cx - 2,  cy + 26, cx - 5,  cy + 15, pal[P_CHROME]);
    s.fillTriangle(cx + 2,  cy + 26, cx + 8,  cy + 26, cx + 5,  cy + 15, pal[P_CHROME]);

    // 3. Sandevistan Vertebrae Nodes below the Oni Mask (y = 140..176)
    bool pulseOn = (subsec < 500000L);
    for (int v = 0; v < 3; ++v) {
        int vy = 142 + v * 11;
        int vw = 28 - v * 5;
        uint16_t vCol = pal[(state == 1) ? P_SANDE_GREEN : (pulseOn ? P_NEON_CYAN : P_GLITCH_RED)];
        swFillChamfer(s, cx - vw, vy, vw * 2, 7, 2, vCol);
        s.fillRect(cx - 3, vy + 1, 7, 5, pal[P_WHITE]);
    }

    // 4. Bottom Cyberpunk Yellow Chamfered Date Block (y = 182..220)
    swFillChamfer(s, 14, 182, 107, 38, 5, pal[P_CYBER_YELLOW]);
    swFillChamfer(s, 17, 185, 101, 32, 4, pal[P_BLACK]);
    s.fillRect(14, 182, 8, 38, pal[P_GLITCH_RED]);
    char dateStr[6] = {
        char('0' + now.tm_mday / 10), char('0' + now.tm_mday % 10), '.',
        char('0' + now.tm_mon / 10), char('0' + now.tm_mon % 10), 0
    };
    swDrawTxt(s, dateStr, cx + 3, 201, 4, 1, MC_DATUM, pal[P_CYBER_YELLOW]);
    (void)nowMs;
}

// Screen 5 (Rightmost): Kiroshi Optics v3.4 Scanner + Cyberdeck RAM Slots + Relic Biochip + Weather
__attribute__((noinline)) void drawCyberpunkHudScreen(TFT_eSprite &s, WeatherService &weather,
                                                      struct tm &now, unsigned long nowMs,
                                                      uint8_t state, const uint16_t *pal) {
    // 1. TOP (y = 22..56): Cyberdeck RAM Units (6 glowing cyan RAM blocks)
    swFillChamfer(s, 10, 22, 115, 34, 4, pal[P_BG_GRID]);
    s.drawRect(10, 22, 115, 34, pal[P_NEON_CYAN]);
    swDrawTxt(s, "CYBERDECK RAM", 67, 26, 1, 1, TC_DATUM, pal[P_NEON_CYAN]);

    int litRam = 1 + (now.tm_sec * 5) / 59;
    for (int r = 0; r < 6; ++r) {
        int rx = 16 + r * 17;
        bool on = (r < litRam) || (state >= 1);
        uint16_t rCol = pal[on ? (state == 2 ? P_GLITCH_RED : P_NEON_CYAN) : P_CYAN_DARK];
        s.fillRect(rx, 39, 13, 11, rCol);
        if (on) s.drawFastHLine(rx + 1, 40, 11, pal[P_WHITE]);
    }

    // 2. MIDDLE (y = 62..156): Night City Atmospheric Scanner (Weather + Temp + HI/LO)
    swFillChamfer(s, 10, 62, 115, 94, 5, pal[P_BG_GRID]);
    s.drawRect(10, 62, 115, 94, pal[P_CYBER_YELLOW]);
    s.fillRect(10, 62, 115, 13, pal[P_CYBER_YELLOW]);
    swDrawTxt(s, "KIROSHI ENV SCAN", 67, 65, 1, 1, TC_DATUM, pal[P_BLACK]);

    char unit = (WeatherService::getUnits().value == "imperial") ? 'F' : 'C';
    float vals[3] = {weather.getNowTemp(), weather.getHigh(5), weather.getLow(5)};
    int ints[3];
    for (int k = 0; k < 3; ++k) ints[k] = (int)(vals[k] + (vals[k] >= 0.0f ? 0.5f : -0.5f));

    char buf[16];
    snprintf(buf, sizeof(buf), "%d%c", ints[0], unit);
    // Chromatic glitch shadow behind temperature
    swDrawTxt(s, buf, 65, 99, 4, 1, MC_DATUM, pal[P_GLITCH_RED]);
    swDrawTxt(s, buf, 67, 98, 4, 1, MC_DATUM, pal[P_CYBER_YELLOW]);

    snprintf(buf, sizeof(buf), "MAX:%d%c", ints[1], unit);
    swDrawTxt(s, buf, 42, 128, 2, 1, MC_DATUM, pal[P_GLITCH_RED]);
    snprintf(buf, sizeof(buf), "MIN:%d%c", ints[2], unit);
    swDrawTxt(s, buf, 96, 128, 2, 1, MC_DATUM, pal[P_NEON_CYAN]);

    swDrawTxt(s, state == 2 ? "RELIC CRITICAL!" : "NIGHT CITY // OK", 67, 143, 1, 1, TC_DATUM,
              pal[state == 2 ? P_WHITE : P_SANDE_GREEN]);

    // 3. BOTTOM (y = 162..220): Relic Biochip Sync Progress Bar & Live Seconds
    swFillChamfer(s, 10, 162, 115, 56, 4, pal[P_BG_GRID]);
    s.drawRect(10, 162, 115, 56, pal[P_GLITCH_RED]);
    snprintf(buf, sizeof(buf), "BIOCHIP SYNC :%02d", now.tm_sec);
    swDrawTxt(s, buf, 67, 168, 1, 1, TC_DATUM, pal[P_WHITE]);

    // Angled segmented Cyberpunk progress bar (x = 16..118, y = 184..208)
    int fillW = (now.tm_sec * 98) / 59;
    if (state >= 1) fillW = 98;
    s.drawRect(16, 184, 102, 22, pal[P_CYBER_YELLOW]);
    if (fillW > 0) {
        s.fillRect(18, 186, fillW, 18, pal[state == 2 ? P_GLITCH_RED : (state == 1 ? P_SANDE_GREEN : P_CYBER_YELLOW)]);
        // Diagonal black hazard stripes inside progress bar
        for (int hx = 22; hx < 18 + fillW - 6; hx += 10) {
            s.drawLine(hx, 203, hx + 6, 186, pal[P_BLACK]);
            s.drawLine(hx + 1, 203, hx + 7, 186, pal[P_BLACK]);
        }
    }
    (void)nowMs;
}

} // namespace

void MatrixClock::renderCyberpunk(IPSClock &clock, TimeSync &time, WeatherService &weather) {
    unsigned long nowMs = millis();
    if (nowMs - lastFrame < 45) return;
    lastFrame = nowMs;
    ++frame;

    struct tm now;
    suseconds_t subsecond;
    time.getLocalTime(&now, &subsecond);

    uint16_t stateMs = 0;
    if (now.tm_sec <= 2) {
        cyberpunkState = 2;
        stateMs = (uint16_t)(now.tm_sec * 1000u + (uint32_t)(subsecond / 1000));
    } else if (now.tm_sec >= 57) {
        cyberpunkState = 1;
        stateMs = (uint16_t)((now.tm_sec - 57) * 1000u + (uint32_t)(subsecond / 1000));
    } else {
        cyberpunkState = 0;
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
    swDimPalette(CYBER_RAW, pal, P_COUNT);

    for (int i = 0; i < 6; ++i) {
        drawCyberpunkBackdrop(sprite, i, nowMs, cyberpunkState, stateMs, pal);

        if (i == 2) {
            drawCyberpunkCenter(sprite, now, subsecond, nowMs, cyberpunkState, pal);
        } else if (i == 5) {
            drawCyberpunkHudScreen(sprite, weather, now, nowMs, cyberpunkState, pal);
        } else {
            bool blankHour = (i == 0 && hour < 10 && !IPSClock::getLeadingZero().value);
            if (!blankHour) {
                drawCyberpunkDigit(sprite, digits[i], nowMs, cyberpunkState, pal);
            }
        }

        tfts->chip_select.setDigit(SW_SCREENS[i]);
        sprite.pushSprite(0, 0);
    }
    tfts->release();
}
