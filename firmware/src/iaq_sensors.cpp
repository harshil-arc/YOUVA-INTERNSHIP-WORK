#include "iaq_sensors.h"

#include "project_config.h"

namespace {
constexpr uint32_t kSensorFreshnessLimitMs = 5000;
constexpr uint16_t kScd30MeasurementIntervalSeconds = 2;
}  // namespace

bool IaqSensors::begin() {
    Wire.begin(IAQ_I2C_SDA_PIN, IAQ_I2C_SCL_PIN);
    Wire.setClock(100000);
    Wire.setTimeOut(50);

    scd30_.begin(Wire);
    const uint16_t intervalError =
        scd30_.setMeasurementInterval(kScd30MeasurementIntervalSeconds);
    const uint16_t startError = scd30_.startPeriodicMeasurement(0);
    scd30Started_ = intervalError == 0 && startError == 0;

    bme680Started_ = bme680_.begin(0x76, &Wire);
    if (!bme680Started_) {
        bme680Started_ = bme680_.begin(0x77, &Wire);
    }

    if (bme680Started_) {
        bme680_.setTemperatureOversampling(BME680_OS_8X);
        bme680_.setHumidityOversampling(BME680_OS_2X);
        bme680_.setPressureOversampling(BME680_OS_4X);
        bme680_.setIIRFilterSize(BME680_FILTER_SIZE_3);
        bme680_.setGasHeater(320, 150);
    }

    pms5003_.begin(Serial2, IAQ_PMS_UART_RX_PIN, IAQ_PMS_UART_TX_PIN);
    return scd30Started_ || bme680Started_;
}

void IaqSensors::poll(IaqTelemetry& telemetry) {
    const uint32_t now = millis();

    if (scd30Started_) {
        uint16_t dataReady = 0;
        if (scd30_.getDataReadyFlag(dataReady) == 0 && dataReady != 0) {
            float co2Ppm = 0.0F;
            float temperatureC = 0.0F;
            float humidityPercent = 0.0F;
            if (scd30_.readMeasurementData(co2Ppm, temperatureC, humidityPercent) == 0 &&
                co2Ppm > 0.0F && humidityPercent >= 0.0F && humidityPercent <= 100.0F) {
                lastReading_.co2Ppm = co2Ppm;
                lastReading_.scd30TemperatureC = temperatureC;
                lastReading_.scd30HumidityPercent = humidityPercent;
                lastScd30Ms_ = now;
                scd30HasReading_ = true;
            }
        }
    }

    if (bme680Started_ && bme680_.performReading()) {
        lastReading_.bmeTemperatureC = bme680_.temperature;
        lastReading_.bmeHumidityPercent = bme680_.humidity;
        lastReading_.pressureHpa = bme680_.pressure / 100.0F;
        lastReading_.gasResistanceOhms = static_cast<float>(bme680_.gas_resistance);
        lastBme680Ms_ = now;
        bme680HasReading_ = true;
    }

    Pms5003Reading particulate;
    if (pms5003_.poll(particulate)) {
        lastReading_.pm1Standard = particulate.pm1Standard;
        lastReading_.pm25Standard = particulate.pm25Standard;
        lastReading_.pm10Standard = particulate.pm10Standard;
        lastReading_.pm1Atmospheric = particulate.pm1Atmospheric;
        lastReading_.pm25Atmospheric = particulate.pm25Atmospheric;
        lastReading_.pm10Atmospheric = particulate.pm10Atmospheric;
        lastPms5003Ms_ = now;
        pms5003HasReading_ = true;
    }

    telemetry = lastReading_;
    telemetry.uptimeMs = now;
    telemetry.scd30Valid = scd30HasReading_ && (now - lastScd30Ms_) <= kSensorFreshnessLimitMs;
    telemetry.bme680Valid = bme680HasReading_ && (now - lastBme680Ms_) <= kSensorFreshnessLimitMs;
    telemetry.pms5003Valid = pms5003HasReading_ && (now - lastPms5003Ms_) <= kSensorFreshnessLimitMs;
}