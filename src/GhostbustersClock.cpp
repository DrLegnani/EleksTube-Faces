#include "MatrixClock.h"
#include "StarWarsHud.h"
#include "IPSClock.h"
#include <math.h>

// Shared with Backlights: 0 = Normal Ecto patrol, 1 = PKE Class 5 Alert (56-59s), 2 = Cross Streams & Trap (00-02s)
uint8_t ghostbustersState = 0;

namespace {

enum GhostPal {
    P_WHITE = 0,
    P_BLACK,
    P_SKY_TOP,      // Deep midnight navy
    P_SKY_BOT,      // Eerie supernatural violet
    P_BUILDING,     // Manhattan skyline dark silhouette
    P_WINDOW_LIT,   // Warm skyscraper office windows
    P_HAZARD_YEL,   // Trap yellow hazard chevron
    P_HAZARD_BLK,   // Trap black chevron
    P_LOGO_RED,     // No-Ghost prohibition red circle & slash
    P_LOGO_RED_DIM, // Prohibition circle bevel
    P_SLIMER_GRN,   // Fluorescent ectoplasm green
    P_SLIMER_AURA,  // Translucent green halo
    P_SLIMER_LIP,   // Slimer mouth / tongue pink
    P_STREAM_RED,   // Proton stream outer red
    P_STREAM_ORG,   // Proton stream orange flame
    P_STREAM_YEL,   // Proton stream intense yellow
    P_STREAM_CYAN,  // Proton electric lightning cyan
    P_TRAP_CONE,    // Ghost trap capture light beam
    P_PKE_AMBER,    // PKE meter LED amber
    P_COUNT
};

const uint16_t GHOST_RAW[P_COUNT] = {
    0xFFFF,                 // P_WHITE
    0x0000,                 // P_BLACK
    swC565(8, 10, 28),      // P_SKY_TOP
    swC565(28, 14, 45),     // P_SKY_BOT
    swC565(14, 15, 24),     // P_BUILDING
    swC565(255, 225, 120),  // P_WINDOW_LIT
    swC565(250, 205, 15),   // P_HAZARD_YEL
    swC565(20, 20, 20),     // P_HAZARD_BLK
    swC565(225, 20, 24),    // P_LOGO_RED
    swC565(140, 10, 15),    // P_LOGO_RED_DIM
    swC565(80, 255, 60),    // P_SLIMER_GRN
    swC565(25, 110, 25),    // P_SLIMER_AURA
    swC565(230, 80, 110),   // P_SLIMER_LIP
    swC565(220, 30, 20),    // P_STREAM_RED
    swC565(255, 125, 20),   // P_STREAM_ORG
    swC565(255, 230, 50),   // P_STREAM_YEL
    swC565(60, 230, 255),   // P_STREAM_CYAN
    swC565(140, 240, 255),  // P_TRAP_CONE
    swC565(255, 160, 20)    // P_PKE_AMBER
};

// Manhattan skyline at night with 55 Central Park West (Spook Central) & Ghost Trap caution chevrons
__attribute__((noinline)) void drawGhostbustersBackdrop(TFT_eSprite &s, int screen, unsigned long nowMs,
                                                        uint8_t state, uint16_t stateMs, const uint16_t *pal) {
    // 1. Midnight Sky Gradient
    for (int y = 0; y < 192; ++y) {
        uint8_t t = (y * 255) / 192;
        uint16_t sky = (t < 128) ? pal[P_SKY_TOP] : pal[P_SKY_BOT];
        s.drawFastHLine(0, y, TFT_WIDTH, sky);
    }

    // 2. Twinkling stars
    uint16_t seed = (uint16_t)(screen * 71 + 33);
    for (int star = 0; star < 10; ++star) {
        seed = seed * 109 + 89;
        int sx = seed % TFT_WIDTH;
        int sy = (seed / 13) % 110;
        bool twinkle = (((nowMs / 250 + seed) % 7) != 0);
        if (twinkle) {
            s.drawPixel(sx, sy, pal[P_WHITE]);
        }
    }

    // 3. Manhattan Skyline Silhouette (Art Deco Spook Central on screen 2/3)
    int bldH[6] = {55, 80, 115, 95, 75, 60};
    int baseH = bldH[screen];
    s.fillRect(6, 192 - baseH, TFT_WIDTH - 12, baseH, pal[P_BUILDING]);
    s.drawRect(6, 192 - baseH, TFT_WIDTH - 12, baseH, pal[P_SKY_BOT]);

    // Spook Central Temple Roof on screen 2
    if (screen == 2) {
        s.fillRect(25, 192 - baseH - 18, TFT_WIDTH - 50, 18, pal[P_BUILDING]);
        s.drawFastHLine(20, 192 - baseH, TFT_WIDTH - 40, pal[P_STREAM_CYAN]);
        // Temple altar pillars
        for (int p = 30; p < TFT_WIDTH - 30; p += 12) {
            s.fillRect(p, 192 - baseH - 14, 4, 14, pal[P_WINDOW_LIT]);
        }
    }

    // Random lit windows in buildings
    for (int wy = 192 - baseH + 10; wy < 180; wy += 14) {
        for (int wx = 16; wx < TFT_WIDTH - 20; wx += 16) {
            uint16_t wseed = (uint16_t)(wx * 37 + wy * 19 + screen * 43);
            if ((wseed % 4) == 0) {
                s.fillRect(wx, wy, 6, 8, pal[P_WINDOW_LIT]);
            }
        }
    }

    // 4. Ectoplasm green mist curling across bottom of skyline
    uint8_t mistPulse = (uint8_t)(10 + ((nowMs / 80 + screen * 25) % 15));
    for (int my = 175; my < 192; ++my) {
        if ((my + (nowMs / 90)) % 3 == 0) {
            s.drawFastHLine(0, my, TFT_WIDTH, pal[P_SLIMER_AURA]);
        }
    }

    // 5. Ghost Trap caution hazard stripes across the very bottom (y: 222-238)
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

    // 6. Ectoplasm Slime Drips from top bezel
    int dripX[4] = {18, 48, 86, 116};
    int dripLen[4] = {8, 16, 11, 20};
    for (int d = 0; d < 4; ++d) {
        int dx = dripX[d];
        int dl = dripLen[d] + ((d + screen) % 3) * 2;
        s.fillRect(dx - 3, 0, 7, dl - 4, pal[P_SLIMER_GRN]);
        s.fillCircle(dx, dl - 3, 4, pal[P_SLIMER_GRN]);
        s.fillCircle(dx - 1, dl - 4, 1, pal[P_WHITE]); // Slime shine
    }
    s.drawFastHLine(0, 0, TFT_WIDTH, pal[P_SLIMER_GRN]);
    s.drawFastHLine(0, 1, TFT_WIDTH, pal[P_SLIMER_GRN]);
}

// Draw the legendary "NO-GHOST" Logo (Mooglie the Ghost in red prohibition circle)
__attribute__((noinline)) void drawNoGhostLogoCenter(TFT_eSprite &s, const struct tm &now,
                                                     suseconds_t subsecond, unsigned long nowMs,
                                                     uint8_t state, const uint16_t *pal) {
    // Header retro badge
    s.fillRect(8, 6, TFT_WIDTH - 16, 16, pal[P_BLACK]);
    s.drawRect(8, 6, TFT_WIDTH - 16, 16, pal[P_LOGO_RED]);
    s.setTextColor(pal[P_WHITE], pal[P_BLACK]);
    s.setTextSize(1);
    s.drawString("GHOSTBUSTERS", 20, 10);

    int cx = TFT_WIDTH / 2;
    int cy = 95;
    int r = 46;

    // Outer Red Prohibition Ring
    for (int dr = 0; dr < 7; ++dr) {
        s.drawCircle(cx, cy, r - dr, pal[P_LOGO_RED]);
    }
    s.drawCircle(cx, cy, r + 1, pal[P_LOGO_RED_DIM]);

    // Mooglie the Ghost inside
    int gx = cx;
    int gy = cy + 2;

    // Ghost body / head
    s.fillCircle(gx, gy - 8, 22, pal[P_WHITE]);          // Head
    s.fillRoundRect(gx - 20, gy - 12, 40, 32, 12, pal[P_WHITE]); // Body
    s.fillCircle(gx + 12, gy + 18, 12, pal[P_WHITE]);    // Tail curving right

    // Ghost cartoon face (eyes, eyebrows, cheerful mouth)
    // Left eye
    s.fillCircle(gx - 7, gy - 12, 4, pal[P_BLACK]);
    s.fillCircle(gx - 6, gy - 13, 1, pal[P_WHITE]);
    // Right eye (winking/tilted)
    s.fillCircle(gx + 7, gy - 12, 4, pal[P_BLACK]);
    s.fillCircle(gx + 8, gy - 13, 1, pal[P_WHITE]);
    // Eyebrows
    s.drawLine(gx - 11, gy - 18, gx - 4, gy - 16, pal[P_BLACK]);
    s.drawLine(gx + 4, gy - 16, gx + 11, gy - 18, pal[P_BLACK]);
    // Wide happy ghost smile
    s.fillCircle(gx, gy - 3, 6, pal[P_BLACK]);
    s.fillCircle(gx, gy - 4, 5, pal[P_WHITE]);
    s.fillCircle(gx, gy - 1, 3, pal[P_SLIMER_LIP]); // Tongue

    // Ghost Waving Hands (breaking past the ring!)
    // Left hand (waving out)
    s.fillCircle(gx - 26, gy - 16, 9, pal[P_WHITE]);
    s.fillCircle(gx - 32, gy - 20, 4, pal[P_WHITE]); // thumb
    s.fillCircle(gx - 30, gy - 26, 4, pal[P_WHITE]); // finger 1
    s.fillCircle(gx - 24, gy - 28, 4, pal[P_WHITE]); // finger 2
    // Right hand
    s.fillCircle(gx + 26, gy - 4, 9, pal[P_WHITE]);
    s.fillCircle(gx + 32, gy - 8, 4, pal[P_WHITE]);

    // Red Diagonal Slash across the ghost (\)
    for (int t = -4; t <= 4; ++t) {
        int x1 = cx - 33 + t;
        int y1 = cy - 33;
        int x2 = cx + 33 + t;
        int y2 = cy + 33;
        s.drawLine(x1, y1, x2, y2, pal[P_LOGO_RED]);
    }

    // Status Banner Below Logo
    s.fillRect(10, 160, TFT_WIDTH - 20, 22, pal[P_BLACK]);
    s.drawRect(10, 160, TFT_WIDTH - 20, 22, pal[P_STREAM_ORG]);
    s.setTextColor(pal[P_STREAM_YEL], pal[P_BLACK]);
    s.setTextSize(1);
    if (state == 2) {
        s.setTextColor(pal[P_WHITE], pal[P_LOGO_RED]);
        s.drawString("STREAMS CROSSED!", 14, 167);
    } else if (state == 1) {
        s.setTextColor(pal[P_WHITE], pal[P_LOGO_RED]);
        s.drawString("P.K.E. CRITICAL!", 18, 167);
    } else {
        s.drawString("WHO YA GONNA CALL?", 12, 167);
    }

    // Date display
    char dateBuf[16];
    snprintf(dateBuf, sizeof(dateBuf), "%02d/%02d", now.tm_mon + 1, now.tm_mday);
    s.setTextColor(pal[P_STREAM_CYAN], pal[P_BLACK]);
    s.drawString(dateBuf, 50, 190);
}

// Draw PKE Meter + Slimer (Geleia) + Weather info on Screen 5
__attribute__((noinline)) void drawGhostbustersHudScreen(TFT_eSprite &s, WeatherService &weather,
                                                         const struct tm &now, unsigned long nowMs,
                                                         uint8_t state, const uint16_t *pal) {
    // 1. Header Box: Ghostbusters NYC Headquarters
    s.fillRect(6, 6, TFT_WIDTH - 12, 32, pal[P_BLACK]);
    s.drawRect(6, 6, TFT_WIDTH - 12, 32, pal[P_STREAM_CYAN]);
    s.setTextColor(pal[P_STREAM_CYAN], pal[P_BLACK]);
    s.setTextSize(1);
    s.drawString("P.K.E. DETECTOR", 18, 11);
    s.setTextColor(pal[P_WHITE], pal[P_BLACK]);
    s.drawString("NYC FIREHOUSE", 24, 23);

    // 2. Animated PKE Meter Sensor Wings
    int pkeY = 70;
    int pkeX = TFT_WIDTH / 2;
    // Wing elevation: 0 = down, 20 = fully deployed upward
    int elevation = (state == 1) ? 22 : (int)(fabsf(sinf(nowMs * 0.003f)) * 14.0f);

    // Central PKE handle
    s.fillRect(pkeX - 10, pkeY, 20, 26, pal[P_BUILDING]);
    s.drawRect(pkeX - 10, pkeY, 20, 26, pal[P_STREAM_CYAN]);
    // Center PKE screen bar graph
    uint8_t bars = (state == 1) ? 5 : ((nowMs / 180) % 6);
    for (int b = 0; b < 5; ++b) {
        uint16_t bcol = (b < bars) ? (b > 3 ? pal[P_LOGO_RED] : pal[P_PKE_AMBER]) : pal[P_BLACK];
        s.fillRect(pkeX - 7 + b * 3, pkeY + 8, 2, 10, bcol);
    }

    // Left Wing
    s.drawLine(pkeX - 10, pkeY + 6, pkeX - 35, pkeY + 6 - elevation, pal[P_WHITE]);
    s.drawLine(pkeX - 10, pkeY + 8, pkeX - 35, pkeY + 8 - elevation, pal[P_STREAM_ORG]);
    // Left Wing LED cluster
    for (int wled = 0; wled < 3; ++wled) {
        int lx = pkeX - 18 - wled * 7;
        int ly = pkeY + 7 - (elevation * (wled + 1)) / 3;
        bool lit = (state == 1) ? (((nowMs / 70 + wled) % 2) == 0) : (((nowMs / 180 + wled) % 3) == 0);
        s.fillCircle(lx, ly, 2, lit ? pal[P_PKE_AMBER] : pal[P_LOGO_RED]);
    }

    // Right Wing
    s.drawLine(pkeX + 10, pkeY + 6, pkeX + 35, pkeY + 6 - elevation, pal[P_WHITE]);
    s.drawLine(pkeX + 10, pkeY + 8, pkeX + 35, pkeY + 8 - elevation, pal[P_STREAM_ORG]);
    // Right Wing LED cluster
    for (int wled = 0; wled < 3; ++wled) {
        int rx = pkeX + 18 + wled * 7;
        int ry = pkeY + 7 - (elevation * (wled + 1)) / 3;
        bool lit = (state == 1) ? (((nowMs / 70 + wled) % 2) == 0) : (((nowMs / 180 + wled) % 3) == 0);
        s.fillCircle(rx, ry, 2, lit ? pal[P_PKE_AMBER] : pal[P_LOGO_RED]);
    }

    // 3. Floating Animated SLIMER (Geleia)
    int slimerY = 120 + (int)(sinf(nowMs * 0.004f) * 6.0f);
    int slimerX = TFT_WIDTH / 2;

    // Slimer glowing green aura
    s.fillCircle(slimerX, slimerY, 22, pal[P_SLIMER_AURA]);
    // Body potato shape
    s.fillRoundRect(slimerX - 16, slimerY - 14, 32, 28, 12, pal[P_SLIMER_GRN]);
    s.fillCircle(slimerX, slimerY - 10, 14, pal[P_SLIMER_GRN]); // top head
    // Fat rolls / bottom blob
    s.fillCircle(slimerX - 6, slimerY + 12, 8, pal[P_SLIMER_GRN]);
    s.fillCircle(slimerX + 7, slimerY + 11, 7, pal[P_SLIMER_GRN]);

    // Slimer cartoon eyes (derpy expression)
    s.fillCircle(slimerX - 6, slimerY - 12, 4, pal[P_WHITE]);
    s.fillCircle(slimerX + 6, slimerY - 12, 4, pal[P_WHITE]);
    s.fillCircle(slimerX - 5, slimerY - 12, 2, pal[P_BLACK]);
    s.fillCircle(slimerX + 5, slimerY - 12, 2, pal[P_BLACK]);

    // Huge open eating mouth
    s.fillRoundRect(slimerX - 10, slimerY - 4, 20, 12, 5, pal[P_BLACK]);
    s.fillCircle(slimerX, slimerY + 2, 4, pal[P_SLIMER_LIP]); // tongue
    s.drawPixel(slimerX - 6, slimerY - 3, pal[P_WHITE]);     // tooth 1
    s.drawPixel(slimerX + 6, slimerY - 3, pal[P_WHITE]);     // tooth 2

    // Slimer stubby arms
    s.fillRoundRect(slimerX - 22, slimerY - 6, 8, 6, 3, pal[P_SLIMER_GRN]);
    s.fillRoundRect(slimerX + 15, slimerY - 8, 8, 6, 3, pal[P_SLIMER_GRN]);

    // 4. Live Weather & Temperature
    s.fillRect(8, 168, TFT_WIDTH - 16, 44, pal[P_BLACK]);
    s.drawRect(8, 168, TFT_WIDTH - 16, 44, pal[P_SLIMER_GRN]);

    s.setTextColor(pal[P_SLIMER_GRN], pal[P_BLACK]);
    s.setTextSize(1);
    s.drawString("P.K.E. ENTITY:", 14, 172);

    if (state == 1 || state == 2) {
        s.setTextColor(pal[P_WHITE], pal[P_LOGO_RED]);
        s.drawString("CLASS 5 VAPOR!", 14, 185);
    } else {
        s.setTextColor(pal[P_STREAM_YEL], pal[P_BLACK]);
        s.drawString("SLIMER SPOTTED", 14, 185);
    }

    // Weather readout
    char unit = (WeatherService::getUnits().value == "imperial") ? 'F' : 'C';
    int temp = (int)(weather.getNowTemp() + (weather.getNowTemp() >= 0.0f ? 0.5f : -0.5f));
    char wBuf[20];
    snprintf(wBuf, sizeof(wBuf), "NYC %d*%c ENTITY OK", temp, unit);
    s.setTextColor(pal[P_WHITE], pal[P_BLACK]);
    s.drawString(wBuf, 14, 198);
}

// Draw stylized high-energy Proton Wand Stream Digits
__attribute__((noinline)) void drawGhostbustersDigit(TFT_eSprite &s, int digit, unsigned long nowMs,
                                                     uint8_t state, const uint16_t *pal) {
    if (digit < 0 || digit > 9) return;

    // 7-segment segment mapping: a, b, c, d, e, f, g
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
    int th = 14; // segment thickness

    auto drawSeg = [&](int sx, int sy, int sw, int sh, bool horiz) {
        // Outer fiery proton stream aura (Red/Orange)
        s.fillRoundRect(sx - 2, sy - 2, sw + 4, sh + 4, 4, pal[P_STREAM_RED]);
        // Mid layer (Proton flame orange)
        s.fillRoundRect(sx, sy, sw, sh, 3, pal[P_STREAM_ORG]);
        // Inner core (Intense yellow)
        if (horiz) {
            s.fillRect(sx + 3, sy + 3, sw - 6, sh - 6, pal[P_STREAM_YEL]);
            // Lightning filament (electric white/cyan center)
            s.drawFastHLine(sx + 5, sy + sh / 2, sw - 10, pal[P_STREAM_CYAN]);
            s.drawFastHLine(sx + 6, sy + sh / 2 - 1, sw - 12, pal[P_WHITE]);
        } else {
            s.fillRect(sx + 3, sy + 3, sw - 6, sh - 6, pal[P_STREAM_YEL]);
            s.drawFastVLine(sx + sw / 2, sy + 5, sh - 10, pal[P_STREAM_CYAN]);
            s.drawFastVLine(sx + sw / 2 - 1, sy + 6, sh - 12, pal[P_WHITE]);
        }
    };

    // Segments:
    // a: top horizontal
    if (mask & 0x01) drawSeg(x0 + 4, y0, w - 8, th, true);
    // b: top right vertical
    if (mask & 0x02) drawSeg(x0 + w - th, y0 + 4, th, (h / 2) - 4, false);
    // c: bottom right vertical
    if (mask & 0x04) drawSeg(x0 + w - th, y0 + (h / 2), th, (h / 2) - 4, false);
    // d: bottom horizontal
    if (mask & 0x08) drawSeg(x0 + 4, y0 + h - th, w - 8, th, true);
    // e: bottom left vertical
    if (mask & 0x10) drawSeg(x0, y0 + (h / 2), th, (h / 2) - 4, false);
    // f: top left vertical
    if (mask & 0x20) drawSeg(x0, y0 + 4, th, (h / 2) - 4, false);
    // g: middle horizontal
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
    // 1. Ghost Trap light beam shooting upward on screens 2 & 3
    if (screen == 2 || screen == 3) {
        int trapX = (screen == 2) ? TFT_WIDTH - 20 : 20;
        for (int y = 50; y < 220; ++y) {
            int beamW = (220 - y) / 3;
            int bx = trapX - beamW / 2;
            s.drawFastHLine(bx, y, beamW, pal[P_TRAP_CONE]);
        }
    }

    // 2. Chaotic wavy Proton Stream crossing all screens
    // Sine-wave proton stream with fluctuating frequency
    for (int x = 0; x < TFT_WIDTH; ++x) {
        int globalX = screen * TFT_WIDTH + x;
        float wave = sinf(globalX * 0.04f + nowMs * 0.02f) * 26.0f;
        int streamY = 110 + (int)wave;

        // Outer red flame
        s.fillCircle(x, streamY, 6, pal[P_STREAM_RED]);
        // Inner orange
        s.fillCircle(x, streamY, 4, pal[P_STREAM_ORG]);
        // Lightning core
        s.drawPixel(x, streamY, pal[P_WHITE]);
        s.drawPixel(x, streamY - 1, pal[P_STREAM_CYAN]);

        // Secondary counter-stream crossing
        float wave2 = cosf(globalX * 0.035f - nowMs * 0.025f) * 32.0f;
        int stream2Y = 125 + (int)wave2;
        s.fillCircle(x, stream2Y, 5, pal[P_STREAM_ORG]);
        s.fillCircle(x, stream2Y, 2, pal[P_STREAM_YEL]);
        s.drawPixel(x, stream2Y, pal[P_STREAM_CYAN]);
    }

    // 3. Shockwave sparks and plasma bolts
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

        // When crossing streams at :00 - :02, draw plasma streams across all screens
        if (ghostbustersState == 2) {
            drawStreamCrossAnimation(sprite, i, stateMs, nowMs, pal);
        }

        tfts->chip_select.setDigit(SW_SCREENS[i]);
        sprite.pushSprite(0, 0);
    }
    tfts->release();
}
