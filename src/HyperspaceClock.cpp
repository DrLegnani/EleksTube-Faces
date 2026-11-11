#include "MatrixClock.h"
#include "StarWarsHud.h"
#include "IPSClock.h"

// Shared state with Backlights: 0 = sublight cruise, 1 = hyperdrive charge (56-59s), 2 = hyperspace jump (00-03s)
uint8_t hyperspaceJumpState = 0;

namespace {

enum PalIdx {
    P_WHITE = 0, P_CYAN, P_HUD, P_DARK_HUD, P_STAR_DIM, P_STAR_MID,
    P_WARP_TAIL, P_WARP_MID, P_WARP_TIP, P_BG_CHG, P_BG_JUMP,
    P_AURA, P_BODY_C, P_BODY_G, P_CORE_C, P_BOX_BG, P_WARM, P_COOL,
    P_HULL_D, P_HULL_M, P_HULL_L, P_HULL_R, P_RED, P_COCKPIT, P_ENG_TAIL,
    P_COUNT
};

const uint16_t RAW_PAL[P_COUNT] = {
    0xFFFF,                 // P_WHITE
    swC565(25, 210, 255),   // P_CYAN
    swC565(0, 140, 210),    // P_HUD
    swC565(0, 65, 130),     // P_DARK_HUD
    swC565(75, 130, 210),   // P_STAR_DIM
    swC565(165, 220, 255),  // P_STAR_MID
    swC565(25, 145, 255),   // P_WARP_TAIL
    swC565(105, 230, 255),  // P_WARP_MID
    swC565(225, 248, 255),  // P_WARP_TIP
    swC565(1, 5, 18),       // P_BG_CHG
    swC565(3, 12, 36),      // P_BG_JUMP
    swC565(0, 52, 118),     // P_AURA
    swC565(10, 185, 245),   // P_BODY_C
    swC565(100, 238, 255),  // P_BODY_G
    swC565(215, 248, 255),  // P_CORE_C
    swC565(4, 12, 30),      // P_BOX_BG
    swC565(255, 220, 140),  // P_WARM
    swC565(130, 235, 255),  // P_COOL
    swC565(68, 78, 92),     // P_HULL_D
    swC565(132, 144, 158),  // P_HULL_M
    swC565(180, 192, 206),  // P_HULL_L
    swC565(220, 232, 245),  // P_HULL_R
    swC565(178, 56, 48),    // P_RED
    swC565(14, 34, 68),     // P_COCKPIT
    swC565(10, 95, 200)     // P_ENG_TAIL
};

__attribute__((noinline)) void drawRadialStarfield(TFT_eSprite &s, int screen, uint32_t warpAccum,
                                                   uint8_t jumpState, uint16_t secMix, const uint16_t *pal) {
    s.fillSprite(jumpState == 2 ? pal[P_BG_JUMP] : (jumpState == 1 ? pal[P_BG_CHG] : TFT_BLACK));

    const int vpX = 442 - screen * 150;
    const int vpY = 104;
    const int dist = (screen < 3) ? (2 - screen) : (screen - 3);
    const uint32_t rate = 76 + dist * 34;
    const int xIn  = (screen < 3) ? 135 : 0;
    const int xOut = (screen < 3) ? -6 : 141;
    const int streakDelta = (jumpState == 2) ? 165 : (jumpState == 1 ? (28 + (int)secMix) : 4);

    for (int k = 0; k < 28; ++k) {
        uint32_t h = (uint32_t)(screen + 11) * 2654435761u ^ (uint32_t)(k + 7) * 2246822519u;
        int tx = (int)((h >> 8) % 135);
        int ty = (int)((h >> 16) % 240);
        int dx = tx - vpX, dy = ty - vpY;
        int f0 = ((xIn - vpX) * 256) / dx;
        int f1 = ((xOut - vpX) * 256) / dx;

        uint32_t u = (((warpAccum * rate) >> 13) + (h & 0xFF)) & 0xFF;
        int p1 = (int)((u * (96u + u)) >> 8);
        int fHead = f0 + ((f1 - f0) * p1) / 350;
        int x1 = vpX + (dx * fHead) / 256;
        int y1 = vpY + (dy * fHead) / 256;

        if (jumpState == 0) {
            if (x1 < 0 || x1 >= TFT_WIDTH || y1 < 0 || y1 >= TFT_HEIGHT) continue;
            uint16_t col = pal[(k % 5 == 0) ? P_WHITE : (k & 1) ? P_STAR_MID : P_STAR_DIM];
            s.drawPixel(x1, y1, col);
            if (k % 7 == 0) {
                s.drawPixel(x1 + 1, y1, col);
                s.drawPixel(x1, y1 + 1, pal[P_STAR_MID]);
            }
        } else {
            int p0 = p1 - streakDelta;
            int fTail = (p0 > 0) ? (f0 + ((f1 - f0) * p0) / 350) : f0 / 2;
            if (fTail < 0) fTail = 0;
            int x0 = vpX + (dx * fTail) / 256;
            int y0 = vpY + (dy * fTail) / 256;
            s.drawLine(x0, y0, x1, y1, pal[(jumpState == 2 && (k % 3 == 0)) ? P_WARP_MID : P_WARP_TAIL]);
            if (jumpState == 2) {
                int xm = (x0 + x1 * 2) / 3, ym = (y0 + y1 * 2) / 3;
                s.drawLine(xm, ym, x1, y1, pal[(k & 1) ? P_WARP_TIP : P_WHITE]);
                if ((k & 3) == 0) s.drawLine(xm, ym + 1, x1, y1 + 1, pal[P_WARP_MID]);
            } else {
                s.drawPixel(x1, y1, pal[P_WHITE]);
            }
        }
    }
}

__attribute__((noinline)) void drawFalconCenterScreen(TFT_eSprite &s, struct tm &now, suseconds_t subsec,
                                                      unsigned long nowMs, uint8_t jumpState, const uint16_t *pal) {
    bool pulseOn = (subsec < 500000L);
    swDrawCenterChrome(s, "HYPERSPACE", now, subsec, pal[jumpState == 2 ? P_WHITE : P_CYAN],
                       pal[P_DARK_HUD], pal[pulseOn ? P_WARP_MID : P_DARK_HUD],
                       pal[pulseOn ? P_WHITE : P_HUD], pal[P_WHITE]);

    int bobY = (jumpState == 2) ? (int)((nowMs / 35) % 3) - 1 : (((nowMs / 320) & 3) == 1 ? -1 : 0);
    const int cx = 60, cy = 112 + bobY;

    static const int8_t triTbl[][7] = {
        {P_HULL_L, -25, -16, -14, -54, -6, -22},
        {P_HULL_M, -14, -54, -6, -54, -6, -22},
        {P_HULL_L, 25, -16, 14, -54, 6, -22},
        {P_HULL_M, 14, -54, 6, -54, 6, -22},
        {P_HULL_M, 22, -10, 44, -25, 27, 8},
        {P_HULL_D, 44, -25, 44, -12, 27, 8}
    };
    for (int i = 0; i < 6; ++i) {
        s.fillTriangle(cx + triTbl[i][1], cy + triTbl[i][2],
                       cx + triTbl[i][3], cy + triTbl[i][4],
                       cx + triTbl[i][5], cy + triTbl[i][6], pal[triTbl[i][0]]);
    }

    static const int8_t rcTbl[][6] = {
        {0, P_RED,     -14, -41, 4, 8},
        {0, P_RED,      10, -41, 4, 8},
        {0, P_HULL_L,   41, -30, 8, 21},
        {1, P_HULL_L,   45, -30, 4, 0},
        {0, P_COCKPIT,  43, -32, 5, 5},
        {0, P_HULL_M,  -37, -5,  5, 10},
        {0, P_HULL_M,   32,  1,  5, 10},
        {1, P_HULL_L,    0,  0, 34, 0},
        {2, P_HULL_R,    0,  0, 34, 0},
        {1, P_HULL_M,    0,  0, 24, 0},
        {2, P_HULL_D,    0,  0, 24, 0},
        {2, P_HULL_D,    0,  0, 13, 0},
        {1, P_HULL_D,    0, -24, 5, 0},
        {0, P_HULL_D,  -14, 15,  7, 9},
        {0, P_HULL_D,    8, 15,  7, 9},
        {0, P_RED,     -20, -24, 6, 4},
        {1, P_HULL_R,  -17, -12, 7, 0},
        {1, P_HULL_M,  -17, -12, 5, 0},
        {1, P_HULL_D,  -17, -12, 2, 0},
        {1, P_HULL_D,    0,  0,  7, 0},
        {1, P_HULL_R,    0,  0,  4, 0},
        {1, P_HULL_D,    0,  0,  2, 0}
    };
    for (int i = 0; i < (int)(sizeof(rcTbl) / sizeof(rcTbl[0])); ++i) {
        uint16_t col = pal[rcTbl[i][1]];
        int px = cx + rcTbl[i][2], py = cy + rcTbl[i][3];
        if (rcTbl[i][0] == 0) s.fillRect(px, py, rcTbl[i][4], rcTbl[i][5], col);
        else if (rcTbl[i][0] == 1) s.fillCircle(px, py, rcTbl[i][4], col);
        else s.drawCircle(px, py, rcTbl[i][4], col);
    }

    s.drawFastHLine(cx + 43, cy - 31, 4, pal[P_CYAN]);
    s.drawLine(cx - 28, cy - 14, cx + 28, cy + 14, pal[P_HULL_D]);
    s.drawLine(cx - 28, cy + 14, cx + 28, cy - 14, pal[P_HULL_D]);
    s.drawFastHLine(cx - 33, cy, 66, pal[P_HULL_D]);
    s.drawFastVLine(cx, cy - 22, 54, pal[P_HULL_D]);

    uint16_t engCore = pal[jumpState == 2 ? P_WHITE : P_CORE_C];
    uint16_t engGlow = pal[jumpState >= 1 ? P_WARP_MID : P_CYAN];
    uint16_t engTail = pal[P_ENG_TAIL];
    int basePlume = (jumpState == 2) ? 14 : (jumpState == 1 ? 8 : 4);
    for (int dx = -23; dx <= 23; ++dx) {
        int ey = cy + 33 - (dx * dx) / 72;
        s.drawFastVLine(cx + dx, ey - 2, 4, engCore);
        int len = basePlume + ((dx * 7 + (int)(nowMs / 45)) & 3) - (abs(dx) >> 3);
        if (len > 1) {
            s.drawFastVLine(cx + dx, ey + 2, len / 2 + 1, engGlow);
            s.drawFastVLine(cx + dx, ey + 2 + len / 2, len / 2, engTail);
        }
    }
}

} // namespace

