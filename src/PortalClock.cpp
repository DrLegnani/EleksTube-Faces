#include "MatrixClock.h"
#include "StarWarsHud.h"
#include "IPSClock.h"

// Shared with Backlights: 0 = Normal Chamber, 1 = GLaDOS Red Warning & Portal Surge (56-59s), 2 = High Energy Pellet & Companion Cube Jump (00-02s)
uint8_t portalState = 0;

namespace {

enum PortalPal {
    P_WHITE = 0,
    P_BLACK,
    P_LAB_WHITE,    // Clean Aperture modular wall panel (#EAEAEA)
    P_LAB_DARK,     // Dark observation chamber panel (#1E222A)
    P_LAB_GREY,     // Heavy concrete border (#78828E)
    P_PORTAL_BLUE,  // Primary Blue Portal (#00A2FF)
    P_BLUE_AURA,    // Blue portal ambient glow (#004C88)
    P_PORTAL_ORANGE,// Primary Orange Portal (#FF7200)
    P_ORANGE_AURA,  // Orange portal ambient glow (#883A00)
    P_TURRET_RED,   // Sentry Turret eye / laser (#FF1824)
    P_CUBE_PINK,    // Companion Cube heart pink (#FF4A88)
    P_HAZARD_YEL,   // Test chamber hazard stripes (#FFD200)
    P_PELLET_CYAN,  // High Energy Pellet core (#88FFFF)
    P_COUNT
};

const uint16_t PORTAL_RAW[P_COUNT] = {
    0xFFFF,                 // P_WHITE
    0x0000,                 // P_BLACK
    swC565(234, 234, 234),  // P_LAB_WHITE
    swC565(30, 34, 42),     // P_LAB_DARK
    swC565(120, 130, 142),  // P_LAB_GREY
    swC565(0, 162, 255),    // P_PORTAL_BLUE
    swC565(0, 76, 136),     // P_BLUE_AURA
    swC565(255, 114, 0),    // P_PORTAL_ORANGE
    swC565(136, 58, 0),     // P_ORANGE_AURA
    swC565(255, 24, 36),    // P_TURRET_RED
    swC565(255, 74, 136),   // P_CUBE_PINK
    swC565(255, 210, 0),    // P_HAZARD_YEL
    swC565(136, 255, 255)   // P_PELLET_CYAN
};

// Aperture Test Chamber clean modular tiles, portal rings & infinite particle streams
__attribute__((noinline)) void drawPortalBackdrop(TFT_eSprite &s, int screen, unsigned long nowMs,
                                                  uint8_t state, uint16_t stateMs, const uint16_t *pal) {
    s.fillSprite(pal[P_LAB_DARK]);

    // 1. Aperture Clean Square Modular Tiles (32x32) on walls
    for (int y = 0; y < TFT_HEIGHT; y += 32) {
        s.drawFastHLine(0, y, TFT_WIDTH, pal[P_BLACK]);
        for (int x = 0; x < TFT_WIDTH; x += 32) {
            s.drawFastVLine(x, y, 32, pal[P_BLACK]);
            s.drawPixel(x + 2, y + 2, pal[P_LAB_GREY]);
        }
    }

    // Top status strip with Test Chamber label
    s.fillRect(0, 0, TFT_WIDTH, 14, pal[P_BLACK]);
    s.drawFastHLine(0, 14, TFT_WIDTH, pal[(screen < 2) ? P_PORTAL_BLUE : ((screen == 2) ? P_LAB_WHITE : P_PORTAL_ORANGE)]);

    // 2. Giant Blue Portal (Screens 0 & 1) and Orange Portal (Screens 3 & 4) Oval Rims!
    bool isBlueSide = (screen < 2);
    bool isOrangeSide = (screen == 3 || screen == 4);

    if (isBlueSide || isOrangeSide) {
        uint16_t pCol = pal[isBlueSide ? P_PORTAL_BLUE : P_PORTAL_ORANGE];
        uint16_t aCol = pal[isBlueSide ? P_BLUE_AURA : P_ORANGE_AURA];
        // Swirling elliptical portal ring on the outer edges
        int pcx = isBlueSide ? (screen == 0 ? 30 : 10) : (screen == 3 ? 125 : 105);
        int pcy = 110;
        int rx = 44, ry = 86;
        for (int r = 0; r < 4; ++r) {
            s.drawCircle(pcx, pcy, ry - r * 6, aCol);
        }
        s.drawCircle(pcx, pcy, ry, pCol);

        // Swirling spatial particles entering Blue Portal and exiting Orange Portal!
        for (int pt = 0; pt < 8; ++pt) {
            int ang = (int)((nowMs / (isBlueSide ? 30u : -30u) + pt * 45) % 360);
            static const int8_t cos8[8] = {30, 21, 0, -21, -30, -21, 0, 21};
            static const int8_t sin8[8] = {0, 21, 30, 21, 0, -21, -30, -21};
            int idx = (ang / 45) % 8;
            int px = pcx + (cos8[idx] * rx) / 30;
            int py = pcy + (sin8[idx] * ry) / 30;
            s.drawPixel(px, py, pal[P_WHITE]);
            s.drawPixel(px + 1, py, pCol);
        }
    }

    // 3. Sec 56-59 (State 1): GLaDOS Neurotoxin / Red Reactor Warning Alarm!
    if (state == 1) {
        bool flash = ((nowMs / 110u + screen) & 1) != 0;
        if (flash) {
            s.drawRect(0, 0, TFT_WIDTH, TFT_HEIGHT, pal[P_TURRET_RED]);
            s.drawRect(1, 1, TFT_WIDTH - 2, TFT_HEIGHT - 2, pal[P_TURRET_RED]);
        }
    }

    // 4. Sec 00-02 (State 2): COMPANION CUBE & HIGH ENERGY PELLET warping between portals!
    if (state == 2) {
        int pelletProg = (int)((stateMs * 1100u) / 3000u);
        int scrX0 = screen * 150;
        int lx = pelletProg - scrX0;
        if (lx >= -20 && lx < TFT_WIDTH + 20) {
            // High Energy bouncing pellet
            s.fillCircle(lx, 110, 14, pal[P_PELLET_CYAN]);
            s.fillCircle(lx, 110, 8, pal[P_WHITE]);
            // Cyan particle stream
            for (int t = 0; t < 18; ++t) {
                s.drawPixel(lx - t * 3, 110 + ((t & 1) ? 2 : -2), pal[P_PORTAL_BLUE]);
            }
        }
    }
}

// Aperture Clean Stencil Digit with Portal Ring Accents
__attribute__((noinline)) void drawPortalDigit(TFT_eSprite &s, int screen, int digit,
                                               unsigned long nowMs, uint8_t state, const uint16_t *pal) {
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

    bool isBlueSide = (screen < 2);
    uint16_t mainCol = pal[isBlueSide ? P_PORTAL_BLUE : P_PORTAL_ORANGE];
    uint16_t auraCol = pal[isBlueSide ? P_BLUE_AURA   : P_ORANGE_AURA];

    uint8_t m = segMask[digit];

    // Pass 1: Portal Energy Ambient Aura
    for (int b = 0; b < 7; ++b) {
        if (!(m & (1 << b))) continue;
        int x = segs[b][0], y = segs[b][1], w = segs[b][2], h = segs[b][3];
        swFillChamfer(s, x - 3, y - 3, w + 6, h + 6, 5, auraCol);
    }

    // Pass 2: Clean Aperture Laboratory White Body + Portal Cyan/Orange Inner Glow
    for (int b = 0; b < 7; ++b) {
        if (!(m & (1 << b))) continue;
        int x = segs[b][0], y = segs[b][1], w = segs[b][2], h = segs[b][3], horiz = segs[b][4];
        swFillChamfer(s, x, y, w, h, 4, pal[P_LAB_WHITE]);
        if (horiz) {
            s.drawFastHLine(x + 4, y + h / 2, w - 8, mainCol);
            s.drawFastHLine(x + 6, y + h / 2 - 1, w - 12, pal[P_WHITE]);
        } else {
            s.drawFastVLine(x + w / 2, y + 4, h - 8, mainCol);
            s.drawFastVLine(x + w / 2 - 1, y + 6, h - 12, pal[P_WHITE]);
        }
    }
    (void)nowMs; (void)state;
}

// Screen 2 (Center): Aperture Science Sentry Turret ("I see you") + Laser Sight + Date
__attribute__((noinline)) void drawPortalTurretCenter(TFT_eSprite &s, struct tm &now, suseconds_t subsec,
                                                      unsigned long nowMs, uint8_t state, const uint16_t *pal) {
    const int cx = 67;

    swDrawTxt(s, state == 1 ? "NEUROTOXIN!" : "APERTURE", cx, 16, 1, 1, TC_DATUM,
              pal[state == 1 ? P_TURRET_RED : P_LAB_WHITE]);
    s.drawFastHLine(20, 27, 95, pal[P_LAB_GREY]);

    // Sentry Turret Body (cx = 67, cy = 95)
    const int ty = 88;

    // Tripod Legs (Black steel)
    s.drawLine(cx - 4, ty + 24, cx - 28, ty + 68, pal[P_BLACK]);
    s.drawLine(cx - 5, ty + 24, cx - 29, ty + 68, pal[P_BLACK]);
    s.drawLine(cx + 4, ty + 24, cx + 28, ty + 68, pal[P_BLACK]);
    s.drawLine(cx + 5, ty + 24, cx + 29, ty + 68, pal[P_BLACK]);
    s.drawLine(cx, ty + 20, cx, ty + 62, pal[P_LAB_GREY]); // Center rear leg

    // Sleek White Egg-Shaped Turret Body
    s.fillCircle(cx, ty - 6, 24, pal[P_LAB_WHITE]);
    s.fillRect(cx - 24, ty - 6, 49, 32, pal[P_LAB_WHITE]);
    s.fillCircle(cx, ty + 24, 20, pal[P_LAB_WHITE]);

    // Turret Side Wing Ammo Pods (open slightly)
    s.fillRect(cx - 27, ty - 2, 6, 28, pal[P_BLACK]);
    s.fillRect(cx + 22, ty - 2, 6, 28, pal[P_BLACK]);

    // Glowing Red Turret Eye in center (cx, ty)
    s.fillCircle(cx, ty + 2, 8, pal[P_BLACK]);
    s.fillCircle(cx, ty + 2, 5, pal[P_TURRET_RED]);
    s.fillCircle(cx, ty + 2, 2, pal[P_WHITE]);

    // Red Laser Targeting Beam sweeping across the bottom!
    int laserSweep = ((int)(nowMs / 30u) % 110) - 55;
    s.drawLine(cx, ty + 2, cx + laserSweep, 235, pal[P_TURRET_RED]);
    s.fillCircle(cx + laserSweep, 235, 3, pal[P_TURRET_RED]);
    s.drawPixel(cx + laserSweep, 235, pal[P_WHITE]);

    // Bottom Date Display in Aperture Test Chamber Sign box (y = 176..220)
    swFillChamfer(s, 16, 176, 103, 44, 4, pal[P_BLACK]);
    s.drawRect(16, 176, 103, 44, pal[P_LAB_WHITE]);
    swDrawTxt(s, "CHAMBER DATE", cx, 180, 1, 1, TC_DATUM, pal[P_HAZARD_YEL]);
    char dateStr[6] = {
        char('0' + now.tm_mday / 10), char('0' + now.tm_mday % 10), '.',
        char('0' + now.tm_mon / 10), char('0' + now.tm_mon % 10), 0
    };
    swDrawTxt(s, dateStr, cx, 202, 4, 1, MC_DATUM, pal[P_LAB_WHITE]);
    (void)subsec;
}

// Screen 5 (Rightmost): Test Chamber Signage + Weighted Companion Cube + The Cake + Weather
__attribute__((noinline)) void drawPortalSignScreen(TFT_eSprite &s, WeatherService &weather,
                                                    struct tm &now, unsigned long nowMs,
                                                    uint8_t state, const uint16_t *pal) {
    // 1. TOP (y = 16..78): Test Chamber Sign with Big Chamber Number 04 + Companion Cube Icon
    swFillChamfer(s, 10, 16, 115, 62, 4, pal[P_BLACK]);
    s.drawRect(10, 16, 115, 62, pal[P_LAB_WHITE]);

    // Big test chamber number "04"
    swDrawTxt(s, "04", 42, 47, 4, 1, MC_DATUM, pal[P_LAB_WHITE]);

    // Companion Cube icon on right of sign (cx = 92, cy = 47)
    const int qx = 92, qy = 47;
    s.fillRect(qx - 14, qy - 14, 28, 28, pal[P_LAB_GREY]);
    s.drawRect(qx - 14, qy - 14, 28, 28, pal[P_LAB_WHITE]);
    // Pink heart in center of Companion Cube
    s.fillCircle(qx - 4, qy - 3, 4, pal[P_CUBE_PINK]);
    s.fillCircle(qx + 4, qy - 3, 4, pal[P_CUBE_PINK]);
    s.fillTriangle(qx - 8, qy - 2, qx + 8, qy - 2, qx, qy + 7, pal[P_CUBE_PINK]);

    // 2. MIDDLE (y = 86..156): Environmental Weather Readout
    swFillChamfer(s, 10, 86, 115, 72, 4, pal[P_BLACK]);
    s.drawRect(10, 86, 115, 72, pal[P_LAB_WHITE]);
    swDrawTxt(s, "TEST CONDITIONS", 67, 91, 1, 1, TC_DATUM, pal[P_HAZARD_YEL]);

    char unit = (WeatherService::getUnits().value == "imperial") ? 'F' : 'C';
    float vals[3] = {weather.getNowTemp(), weather.getHigh(5), weather.getLow(5)};
    int ints[3];
    for (int k = 0; k < 3; ++k) ints[k] = (int)(vals[k] + (vals[k] >= 0.0f ? 0.5f : -0.5f));

    char buf[16];
    snprintf(buf, sizeof(buf), "%d%c", ints[0], unit);
    swDrawTxt(s, buf, 42, 124, 4, 1, MC_DATUM, pal[P_PORTAL_BLUE]);

    snprintf(buf, sizeof(buf), "HI:%d", ints[1]);
    swDrawTxt(s, buf, 94, 116, 2, 1, MC_DATUM, pal[P_PORTAL_ORANGE]);
    snprintf(buf, sizeof(buf), "LO:%d", ints[2]);
    swDrawTxt(s, buf, 94, 134, 2, 1, MC_DATUM, pal[P_PORTAL_BLUE]);

    // 3. BOTTOM (y = 164..232): "THE CAKE IS A LIE" Icon & Test Progress Bar
    swFillChamfer(s, 10, 164, 115, 68, 4, pal[P_BLACK]);
    s.drawRect(10, 164, 115, 68, pal[P_LAB_WHITE]);
    swDrawTxt(s, (now.tm_sec % 20 < 10) ? "THE CAKE IS A LIE" : "GLADOS ONLINE",
              67, 170, 1, 1, TC_DATUM, pal[P_PORTAL_ORANGE]);

    // Test Progress Bar across seconds
    int fillW = (now.tm_sec * 98) / 59;
    s.drawRect(16, 192, 102, 16, pal[P_LAB_WHITE]);
    if (fillW > 0) {
        s.fillRect(18, 194, fillW, 12, pal[(fillW > 60) ? P_PORTAL_ORANGE : P_PORTAL_BLUE]);
    }
    snprintf(buf, sizeof(buf), ":%02d", now.tm_sec);
    swDrawTxt(s, buf, 67, 218, 2, 1, MC_DATUM, pal[P_LAB_WHITE]);
    (void)nowMs; (void)state;
}

} // namespace

