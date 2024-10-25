#pragma once

#include <Arduino.h>
#include "BasePacket.h"

class RecChannelData final : public BasePacket {
public:
    explicit RecChannelData(const uint8_t *_data) : BasePacket(_data) {
    }

    [[nodiscard]] __attribute__((always_inline)) bool isMaster() const {
        return data[Positions::IS_MASTER_INDEX] == 1;
    }

    [[nodiscard]] __attribute__((always_inline)) uint8_t getMaxVolume() const {
        return data[Positions::MAX_VOLUME_INDEX];
    }

    [[nodiscard]] __attribute__((always_inline)) bool isMuted() const {
        return data[Positions::IS_MUTED_INDEX] == 1;
    }

    [[nodiscard]] __attribute__((always_inline)) uint32_t getPID() const {
        uint32_t value;
        memcpy(&value, data + Positions::PID_INDEX, sizeof(uint32_t));
        return value;
    }

    void __attribute__((always_inline)) getName(char *name) const {
        memcpy(name, &data[Positions::NAME_INDEX], NAME_LENGTH_MAX);
    }

private:
    using Positions = PacketPositions::ChannelData;
};
