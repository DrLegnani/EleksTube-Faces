#include "MatrixClock.h"
#include "TFTs.h"
#include "ColorConversion.h"
#include "DigitalRainAnimation.h"
#include <math.h>
#include <new>

// Global state shared with Backlights for Matrix theme
uint8_t matrixMinutePulse = 0;

namespace {
constexpr int COLS = 9;
constexpr int ROWS = 15;
constexpr int CELL_W = 15;
constexpr int CELL_H = 16;
constexpr int MASK_W = (TFT_WIDTH + 1) / 2;
constexpr int MASK_H = (TFT_HEIGHT + 1) / 2;
constexpr int MASK_BYTES = (MASK_W * MASK_H + 7) / 8;
const uint8_t screens[6] = {HOURS_TENS, HOURS_ONES, MINUTES_TENS, MINUTES_ONES, SECONDS_TENS, SECONDS_ONES};
const uint8_t segmentMasks[] = {0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f};
const uint8_t segmentX[] = {39,99,99,39,28,28,39};
const uint8_t segmentY[] = {22,32,79,119,79,32,71};

constexpr uint16_t c565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

bool inDigit(const uint8_t *mask, int x, int y) {
    if (!mask || x < 0 || y < 0 || x >= TFT_WIDTH || y >= TFT_HEIGHT) return false;
    unsigned int bit = (y / 2) * MASK_W + x / 2;
    return (mask[bit / 8] & (1u << (bit % 8))) != 0;
}

__attribute__((noinline)) void captureDigitMask(TFT_eSprite &sprite, int digit, uint8_t *mask) {
    memset(mask, 0, MASK_BYTES);
    sprite.fillSprite(TFT_BLACK);
    sprite.setTextFont(4);
    sprite.setTextSize(6);
    sprite.setTextDatum(MC_DATUM);
    sprite.setTextColor(TFT_WHITE);
    char text[2] = {char(digit == 10 ? ':' : '0' + digit), 0};
    for (int dx = -1; dx <= 1; ++dx)
        sprite.drawString(text, TFT_WIDTH / 2 + dx, (digit == 10 ? TFT_HEIGHT / 2 - 18 : TFT_HEIGHT / 2), 4);
    for (int y = 0; y < MASK_H; ++y) {
        for (int x = 0; x < MASK_W; ++x) {
            if (sprite.readPixel(x * 2, y * 2) != TFT_BLACK) {
                unsigned int bit = y * MASK_W + x;
                mask[bit / 8] |= 1u << (bit % 8);
            }
        }
    }
}

__attribute__((noinline)) void drawClippedGlyph(TFT_eSprite &sprite, char character, int x, int y, const uint8_t *mask, uint16_t color) {
    const GFXfont *font = tfts->getMatrixFont();
    unsigned int code = static_cast<unsigned char>(character);
    unsigned int first = pgm_read_word(&font->first);
    unsigned int last = pgm_read_word(&font->last);
    if (!mask || code < first || code > last) return;
    GFXglyph *glyph = &(((GFXglyph *)pgm_read_dword(&font->glyph))[code - first]);
    uint8_t width = pgm_read_byte(&glyph->width), height = pgm_read_byte(&glyph->height);
    int8_t dx = (int8_t)pgm_read_byte(&glyph->xOffset), dy = (int8_t)pgm_read_byte(&glyph->yOffset);
    const uint8_t *bitmap = (const uint8_t *)pgm_read_dword(&font->bitmap);
    unsigned int offset = pgm_read_word(&glyph->bitmapOffset);
    uint8_t bits = 0, bit = 0;
    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            if (!bit) { bits = pgm_read_byte(&bitmap[offset++]); bit = 0x80; }
            int px = x + dx + col, py = y + dy + row;
            if ((bits & bit) && inDigit(mask, px, py)) sprite.drawPixel(px, py, color);
            bit >>= 1;
        }
    }
}

