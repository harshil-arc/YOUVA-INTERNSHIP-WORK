#pragma once

#include <Arduino.h>

// Local credentials are provided by an ignored secrets.h, never committed.
#if __has_include("secrets.h")
#include "secrets.h"
#endif

#ifndef IAQ_WIFI_SSID
#define IAQ_WIFI_SSID ""
#endif

#ifndef IAQ_WIFI_PASSWORD
#define IAQ_WIFI_PASSWORD ""
#endif

#ifndef IAQ_MQTT_HOST
#define IAQ_MQTT_HOST ""
#endif

#ifndef IAQ_MQTT_PORT
#define IAQ_MQTT_PORT 8883
#endif

#ifndef IAQ_MQTT_USERNAME
#define IAQ_MQTT_USERNAME ""
#endif

#ifndef IAQ_MQTT_PASSWORD
#define IAQ_MQTT_PASSWORD ""
#endif

#ifndef IAQ_MQTT_ROOT_CA
#define IAQ_MQTT_ROOT_CA ""
#endif

#ifndef IAQ_DEVICE_ID
#define IAQ_DEVICE_ID "iaq-esp32-dev"
#endif

// Sensor and network cadence targets. Network work never runs in the sensor task.
#define IAQ_SENSOR_PERIOD_MS 2000UL
#define IAQ_MQTT_PUBLISH_PERIOD_MS 60000UL

// Example configurable thresholds; not medical or life-safety limits.
#define IAQ_CO2_ALARM_ASSERT_PPM 1200.0F
#define IAQ_CO2_ALARM_RELEASE_PPM 1000.0F

// Reference ESP32-WROOM-32 development-board pin map.
#define IAQ_I2C_SDA_PIN 21
#define IAQ_I2C_SCL_PIN 22
#define IAQ_PMS_UART_RX_PIN 16
#define IAQ_PMS_UART_TX_PIN 17
#define IAQ_ALARM_LED_PIN 26
#define IAQ_BUZZER_PIN 27
