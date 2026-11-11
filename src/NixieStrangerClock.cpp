#include "MatrixClock.h"
#include "StarWarsHud.h"
#include "IPSClock.h"

// Shared with Backlights: 0 = Warm Vintage IN-18 Nixie Tube, 1 = Upside Down Flicker & Red Lightning (55-59s), 2 = Vecna's Curse / Mind Flayer Storm (00-02s)
uint8_t nixieStrangerState = 0;

namespace {

enum NixiePal {
    P_WHITE = 0,
    P_BLACK,
    P_BG_WARM,      // Dark warm vacuum interior
    P_BG_UPSIDE,    // Deep abyssal blue-black Upside Down sky
    P_GLASS_EDGE,   // Cylindrical glass bulb rim reflection
    P_GLASS_SPEC,   // Bright specular highlight on left glass wall
    P_ANODE_GRID,   // Fine honeycomb metal anode mesh
    P_CATHODE_OFF,  // Unlit stacked copper/steel cathode wire
    P_GAS_VIOLET,   // Subtle neon-argon outer ionization aura
    P_NIXIE_AURA,   // Deep orange-red neon plasma halo
    P_NIXIE_ORANGE, // Classic IN-18 glowing neon orange cathode
    P_NIXIE_GOLD,   // Bright yellow-amber filament core
    P_CERAMIC,      // White/grey ceramic insulator bead
    P_BRASS_PIN,    // Gold/brass base contact pins
    P_VINE_DARK,    // Upside Down organic creeping tendril
    P_SPORE_GREY,   // Floating Upside Down ash/spores
    P_RIFT_RED,     // Crimson Mind Flayer / Vecna lightning
    P_RIFT_CORE,    // Bright scarlet-white lightning core
    P_COUNT
};

const uint16_t NIXIE_RAW[P_COUNT] = {
    0xFFFF,                 // P_WHITE
    0x0000,                 // P_BLACK
    swC565(14, 6, 4),       // P_BG_WARM
    swC565(8, 12, 28),      // P_BG_UPSIDE
    swC565(52, 60, 72),     // P_GLASS_EDGE
    swC565(135, 155, 180),  // P_GLASS_SPEC
    swC565(34, 24, 20),     // P_ANODE_GRID
    swC565(46, 36, 32),     // P_CATHODE_OFF
    swC565(64, 14, 38),     // P_GAS_VIOLET
    swC565(145, 24, 8),     // P_NIXIE_AURA
    swC565(255, 92, 14),    // P_NIXIE_ORANGE
    swC565(255, 210, 95),   // P_NIXIE_GOLD
    swC565(180, 172, 160),  // P_CERAMIC
    swC565(195, 150, 45),   // P_BRASS_PIN
    swC565(26, 16, 28),     // P_VINE_DARK
    swC565(140, 155, 170),  // P_SPORE_GREY
    swC565(235, 16, 28),    // P_RIFT_RED
    swC565(255, 175, 185)   // P_RIFT_CORE
};

// Draw realistic IN-18 glass vacuum envelope, honeycomb anode mesh, bakelite base & Upside Down effects
__attribute__((noinline)) void drawNixieEnvelope(TFT_eSprite &s, int screen, unsigned long nowMs,
                                                 uint8_t state, uint16_t stateMs, const uint16_t *pal) {
    bool upside = (state >= 1);
    s.fillSprite(pal[upside ? P_BG_UPSIDE : P_BLACK]);

    // 1. Glass Tube Envelope (x = 9..125, y = 12..216) with domed top & exhaust pip
    swFillChamfer(s, 9, 14, 117, 202, 14, pal[upside ? P_BG_UPSIDE : P_BG_WARM]);
    // Exhaust glass tip at top center (cx = 67, y = 4..14)
    s.fillTriangle(61, 14, 73, 14, 67, 4, pal[P_GLASS_EDGE]);
    s.drawLine(67, 4, 62, 14, pal[P_GLASS_SPEC]);

    // Cylindrical glass side outlines & left-side curved specular reflection
    s.drawFastVLine(9, 26, 180, pal[P_GLASS_EDGE]);
    s.drawFastVLine(125, 26, 180, pal[P_GLASS_EDGE]);
    s.drawLine(9, 26, 22, 14, pal[P_GLASS_SPEC]);
    s.drawLine(125, 26, 112, 14, pal[P_GLASS_EDGE]);
    s.drawFastHLine(22, 14, 90, pal[P_GLASS_EDGE]);
    // Specular glass streak on left side of bulb
    s.drawFastVLine(14, 32, 155, pal[P_GLASS_SPEC]);
    s.drawFastVLine(15, 42, 125, pal[P_GLASS_EDGE]);

    // 2. Fine Honeycomb / Diamond Metal Anode Mesh inside the glass tube
    uint16_t meshCol = pal[upside ? P_VINE_DARK : P_ANODE_GRID];
    for (int my = 24; my < 204; my += 14) {
        s.drawFastHLine(16, my, 103, meshCol);
        for (int mx = 20 + ((my / 14) & 1) * 7; mx < 116; mx += 14) {
            s.drawLine(mx, my, mx + 7, my + 7, meshCol);
            s.drawLine(mx + 7, my + 7, mx, my + 14, meshCol);
        }
    }

    // 3. Bakelite / Ceramic Tube Socket Base & Brass Contact Pins (y = 212..239)
    swFillChamfer(s, 12, 212, 111, 18, 4, pal[P_CATHODE_OFF]);
    s.drawFastHLine(14, 212, 107, pal[P_GLASS_EDGE]);
    s.fillRect(16, 216, 103, 12, pal[P_BLACK]);
    // 7 Brass anode/cathode pins extending into socket at bottom
    for (int p = 0; p < 7; ++p) {
        int px = 25 + p * 14;
        s.fillRect(px, 228, 3, 10, pal[P_BRASS_PIN]);
        s.drawFastVLine(px, 204, 8, pal[P_CATHODE_OFF]);
    }

    // 4. UPSIDE DOWN MODE (Sec 55-02): Red Mind Flayer Lightning, Creeping Vines & Floating Spores!
    if (upside) {
        // Branched crimson lightning bolts striking behind the glass tube
        if (((nowMs / 85u + screen) & 1) != 0 || state == 2) {
            int lx = 28 + ((int)(nowMs / 40u + screen * 37) % 78);
            for (int seg = 0; seg < 6; ++seg) {
                int ny = 18 + (seg + 1) * 30;
                int nlx = lx + ((int)((nowMs >> seg) + seg * 19 + screen * 11) % 29) - 14;
                if (nlx < 16) nlx = 16;
                if (nlx > 118) nlx = 118;
                s.drawLine(lx - 1, 18 + seg * 30, nlx - 1, ny, pal[P_RIFT_RED]);
                s.drawLine(lx + 1, 18 + seg * 30, nlx + 1, ny, pal[P_RIFT_RED]);
                s.drawLine(lx, 18 + seg * 30, nlx, ny, pal[P_RIFT_CORE]);
                lx = nlx;
            }
        }

        // Creeping dark organic Upside Down vines climbing the bottom/sides of the tube
        for (int v = 0; v < 4; ++v) {
            int vx = (v < 2) ? (14 + v * 9) : (108 + (v - 2) * 9);
            for (int vy = 210; vy > 110; vy -= 12) {
                int sway = ((vy / 12 + v + screen) & 1) ? 4 : -4;
                s.drawLine(vx, vy, vx + sway, vy - 12, pal[P_RIFT_RED]);
                vx += sway;
            }
        }

        // Floating white/grey Upside Down ash & spores drifting upward
        for (int sp = 0; sp < 18; ++sp) {
            uint32_t h = (uint32_t)(screen + 2) * 2654435761u ^ (uint32_t)(sp + 7) * 2246822519u;
            int sy = 210 - (int)(((nowMs * (16 + (sp % 3) * 8)) / 1000u + (h & 0xFF)) % 190u);
            int sx = 16 + (((int)(h >> 8) + (int)(nowMs / 220u + sp * 5)) % 102);
            s.drawPixel(sx, sy, pal[(sp & 1) ? P_WHITE : P_SPORE_GREY]);
            if ((sp & 3) == 0) s.drawPixel(sx + 1, sy, pal[P_SPORE_GREY]);
        }
    }
    (void)stateMs;
}

// Realistic IN-18 Wire-Cathode Nixie Digit (with background ghost cathodes & ceramic beads!)
__attribute__((noinline)) void drawNixieCathodeDigit(TFT_eSprite &s, int digit, unsigned long nowMs,
                                                     uint8_t state, const uint16_t *pal) {
    if (digit < 0 || digit > 9) return;
    static const uint8_t segMask[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
    static const uint8_t segs[7][5] = {
        {30, 32, 75, 13, 1},  // A
        {93, 38, 13, 72, 0},  // B
        {93, 114, 13, 72, 0}, // C
        {30, 178, 75, 13, 1}, // D
        {28, 114, 13, 72, 0}, // E
        {28, 38, 13, 72, 0},  // F
        {32, 105, 71, 13, 1}  // G
    };

    // 1. Draw unlit stacked ghost cathode wires behind the active digit (authentic IN-18 depth!)
    for (int b = 0; b < 7; ++b) {
        int x = segs[b][0], y = segs[b][1], w = segs[b][2], h = segs[b][3];
        s.drawRect(x + 2, y + 2, w - 4, h - 4, pal[P_CATHODE_OFF]);
    }
    // Diagonal ghost tail of '7'/'2' wire in back of stack
    s.drawLine(94, 44, 38, 182, pal[P_CATHODE_OFF]);
    // White ceramic insulator beads on top & bottom support rods
    s.fillCircle(34, 28, 2, pal[P_CERAMIC]);
    s.fillCircle(100, 28, 2, pal[P_CERAMIC]);
    s.fillCircle(34, 196, 2, pal[P_CERAMIC]);
    s.fillCircle(100, 196, 2, pal[P_CERAMIC]);

    // Joyce Byers Christmas-light flicker effect during Sec 55-59 (State 1)
    bool flickerDim = (state == 1) && (((nowMs / 90u + digit) % 5) == 0);
    uint16_t haloOuter = pal[state == 2 ? P_NIXIE_AURA : (flickerDim ? P_ANODE_GRID : P_GAS_VIOLET)];
    uint16_t haloInner = pal[state == 2 ? P_RIFT_RED   : (flickerDim ? P_NIXIE_AURA : P_NIXIE_AURA)];
    uint16_t wireBody  = pal[state == 2 ? P_RIFT_RED   : (flickerDim ? P_NIXIE_AURA : P_NIXIE_ORANGE)];
    uint16_t wireCore  = pal[state == 2 ? P_WHITE      : (flickerDim ? P_NIXIE_ORANGE : P_NIXIE_GOLD)];

    uint8_t m = segMask[digit];

    // 2. Pass 1: Warm Neon Gas Ionization Cloud around active cathode segments
    for (int b = 0; b < 7; ++b) {
        if (!(m & (1 << b))) continue;
        int x = segs[b][0], y = segs[b][1], w = segs[b][2], h = segs[b][3];
        swFillChamfer(s, x - 5, y - 5, w + 10, h + 10, 6, haloOuter);
        swFillChamfer(s, x - 2, y - 2, w + 4, h + 4, 5, haloInner);
    }

    // 3. Pass 2: Glowing Shaped Wire Cathode + Incandescent Gold-White Core
    for (int b = 0; b < 7; ++b) {
        if (!(m & (1 << b))) continue;
        int x = segs[b][0], y = segs[b][1], w = segs[b][2], h = segs[b][3], horiz = segs[b][4];
        swFillChamfer(s, x, y, w, h, 4, wireBody);
        if (horiz) {
            swFillChamfer(s, x + 3, y + 3, w - 6, h - 6, 2, wireCore);
            s.drawFastHLine(x + 5, y + h / 2, w - 10, pal[P_WHITE]);
        } else {
            swFillChamfer(s, x + 3, y + 3, w - 6, h - 6, 2, wireCore);
            s.drawFastVLine(x + w / 2, y + 5, h - 10, pal[P_WHITE]);
        }
    }
}

// Screen 2 (Center): Creel House Grandfather Clock / Dual IN-3 Neon Bulb + Pendulum + Date
__attribute__((noinline)) void drawNixieStrangerCenter(TFT_eSprite &s, struct tm &now, suseconds_t subsec,
                                                       unsigned long nowMs, uint8_t state, const uint16_t *pal) {
    const int cx = 67;
    bool upside = (state >= 1);

    swDrawTxt(s, upside ? "UPSIDE DOWN" : "IN-18 NIXIE", cx, 20, 1, 1, TC_DATUM,
              pal[upside ? P_RIFT_RED : P_NIXIE_ORANGE]);

    // 1. Upper Vintage Clock Dial / Vecna's Grandfather Clock Face (cx = 67, cy = 74, r = 34)
    const int cy = 74, r = 33;
    s.fillCircle(cx, cy, r + 3, pal[upside ? P_RIFT_RED : P_NIXIE_AURA]);
    s.fillCircle(cx, cy, r, pal[P_BLACK]);
    s.drawCircle(cx, cy, r, pal[upside ? P_RIFT_CORE : P_NIXIE_ORANGE]);
    s.drawCircle(cx, cy, r - 3, pal[P_BRASS_PIN]);

    // 12 Roman/tick marks around the grandfather clock dial
    for (int h = 0; h < 12; ++h) {
        static const int8_t cos12[12] = {0, 15, 26, 30, 26, 15, 0, -15, -26, -30, -26, -15};
        static const int8_t sin12[12] = {-30, -26, -15, 0, 15, 26, 30, 26, 15, 0, -15, -26};
        int x0 = cx + (cos12[h] * 24) / 30;
        int y0 = cy + (sin12[h] * 24) / 30;
        int x1 = cx + (cos12[h] * 29) / 30;
        int y1 = cy + (sin12[h] * 29) / 30;
        s.drawLine(x0, y0, x1, y1, pal[upside ? P_RIFT_RED : P_NIXIE_GOLD]);
    }

    // Clock hands on the dial (spinning backwards crazily during Upside Down state!)
    int secIdx = upside ? ((int)(120000u - (nowMs / 35u)) % 12) : ((now.tm_sec / 5) % 12);
    static const int8_t hDX[12] = {0, 12, 21, 24, 21, 12, 0, -12, -21, -24, -21, -12};
    static const int8_t hDY[12] = {-24, -21, -12, 0, 12, 21, 24, 21, 12, 0, -12, -21};
    s.drawLine(cx, cy, cx + hDX[secIdx], cy + hDY[secIdx], pal[P_WHITE]);
    s.drawLine(cx, cy, cx + hDX[(now.tm_hour % 12)] * 2 / 3, cy + hDY[(now.tm_hour % 12)] * 2 / 3,
               pal[upside ? P_RIFT_RED : P_NIXIE_ORANGE]);
    s.fillCircle(cx, cy, 3, pal[P_NIXIE_GOLD]);

    // 2. Swinging Brass/Neon Pendulum below the dial (y = 108..166)
    // Smooth 1-second swing using subsec
    int swingPhase = (int)((nowMs % 2000u)); // 0..1999ms full period
    int norm = (swingPhase < 1000) ? (swingPhase - 500) : (1500 - swingPhase); // -500..+500
    int bobX = cx + (norm * 24) / 500;
    int bobY = 156 - (abs(norm) * 6) / 500;
    s.drawLine(cx, cy + r, bobX, bobY, pal[upside ? P_RIFT_RED : P_BRASS_PIN]);
    s.fillCircle(bobX, bobY, 9, pal[upside ? P_RIFT_RED : P_NIXIE_AURA]);
    s.fillCircle(bobX, bobY, 6, pal[upside ? P_RIFT_CORE : P_NIXIE_ORANGE]);
    s.fillCircle(bobX, bobY, 3, pal[P_WHITE]);

    // Twin IN-3 Neon Colon Bulbs flanking the pendulum shaft
    bool pulseOn = (subsec < 500000L);
    s.fillCircle(24, 126, 5, pal[pulseOn ? (upside ? P_RIFT_RED : P_NIXIE_ORANGE) : P_CATHODE_OFF]);
    s.fillCircle(110, 126, 5, pal[pulseOn ? (upside ? P_RIFT_RED : P_NIXIE_ORANGE) : P_CATHODE_OFF]);
    if (pulseOn) {
        s.fillCircle(24, 126, 2, pal[P_WHITE]);
        s.fillCircle(110, 126, 2, pal[P_WHITE]);
    }

    // 3. Bottom Glowing Nixie Date Display (y = 172..208)
    s.drawFastHLine(20, 172, 95, pal[upside ? P_RIFT_RED : P_NIXIE_AURA]);
    char dateStr[6] = {
        char('0' + now.tm_mday / 10), char('0' + now.tm_mday % 10), '.',
        char('0' + now.tm_mon / 10), char('0' + now.tm_mon % 10), 0
    };
    swDrawTxt(s, dateStr, cx, 192, 4, 1, MC_DATUM, pal[upside ? P_RIFT_CORE : P_NIXIE_GOLD]);
}

// Screen 5 (Rightmost): Hawkins National Lab (HNL) Vacuum Telemetry + IN-9 Plasma Bargraph + Weather
__attribute__((noinline)) void drawNixieLabScreen(TFT_eSprite &s, WeatherService &weather,
                                                  struct tm &now, unsigned long nowMs,
                                                  uint8_t state, const uint16_t *pal) {
    bool upside = (state >= 1);

    // 1. RIGHT SIDE: IN-9 Vertical Neon Plasma Bargraph Tube (x = 98..116, y = 26..202)
    const int bx = 100, by = 28, bw = 14, bh = 172;
    s.drawRect(bx, by, bw, bh, pal[P_GLASS_EDGE]);
    s.fillRect(bx + 1, by + 1, bw - 2, bh - 2, pal[P_BLACK]);
    int plasmaH = ((now.tm_sec + 1) * (bh - 6)) / 60;
    if (upside) plasmaH = bh - 6;
    int pyTop = by + bh - 3 - plasmaH;
    s.fillRect(bx + 2, pyTop, bw - 4, plasmaH, pal[upside ? P_RIFT_RED : P_NIXIE_AURA]);
    s.fillRect(bx + 4, pyTop, bw - 8, plasmaH, pal[upside ? P_RIFT_CORE : P_NIXIE_ORANGE]);
    s.drawFastVLine(bx + bw / 2, pyTop, plasmaH, pal[P_NIXIE_GOLD]);
    // Graduation tick marks on IN-9 bargraph glass
    for (int ty = by + 10; ty < by + bh - 6; ty += 14) {
        s.drawFastHLine(bx + 1, ty, 4, pal[P_GLASS_SPEC]);
    }

    // 2. LEFT/CENTER: Hawkins Lab Telemetry Readouts (x = 16..92)
    swDrawTxt(s, upside ? "RIFT ALERT!" : "HAWKINS LAB", 54, 22, 1, 1, TC_DATUM,
              pal[upside ? P_RIFT_RED : P_NIXIE_GOLD]);
    s.drawFastHLine(18, 33, 74, pal[upside ? P_RIFT_RED : P_NIXIE_AURA]);

    char unit = (WeatherService::getUnits().value == "imperial") ? 'F' : 'C';
    float vals[3] = {weather.getNowTemp(), weather.getHigh(5), weather.getLow(5)};
    int ints[3];
    for (int k = 0; k < 3; ++k) ints[k] = (int)(vals[k] + (vals[k] >= 0.0f ? 0.5f : -0.5f));

    // Nixie Temperature Readout
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%c", ints[0], unit);
    swDrawTxt(s, buf, 54, 62, 4, 1, MC_DATUM, pal[upside ? P_WHITE : P_NIXIE_ORANGE]);

    snprintf(buf, sizeof(buf), "MAX %d%c", ints[1], unit);
    swDrawTxt(s, buf, 54, 92, 2, 1, MC_DATUM, pal[P_NIXIE_GOLD]);
    snprintf(buf, sizeof(buf), "MIN %d%c", ints[2], unit);
    swDrawTxt(s, buf, 54, 110, 2, 1, MC_DATUM, pal[P_GLASS_SPEC]);

    // 3. LOWER: Dimensional Rift / PSI Analog Vacuum Meter (y = 126..202)
    s.drawRect(18, 126, 74, 74, pal[P_GLASS_EDGE]);
    swDrawTxt(s, upside ? "VECNA CURSE" : "PSI / RIFT", 55, 132, 1, 1, TC_DATUM,
              pal[upside ? P_RIFT_RED : P_NIXIE_ORANGE]);

    // Analog arc & vibrating needle inside vacuum gauge
    const int mcx = 55, mcy = 188;
    s.drawCircle(mcx, mcy, 32, pal[P_NIXIE_AURA]);
    int needleX = mcx + (upside ? (((int)(nowMs / 25u) % 44) - 22) : ((now.tm_sec * 40) / 59 - 20));
    int needleY = mcy - 26;
    s.drawLine(mcx, mcy, needleX, needleY, pal[upside ? P_WHITE : P_NIXIE_GOLD]);
    s.fillCircle(mcx, mcy, 4, pal[P_NIXIE_ORANGE]);

    snprintf(buf, sizeof(buf), ":%02d", now.tm_sec);
    swDrawTxt(s, buf, 55, 192, 2, 1, MC_DATUM, pal[P_WHITE]);
}

} // namespace

void MatrixClock::renderNixieStranger(IPSClock &clock, TimeSync &time, WeatherService &weather) {
    unsigned long nowMs = millis();
    if (nowMs - lastFrame < 45) return;
    lastFrame = nowMs;
    ++frame;

    struct tm now;
    suseconds_t subsecond;
    time.getLocalTime(&now, &subsecond);

    uint16_t stateMs = 0;
    if (now.tm_sec <= 2) {
        nixieStrangerState = 2;
        stateMs = (uint16_t)(now.tm_sec * 1000u + (uint32_t)(subsecond / 1000));
    } else if (now.tm_sec >= 55) {
        nixieStrangerState = 1;
        stateMs = (uint16_t)((now.tm_sec - 55) * 1000u + (uint32_t)(subsecond / 1000));
    } else {
        nixieStrangerState = 0;
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
    swDimPalette(NIXIE_RAW, pal, P_COUNT);

    for (int i = 0; i < 6; ++i) {
        drawNixieEnvelope(sprite, i, nowMs, nixieStrangerState, stateMs, pal);

        if (i == 2) {
            drawNixieStrangerCenter(sprite, now, subsecond, nowMs, nixieStrangerState, pal);
        } else if (i == 5) {
            drawNixieLabScreen(sprite, weather, now, nowMs, nixieStrangerState, pal);
        } else {
            bool blankHour = (i == 0 && hour < 10 && !IPSClock::getLeadingZero().value);
            if (!blankHour) {
                drawNixieCathodeDigit(sprite, digits[i], nowMs, nixieStrangerState, pal);
            }
        }

        tfts->chip_select.setDigit(SW_SCREENS[i]);
        sprite.pushSprite(0, 0);
    }
    tfts->release();
}
