#include "MatrixClock.h"
#include "StarWarsHud.h"
#include "IPSClock.h"

// Shared with Backlights: 0 = Normal Time Circuits, 1 = 88 MPH 1.21 GW lightning buildup (56-59s), 2 = Time Jump Fire Trails (00-02s)
uint8_t deloreanState = 0;

namespace {

enum DeloreanPal {
    P_WHITE = 0,
    P_BLACK,
    P_STEEL_DARK,   // Brushed stainless chassis shadow
    P_STEEL_MID,    // DeLorean DMC-12 brushed steel body
    P_STEEL_LIGHT,  // Stainless specular highlight
    P_PANEL_BG,     // Recessed dark LED housing
    P_SEG_OFF,      // Unlit 7-segment ghost segment
    P_DYMO_RED,     // Embossed red Dymo label tape
    P_DYMO_BLK,     // Embossed black Dymo label tape
    P_RED_AURA,     // Destination Time red LED glow
    P_RED_LED,      // Destination Time bright red LED
    P_RED_CORE,     // Hot orange-white LED center
    P_GRN_AURA,     // Present Time green LED glow
    P_GRN_LED,      // Present Time bright green LED
    P_GRN_CORE,     // Pale green-white LED center
    P_AMBER_LED,    // Last Time Departed / Flux warm amber
    P_FLUX_CYAN,    // 1.21 GW temporal lightning cyan
    P_FLUX_BLUE,    // Temporal electric blue
    P_FIRE_ORANGE,  // 88 MPH tire fire trail orange
    P_COUNT
};

const uint16_t DELOREAN_RAW[P_COUNT] = {
    0xFFFF,                 // P_WHITE
    0x0000,                 // P_BLACK
    swC565(36, 40, 46),     // P_STEEL_DARK
    swC565(74, 82, 92),     // P_STEEL_MID
    swC565(150, 162, 176),  // P_STEEL_LIGHT
    swC565(12, 14, 18),     // P_PANEL_BG
    swC565(28, 22, 24),     // P_SEG_OFF
    swC565(175, 22, 26),    // P_DYMO_RED
    swC565(20, 22, 26),     // P_DYMO_BLK
    swC565(90, 10, 8),      // P_RED_AURA
    swC565(255, 38, 18),    // P_RED_LED
    swC565(255, 210, 160),  // P_RED_CORE
    swC565(8, 80, 20),      // P_GRN_AURA
    swC565(35, 245, 65),    // P_GRN_LED
    swC565(200, 255, 210),  // P_GRN_CORE
    swC565(255, 185, 20),   // P_AMBER_LED
    swC565(110, 235, 255),  // P_FLUX_CYAN
    swC565(25, 120, 255),   // P_FLUX_BLUE
    swC565(255, 110, 15)    // P_FIRE_ORANGE
};

// Draw jagged temporal lightning bolt between two points
__attribute__((noinline)) void drawLightningBolt(TFT_eSprite &s, int x0, int y0, int x1, int y1,
                                                 unsigned long seed, const uint16_t *pal) {
    int px = x0, py = y0;
    for (int step = 1; step <= 5; ++step) {
        int nx = x0 + ((x1 - x0) * step) / 5;
        int ny = y0 + ((y1 - y0) * step) / 5;
        if (step < 5) {
            int jitter = (int)(((seed >> (step * 3)) & 15)) - 7;
            nx += jitter;
            ny += (jitter / 2);
        }
        s.drawLine(px - 1, py, nx - 1, ny, pal[P_FLUX_BLUE]);
        s.drawLine(px + 1, py, nx + 1, ny, pal[P_FLUX_CYAN]);
        s.drawLine(px, py, nx, ny, pal[P_WHITE]);
        px = nx;
        py = ny;
    }
}

// Brushed stainless-steel DeLorean Time Circuits backdrop + 88 MPH lightning & fire trails
__attribute__((noinline)) void drawDeloreanBackdrop(TFT_eSprite &s, int screen, unsigned long nowMs,
                                                    uint8_t state, uint16_t stateMs, const uint16_t *pal) {
    // 1. Brushed DMC-12 stainless steel chassis frame
    s.fillSprite(pal[P_STEEL_DARK]);
    for (int y = 0; y < TFT_HEIGHT; y += 3) {
        s.drawFastHLine(0, y, TFT_WIDTH, pal[(y & 1) ? P_STEEL_MID : P_STEEL_DARK]);
    }
    // Beveled metallic outer frame & corner rivets
    s.drawRect(2, 2, TFT_WIDTH - 4, TFT_HEIGHT - 4, pal[P_STEEL_LIGHT]);
    s.drawRect(4, 4, TFT_WIDTH - 8, TFT_HEIGHT - 8, pal[P_BLACK]);
    const int rvX[4] = {8, 126, 8, 126};
    const int rvY[4] = {8, 8, 231, 231};
    for (int r = 0; r < 4; ++r) {
        s.fillCircle(rvX[r], rvY[r], 2, pal[P_STEEL_LIGHT]);
        s.drawPixel(rvX[r] + 1, rvY[r] + 1, pal[P_BLACK]);
    }

    // High-voltage time-circuit wiring bundles running along the top & bottom bulkheads
    s.drawFastHLine(14, 7, 107, pal[P_DYMO_RED]);
    s.drawFastHLine(14, 9, 107, pal[P_AMBER_LED]);
    s.drawFastHLine(14, 11, 107, pal[P_FLUX_BLUE]);

    // 2. Sec 56-59 (State 1): 88 MPH Temporal Lightning Arcing across the stainless steel!
    if (state == 1) {
        if (((nowMs / 70u + screen) & 1) != 0) {
            drawLightningBolt(s, 6, 18, 128, 220, nowMs + screen * 97u, pal);
        }
        drawLightningBolt(s, 125, 14, 10, 120, nowMs * 3u + screen * 53u, pal);
    }

    // 3. Sec 00-02 (State 2): 88 MPH TIME JUMP — Twin Flaming Tire Tracks burning across all 6 screens!
    if (state == 2) {
        int flameFront = (int)((stateMs * 1050u) / 3000u);
        int scrX0 = screen * 150;
        int localEnd = flameFront - scrX0;
        if (localEnd > 0) {
            if (localEnd > TFT_WIDTH) localEnd = TFT_WIDTH;
            // Twin horizontal tire fire tracks at y = 204..216 and y = 222..234
            for (int track = 0; track < 2; ++track) {
                int ty = 206 + track * 17;
                s.fillRect(0, ty, localEnd, 9, pal[P_RED_LED]);
                s.fillRect(0, ty + 2, localEnd, 6, pal[P_FIRE_ORANGE]);
                s.fillRect(0, ty + 4, localEnd, 3, pal[P_AMBER_LED]);
                // Dancing flame tongues along the track
                for (int fx = 4; fx < localEnd; fx += 9) {
                    int fh = 4 + ((int)((nowMs / 35u + fx * 7 + track * 13) % 10));
                    s.fillTriangle(fx - 3, ty, fx + 3, ty, fx, ty - fh, pal[P_FIRE_ORANGE]);
                    s.drawLine(fx, ty, fx, ty - fh + 2, pal[P_WHITE]);
                }
            }
        }
        // Temporal white-cyan flash border
        s.drawRect(0, 0, TFT_WIDTH, TFT_HEIGHT, pal[P_FLUX_CYAN]);
    }
}

// DeLorean Time-Circuit 7-Segment LED Digit (Screens 0, 1 = Red Destination; Screens 3, 4 = Green Present)
__attribute__((noinline)) void drawDeloreanDigit(TFT_eSprite &s, int screen, int digit,
                                                 unsigned long nowMs, uint8_t state, const uint16_t *pal) {
    // Embossed Dymo label strip above digit housing
    bool isHour = (screen < 2);
    const char *label = isHour ? (screen == 0 ? "DEST. HOUR" : "TIME CIRCUITS")
                               : (screen == 3 ? "PRES. MIN"  : "1.21 GIGAWATT");
    swFillChamfer(s, 14, 15, 107, 15, 2, pal[isHour ? P_DYMO_RED : P_DYMO_BLK]);
    s.drawRect(14, 15, 107, 15, pal[P_STEEL_LIGHT]);
    swDrawTxt(s, label, TFT_WIDTH / 2, 19, 1, 1, TC_DATUM, pal[P_WHITE]);

    // Recessed black metal LED window (x = 12..122, y = 33..202)
    swFillChamfer(s, 10, 33, 115, 170, 5, pal[P_BLACK]);
    s.drawRect(12, 35, 111, 166, pal[P_STEEL_MID]);
    s.fillRect(14, 37, 107, 162, pal[P_PANEL_BG]);

    static const uint8_t segMask[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
    static const uint8_t segs[7][5] = {
        {28, 44, 79, 16, 1},  // A
        {93, 50, 16, 70, 0},  // B
        {93, 122, 16, 70, 0}, // C
        {28, 178, 79, 16, 1}, // D
        {26, 122, 16, 70, 0}, // E
        {26, 50, 16, 70, 0},  // F
        {30, 111, 75, 16, 1}  // G
    };

    uint8_t m = (digit >= 0 && digit <= 9) ? segMask[digit] : 0;

    // Pass 1: Unlit dark 7-segment ghost cavities (authentic 80s LED display look)
    for (int b = 0; b < 7; ++b) {
        if (m & (1 << b)) continue;
        swFillChamfer(s, segs[b][0], segs[b][1], segs[b][2], segs[b][3], 4, pal[P_SEG_OFF]);
    }

    // Choose LED color: Hours = Red/Orange LED, Minutes = Bright Green LED (or Temporal Cyan during jump!)
    uint16_t auraCol = pal[state == 2 ? P_FLUX_BLUE : (isHour ? P_RED_AURA : P_GRN_AURA)];
    uint16_t bodyCol = pal[state == 2 ? P_FLUX_CYAN : (isHour ? P_RED_LED  : P_GRN_LED)];
    uint16_t coreCol = pal[state == 2 ? P_WHITE     : (isHour ? P_RED_CORE : P_GRN_CORE)];

    // Pass 2: Lit LED segments with warm glow halo & bright inner filament bar
    for (int b = 0; b < 7; ++b) {
        if (!(m & (1 << b))) continue;
        int x = segs[b][0], y = segs[b][1], w = segs[b][2], h = segs[b][3], horiz = segs[b][4];
        swFillChamfer(s, x - 2, y - 2, w + 4, h + 4, 5, auraCol);
        swFillChamfer(s, x, y, w, h, 4, bodyCol);
        if (horiz) {
            swFillChamfer(s, x + 5, y + 4, w - 10, h - 8, 2, coreCol);
            s.drawFastHLine(x + 8, y + h / 2, w - 16, pal[P_WHITE]);
        } else {
            swFillChamfer(s, x + 4, y + 5, w - 8, h - 10, 2, coreCol);
            s.drawFastVLine(x + w / 2, y + 8, h - 16, pal[P_WHITE]);
        }
    }

    // Bottom Dymo status plate when not covered by fire trails
    if (state != 2) {
        swFillChamfer(s, 18, 209, 99, 18, 2, pal[P_DYMO_BLK]);
        s.drawRect(18, 209, 99, 18, pal[P_STEEL_MID]);
        swDrawTxt(s, isHour ? "OCT 26 1985" : "NOV 05 1955", TFT_WIDTH / 2, 214, 1, 1, TC_DATUM,
                  pal[isHour ? P_AMBER_LED : P_GRN_LED]);
    }
    (void)nowMs;
}

// Screen 2 (Center): Iconic Y-Shaped FLUX CAPACITOR + Temporal Pulse Animation + Date Readout
__attribute__((noinline)) void drawFluxCapacitorScreen(TFT_eSprite &s, struct tm &now, suseconds_t subsec,
                                                       unsigned long nowMs, uint8_t state, const uint16_t *pal) {
    // Top Red Dymo Label "FLUX CAPACITOR"
    swFillChamfer(s, 14, 14, 107, 15, 2, pal[P_DYMO_RED]);
    s.drawRect(14, 14, 107, 15, pal[P_WHITE]);
    swDrawTxt(s, "FLUX CAPACITOR", TFT_WIDTH / 2, 18, 1, 1, TC_DATUM, pal[P_WHITE]);

    // Flux Capacitor Rounded Industrial Enclosure (x = 14..120, y = 33..182)
    swFillChamfer(s, 12, 33, 111, 150, 10, pal[P_STEEL_MID]);
    swFillChamfer(s, 16, 37, 103, 142, 8, pal[P_BLACK]);
    swFillChamfer(s, 20, 41, 95, 134, 6, pal[P_PANEL_BG]);

    const int cx = 67, cy = 108; // Center junction of the Y
    const int xTL = 35, yTL = 58;
    const int xTR = 99, yTR = 58;
    const int xB  = 67, yB  = 160;

    // Red/Orange high-voltage cables feeding the 3 glass vacuum electrodes
    s.drawLine(24, 46, xTL, yTL, pal[P_DYMO_RED]);
    s.drawLine(110, 46, xTR, yTR, pal[P_DYMO_RED]);
    s.drawLine(42, 168, xB, yB, pal[P_AMBER_LED]);

    // 3 Glass Vacuum Tubes arranged in the Y configuration
    for (int d = -3; d <= 3; ++d) {
        s.drawLine(xTL + d, yTL, cx + d, cy, pal[P_STEEL_DARK]);
        s.drawLine(xTR + d, yTR, cx + d, cy, pal[P_STEEL_DARK]);
        s.drawLine(xB + d, yB, cx + d, cy, pal[P_STEEL_DARK]);
    }
    // Brass/Gold electrode caps at the 3 outer ends of the Y
    s.fillCircle(xTL, yTL, 7, pal[P_AMBER_LED]);
    s.fillCircle(xTR, yTR, 7, pal[P_AMBER_LED]);
    s.fillCircle(xB,  yB,  7, pal[P_AMBER_LED]);
    s.fillCircle(xTL, yTL, 4, pal[P_DYMO_RED]);
    s.fillCircle(xTR, yTR, 4, pal[P_DYMO_RED]);
    s.fillCircle(xB,  yB,  4, pal[P_DYMO_RED]);

    // Animated Flux Energy Pulses converging from the 3 outer caps into the center (cx, cy)!
    int speedDiv = (state >= 1) ? 8 : 22;
    for (int p = 0; p < 3; ++p) {
        int prog = (int)(((nowMs / speedDiv) + p * 33) % 100); // 0..99% from outer cap to center
        int px1 = xTL + ((cx - xTL) * prog) / 100;
        int py1 = yTL + ((cy - yTL) * prog) / 100;
        int px2 = xTR + ((cx - xTR) * prog) / 100;
        int py2 = yTR + ((cy - yTR) * prog) / 100;
        int px3 = xB  + ((cx - xB)  * prog) / 100;
        int py3 = yB  + ((cy - yB)  * prog) / 100;

        uint16_t pCol = pal[(state >= 1) ? P_FLUX_CYAN : P_AMBER_LED];
        s.fillCircle(px1, py1, 3, pCol); s.drawPixel(px1, py1, pal[P_WHITE]);
        s.fillCircle(px2, py2, 3, pCol); s.drawPixel(px2, py2, pal[P_WHITE]);
        s.fillCircle(px3, py3, 3, pCol); s.drawPixel(px3, py3, pal[P_WHITE]);
    }

    // Blazing central Y-singularity core!
    bool pulseOn = (subsec < 500000L) || (state >= 1);
    int coreR = (state == 2) ? 14 : (state == 1 ? 10 : (pulseOn ? 8 : 5));
    s.fillCircle(cx, cy, coreR + 3, pal[state >= 1 ? P_FLUX_BLUE : P_RED_AURA]);
    s.fillCircle(cx, cy, coreR, pal[state >= 1 ? P_FLUX_CYAN : P_AMBER_LED]);
    s.fillCircle(cx, cy, coreR / 2 + 1, pal[P_WHITE]);

    // Yellow Dymo warning label at bottom of Flux box
    swDrawTxt(s, "SHIELD EYES", cx, 166, 1, 1, TC_DATUM, pal[P_AMBER_LED]);

    // Bottom Date Display in Amber "LAST TIME DEPARTED" LED box (y = 188..233)
    swFillChamfer(s, 14, 187, 107, 45, 4, pal[P_BLACK]);
    s.drawRect(15, 188, 105, 43, pal[P_STEEL_LIGHT]);
    char dateStr[6] = {
        char('0' + now.tm_mday / 10), char('0' + now.tm_mday % 10), '.',
        char('0' + now.tm_mon / 10), char('0' + now.tm_mon % 10), 0
    };
    swDrawTxt(s, dateStr, cx, 210, 4, 1, MC_DATUM, pal[state == 2 ? P_FLUX_CYAN : P_AMBER_LED]);
}

// Screen 5 (Rightmost): 88.0 MPH Digital Speedometer + 1.21 Gigawatts Plutonium Gauge + Weather
__attribute__((noinline)) void drawDeloreanSpeedoScreen(TFT_eSprite &s, WeatherService &weather,
                                                        struct tm &now, unsigned long nowMs,
                                                        uint8_t state, const uint16_t *pal) {
    // 1. TOP (y = 14..84): Digital 88.0 MPH Speedometer!
    // Speed climbs from 12.0 MPH at :00s up to 88.0 MPH at :56..:59s (and locks at 88.8 / 88.0 during jump!)
    swFillChamfer(s, 10, 14, 115, 70, 5, pal[P_BLACK]);
    s.drawRect(12, 16, 111, 66, pal[P_STEEL_LIGHT]);
    swFillChamfer(s, 16, 19, 64, 12, 2, pal[P_DYMO_RED]);
    swDrawTxt(s, "SPEED MPH", 48, 21, 1, 1, TC_DATUM, pal[P_WHITE]);

    int mphTenths = (state >= 1) ? 880 : (150 + (now.tm_sec * 730) / 56);
    char mphBuf[12];
    snprintf(mphBuf, sizeof(mphBuf), "%02d.%d", mphTenths / 10, mphTenths % 10);
    swDrawTxt(s, mphBuf, 67, 56, 4, 1, MC_DATUM, pal[state >= 1 ? P_FLUX_CYAN : P_RED_LED]);

    // 2. MIDDLE (y = 89..168): Plutonium Chamber & Atmospheric Weather Telemetry
    swFillChamfer(s, 10, 89, 115, 78, 5, pal[P_BLACK]);
    s.drawRect(12, 91, 111, 74, pal[P_STEEL_MID]);
    swFillChamfer(s, 16, 94, 103, 13, 2, pal[P_DYMO_BLK]);
    swDrawTxt(s, state >= 1 ? "1.21 GIGAWATTS!" : "PLUTONIUM / TEMP", 67, 97, 1, 1, TC_DATUM,
              pal[state >= 1 ? P_FLUX_CYAN : P_AMBER_LED]);

    char unit = (WeatherService::getUnits().value == "imperial") ? 'F' : 'C';
    float vals[3] = {weather.getNowTemp(), weather.getHigh(5), weather.getLow(5)};
    int ints[3];
    for (int k = 0; k < 3; ++k) ints[k] = (int)(vals[k] + (vals[k] >= 0.0f ? 0.5f : -0.5f));

    char buf[16];
    snprintf(buf, sizeof(buf), "%d%c", ints[0], unit);
    swDrawTxt(s, buf, 42, 126, 4, 1, MC_DATUM, pal[P_GRN_LED]);

    snprintf(buf, sizeof(buf), "HI:%d", ints[1]);
    swDrawTxt(s, buf, 94, 118, 2, 1, MC_DATUM, pal[P_RED_LED]);
    snprintf(buf, sizeof(buf), "LO:%d", ints[2]);
    swDrawTxt(s, buf, 94, 136, 2, 1, MC_DATUM, pal[P_FLUX_CYAN]);

    // Roentgen / Gigawatt segmented LED meter bar at bottom of middle box (y = 149..160)
    for (int seg = 0; seg < 12; ++seg) {
        int sx = 17 + seg * 8;
        bool lit = (seg <= (now.tm_sec * 11) / 59) || (state >= 1);
        uint16_t sCol = pal[!lit ? P_SEG_OFF : (seg < 6 ? P_GRN_LED : (seg < 9 ? P_AMBER_LED : P_RED_LED))];
        s.fillRect(sx, 150, 6, 9, sCol);
    }

    // 3. BOTTOM (y = 172..233): Iconic California "OUTATIME" Barcode License Plate!
    swFillChamfer(s, 10, 172, 115, 58, 5, pal[P_STEEL_LIGHT]);
    s.fillRect(13, 175, 109, 52, pal[P_PANEL_BG]);
    // Reflective orange & blue barcode stripes
    for (int bx = 16; bx < 118; bx += 5) {
        s.fillRect(bx, 178, 3, 8, pal[(bx & 1) ? P_FIRE_ORANGE : P_FLUX_CYAN]);
        s.fillRect(bx, 216, 3, 8, pal[(bx & 1) ? P_FLUX_BLUE : P_AMBER_LED]);
    }
    swDrawTxt(s, "OUTATIME", 67, 198, 2, 1, MC_DATUM, pal[P_WHITE]);
    (void)nowMs;
}

} // namespace

void MatrixClock::renderDelorean(IPSClock &clock, TimeSync &time, WeatherService &weather) {
    unsigned long nowMs = millis();
    if (nowMs - lastFrame < 45) return;
    lastFrame = nowMs;
    ++frame;

    struct tm now;
    suseconds_t subsecond;
    time.getLocalTime(&now, &subsecond);

    uint16_t stateMs = 0;
    if (now.tm_sec <= 2) {
        deloreanState = 2;
        stateMs = (uint16_t)(now.tm_sec * 1000u + (uint32_t)(subsecond / 1000));
    } else if (now.tm_sec >= 56) {
        deloreanState = 1;
        stateMs = (uint16_t)((now.tm_sec - 56) * 1000u + (uint32_t)(subsecond / 1000));
    } else {
        deloreanState = 0;
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
    swDimPalette(DELOREAN_RAW, pal, P_COUNT);

    for (int i = 0; i < 6; ++i) {
        drawDeloreanBackdrop(sprite, i, nowMs, deloreanState, stateMs, pal);

        if (i == 2) {
            drawFluxCapacitorScreen(sprite, now, subsecond, nowMs, deloreanState, pal);
        } else if (i == 5) {
            drawDeloreanSpeedoScreen(sprite, weather, now, nowMs, deloreanState, pal);
        } else {
            bool blankHour = (i == 0 && hour < 10 && !IPSClock::getLeadingZero().value);
            drawDeloreanDigit(sprite, i, blankHour ? -1 : digits[i], nowMs, deloreanState, pal);
        }

        tfts->chip_select.setDigit(SW_SCREENS[i]);
        sprite.pushSprite(0, 0);
    }
    tfts->release();
}