__attribute__((noinline)) void rain(TFT_eSprite &sprite, int screen, unsigned long frame, const uint8_t *mask, int digit, uint8_t brightness) {
    sprite.fillSprite(TFT_BLACK);
    
    // Background subdued silhouette so the digit outline is readable
    if (mask && digit >= 0) {
        char text[2] = {char(digit == 10 ? ':' : '0' + digit), 0};
        sprite.setTextFont(4);
        sprite.setTextSize(6);
        sprite.setTextDatum(MC_DATUM);
        uint16_t sil = c565(0, 45, 15);
        sprite.setTextColor(tfts->dimColor(sil));
        int cy = (digit == 10 ? TFT_HEIGHT / 2 - 18 : TFT_HEIGHT / 2);
        for (int dx = -1; dx <= 1; ++dx)
            sprite.drawString(text, TFT_WIDTH / 2 + dx, cy, 4);
    }

    // Fluid digital rain with incandescent white heads
    sprite.setFreeFont(tfts->getMatrixFont());
    sprite.setTextSize(1);
    for (int col = 0; col < COLS; ++col) {
        int colSpeed = 1 + (col * 5 + screen * 7) % 3;
        int head = (frame * colSpeed / 2 + col * 17 + screen * 23) % (ROWS + 14);
        int secondHead = (head + (ROWS + 14) / 2) % (ROWS + 14);
        for (int row = 0; row < ROWS; ++row) {
            int dist = head - row;
            if (dist < 0 || dist >= 9) {
                dist = secondHead - row;
                if (dist < 0 || dist >= 9) continue;
            }
            uint16_t color = dist == 0 ? 0xFFFF :
                             dist == 1 ? c565(90, 255, 120) :
                             c565(0, 230 - dist * 24, (230 - dist * 24) >> 3);
            if (((row * 31 + col * 17 + frame + screen) & 0x1F) == 0) {
                color = 0xFFFF;
            }
            sprite.setTextColor(tfts->dimColor(color), TFT_BLACK);
            sprite.setCursor(col * CELL_W + 1, row * CELL_H + 14);
            unsigned int code = row * 19 + col * 11 + screen * 23 + (frame / 2 + row);
            char ch = (code & 1) ? char(33 + code % 31) : char(91 + code % 36);
            sprite.write(ch);
        }
    }

    // Characters clipped to the digit silhouette (forming the numbers out of Matrix characters)
    if (mask) {
        uint16_t light = c565(220, 255, 230);
        for (int col = 0; col < 11; ++col) {
            for (int row = 0; row < 17; ++row) {
                unsigned int code = row * 19 + col * 11 + screen * 23 + (frame / 2 + row);
                char character = (code & 1) ? char(33 + code % 31) : char(91 + code % 36);
                drawClippedGlyph(sprite, character, 1 + col * 12, row * 14 + 14, mask, light);
            }
        }
    }
}

// Screen 2: Date HUD
__attribute__((noinline)) void drawMatrixColonAndDate(TFT_eSprite &sprite, struct tm &now, unsigned long nowMs, uint8_t brightness) {
    uint16_t greenNeon = tfts->dimColor(c565(0, 255, 70));
    uint16_t white     = tfts->dimColor(0xFFFF);
    uint16_t darkLine  = tfts->dimColor(c565(0, 60, 20));
    
    // Header
    sprite.setTextFont(1);
    sprite.setTextSize(1);
    sprite.setTextDatum(TC_DATUM);
    sprite.setTextColor(greenNeon);
    sprite.drawString("// SYS_OP //", TFT_WIDTH / 2, 14, 1);
    sprite.drawFastHLine(20, 26, 95, darkLine);
    
    // Cyber Date readout in bright white for clear visibility
    sprite.drawFastHLine(20, 172, 95, darkLine);
    
    char date[8];
    date[0] = '0' + now.tm_mday / 10;
    date[1] = '0' + now.tm_mday % 10;
    date[2] = '.';
    date[3] = '0' + now.tm_mon / 10;
    date[4] = '0' + now.tm_mon % 10;
    date[5] = 0;
    sprite.setTextFont(4);
    sprite.setTextSize(1);
    sprite.setTextDatum(MC_DATUM);
    sprite.setTextColor(white);
    sprite.drawString(date, TFT_WIDTH / 2, 196, 4);
}

