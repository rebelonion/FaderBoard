#pragma once

#include <Arduino.h>
#include "BasePacket.h"

class RecMasterMaxChange final : public BasePacket {
public:
    explicit RecMasterMaxChange(const uint8_t *_data) : BasePacket(_data) {
    }

    [[nodiscard]] __attribute__((always_inline)) uint8_t getMasterMaxVolume() const {
        return data[Positions::MASTER_MAX_VOLUME_INDEX];
    }

    [[nodiscard]] __attribute__((always_inline)) bool isMasterMuted() const {
        return data[Positions::MASTER_MAX_MUTE_INDEX] == 1;
    }

private:
    using Positions = PacketPositions::MasterMaxChange;
};