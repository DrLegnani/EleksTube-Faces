#include "OpenMeteoWeatherService.h"
#include <LittleFS.h>
#include "TFTs.h"
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <time.h>

uint8_t weatherDiagnostic = 0;

// Map WMO conditions to the icon names used by the existing clock faces.
static String iconFor(int code, bool day) {
    const char *base = "unknown";
    if (code == 0) base = "01";
    else if (code == 1 || code == 2) base = "02";
    else if (code == 3) base = "04";
    else if (code == 45 || code == 48) base = "50";
    else if (code >= 51 && code <= 57) base = "09";
    else if (code >= 61 && code <= 67) base = "10";
    else if (code >= 71 && code <= 77) base = "13";
    else if (code >= 80 && code <= 82) base = "09";
    else if (code == 85 || code == 86) base = "13";
    else if (code >= 95 && code <= 99) base = "11";
    if (strcmp(base, "unknown") == 0) return String(base);
    return String(base) + (day ? "d" : "n");
}

bool OpenMeteoWeatherService::getWeatherInfo() {
    weatherDiagnostic = 1;
    const String lat = getLatitude().value;
    const String lon = getLongitude().value;
    if (lat.length() == 0 || lon.length() == 0) return false;

    String path = "/v1/forecast?latitude=" + lat + "&longitude=" + lon;
    path += "&current=temperature_2m,weather_code,is_day&daily=weather_code,temperature_2m_max,temperature_2m_min&timezone=auto&forecast_days=6";
    if (getUnits().value == "imperial") path += "&temperature_unit=fahrenheit";

    // Keep the public CA bundle alongside the display assets to save app flash.
    // The caller holds memMutex, pausing every display renderer. Borrow its
    // existing pixel buffer instead of reserving more TLS heap or task stack.
    char *certificates = reinterpret_cast<char*>(StaticSprite::output_buffer);
    fs::File ca = LittleFS.open("/ips/tron/ca.pem", "r");
    if (!ca || ca.size() >= 3072) return false;
    size_t size = ca.size();
    if (ca.readBytes(certificates, size) != size) { ca.close(); return false; }
    ca.close(); certificates[size] = '\0';
    WiFiClientSecure client;
    client.setCACert(certificates);
    client.setTimeout(15000);
    weatherDiagnostic = 2;
    if (!client.connect("api.open-meteo.com", 443)) return false;
    client.print(String("GET ") + path + " HTTP/1.0\r\nHost: api.open-meteo.com\r\nConnection: close\r\nAccept: application/json\r\n\r\n");
    weatherDiagnostic = 3;
    String status = client.readStringUntil('\n');
    if (!status.startsWith("HTTP/1.") || status.indexOf(" 200 ") < 0) { client.stop(); return false; }
    weatherDiagnostic = 4;
    bool headersComplete = false;
    while (client.connected()) {
        String line = client.readStringUntil('\n');
        if (line == "\r" || line.length() == 0) { headersComplete = true; break; }
        line.toLowerCase();
        if (line.startsWith("transfer-encoding:") && line.indexOf("chunked") >= 0) { client.stop(); return false; }
    }
    if (!headersComplete) { client.stop(); return false; }

    weatherDiagnostic = 5;
    JsonDocument filter;
    filter["current"]["temperature_2m"] = true;
    filter["current"]["weather_code"] = true;
    filter["current"]["is_day"] = true;
    filter["daily"]["time"][0] = true;
    filter["daily"]["weather_code"][0] = true;
    filter["daily"]["temperature_2m_max"][0] = true;
    filter["daily"]["temperature_2m_min"][0] = true;
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, client, DeserializationOption::Filter(filter));
    client.stop();
    if (err || doc["current"]["temperature_2m"].isNull()) return false;

    weatherDiagnostic = 6;
    JsonArray dates = doc["daily"]["time"].as<JsonArray>();
    JsonArray codes = doc["daily"]["weather_code"].as<JsonArray>();
    JsonArray maxes = doc["daily"]["temperature_2m_max"].as<JsonArray>();
    JsonArray mins = doc["daily"]["temperature_2m_min"].as<JsonArray>();
    if (dates.size() < 6 || codes.size() < 6 || maxes.size() < 6 || mins.size() < 6) return false;

    for (int i = 0; i < 6; ++i) {
        if (dates[i].isNull() || codes[i].isNull() || maxes[i].isNull() || mins[i].isNull()) return false;
        const char *date = dates[i];
        int year, month, day;
        if (!date || sscanf(date, "%d-%d-%d", &year, &month, &day) != 3) return false;
    }
    current = doc["current"]["temperature_2m"].as<float>();
    for (int i = 0; i < 6; ++i) {
        int index = 5 - i, year, month, day;
        sscanf(dates[i].as<const char *>(), "%d-%d-%d", &year, &month, &day);
        struct tm date = {};
        date.tm_year = year - 1900;
        date.tm_mon = month - 1;
        date.tm_mday = day;
        date.tm_hour = 12;
        date.tm_isdst = -1;
        mktime(&date);
        weekdays[index] = date.tm_wday;
        highs[index] = maxes[i].as<float>();
        lows[index] = mins[i].as<float>();
        bool daytime = i != 0 || (doc["current"]["is_day"] | 1) == 1;
        int code = i == 0 ? (doc["current"]["weather_code"] | -1) : codes[i].as<int>();
        icons[index] = iconFor(code, daytime);
    }
    weatherDiagnostic = 7;
    return true;
}
