#include "pms5003.h"

void Pms5003::begin(HardwareSerial& serial, const int rxPin, const int txPin) {
    serial_ = &serial;
    serial_->begin(9600, SERIAL_8N1, rxPin, txPin);
}

bool Pms5003::poll(Pms5003Reading& reading) {
    if (serial_ == nullptr) {
        return false;
    }

    while (serial_->available() > 0) {
        const uint8_t value = static_cast<uint8_t>(serial_->read());

        if (frameIndex_ == 0) {
            if (value == 0x42) {
                frame_[frameIndex_++] = value;
            }
            continue;
        }

        if (frameIndex_ == 1 && value != 0x4D) {
            // Preserve a possible first header byte during stream resynchronization.
            frameIndex_ = value == 0x42 ? 1 : 0;
            if (frameIndex_ == 1) {
                frame_[0] = value;
            }
            continue;
        }

        frame_[frameIndex_++] = value;

        if (frameIndex_ == 4) {
            const uint16_t payloadLength =
                (static_cast<uint16_t>(frame_[2]) << 8U) | frame_[3];
            if (payloadLength != kPayloadLength) {
                frameIndex_ = 0;
            }
        }

        if (frameIndex_ == kFrameSize) {
            const bool valid = parseFrame(reading);
            frameIndex_ = 0;
            if (valid) {
                return true;
            }
        }
    }

    return false;
}

bool Pms5003::parseFrame(Pms5003Reading& reading) {
    uint16_t calculatedChecksum = 0;
    for (size_t index = 0; index < kFrameSize - 2; ++index) {
        calculatedChecksum += frame_[index];
    }

    const uint16_t receivedChecksum =
        (static_cast<uint16_t>(frame_[30]) << 8U) | frame_[31];
    if (calculatedChecksum != receivedChecksum) {
        return false;
    }

    const auto readWord = [this](const size_t offset) -> uint16_t {
        return (static_cast<uint16_t>(frame_[offset]) << 8U) | frame_[offset + 1];
    };

    reading.pm1Standard = readWord(4);
    reading.pm25Standard = readWord(6);
    reading.pm10Standard = readWord(8);
    reading.pm1Atmospheric = readWord(10);
    reading.pm25Atmospheric = readWord(12);
    reading.pm10Atmospheric = readWord(14);
    return true;
}