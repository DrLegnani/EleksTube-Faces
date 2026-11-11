#include <IPAddress.h>
#include <WiFi.h>
#include <AsyncWiFiManager.h>
#include <ArduinoJson.h>
#include <ConfigItem.h>

#include "ScreenSaver.h"
#include "Backlights.h"
#include "mqttBroker.h"
#include "IPSClock.h"

extern AsyncWiFiManager *wifiManager;
extern CompositeConfigItem rootConfig;
extern ScreenSaver *screenSaver;
extern void broadcastUpdate(const BaseConfigItem&);
extern IRAMPtrArray<const char*> manifest;
extern SemaphoreHandle_t memMutex;
extern QueueHandle_t mainQueue;

static const char *device_s = "dev";
IRAMPtrArray<const char*> MQTTBroker::displayStates {
    "Time",
    "Date",
    "Weather",
    "Slideshow",
    0
};

void MQTTBroker::onConnect(bool sessionPresent)
{
	reconnect = false;
//	Serial.println("Connected to MQTT");
    client.subscribe("homeassistant/status", 2);
    sendHADiscoveryMessage();
}

#ifdef ASYNC_MQTT_HA_CLIENT
void MQTTBroker::onDisconnect(AsyncMqttClientDisconnectReason reason)
#else
void MQTTBroker::onDisconnect(espMqttClientTypes::DisconnectReason reason)
#endif
{
//	Serial.printf("Disconnected from MQTT: %u\n", static_cast<uint8_t>(reason));

    reconnect = true;
    lastReconnect = millis();
}

#ifdef ASYNC_MQTT_HA_CLIENT
void MQTTBroker::onMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t length, size_t index, size_t total_length)
#else
void MQTTBroker::onMessage(const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t*  payload, size_t length, size_t index, size_t total_length)
#endif
{
	uint8_t mqttMessageBuffer[32];

		// payload is bigger then max: return chunked
	if (total_length >= sizeof(mqttMessageBuffer)) {
		Serial.printf("MQTT message too large (%d): topic=%s, payload=%s\n", total_length, topic, payload);
		return;
	}

	// add data and dispatch when done
	memcpy(&mqttMessageBuffer[index], payload, length);
	if (index + length == total_length) {
		// message is complete here
		mqttMessageBuffer[total_length] = 0;

        /*
         e.g. home = "/fred"
         topic = "/fred/topic"
         topicIndex = 5
         */
        int topicIndex = strlen(home);
        if (strcmp(topic, "homeassistant/status") == 0) {
            if (strcmp("online", (const char*)mqttMessageBuffer)) {
                Serial.println("HA online");
                sendHADiscoveryMessage();
            }
        } else if (strcmp(topic + topicIndex, screenSaverTopic + 1) == 0) {
            if (strcmp((const char*)mqttMessageBuffer, "OFF") == 0) { 
                screenSaver->reset();
            } else {
                screenSaver->start();
            }
        } else if (strcmp(topic + topicIndex, screenSaverDelayTopic + 1) == 0) {
            ScreenSaver::getScreenSaverDelay() = atoi((const char*)mqttMessageBuffer);
            broadcastUpdate(ScreenSaver::getScreenSaverDelay());
        } else if (strcmp(topic + topicIndex, brightnessTopic + 1) == 0) {
            IPSClock::getBrightnessConfig() = atoi((const char*)mqttMessageBuffer);
            broadcastUpdate(IPSClock::getBrightnessConfig());
        } else if (strcmp(topic + topicIndex, displayTopic + 1) == 0) {
            IPSClock::getTimeOrDate() = atoi((const char*)mqttMessageBuffer);
            broadcastUpdate(IPSClock::getTimeOrDate());
            IPSClock::getTimeOrDate().notify();
        } else if (strcmp(topic + topicIndex, customDataTopic + 1) == 0) {
            // TODO: get the max data from somewhere
            if (length <= 6){
                // empty string or clear/CLEAR will clear the custom data
                if (strcmp((const char*)mqttMessageBuffer, "CLEAR") == 0
                    || strcmp((const char*)mqttMessageBuffer, "clear") == 0){
                    IPSClock::getCustomData() = "";
                }
                else{
                    IPSClock::getCustomData() = (const char*)mqttMessageBuffer;
                }
            }
            else{
//                Serial.println("Custom data too long, ignoring");
            }
        } else if (strcmp(topic + topicIndex, backlightStateTopic + 1) == 0) {
            if (strcmp((const char*)mqttMessageBuffer, "OFF") == 0) {
                Backlights::backlightState = false;
            } else {
                Backlights::backlightState = true;
            }
        } else if (strcmp(topic + topicIndex, backlightHSTopic + 1) == 0) {
            const char *comma = strchr((const char*)mqttMessageBuffer, ',');
            if (comma) {
                Backlights::backlightHue = int(atof((const char*)mqttMessageBuffer) * 255 / 360);
                Backlights::backlightSaturation = int(atof(comma + 1) * 255 / 100);
            }
        } else if (strcmp(topic + topicIndex, backlightBrightnessTopic + 1) == 0) {
            Backlights::backlightBrightness = atoi((const char*)mqttMessageBuffer);
#if (NUM_LEDS > 6)
        } else if (strcmp(topic + topicIndex, underlightStateTopic + 1) == 0) {
            if (strcmp((const char*)mqttMessageBuffer, "OFF") == 0) {
                Backlights::backlightState = false;
            } else {
                Backlights::backlightState = true;
            }
        } else if (strcmp(topic + topicIndex, underlightHSTopic + 1) == 0) {
            const char *comma = strchr((const char*)mqttMessageBuffer, ',');
            if (comma) {
                Backlights::underlightHue = int(atof((const char*)mqttMessageBuffer) * 255 / 360);
                Backlights::underlightSaturation = int(atof(comma + 1) * 255 / 100);
            }
        } else if (strcmp(topic + topicIndex, underlightBrightnessTopic + 1) == 0) {
            Backlights::underlightBrightness = atoi((const char*)mqttMessageBuffer);
#endif
        } else {
            // Serial.print("Unknown topic: ");
            // Serial.print(topic);
            // Serial.print(", value: ");
            // Serial.println((char*)mqttMessageBuffer);
        }
	}

    uint32_t msg = 1;
	xQueueSend(mainQueue, &msg, pdMS_TO_TICKS(100));
}

