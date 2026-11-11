#include "MatrixClock.h"
#include "StarWarsHud.h"
#include "IPSClock.h"

// Shared with Backlights: 0 = patrol, 1 = superlaser charge (56-59s), 2 = superlaser fire (00-02s)
uint8_t deathStarState = 0;

namespace {

enum DsPal {
    P_WHITE = 0, P_BLACK, P_BG_FIRE, P_STAR_DIM, P_STAR_MID,
    P_HULL_D, P_HULL_M, P_HULL_L, P_HULL_R, P_DISH_D,
    P_RED_AURA, P_RED_MID, P_STEEL_D,
    P_LASER_AURA, P_LASER_MID, P_LASER_CORE,
    P_DARK_HUD, P_WARM, P_COOL, P_BOX_BG,
    P_COUNT
};

const uint16_t DS_RAW[P_COUNT] = {
    0xFFFF,               // P_WHITE
    0x0000,               // P_BLACK
    swC565(3, 18, 8),     // P_BG_FIRE
    swC565(78, 95, 128),  // P_STAR_DIM
    swC565(175, 195, 225),// P_STAR_MID
    swC565(36, 40, 50),   // P_HULL_D
    swC565(104, 114, 130),// P_HULL_M
    swC565(156, 168, 184),// P_HULL_L
    swC565(205, 215, 230),// P_HULL_R
    swC565(55, 62, 76),   // P_DISH_D
    swC565(88, 10, 12),   // P_RED_AURA
    swC565(225, 42, 32),  // P_RED_MID
    swC565(110, 118, 132),// P_STEEL_D
    swC565(0, 95, 28),    // P_LASER_AURA
    swC565(45, 245, 95),  // P_LASER_MID
    swC565(195, 255, 210),// P_LASER_CORE
    swC565(60, 20, 22),   // P_DARK_HUD
    swC565(255, 215, 135),// P_WARM
    swC565(145, 225, 255),// P_COOL
    swC565(12, 16, 24)    // P_BOX_BG
};

// Global Death Star superlaser dish origin (screen 2: cx=67, cy=112 -> dish at 79, 97 -> global X = 300+79 = 379)
const int DISH_GLOBAL_X = 379;
const int DISH_GLOBAL_Y = 97;

__attribute__((noinline)) void drawTieFighter(TFT_eSprite &s, int x, int y, const uint16_t *pal) {
    s.drawFastVLine(x - 6, y - 6, 13, pal[P_HULL_R]);
    s.drawFastVLine(x + 6, y - 6, 13, pal[P_HULL_R]);
    s.drawFastVLine(x - 5, y - 5, 11, pal[P_HULL_D]);
    s.drawFastVLine(x + 5, y - 5, 11, pal[P_HULL_D]);
    s.drawFastHLine(x - 5, y, 11, pal[P_HULL_M]);
    s.fillCircle(x, y, 3, pal[P_HULL_L]);
    s.drawPixel(x, y, pal[P_BLACK]);
    s.drawPixel(x - 2, y, pal[P_RED_MID]);
}

__attribute__((noinline)) void drawImperialSpace(TFT_eSprite &s, int screen, unsigned long nowMs,
                                                 uint8_t state, uint16_t stateMs, const uint16_t *pal) {
    s.fillSprite(state == 2 ? pal[P_BG_FIRE] : TFT_BLACK);

    // Crisp space starfield
    for (int k = 0; k < 20; ++k) {
        uint32_t h = (uint32_t)(screen + 9) * 2654435761u ^ (uint32_t)(k + 3) * 2246822519u;
        int sx = (int)((h >> 8) % 135);
        int sy = (int)((h >> 16) % 240);
        uint16_t col = pal[((nowMs / 450 + k) % 7 == 0) ? P_WHITE : (k & 1) ? P_STAR_MID : P_STAR_DIM];
        s.drawPixel(sx, sy, col);
    }

    // Patrolling TIE Fighters on outer screens (0, 1, 4, 5)
    if (screen != 2 && screen != 3) {
        int tx = (int)(((nowMs / 38u) + screen * 47u) % 165u) - 15;
        int ty = 42 + (screen % 2) * 145 + (int)(((nowMs / 350u + screen) & 3) - 1);
        drawTieFighter(s, tx, ty, pal);
    }

    // Sec 56-59: Emerald energy streams converging across outer tubes into center dish
    if (state == 1 && screen != 2) {
        int dishX = DISH_GLOBAL_X - screen * 150;
        int dishY = DISH_GLOBAL_Y;
        int pulsePhase = (int)(stateMs / 80u) & 7;
        for (int b = 0; b < 3; ++b) {
            int ox = (screen < 2) ? 0 : TFT_WIDTH - 1;
            int oy = 50 + b * 68;
            int dx = dishX - ox, dy = dishY - oy;
            int seg = ((pulsePhase + b * 2) % 8);
            int x0 = ox + (dx * seg) / 9, y0 = oy + (dy * seg) / 9;
            int x1 = ox + (dx * (seg + 2)) / 9, y1 = oy + (dy * (seg + 2)) / 9;
            s.drawLine(x0, y0, x1, y1, pal[P_LASER_MID]);
            s.drawPixel(x1, y1, pal[P_WHITE]);
        }
    }

    // Sec 00-02: Superlaser beam blasting continuously out of Screen 2 through Screens 3 -> 4 -> 5
    if (state == 2 && screen >= 2) {
        int reachGx = DISH_GLOBAL_X + (int)((stateMs < 400u) ? (stateMs * 1400u / 400u) : 1400u);
        int scrX0 = screen * 150;
        int localStart = (screen == 2) ? (DISH_GLOBAL_X - scrX0) : 0;
        int localEnd = reachGx - scrX0;
        if (localEnd > localStart && localStart < TFT_WIDTH) {
            if (localEnd >= TFT_WIDTH) localEnd = TFT_WIDTH - 1;
            int y0 = DISH_GLOBAL_Y - (scrX0 + localStart - DISH_GLOBAL_X) / 14;
            int y1 = DISH_GLOBAL_Y - (scrX0 + localEnd - DISH_GLOBAL_X) / 14;
            for (int d = -5; d <= 5; ++d) s.drawLine(localStart, y0 + d, localEnd, y1 + d, pal[P_LASER_AURA]);
            for (int d = -2; d <= 2; ++d) s.drawLine(localStart, y0 + d, localEnd, y1 + d, pal[P_LASER_MID]);
            s.drawLine(localStart, y0, localEnd, y1, pal[P_WHITE]);
            // Energy plasma rings along the beam
            int ringX = (int)((nowMs / 18u) % 90u) + 20;
            if (ringX > localStart && ringX < localEnd) {
                int ry = DISH_GLOBAL_Y - (scrX0 + ringX - DISH_GLOBAL_X) / 14;
                s.drawCircle(ringX, ry, 8, pal[P_LASER_CORE]);
            }
        }
    }
}

__attribute__((noinline)) void drawDeathStar(TFT_eSprite &s, struct tm &now, suseconds_t subsec,
                                             unsigned long nowMs, uint8_t state, const uint16_t *pal) {
    bool pulseOn = (subsec < 500000L);
    bool energyHigh = (state >= 1);
    swDrawCenterChrome(s, "DEATH STAR", now, subsec,
                       pal[state == 2 ? P_LASER_CORE : (state == 1 ? P_LASER_MID : P_RED_MID)],
                       pal[energyHigh ? P_LASER_AURA : P_DARK_HUD],
                       pal[energyHigh ? P_LASER_MID : (pulseOn ? P_RED_MID : P_DARK_HUD)],
                       pal[pulseOn || energyHigh ? P_WHITE : P_RED_AURA],
                       pal[P_WHITE]);

    const int cx = 67, cy = 112, R = 38;

    // 3D scanline-shaded Death Star sphere with armored plating & curved terminator
    for (int dy = -R; dy <= R; ++dy) {
        int half = 0;
        int rr = R * R - dy * dy;
        while ((half + 1) * (half + 1) <= rr) ++half;
        int xStart = cx - half, xEnd = cx + half;
        int py = cy + dy;

        // Signature recessed Equatorial Trench
        if (dy >= -1 && dy <= 1) {
            s.drawFastHLine(xStart, py, xEnd - xStart + 1, pal[P_BLACK]);
            if (dy == 0) {
                for (int lx = xStart + 6; lx < cx + 10; lx += 11) s.drawPixel(lx, py, pal[P_STEEL_D]);
            }
            continue;
        }

        // Curved day/night terminator angled across sphere
        int xTerm = cx + 8 - dy / 3;
        int litEnd = (xTerm < xEnd) ? xTerm : xEnd;
        if (litEnd >= xStart) {
            int band = (dy / 6) & 1;
            uint16_t plate = pal[band ? P_HULL_L : P_HULL_M];
            s.drawFastHLine(xStart, py, litEnd - xStart + 1, plate);
            // Subtle panel seams on lit hemisphere
            for (int px = ((xStart + 7) & ~7); px <= litEnd; px += 9) {
                if (((px + dy * 3) & 3) == 0) s.drawPixel(px, py, pal[P_HULL_D]);
            }
        }
        if (litEnd < xEnd) {
            s.drawFastHLine(litEnd + 1, py, xEnd - litEnd, pal[P_HULL_D]);
        }
    }
    s.drawCircle(cx, cy, R, pal[state == 2 ? P_LASER_CORE : P_HULL_R]);

    // Concave Superlaser focusing dish (upper right quadrant)
    const int dishX = cx + 12, dishY = cy - 15; // (79, 97) = global (379, 97)
    s.fillCircle(dishX, dishY, 11, pal[P_DISH_D]);
    s.drawCircle(dishX, dishY, 11, pal[state >= 1 ? P_LASER_MID : P_HULL_R]);
    s.drawCircle(dishX, dishY, 6, pal[state >= 1 ? P_LASER_AURA : P_HULL_M]);

    // 8 tributary emitter spokes converging into focal orb
    static const int8_t spokes[8][2] = {
        {10, 0}, {7, 7}, {0, 10}, {-7, 7}, {-10, 0}, {-7, -7}, {0, -10}, {7, -7}
    };
    uint16_t rayCol = pal[(state == 2) ? P_WHITE : (state == 1) ? P_LASER_MID : P_HULL_M];
    if (state >= 1 || ((nowMs / 500u) & 1)) {
        for (int k = 0; k < 8; ++k) {
            s.drawLine(dishX + spokes[k][0], dishY + spokes[k][1], dishX, dishY, rayCol);
        }
    }
    if (state == 2) {
        s.fillCircle(dishX, dishY, 6, pal[P_LASER_MID]);
        s.fillCircle(dishX, dishY, 3, pal[P_WHITE]);
    } else if (state == 1) {
        int rGlow = 1 + (int)((nowMs / 120u) % 3u);
        s.fillCircle(dishX, dishY, rGlow, pal[P_LASER_CORE]);
    } else {
        s.fillCircle(dishX, dishY, 2, pal[P_HULL_R]);
    }
}

} // namespace

