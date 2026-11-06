#include "network_service.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <esp_task_wdt.h>

#include <algorithm>
#include <cstring>

#include "project_config.h"
#include "telemetry.h"

namespace {
constexpr uint32_t kNetworkLoopDelayMs = 20;
constexpr uint32_t kMinimumReconnectDelayMs = 1000;
constexpr uint32_t kMaximumReconnectDelayMs = 60000;
constexpr uint32_t kMqttSocketTimeoutSeconds = 2;

WiFiClientSecure tlsClient;
PubSubClient mqttClient(tlsClient);

bool timeReached(const uint32_t now, const uint32_t target) {
    return static_cast<int32_t>(now - target) >= 0;
}

void feedTaskWatchdog() {
    const esp_err_t result = esp_task_wdt_reset();
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
        Serial.printf("WDT reset error: %d\n", static_cast<int>(result));
    }
}

bool publishTelemetry(const IaqTelemetry& reading) {
    char topic[96];
    char payload[384];

    snprintf(topic, sizeof(topic), "iaq/%s/telemetry", IAQ_DEVICE_ID);
    const int written = snprintf(
        payload,
        sizeof(payload),
        "{\"seq\":%lu,\"uptime_ms\":%lu,\"co2_ppm\":%.1f,"
        "\"scd30_valid\":%s,\"scd30_temp_c\":%.2f,\"scd30_rh_pct\":%.2f,"
        "\"bme680_valid\":%s,\"bme_temp_c\":%.2f,\"bme_rh_pct\":%.2f,"
        "\"pressure_hpa\":%.2f,\"gas_resistance_ohms\":%.0f,"
        "\"pms5003_valid\":%s,\"pm25_atm_ug_m3\":%u,\"co2_alarm\":%s}",
        static_cast<unsigned long>(reading.sequence),
        static_cast<unsigned long>(reading.uptimeMs),
        reading.co2Ppm,
        reading.scd30Valid ? "true" : "false",
        reading.scd30TemperatureC,
        reading.scd30HumidityPercent,
        reading.bme680Valid ? "true" : "false",
        reading.bmeTemperatureC,
        reading.bmeHumidityPercent,
        reading.pressureHpa,
        reading.gasResistanceOhms,
        reading.pms5003Valid ? "true" : "false",
        static_cast<unsigned int>(reading.pm25Atmospheric),
        reading.co2Alarm ? "true" : "false");

    if (written <= 0 || static_cast<size_t>(written) >= sizeof(payload)) {
        Serial.println("Telemetry payload exceeded buffer");
        return false;
    }

    // PubSubClient's publish API sends QoS 0; retain only the latest state.
    return mqttClient.publish(topic, payload, true);
}
}  // namespace

void networkTask(void* queueParameter) {
    const QueueHandle_t telemetryQueue = static_cast<QueueHandle_t>(queueParameter);
    const esp_err_t watchdogResult = esp_task_wdt_add(nullptr);
    if (watchdogResult != ESP_OK && watchdogResult != ESP_ERR_INVALID_STATE) {
        Serial.printf("Network task WDT registration error: %d\n",
                      static_cast<int>(watchdogResult));
    }

    const bool hasWifiCredentials = strlen(IAQ_WIFI_SSID) > 0;
    const bool hasBroker = strlen(IAQ_MQTT_HOST) > 0;
    const bool hasTrustAnchor = strlen(IAQ_MQTT_ROOT_CA) > 0;

    if (hasTrustAnchor) {
        tlsClient.setCACert(IAQ_MQTT_ROOT_CA);
    } else {
        Serial.println("MQTT disabled: trusted root CA is not configured");
    }

    if (!hasWifiCredentials) {
        Serial.println("Wi-Fi disabled: configure credentials in secrets.h");
    }

    mqttClient.setServer(IAQ_MQTT_HOST, IAQ_MQTT_PORT);
    mqttClient.setSocketTimeout(kMqttSocketTimeoutSeconds);
    mqttClient.setKeepAlive(30);
    mqttClient.setBufferSize(512);
    WiFi.mode(WIFI_STA);

    uint32_t wifiRetryDelayMs = kMinimumReconnectDelayMs;
    uint32_t mqttRetryDelayMs = kMinimumReconnectDelayMs;
    uint32_t nextWifiAttemptMs = 0;
    uint32_t nextMqttAttemptMs = 0;
    uint32_t nextPublishMs = millis();
    bool initialPublishPending = true;

    for (;;) {
        const uint32_t now = millis();

        if (hasWifiCredentials && WiFi.status() != WL_CONNECTED &&
            timeReached(now, nextWifiAttemptMs)) {
            WiFi.begin(IAQ_WIFI_SSID, IAQ_WIFI_PASSWORD);
            nextWifiAttemptMs = now + wifiRetryDelayMs;
            wifiRetryDelayMs = std::min(wifiRetryDelayMs * 2, kMaximumReconnectDelayMs);
        } else if (WiFi.status() == WL_CONNECTED) {
            wifiRetryDelayMs = kMinimumReconnectDelayMs;
        }

        const bool mqttConfigured = hasWifiCredentials && hasBroker && hasTrustAnchor;
        if (mqttConfigured && WiFi.status() == WL_CONNECTED) {
            if (!mqttClient.connected() && timeReached(now, nextMqttAttemptMs)) {
                char clientId[64];
                snprintf(clientId, sizeof(clientId), "%s", IAQ_DEVICE_ID);

                bool connected = false;
                if (strlen(IAQ_MQTT_USERNAME) > 0) {
                    connected = mqttClient.connect(
                        clientId, IAQ_MQTT_USERNAME, IAQ_MQTT_PASSWORD);
                } else {
                    connected = mqttClient.connect(clientId);
                }

                if (connected) {
                    Serial.println("MQTT connected using TLS");
                    mqttRetryDelayMs = kMinimumReconnectDelayMs;
                    initialPublishPending = true;
                } else {
                    Serial.printf("MQTT connect failed, state=%d\n", mqttClient.state());
                    nextMqttAttemptMs = now + mqttRetryDelayMs;
                    mqttRetryDelayMs =
                        std::min(mqttRetryDelayMs * 2, kMaximumReconnectDelayMs);
                }
            }

            if (mqttClient.connected()) {
                mqttClient.loop();
                if (initialPublishPending || timeReached(now, nextPublishMs)) {
                    IaqTelemetry latest;
                    if (xQueuePeek(telemetryQueue, &latest, 0) == pdTRUE &&
                        publishTelemetry(latest)) {
                        initialPublishPending = false;
                        nextPublishMs = millis() + IAQ_MQTT_PUBLISH_PERIOD_MS;
                    }
                }
            }
        }

        feedTaskWatchdog();
        vTaskDelay(pdMS_TO_TICKS(kNetworkLoopDelayMs));
    }
}