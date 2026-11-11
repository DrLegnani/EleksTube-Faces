#include "MatrixClock.h"
#include "StarWarsHud.h"
#include "IPSClock.h"
#include "GhostbustersLogo.h"
#include <math.h>

// Shared with Backlights: 0 = Normal Ecto patrol, 1 = PKE Class 5 Alert (56-59s), 2 = Cross Streams & Trap (00-02s)
uint8_t ghostbustersState = 0;

namespace {

enum GhostPal {
    P_WHITE = 0,
    P_BLACK,
    P_SKY_TOP,      // Deep midnight navy
    P_SKY_BOT,      // Eerie supernatural violet
    P_BUILDING,     // Manhattan skyline dark skyscraper silhouette
    P_BUILDING_EDGE,// Architectural bevel/roofline highlight
    P_WINDOW_LIT,   // Warm skyscraper office windows
    P_HAZARD_YEL,   // Trap yellow hazard chevron
    P_HAZARD_BLK,   // Trap black chevron
    P_LOGO_RED,     // No-Ghost prohibition red circle & slash
    P_SLIMER_GRN,   // Fluorescent ectoplasm green
    P_SLIMER_AURA,  // Translucent green halo
    P_SLIMER_LIP,   // Slimer mouth / tongue pink
    P_STREAM_RED,   // Proton stream outer red
    P_STREAM_ORG,   // Proton stream orange flame
    P_STREAM_YEL,   // Proton stream intense yellow
    P_STREAM_CYAN,  // Proton electric lightning cyan
    P_TRAP_CONE,    // Ghost trap capture light beam
    P_PKE_AMBER,    // PKE meter LED amber
    P_PANEL_BG,     // Deep beveled HUD backdrop
    P_COUNT
};

const uint16_t GHOST_RAW[P_COUNT] = {
    0xFFFF,                 // P_WHITE
    0x0000,                 // P_BLACK
    swC565(6, 8, 24),       // P_SKY_TOP
    swC565(32, 14, 52),     // P_SKY_BOT
    swC565(18, 16, 26),     // P_BUILDING
    swC565(48, 42, 64),     // P_BUILDING_EDGE
    swC565(255, 225, 120),  // P_WINDOW_LIT
    swC565(250, 205, 15),   // P_HAZARD_YEL
    swC565(20, 20, 20),     // P_HAZARD_BLK
    swC565(225, 20, 24),    // P_LOGO_RED
    swC565(80, 255, 60),    // P_SLIMER_GRN
    swC565(25, 110, 25),    // P_SLIMER_AURA
    swC565(230, 80, 110),   // P_SLIMER_LIP
    swC565(220, 30, 20),    // P_STREAM_RED
    swC565(255, 125, 20),   // P_STREAM_ORG
    swC565(255, 230, 50),   // P_STREAM_YEL
    swC565(60, 230, 255),   // P_STREAM_CYAN
    swC565(140, 240, 255),  // P_TRAP_CONE
    swC565(255, 160, 20),   // P_PKE_AMBER
    swC565(14, 12, 22)      // P_PANEL_BG
};

// Manhattan skyline at night (solid buildings spanning full width, sky strictly above, zero ghosting)
__attribute__((noinline)) void drawGhostbustersBackdrop(TFT_eSprite &s, int screen, unsigned long nowMs,
                                                        uint8_t state, uint16_t stateMs, const uint16_t *pal) {
    // 0. ALWAYS clear full sprite to prevent ANY persistence between screens
    s.fillSprite(pal[P_BLACK]);

    // Building roofline heights per screen (0 = HoursTens, 1 = HoursOnes, 2 = Center Logo, 3 = MinTens, 4 = MinOnes, 5 = HUD)
    // Roofline y is where the sky ends and the solid building begins:
    // Screen 2 has lower buildings (roof at y=168) so the No-Ghost logo has clean starry sky behind it!
    int roofY[6] = {125, 110, 165, 118, 130, 105};
    int rY = roofY[screen];

    // 1. Midnight Sky Gradient (strictly from y = 0 down to rY)
    for (int y = 0; y < rY; ++y) {
        uint8_t t = (y * 255) / rY;
        uint16_t sky = (t < 110) ? pal[P_SKY_TOP] : pal[P_SKY_BOT];
        s.drawFastHLine(0, y, TFT_WIDTH, sky);
    }

    // 2. Twinkling stars in the upper sky
    uint16_t seed = (uint16_t)(screen * 71 + 33);
    for (int star = 0; star < 12; ++star) {
        seed = seed * 109 + 89;
        int sx = seed % TFT_WIDTH;
        int sy = (seed / 13) % (rY > 15 ? (rY - 10) : 10);
        bool twinkle = (((nowMs / 250 + seed) % 7) != 0);
        if (twinkle) {
            s.drawPixel(sx, sy, pal[P_WHITE]);
        }
    }

    // 3. Solid Manhattan Buildings (strictly from rY down to y = 222, spanning FULL width 0..TFT_WIDTH)
    s.fillRect(0, rY, TFT_WIDTH, 222 - rY, pal[P_BUILDING]);
    s.drawFastHLine(0, rY, TFT_WIDTH, pal[P_BUILDING_EDGE]);

    // Architectural crowns / spires
    if (screen == 0 || screen == 4) {
        s.fillRect(20, rY - 14, 30, 14, pal[P_BUILDING]);
        s.drawFastHLine(20, rY - 14, 30, pal[P_BUILDING_EDGE]);
        s.drawFastVLine(35, rY - 26, 12, pal[P_BUILDING_EDGE]); // Antenna spire
    } else if (screen == 1 || screen == 3) {
        s.fillRect(70, rY - 18, 40, 18, pal[P_BUILDING]);
        s.drawFastHLine(70, rY - 18, 40, pal[P_BUILDING_EDGE]);
        s.drawFastVLine(90, rY - 30, 12, pal[P_BUILDING_EDGE]); // Antenna spire
    }

    // Lit windows in buildings
    for (int wy = rY + 8; wy < 216; wy += 14) {
        for (int wx = 12; wx < TFT_WIDTH - 16; wx += 16) {
            uint16_t wseed = (uint16_t)(wx * 37 + wy * 19 + screen * 43);
            if ((wseed % 3) == 0) {
                s.fillRect(wx, wy, 6, 8, pal[P_WINDOW_LIT]);
            }
        }
    }

    // 4. Ghost Trap caution hazard stripes across the very bottom (y: 222-239)
    s.fillRect(0, 222, TFT_WIDTH, 18, pal[P_HAZARD_BLK]);
    for (int x = -20; x < TFT_WIDTH + 20; x += 16) {
        int sx = x + (screen * 8);
        for (int step = 0; step < 18; ++step) {
            int px = sx + step;
            if (px >= 0 && px < TFT_WIDTH) {
                s.drawPixel(px, 222 + step, pal[P_HAZARD_YEL]);
                if (px + 1 < TFT_WIDTH) s.drawPixel(px + 1, 222 + step, pal[P_HAZARD_YEL]);
                if (px + 2 < TFT_WIDTH) s.drawPixel(px + 2, 222 + step, pal[P_HAZARD_YEL]);
                if (px + 3 < TFT_WIDTH) s.drawPixel(px + 3, 222 + step, pal[P_HAZARD_YEL]);
            }
        }
    }
    s.drawFastHLine(0, 221, TFT_WIDTH, pal[P_HAZARD_YEL]);

    // 5. Ectoplasm Slime Drips from top bezel
    int dripX[4] = {18, 48, 86, 116};
    int dripLen[4] = {8, 16, 11, 20};
    for (int d = 0; d < 4; ++d) {
        int dx = dripX[d];
        int dl = dripLen[d] + ((d + screen) % 3) * 2;
        s.fillRect(dx - 3, 0, 7, dl - 4, pal[P_SLIMER_GRN]);
        s.fillCircle(dx, dl - 3, 4, pal[P_SLIMER_GRN]);
        s.fillCircle(dx - 1, dl - 4, 1, pal[P_WHITE]);
    }
    s.drawFastHLine(0, 0, TFT_WIDTH, pal[P_SLIMER_GRN]);
    s.drawFastHLine(0, 1, TFT_WIDTH, pal[P_SLIMER_GRN]);
}

// Draw the REAL, iconic Ghostbusters No-Ghost Logo on Screen 2 (ONLY ON JANELA 3)
__attribute__((noinline)) void drawNoGhostLogoCenter(TFT_eSprite &s, const struct tm &now,
                                                     suseconds_t subsecond, unsigned long nowMs,
                                                     uint8_t state, const uint16_t *pal) {
    int cx = TFT_WIDTH / 2; // 67

    // Top Header Badge
    swFillChamfer(s, 10, 8, 115, 26, 4, pal[P_LOGO_RED]);
    s.fillRect(12, 10, 111, 22, pal[P_PANEL_BG]);
    swDrawTxt(s, "GHOSTBUSTERS", cx, 13, 1, 1, TC_DATUM, pal[P_WHITE]);
    swDrawTxt(s, "NYC  1984", cx, 23, 1, 1, TC_DATUM, pal[P_STREAM_ORG]);

    // REAL BITMAP LOGO (112x112 from PROGMEM)
    int startX = (TFT_WIDTH - GHOST_LOGO_W) / 2; // 11
    int startY = 40;

    for (int y = 0; y < GHOST_LOGO_H; ++y) {
        for (int x = 0; x < GHOST_LOGO_W; ++x) {
            uint16_t rawColor = pgm_read_word(&GHOST_LOGO_DATA[y * GHOST_LOGO_W + x]);
            if (rawColor != 0x0000) {
                s.drawPixel(startX + x, startY + y, tfts->dimColor(rawColor));
            }
        }
    }

    // Bottom Status Badge (DATA - ESTA É A ÚNICA TELA QUE DEVE TER A DATA)
    swFillChamfer(s, 12, 168, 111, 48, 4, pal[P_HAZARD_YEL]);
    s.fillRect(14, 170, 107, 44, pal[P_PANEL_BG]);

    if (state == 2) {
        swDrawTxt(s, "STREAMS CROSSED!", cx, 174, 1, 1, TC_DATUM, pal[P_LOGO_RED]);
    } else if (state == 1) {
        swDrawTxt(s, "P.K.E. CRITICAL!", cx, 174, 1, 1, TC_DATUM, pal[P_LOGO_RED]);
    } else {
        swDrawTxt(s, "WHO YA GONNA CALL?", cx, 174, 1, 1, TC_DATUM, pal[P_STREAM_ORG]);
    }

    // Date readout (ONLY ON SCREEN 2 / JANELA 3)
    char dateBuf[16];
    snprintf(dateBuf, sizeof(dateBuf), "%02d.%02d", now.tm_mon + 1, now.tm_mday);
    swDrawTxt(s, dateBuf, cx, 196, 4, 1, MC_DATUM, pal[P_STREAM_CYAN]);
}

// Draw PKE Meter + Slimer + Weather readout on Screen 5 (ONLY ON JANELA 6)
__attribute__((noinline)) void drawGhostbustersHudScreen(TFT_eSprite &s, WeatherService &weather,
                                                         const struct tm &now, unsigned long nowMs,
                                                         uint8_t state, const uint16_t *pal) {
    int cx = TFT_WIDTH / 2; // 67

    // 1. Top P.K.E. Detection Header
    swFillChamfer(s, 10, 8, 115, 32, 4, pal[P_STREAM_CYAN]);
    s.fillRect(12, 10, 111, 28, pal[P_PANEL_BG]);
    swDrawTxt(s, "P.K.E. DETECTOR", cx, 12, 1, 1, TC_DATUM, pal[P_STREAM_CYAN]);
    swDrawTxt(s, "FIREHOUSE HQ", cx, 23, 1, 1, TC_DATUM, pal[P_WHITE]);

    // 2. Animated PKE Meter Sensor Wings & Bar Graph
    int pkeY = 54;
    int elevation = (state == 1) ? 22 : (int)(fabsf(sinf(nowMs * 0.003f)) * 14.0f);

    // Central PKE handle
    s.fillRect(cx - 10, pkeY, 20, 24, pal[P_BUILDING]);
    s.drawRect(cx - 10, pkeY, 20, 24, pal[P_STREAM_CYAN]);

    // Center PKE screen bar graph
    uint8_t bars = (state == 1) ? 5 : ((nowMs / 180) % 6);
    for (int b = 0; b < 5; ++b) {
        uint16_t bcol = (b < bars) ? (b > 3 ? pal[P_LOGO_RED] : pal[P_PKE_AMBER]) : pal[P_BLACK];
        s.fillRect(cx - 7 + b * 3, pkeY + 7, 2, 10, bcol);
    }

    // Left Wing
    s.drawLine(cx - 10, pkeY + 6, cx - 35, pkeY + 6 - elevation, pal[P_WHITE]);
    s.drawLine(cx - 10, pkeY + 8, cx - 35, pkeY + 8 - elevation, pal[P_STREAM_ORG]);
    for (int wled = 0; wled < 3; ++wled) {
        int lx = cx - 18 - wled * 7;
        int ly = pkeY + 7 - (elevation * (wled + 1)) / 3;
        bool lit = (state == 1) ? (((nowMs / 70 + wled) % 2) == 0) : (((nowMs / 180 + wled) % 3) == 0);
        s.fillCircle(lx, ly, 2, lit ? pal[P_PKE_AMBER] : pal[P_LOGO_RED]);
    }

    // Right Wing
    s.drawLine(cx + 10, pkeY + 6, cx + 35, pkeY + 6 - elevation, pal[P_WHITE]);
    s.drawLine(cx + 10, pkeY + 8, cx + 35, pkeY + 8 - elevation, pal[P_STREAM_ORG]);
    for (int wled = 0; wled < 3; ++wled) {
        int rx = cx + 18 + wled * 7;
        int ry = pkeY + 7 - (elevation * (wled + 1)) / 3;
        bool lit = (state == 1) ? (((nowMs / 70 + wled) % 2) == 0) : (((nowMs / 180 + wled) % 3) == 0);
        s.fillCircle(rx, ry, 2, lit ? pal[P_PKE_AMBER] : pal[P_LOGO_RED]);
    }

    // 3. Floating Animated SLIMER (Geleia)
    int slimerY = 112 + (int)(sinf(nowMs * 0.004f) * 6.0f);
    int slimerX = cx;

    s.fillCircle(slimerX, slimerY, 20, pal[P_SLIMER_AURA]);
    s.fillRoundRect(slimerX - 15, slimerY - 13, 30, 26, 10, pal[P_SLIMER_GRN]);
    s.fillCircle(slimerX, slimerY - 9, 13, pal[P_SLIMER_GRN]);
    s.fillCircle(slimerX - 5, slimerY + 11, 7, pal[P_SLIMER_GRN]);
    s.fillCircle(slimerX + 6, slimerY + 10, 6, pal[P_SLIMER_GRN]);

    s.fillCircle(slimerX - 5, slimerY - 11, 3, pal[P_WHITE]);
    s.fillCircle(slimerX + 5, slimerY - 11, 3, pal[P_WHITE]);
    s.fillCircle(slimerX - 4, slimerY - 11, 1, pal[P_BLACK]);
    s.fillCircle(slimerX + 4, slimerY - 11, 1, pal[P_BLACK]);

    s.fillRoundRect(slimerX - 9, slimerY - 3, 18, 10, 4, pal[P_BLACK]);
    s.fillCircle(slimerX, slimerY + 2, 3, pal[P_SLIMER_LIP]);
    s.drawPixel(slimerX - 5, slimerY - 2, pal[P_WHITE]);
    s.drawPixel(slimerX + 5, slimerY - 2, pal[P_WHITE]);

    s.fillRoundRect(slimerX - 20, slimerY - 5, 7, 5, 2, pal[P_SLIMER_GRN]);
    s.fillRoundRect(slimerX + 13, slimerY - 7, 7, 5, 2, pal[P_SLIMER_GRN]);

    // 4. Weather & Entity Status Box (Clean chamfer panel, ONLY ON SCREEN 5)
    swFillChamfer(s, 10, 162, 115, 54, 4, pal[P_SLIMER_GRN]);
    s.fillRect(12, 164, 111, 50, pal[P_PANEL_BG]);

    swDrawTxt(s, "PARANORMAL SCAN", cx, 167, 1, 1, TC_DATUM, pal[P_SLIMER_GRN]);

    if (state == 1 || state == 2) {
        swDrawTxt(s, "CLASS 5 VAPOR!", cx, 178, 1, 1, TC_DATUM, pal[P_LOGO_RED]);
    } else {
        swDrawTxt(s, "SLIMER SPOTTED", cx, 178, 1, 1, TC_DATUM, pal[P_STREAM_YEL]);
    }

    // Weather readout (ONLY ON SCREEN 5)
    char unit = (WeatherService::getUnits().value == "imperial") ? 'F' : 'C';
    float curTemp = weather.getNowTemp();
    int temp = (int)(curTemp + (curTemp >= 0.0f ? 0.5f : -0.5f));
    char wBuf[16];
    snprintf(wBuf, sizeof(wBuf), "NYC %d%c", temp, unit);
    swDrawTxt(s, wBuf, 38, 198, 2, 1, MC_DATUM, pal[P_WHITE]);

    float highT = weather.getHigh(5);
    int hi = (int)(highT + (highT >= 0.0f ? 0.5f : -0.5f));
    char hiBuf[12];
    snprintf(hiBuf, sizeof(hiBuf), "HI:%d", hi);
    swDrawTxt(s, hiBuf, 94, 198, 1, 1, MC_DATUM, pal[P_STREAM_ORG]);
}

// Draw stylized high-energy Proton Wand Stream Digits
__attribute__((noinline)) void drawGhostbustersDigit(TFT_eSprite &s, int digit, unsigned long nowMs,
                                                     uint8_t state, const uint16_t *pal) {
    if (digit < 0 || digit > 9) return;

    static const uint8_t segs[10] = {
        0x3F, // 0: a b c d e f
        0x06, // 1: b c
        0x5B, // 2: a b d e g
        0x4F, // 3: a b c d g
        0x66, // 4: b c f g
        0x6D, // 5: a c d f g
        0x7D, // 6: a c d e f g
        0x07, // 7: a b c
        0x7F, // 8: a b c d e f g
        0x6F  // 9: a b c d f g
    };

    uint8_t mask = segs[digit];
    int x0 = 24;
    int y0 = 36;
    int w = 86;
    int h = 138;
    int th = 14;

    auto drawSeg = [&](int sx, int sy, int sw, int sh, bool horiz) {
        // Outer fiery proton stream aura (Red/Orange)
        s.fillRoundRect(sx - 2, sy - 2, sw + 4, sh + 4, 4, pal[P_STREAM_RED]);
        // Mid layer (Proton flame orange)
        s.fillRoundRect(sx, sy, sw, sh, 3, pal[P_STREAM_ORG]);
        // Inner core (Intense yellow)
        if (horiz) {
            s.fillRect(sx + 3, sy + 3, sw - 6, sh - 6, pal[P_STREAM_YEL]);
            s.drawFastHLine(sx + 5, sy + sh / 2, sw - 10, pal[P_STREAM_CYAN]);
            s.drawFastHLine(sx + 6, sy + sh / 2 - 1, sw - 12, pal[P_WHITE]);
        } else {
            s.fillRect(sx + 3, sy + 3, sw - 6, sh - 6, pal[P_STREAM_YEL]);
            s.drawFastVLine(sx + sw / 2, sy + 5, sh - 10, pal[P_STREAM_CYAN]);
            s.drawFastVLine(sx + sw / 2 - 1, sy + 6, sh - 12, pal[P_WHITE]);
        }
    };

    // Segments:
    if (mask & 0x01) drawSeg(x0 + 4, y0, w - 8, th, true);
    if (mask & 0x02) drawSeg(x0 + w - th, y0 + 4, th, (h / 2) - 4, false);
    if (mask & 0x04) drawSeg(x0 + w - th, y0 + (h / 2), th, (h / 2) - 4, false);
    if (mask & 0x08) drawSeg(x0 + 4, y0 + h - th, w - 8, th, true);
    if (mask & 0x10) drawSeg(x0, y0 + (h / 2), th, (h / 2) - 4, false);
    if (mask & 0x20) drawSeg(x0, y0 + 4, th, (h / 2) - 4, false);
    if (mask & 0x40) drawSeg(x0 + 4, y0 + (h / 2) - (th / 2), w - 8, th, true);

    // Drifting electric proton sparks around digits
    uint16_t spSeed = (uint16_t)(digit * 47 + nowMs / 60);
    for (int p = 0; p < 4; ++p) {
        spSeed = spSeed * 97 + 13;
        int px = x0 - 6 + (spSeed % (w + 12));
        int py = y0 - 6 + ((spSeed / 11) % (h + 12));
        s.drawPixel(px, py, pal[P_STREAM_CYAN]);
    }
}

// Crossing the Streams & Ghost Trap blast at :00 - :02s
__attribute__((noinline)) void drawStreamCrossAnimation(TFT_eSprite &s, int screen, uint16_t stateMs,
                                                        unsigned long nowMs, const uint16_t *pal) {
    if (screen == 2 || screen == 3) {
        int trapX = (screen == 2) ? TFT_WIDTH - 20 : 20;
        for (int y = 50; y < 220; ++y) {
            int beamW = (220 - y) / 3;
            int bx = trapX - beamW / 2;
            s.drawFastHLine(bx, y, beamW, pal[P_TRAP_CONE]);
        }
    }

    for (int x = 0; x < TFT_WIDTH; ++x) {
        int globalX = screen * TFT_WIDTH + x;
        float wave = sinf(globalX * 0.04f + nowMs * 0.02f) * 26.0f;
        int streamY = 110 + (int)wave;

        s.fillCircle(x, streamY, 6, pal[P_STREAM_RED]);
        s.fillCircle(x, streamY, 4, pal[P_STREAM_ORG]);
        s.drawPixel(x, streamY, pal[P_WHITE]);
        s.drawPixel(x, streamY - 1, pal[P_STREAM_CYAN]);

        float wave2 = cosf(globalX * 0.035f - nowMs * 0.025f) * 32.0f;
        int stream2Y = 125 + (int)wave2;
        s.fillCircle(x, stream2Y, 5, pal[P_STREAM_ORG]);
        s.fillCircle(x, stream2Y, 2, pal[P_STREAM_YEL]);
        s.drawPixel(x, stream2Y, pal[P_STREAM_CYAN]);
    }

    uint16_t sparkSeed = (uint16_t)(screen * 113 + stateMs / 30);
    for (int sp = 0; sp < 12; ++sp) {
        sparkSeed = sparkSeed * 89 + 41;
        int sx = sparkSeed % TFT_WIDTH;
        int sy = 60 + ((sparkSeed / 17) % 130);
        s.drawFastHLine(sx - 3, sy, 7, pal[P_STREAM_CYAN]);
        s.drawFastVLine(sx, sy - 3, 7, pal[P_WHITE]);
    }
}

} // namespace