bool MQTTBroker::init(const String& id) {
    this->id = id;

    client.disconnect();

    IPAddress ipAddress;
    if (ipAddress.fromString(getHost().value.c_str())) {
        sprintf(home, "clock/%s", id.c_str());
        sprintf(volatileStateTopic, "clock/%s/volatile/state", id.c_str());
        sprintf(persistentStateTopic, "clock/%s/persistent/state", id.c_str());
        sprintf(availabilityTopic, "clock/%s/availability", id.c_str());

        client.setServer(getHost().value.c_str(), getPort());
        client.setCredentials(getUser().value.c_str(),getPassword().value.c_str());
        client.setWill(availabilityTopic, 2, true, "offline");
        client.onConnect([this](bool sessionPresent) { this->onConnect(sessionPresent); });
#ifdef ASYNC_MQTT_HA_CLIENT
        client.onDisconnect([this](AsyncMqttClientDisconnectReason reason) { this->onDisconnect(reason); });
        client.onMessage([this](char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t length, size_t index, size_t total_length) {
            this->onMessage(topic, payload, properties, length, index, total_length); });
#else
        client.onDisconnect([this](espMqttClientTypes::DisconnectReason reason) { this->onDisconnect(reason); });
        client.onMessage([this](const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t*  payload, size_t length, size_t index, size_t total_length) {
            this->onMessage(properties, topic, payload, length, index, total_length); });
#endif
        screenSaver->setChangeCallback([this](bool isOff) { 
            uint32_t msg = 1;
            xQueueSend(mainQueue, &msg, pdMS_TO_TICKS(100));
        });
        
        reconnect = true;
    }

    return reconnect;
}

void MQTTBroker::connect() {
    if (WiFi.isConnected() && !wifiManager->isAP()) {
//        Serial.println("Connecting to MQTT...");
        client.connect();
    }
}

void MQTTBroker::checkConnection() {
    if (reconnect && (millis() - lastReconnect >= 2000)) {
        lastReconnect = millis();
        connect();
    }
}

static __attribute__((noinline)) void addLightState(JsonDocument &doc, const char *prefix, uint8_t hue, uint8_t sat, uint8_t bri, bool state) {
    char key[24];
    strcpy(key, prefix); strcat(key, "_state");
    doc[key] = state ? "ON" : "OFF";
    strcpy(key, prefix); strcat(key, "_hs");
    JsonArray a = doc[key].to<JsonArray>();
    a.add(int(360 * hue / 255));
    a.add(int(100 * sat / 255));
    strcpy(key, prefix); strcat(key, "_brightness");
    doc[key] = bri;
}

