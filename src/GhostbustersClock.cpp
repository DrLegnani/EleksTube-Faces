#include "MatrixClock.h"
#include "StarWarsHud.h"
#include "IPSClock.h"
#include "GhostbustersLogo.h"
#include "SlimerLogo.h"
#include <math.h>

// Shared with Backlights: 0 = Normal Ecto patrol, 1 = PKE Class 5 Alert (56-59s), 2 = Cross Streams & Trap (00-02s)
uint8_t ghostbustersState = 0;

namespace {

enum GhostPal {
    P_WHITE = 0,
    P_BLACK,
    P_SKY_TOP,          // Deep midnight navy
    P_SKY_BOT,          // Eerie supernatural violet
    P_BUILDING,         // Art Deco concrete grey (visibly grey, not black!)
    P_BUILDING_SHADOW,  // Darker grey for 3D pillar recesses
    P_BUILDING_EDGE,    // Crisp light grey architectural cornices & spires
    P_WINDOW_LIT,       // Warm skyscraper office windows
    P_HAZARD_YEL,       // Trap yellow hazard chevron
    P_HAZARD_BLK,       // Trap black chevron
    P_LOGO_RED,         // No-Ghost prohibition red circle & slash
    P_SLIMER_GRN,       // Fluorescent ectoplasm green
    P_SLIMER_AURA,      // Translucent green halo
    P_SLIMER_LIP,       // Slimer mouth / tongue pink
    P_STREAM_RED,       // Proton stream outer red
    P_STREAM_ORG,       // Proton stream orange flame
    P_STREAM_YEL,       // Proton stream intense yellow
    P_STREAM_CYAN,      // Proton electric lightning cyan
    P_TRAP_CONE,        // Ghost trap capture light beam
    P_PKE_AMBER,        // PKE meter LED amber
    P_PANEL_BG,         // Deep beveled HUD backdrop
    P_COUNT
};

const uint16_t GHOST_RAW[P_COUNT] = {
    0xFFFF,                 // P_WHITE
    0x0000,                 // P_BLACK
    swC565(6, 8, 24),       // P_SKY_TOP
    swC565(32, 14, 52),     // P_SKY_BOT
    swC565(85, 90, 105),    // P_BUILDING (Clean concrete grey)
    swC565(55, 60, 72),     // P_BUILDING_SHADOW (Recessed columns)
    swC565(155, 160, 180),  // P_BUILDING_EDGE (Light architectural trim)
    swC565(255, 230, 130),  // P_WINDOW_LIT
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
    swC565(16, 18, 26)      // P_PANEL_BG
};

// Manhattan skyline at night (visible GREY concrete buildings spanning full width, sky strictly above, zero ghosting)
__attribute__((noinline)) void drawGhostbustersBackdrop(TFT_eSprite &s, int screen, unsigned long nowMs,
                                                        uint8_t state, uint16_t stateMs, const uint16_t *pal) {
    // 0. ALWAYS clear full sprite to prevent ANY persistence between screens
    s.fillSprite(pal[P_BLACK]);

    // Building roofline heights per screen (0 = HoursTens, 1 = HoursOnes, 2 = Center Logo, 3 = MinTens, 4 = MinOnes, 5 = HUD)
    // Screen 2 roof is at y=165 so the No-Ghost logo floats in the starry sky
    int roofY[6] = {122, 112, 165, 118, 128, 104};
    int rY = roofY[screen];

    // 1. Midnight Sky Gradient (strictly from y = 0 down to rY)
    for (int y = 0; y < rY; ++y) {
        uint8_t t = (y * 255) / rY;
        uint16_t sky = (t < 110) ? pal[P_SKY_TOP] : pal[P_SKY_BOT];
        s.drawFastHLine(0, y, TFT_WIDTH, sky);
    }

    // 2. Twinkling stars in the sky
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

    // 3. Solid Manhattan Buildings in VISIBLE CONCRETE GREY (spanning FULL width 0..TFT_WIDTH)
    s.fillRect(0, rY, TFT_WIDTH, 222 - rY, pal[P_BUILDING]);
    s.drawFastHLine(0, rY, TFT_WIDTH, pal[P_BUILDING_EDGE]);
    s.drawFastHLine(0, rY + 1, TFT_WIDTH, pal[P_BUILDING_EDGE]);

    // Architectural depth: recessed vertical columns (3D pillar shadows)
    for (int colX = 14; colX < TFT_WIDTH; colX += 28) {
        s.fillRect(colX, rY + 4, 8, 218 - rY, pal[P_BUILDING_SHADOW]);
    }

    // Decorative mid-building cornices
    if (222 - rY > 40) {
        s.drawFastHLine(0, rY + 36, TFT_WIDTH, pal[P_BUILDING_EDGE]);
    }

    // Architectural crowns / antennas / water towers on rooftops
    if (screen == 0 || screen == 4) {
        // Stepped Art Deco crown
        s.fillRect(24, rY - 16, 32, 16, pal[P_BUILDING]);
        s.drawFastHLine(24, rY - 16, 32, pal[P_BUILDING_EDGE]);
        s.fillRect(32, rY - 26, 16, 10, pal[P_BUILDING]);
        s.drawFastHLine(32, rY - 26, 16, pal[P_BUILDING_EDGE]);
        s.drawFastVLine(40, rY - 38, 12, pal[P_BUILDING_EDGE]); // Antenna spire
        s.fillCircle(40, rY - 38, 2, pal[P_LOGO_RED]);          // Blinking aircraft beacon
    } else if (screen == 1 || screen == 3) {
        // Classic NYC wooden water tower on roof
        s.fillRect(72, rY - 18, 22, 18, pal[P_BUILDING_SHADOW]);
        s.drawFastHLine(70, rY - 18, 26, pal[P_BUILDING_EDGE]);
        s.drawFastVLine(74, rY, 4, pal[P_BUILDING_EDGE]); // Stilt 1
        s.drawFastVLine(90, rY, 4, pal[P_BUILDING_EDGE]); // Stilt 2
        // Antenna
        s.drawFastVLine(25, rY - 24, 24, pal[P_BUILDING_EDGE]);
        s.fillCircle(25, rY - 24, 2, pal[P_LOGO_RED]);
    }

    // Lit windows in buildings (bright warm amber squares)
    for (int wy = rY + 10; wy < 214; wy += 14) {
        for (int wx = 8; wx < TFT_WIDTH - 14; wx += 14) {
            uint16_t wseed = (uint16_t)(wx * 37 + wy * 19 + screen * 43);
            if ((wseed % 3) != 0) { // 66% of windows lit
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

// Draw REAL Slimer (Geleia) + Clean Dynamic Weather on Screen 5 (ONLY ON JANELA 6)
__attribute__((noinline)) void drawGhostbustersHudScreen(TFT_eSprite &s, WeatherService &weather,
                                                         const struct tm &now, unsigned long nowMs,
                                                         uint8_t state, const uint16_t *pal) {
    int cx = TFT_WIDTH / 2; // 67

    // 1. Top Mini-Badge: ECTO RADAR
    swFillChamfer(s, 16, 6, 103, 18, 3, pal[P_SLIMER_GRN]);
    s.fillRect(18, 8, 99, 14, pal[P_PANEL_BG]);
    swDrawTxt(s, "ECTO-1 RADAR", cx, 9, 1, 1, TC_DATUM, pal[P_SLIMER_GRN]);

    // 2. REAL SLIMER (GELEIA) BITMAP (80x80) with floating bobbing wave
    int slimerBob = (int)(sinf(nowMs * 0.005f) * 5.0f);
    int slimerX = (TFT_WIDTH - SLIMER_LOGO_W) / 2; // (135 - 80) / 2 = 27
    int slimerY = 26 + slimerBob;

    // Glowing ectoplasm aura behind Slimer
    s.fillCircle(cx, slimerY + SLIMER_LOGO_H / 2, 38, pal[P_SLIMER_AURA]);

    for (int y = 0; y < SLIMER_LOGO_H; ++y) {
        for (int x = 0; x < SLIMER_LOGO_W; ++x) {
            uint16_t rawColor = pgm_read_word(&SLIMER_LOGO_DATA[y * SLIMER_LOGO_W + x]);
            if (rawColor != 0x0000) {
                s.drawPixel(slimerX + x, slimerY + y, tfts->dimColor(rawColor));
            }
        }
    }

    // 3. Clean, Dynamic Weather & Forecast Panel (y = 112..218)
    swFillChamfer(s, 10, 112, 115, 104, 4, pal[P_SLIMER_GRN]);
    s.fillRect(12, 114, 111, 100, pal[P_PANEL_BG]);

    // Panel Header
    swDrawTxt(s, "PREVISAO DO TEMPO", cx, 118, 1, 1, TC_DATUM, pal[P_STREAM_CYAN]);

    // Current Temperature in BIG Font 4
    char unit = (WeatherService::getUnits().value == "imperial") ? 'F' : 'C';
    float curTemp = weather.getNowTemp();
    int temp = (int)(curTemp + (curTemp >= 0.0f ? 0.5f : -0.5f));
    char tempBuf[16];
    snprintf(tempBuf, sizeof(tempBuf), "%d*%c", temp, unit);
    swDrawTxt(s, tempBuf, cx, 146, 4, 1, MC_DATUM, pal[P_WHITE]);

    // High & Low Temperatures (Clean Max / Min line)
    float highT = weather.getHigh(5);
    float lowT = weather.getLow(5);
    int hi = (int)(highT + (highT >= 0.0f ? 0.5f : -0.5f));
    int lo = (int)(lowT + (lowT >= 0.0f ? 0.5f : -0.5f));
    char hiLoBuf[24];
    snprintf(hiLoBuf, sizeof(hiLoBuf), "MAX %d*  MIN %d*", hi, lo);
    swDrawTxt(s, hiLoBuf, cx, 174, 2, 1, MC_DATUM, pal[P_STREAM_ORG]);

    // Dynamic Ghostbusters Paranormal Activity Status
    if (state == 2) {
        swDrawTxt(s, "CAPTURA ATIVA!", cx, 196, 1, 1, TC_DATUM, pal[P_STREAM_CYAN]);
    } else if (state == 1) {
        // Blinking Class 5 alert
        bool flash = ((nowMs / 100) & 1) != 0;
        swDrawTxt(s, "ALERTA: CLASSE 5!", cx, 196, 1, 1, TC_DATUM, flash ? pal[P_LOGO_RED] : pal[P_WHITE]);
    } else {
        swDrawTxt(s, "ECTO: NORMAL", cx, 196, 1, 1, TC_DATUM, pal[P_SLIMER_GRN]);
    }
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
