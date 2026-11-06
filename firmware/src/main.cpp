#include <Arduino.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "iaq_sensors.h"
#include "network_service.h"
#include "project_config.h"
#include "telemetry.h"

namespace {
constexpr UBaseType_t kSensorTaskPriority = 4;
constexpr UBaseType_t kNetworkTaskPriority = 1;
constexpr uint32_t kSensorTaskStackBytes = 8192;
constexpr uint32_t kNetworkTaskStackBytes = 8192;

QueueHandle_t telemetryQueue = nullptr;
IaqSensors sensors;

void updateLocalAlarm(IaqTelemetry& reading, bool& alarmActive) {
    if (reading.scd30Valid) {
        if (!alarmActive && reading.co2Ppm >= IAQ_CO2_ALARM_ASSERT_PPM) {
            alarmActive = true;
        } else if (alarmActive && reading.co2Ppm <= IAQ_CO2_ALARM_RELEASE_PPM) {
            alarmActive = false;
        }
    }

    reading.co2Alarm = alarmActive;
    digitalWrite(IAQ_ALARM_LED_PIN, alarmActive ? HIGH : LOW);
    digitalWrite(IAQ_BUZZER_PIN, alarmActive ? HIGH : LOW);
}

void sensorTask(void* queueParameter) {
    const QueueHandle_t queue = static_cast<QueueHandle_t>(queueParameter);
    const esp_err_t watchdogResult = esp_task_wdt_add(nullptr);
    if (watchdogResult != ESP_OK && watchdogResult != ESP_ERR_INVALID_STATE) {
        Serial.printf("Sensor task WDT registration error: %d\n",
                      static_cast<int>(watchdogResult));
    }

    const TickType_t periodTicks = pdMS_TO_TICKS(IAQ_SENSOR_PERIOD_MS);
    TickType_t lastWakeTime = xTaskGetTickCount();
    uint32_t sequence = 0;
    bool alarmActive = false;

    for (;;) {
        const uint32_t cycleStartedMs = millis();
        IaqTelemetry reading;
        sensors.poll(reading);
        reading.sequence = ++sequence;
        updateLocalAlarm(reading, alarmActive);

        // A one-element overwrite queue bounds memory and keeps only the newest sample.
        if (xQueueOverwrite(queue, &reading) != pdTRUE) {
            Serial.println("Telemetry queue write failed");
        }

        const uint32_t cycleDurationMs = millis() - cycleStartedMs;
        if (cycleDurationMs >= IAQ_SENSOR_PERIOD_MS) {
            Serial.printf("Sensor cycle overrun: %lu ms\n",
                          static_cast<unsigned long>(cycleDurationMs));
        }

        // GPIO updates above occur in the sensor task; no network operation can delay them.
        esp_task_wdt_reset();
        vTaskDelayUntil(&lastWakeTime, periodTicks);
    }
}
}  // namespace

void setup() {
    Serial.begin(115200);
    pinMode(IAQ_ALARM_LED_PIN, OUTPUT);
    pinMode(IAQ_BUZZER_PIN, OUTPUT);
    digitalWrite(IAQ_ALARM_LED_PIN, LOW);
    digitalWrite(IAQ_BUZZER_PIN, LOW);

    if (!sensors.begin()) {
        Serial.println("Warning: no environmental sensor initialized successfully");
    }

    telemetryQueue = xQueueCreate(1, sizeof(IaqTelemetry));
    if (telemetryQueue == nullptr) {
        Serial.println("Fatal: telemetry queue allocation failed");
        for (;;) {
            digitalWrite(IAQ_ALARM_LED_PIN, !digitalRead(IAQ_ALARM_LED_PIN));
            delay(250);
        }
    }

    const BaseType_t networkCreated = xTaskCreatePinnedToCore(
        networkTask,
        "network",
        kNetworkTaskStackBytes,
        telemetryQueue,
        kNetworkTaskPriority,
        nullptr,
        1);

    const BaseType_t sensorCreated = xTaskCreatePinnedToCore(
        sensorTask,
        "sensor",
        kSensorTaskStackBytes,
        telemetryQueue,
        kSensorTaskPriority,
        nullptr,
        0);

    if (networkCreated != pdPASS || sensorCreated != pdPASS) {
        Serial.println("Fatal: FreeRTOS task creation failed");
        digitalWrite(IAQ_ALARM_LED_PIN, HIGH);
        for (;;) {
            delay(1000);
        }
    }
}

void loop() {
    // Work belongs to the pinned application tasks; keep Arduino's loop task idle.
    vTaskDelay(pdMS_TO_TICKS(1000));
}