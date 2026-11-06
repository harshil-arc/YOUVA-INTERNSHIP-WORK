#pragma once

#include <Arduino.h>

struct Pms5003Reading {
    uint16_t pm1Standard = 0;
    uint16_t pm25Standard = 0;
    uint16_t pm10Standard = 0;
    uint16_t pm1Atmospheric = 0;
    uint16_t pm25Atmospheric = 0;
    uint16_t pm10Atmospheric = 0;
};

// Incremental PMS5003 frame parser; poll() never waits for a complete UART frame.
class Pms5003 {
public:
    void begin(HardwareSerial& serial, int rxPin, int txPin);
    bool poll(Pms5003Reading& reading);

private:
    static constexpr size_t kFrameSize = 32;
    static constexpr uint16_t kPayloadLength = 28;

    HardwareSerial* serial_ = nullptr;
    uint8_t frame_[kFrameSize] = {};
    size_t frameIndex_ = 0;

    bool parseFrame(Pms5003Reading& reading);
};