void MQTTBroker::publishState() {
    if (client.connected()) {
        if (xSemaphoreTake(memMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
            JsonDocument volatileState;
            volatileState["screen_saver_on"] = screenSaver->isOn() ? "ON" : "OFF";
            volatileState["brightness"] = IPSClock::getBrightnessConfig().value;
            volatileState["custom"] = IPSClock::getCustomData().value;
            volatileState["display"] = IPSClock::getTimeOrDate().value;

            addLightState(volatileState, "backlight", Backlights::backlightHue, Backlights::backlightSaturation, Backlights::backlightBrightness, Backlights::backlightState);
#if (NUM_LEDS > 6)
            addLightState(volatileState, "underlight", Backlights::underlightHue, Backlights::underlightSaturation, Backlights::underlightBrightness, Backlights::backlightState);
#endif
            char buffer[384];
            serializeJson(volatileState, buffer);
            client.publish(volatileStateTopic, 1, false, buffer);
            client.publish(persistentStateTopic, 1, false, rootConfig.toJSON().c_str());
            xSemaphoreGive(memMutex);
        }
    }
}

static __attribute__((noinline)) void prepDiscovery(
    JsonDocument &doc, const char *home, const char *id,
    const char *name, const char *icon, const char *uidPrefix,
    const char *stat, const char *cmd, const char *tpl
) {
    char uid[64];
    strcpy(uid, uidPrefix); strcat(uid, id);
    doc.clear();
    doc["~"] = home;
    doc["name"] = name;
    doc["icon"] = icon;
    doc["unique_id"] = uid;
    doc["stat_t"] = stat;
    doc["cmd_t"] = cmd;
    if (tpl) doc["val_tpl"] = tpl;
}

static __attribute__((noinline)) void sendDiscovery(
    AsyncMqttClient &client, JsonDocument &doc, char *buffer,
    const char *comp, const char *id, const char *objId, const char *avty
) {
    char topic[128];
    sprintf(topic, "http://%s/", WiFi.localIP().toString().c_str());
    doc["avty_t"] = avty;
    doc["dev"]["configuration_url"] = topic;
    doc["dev"]["name"] = manifest[3];
    doc["dev"]["identifiers"][0] = WiFi.macAddress();
    doc["dev"]["model"] = manifest[0];
    doc["dev"]["sw_version"] = manifest[1];
    sprintf(topic, "homeassistant/%s/%s/%s/config", comp, id, objId);
    serializeJson(doc, buffer, 1024);
    client.publish(topic, 1, false, buffer);
}

void MQTTBroker::sendHADiscoveryMessage() {
    char buffer[1024];
    delay(300);

    strcpy(buffer, home);
    strcat(buffer, "/set/#");
    client.subscribe(buffer, 0);

    JsonDocument doc;
    const char *cid = id.c_str();

    struct { const char *name, *icon, *uid, *stat, *cmd, *tpl, *comp, *obj; uint8_t mn, mx; } items[] = {
        {"Screen Saver", "mdi:sleep", "screen_saver_", volatileStateTopic, screenSaverTopic, "{{value_json.screen_saver_on}}", "switch", "screen_saver", 0, 0},
        {"Screen Saver Delay", "mdi:knob", "screen_saver_delay", "~/persistent/state", screenSaverDelayTopic, "{{value_json.matrix.screen_saver_delay}}", "number", "screen_saver_delay", 0, 120},
        {"Brightness", "mdi:brightness-6", "brightness", volatileStateTopic, brightnessTopic, "{{value_json.brightness}}", "number", "brightness", 10, 255},
        {"Custom Data", "mdi:clock-edit", "custom-data", volatileStateTopic, customDataTopic, "{{value_json.custom}}", "text", "custom", 0, 0},
    };
    for (auto &it : items) {
        prepDiscovery(doc, home, cid, it.name, it.icon, it.uid, it.stat, it.cmd, it.tpl);
        if (it.mx) { doc["min"] = it.mn; doc["max"] = it.mx; }
        sendDiscovery(client, doc, buffer, it.comp, cid, it.obj, availabilityTopic);
    }

    const char *lNames[2] = {"Backlight", "Underlight"};
    const char *lIds[2] = {"backlight", "underlight"};
    const char *lCmds[2] = {backlightStateTopic, underlightStateTopic};
    const char *lBriCmds[2] = {backlightBrightnessTopic, underlightBrightnessTopic};
    const char *lHsCmds[2] = {backlightHSTopic, underlightHSTopic};

    for (int k = 0; k < (NUM_LEDS > 6 ? 2 : 1); ++k) {
        prepDiscovery(doc, home, cid, lNames[k], "mdi:led-strip-variant", lIds[k], volatileStateTopic, lCmds[k], nullptr);
        char tpl[64];
        sprintf(tpl, "{{value_json.%s_state}}", lIds[k]);
        doc["stat_val_tpl"] = tpl;
        doc["bri_stat_t"] = volatileStateTopic;
        doc["bri_cmd_t"] = lBriCmds[k];
        sprintf(tpl, "{{value_json.%s_brightness}}", lIds[k]);
        doc["bri_val_tpl"] = tpl;
        doc["hs_stat_t"] = volatileStateTopic;
        doc["hs_cmd_t"] = lHsCmds[k];
        sprintf(tpl, "{{value_json.%s_hs | join(',')}}", lIds[k]);
        doc["hs_val_tpl"] = tpl;
        doc["clrm"] = true;
        doc["sup_clrm"].to<JsonArray>().add("hs");
        sendDiscovery(client, doc, buffer, "light", cid, lIds[k], availabilityTopic);
    }

    prepDiscovery(doc, home, cid, "Display", "mdi:theater", "display", volatileStateTopic, displayTopic,
        "{{ ['Time','Date','Weather','Slideshow'][value_json.display] }}");
    doc["command_template"] = "{{ ['Time','Date','Weather','Slideshow'].index(value) }}";
    JsonArray optArray = doc["options"].to<JsonArray>();
    for (int i = 0; displayStates[i] != 0; i++) {
        optArray.add(displayStates[i]);
    }
    sendDiscovery(client, doc, buffer, "select", cid, "display", availabilityTopic);

    client.publish(availabilityTopic, 2, true, "online");

    uint32_t msg = 1;
    xQueueSend(mainQueue, &msg, pdMS_TO_TICKS(100));
}
