#pragma once

#include <Arduino.h>
#include "BasePacket.h"

class RecIconPacket final : public BasePacket {
public:
    explicit RecIconPacket(const uint8_t *_data) : BasePacket(_data) {
    }

    __attribute__((always_inline)) void emplaceIconData() const {
        memcpy(&compressionBuffer[compressionIndex], data + Positions::ICON_INDEX, Positions::NUM_ICON_BYTES_SENT);
        compressionIndex += Positions::NUM_ICON_BYTES_SENT;
    }

private:
    using Positions = PacketPositions::IconPacket;
};