void MatrixClock::renderPortal(IPSClock &clock, TimeSync &time, WeatherService &weather) {
    unsigned long nowMs = millis();
    if (nowMs - lastFrame < 45) return;
    lastFrame = nowMs;
    ++frame;

    struct tm now;
    suseconds_t subsecond;
    time.getLocalTime(&now, &subsecond);

    uint16_t stateMs = 0;
    if (now.tm_sec <= 2) {
        portalState = 2;
        stateMs = (uint16_t)(now.tm_sec * 1000u + (uint32_t)(subsecond / 1000));
    } else if (now.tm_sec >= 56) {
        portalState = 1;
        stateMs = (uint16_t)((now.tm_sec - 56) * 1000u + (uint32_t)(subsecond / 1000));
    } else {
        portalState = 0;
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
    swDimPalette(PORTAL_RAW, pal, P_COUNT);

    for (int i = 0; i < 6; ++i) {
        drawPortalBackdrop(sprite, i, nowMs, portalState, stateMs, pal);

        if (i == 2) {
            drawPortalTurretCenter(sprite, now, subsecond, nowMs, portalState, pal);
        } else if (i == 5) {
            drawPortalSignScreen(sprite, weather, now, nowMs, portalState, pal);
        } else {
            bool blankHour = (i == 0 && hour < 10 && !IPSClock::getLeadingZero().value);
            if (!blankHour) {
                drawPortalDigit(sprite, i, digits[i], nowMs, portalState, pal);
            }
        }

        tfts->chip_select.setDigit(SW_SCREENS[i]);
        sprite.pushSprite(0, 0);
    }
    tfts->release();
}