// Screen 5: Binary Cloud/Sun weather with 60-second progress bar
__attribute__((noinline)) void weatherOverlay(TFT_eSprite &sprite, WeatherService &weather, struct tm &now, uint8_t brightness) {
    uint8_t value = DigitalRainAnimation::getMatrixValue().value * brightness / 255;
    uint16_t white = sprite.color565(value, value, value);
    uint16_t whitePure = tfts->dimColor(0xFFFF);
    uint16_t darkBorder = c565(40, 70, 50);
    String icon = weather.getIconName(5);
    const char *ic = icon.c_str();
    bool isSun = (ic[0] == '0' && ic[1] == '1');
    static const uint16_t cloudMask[7] = {0x070, 0x088, 0x106, 0x201, 0x401, 0x401, 0x3FE};
    static const uint16_t sunMask[7]   = {0x421, 0x222, 0x0F8, 0x104, 0x505, 0x104, 0x0F8};
    const uint16_t *mask = isSun ? sunMask : cloudMask;
    sprite.setTextFont(1);
    sprite.setTextSize(1);
    sprite.setTextColor(white);
    for (int row = 0; row < 7; ++row) {
        uint16_t bits = mask[row];
        for (int col = 0; col < 11; ++col) {
            if (bits & (1 << (10 - col))) {
                sprite.setCursor(24 + col * 8, 30 + row * 10);
                sprite.write(((row + col) & 1) ? '1' : '0');
            }
        }
    }
    if ((ic[0] == '0' && (ic[1] == '9' || ic[1] == '0')) || (ic[0] == '1' && (ic[1] == '0' || ic[1] == '1' || ic[1] == '3'))) {
        for (int i = 0; i < 3; ++i) {
            sprite.setCursor(42 + i * 20, 102);
            sprite.write((ic[0] == '1' && ic[1] == '3') ? '*' : '/');
        }
    }
    float temperature = weather.getNowTemp();
    char text[12];
    if (isnan(temperature)) strcpy(text, "--C");
    else sprintf(text, "%dC", (int)temperature);
    sprite.setTextSize(2);
    sprite.setTextDatum(MC_DATUM);
    sprite.setTextColor(white);
    sprite.drawString(text, 67, 128, 4);
    
    // MAX / MIN temperature with larger font (Font 2) for easy reading
    float high = weather.getHigh(5), low = weather.getLow(5);
    if (!isnan(high) && !isnan(low)) {
        sprite.setTextFont(2);
        sprite.setTextSize(1);
        sprintf(text, "H:%dC  L:%dC", (int)high, (int)low);
        sprite.drawString(text, 67, 168, 2);
    }
    
    // 60-Second fill bar at the bottom in white
    sprite.drawRect(18, 218, 99, 10, darkBorder);
    int barW = (now.tm_sec * 95) / 59;
    if (barW > 0) sprite.fillRect(20, 220, barW, 6, whitePure);
}
} // namespace

void MatrixClock::releaseMasks() {
    delete[] digitMasks;
    digitMasks = nullptr;
    for (int &digit : cachedDigits) digit = -1;
}

void MatrixClock::render(IPSClock &clock, TimeSync &time, WeatherService &weather) {
    unsigned long nowMs = millis();
    unsigned int speed = DigitalRainAnimation::getMatrixSpeed().value;
    unsigned long frameDelay = speed ? 1000UL / speed : 45UL;
    if (frameDelay < 45) frameDelay = 45;
    if (nowMs - lastFrame < frameDelay) return;
    lastFrame = nowMs;
    ++frame;

    struct tm now;
    suseconds_t subsecond;
    time.getLocalTime(&now, &subsecond);
    int hour = now.tm_hour;
    if (IPSClock::getHourFormat()) { hour %= 12; if (hour == 0) hour = 12; }
    int digits[6] = {hour / 10, hour % 10, 10, now.tm_min / 10, now.tm_min % 10, -1};

    // Minute transition pulse for backlights
    matrixMinutePulse = (now.tm_sec == 0) ? 1 : 0;

    tfts->claim();
    if (!digitMasks) digitMasks = new (std::nothrow) uint8_t[5 * MASK_BYTES]();
    if (!digitMasks) { tfts->release(); return; }
    uint8_t brightness = clock.getBrightness();
    tfts->setDimming(brightness);
    tfts->enableAllDisplays();
    TFT_eSprite &sprite = tfts->getSprite();
    
    for (int i = 0; i < 6; ++i) {
        bool blankHour = (i == 0 && hour < 10 && !IPSClock::getLeadingZero().value);
        uint8_t *mask = nullptr;
        if (digits[i] >= 0 && !blankHour) {
            int slot = (i < 2) ? i : (i == 2 ? 4 : i - 1);
            mask = digitMasks + slot * MASK_BYTES;
            if (cachedDigits[slot] != digits[i]) {
                captureDigitMask(sprite, digits[i], mask);
                cachedDigits[slot] = digits[i];
            }
        }
        
        rain(sprite, i, frame, mask, digits[i], brightness);
        
        if (i == 2) {
            drawMatrixColonAndDate(sprite, now, nowMs, brightness);
        } else if (i == 5) {
            weatherOverlay(sprite, weather, now, brightness);
        }
        
        tfts->chip_select.setDigit(screens[i]);
        sprite.pushSprite(0, 0);
    }
    sprite.setTextSize(1);
    tfts->release();
}


