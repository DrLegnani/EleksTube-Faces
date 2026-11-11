#include "StarWarsHud.h"

const uint8_t SW_SCREENS[6] = {HOURS_TENS, HOURS_ONES, MINUTES_TENS, MINUTES_ONES, SECONDS_TENS, SECONDS_ONES};

void swDimPalette(const uint16_t *raw, uint16_t *out, int count) {
    for (int i = 0; i < count; ++i) {
        out[i] = tfts->dimColor(raw[i]);
    }
}

void swFillChamfer(TFT_eSprite &s, int x, int y, int w, int h, int c, uint16_t col) {
    if (w <= 0 || h <= 0) return;
    if (c * 2 > w) c = w / 2;
    if (c * 2 > h) c = h / 2;
    if (h > 2 * c) s.fillRect(x, y + c, w, h - 2 * c, col);
    for (int i = 0; i < c; ++i) {
        int inset = c - i;
        int rw = w - 2 * inset;
        if (rw > 0) {
            s.drawFastHLine(x + inset, y + i, rw, col);
            s.drawFastHLine(x + inset, y + h - 1 - i, rw, col);
        }
    }
}

void swDrawTxt(TFT_eSprite &s, const char *str, int x, int y, uint8_t font, uint8_t size, uint8_t datum, uint16_t col) {
    s.setTextFont(font);
    s.setTextSize(size);
    s.setTextDatum(datum);
    s.setTextColor(col);
    s.drawString(str, x, y, font);
}

