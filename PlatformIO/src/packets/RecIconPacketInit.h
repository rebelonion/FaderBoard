#pragma once

#include <Arduino.h>
#include "BasePacket.h"

class RecIconPacketInit final : public BasePacket {
public:
    explicit RecIconPacketInit(const uint8_t *_data) : BasePacket(_data) {
    }

    [[nodiscard]] __attribute__((always_inline)) uint32_t getPID() const {
        uint32_t value;
        memcpy(&value, data + Positions::ICON_PID_INDEX, sizeof(uint32_t));
        return value;
    }

    [[nodiscard]] __attribute__((always_inline)) uint32_t getPacketCount() const {
        uint32_t value;
        memcpy(&value, data + Positions::ICON_PACKET_COUNT_INDEX, sizeof(uint32_t));
        return value;
    }

    [[nodiscard]] __attribute__((always_inline)) uint32_t getByteCount() const {
        uint32_t value;
        memcpy(&value, data + Positions::ICON_BYTE_COUNT_INDEX, sizeof(uint32_t));
        return value;
    }

private:
    using Positions = PacketPositions::IconPacketInit;
};
