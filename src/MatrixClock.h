#pragma once

#include <TimeSync.h>
#include "IPSClock.h"
#include "WeatherService.h"

// Runtime-rendered clock face. Other clock modes and faces use their normal renderer.
class MatrixClock {
public:
    void render(IPSClock &clock, TimeSync &time, WeatherService &weather);
    void renderHyperspace(IPSClock &clock, TimeSync &time, WeatherService &weather);
    void renderVader(IPSClock &clock, TimeSync &time, WeatherService &weather);
    void renderDeathStar(IPSClock &clock, TimeSync &time, WeatherService &weather);
    void renderViceCity(IPSClock &clock, TimeSync &time, WeatherService &weather);
    void renderDelorean(IPSClock &clock, TimeSync &time, WeatherService &weather);
    void renderNixieStranger(IPSClock &clock, TimeSync &time, WeatherService &weather);
    void renderCyberpunk(IPSClock &clock, TimeSync &time, WeatherService &weather);
    void renderTron(IPSClock &clock, TimeSync &time, WeatherService &weather);
    void renderMario(IPSClock &clock, TimeSync &time, WeatherService &weather);
    void renderTatooine(IPSClock &clock, TimeSync &time, WeatherService &weather);
    void releaseMasks();

private:
    unsigned long lastFrame = 0;
    unsigned long frame = 0;
    uint8_t *digitMasks = nullptr;
    int cachedDigits[5] = {-1, -1, -1, -1, -1};
};
