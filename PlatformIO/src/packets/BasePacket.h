#pragma once

#include <Arduino.h>

struct BasePacketPositions {
    static constexpr uint8_t VERSION_INDEX = 0;
    static constexpr uint8_t COUNT_INDEX = VERSION_INDEX + sizeof(uint8_t);
    static constexpr uint8_t STATUS_INDEX = COUNT_INDEX + sizeof(uint16_t);
    static constexpr uint8_t NEXT_FREE_INDEX = STATUS_INDEX + 1;
};

class BasePacket {
public:
    explicit BasePacket(const uint8_t *_data) : data(_data) {
    }

    ~BasePacket() = default;

    [[nodiscard]] __attribute__((always_inline)) uint8_t getVersion() const {
        return data[BasePacketPositions::VERSION_INDEX];
    }

    [[nodiscard]] __attribute__((always_inline)) uint16_t getCount() const {
        uint16_t value;
        memcpy(&value, data + BasePacketPositions::COUNT_INDEX, sizeof(uint16_t));
        return value;
    }

    [[nodiscard]] __attribute__((always_inline)) uint8_t getStatus() const {
        return data[BasePacketPositions::STATUS_INDEX];
    }
protected:
    const uint8_t *data;
};