// Global state shared with Backlights for Mario theme
uint8_t marioActiveScreen = 255;
uint8_t marioHitFlash = 0;
uint8_t marioTimeOfDay = 0; // 0=Day, 1=Dusk, 2=Night

namespace {
// Super Mario World lush green grass & chocolate dirt ground
__attribute__((noinline)) void drawGround(TFT_eSprite &sprite, int screen, uint8_t tod) {
    uint16_t border = tfts->dimColor(TFT_BLACK);
    uint16_t grassTop = tfts->dimColor(tod == 2 ? c565(30, 95, 45) : (tod == 1 ? c565(160, 130, 30) : c565(120, 215, 40)));
    uint16_t grassMid = tfts->dimColor(tod == 2 ? c565(18, 65, 30) : (tod == 1 ? c565(115, 80, 20) : c565(48, 160, 36)));
    uint16_t dirt     = tfts->dimColor(tod == 2 ? c565(45, 25, 20) : c565(136, 76, 32));
    uint16_t dirtHi   = tfts->dimColor(tod == 2 ? c565(70, 42, 35) : c565(178, 108, 48));

    // Dirt base
    sprite.fillRect(0, 198, TFT_WIDTH, 42, dirt);
    for (int y = 208; y <= 236; y += 14) sprite.drawFastHLine(0, y, TFT_WIDTH, border);
    int offset = (screen * 160) % 24;
    for (int x = -offset; x < TFT_WIDTH; x += 24) {
        sprite.drawFastVLine(x, 208, 14, border);
        sprite.drawFastVLine(x + 12, 222, 14, border);
        sprite.fillRect(x + 2, 210, 4, 3, dirtHi);
        sprite.fillRect(x + 14, 224, 4, 3, dirtHi);
    }

    // Wavy SMW grass cap
    sprite.fillRect(0, 194, TFT_WIDTH, 5, grassMid);
    sprite.drawFastHLine(0, 194, TFT_WIDTH, grassTop);
    sprite.drawFastHLine(0, 193, TFT_WIDTH, border);
    for (int x = 0; x < TFT_WIDTH; x += 8) {
        sprite.drawPixel(x + 4, 192, border);
        sprite.drawPixel(x + 4, 193, grassTop);
        sprite.fillRect(x + 2, 199, 4, 2, grassMid);
    }
    sprite.drawFastHLine(0, 200, TFT_WIDTH, border);
}

// SMW Hills with Eyes & Clouds
__attribute__((noinline)) void drawBackdrop(TFT_eSprite &sprite, int screen, uint8_t tod, unsigned long ms) {
    uint16_t border  = tfts->dimColor(TFT_BLACK);
    uint16_t white   = tfts->dimColor(TFT_WHITE);
    uint16_t hillCol = tfts->dimColor(tod == 2 ? c565(15, 45, 32) : (tod == 1 ? c565(95, 45, 75) : c565(48, 168, 68)));
    uint16_t hillHi  = tfts->dimColor(tod == 2 ? c565(25, 70, 48) : (tod == 1 ? c565(140, 70, 105) : c565(115, 218, 105)));

    if (tod == 2) {
        // Night: Stars and crescent moon
        for (int j = 0; j < 6; ++j) {
            int sx = (j * 27 + screen * 19) % 130, sy = 6 + (j * 29) % 75;
            sprite.drawPixel(sx, sy, ((ms / 300 + j + screen) % 3) ? tfts->dimColor(c565(255, 235, 140)) : white);
        }
        if (screen == 1) {
            uint16_t moon = tfts->dimColor(c565(255, 240, 160));
            sprite.fillCircle(105, 36, 12, moon);
            sprite.fillCircle(100, 34, 10, tfts->dimColor(c565(14, 18, 48)));
        }
    } else {
        // Day/Dusk: SMW Happy Clouds
        int cx = (screen * 41) % 65 + 8, cy = 16 + (screen % 2) * 16;
        sprite.fillCircle(cx + 12, cy + 10, 10, white);
        sprite.fillCircle(cx + 28, cy + 10, 10, white);
        sprite.fillRect(cx + 12, cy, 16, 20, white);
        sprite.drawFastHLine(cx + 6, cy + 19, 28, border);
        // Cute smiling eyes on cloud
        sprite.fillRect(cx + 16, cy + 8, 2, 5, border);
        sprite.fillRect(cx + 22, cy + 8, 2, 5, border);
    }

    // Iconic SMW rounded hills in background with eyes
    int hx = ((screen * 160) % 220) - 40;
    sprite.fillCircle(hx + 60, 193, 44, hillCol);
    sprite.drawCircle(hx + 60, 193, 44, border);
    sprite.drawFastHLine(hx + 40, 155, 40, hillHi);
    // Two characteristic SMW hill eyes
    sprite.fillRect(hx + 55, 162, 3, 7, border);
    sprite.fillRect(hx + 63, 162, 3, 7, border);
}

// Classic SMW Rotating / Question Block
__attribute__((noinline)) void drawQuestionBlock(TFT_eSprite &sprite, int x, int y, int size, bool hit, int anim) {
    uint16_t border = tfts->dimColor(TFT_BLACK);
    uint16_t shadow = tfts->dimColor(hit ? c565(75, 45, 20) : c565(178, 86, 0));
    uint16_t face   = tfts->dimColor(hit ? c565(136, 84, 46) : c565(250, 166, 12));
    uint16_t hi     = tfts->dimColor(hit ? c565(184, 122, 80) : c565(255, 238, 130));

    sprite.fillRect(x, y, size, size, border);
    sprite.fillRect(x + 2, y + 2, size - 4, size - 4, face);
    sprite.drawFastHLine(x + 2, y + 2, size - 4, hi);
    sprite.drawFastVLine(x + 2, y + 2, size - 4, hi);
    sprite.drawFastHLine(x + 2, y + size - 3, size - 4, shadow);
    sprite.drawFastVLine(x + size - 3, y + 2, size - 4, shadow);

    for (int ry = 0; ry < 2; ++ry) {
        for (int rx = 0; rx < 2; ++rx) {
            sprite.fillRect(x + 3 + rx * (size - 8), y + 3 + ry * (size - 8), 2, 2, border);
        }
    }

    if (!hit) {
        uint16_t qColor = (anim & 1) ? hi : border;
        int cx = x + size / 2, cy = y + size / 2 - 14;
        sprite.fillRect(cx - 10, cy, 20, 5, qColor);
        sprite.fillRect(cx + 5, cy + 5, 5, 8, qColor);
        sprite.fillRect(cx - 2, cy + 13, 10, 5, qColor);
        sprite.fillRect(cx - 2, cy + 18, 5, 5, qColor);
        sprite.fillRect(cx - 2, cy + 27, 5, 5, qColor);
    }
}

// SMW Warp Pipe with rounded specular highlight
__attribute__((noinline)) void drawPipe(TFT_eSprite &sprite, int x, int y, int w, int h, uint8_t tod) {
    uint16_t border = tfts->dimColor(TFT_BLACK);
    uint16_t dark   = tfts->dimColor(tod == 2 ? c565(0, 48, 12) : c565(0, 84, 16));
    uint16_t mid    = tfts->dimColor(tod == 2 ? c565(18, 105, 38) : c565(36, 178, 48));
    uint16_t hi     = tfts->dimColor(tod == 2 ? c565(65, 170, 80) : c565(140, 240, 80));

    for (int part = 0; part < 2; ++part) {
        int px = part ? x : x - 4, py = part ? y + 14 : y;
        int pw = part ? w : w + 8, ph = part ? h - 14 : 14;
        sprite.fillRect(px, py, pw, ph, border);
        sprite.fillRect(px + 2, py + 2, pw - 4, ph - 4, mid);
        sprite.fillRect(px + 3, py + 2, 5, ph - 4, hi);
        sprite.fillRect(px + pw - 8, py + 2, 6, ph - 4, dark);
    }
}

// Oval SMW Coin
__attribute__((noinline)) void drawCoin(TFT_eSprite &sprite, int cx, int cy, int frame) {
    int w = (frame % 4 == 0 || frame % 4 == 2) ? 4 : (frame % 4 == 1 ? 14 : 9);
    uint16_t border = tfts->dimColor(TFT_BLACK);
    uint16_t gold   = tfts->dimColor(c565(255, 218, 16));
    uint16_t hi     = tfts->dimColor(c565(255, 255, 195));
    sprite.fillRect(cx - w / 2 - 1, cy - 9, w + 2, 18, border);
    sprite.fillRect(cx - w / 2, cy - 8, w, 16, gold);
    if (w > 6) sprite.drawFastVLine(cx - 1, cy - 6, 12, hi);
}

// 7-Segment digit styled like SMW wood/stone blocks
__attribute__((noinline)) void drawSegmentDigit(TFT_eSprite &sprite, int digit, int bumpY, uint16_t gold, uint16_t outline) {
    uint16_t hi = tfts->dimColor(c565(255, 225, 120));
    for (int n = 0; n < 7; ++n) if (segmentMasks[digit] & (1 << n)) {
        int w = (n == 0 || n == 3 || n == 6) ? 57 : 9, h = (w == 57) ? 9 : 35;
        int sx = segmentX[n], sy = segmentY[n] + bumpY;
        sprite.fillRect(sx - 2, sy - 2, w + 4, h + 4, outline);
        sprite.fillRect(sx, sy, w, h, gold);
        sprite.drawFastHLine(sx, sy, w, hi);
        sprite.drawFastVLine(sx, sy, h, hi);
    }
}

// Stylized Super Mario World Weather Icons
__attribute__((noinline)) void drawSMWWeather(TFT_eSprite &sprite, WeatherService &weather, uint16_t gold, uint16_t outline, uint16_t white) {
    String icon = weather.getIconName(5);
    const char *ic = icon.c_str();
    uint16_t border = tfts->dimColor(TFT_BLACK);

    if (ic[0] == '0' && ic[1] == '1') {
        // SMW Smiling Sun
        uint16_t sunCore = tfts->dimColor(c565(255, 204, 0));
        uint16_t rayCol  = tfts->dimColor(c565(255, 136, 0));
        static const int8_t ray[8][2] = {{22,0},{16,16},{0,22},{-16,16},{-22,0},{-16,-16},{0,-22},{16,-16}};
        for (int r = 0; r < 8; ++r) {
            sprite.fillRect(67 + ray[r][0] - 2, 42 + ray[r][1] - 2, 5, 5, rayCol);
        }
        sprite.fillCircle(67, 42, 16, sunCore);
        sprite.drawCircle(67, 42, 16, border);
        // Smiling face
        sprite.fillRect(62, 38, 2, 5, border);
        sprite.fillRect(70, 38, 2, 5, border);
        sprite.drawFastHLine(64, 47, 7, border);
    } else if (ic[0] == '1' && ic[1] == '1') {
        // SMW Lightning Bolt Storm Cloud
        sprite.fillCircle(55, 40, 14, tfts->dimColor(c565(90, 95, 115)));
        sprite.fillCircle(78, 40, 16, tfts->dimColor(c565(75, 80, 100)));
        sprite.fillRect(55, 34, 25, 18, tfts->dimColor(c565(75, 80, 100)));
        // Yellow Mario Kart/SMW lightning bolt
        uint16_t bolt = tfts->dimColor(c565(255, 235, 30));
        sprite.fillTriangle(68, 46, 60, 60, 68, 60, bolt);
        sprite.fillTriangle(66, 60, 58, 76, 74, 58, bolt);
    } else if (ic[0] == '0' && (ic[1] == '9' || ic[1] == '0')) {
        // Lakitu Cloud with Rain drops
        sprite.fillCircle(54, 38, 12, white);
        sprite.fillCircle(78, 38, 15, white);
        sprite.fillRect(54, 32, 26, 18, white);
        sprite.drawFastHLine(48, 50, 40, border);
        // Lakitu smiling eyes
        sprite.fillRect(63, 38, 2, 5, border);
        sprite.fillRect(71, 38, 2, 5, border);
        // Blue rain drops
        uint16_t drop = tfts->dimColor(c565(80, 180, 255));
        for (int d = 0; d < 4; ++d) {
            sprite.fillRect(48 + d * 12, 58 + (d % 2) * 8, 3, 8, drop);
        }
    } else if (ic[0] == '1' && ic[1] == '3') {
        // SMW Ice Flower / Snow crystal
        uint16_t ice = tfts->dimColor(c565(130, 230, 255));
        sprite.fillCircle(67, 42, 14, ice);
        sprite.drawCircle(67, 42, 14, white);
        sprite.drawFastHLine(55, 42, 25, white);
        sprite.drawFastVLine(67, 30, 25, white);
    } else {
        // SMW Cape Feather (Wind/Clouds)
        uint16_t fYel = tfts->dimColor(c565(255, 215, 0));
        uint16_t fRed = tfts->dimColor(c565(235, 40, 40));
        sprite.fillRect(56, 32, 22, 16, fYel);
        sprite.fillRect(52, 48, 30, 10, fRed);
        sprite.drawFastVLine(67, 30, 28, border);
    }

    float temp = weather.getNowTemp(), high = weather.getHigh(5), low = weather.getLow(5);
    char text[20];
    if (isnan(temp)) strcpy(text, "--C"); else snprintf(text, sizeof(text), "%dC", (int)roundf(temp));
    sprite.setTextFont(4); sprite.setTextSize(2); sprite.setTextDatum(MC_DATUM);
    sprite.setTextColor(outline); sprite.drawString(text, 68, 116, 4);
    sprite.setTextColor(gold); sprite.drawString(text, 67, 115, 4);
    if (!isnan(high) && !isnan(low)) {
        sprite.setTextSize(1);
        snprintf(text, sizeof(text), "H %d L %d", (int)roundf(high), (int)roundf(low));
        sprite.setTextColor(outline); sprite.drawString(text, 68, 169, 4);
        sprite.setTextColor(gold); sprite.drawString(text, 67, 168, 4);
    }
}
}