void MatrixClock::renderHyperspace(IPSClock &clock, TimeSync &time, WeatherService &weather) {
    unsigned long nowMs = millis();
    if (nowMs - lastFrame < 45) return;
    unsigned long dt = lastFrame ? (nowMs - lastFrame) : 45;
    if (dt > 120) dt = 45;
    lastFrame = nowMs;
    ++frame;

    struct tm now;
    suseconds_t subsecond;
    time.getLocalTime(&now, &subsecond);

    static unsigned long warpAccum = 0;
    uint16_t secMix = 0;
    if (now.tm_sec <= 3) {
        hyperspaceJumpState = 2;
        warpAccum += dt * 92u;
    } else if (now.tm_sec >= 56) {
        hyperspaceJumpState = 1;
        uint32_t frac = (uint32_t)(now.tm_sec - 56) * 1000u + (uint32_t)(subsecond / 1000);
        secMix = (uint16_t)(frac / 48u);
        warpAccum += dt * (20u + frac / 95u);
    } else {
        hyperspaceJumpState = 0;
        warpAccum += dt * 14u;
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
    swDimPalette(RAW_PAL, pal, P_COUNT);

    for (int i = 0; i < 6; ++i) {
        drawRadialStarfield(sprite, i, warpAccum, hyperspaceJumpState, secMix, pal);

        if (i == 2) {
            drawFalconCenterScreen(sprite, now, subsecond, nowMs, hyperspaceJumpState, pal);
        } else if (i == 5) {
            swDrawNavWeatherHud(sprite, weather, now, "NAV-HUD",
                                hyperspaceJumpState == 2 ? "HYPERSPACE!" : "HYPERDRIVE",
                                pal[P_CYAN], pal[P_DARK_HUD], pal[P_BOX_BG], pal[P_HUD],
                                pal[P_WARM], pal[P_COOL], hyperspaceJumpState >= 1);
        } else {
            bool blankHour = (i == 0 && hour < 10 && !IPSClock::getLeadingZero().value);
            if (!blankHour) {
                uint16_t body = pal[hyperspaceJumpState == 2 ? P_BODY_G : (hyperspaceJumpState == 1 ? P_CYAN : P_BODY_C)];
                uint16_t core = pal[hyperspaceJumpState == 2 ? P_WHITE : P_CORE_C];
                swDrawHoloDigit(sprite, digits[i], pal[P_AURA], body, core, pal[P_HUD]);
            }
        }

        tfts->chip_select.setDigit(SW_SCREENS[i]);
        sprite.pushSprite(0, 0);
    }
    tfts->release();
}
