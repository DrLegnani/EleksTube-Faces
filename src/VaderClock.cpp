#include "MatrixClock.h"
#include "StarWarsHud.h"
#include "IPSClock.h"

// Shared with Backlights: 0 = Vader dark-side breathing, 1 = deflection buildup (57-59s), 2 = Rogue One rampage slash (00-02s)
uint8_t vaderSaberState = 0;

namespace {

enum VaderPal {
    P_WHITE = 0,
    P_BLACK,
    P_BG_DEEP,      // Deep corridor void
    P_BG_SMOKE_D,   // Low red smoke shadow
    P_BG_SMOKE_M,   // Mid crimson fog
    P_BG_SMOKE_H,   // Bright red-lit steam near blade
    P_BULKHEAD,     // Dark steel corridor wall rib
    P_PANEL_DARK,   // Recessed wall panel
    P_GRILLE_DIM,   // Imperial wall light-grille (dim crimson-steel)
    P_GRILLE_LIT,   // Imperial wall light-grille (lit warm red)
    P_STEEL_D,      // Dark armor steel
    P_STEEL,        // Mid metallic steel
    P_STEEL_L,      // Specular chrome highlight
    P_ARMOR_BLACK,  // Vader obsidian armor fill
    P_ARMOR_RIM,    // Red rim-lighting on armor/cape
    P_AURA_OUTER,   // Wide lightsaber ambient glow
    P_SABER_AURA,   // Inner crimson blade halo
    P_RED_BLADE,    // Pure Sith crimson blade
    P_RED_HOT,      // High-intensity scarlet plasma
    P_PINK_CORE,    // Inner hot pinkish-white core
    P_EMBER_O,      // Deflection / spark orange
    P_CHEST_GREEN,  // Chest box green status switch
    P_CHEST_BLUE,   // Chest box blue status switch
    P_COUNT
};

const uint16_t VADER_RAW[P_COUNT] = {
    0xFFFF,                 // P_WHITE
    0x0000,                 // P_BLACK
    swC565(10, 2, 4),       // P_BG_DEEP
    swC565(28, 4, 6),       // P_BG_SMOKE_D
    swC565(58, 7, 10),      // P_BG_SMOKE_M
    swC565(105, 14, 18),    // P_BG_SMOKE_H
    swC565(22, 24, 30),     // P_BULKHEAD
    swC565(14, 15, 20),     // P_PANEL_DARK
    swC565(68, 22, 26),     // P_GRILLE_DIM
    swC565(165, 38, 42),    // P_GRILLE_LIT
    swC565(48, 52, 62),     // P_STEEL_D
    swC565(112, 120, 136),  // P_STEEL
    swC565(195, 206, 222),  // P_STEEL_L
    swC565(16, 17, 22),     // P_ARMOR_BLACK
    swC565(150, 18, 24),    // P_ARMOR_RIM
    swC565(52, 2, 6),       // P_AURA_OUTER
    swC565(115, 4, 12),     // P_SABER_AURA
    swC565(235, 14, 22),    // P_RED_BLADE
    swC565(255, 58, 64),    // P_RED_HOT
    swC565(255, 215, 222),  // P_PINK_CORE
    swC565(255, 150, 35),   // P_EMBER_O
    swC565(45, 210, 85),    // P_CHEST_GREEN
    swC565(55, 155, 255)    // P_CHEST_BLUE
};

// Breath phase 0..255 over a 4000ms cycle (matches Backlights Vader breathing)
inline uint8_t getBreathLevel(unsigned long nowMs) {
    uint16_t phase = (uint16_t)(nowMs % 4000u);
    if (phase < 1600u) return (uint8_t)((phase * 255u) / 1600u);
    if (phase < 2000u) return 255u;
    if (phase < 3500u) return (uint8_t)(255u - ((phase - 2000u) * 255u) / 1500u);
    return 0u;
}

// Draw glowing lightsaber line with outer halo, crimson plasma body, and white-hot core
__attribute__((noinline)) void drawSaberBeam(TFT_eSprite &s, int x0, int y0, int x1, int y1,
                                             const uint16_t *pal, bool verticalBias, bool intense) {
    if (verticalBias) {
        for (int dx = -5; dx <= 5; ++dx) s.drawLine(x0 + dx, y0, x1 + dx, y1, pal[P_AURA_OUTER]);
        for (int dx = -3; dx <= 3; ++dx) s.drawLine(x0 + dx, y0, x1 + dx, y1, pal[P_SABER_AURA]);
        for (int dx = -2; dx <= 2; ++dx) s.drawLine(x0 + dx, y0, x1 + dx, y1, pal[intense ? P_RED_HOT : P_RED_BLADE]);
        s.drawLine(x0 - 1, y0, x1 - 1, y1, pal[P_PINK_CORE]);
        s.drawLine(x0, y0, x1, y1, pal[P_WHITE]);
        s.drawLine(x0 + 1, y0, x1 + 1, y1, pal[P_PINK_CORE]);
    } else {
        for (int dy = -5; dy <= 5; ++dy) s.drawLine(x0, y0 + dy, x1, y1 + dy, pal[P_AURA_OUTER]);
        for (int dy = -3; dy <= 3; ++dy) s.drawLine(x0, y0 + dy, x1, y1 + dy, pal[P_SABER_AURA]);
        for (int dy = -2; dy <= 2; ++dy) s.drawLine(x0, y0 + dy, x1, y1 + dy, pal[intense ? P_RED_HOT : P_RED_BLADE]);
        s.drawLine(x0, y0 - 1, x1, y1 - 1, pal[P_PINK_CORE]);
        s.drawLine(x0, y0, x1, y1, pal[P_WHITE]);
        s.drawLine(x0, y0 + 1, x1, y1 + 1, pal[P_PINK_CORE]);
    }
}

// Panoramic Rogue One Tantive IV / Imperial Corridor backdrop across all 6 screens
__attribute__((noinline)) void drawRogueOneCorridor(TFT_eSprite &s, int screen, unsigned long nowMs,
                                                    uint8_t state, uint16_t stateMs, uint8_t breath,
                                                    const uint16_t *pal) {
    s.fillSprite(pal[state == 2 ? P_BG_SMOKE_D : P_BG_DEEP]);

    // 1. Ceiling architecture & pulsing red klaxon alarm light (y = 0..24)
    s.fillRect(0, 0, TFT_WIDTH, 16, pal[P_PANEL_DARK]);
    s.drawLine(0, 22, 18, 14, pal[P_BULKHEAD]);
    s.drawLine(134, 22, 116, 14, pal[P_BULKHEAD]);
    s.drawFastHLine(0, 15, TFT_WIDTH, pal[P_STEEL_D]);

    bool klaxonHigh = (state >= 1) ? (((nowMs / 160u) + screen) & 1) : (breath > 150);
    uint16_t klaxonBody = pal[klaxonHigh ? P_RED_BLADE : P_SABER_AURA];
    uint16_t klaxonCore = pal[klaxonHigh ? P_WHITE : P_RED_HOT];
    swFillChamfer(s, 44, 2, 47, 7, 2, klaxonBody);
    s.drawFastHLine(50, 5, 35, klaxonCore);
    if (klaxonHigh) {
        s.drawFastHLine(36, 10, 63, pal[P_AURA_OUTER]);
        s.drawFastHLine(42, 12, 51, pal[P_BG_SMOKE_M]);
    }

    // 2. Iconic Imperial Corridor bulkhead pillars & illuminated pill-grilles on left/right walls
    s.fillRect(0, 20, 14, 186, pal[P_PANEL_DARK]);
    s.fillRect(121, 20, 14, 186, pal[P_PANEL_DARK]);
    s.drawFastVLine(14, 20, 186, pal[P_BULKHEAD]);
    s.drawFastVLine(120, 20, 186, pal[P_BULKHEAD]);
    s.drawFastVLine(15, 24, 178, pal[klaxonHigh ? P_GRILLE_DIM : P_STEEL_D]);
    s.drawFastVLine(119, 24, 178, pal[klaxonHigh ? P_GRILLE_DIM : P_STEEL_D]);

    // Vertical array of Imperial glowing wall slots (pill-shaped light panels)
    for (int gy = 30; gy <= 174; gy += 24) {
        bool nearBlade = (gy >= 110) || (state >= 1);
        uint16_t gCol = pal[nearBlade ? P_GRILLE_LIT : P_GRILLE_DIM];
        uint16_t gCore = pal[nearBlade ? P_RED_HOT : P_GRILLE_LIT];
        // Left wall grille
        swFillChamfer(s, 3, gy, 8, 16, 2, gCol);
        s.drawFastVLine(6, gy + 3, 10, gCore);
        // Right wall grille
        swFillChamfer(s, 124, gy, 8, 16, 2, gCol);
        s.drawFastVLine(127, gy + 3, 10, gCore);
    }

    // Recessed blast-door structural ribs in background
    for (int ry = 56; ry <= 168; ry += 56) {
        s.drawFastHLine(16, ry, 103, pal[P_BULKHEAD]);
        s.drawFastHLine(24, ry + 2, 87, pal[P_BG_SMOKE_D]);
    }

    // 3. Volumetric red-lit corridor smoke / steam clouds (drifting horizontally at bottom)
    int smokeShift = (int)((nowMs / 55u) + screen * 37);
    for (int y = 148; y < 208; y += 2) {
        int wave = ((y * 7 + smokeShift) % 29) - 14;
        uint16_t sCol = (y > 190) ? pal[P_BG_SMOKE_H]
                      : (y > 170) ? pal[P_BG_SMOKE_M]
                                  : pal[P_BG_SMOKE_D];
        int xStart = 16 + ((wave > 0) ? wave : 0);
        int wLine = 103 - abs(wave);
        s.drawFastHLine(xStart, y, wLine, sCol);
    }

    // 4. Reflective metallic corridor floor grating (y = 206..239)
    s.fillRect(0, 206, TFT_WIDTH, 34, pal[P_PANEL_DARK]);
    s.drawFastHLine(0, 206, TFT_WIDTH, pal[P_RED_BLADE]);
    s.drawFastHLine(0, 207, TFT_WIDTH, pal[P_SABER_AURA]);

    // Perspective floor grid lines
    for (int fy = 212; fy < 240; fy += 6) {
        uint16_t fCol = (fy < 224) ? pal[P_GRILLE_DIM] : pal[P_BULKHEAD];
        s.drawFastHLine(0, fy, TFT_WIDTH, fCol);
    }
    for (int fx = 12; fx < TFT_WIDTH; fx += 22) {
        int spread = (fx - 67) / 3;
        s.drawLine(fx, 207, fx + spread, 239, pal[P_BULKHEAD]);
    }

    // Continuous low crimson plasma reflection across the corridor floor & floating sparks
    int humWobble = (int)((nowMs / 40u + screen * 19) % 7) - 3;
    s.drawFastHLine(18, 210 + (humWobble & 1), 99, pal[P_SABER_AURA]);
    s.drawFastHLine(30, 215, 75, pal[P_BG_SMOKE_H]);

    // Rising Force embers & sparks in the smoky corridor
    for (int k = 0; k < 18; ++k) {
        uint32_t h = (uint32_t)(screen + 3) * 2654435761u ^ (uint32_t)(k + 11) * 2246822519u;
        int spd = 24 + (k % 4) * 12 + (state >= 1 ? 22 : 0);
        int ey = 235 - (int)(((nowMs * spd) / 1000u + (h & 0xFF)) % 210u);
        int sway = (int)(((nowMs / 150u + k) & 7)) - 3;
        int ex = 16 + ((int)((h >> 8) % 103) + sway + 103) % 103;
        uint16_t col = pal[(k % 5 == 0) ? P_WHITE : (k & 1) ? P_EMBER_O : P_RED_HOT];
        s.drawPixel(ex, ey, col);
        if ((k & 3) == 0) s.drawPixel(ex + 1, ey, pal[P_RED_BLADE]);
    }

    // 5. Sec 57-59 (State 1): Rebel blaster fire flying down the corridor & deflected by Vader!
    if (state == 1) {
        int boltPhase = (int)((stateMs * 3u) / 1000u); // 3 volley waves
        int boltProg = (int)((stateMs * 3u) % 1000u);  // 0..999ms per volley
        // Bolt travels from right (screen 5) to center (screen 2) where Vader deflects it!
        int globalX = 850 - (boltProg * 580) / 1000;
        int scrX0 = screen * 150;
        int bx = globalX - scrX0;
        int by = 86 + (boltPhase % 3) * 32;
        if (bx >= -20 && bx < TFT_WIDTH + 20) {
            drawSaberBeam(s, bx, by, bx + 22, by + 3, pal, false, true);
        }
        // Deflection flash & wall scorch sparks when bolt reaches screen 2..3
        if (screen == 2 || screen == 3) {
            for (int sp = 0; sp < 8; ++sp) {
                int sx = 67 + ((int)((nowMs / 18 + sp * 29) % 70) - 35);
                int sy = 75 + ((int)((nowMs / 23 + sp * 19) % 80) - 40);
                s.drawLine(67, 92, sx, sy, pal[(sp & 1) ? P_EMBER_O : P_WHITE]);
            }
        }
    }

    // 6. Sec 00-02 (State 2): Full Rogue One Crimson X-Slash tearing across all 6 screens
    if (state == 2) {
        int gHead = (int)((stateMs * 1180u) / 3000u) - 40;
        int gTail = gHead - 420;
        int scrX0 = screen * 150;
        int x0 = gTail - scrX0;
        int x1 = gHead - scrX0;
        if (x1 >= 0 && x0 < TFT_WIDTH) {
            if (x0 < 0) x0 = 0;
            if (x1 >= TFT_WIDTH) x1 = TFT_WIDTH - 1;
            int yA0 = 64 + (scrX0 + x0) / 9;
            int yA1 = 64 + (scrX0 + x1) / 9;
            int yB0 = 176 - (scrX0 + x0) / 9;
            int yB1 = 176 - (scrX0 + x1) / 9;
            drawSaberBeam(s, x0, yA0, x1, yA1, pal, false, true);
            drawSaberBeam(s, x0, yB0, x1, yB1, pal, false, true);
        }
    }
}

// Custom Darth Vader Lightsaber-Segment Digit (Screens 0, 1, 3, 4)
// Each active segment is rendered as an ignited Sith lightsaber blade with metallic emitter caps
// plus a crimson floor reflection on the corridor grating below!
__attribute__((noinline)) void drawVaderSaberDigit(TFT_eSprite &s, int digit, unsigned long nowMs,
                                                   uint8_t state, const uint16_t *pal) {
    if (digit < 0 || digit > 9) return;
    static const uint8_t segMask[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
    // Segment geometry {x, y, w, h, horiz(1)/vert(0)}
    static const uint8_t segs[7][5] = {
        {28, 30, 79, 15, 1},  // A (top)
        {94, 36, 15, 78, 0},  // B (top-right)
        {94, 116, 15, 78, 0}, // C (bottom-right)
        {28, 184, 79, 15, 1}, // D (bottom)
        {26, 116, 15, 78, 0}, // E (bottom-left)
        {26, 36, 15, 78, 0},  // F (top-left)
        {30, 107, 75, 15, 1}  // G (middle)
    };

    uint8_t m = segMask[digit];
    bool shimmer = ((nowMs / 65u) & 1) != 0;
    uint16_t bladeCol = pal[(state == 2 || shimmer) ? P_RED_HOT : P_RED_BLADE];
    uint16_t coreCol  = pal[(state == 2) ? P_WHITE : (shimmer ? P_WHITE : P_PINK_CORE)];

    // Pass 1: Wide crimson ambient aura behind active segments
    for (int b = 0; b < 7; ++b) {
        if (!(m & (1 << b))) continue;
        int x = segs[b][0], y = segs[b][1], w = segs[b][2], h = segs[b][3];
        swFillChamfer(s, x - 4, y - 4, w + 8, h + 8, 6, pal[P_AURA_OUTER]);
        swFillChamfer(s, x - 2, y - 2, w + 4, h + 4, 5, pal[P_SABER_AURA]);
    }

    // Pass 2: Plasma blade body, white-hot kyber core, and metallic Sith emitter caps
    for (int b = 0; b < 7; ++b) {
        if (!(m & (1 << b))) continue;
        int x = segs[b][0], y = segs[b][1], w = segs[b][2], h = segs[b][3], horiz = segs[b][4];
        swFillChamfer(s, x, y, w, h, 4, bladeCol);

        if (horiz) {
            // Ignited white-hot horizontal core
            swFillChamfer(s, x + 6, y + 4, w - 12, h - 8, 2, pal[P_PINK_CORE]);
            s.drawFastHLine(x + 8, y + h / 2, w - 16, coreCol);
            // Chrome/obsidian emitter rings at segment ends
            s.fillRect(x, y + 2, 5, h - 4, pal[P_STEEL_D]);
            s.drawFastVLine(x + 2, y + 2, h - 4, pal[P_STEEL_L]);
            s.fillRect(x + w - 5, y + 2, 5, h - 4, pal[P_STEEL_D]);
            s.drawFastVLine(x + w - 3, y + 2, h - 4, pal[P_STEEL_L]);
        } else {
            // Ignited white-hot vertical core
            swFillChamfer(s, x + 4, y + 6, w - 8, h - 12, 2, pal[P_PINK_CORE]);
            s.drawFastVLine(x + w / 2, y + 8, h - 16, coreCol);
            // Chrome/obsidian emitter rings at top & bottom of vertical blade
            s.fillRect(x + 2, y, w - 4, 5, pal[P_STEEL_D]);
            s.drawFastHLine(x + 2, y + 2, w - 4, pal[P_STEEL_L]);
            s.fillRect(x + 2, y + h - 5, w - 4, 5, pal[P_STEEL_D]);
            s.drawFastHLine(x + 2, y + h - 3, w - 4, pal[P_STEEL_L]);
        }
    }

    // Pass 3: Semi-transparent corridor smoke wisps drifting IN FRONT of lower digit (y = 175..198)
    int wispX = 24 + (int)((nowMs / 45u) % 52);
    s.drawFastHLine(wispX, 178, 36, pal[P_BG_SMOKE_H]);
    s.drawFastHLine(wispX + 8, 189, 42, pal[P_SABER_AURA]);

    // Pass 4: Specular crimson reflection of active segments on the wet corridor floor (y = 210..235)
    if (m & (1 << 4)) { // Bottom-left vertical segment reflects on left floor
        s.fillRect(26, 210, 15, 22, pal[P_AURA_OUTER]);
        s.fillRect(30, 212, 7, 16, pal[P_SABER_AURA]);
        s.drawFastVLine(33, 213, 12, pal[P_RED_BLADE]);
    }
    if (m & (1 << 3)) { // Bottom horizontal segment reflects across center floor
        s.fillRect(36, 211, 63, 12, pal[P_SABER_AURA]);
        s.drawFastHLine(42, 215, 51, pal[P_RED_BLADE]);
        s.drawFastHLine(48, 217, 39, pal[P_PINK_CORE]);
    }
    if (m & (1 << 2)) { // Bottom-right vertical segment reflects on right floor
        s.fillRect(94, 210, 15, 22, pal[P_AURA_OUTER]);
        s.fillRect(98, 212, 7, 16, pal[P_SABER_AURA]);
        s.drawFastVLine(101, 213, 12, pal[P_RED_BLADE]);
    }
}

// Screen 2 (Center): Half-Body Darth Vader in the Rogue One Corridor + Ignited Lightsaber + Chest Box + Date
__attribute__((noinline)) void drawVaderRogueOneCenter(TFT_eSprite &s, struct tm &now, suseconds_t subsec,
                                                       unsigned long nowMs, uint8_t state, uint8_t breath,
                                                       const uint16_t *pal) {
    const int cx = 56; // Shifted slightly left so his ignited lightsaber blazes up the right side!
    const int hy = 68; // Helmet center Y

    // 1. Red radial backlight halo behind Vader's silhouette in the corridor fog
    s.fillCircle(cx + 6, hy + 28, 52, pal[P_AURA_OUTER]);
    s.fillCircle(cx + 10, hy + 24, 38, pal[P_BG_SMOKE_H]);

    // 2. Flowing Sith Cape & Segmented Durasteel Shoulder Armor (y = 94..198)
    s.fillTriangle(cx - 48, 198, cx - 28, 98, cx + 44, 198, pal[P_ARMOR_BLACK]);
    s.fillTriangle(cx + 48, 198, cx + 28, 98, cx - 20, 198, pal[P_ARMOR_BLACK]);
    s.fillRect(cx - 32, 98, 64, 100, pal[P_ARMOR_BLACK]);

    // Pauldron armor plates & red lightsaber rim-lighting on right shoulder/cape
    s.drawLine(cx - 28, 98, cx - 47, 196, pal[P_STEEL_D]);
    s.drawLine(cx + 28, 98, cx + 47, 196, pal[P_RED_HOT]);
    s.drawLine(cx + 27, 98, cx + 46, 196, pal[P_RED_BLADE]);
    // Shoulder bell segments
    s.drawLine(cx - 26, 102, cx - 10, 114, pal[P_STEEL]);
    s.drawLine(cx + 26, 102, cx + 10, 114, pal[P_ARMOR_RIM]);
    // Vertical quilted leather torso pleats
    for (int px = -22; px <= 22; px += 11) {
        s.drawFastVLine(cx + px, 114, 78, pal[P_BULKHEAD]);
    }

    // 3. Iconic Darth Vader Helmet & Mask (large, aggressive close-up)
    // Flared dome & cheek wings
    s.fillCircle(cx, hy - 14, 24, pal[P_ARMOR_BLACK]);
    s.fillTriangle(cx - 23, hy - 12, cx - 39, hy + 28, cx - 6, hy + 28, pal[P_ARMOR_BLACK]);
    s.fillTriangle(cx + 23, hy - 12, cx + 39, hy + 28, cx + 6, hy + 28, pal[P_ARMOR_BLACK]);
    s.fillRect(cx - 23, hy - 12, 47, 40, pal[P_ARMOR_BLACK]);

    // Helmet rim highlights: left side cold steel, right side blazing crimson from the saber!
    s.drawCircle(cx, hy - 14, 24, pal[P_STEEL]);
    s.drawLine(cx - 23, hy - 12, cx - 39, hy + 28, pal[P_STEEL_L]);
    s.drawLine(cx + 23, hy - 12, cx + 39, hy + 28, pal[P_RED_HOT]);
    s.drawLine(cx + 22, hy - 12, cx + 38, hy + 28, pal[P_RED_BLADE]);
    s.drawLine(cx - 39, hy + 28, cx - 14, hy + 31, pal[P_STEEL]);
    s.drawLine(cx + 39, hy + 28, cx + 14, hy + 31, pal[P_RED_BLADE]);

    // Center cranial crest & V-brow
    s.fillRect(cx - 3, hy - 38, 6, 24, pal[P_STEEL_D]);
    s.drawFastVLine(cx - 1, hy - 37, 22, pal[P_STEEL_L]);
    s.drawFastVLine(cx + 2, hy - 37, 22, pal[P_RED_BLADE]);
    s.drawLine(cx - 21, hy - 12, cx, hy - 4, pal[P_STEEL_L]);
    s.drawLine(cx + 21, hy - 12, cx, hy - 4, pal[P_RED_HOT]);

    // Angular Sith Eye Lenses (glowing ruby-red from the corridor lightsaber reflection)
    s.fillTriangle(cx - 18, hy - 9, cx - 4, hy - 4, cx - 14, hy + 1, pal[P_SABER_AURA]);
    s.fillTriangle(cx + 18, hy - 9, cx + 4, hy - 4, cx + 14, hy + 1, pal[P_RED_BLADE]);
    s.drawLine(cx - 16, hy - 7, cx - 7, hy - 4, pal[P_RED_HOT]);
    s.drawLine(cx + 7, hy - 4, cx + 16, hy - 7, pal[P_WHITE]);

    // Respirator snout & silver vocoder grate
    s.fillTriangle(cx - 13, hy + 23, cx, hy + 3, cx + 13, hy + 23, pal[P_BLACK]);
    s.drawLine(cx - 13, hy + 23, cx, hy + 3, pal[P_STEEL_L]);
    s.drawLine(cx, hy + 3, cx + 13, hy + 23, pal[P_RED_HOT]);
    s.drawFastHLine(cx - 13, hy + 23, 27, pal[P_STEEL]);
    for (int vx = -6; vx <= 6; vx += 3) {
        int vTop = hy + 6 + abs(vx) * 2;
        s.drawFastVLine(cx + vx, vTop, (hy + 22) - vTop, pal[(vx > 0) ? P_RED_BLADE : P_STEEL_L]);
    }
    s.fillCircle(cx - 13, hy + 24, 3, pal[P_STEEL]);
    s.fillCircle(cx + 13, hy + 24, 3, pal[P_RED_HOT]);

    // 4. Authentic Cybernetic Chest Control Box (cx - 18..cx + 18, y = 122..162)
    const int bx = cx - 18, by = 122, bw = 36, bh = 40;
    s.fillRect(bx, by, bw, bh, pal[P_PANEL_DARK]);
    s.drawRect(bx, by, bw, bh, pal[P_STEEL]);
    s.drawRect(bx + 2, by + 2, bw - 4, bh - 4, pal[P_BULKHEAD]);

    // Left rocker switches (Red & Blue/Green)
    s.fillRect(bx + 5, by + 6, 8, 6, pal[P_RED_HOT]);
    s.fillRect(bx + 5, by + 15, 8, 6, pal[P_CHEST_BLUE]);
    s.fillRect(bx + 5, by + 24, 8, 6, pal[P_CHEST_GREEN]);
    s.fillRect(bx + 5, by + 32, 8, 4, pal[P_STEEL_L]);

    // Right 3 vertical coin-slot breathing LEDs (animate with 4s breath!)
    for (int slot = 0; slot < 3; ++slot) {
        bool slotLit = (breath > (uint8_t)(50 + slot * 65)) || (state >= 1);
        uint16_t slotCol = pal[slotLit ? (slot == 2 ? P_WHITE : P_RED_HOT) : P_GRILLE_DIM];
        s.fillRect(bx + 18 + slot * 5, by + 7, 3, 18, slotCol);
    }
    // Lower chest box silver toggles
    for (int tx = 0; tx < 3; ++tx) {
        s.fillRect(bx + 17 + tx * 5, by + 29, 3, 6, pal[P_STEEL_L]);
    }

    // 5. Outstretched Force Hand / Armored Gauntlet & IGNITED SITH LIGHTSABER!
    // Hilt held at lower-right (x = 92..106, y = 154..192), blade blazing up to (116, 20)
    const int hx = 96, hyHilt = 160;
    // Ignited blade (drawn before hilt emitter shroud so emitter overlaps cleanly)
    int tipX = 114 + (int)((nowMs / 60u) % 3) - 1;
    drawSaberBeam(s, hx + 3, hyHilt - 2, tipX, 20, pal, true, state >= 1);

    // Plasma crackle sparks dancing along the ignited blade
    for (int sp = 0; sp < 5; ++sp) {
        int frac = (int)((nowMs / 18u + sp * 31) % 135);
        int sx = hx + 3 + ((tipX - (hx + 3)) * frac) / 135 + ((sp & 1) ? 4 : -4);
        int sy = (hyHilt - 2) - frac;
        s.drawPixel(sx, sy, pal[(sp & 1) ? P_WHITE : P_EMBER_O]);
    }

    // Detailed Sith Lightsaber Hilt (chrome cylinder, black grip ribs, angled emitter shroud)
    s.fillRect(hx - 2, hyHilt, 11, 30, pal[P_STEEL]);
    s.drawFastVLine(hx + 1, hyHilt, 30, pal[P_STEEL_L]);
    s.fillRect(hx - 3, hyHilt - 4, 13, 6, pal[P_ARMOR_BLACK]); // Angled black emitter shroud
    s.drawFastHLine(hx - 3, hyHilt - 2, 13, pal[P_RED_HOT]);
    for (int gy = hyHilt + 10; gy < hyHilt + 28; gy += 4) {
        s.fillRect(hx - 2, gy, 11, 2, pal[P_ARMOR_BLACK]); // Black T-track grip ribs
    }
    // Armored glove gripping the hilt
    swFillChamfer(s, hx - 6, hyHilt + 8, 17, 13, 3, pal[P_ARMOR_BLACK]);
    s.drawFastHLine(hx - 5, hyHilt + 9, 15, pal[P_ARMOR_RIM]);

    // 6. Pulsing Colon Indicators integrated as Left Bulkhead Plasma Nodes
    bool pulseOn = (subsec < 500000L);
    for (int j = 0; j < 2; ++j) {
        int py = 66 + j * 34;
        swFillChamfer(s, 6, py, 11, 13, 3, pal[pulseOn ? P_RED_BLADE : P_SABER_AURA]);
        swFillChamfer(s, 8, py + 3, 7, 7, 2, pal[pulseOn ? P_WHITE : P_RED_HOT]);
    }

    // 7. Bottom Imperial Corridor Blast-Door Date Console (y = 198..236)
    swFillChamfer(s, 14, 198, 107, 36, 5, pal[P_PANEL_DARK]);
    s.drawFastHLine(20, 198, 95, pal[P_RED_BLADE]);
    s.drawFastHLine(24, 233, 87, pal[P_STEEL_D]);
    char dateStr[6] = {
        char('0' + now.tm_mday / 10), char('0' + now.tm_mday % 10), '.',
        char('0' + now.tm_mon / 10), char('0' + now.tm_mon % 10), 0
    };
    swDrawTxt(s, dateStr, TFT_WIDTH / 2, 217, 4, 1, MC_DATUM, pal[P_WHITE]);
}

// Screen 5 (Rightmost): Rogue One Tactical Telemetry + Breathing Waveform + Vertical 60s Lightsaber
__attribute__((noinline)) void drawVaderCorridorTelemetry(TFT_eSprite &s, WeatherService &weather,
                                                          struct tm &now, unsigned long nowMs,
                                                          uint8_t state, uint8_t breath,
                                                          const uint16_t *pal) {
    // 1. RIGHT SIDE: Vertical 60-Second Sith Lightsaber (x = 104..126, y = 22..232)
    const int sx = 113;
    const int hiltTop = 196;
    const int maxBladeH = 168; // y = 194 down to y = 26
    int bladeH = (now.tm_sec * maxBladeH) / 59;
    if (state >= 1 && bladeH < 20) bladeH = maxBladeH;

    // Blade guide track in corridor wall
    s.drawFastVLine(sx, 26, maxBladeH, pal[P_GRILLE_DIM]);
    if (bladeH > 0) {
        int yTop = hiltTop - 2 - bladeH;
        drawSaberBeam(s, sx, hiltTop - 2, sx, yTop, pal, true, state >= 1);
        // Glowing blade tip cap
        s.fillCircle(sx, yTop, 3, pal[P_WHITE]);
    }

    // Detailed Vertical Saber Hilt at bottom-right (y = 196..232)
    s.fillRect(sx - 5, hiltTop, 11, 34, pal[P_STEEL]);
    s.drawFastVLine(sx - 2, hiltTop, 34, pal[P_STEEL_L]);
    s.fillRect(sx - 6, hiltTop - 3, 13, 5, pal[P_ARMOR_BLACK]); // Emitter shroud
    s.drawFastHLine(sx - 6, hiltTop - 3, 13, pal[P_RED_HOT]);
    s.fillRect(sx - 7, hiltTop + 10, 2, 7, pal[P_RED_BLADE]);   // Activation box switch
    for (int gy = hiltTop + 18; gy < hiltTop + 32; gy += 4) {
        s.fillRect(sx - 5, gy, 11, 2, pal[P_ARMOR_BLACK]);
    }

    // 2. LEFT/CENTER: Imperial Corridor Blast-Door Telemetry Console (x = 12..98)
    swFillChamfer(s, 14, 24, 84, 78, 6, pal[P_PANEL_DARK]);
    s.drawRect(16, 26, 80, 74, pal[P_GRILLE_LIT]);
    s.drawFastHLine(22, 26, 68, pal[P_RED_HOT]);

    // Weather condition rendered as Sith Hologram inside the upper blast-door viewport
    const char *ic = weather.getIconName(5).c_str();
    char c0 = ic[0], c1 = ic[1];
    int wx = 36, wy = 50;
    if (c0 == '0' && c1 == '1') {
        // Mustafar binary eclipse sun
        s.fillCircle(wx, wy, 10, pal[P_RED_BLADE]);
        s.drawCircle(wx, wy, 10, pal[P_EMBER_O]);
        s.fillCircle(wx + 8, wy - 5, 5, pal[P_WHITE]);
    } else if (c0 == '1' && c1 == '1') {
        // Sith Force storm lightning
        s.drawCircle(wx + 4, wy - 3, 9, pal[P_RED_HOT]);
        s.drawLine(wx + 4, wy + 2, wx - 2, wy + 12, pal[P_EMBER_O]);
        s.drawLine(wx - 2, wy + 12, wx + 6, wy + 12, pal[P_WHITE]);
        s.drawLine(wx + 6, wy + 12, wx + 1, wy + 20, pal[P_EMBER_O]);
    } else {
        // Imperial scanner cloud + rain/ash
        s.drawCircle(wx - 2, wy, 8, pal[P_RED_HOT]);
        s.drawCircle(wx + 8, wy - 3, 10, pal[P_RED_BLADE]);
        s.drawFastHLine(wx - 8, wy + 7, 24, pal[P_WHITE]);
        if (c0 == '0' && (c1 == '9' || c1 == '0')) {
            for (int r = 0; r < 3; ++r) s.drawLine(wx - 4 + r * 7, wy + 10, wx - 7 + r * 7, wy + 17, pal[P_EMBER_O]);
        }
    }

    char unit = (WeatherService::getUnits().value == "imperial") ? 'F' : 'C';
    float vals[3] = {weather.getNowTemp(), weather.getHigh(5), weather.getLow(5)};
    int ints[3];
    for (int k = 0; k < 3; ++k) ints[k] = (int)(vals[k] + (vals[k] >= 0.0f ? 0.5f : -0.5f));

    char buf[16];
    snprintf(buf, sizeof(buf), "%d%c", ints[0], unit);
    swDrawTxt(s, buf, 74, 52, 4, 1, MC_DATUM, pal[P_WHITE]);

    // Divider & MAX / MIN thermal readouts inside the blast-door console
    s.drawFastHLine(20, 72, 72, pal[P_SABER_AURA]);
    snprintf(buf, sizeof(buf), "HI:%d  LO:%d", ints[1], ints[2]);
    swDrawTxt(s, buf, 56, 86, 2, 1, MC_DATUM, pal[P_PINK_CORE]);

    // 3. MIDDLE: Live Respirator / Vocoder Waveform Monitor (y = 108..166)
    swFillChamfer(s, 14, 108, 84, 58, 5, pal[P_PANEL_DARK]);
    s.drawRect(16, 110, 80, 54, pal[P_STEEL_D]);
    swDrawTxt(s, state == 2 ? "DARK RAGE" : "VOCODER", 56, 115, 1, 1, TC_DATUM, pal[P_RED_HOT]);

    // Animated respirator oscilloscope wave across x = 20..92
    int midY = 144;
    s.drawFastHLine(20, midY, 72, pal[P_BG_SMOKE_M]);
    int amp = 3 + (breath * 13) / 255 + (state >= 1 ? 6 : 0);
    int prevY = midY;
    for (int x = 20; x <= 92; x += 3) {
        int wavePhase = (int)((nowMs / 28u) + x * 5);
        int tri = (wavePhase & 31) < 16 ? ((wavePhase & 15) - 8) : (8 - (wavePhase & 15));
        int env = (x > 32 && x < 80) ? amp : (amp / 3);
        int wy2 = midY + (tri * env) / 8;
        s.drawLine(x - 3, prevY, x, wy2, pal[(abs(wy2 - midY) > 7) ? P_WHITE : P_RED_HOT]);
        prevY = wy2;
    }

    // 4. BOTTOM: Digital Seconds Counter & Corridor Alert Status (y = 172..234)
    swFillChamfer(s, 14, 172, 84, 60, 5, pal[P_PANEL_DARK]);
    s.drawFastHLine(20, 172, 72, pal[P_RED_BLADE]);
    swDrawTxt(s, state >= 1 ? "ALERTA!" : "CORREDOR", 56, 180, 1, 1, TC_DATUM,
              pal[state >= 1 ? P_WHITE : P_EMBER_O]);
    snprintf(buf, sizeof(buf), ":%02d", now.tm_sec);
    swDrawTxt(s, buf, 56, 210, 4, 1, MC_DATUM, pal[state == 2 ? P_WHITE : P_RED_HOT]);
}

} // namespace

void MatrixClock::renderVader(IPSClock &clock, TimeSync &time, WeatherService &weather) {
    unsigned long nowMs = millis();
    if (nowMs - lastFrame < 45) return;
    lastFrame = nowMs;
    ++frame;

    struct tm now;
    suseconds_t subsecond;
    time.getLocalTime(&now, &subsecond);

    uint16_t stateMs = 0;
    if (now.tm_sec <= 2) {
        vaderSaberState = 2;
        stateMs = (uint16_t)(now.tm_sec * 1000u + (uint32_t)(subsecond / 1000));
    } else if (now.tm_sec >= 57) {
        vaderSaberState = 1;
        stateMs = (uint16_t)((now.tm_sec - 57) * 1000u + (uint32_t)(subsecond / 1000));
    } else {
        vaderSaberState = 0;
    }

    uint8_t breath = getBreathLevel(nowMs);

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
    swDimPalette(VADER_RAW, pal, P_COUNT);

    for (int i = 0; i < 6; ++i) {
        drawRogueOneCorridor(sprite, i, nowMs, vaderSaberState, stateMs, breath, pal);

        if (i == 2) {
            drawVaderRogueOneCenter(sprite, now, subsecond, nowMs, vaderSaberState, breath, pal);
        } else if (i == 5) {
            drawVaderCorridorTelemetry(sprite, weather, now, nowMs, vaderSaberState, breath, pal);
        } else {
            bool blankHour = (i == 0 && hour < 10 && !IPSClock::getLeadingZero().value);
            if (!blankHour) {
                drawVaderSaberDigit(sprite, digits[i], nowMs, vaderSaberState, pal);
            }
        }

        tfts->chip_select.setDigit(SW_SCREENS[i]);
        sprite.pushSprite(0, 0);
    }
    tfts->release();
}