// Super Mario World 16-Bit Clock Renderer
void MatrixClock::renderMario(IPSClock &clock, TimeSync &time, WeatherService &weather) {
    unsigned long ms = millis();
    static int8_t shown[6] = {-1,-1,-1,-1,-1,-1};
    static int8_t target[6] = {-1,-1,-1,-1,-1,-1};
    static unsigned long lastVisit = 0, start = 0, lastWeatherMs = 0;
    static bool active = false;
    static uint8_t dirty = 0x3F, lastHalfSec = 255;
    static int8_t prevScreen = -1;

    if (ms - lastFrame < (active ? 45UL : 200UL)) return;
    lastFrame = ms;

    struct tm now; suseconds_t subsecond;
    time.getLocalTime(&now, &subsecond);
    int rawHour = now.tm_hour, hour = rawHour;
    if (IPSClock::getHourFormat()) { hour %= 12; if (!hour) hour = 12; }
    int current[6] = {hour/10, hour%10, -1, now.tm_min/10, now.tm_min%10, -1};

    // Calculate Time of Day: 0=Day (7-17), 1=Dusk (5-7 and 17-19), 2=Night
    if ((rawHour >= 5 && rawHour < 7) || (rawHour >= 17 && rawHour < 19)) {
        marioTimeOfDay = 1; // Dusk
    } else if (rawHour >= 7 && rawHour < 17) {
        marioTimeOfDay = 0; // Day
    } else {
        marioTimeOfDay = 2; // Night
    }

    if (shown[0] < 0 || ms - lastVisit > 2000) {
        for (int i = 0; i < 6; ++i) shown[i] = target[i] = current[i];
        active = false;
        dirty = 0x3F;
        marioActiveScreen = 255;
        marioHitFlash = 0;
    }
    lastVisit = ms;

    if (!active) {
        bool changed = false;
        for (int i = 0; i < 6; ++i) {
            target[i] = current[i];
            if (shown[i] != current[i]) changed = true;
        }
        if (changed) {
            active = true;
            start = ms;
            dirty = 0x3F;
        }
    }

    unsigned long elapsed = ms - start;
    int world = active ? int(elapsed * 1008UL / 12000UL) - 48 : -100;
    if (active && elapsed >= 12000) {
        for (int i = 0; i < 6; ++i) shown[i] = target[i];
        active = false;
        dirty = 0x3F;
        marioActiveScreen = 255;
        marioHitFlash = 0;
    }

    if (active) {
        int s1 = world / 160;
        int s2 = (world + 48) / 160;
        if (s1 >= 0 && s1 < 6) dirty |= (1 << s1);
        if (s2 >= 0 && s2 < 6) dirty |= (1 << s2);
        if (prevScreen >= 0 && prevScreen != s1 && prevScreen != s2) dirty |= (1 << prevScreen);
        prevScreen = s1;
        marioActiveScreen = (s1 >= 0 && s1 < 6) ? s1 : (s2 >= 0 && s2 < 6 ? s2 : 255);
    } else {
        marioActiveScreen = 255;
    }

    uint8_t halfSec = (ms / 500) & 1;
    if (halfSec != lastHalfSec) {
        lastHalfSec = halfSec;
        dirty |= (1 << 2);
    }
    if (ms - lastWeatherMs > 5000) {
        lastWeatherMs = ms;
        dirty |= (1 << 5);
    }

    uint8_t brightness = clock.getBrightness();
    tfts->claim(); tfts->setDimming(brightness); tfts->setBox(TFT_WIDTH, TFT_HEIGHT);
    tfts->setImageJustification(TFTs::MIDDLE_CENTER); tfts->enableAllDisplays();
    TFT_eSprite &sprite = tfts->getSprite();

    // SMW Sky Palettes for Day, Sunset & Night
    uint16_t sky = tfts->dimColor(marioTimeOfDay == 2 ? c565(14, 18, 48) :
                                 (marioTimeOfDay == 1 ? c565(245, 115, 68) : c565(96, 176, 255)));
    uint16_t gold    = tfts->dimColor(c565(250, 166, 12));
    uint16_t outline = tfts->dimColor(c565(60, 24, 0));
    uint16_t white   = tfts->dimColor(TFT_WHITE);

    bool anyHitFlash = false;

    for (int i = 0; i < 6; ++i) {
        if (!(dirty & (1 << i))) continue;
        dirty &= ~(1 << i);

        int x = world - i * 160;
        bool willChange = active && current[i] >= 0 && shown[i] != target[i];
        bool present = active && x < TFT_WIDTH && x + 48 > 0;

        sprite.fillSprite(sky);
        drawBackdrop(sprite, i, marioTimeOfDay, ms);
        drawGround(sprite, i, marioTimeOfDay);

        if (i == 2) {
            drawPipe(sprite, 45, 156, 44, 44, marioTimeOfDay);
        }

        int marioY = 148;
        int marioPose = (ms / 120) % 2;
        int bumpY = 0;
        bool isHit = false;

        if (willChange) {
            int jx = x - 12;
            if (jx >= 0 && jx <= 64) {
                int h = (4 * jx * (64 - jx) * 52) / (64 * 64);
                marioY = 148 - h;
                marioPose = 2; // Jump

                if (jx >= 30 && jx < 42) {
                    bumpY = -6 + (jx - 30);
                    isHit = true;
                    anyHitFlash = true;
                    int coinY = 55 - (jx - 30) * 3;
                    drawCoin(sprite, 67, coinY, jx);
                } else if (jx >= 42) {
                    isHit = true;
                    int fx = jx - 42;
                    sprite.fillRect(36 - fx, 60 - fx + fx * fx / 6, 6, 6, gold);
                    sprite.fillRect(92 + fx, 60 - fx + fx * fx / 6, 6, 6, gold);
                    sprite.fillRect(32 - fx * 2, 78 + fx * fx / 10, 6, 6, outline);
                    sprite.fillRect(96 + fx * 2, 78 + fx * fx / 10, 6, 6, outline);
                }
            }
            if (jx >= 64) {
                shown[i] = target[i];
                willChange = false;
            }
        }

        if (shown[i] >= 0 && willChange && !isHit) {
            drawQuestionBlock(sprite, 36, 60 + bumpY, 62, false, ms / 250);
        } else if (shown[i] >= 0) {
            drawSegmentDigit(sprite, shown[i], willChange ? bumpY : 0, gold, outline);
        } else if (i == 2) {
            drawCoin(sprite, 67, 68, halfSec ? 1 : 0);
            drawCoin(sprite, 67, 108, halfSec ? 1 : 0);
            char date[6]; snprintf(date, sizeof(date), "%02d/%02d", now.tm_mday, now.tm_mon);
            sprite.setTextFont(4); sprite.setTextSize(1); sprite.setTextDatum(MC_DATUM);
            sprite.setTextColor(outline); sprite.drawString(date, 68, 143, 4);
            sprite.setTextColor(gold); sprite.drawString(date, 67, 142, 4);
        } else {
            drawSMWWeather(sprite, weather, gold, outline, white);
        }

        if (present) {
            char filename[24]; snprintf(filename, sizeof(filename), "/ips/mario/%d.bmp", marioPose);
            tfts->overlayImageAt(filename, x, marioY);
        }

        tfts->chip_select.setDigit(screens[i]);
        sprite.pushSprite(0, 0);
    }

    marioHitFlash = anyHitFlash ? 1 : 0;

    sprite.setTextSize(1);
    tfts->release();
}
