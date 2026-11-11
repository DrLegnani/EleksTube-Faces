#include "MatrixClock.h"
#include "TFTs.h"
#include <math.h>

namespace {
const uint8_t segmentMap[10] = {0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f};
const uint8_t screenDigits[6] = {HOURS_TENS,HOURS_ONES,MINUTES_TENS,MINUTES_ONES,SECONDS_TENS,SECONDS_ONES};

__attribute__((noinline)) int iwave(int v) {
    int p = (v & 255);
    int tri = p < 128 ? (p - 64) : (192 - p);
    return (tri * (96 - (tri < 0 ? -tri : tri))) >> 8;
}

constexpr uint16_t c565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

__attribute__((noinline)) uint16_t dimRgb(TFT_eSprite &s, uint8_t r, uint8_t g, uint8_t b) {
    return tfts->dimColor(c565(r, g, b));
}

__attribute__((noinline)) void fillChamfer(TFT_eSprite &s, int x, int y, int w, int h, uint16_t col) {
    s.fillRect(x + 2, y, w - 4, h, col);
    s.fillRect(x + 1, y + 1, w - 2, h - 2, col);
    s.fillRect(x, y + 2, w, h - 4, col);
}

__attribute__((noinline)) void drawShadowText(TFT_eSprite &s, const char *txt, int y, uint8_t sz, uint16_t fg, uint16_t sh) {
    s.setTextSize(sz);
    s.setTextColor(sh);
    s.drawString(txt, 69, y + 1, 4);
    s.setTextColor(fg);
    s.drawString(txt, 67, y, 4);
}

__attribute__((noinline)) void drawBevelDigit(TFT_eSprite &s, int value, uint16_t amber, uint16_t core, uint16_t shadow, uint16_t ghost) {
    static const uint8_t sx[7] = {40, 98, 98, 40, 30, 30, 40};
    static const uint8_t sy[7] = {42, 50, 114, 172, 114, 50, 107};
    static const uint8_t sw[7] = {55, 11, 11, 55, 11, 11, 55};
    static const uint8_t sh[7] = {10, 56, 56, 10, 56, 56, 10};
    uint8_t mask = segmentMap[value % 10];
    for (int i = 0; i < 7; ++i) {
        int x = sx[i], y = sy[i], w = sw[i], h = sh[i];
        bool horiz = w > h;
        if (!(mask & (1 << i))) {
            if (horiz) s.fillRect(x + 4, y + 3, w - 8, 4, ghost);
            else s.fillRect(x + 3, y + 4, 4, h - 8, ghost);
            continue;
        }
        fillChamfer(s, x + 3, y + 4, w, h, shadow);
        fillChamfer(s, x, y, w, h, amber);
        if (horiz) s.fillRect(x + 5, y + 2, w - 10, 2, core);
        else s.fillRect(x + 2, y + 5, 2, h - 10, core);
    }
}

__attribute__((noinline)) void drawVaporator(TFT_eSprite &s, int x, int baseY, uint16_t col, uint16_t hi) {
    s.fillRect(x, baseY - 38, 2, 38, col);
    s.fillRect(x - 2, baseY - 14, 6, 14, col);
    s.drawFastHLine(x - 7, baseY - 30, 15, col);
    s.drawFastHLine(x - 5, baseY - 22, 11, col);
    s.drawFastHLine(x - 4, baseY - 16, 9, hi);
    s.drawPixel(x, baseY - 40, hi);
}

__attribute__((noinline)) void drawDesertPanorama(TFT_eSprite &s, int screen, unsigned long ms) {
    static const uint8_t stopsY[5] = {0, 48, 108, 162, 206};
    static const uint8_t stopsRGB[5][3] = {
        {24, 15, 52},
        {86, 32, 90},
        {188, 58, 64},
        {242, 128, 40},
        {255, 214, 102}
    };
    int seg = 0;
    for (int y = 0; y < 206; ++y) {
        while (seg < 3 && y >= stopsY[seg + 1]) ++seg;
        int y0 = stopsY[seg], span = stopsY[seg + 1] - y0, t = y - y0;
        uint8_t r = stopsRGB[seg][0] + (stopsRGB[seg + 1][0] - stopsRGB[seg][0]) * t / span;
        uint8_t g = stopsRGB[seg][1] + (stopsRGB[seg + 1][1] - stopsRGB[seg][1]) * t / span;
        uint8_t b = stopsRGB[seg][2] + (stopsRGB[seg + 1][2] - stopsRGB[seg][2]) * t / span;
        s.drawFastHLine(0, y, TFT_WIDTH, dimRgb(s, r, g, b));
    }

    uint16_t starBright = tfts->dimColor(c565(255, 235, 190));
    uint16_t starDim = tfts->dimColor(c565(150, 115, 165));
    for (int k = 0; k < 7; ++k) {
        int sx = (k * 37 + screen * 29 + 11) % 128 + 3;
        int sy = (k * 19 + screen * 13 + 5) % 42 + 4;
        bool twinkle = ((ms / 500 + k + screen) % 5) == 0;
        s.drawPixel(sx, sy, twinkle ? starBright : starDim);
        if (twinkle && (k & 1)) s.drawFastHLine(sx - 1, sy, 3, starDim);
    }

    if (screen == 2) {
        int pulse = (ms / 250) % 8;
        int dr = (pulse < 4 ? pulse : 8 - pulse) / 2;
        uint16_t haloOut = tfts->dimColor(c565(248, 142, 48));
        uint16_t haloMid = tfts->dimColor(c565(255, 185, 76));
        s.fillCircle(40, 146, 18 + dr, haloOut);
        s.fillCircle(40, 146, 15, haloMid);
        s.fillCircle(40, 146, 12, tfts->dimColor(c565(255, 242, 175)));
        s.fillCircle(92, 165, 13 + dr, haloOut);
        s.fillCircle(92, 165, 10, haloMid);
        s.fillCircle(92, 165, 8, dimRgb(s, 255, 210, 105));
    }

    uint16_t mesaCol = dimRgb(s, 112, 44, 74);
    uint16_t mesaTop = dimRgb(s, 168, 72, 68);
    uint16_t duneMid = dimRgb(s, 84, 34, 68);
    uint16_t duneMidHi = dimRgb(s, 195, 96, 62);
    uint16_t duneNear = dimRgb(s, 46, 22, 52);
    uint16_t duneNearHi = dimRgb(s, 145, 62, 66);

    s.fillRect(0, 206, TFT_WIDTH, 34, duneNear);

    for (int x = 0; x < TFT_WIDTH; ++x) {
        int gx = screen * TFT_WIDTH + x;
        int mWave = iwave(gx * 2 + 30) + (iwave(gx * 5 + 90) >> 1);
        if (mWave > 7) mWave = 7;
        int yMesa = 183 - mWave;
        int yMid = 197 - ((iwave(gx * 2 + 140) * 3) >> 2) - (iwave(gx * 3 + 20) >> 2);
        int yNear = 213 - ((iwave(gx * 2 + 210) * 3) >> 2) - (iwave(gx * 4 + 70) >> 2);

        if (yMesa < yMid) {
            s.drawFastVLine(x, yMesa, yMid - yMesa, mesaCol);
            s.drawPixel(x, yMesa, mesaTop);
        }
        if (yMid < yNear) {
            s.drawFastVLine(x, yMid, yNear - yMid, duneMid);
            s.drawPixel(x, yMid, duneMidHi);
        }
        s.drawFastVLine(x, yNear, TFT_HEIGHT - yNear, duneNear);
        s.drawPixel(x, yNear, duneNearHi);
    }

    static const uint8_t vapX[6] = {22, 104, 0, 28, 34, 0};
    static const uint8_t vapY[6] = {198, 196, 0, 195, 204, 0};
    if (vapX[screen]) drawVaporator(s, vapX[screen], vapY[screen], duneNear, duneMidHi);
    if (screen == 0) drawVaporator(s, 108, 206, duneNear, duneMidHi);
    else if (screen == 4) drawVaporator(s, 96, 194, duneNear, duneMidHi);
    else if (screen == 1) {
        s.fillCircle(36, 194, 16, duneMidHi);
        s.fillCircle(36, 195, 15, duneNear);
        s.fillRect(16, 195, 54, 12, duneNear);
        s.fillRect(52, 189, 18, 8, duneNear);
        s.drawFastHLine(52, 189, 18, duneMidHi);
        s.fillCircle(61, 196, 4, tfts->dimColor(c565(255, 170, 60)));
    } else if (screen == 3) {
        s.fillTriangle(96, 196, 104, 168, 114, 196, mesaCol);
    } else if (screen == 5) {
        int cx = 74, cy = 198;
        s.fillTriangle(cx, cy, cx + 8, cy - 18, cx + 40, cy, duneNear);
        s.fillRect(cx + 8, cy - 18, 32, 18, duneNear);
        s.fillTriangle(cx + 40, cy - 18, cx + 40, cy, cx + 46, cy, duneNear);
        s.drawFastHLine(cx + 8, cy - 18, 32, duneMidHi);
        s.fillRect(cx + 6, cy, 14, 5, TFT_BLACK);
        s.fillRect(cx + 26, cy, 14, 5, TFT_BLACK);
        s.fillRect(cx + 6, cy - 13, 9, 3, tfts->dimColor(c565(255, 195, 70)));
    }
}

__attribute__((noinline)) void drawLandspeeder(TFT_eSprite &s, int x, int y) {
    uint16_t stripe = tfts->dimColor(c565(255, 205, 95));
    uint16_t glow = tfts->dimColor(c565(140, 235, 255));
    s.fillRect(x + 4, y + 14, 32, 3, tfts->dimColor(c565(28, 12, 34)));
    s.drawFastHLine(x - 14, y + 3, 14, glow);
    s.drawFastHLine(x - 9, y + 5, 9, stripe);
    s.fillRect(x + 2, y - 4, 11, 4, tfts->dimColor(c565(140, 130, 145)));
    s.fillRect(x, y - 3, 3, 3, glow);
    fillChamfer(s, x + 2, y + 1, 36, 7, tfts->dimColor(c565(215, 95, 70)));
    s.drawFastHLine(x + 6, y + 3, 28, stripe);
    s.fillTriangle(x + 16, y + 1, x + 23, y - 3, x + 27, y + 1, glow);
}

int weatherAsset(const String &icon) {
    int code = icon.toInt();
    return code == 1 ? (icon.endsWith("n") ? 1 : 0)
         : code == 2 ? 2
         : code == 9 ? 4
         : code == 10 ? 5
         : code == 11 ? 6
         : code == 13 ? 7
         : code == 50 ? 8 : 3;
}
} // namespace

