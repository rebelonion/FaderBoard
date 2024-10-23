#pragma once

#include <Arduino.h>
#include "BasePacket.h"

class RecIconPacket final : public BasePacket {
public:
    explicit RecIconPacket(const uint8_t *_data) : BasePacket(_data) {
    }

    __attribute__((always_inline)) void emplaceIconData() const {
        memcpy(&compressionBuffer[compressionIndex], data + ICON_INDEX, NUM_ICON_BYTES_SENT);
        compressionIndex += NUM_ICON_BYTES_SENT;
    }

private:
    static constexpr uint8_t ICON_INDEX = BasePacketPositions::NEXT_FREE_INDEX;
    static constexpr uint8_t NUM_ICON_BYTES_SENT = PACKET_SIZE - ICON_INDEX;
};