#pragma once

#include <Arduino.h>
#include "PacketPositions.h"


class BasePacket {
public:
    explicit BasePacket(const uint8_t *_data) : data(_data) {
    }

    ~BasePacket() = default;

    [[nodiscard]] __attribute__((always_inline)) uint8_t getVersion() const {
        return data[Positions::VERSION_INDEX];
    }

    [[nodiscard]] __attribute__((always_inline)) uint16_t getCount() const {
        uint16_t value;
        memcpy(&value, data + Positions::COUNT_INDEX, sizeof(uint16_t));
        return value;
    }

    [[nodiscard]] __attribute__((always_inline)) uint8_t getStatus() const {
        return data[Positions::STATUS_INDEX];
    }
private:
    using Positions = PacketPositions::Base;
protected:
    const uint8_t *data;
};