void MatrixClock::renderDeathStar(IPSClock &clock, TimeSync &time, WeatherService &weather) {
    unsigned long nowMs = millis();
    if (nowMs - lastFrame < 45) return;
    lastFrame = nowMs;
    ++frame;

    struct tm now;
    suseconds_t subsecond;
    time.getLocalTime(&now, &subsecond);

    uint16_t stateMs = 0;
    if (now.tm_sec <= 2) {
        deathStarState = 2;
        stateMs = (uint16_t)(now.tm_sec * 1000u + (uint32_t)(subsecond / 1000));
    } else if (now.tm_sec >= 56) {
        deathStarState = 1;
        stateMs = (uint16_t)((now.tm_sec - 56) * 1000u + (uint32_t)(subsecond / 1000));
    } else {
        deathStarState = 0;
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
    swDimPalette(DS_RAW, pal, P_COUNT);

    for (int i = 0; i < 6; ++i) {
        drawImperialSpace(sprite, i, nowMs, deathStarState, stateMs, pal);

        if (i == 2) {
            drawDeathStar(sprite, now, subsecond, nowMs, deathStarState, pal);
        } else if (i == 5) {
            const char *lbl = (deathStarState == 2) ? "FIRE!" : (deathStarState == 1) ? "CHARGING..." : "SUPERLASER";
            swDrawNavWeatherHud(sprite, weather, now, "IMPERIAL", lbl,
                                pal[deathStarState >= 1 ? P_LASER_MID : P_RED_MID],
                                pal[deathStarState >= 1 ? P_LASER_AURA : P_DARK_HUD],
                                pal[P_BOX_BG],
                                pal[deathStarState >= 1 ? P_LASER_MID : P_RED_AURA],
                                pal[P_WARM], pal[P_COOL], deathStarState >= 1);
        } else {
            bool blankHour = (i == 0 && hour < 10 && !IPSClock::getLeadingZero().value);
            if (!blankHour) {
                uint16_t aura = pal[deathStarState >= 1 ? P_LASER_AURA : P_RED_AURA];
                uint16_t body = pal[deathStarState >= 1 ? P_LASER_MID : P_RED_MID];
                swDrawHoloDigit(sprite, digits[i], aura, body, pal[P_WHITE], pal[P_STEEL_D]);
            }
        }

        tfts->chip_select.setDigit(SW_SCREENS[i]);
        sprite.pushSprite(0, 0);
    }
    tfts->release();
}
