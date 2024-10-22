#pragma once

#include <Arduino.h>
#include "BasePacket.h"

class RecPIDClosed final : public BasePacket {
public:
    explicit RecPIDClosed(const uint8_t *_data) : BasePacket(_data) {
    }

    [[nodiscard]] __attribute__((always_inline)) uint32_t getPID() const {
        uint32_t value;
        memcpy(&value, data + PID_INDEX, sizeof(uint32_t));
        return value;
    }

private:
    static constexpr uint8_t PID_INDEX = BasePacketPositions::NEXT_FREE_INDEX;
};