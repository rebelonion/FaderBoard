#pragma once

#include <Arduino.h>
#include "BasePacket.h"

class RecCurrentVolumeLevels final : public BasePacket {
public:
    explicit RecCurrentVolumeLevels(const uint8_t *_data) : BasePacket(_data) {
    }

    [[nodiscard]] __attribute__((always_inline)) uint32_t getPID(const uint8_t channelIndex) const {
        uint32_t value;
        memcpy(&value, data + Positions::PID_INDEX + channelIndex * Positions::CHANNEL_SIZE, sizeof(uint32_t));
        return value;
    }

    [[nodiscard]] __attribute__((always_inline)) uint8_t getVolume(const uint8_t channelIndex) const {
        return data[Positions::VOLUME_INDEX + channelIndex * Positions::CHANNEL_SIZE];
    }

private:
    using Positions = PacketPositions::RecCurrentVolumeLevels;
};