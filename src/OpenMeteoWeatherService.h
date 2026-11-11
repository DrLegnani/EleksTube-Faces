#pragma once

#include <ConfigItem.h>
#include <math.h>
#include "WeatherService.h"

class OpenMeteoWeatherService : public WeatherService {
public:
    bool getWeatherInfo() override;
    const String& getIconName(int day) override { return icons[day]; }
    float getHigh(int day) override { return highs[day]; }
    float getLow(int day) override { return lows[day]; }
    int getDayOfWeek(int day) override { return weekdays[day]; }
    float getNowTemp() override { return current; }

private:
    String icons[6] = {"unknown", "unknown", "unknown", "unknown", "unknown", "unknown"};
    float highs[6] = {NAN, NAN, NAN, NAN, NAN, NAN};
    float lows[6] = {NAN, NAN, NAN, NAN, NAN, NAN};
    int weekdays[6] = {-1, -1, -1, -1, -1, -1};
    float current = NAN;
};
