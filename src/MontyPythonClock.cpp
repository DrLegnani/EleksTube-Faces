#include "MatrixClock.h"
#include "StarWarsHud.h"
#include "IPSClock.h"

// Shared with Backlights: 0 = Ministry of Silly Walks, 1 = Black Knight "Tis but a scratch" (56-59s), 2 = The Giant Flying Foot of Cupid STOMP! (00-02s)
uint8_t montyPythonState = 0;

namespace {

enum MontyPal {
    P_WHITE = 0,
    P_BLACK,
    P_PARCHMENT,    // Vintage Victorian buff paper (#EDE3C8)
    P_PARCH_DARK,   // Aged sepia border (#7D6A4E)
    P_VICTORIAN_RED,// British mail / circus banner red (#B82424)
    P_GOLD,         // Terry Gilliam gilded gold (#D8A82A)
    P_ROYAL_BLUE,   // Victorian royal blue (#244288)
    P_FOOT_FLESH,   // The Giant Foot pinkish-flesh (#ECC4A8)
    P_FOOT_SHADOW,  // Foot contour shade (#B8886E)
    P_BLOOD_RED,    // Black Knight cartoon blood (#D8121E)
    P_SUIT_GREY,    // Silly Walks bowler hat & suit grey (#363840)
    P_KNIGHT_STEEL, // Black Knight iron armor (#1A1C22)
    P_COUNT
};

const uint16_t MONTY_RAW[P_COUNT] = {
    0xFFFF,                 // P_WHITE
    0x0000,                 // P_BLACK
    swC565(237, 227, 200),  // P_PARCHMENT
    swC565(125, 106, 78),   // P_PARCH_DARK
    swC565(184, 36, 36),    // P_VICTORIAN_RED
    swC565(216, 168, 42),   // P_GOLD
    swC565(36, 66, 136),    // P_ROYAL_BLUE
    swC565(236, 196, 168),  // P_FOOT_FLESH
    swC565(184, 136, 110),  // P_FOOT_SHADOW
    swC565(216, 18, 30),    // P_BLOOD_RED
    swC565(54, 56, 64),     // P_SUIT_GREY
    swC565(26, 28, 34)      // P_KNIGHT_STEEL
};

// Victorian engraving backdrop + John Cleese Silly Walk walking across the bottom
__attribute__((noinline)) void drawMontyBackdrop(TFT_eSprite &s, int screen, unsigned long nowMs,
                                                 uint8_t state, uint16_t stateMs, const uint16_t *pal) {
    s.fillSprite(pal[P_PARCHMENT]);

    // 1. Victorian Ornate Picture Frame on each tube (woodcut engraving style)
    s.drawRect(2, 2, TFT_WIDTH - 4, TFT_HEIGHT - 4, pal[P_PARCH_DARK]);
    s.drawRect(4, 4, TFT_WIDTH - 8, TFT_HEIGHT - 8, pal[P_GOLD]);
    s.drawRect(6, 6, TFT_WIDTH - 12, TFT_HEIGHT - 12, pal[P_PARCH_DARK]);

    // Engraved corner flourishes
    const int cOff[4][2] = {{8, 8}, {126, 8}, {8, 231}, {126, 231}};
    for (int c = 0; c < 4; ++c) {
        s.drawCircle(cOff[c][0], cOff[c][1], 5, pal[P_GOLD]);
        s.drawPixel(cOff[c][0], cOff[c][1], pal[P_VICTORIAN_RED]);
    }

    // Top Victorian Circus Banner strip
    s.fillRect(8, 8, TFT_WIDTH - 16, 13, pal[P_VICTORIAN_RED]);
    swDrawTxt(s, state == 2 ? "SPLAT!!!" : (state == 1 ? "TIS BUT A SCRATCH!" : "MONTY PYTHON"),
              TFT_WIDTH / 2, 11, 1, 1, TC_DATUM, pal[P_GOLD]);

    // 2. Ministry of Silly Walks: John Cleese character walking across the bottom of the screens!
    // Global X progresses across all 6 screens over a 12-second loop
    int walkLoop = (int)((nowMs / 25u) % 1100u);
    int globalWalkerX = walkLoop - 80;
    int scrX0 = screen * 150;
    int wx = globalWalkerX - scrX0;

    if (wx >= -25 && wx < TFT_WIDTH + 25 && state != 2) {
        const int wy = 196;
        int stepPhase = (int)((nowMs / 120u) % 4); // 4 extreme silly walk poses

        // Bowler Hat
        s.fillCircle(wx, wy - 30, 4, pal[P_SUIT_GREY]);
        s.drawFastHLine(wx - 6, wy - 27, 13, pal[P_BLACK]);
        // Head
        s.fillCircle(wx, wy - 22, 5, pal[P_FOOT_FLESH]);
        // Suit Torso & Briefcase
        s.fillRect(wx - 3, wy - 17, 7, 16, pal[P_SUIT_GREY]);
        s.fillRect(wx + 4, wy - 9, 6, 8, pal[P_PARCH_DARK]); // Brown briefcase

        // Absurd 90-degree Silly Walk legs!
        if (stepPhase == 0) {
            // Right leg kicked straight up to horizontal height!
            s.drawLine(wx, wy - 2, wx + 18, wy - 12, pal[P_BLACK]);
            s.drawLine(wx + 18, wy - 12, wx + 24, wy - 12, pal[P_BLACK]); // Foot
            // Left leg standing straight
            s.drawLine(wx, wy - 2, wx, wy + 16, pal[P_BLACK]);
        } else if (stepPhase == 1) {
            // Legs spread in deep walking stride
            s.drawLine(wx, wy - 2, wx - 10, wy + 16, pal[P_BLACK]);
            s.drawLine(wx, wy - 2, wx + 10, wy + 16, pal[P_BLACK]);
        } else if (stepPhase == 2) {
            // Left leg kicked backwards in arc!
            s.drawLine(wx, wy - 2, wx - 16, wy - 8, pal[P_BLACK]);
            s.drawLine(wx, wy - 2, wx + 4, wy + 16, pal[P_BLACK]);
        } else {
            // High knee crane pose
            s.drawLine(wx, wy - 2, wx + 8, wy - 10, pal[P_BLACK]);
            s.drawLine(wx + 8, wy - 10, wx + 8, wy + 6, pal[P_BLACK]);
            s.drawLine(wx, wy - 2, wx - 4, wy + 16, pal[P_BLACK]);
        }
    }

    // 3. Sec 00-02 (State 2): TERRY GILLIAM'S GIANT CUPID FOOT CRASHES DOWN FROM THE SKY! (SPLAT!)
    if (state == 2) {
        // Foot drops down from y = -140 to ground y = 140 over the first 700ms
        int footDrop = (int)((stateMs * 280u) / 600u) - 100;
        if (footDrop > 130) footDrop = 130; // Ground squash position

        const int fx = 67, fy = footDrop;

        // Giant Gilliam Foot Leg & Calf
        s.fillRect(fx - 24, fy - 180, 48, 140, pal[P_FOOT_FLESH]);
        s.drawFastVLine(fx - 24, fy - 180, 140, pal[P_FOOT_SHADOW]);
        s.drawFastVLine(fx + 24, fy - 180, 140, pal[P_FOOT_SHADOW]);

        // Rounded Heel & Arch
        s.fillCircle(fx - 14, fy - 22, 28, pal[P_FOOT_FLESH]);
        s.fillCircle(fx + 22, fy - 16, 24, pal[P_FOOT_FLESH]);
        s.fillRect(fx - 24, fy - 20, 56, 32, pal[P_FOOT_FLESH]);

        // 5 Giant Toes squashing the bottom of the screen!
        s.fillCircle(fx + 38, fy + 8, 12, pal[P_FOOT_FLESH]); // Big toe
        s.fillCircle(fx + 24, fy + 12, 10, pal[P_FOOT_FLESH]);
        s.fillCircle(fx + 10, fy + 12, 9, pal[P_FOOT_FLESH]);
        s.fillCircle(fx - 4,  fy + 12, 8, pal[P_FOOT_FLESH]);
        s.fillCircle(fx - 16, fy + 11, 7, pal[P_FOOT_FLESH]); // Little toe

        // Toes outline & cartoon squish lines
        s.drawCircle(fx + 38, fy + 8, 12, pal[P_FOOT_SHADOW]);
        s.drawFastHLine(fx - 36, fy + 22, 88, pal[P_BLACK]);
        s.drawFastHLine(fx - 42, fy + 23, 98, pal[P_FOOT_SHADOW]);

        // Splat impact squish clouds
        if (footDrop >= 125) {
            for (int sq = 0; sq < 6; ++sq) {
                int sx = 20 + sq * 18;
                s.fillCircle(sx, fy + 18, 6, pal[P_WHITE]);
            }
        }
    }
}

// Terry Gilliam Victorian Cutout Engraving Digit
__attribute__((noinline)) void drawMontyDigit(TFT_eSprite &s, int digit, unsigned long nowMs,
                                              uint8_t state, const uint16_t *pal) {
    if (digit < 0 || digit > 9) return;
    static const uint8_t segMask[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
    static const uint8_t segs[7][5] = {
        {26, 28, 83, 16, 1},  // A
        {93, 34, 16, 74, 0},  // B
        {93, 110, 16, 74, 0}, // C
        {26, 172, 83, 16, 1}, // D
        {26, 110, 16, 74, 0}, // E
        {26, 34, 16, 74, 0},  // F
        {28, 102, 79, 16, 1}  // G
    };

    uint8_t m = segMask[digit];

    // Pass 1: Victorian Woodcut Cross-Hatch Shadow (offset +5, +5)
    for (int b = 0; b < 7; ++b) {
        if (!(m & (1 << b))) continue;
        int x = segs[b][0] + 4, y = segs[b][1] + 4, w = segs[b][2], h = segs[b][3];
        swFillChamfer(s, x, y, w, h, 4, pal[P_PARCH_DARK]);
    }

    // Pass 2: Royal Circus Victorian Red Body + Gold Leaf Inset Border
    for (int b = 0; b < 7; ++b) {
        if (!(m & (1 << b))) continue;
        int x = segs[b][0], y = segs[b][1], w = segs[b][2], h = segs[b][3], horiz = segs[b][4];
        swFillChamfer(s, x, y, w, h, 4, pal[P_VICTORIAN_RED]);
        swFillChamfer(s, x + 2, y + 2, w - 4, h - 4, 3, pal[P_GOLD]);
        swFillChamfer(s, x + 4, y + 4, w - 8, h - 8, 2, pal[P_PARCHMENT]);

        // Victorian engraving crosshatch tick marks
        if (horiz) {
            for (int lx = x + 8; lx < x + w - 8; lx += 6) {
                s.drawFastVLine(lx, y + 4, h - 8, pal[P_PARCH_DARK]);
            }
        } else {
            for (int ly = y + 8; ly < y + h - 8; ly += 6) {
                s.drawFastHLine(x + 4, ly, w - 8, pal[P_PARCH_DARK]);
            }
        }
    }
    (void)nowMs; (void)state;
}

// Screen 2 (Center): THE BLACK KNIGHT ("Tis but a scratch!") + Holy Hand Grenade + Date
__attribute__((noinline)) void drawMontyCenter(TFT_eSprite &s, struct tm &now, suseconds_t subsec,
                                               unsigned long nowMs, uint8_t state, const uint16_t *pal) {
    const int cx = 67;

    swDrawTxt(s, state == 1 ? "A MERE FLESH WOUND!" : "BLACK KNIGHT", cx, 16, 1, 1, TC_DATUM, pal[P_VICTORIAN_RED]);
    s.drawFastHLine(20, 27, 95, pal[P_PARCH_DARK]);

    // The Black Knight Iron Armor (cx = 67, cy = 90)
    const int ky = 88;

    // Great Helm Helmet (Cylindrical iron helmet)
    swFillChamfer(s, cx - 18, ky - 36, 37, 38, 4, pal[P_KNIGHT_STEEL]);
    s.drawRect(cx - 18, ky - 36, 37, 38, pal[P_GOLD]);
    // Cruciform eye-slit visor
    s.drawFastHLine(cx - 14, ky - 18, 29, pal[P_GOLD]);
    s.drawFastHLine(cx - 14, ky - 17, 29, pal[P_BLACK]);
    s.drawFastVLine(cx, ky - 28, 22, pal[P_GOLD]);
    s.drawFastVLine(cx + 1, ky - 28, 22, pal[P_BLACK]);

    // Red Dragon Crest painted on helmet
    s.fillCircle(cx, ky - 28, 3, pal[P_VICTORIAN_RED]);

    // Iron Breastplate Torso with Red Boar/Dragon emblem
    swFillChamfer(s, cx - 24, ky + 2, 49, 44, 6, pal[P_KNIGHT_STEEL]);
    s.drawRect(cx - 24, ky + 2, 49, 44, pal[P_PARCH_DARK]);
    s.fillCircle(cx, ky + 18, 8, pal[P_VICTORIAN_RED]);
    s.drawCircle(cx, ky + 18, 8, pal[P_GOLD]);

    // The Black Knight Arms (dissevered one by one during State 1!)
    bool leftArmOff  = (now.tm_sec > 15 || state >= 1);
    bool rightArmOff = (now.tm_sec > 35 || state >= 1);

    if (!leftArmOff) {
        // Holding heavy two-handed broadsword
        s.fillRect(cx - 38, ky + 6, 14, 34, pal[P_KNIGHT_STEEL]);
        s.drawLine(cx - 36, ky - 20, cx - 36, ky + 55, pal[P_WHITE]); // Sword blade
        s.drawFastHLine(cx - 42, ky + 24, 13, pal[P_GOLD]); // Crossguard
    } else {
        // Dissevered left shoulder gushing cartoon blood!
        s.fillCircle(cx - 26, ky + 8, 5, pal[P_BLOOD_RED]);
        for (int b = 0; b < 3; ++b) {
            int by2 = ky + 14 + ((int)(nowMs / 40u + b * 11) % 18);
            s.drawFastVLine(cx - 27 + b * 2, ky + 10, by2 - ky - 10, pal[P_BLOOD_RED]);
        }
    }

    if (!rightArmOff) {
        s.fillRect(cx + 24, ky + 6, 14, 34, pal[P_KNIGHT_STEEL]);
    } else {
        // Dissevered right shoulder gushing cartoon blood!
        s.fillCircle(cx + 26, ky + 8, 5, pal[P_BLOOD_RED]);
        for (int b = 0; b < 3; ++b) {
            int by2 = ky + 14 + ((int)(nowMs / 45u + b * 13) % 18);
            s.drawFastVLine(cx + 25 + b * 2, ky + 10, by2 - ky - 10, pal[P_BLOOD_RED]);
        }
    }

    // Bottom Date Display in Ornate Gilded Gold Baroque Plaque (y = 176..220)
    swFillChamfer(s, 16, 176, 103, 44, 5, pal[P_GOLD]);
    swFillChamfer(s, 18, 178, 99, 40, 4, pal[P_PARCHMENT]);
    s.drawRect(20, 180, 95, 36, pal[P_PARCH_DARK]);
    swDrawTxt(s, "ANNO DOMINI", cx, 184, 1, 1, TC_DATUM, pal[P_VICTORIAN_RED]);
    char dateStr[6] = {
        char('0' + now.tm_mday / 10), char('0' + now.tm_mday % 10), '.',
        char('0' + now.tm_mon / 10), char('0' + now.tm_mon % 10), 0
    };
    swDrawTxt(s, dateStr, cx, 203, 4, 1, MC_DATUM, pal[P_PARCH_DARK]);
    (void)subsec;
}

// Screen 5 (Rightmost): HOLY HAND GRENADE OF ANTIOCH + Killer Rabbit of Caerbannog + Weather
__attribute__((noinline)) void drawMontyHolyScreen(TFT_eSprite &s, WeatherService &weather,
                                                   struct tm &now, unsigned long nowMs,
                                                   uint8_t state, const uint16_t *pal) {
    // 1. TOP (y = 16..84): Holy Hand Grenade of Antioch icon & "COUNT TO THREE"
    swFillChamfer(s, 10, 16, 115, 68, 5, pal[P_PARCH_DARK]);
    swFillChamfer(s, 12, 18, 111, 64, 4, pal[P_PARCHMENT]);
    swDrawTxt(s, "HOLY HAND GRENADE", 67, 21, 1, 1, TC_DATUM, pal[P_VICTORIAN_RED]);

    // Golden Orb of Antioch with jeweled crucifix cross (cx = 67, cy = 54)
    const int gx = 67, gy = 54;
    s.fillCircle(gx, gy, 15, pal[P_GOLD]);
    s.drawCircle(gx, gy, 15, pal[P_PARCH_DARK]);
    // Inset ruby jewels on the orb belt
    s.drawFastHLine(gx - 13, gy, 27, pal[P_VICTORIAN_RED]);
    s.drawFastVLine(gx, gy - 13, 27, pal[P_VICTORIAN_RED]);
    // Gold cross on top of grenade
    s.fillRect(gx - 2, gy - 24, 5, 11, pal[P_GOLD]);
    s.fillRect(gx - 6, gy - 21, 13, 4, pal[P_GOLD]);

    // 2. MIDDLE (y = 90..160): Weather in Victorian English Manor style
    swFillChamfer(s, 10, 90, 115, 70, 5, pal[P_PARCH_DARK]);
    swFillChamfer(s, 12, 92, 111, 66, 4, pal[P_PARCHMENT]);
    swDrawTxt(s, "THE CLIMATE", 67, 96, 1, 1, TC_DATUM, pal[P_ROYAL_BLUE]);

    char unit = (WeatherService::getUnits().value == "imperial") ? 'F' : 'C';
    float vals[3] = {weather.getNowTemp(), weather.getHigh(5), weather.getLow(5)};
    int ints[3];
    for (int k = 0; k < 3; ++k) ints[k] = (int)(vals[k] + (vals[k] >= 0.0f ? 0.5f : -0.5f));

    char buf[16];
    snprintf(buf, sizeof(buf), "%d%c", ints[0], unit);
    swDrawTxt(s, buf, 42, 128, 4, 1, MC_DATUM, pal[P_PARCH_DARK]);

    snprintf(buf, sizeof(buf), "HI:%d", ints[1]);
    swDrawTxt(s, buf, 94, 120, 2, 1, MC_DATUM, pal[P_VICTORIAN_RED]);
    snprintf(buf, sizeof(buf), "LO:%d", ints[2]);
    swDrawTxt(s, buf, 94, 138, 2, 1, MC_DATUM, pal[P_ROYAL_BLUE]);

    // 3. BOTTOM (y = 166..232): The Killer Rabbit of Caerbannog + Seconds ("1.. 2.. 5! THREE, SIR!")
    swFillChamfer(s, 10, 166, 115, 66, 5, pal[P_PARCH_DARK]);
    swFillChamfer(s, 12, 168, 111, 62, 4, pal[P_PARCHMENT]);

    int count3 = (now.tm_sec % 4);
    const char *cntStr = (count3 == 0) ? "ONE..." : ((count3 == 1) ? "TWO..." : ((count3 == 2) ? "FIVE!" : "THREE, SIR!"));
    swDrawTxt(s, cntStr, 67, 172, 1, 1, TC_DATUM, pal[P_VICTORIAN_RED]);

    // Cute fluffy white bunny with blood-dripping fangs! (cx = 67, cy = 205)
    const int bx = 67, by = 206;
    // Bunny body & head
    s.fillCircle(bx, by, 11, pal[P_WHITE]);
    s.fillCircle(bx + 9, by - 6, 8, pal[P_WHITE]);
    // Long upright white bunny ears with pink interior
    s.fillRect(bx + 6, by - 22, 4, 16, pal[P_WHITE]);
    s.fillRect(bx + 12, by - 20, 4, 14, pal[P_WHITE]);
    s.drawFastVLine(bx + 7, by - 20, 12, pal[P_FOOT_FLESH]);
    s.drawFastVLine(bx + 13, by - 18, 10, pal[P_FOOT_FLESH]);

    // Red demonic eyes & blood dripping from mouth!
    s.fillCircle(bx + 12, by - 8, 2, pal[P_BLOOD_RED]);
    s.fillTriangle(bx + 15, by - 4, bx + 17, by - 4, bx + 16, by, pal[P_WHITE]); // Sharp fangs
    s.fillRect(bx + 13, by - 3, 5, 5, pal[P_BLOOD_RED]); // Blood

    snprintf(buf, sizeof(buf), ":%02d", now.tm_sec);
    swDrawTxt(s, buf, 34, 204, 2, 1, MC_DATUM, pal[P_PARCH_DARK]);
    (void)nowMs; (void)state;
}

} // namespace

void MatrixClock::renderMontyPython(IPSClock &clock, TimeSync &time, WeatherService &weather) {
    unsigned long nowMs = millis();
    if (nowMs - lastFrame < 45) return;
    lastFrame = nowMs;
    ++frame;

    struct tm now;
    suseconds_t subsecond;
    time.getLocalTime(&now, &subsecond);

    uint16_t stateMs = 0;
    if (now.tm_sec <= 2) {
        montyPythonState = 2;
        stateMs = (uint16_t)(now.tm_sec * 1000u + (uint32_t)(subsecond / 1000));
    } else if (now.tm_sec >= 56) {
        montyPythonState = 1;
        stateMs = (uint16_t)((now.tm_sec - 56) * 1000u + (uint32_t)(subsecond / 1000));
    } else {
        montyPythonState = 0;
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
    swDimPalette(MONTY_RAW, pal, P_COUNT);

    for (int i = 0; i < 6; ++i) {
        drawMontyBackdrop(sprite, i, nowMs, montyPythonState, stateMs, pal);

        if (i == 2) {
            drawMontyCenter(sprite, now, subsecond, nowMs, montyPythonState, pal);
        } else if (i == 5) {
            drawMontyHolyScreen(sprite, weather, now, nowMs, montyPythonState, pal);
        } else {
            bool blankHour = (i == 0 && hour < 10 && !IPSClock::getLeadingZero().value);
            if (!blankHour) {
                drawMontyDigit(sprite, digits[i], nowMs, montyPythonState, pal);
            }
        }

        tfts->chip_select.setDigit(SW_SCREENS[i]);
        sprite.pushSprite(0, 0);
    }
    tfts->release();
}