void swDrawHoloDigit(TFT_eSprite &s, int digit, uint16_t auraCol, uint16_t bodyCol, uint16_t coreCol, uint16_t bracketCol) {
    if (digit < 0 || digit > 9) return;
    static const uint8_t segMask[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
    static const uint8_t segGeom[7][5] = {
        {25, 31, 85, 17, 5},  // A
        {97, 35, 17, 85, 4},  // B
        {97, 121, 17, 85, 4}, // C
        {25, 192, 85, 17, 5}, // D
        {21, 121, 17, 85, 4}, // E
        {21, 35, 17, 85, 4},  // F
        {27, 111, 81, 17, 5}  // G
    };

    s.drawFastHLine(12, 16, 12, bracketCol);  s.drawFastVLine(12, 16, 10, bracketCol);
    s.drawFastHLine(111, 16, 12, bracketCol); s.drawFastVLine(122, 16, 10, bracketCol);
    s.drawFastHLine(12, 222, 12, bracketCol); s.drawFastVLine(12, 213, 10, bracketCol);
    s.drawFastHLine(111, 222, 12, bracketCol); s.drawFastVLine(122, 213, 10, bracketCol);

    uint8_t m = segMask[digit];
    for (int b = 0; b < 7; ++b) {
        if (!(m & (1 << b))) continue;
        int x = segGeom[b][0], y = segGeom[b][1], w = segGeom[b][2], h = segGeom[b][3], c = segGeom[b][4];
        swFillChamfer(s, x - 2, y - 2, w + 4, h + 4, c + 1, auraCol);
        swFillChamfer(s, x, y, w, h, c, bodyCol);
        swFillChamfer(s, x + 5, y + 5, w - 10, h - 10, c > 2 ? c - 2 : 1, coreCol);
    }
}

void swDrawCenterChrome(TFT_eSprite &s, const char *title, struct tm &now, suseconds_t subsec,
                        uint16_t titleCol, uint16_t lineCol, uint16_t colonBody, uint16_t colonCore, uint16_t dateCol) {
    swDrawTxt(s, title, TFT_WIDTH / 2, 7, 1, 1, TC_DATUM, titleCol);
    s.drawFastHLine(16, 19, 103, lineCol);

    for (int j = 0; j < 2; ++j) {
        int cyDot = 25 + j * 14;
        swFillChamfer(s, 63, cyDot, 9, 9, 2, colonBody);
        swFillChamfer(s, 65, cyDot + 2, 5, 5, 1, colonCore);
    }
    (void)subsec;

    s.drawFastHLine(16, 173, 103, lineCol);
    char date[6] = {
        char('0' + now.tm_mday / 10), char('0' + now.tm_mday % 10), '.',
        char('0' + now.tm_mon / 10), char('0' + now.tm_mon % 10), 0
    };
    swDrawTxt(s, date, TFT_WIDTH / 2, 198, 4, 1, MC_DATUM, dateCol);
    s.drawFastHLine(32, 220, 71, lineCol);
}

void swDrawNavWeatherHud(TFT_eSprite &s, WeatherService &weather, struct tm &now,
                         const char *topTitle, const char *barLabel,
                         uint16_t accentCol, uint16_t lineCol, uint16_t boxBg, uint16_t borderCol,
                         uint16_t warmCol, uint16_t coolCol, bool highlightBar) {
    uint16_t white = tfts->dimColor(0xFFFF);
    uint16_t amber = tfts->dimColor(swC565(255, 195, 50));
    uint16_t red   = tfts->dimColor(swC565(195, 55, 48));

    swDrawTxt(s, topTitle, TFT_WIDTH / 2, 7, 1, 1, TC_DATUM, accentCol);
    s.drawFastHLine(16, 19, 103, lineCol);

    swFillChamfer(s, 29, 25, 77, 52, 6, boxBg);
    s.drawFastHLine(35, 25, 16, borderCol); s.drawFastHLine(84, 25, 16, borderCol);
    s.drawFastHLine(35, 76, 16, borderCol); s.drawFastHLine(84, 76, 16, borderCol);

    const char *ic = weather.getIconName(5).c_str();
    char c0 = ic[0], c1 = ic[1];
    if (c0 == '0' && c1 == '1') {
        s.fillCircle(60, 50, 11, amber); s.drawCircle(60, 50, 11, white);
        s.fillCircle(79, 43, 6, red);    s.drawCircle(79, 43, 6, amber);
    } else if (c0 == '1' && c1 == '3') {
        s.drawFastHLine(53, 51, 29, white); s.drawFastVLine(67, 37, 29, white);
        s.drawLine(57, 41, 77, 61, accentCol); s.drawLine(57, 61, 77, 41, accentCol);
        s.fillCircle(67, 51, 3, white);
    } else if (c0 == '5' && c1 == '0') {
        for (int i = 0; i < 4; ++i) s.drawFastHLine(44 + (i & 1) * 8, 38 + i * 7, 38, (i & 1) ? white : accentCol);
    } else {
        s.fillCircle(57, 49, 10, boxBg); s.drawCircle(57, 49, 10, accentCol);
        s.fillCircle(71, 45, 12, boxBg); s.drawCircle(71, 45, 12, accentCol);
        s.fillCircle(82, 51, 8, boxBg);  s.drawCircle(82, 51, 8, accentCol);
        s.fillRect(57, 44, 25, 14, boxBg);
        s.drawFastHLine(54, 59, 30, white);
        if (c0 == '0' && c1 == '2') {
            s.fillCircle(48, 41, 6, amber);
        } else if (c0 == '1' && c1 == '1') {
            s.drawLine(67, 56, 62, 65, amber);
            s.drawLine(62, 65, 70, 65, amber);
            s.drawLine(70, 65, 65, 74, white);
        } else if (c0 == '0' && (c1 == '9' || c1 == '0')) {
            for (int r = 0; r < 3; ++r) s.drawLine(55 + r * 10, 62, 52 + r * 10, 71, white);
        }
    }

    char unit = (WeatherService::getUnits().value == "imperial") ? 'F' : 'C';
    float vals[3] = {weather.getNowTemp(), weather.getHigh(5), weather.getLow(5)};
    int ints[3];
    for (int k = 0; k < 3; ++k) ints[k] = (int)(vals[k] + (vals[k] >= 0.0f ? 0.5f : -0.5f));

    char buf[16];
    snprintf(buf, sizeof(buf), "%d%c", ints[0], unit);
    swDrawTxt(s, buf, TFT_WIDTH / 2, 100, 4, 1, MC_DATUM, white);

    snprintf(buf, sizeof(buf), "MAX %d%c", ints[1], unit);
    swDrawTxt(s, buf, TFT_WIDTH / 2, 132, 1, 2, MC_DATUM, warmCol);

    snprintf(buf, sizeof(buf), "MIN %d%c", ints[2], unit);
    swDrawTxt(s, buf, TFT_WIDTH / 2, 156, 1, 2, MC_DATUM, coolCol);

    s.drawFastHLine(16, 180, 103, lineCol);
    swDrawTxt(s, barLabel, TFT_WIDTH / 2, 188, 1, 1, TC_DATUM, highlightBar ? white : accentCol);

    const int bx = 17, by = 203, bw = 101, bh = 14;
    s.drawRect(bx, by, bw, bh, highlightBar ? white : borderCol);
    s.fillRect(bx + 1, by + 1, bw - 2, bh - 2, boxBg);
    int fillW = (now.tm_sec * (bw - 4)) / 59;
    if (highlightBar && fillW == 0) fillW = 4;
    if (fillW > 0) s.fillRect(bx + 2, by + 2, fillW, bh - 4, white);
}