void MatrixClock::renderGhostbusters(IPSClock &clock, TimeSync &time, WeatherService &weather) {
    unsigned long nowMs = millis();
    if (nowMs - lastFrame < 45) return;
    lastFrame = nowMs;
    ++frame;

    struct tm now;
    suseconds_t subsecond;
    time.getLocalTime(&now, &subsecond);

    uint16_t stateMs = 0;
    if (now.tm_sec <= 2) {
        ghostbustersState = 2; // Crossing the Streams & Trap capture
        stateMs = (uint16_t)(now.tm_sec * 1000u + (uint32_t)(subsecond / 1000));
    } else if (now.tm_sec >= 56) {
        ghostbustersState = 1; // PKE Critical / Ecto-1 Siren alert
        stateMs = (uint16_t)((now.tm_sec - 56) * 1000u + (uint32_t)(subsecond / 1000));
    } else {
        ghostbustersState = 0; // Normal Ecto patrol
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
    swDimPalette(GHOST_RAW, pal, P_COUNT);

    for (int i = 0; i < 6; ++i) {
        drawGhostbustersBackdrop(sprite, i, nowMs, ghostbustersState, stateMs, pal);

        if (i == 2) {
            drawNoGhostLogoCenter(sprite, now, subsecond, nowMs, ghostbustersState, pal);
        } else if (i == 5) {
            drawGhostbustersHudScreen(sprite, weather, now, nowMs, ghostbustersState, pal);
        } else {
            bool blankHour = (i == 0 && hour < 10 && !IPSClock::getLeadingZero().value);
            if (!blankHour) {
                drawGhostbustersDigit(sprite, digits[i], nowMs, ghostbustersState, pal);
            }
        }

        if (ghostbustersState == 2) {
            drawStreamCrossAnimation(sprite, i, stateMs, nowMs, pal);
        }

        tfts->chip_select.setDigit(SW_SCREENS[i]);
        sprite.pushSprite(0, 0);
    }
    tfts->release();
}