void MatrixClock::renderTatooine(IPSClock &clock, TimeSync &time, WeatherService &weather) {
    unsigned long ms = millis();
    static int8_t shown[6] = {-1, -1, -1, -1, -1, -1};
    static int8_t target[6] = {-1, -1, -1, -1, -1, -1};
    static unsigned long lastVisit = 0, animStart = 0, lastWeatherMs = 0;
    static uint8_t lastBri = 0;
    static bool active = false;
    static bool dirty[6] = {true, true, true, true, true, true};

    if (ms - lastFrame < (active ? 85UL : 250UL)) return;
    lastFrame = ms;

    struct tm now;
    suseconds_t subsecond;
    time.getLocalTime(&now, &subsecond);
    int hour = now.tm_hour;
    if (IPSClock::getHourFormat()) {
        hour %= 12;
        if (!hour) hour = 12;
    }
    int current[6] = {hour / 10, hour % 10, -1, now.tm_min / 10, now.tm_min % 10, -1};
    if (!IPSClock::getLeadingZero().value && current[0] == 0) current[0] = -2;

    uint8_t bri = clock.getBrightness();
    if (shown[0] == -1 || ms - lastVisit > 2000 || bri != lastBri) {
        for (int i = 0; i < 6; ++i) {
            shown[i] = target[i] = current[i];
            dirty[i] = true;
        }
        lastBri = bri;
        active = false;
    }
    lastVisit = ms;

    if (!active) {
        for (int i = 0; i < 6; ++i) {
            target[i] = current[i];
            if (shown[i] != current[i]) {
                active = true;
                animStart = ms;
            }
        }
    }

    unsigned long elapsed = ms - animStart;
    int worldX = active ? int(elapsed * 940UL / 4200UL) - 44 : -200;
    if (active && elapsed >= 4200) {
        for (int i = 0; i < 6; ++i) {
            if (shown[i] != target[i]) {
                shown[i] = target[i];
                dirty[i] = true;
            }
        }
        active = false;
    }

    dirty[2] = true;
    if (ms - lastWeatherMs > 5000) {
        dirty[5] = true;
        lastWeatherMs = ms;
    }

    tfts->claim();
    tfts->setDimming(bri);
    tfts->setBox(TFT_WIDTH, TFT_HEIGHT);
    tfts->setImageJustification(TFTs::MIDDLE_CENTER);
    tfts->enableAllDisplays();
    TFT_eSprite &s = tfts->getSprite();

    uint16_t amber = dimRgb(s, 255, 206, 82);
    uint16_t core = dimRgb(s, 255, 244, 185);
    uint16_t shadow = dimRgb(s, 48, 16, 36);
    uint16_t ghost = dimRgb(s, 92, 38, 58);

    for (int i = 0; i < 6; ++i) {
        int lx = worldX - i * 148;
        bool speederHere = active && lx < TFT_WIDTH + 16 && lx + 44 > 0;
        static bool wasSpeeder[6] = {};

        if (active && shown[i] != target[i] && lx >= 52) {
            shown[i] = target[i];
            dirty[i] = true;
        }

        if (!dirty[i] && !speederHere && !wasSpeeder[i]) continue;
        wasSpeeder[i] = speederHere;
        dirty[i] = false;

        drawDesertPanorama(s, i, ms);
        s.setTextFont(4);
        s.setTextDatum(MC_DATUM);

        if (shown[i] >= 0) {
            drawBevelDigit(s, shown[i], amber, core, shadow, ghost);
        } else if (i == 2) {
            int wave = (ms % 1000) < 500 ? (ms % 500) : (1000 - (ms % 1000));
            uint8_t p = 155 + wave * 100 / 500;
            uint16_t dotCol = dimRgb(s, 255, (p * 210) >> 8, (p * 95) >> 8);
            uint16_t dotHalo = dimRgb(s, 180, 68, 45);
            s.fillCircle(67, 68, 6, dotHalo);
            s.fillCircle(67, 68, 4, dotCol);
            s.fillCircle(67, 108, 6, dotHalo);
            s.fillCircle(67, 108, 4, dotCol);

            char date[6];
            snprintf(date, sizeof(date), "%02d/%02d", now.tm_mday, now.tm_mon);
            drawShadowText(s, date, 200, 1, core, shadow);
        } else if (i == 5) {
            char path[24];
            snprintf(path, sizeof(path), "/ips/tron/w%d.bmp", weatherAsset(weather.getIconName(5)));
            tfts->overlayImageAt(path, 25, 16);
            float temp = weather.getNowTemp(), hi = weather.getHigh(5), lo = weather.getLow(5);
            char text[16];
            if (isnan(temp)) strcpy(text, "--C");
            else snprintf(text, sizeof(text), "%dC", (int)roundf(temp));
            drawShadowText(s, text, 115, 2, core, shadow);

            if (!isnan(hi) && !isnan(lo)) {
                snprintf(text, sizeof(text), "MAX %dC", (int)roundf(hi));
                drawShadowText(s, text, 159, 1, amber, shadow);
                snprintf(text, sizeof(text), "MIN %dC", (int)roundf(lo));
                drawShadowText(s, text, 180, 1, amber, shadow);
            }
        }

        if (speederHere) {
            drawLandspeeder(s, lx, 184 + ((lx / 8) & 1));
        }

        tfts->chip_select.setDigit(screenDigits[i]);
        s.pushSprite(0, 0);
    }

    s.setTextSize(1);
    tfts->release();
}
