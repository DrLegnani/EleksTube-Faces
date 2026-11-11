#ifndef _STAR_WARS_HUD_H
#define _STAR_WARS_HUD_H

#include "TFTs.h"
#include "WeatherService.h"
#include <Arduino.h>
#include <time.h>

extern TFTs *tfts;

constexpr uint16_t swC565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

extern const uint8_t SW_SCREENS[6];

void swDimPalette(const uint16_t *raw, uint16_t *out, int count);
void swFillChamfer(TFT_eSprite &s, int x, int y, int w, int h, int c, uint16_t col);
void swDrawTxt(TFT_eSprite &s, const char *str, int x, int y, uint8_t font, uint8_t size, uint8_t datum, uint16_t col);

// 7-segment chamfered digit with corner brackets
void swDrawHoloDigit(TFT_eSprite &s, int digit, uint16_t auraCol, uint16_t bodyCol, uint16_t coreCol, uint16_t bracketCol);

// Center screen (2) header, pulsing ':', bottom white DD.MM date frame
void swDrawCenterChrome(TFT_eSprite &s, const char *title, struct tm &now, suseconds_t subsec,
                        uint16_t titleCol, uint16_t lineCol, uint16_t colonBody, uint16_t colonCore, uint16_t dateCol);

// Last screen (5) NavHUD: dark weather box + vector icon + temp + 2x MAX/MIN + white 60s bar
void swDrawNavWeatherHud(TFT_eSprite &s, WeatherService &weather, struct tm &now,
                         const char *topTitle, const char *barLabel,
                         uint16_t accentCol, uint16_t lineCol, uint16_t boxBg, uint16_t borderCol,
                         uint16_t warmCol, uint16_t coolCol, bool highlightBar);

#endif
