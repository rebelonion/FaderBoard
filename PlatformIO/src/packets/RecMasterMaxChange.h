#pragma once

#include <Arduino.h>
#include "BasePacket.h"

class RecMasterMaxChange final : public BasePacket {
public:
    explicit RecMasterMaxChange(const uint8_t *_data) : BasePacket(_data) {
    }

    [[nodiscard]] __attribute__((always_inline)) uint8_t getMasterMaxVolume() const {
        return data[MASTER_MAX_VOLUME_INDEX];
    }

    [[nodiscard]] __attribute__((always_inline)) bool isMasterMuted() const {
        return data[MATER_MAX_MUTE_INDEX] == 1;
    }

private:
    static constexpr uint8_t MASTER_MAX_VOLUME_INDEX = NEXT_FREE_INDEX;
    static constexpr uint8_t MATER_MAX_MUTE_INDEX = MASTER_MAX_VOLUME_INDEX + sizeof(uint8_t);
};