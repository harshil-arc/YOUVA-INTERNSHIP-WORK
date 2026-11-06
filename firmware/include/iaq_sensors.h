#pragma once

#include <Adafruit_BME680.h>
#include <SensirionI2CScd30.h>
#include <Wire.h>

#include "pms5003.h"
#include "telemetry.h"

// Owns sensor drivers and publishes the most recent validated readings.
class IaqSensors {
public:
    bool begin();
    void poll(IaqTelemetry& telemetry);

private:
    SensirionI2CScd30 scd30_;
    Adafruit_BME680 bme680_;
    Pms5003 pms5003_;

    bool scd30Started_ = false;
    bool bme680Started_ = false;
    bool scd30HasReading_ = false;
    bool bme680HasReading_ = false;
    bool pms5003HasReading_ = false;

    uint32_t lastScd30Ms_ = 0;
    uint32_t lastBme680Ms_ = 0;
    uint32_t lastPms5003Ms_ = 0;

    IaqTelemetry lastReading_;
};