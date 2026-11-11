#include "MatrixClock.h"
#include "StarWarsHud.h"
#include "IPSClock.h"

// Shared with Backlights: 0 = Vice Sunset cruise, 1 = 5-Star Police Heli Chase (57-59s), 2 = Nitro Turbo Overdrive (00-02s)
uint8_t viceCityState = 0;

namespace {

enum VicePal {
    P_WHITE = 0,
    P_BLACK,
    P_SKY_TOP,      // Deep night violet (#1B0738)
    P_SKY_VIOLET,   // Rich GTA VI purple (#4B1068)
    P_SKY_MAGENTA,  // Vibrant Vice magenta (#9C1878)
    P_SKY_PINK,     // GTA VI neon hot-pink (#FF2882)
    P_SKY_CORAL,    // Sunset coral-orange (#FF6838)
    P_SKY_GOLD,     // Horizon golden-yellow (#FFB82E)
    P_SUN_TOP,      // Retro sun upper gold (#FFEE55)
    P_SUN_BOT,      // Retro sun lower pink-orange (#FF4578)
    P_SILHOUETTE,   // Dark purple-black palm & skyline silhouette (#120424)
    P_SEA_DARK,     // Deep ocean indigo (#140936)
    P_SEA_CYAN,     // Tropical bay turquoise shimmer (#00D8E8)
    P_ROAD_DARK,    // Wet asphalt (#181028)
    P_NEON_PINK,    // Bright neon pink tube (#FF1E90)
    P_NEON_PINK_L,  // Hot pink inner glow (#FF92D0)
    P_NEON_CYAN,    // Electric Miami cyan (#00F5FF)
    P_NEON_CYAN_D,  // Deep cyan shadow/border (#006880)
    P_GTA_GREEN,    // Classic GTA cash green (#28E858)
    P_POLICE_RED,   // VCPD siren red (#FF1828)
    P_POLICE_BLUE,  // VCPD siren blue (#1868FF)
    P_CHROME,       // Car body / chrome highlight (#D8E8F8)
    P_COUNT
};

const uint16_t VICE_RAW[P_COUNT] = {
    0xFFFF,                 // P_WHITE
    0x0000,                 // P_BLACK
    swC565(27, 7, 56),      // P_SKY_TOP
    swC565(75, 16, 104),    // P_SKY_VIOLET
    swC565(156, 24, 120),   // P_SKY_MAGENTA
    swC565(255, 40, 130),   // P_SKY_PINK
    swC565(255, 104, 56),   // P_SKY_CORAL
    swC565(255, 184, 46),   // P_SKY_GOLD
    swC565(255, 238, 85),   // P_SUN_TOP
    swC565(255, 69, 120),   // P_SUN_BOT
    swC565(18, 4, 36),      // P_SILHOUETTE
    swC565(20, 9, 54),      // P_SEA_DARK
    swC565(0, 216, 232),    // P_SEA_CYAN
    swC565(24, 16, 40),     // P_ROAD_DARK
    swC565(255, 30, 144),   // P_NEON_PINK
    swC565(255, 146, 208),  // P_NEON_PINK_L
    swC565(0, 245, 255),    // P_NEON_CYAN
    swC565(0, 104, 128),    // P_NEON_CYAN_D
    swC565(40, 232, 88),    // P_GTA_GREEN
    swC565(255, 24, 40),    // P_POLICE_RED
    swC565(24, 104, 255),   // P_POLICE_BLUE
    swC565(216, 232, 248)   // P_CHROME
};

// Draw a detailed tropical palm tree silhouette (curved trunk + 6 drooping fronds)
__attribute__((noinline)) void drawPalmTree(TFT_eSprite &s, int baseX, int baseY, int height,
                                            int leanX, unsigned long nowMs, uint16_t col, uint16_t rimCol) {
    int sway = (int)((nowMs / 280u + baseX) & 3) - 1;
    int topX = baseX + leanX + sway;
    int topY = baseY - height;

    // Curved segmented trunk
    for (int i = 0; i <= 12; ++i) {
        int ty = baseY - (height * i) / 12;
        int curve = (leanX * i * i) / 144 + (sway * i) / 12;
        int tx = baseX + curve;
        int thick = (i < 4) ? 3 : 2;
        s.fillRect(tx - 1, ty, thick, (height / 11) + 1, col);
        if (i & 1) s.drawPixel(tx + thick - 1, ty, rimCol);
    }

    // 6 arched palm fronds radiating from crown (topX, topY)
    static const int8_t frondDX[6] = {-22, -16, -7, 8, 17, 23};
    static const int8_t frondDY[6] = {  7,  -5, -11, -10, -4, 8};
    for (int f = 0; f < 6; ++f) {
        int fx = topX + (frondDX[f] * height) / 52;
        int fy = topY + (frondDY[f] * height) / 52;
        int midX = (topX + fx) / 2;
        int midY = ((topY + fy) / 2) - 4;
        s.drawLine(topX, topY, midX, midY, col);
        s.drawLine(midX, midY, fx, fy, col);
        s.drawLine(topX, topY - 1, midX, midY - 1, rimCol);
        // Frond leaflets hanging down
        for (int l = 1; l <= 3; ++l) {
            int lx = topX + ((fx - topX) * l) / 4;
            int ly = topY + ((fy - topY) * l) / 4 - (l == 2 ? 2 : 0);
            s.drawLine(lx, ly, lx + (frondDX[f] > 0 ? 2 : -2), ly + 4, col);
        }
    }
}

// Panoramic GTA VI / Vice City Ocean Drive Sunset + Neon Grid backdrop across all 6 screens
__attribute__((noinline)) void drawViceBackdrop(TFT_eSprite &s, int screen, unsigned long nowMs,
                                                uint8_t state, uint16_t stateMs, const uint16_t *pal) {
    // 1. GTA VI Tropical Sunset Sky Gradient (y = 0..146)
    static const uint8_t skyBands[6] = {
        P_SKY_TOP, P_SKY_VIOLET, P_SKY_MAGENTA, P_SKY_PINK, P_SKY_CORAL, P_SKY_GOLD
    };
    for (int y = 0; y < 146; ++y) {
        int bandIdx = (y * 5) / 146;
        int sub = (y * 5) % 146;
        // Dithered transition between adjacent sunset bands for ultra-smooth gradient
        uint8_t cIdx = (sub > 95 && (y & 1)) ? skyBands[bandIdx + 1] : skyBands[bandIdx];
        s.drawFastHLine(0, y, TFT_WIDTH, pal[cIdx]);
    }

    // Twinkling evening stars in upper violet sky (y = 4..36)
    for (int k = 0; k < 7; ++k) {
        int sx = (screen * 31 + k * 19 + 7) % 131 + 2;
        int sy = 4 + ((screen * 13 + k * 11) % 30);
        if (((nowMs / 220u + k + screen) & 3) != 0) {
            s.drawPixel(sx, sy, pal[P_WHITE]);
        }
    }

    // 5-Star Police Siren Strobe Bar in upper sky during Sec 57-02 (State 1 & 2)
    if (state >= 1) {
        bool redPhase = (((nowMs / 95u) + screen) & 1) != 0;
        uint16_t sirenCol = pal[redPhase ? P_POLICE_RED : P_POLICE_BLUE];
        s.fillRect(0, 0, TFT_WIDTH, 5, sirenCol);
        s.drawFastHLine(0, 6, TFT_WIDTH, pal[P_WHITE]);
    }

    // 2. Sliced Retro Synthwave Sunset Sun (largest on center screens, glowing on horizon y = 92..145)
    if (screen == 2 || screen == 1 || screen == 3) {
        int sunCX = (screen == 2) ? 67 : (screen == 1 ? 128 : 6);
        int sunCY = 120;
        int sunR  = (screen == 2) ? 34 : 26;
        for (int dy = -sunR; dy <= sunR; ++dy) {
            int sy = sunCY + dy;
            if (sy < 78 || sy >= 146) continue;
            // Iconic horizontal synthwave slice gaps in lower half of the sun
            if (dy > 4 && ((dy % 6) >= 3)) continue;
            int halfW = 0;
            while ((halfW + 1) * (halfW + 1) + dy * dy <= sunR * sunR) ++halfW;
            uint16_t sunCol = pal[(dy < -8) ? P_SUN_TOP : (dy < 8 ? P_SKY_GOLD : P_SUN_BOT)];
            int x0 = sunCX - halfW; if (x0 < 0) x0 = 0;
            int x1 = sunCX + halfW; if (x1 >= TFT_WIDTH) x1 = TFT_WIDTH - 1;
            if (x1 >= x0) s.drawFastHLine(x0, sy, x1 - x0 + 1, sunCol);
        }
    }

    // 3. Distant Miami / Ocean Drive Art Deco Skyline Silhouette (y = 118..146)
    for (int b = 0; b < 5; ++b) {
        int bx = b * 27 + ((screen * 9) % 7);
        int bw = 18 + ((b + screen) % 3) * 4;
        int bh = 14 + ((b * 3 + screen) % 4) * 6;
        int by = 146 - bh;
        s.fillRect(bx, by, bw, bh, pal[P_SILHOUETTE]);
        // Vertical neon hotel sign strip (alternating Cyan and Hot Pink)
        uint16_t neonCol = pal[((b + screen) & 1) ? P_NEON_CYAN : P_NEON_PINK];
        s.drawFastVLine(bx + 3, by + 2, bh - 4, neonCol);
        s.drawFastHLine(bx, by, bw, neonCol);
    }

    // Seagulls gliding across the sunset sky
    for (int g = 0; g < 2; ++g) {
        int gx = ((int)(nowMs / 45u) + screen * 47 + g * 61) % 125 + 5;
        int gy = 48 + g * 18 + (screen & 1) * 7;
        int wing = ((nowMs / 160u + g) & 1) ? -2 : 1;
        s.drawLine(gx - 4, gy + wing, gx, gy, pal[P_SILHOUETTE]);
        s.drawLine(gx, gy, gx + 4, gy + wing, pal[P_SILHOUETTE]);
    }

    // 4. Vice Beach Ocean Water & Shimmering Neon Reflections (y = 146..182)
    s.fillRect(0, 146, TFT_WIDTH, 36, pal[P_SEA_DARK]);
    s.drawFastHLine(0, 146, TFT_WIDTH, pal[P_NEON_CYAN]);
    for (int wy = 149; wy < 182; wy += 3) {
        int shift = (int)((nowMs / 50u + wy * 5 + screen * 19) % 24) - 12;
        uint16_t wCol = pal[((wy / 3) & 1) ? P_NEON_PINK : P_SEA_CYAN];
        int wx = 18 + shift;
        int ww = 98 - abs(shift);
        s.drawFastHLine(wx, wy, ww, wCol);
    }

    // 5. Ocean Drive Perspective Synthwave Grid & Wet Asphalt (y = 182..239)
    s.fillRect(0, 182, TFT_WIDTH, 58, pal[P_ROAD_DARK]);
    s.drawFastHLine(0, 182, TFT_WIDTH, pal[P_NEON_PINK]);
    s.drawFastHLine(0, 183, TFT_WIDTH, pal[P_WHITE]);

    // Scrolling horizontal synthwave grid lines
    int roadScroll = (int)((nowMs / 35u) % 10);
    for (int ry = 188 + roadScroll; ry < 240; ry += 10) {
        s.drawFastHLine(0, ry, TFT_WIDTH, pal[P_SKY_MAGENTA]);
    }
    // Perspective neon cyan grid lines radiating toward viewer
    for (int gx = -15; gx <= 150; gx += 33) {
        int botX = 67 + ((gx - 67) * 18) / 10;
        s.drawLine(gx, 184, botX, 239, pal[P_NEON_CYAN_D]);
    }

    // Palm tree silhouettes framing the outer edges of each screen
    drawPalmTree(s, 11, 184, 52, 8, nowMs, pal[P_SILHOUETTE], pal[P_NEON_PINK]);
    drawPalmTree(s, 124, 184, 46, -7, nowMs, pal[P_SILHOUETTE], pal[P_NEON_CYAN]);

    // 6. Sec 57-59 (State 1): VCPD Police Helicopter & Searchlight Cone sweeping the screens!
    if (state == 1) {
        int beamX = (int)((stateMs * 135u) / 3000u);
        if ((screen & 1) == 1) beamX = 135 - beamX;
        // Diagonal white/cyan helicopter searchlight rays
        for (int d = -10; d <= 10; d += 2) {
            s.drawLine(67, 0, beamX + d, 230, pal[(abs(d) <= 2) ? P_WHITE : P_NEON_CYAN]);
        }
    }

    // 7. Sec 00-02 (State 2): Nitro Supercar streaking across all 6 screens with Neon Twin Flame Trails!
    if (state == 2) {
        int gCarX = (int)((stateMs * 1120u) / 3000u) - 60;
        int scrX0 = screen * 150;
        int localCar = gCarX - scrX0;
        int trailStart = localCar - 320;
        if (localCar >= 0 && trailStart < TFT_WIDTH) {
            int x0 = trailStart < 0 ? 0 : trailStart;
            int x1 = localCar >= TFT_WIDTH ? TFT_WIDTH - 1 : localCar;
            // Twin hot-pink & electric-cyan nitro tire streaks along Ocean Drive (y = 198..212)
            s.fillRect(x0, 198, x1 - x0 + 1, 4, pal[P_NEON_CYAN]);
            s.drawFastHLine(x0, 199, x1 - x0 + 1, pal[P_WHITE]);
            s.fillRect(x0, 206, x1 - x0 + 1, 4, pal[P_NEON_PINK]);
            s.drawFastHLine(x0, 207, x1 - x0 + 1, pal[P_SKY_GOLD]);
        }
        // Draw speeding white/pink Lamborghini Countach side silhouette at the head of the nitro trail
        if (localCar >= -30 && localCar < TFT_WIDTH + 10) {
            int cx = localCar;
            int cy = 196;
            s.fillTriangle(cx - 28, cy + 8, cx - 10, cy - 3, cx + 12, cy + 8, pal[P_WHITE]);
            s.fillRect(cx - 28, cy + 3, 36, 6, pal[P_NEON_PINK]);
            s.fillCircle(cx - 20, cy + 9, 4, pal[P_SILHOUETTE]);
            s.fillCircle(cx + 2, cy + 9, 4, pal[P_SILHOUETTE]);
        }
    }
}

// Custom GTA Vice City / GTA VI Neon Art-Deco Digit (Screens 0, 1, 3, 4)
// Double-tube Miami neon sign with 3D drop-shadow, hot-pink & cyan tubes, and water/road reflection!
__attribute__((noinline)) void drawViceNeonDigit(TFT_eSprite &s, int digit, unsigned long nowMs,
                                                 uint8_t state, const uint16_t *pal) {
    if (digit < 0 || digit > 9) return;
    static const uint8_t segMask[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
    static const uint8_t segs[7][5] = {
        {26, 26, 83, 16, 1},  // A (top)
        {93, 32, 16, 76, 0},  // B (top-right)
        {93, 110, 16, 76, 0}, // C (bottom-right)
        {26, 176, 83, 16, 1}, // D (bottom)
        {26, 110, 16, 76, 0}, // E (bottom-left)
        {26, 32, 16, 76, 0},  // F (top-left)
        {28, 101, 79, 16, 1}  // G (middle)
    };

    uint8_t m = segMask[digit];

    // Pass 1: Classic GTA 3D Deep Drop-Shadow (offset +5, +5 in dark violet-black)
    for (int b = 0; b < 7; ++b) {
        if (!(m & (1 << b))) continue;
        int x = segs[b][0], y = segs[b][1], w = segs[b][2], h = segs[b][3];
        swFillChamfer(s, x + 5, y + 5, w, h, 4, pal[P_SILHOUETTE]);
    }

    // Pass 2: Outer Electric Cyan Neon Frame
    for (int b = 0; b < 7; ++b) {
        if (!(m & (1 << b))) continue;
        int x = segs[b][0], y = segs[b][1], w = segs[b][2], h = segs[b][3];
        swFillChamfer(s, x - 2, y - 2, w + 4, h + 4, 5, pal[state == 2 ? P_SKY_GOLD : P_NEON_CYAN]);
    }

    // Pass 3: GTA VI Gradient Fill (Hot Pink upper -> Sunset Coral/Gold lower) + White Neon Glass Core
    for (int b = 0; b < 7; ++b) {
        if (!(m & (1 << b))) continue;
        int x = segs[b][0], y = segs[b][1], w = segs[b][2], h = segs[b][3], horiz = segs[b][4];
        uint16_t bodyCol = pal[(y < 95) ? P_NEON_PINK : (y < 140 ? P_SKY_CORAL : P_SKY_GOLD)];
        swFillChamfer(s, x, y, w, h, 4, bodyCol);

        // Inner white-hot & pink neon glass tube core
        if (horiz) {
            s.drawFastHLine(x + 5, y + 5, w - 10, pal[P_WHITE]);
            s.drawFastHLine(x + 5, y + 6, w - 10, pal[P_NEON_PINK_L]);
            s.drawFastHLine(x + 6, y + h - 5, w - 12, pal[P_NEON_CYAN]);
        } else {
            s.drawFastVLine(x + 5, y + 5, h - 10, pal[P_WHITE]);
            s.drawFastVLine(x + 6, y + 5, h - 10, pal[P_NEON_PINK_L]);
            s.drawFastVLine(x + w - 5, y + 6, h - 12, pal[P_NEON_CYAN]);
        }
    }

    // Pass 4: Neon reflection on the wet Ocean Drive asphalt below the digit (y = 200..232)
    int shimmer = (int)((nowMs / 60u) & 3) - 1;
    if (m & (1 << 4)) {
        s.fillRect(28 + shimmer, 202, 12, 22, pal[P_NEON_PINK]);
        s.drawFastVLine(34 + shimmer, 204, 18, pal[P_NEON_CYAN]);
    }
    if (m & (1 << 3)) {
        s.fillRect(36, 206 + (shimmer & 1), 62, 8, pal[P_NEON_PINK]);
        s.drawFastHLine(42, 209, 50, pal[P_WHITE]);
    }
    if (m & (1 << 2)) {
        s.fillRect(95 + shimmer, 202, 12, 22, pal[P_NEON_CYAN]);
        s.drawFastVLine(101 + shimmer, 204, 18, pal[P_NEON_PINK]);
    }
}

// Screen 2 (Center): GTA VI "VI / VICE" Emblem + 80s Supercar (Cheetah/Countach) + Florida Date Plate
__attribute__((noinline)) void drawViceCenterScreen(TFT_eSprite &s, struct tm &now, suseconds_t subsec,
                                                    unsigned long nowMs, uint8_t state, const uint16_t *pal) {
    // 1. TOP (y = 8..68): Iconic "VI" Roman Numeral Emblem + Neon "VICE" Overlay
    // Heavy black drop shadow for "VI"
    // 'V' left leg, right leg
    for (int t = 0; t < 11; ++t) {
        s.drawLine(28 + t + 3, 12 + 3, 44 + t / 2 + 3, 58 + 3, pal[P_SILHOUETTE]);
        s.drawLine(60 - t / 2 + 3, 12 + 3, 44 + t / 2 + 3, 58 + 3, pal[P_SILHOUETTE]);
    }
    s.fillRect(72 + 3, 12 + 3, 14, 47, pal[P_SILHOUETTE]);

    // Gradient-filled "V" and "I" (Pink -> Coral -> Gold) with White/Cyan neon border
    for (int row = 0; row < 46; ++row) {
        int y = 12 + row;
        uint16_t gradCol = pal[(row < 16) ? P_NEON_PINK : (row < 32 ? P_SKY_CORAL : P_SKY_GOLD)];
        int lx0 = 28 + (row * 16) / 46;
        int lx1 = lx0 + 10;
        int rx1 = 64 - (row * 14) / 46;
        int rx0 = rx1 - 9;
        s.drawFastHLine(lx0, y, lx1 - lx0, gradCol);
        s.drawFastHLine(rx0, y, rx1 - rx0, gradCol);
        // 'I' bar
        s.drawFastHLine(72, y, 13, gradCol);
    }
    // Crisp white & cyan neon outline on "VI"
    s.drawLine(27, 12, 43, 58, pal[P_WHITE]);
    s.drawLine(65, 12, 50, 58, pal[P_NEON_CYAN]);
    s.drawRect(71, 12, 15, 47, pal[P_WHITE]);

    // Neon cyan "VICE" banner across the lower part of "VI"
    swFillChamfer(s, 22, 44, 72, 15, 3, pal[P_SILHOUETTE]);
    s.drawRect(23, 45, 70, 13, pal[P_NEON_PINK]);
    swDrawTxt(s, state == 2 ? "TURBO!" : "VICE CITY", 58, 48, 1, 1, TC_DATUM, pal[P_NEON_CYAN]);

    // Pulsing Colon Indicators as Neon Flamingo/Star Nodes on right side of emblem (x = 104)
    bool pulseOn = (subsec < 500000L);
    for (int j = 0; j < 2; ++j) {
        int cyDot = 20 + j * 22;
        swFillChamfer(s, 101, cyDot, 12, 12, 3, pal[pulseOn ? P_NEON_PINK : P_SILHOUETTE]);
        swFillChamfer(s, 103, cyDot + 2, 8, 8, 2, pal[pulseOn ? P_NEON_CYAN : P_NEON_PINK]);
        if (pulseOn) s.fillRect(105, cyDot + 4, 4, 4, pal[P_WHITE]);
    }

    // 2. CENTER/LOWER (y = 128..194): 80s White/Neon Supercar (Countach / Testarossa) front view on Ocean Drive!
    int bounce = ((nowMs / 110u) & 1);
    const int cx = 67, cy = 158 + bounce;

    // Wide low-slung aerodynamic wedge body
    // Tires
    s.fillRect(cx - 42, cy + 12, 13, 14, pal[P_SILHOUETTE]);
    s.fillRect(cx + 29, cy + 12, 13, 14, pal[P_SILHOUETTE]);
    // Cabin & Sunset-reflecting windshield
    s.fillTriangle(cx - 26, cy - 2, cx - 18, cy - 16, cx + 18, cy - 2, pal[P_SILHOUETTE]);
    s.fillTriangle(cx + 26, cy - 2, cx + 18, cy - 16, cx - 18, cy - 2, pal[P_SILHOUETTE]);
    s.fillRect(cx - 18, cy - 16, 36, 14, pal[P_SKY_MAGENTA]);
    s.drawLine(cx - 16, cy - 14, cx - 6, cy - 4, pal[P_NEON_CYAN]); // Windshield glare
    s.drawLine(cx - 12, cy - 14, cx - 2, cy - 4, pal[P_WHITE]);
    s.drawFastHLine(cx - 18, cy - 16, 36, pal[P_CHROME]);

    // Front hood & wedge fenders (White & Hot Pink Vice styling)
    swFillChamfer(s, cx - 40, cy - 2, 81, 18, 4, pal[P_CHROME]);
    s.fillRect(cx - 38, cy + 6, 77, 9, pal[P_NEON_PINK]);
    s.drawFastHLine(cx - 38, cy + 5, 77, pal[P_WHITE]);
    // Front grill & neon underglow!
    s.fillRect(cx - 22, cy + 8, 45, 6, pal[P_SILHOUETTE]);
    for (int gx = cx - 18; gx <= cx + 18; gx += 6) {
        s.drawFastVLine(gx, cy + 9, 4, pal[P_NEON_CYAN]);
    }
    // Bright Cyan/Pink Neon Underglow beneath the car on the wet asphalt!
    s.fillRect(cx - 36, cy + 24, 73, 4, pal[P_NEON_CYAN]);
    s.drawFastHLine(cx - 30, cy + 25, 61, pal[P_WHITE]);

    // Pop-up Headlights blazing with high-beams!
    s.fillRect(cx - 36, cy - 1, 11, 6, pal[P_SKY_GOLD]);
    s.fillRect(cx + 26, cy - 1, 11, 6, pal[P_SKY_GOLD]);
    s.fillRect(cx - 34, cy, 7, 4, pal[P_WHITE]);
    s.fillRect(cx + 28, cy, 7, 4, pal[P_WHITE]);
    // Headlight beams cutting forward across the road
    s.drawLine(cx - 31, cy + 5, cx - 48, cy + 28, pal[P_NEON_CYAN]);
    s.drawLine(cx + 31, cy + 5, cx + 48, cy + 28, pal[P_NEON_CYAN]);

    // 3. BOTTOM (y = 196..236): Florida / Vice City License Plate with Date (DD.MM)
    swFillChamfer(s, 16, 196, 103, 38, 5, pal[P_SILHOUETTE]);
    s.drawRect(18, 198, 99, 34, pal[P_NEON_CYAN]);
    s.drawFastHLine(22, 200, 91, pal[P_NEON_PINK]);
    char dateStr[6] = {
        char('0' + now.tm_mday / 10), char('0' + now.tm_mday % 10), '.',
        char('0' + now.tm_mon / 10), char('0' + now.tm_mon % 10), 0
    };
    swDrawTxt(s, dateStr, TFT_WIDTH / 2, 216, 4, 1, MC_DATUM, pal[P_SKY_GOLD]);
}

// Draw a 5-pointed GTA Wanted Star centered at (cx, cy)
__attribute__((noinline)) void drawWantedStar(TFT_eSprite &s, int cx, int cy, uint16_t fillCol, uint16_t borderCol) {
    s.fillTriangle(cx, cy - 7, cx - 3, cy + 2, cx + 3, cy + 2, fillCol);
    s.fillTriangle(cx - 7, cy - 2, cx + 7, cy - 2, cx, cy + 3, fillCol);
    s.fillTriangle(cx - 3, cy, cx - 5, cy + 6, cx, cy + 3, fillCol);
    s.fillTriangle(cx + 3, cy, cx + 5, cy + 6, cx, cy + 3, fillCol);
    s.drawPixel(cx, cy - 7, borderCol);
}

// Screen 5 (Rightmost): Classic GTA HUD (5 Wanted Stars + Cash $ + Health/Armor + Radar Weather + FLASH FM)
__attribute__((noinline)) void drawViceHudScreen(TFT_eSprite &s, WeatherService &weather,
                                                 struct tm &now, unsigned long nowMs,
                                                 uint8_t state, const uint16_t *pal) {
    // 1. TOP (y = 8..30): Classic GTA 5-Star Wanted Level Row!
    // Lit stars = 1..5 across the 60s minute (all 5 flash rapidly during 57-02s chase!)
    swFillChamfer(s, 6, 6, 123, 24, 4, pal[P_SILHOUETTE]);
    s.drawRect(7, 7, 121, 22, pal[state >= 1 ? P_POLICE_RED : P_NEON_PINK]);

    int litStars = (state >= 1) ? 5 : (1 + (now.tm_sec * 4) / 59);
    bool flashHide = (state >= 1) && (((nowMs / 120u) & 1) == 0);
    for (int st = 0; st < 5; ++st) {
        int sx = 21 + st * 23;
        bool active = (st < litStars) && !flashHide;
        uint16_t starCol = pal[active ? P_SKY_GOLD : P_SEA_DARK];
        uint16_t rimCol  = pal[active ? P_WHITE : P_NEON_CYAN_D];
        drawWantedStar(s, sx, 18, starCol, rimCol);
    }

    // 2. UPPER-MID (y = 34..78): GTA Cash Counter ($00251159) + Health (Pink) & Armor (Cyan) Bars!
    swFillChamfer(s, 6, 33, 123, 44, 4, pal[P_SILHOUETTE]);

    char unit = (WeatherService::getUnits().value == "imperial") ? 'F' : 'C';
    float vals[3] = {weather.getNowTemp(), weather.getHigh(5), weather.getLow(5)};
    int ints[3];
    for (int k = 0; k < 3; ++k) ints[k] = (int)(vals[k] + (vals[k] >= 0.0f ? 0.5f : -0.5f));

    // Classic 8-digit GTA Money readout encoding live seconds & vice cash ($008600SS)
    char cashBuf[16];
    snprintf(cashBuf, sizeof(cashBuf), "$008619%02d", now.tm_sec);
    swDrawTxt(s, cashBuf, 67, 45, 2, 1, MC_DATUM, pal[P_GTA_GREEN]);

    // Health Bar (Hot Pink) & Body Armor Bar (Electric Cyan)
    s.drawRect(12, 59, 52, 10, pal[P_WHITE]);
    s.fillRect(14, 61, 48, 6, pal[P_NEON_PINK]);
    s.drawRect(70, 59, 52, 10, pal[P_WHITE]);
    int armorW = (now.tm_sec * 48) / 59;
    if (armorW < 4) armorW = 4;
    s.fillRect(72, 61, armorW, 6, pal[P_NEON_CYAN]);

    // 3. CENTER (y = 82..172): GTA Mini-Radar Circle + Weather & Temperature Readout
    swFillChamfer(s, 6, 81, 123, 92, 6, pal[P_SILHOUETTE]);
    s.drawRect(8, 83, 119, 88, pal[P_NEON_CYAN]);

    // Left: Circular GTA Radar Minimap with pink/cyan ring & North 'N' marker
    const int rcx = 36, rcy = 127, rr = 24;
    s.fillCircle(rcx, rcy, rr, pal[P_SEA_DARK]);
    s.drawCircle(rcx, rcy, rr, pal[P_NEON_PINK]);
    s.drawCircle(rcx, rcy, rr - 1, pal[P_NEON_CYAN]);
    // Radar streets & Vice Beach coastline inside minimap
    s.fillRect(rcx - 16, rcy - 6, 20, 14, pal[P_SKY_VIOLET]);
    s.drawFastHLine(rcx - 21, rcy + 2, 42, pal[P_NEON_CYAN]);
    s.drawFastVLine(rcx - 4, rcy - 21, 42, pal[P_NEON_PINK]);
    // Player white arrow blip in center of radar + 'N' badge at top
    s.fillTriangle(rcx, rcy - 5, rcx - 4, rcy + 4, rcx + 4, rcy + 4, pal[P_WHITE]);
    s.fillCircle(rcx, rcy - rr, 5, pal[P_SILHOUETTE]);
    swDrawTxt(s, "N", rcx, rcy - rr - 3, 1, 1, TC_DATUM, pal[P_WHITE]);

    // Right of Radar: Live Temperature + HI / LO in Vice Neon Colors
    char tempBuf[16];
    snprintf(tempBuf, sizeof(tempBuf), "%d%c", ints[0], unit);
    swDrawTxt(s, tempBuf, 94, 108, 4, 1, MC_DATUM, pal[P_WHITE]);

    snprintf(tempBuf, sizeof(tempBuf), "MAX %d%c", ints[1], unit);
    swDrawTxt(s, tempBuf, 94, 136, 2, 1, MC_DATUM, pal[P_SKY_GOLD]);

    snprintf(tempBuf, sizeof(tempBuf), "MIN %d%c", ints[2], unit);
    swDrawTxt(s, tempBuf, 94, 154, 2, 1, MC_DATUM, pal[P_NEON_CYAN]);

    // 4. BOTTOM (y = 177..236): "FLASH FM" Car Radio Deck + Neon Spectrum Analyzer Bars!
    swFillChamfer(s, 6, 177, 123, 57, 5, pal[P_SILHOUETTE]);
    s.drawRect(8, 179, 119, 53, pal[P_NEON_PINK]);
    swDrawTxt(s, state == 1 ? "VCPD DISPATCH" : (state == 2 ? "FEVER 105!" : "FLASH FM 105"),
              67, 184, 1, 1, TC_DATUM, pal[P_SKY_GOLD]);

    // 10 animated synthwave equalizer bars (x = 15..115, y = 198..228)
    for (int bar = 0; bar < 10; ++bar) {
        int bx = 15 + bar * 10;
        int bh = 5 + ((int)((nowMs / 45u + bar * 17) * 7) % 22);
        if (state >= 1) bh = 14 + ((bar + (int)(nowMs / 35u)) % 14);
        int by = 228 - bh;
        uint16_t bCol = pal[(bar & 1) ? P_NEON_CYAN : P_NEON_PINK];
        s.fillRect(bx, by, 7, bh, bCol);
        s.drawFastHLine(bx, by, 7, pal[P_WHITE]);
    }
}

} // namespace

void MatrixClock::renderViceCity(IPSClock &clock, TimeSync &time, WeatherService &weather) {
    unsigned long nowMs = millis();
    if (nowMs - lastFrame < 45) return;
    lastFrame = nowMs;
    ++frame;

    struct tm now;
    suseconds_t subsecond;
    time.getLocalTime(&now, &subsecond);

    uint16_t stateMs = 0;
    if (now.tm_sec <= 2) {
        viceCityState = 2;
        stateMs = (uint16_t)(now.tm_sec * 1000u + (uint32_t)(subsecond / 1000));
    } else if (now.tm_sec >= 57) {
        viceCityState = 1;
        stateMs = (uint16_t)((now.tm_sec - 57) * 1000u + (uint32_t)(subsecond / 1000));
    } else {
        viceCityState = 0;
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
    swDimPalette(VICE_RAW, pal, P_COUNT);

    for (int i = 0; i < 6; ++i) {
        drawViceBackdrop(sprite, i, nowMs, viceCityState, stateMs, pal);

        if (i == 2) {
            drawViceCenterScreen(sprite, now, subsecond, nowMs, viceCityState, pal);
        } else if (i == 5) {
            drawViceHudScreen(sprite, weather, now, nowMs, viceCityState, pal);
        } else {
            bool blankHour = (i == 0 && hour < 10 && !IPSClock::getLeadingZero().value);
            if (!blankHour) {
                drawViceNeonDigit(sprite, digits[i], nowMs, viceCityState, pal);
            }
        }

        tfts->chip_select.setDigit(SW_SCREENS[i]);
        sprite.pushSprite(0, 0);
    }
    tfts->release();
}
