#pragma once

#include <Arduino.h>
#include "BasePacket.h"

class RecCurrentVolumeLevels final : public BasePacket {
public:
    explicit RecCurrentVolumeLevels(const uint8_t *_data) : BasePacket(_data) {
    }

    [[nodiscard]] __attribute__((always_inline)) uint32_t getPID(const uint8_t channelIndex) const {
        uint32_t value;
        memcpy(&value, data + PID_INDEX + channelIndex * ChannelData::SIZE, sizeof(uint32_t));
        return __REV(value);
    }

    [[nodiscard]] __attribute__((always_inline)) uint8_t getVolume(const uint8_t channelIndex) const {
        return data[VOLUME_INDEX + channelIndex * ChannelData::SIZE];
    }

private:
    static constexpr uint8_t PID_INDEX = NEXT_FREE_INDEX;
    static constexpr uint8_t VOLUME_INDEX = PID_INDEX + sizeof(uint32_t);
    struct ChannelData {
        static constexpr size_t SIZE = sizeof(uint32_t) + sizeof(uint8_t);  // PID + volume
    };
};