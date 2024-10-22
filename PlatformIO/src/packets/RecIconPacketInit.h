#pragma once

#include <Arduino.h>
#include "BasePacket.h"

class RecIconPacketInit final : public BasePacket {
public:
    explicit RecIconPacketInit(const uint8_t *_data) : BasePacket(_data) {
    }

    [[nodiscard]] __attribute__((always_inline)) uint32_t getPID() const {
        uint32_t value;
        memcpy(&value, data + ICON_PID_INDEX, sizeof(uint32_t));
        return __REV(value);
    }

    [[nodiscard]] __attribute__((always_inline)) uint32_t getPacketCount() const {
        uint32_t value;
        memcpy(&value, data + ICON_PACKET_COUNT_INDEX, sizeof(uint32_t));
        return __REV(value);
    }


private:
    static constexpr uint8_t ICON_PID_INDEX = NEXT_FREE_INDEX;
    static constexpr uint8_t ICON_PACKET_COUNT_INDEX = ICON_PID_INDEX + sizeof(uint32_t);
};
