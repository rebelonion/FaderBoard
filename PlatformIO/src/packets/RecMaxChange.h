#pragma once

#include <Arduino.h>
#include "BasePacket.h"

class RecMaxChange final : public BasePacket {
public:
    explicit RecMaxChange(const uint8_t *_data) : BasePacket(_data) {
    }

    [[nodiscard]] __attribute__((always_inline)) uint32_t getPID() const {
        uint32_t value;
        memcpy(&value, data + Process::PID_INDEX, sizeof(uint32_t));
        return value;
    }

    [[nodiscard]] __attribute__((always_inline)) uint8_t getMaxVolume() const {
        return data[Process::MAX_VOLUME_INDEX];
    }

    [[nodiscard]] __attribute__((always_inline)) bool isMuted() const {
        return data[Process::IS_MUTED_INDEX] == 1;
    }

private:
    using Process = PacketPositions::MaxChange;
};
