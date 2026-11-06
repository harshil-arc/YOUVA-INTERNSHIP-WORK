#pragma once

#include <Arduino.h>

// Plain-data snapshot copied through a length-one FreeRTOS queue.
// Keep this type trivially copyable; it must not own heap memory.
struct IaqTelemetry {
    uint32_t sequence = 0;
    uint32_t uptimeMs = 0;

    float co2Ppm = 0.0F;
    float scd30TemperatureC = 0.0F;
    float scd30HumidityPercent = 0.0F;

    float bmeTemperatureC = 0.0F;
    float bmeHumidityPercent = 0.0F;
    float pressureHpa = 0.0F;
    float gasResistanceOhms = 0.0F;

    uint16_t pm1Standard = 0;
    uint16_t pm25Standard = 0;
    uint16_t pm10Standard = 0;
    uint16_t pm1Atmospheric = 0;
    uint16_t pm25Atmospheric = 0;
    uint16_t pm10Atmospheric = 0;

    bool scd30Valid = false;
    bool bme680Valid = false;
    bool pms5003Valid = false;
    bool co2Alarm = false;
};