#pragma once

#include <Arduino.h>
#include "BasePacket.h"

class RecMaxChange final : public BasePacket {
public:
    explicit RecMaxChange(const uint8_t *_data) : BasePacket(_data) {
    }

    [[nodiscard]] __attribute__((always_inline)) uint32_t getPID() const {
        uint32_t value;
        memcpy(&value, data + PID_INDEX, sizeof(uint32_t));
        return __REV(value);
    }

    [[nodiscard]] __attribute__((always_inline)) uint8_t getMaxVolume() const {
        return data[MAX_VOLUME_INDEX];
    }

    [[nodiscard]] __attribute__((always_inline)) bool isMuted() const {
        return data[IS_MUTED_INDEX] == 1;
    }

private:
    static constexpr uint8_t PID_INDEX = NEXT_FREE_INDEX;
    static constexpr uint8_t MAX_VOLUME_INDEX = PID_INDEX + sizeof(uint32_t);
    static constexpr uint8_t IS_MUTED_INDEX = MAX_VOLUME_INDEX + sizeof(uint8_t);
};
