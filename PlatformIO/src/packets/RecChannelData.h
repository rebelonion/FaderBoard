#pragma once

#include <Arduino.h>
#include "BasePacket.h"

class RecChannelData final : public BasePacket {
public:
    explicit RecChannelData(const uint8_t *_data) : BasePacket(_data) {
    }

    [[nodiscard]] __attribute__((always_inline)) bool isMaster() const {
        return data[IS_MASTER_INDEX] == 1;
    }

    [[nodiscard]] __attribute__((always_inline)) uint8_t getMaxVolume() const {
        return data[MAX_VOLUME_INDEX];
    }

    [[nodiscard]] __attribute__((always_inline)) bool isMuted() const {
        return data[IS_MUTED_INDEX] == 1;
    }

    [[nodiscard]] __attribute__((always_inline)) uint32_t getPID() const {
        uint32_t value;
        memcpy(&value, data + PID_INDEX, sizeof(uint32_t));
        return __REV(value);
    }

    void __attribute__((always_inline)) getName(char *name) const {
        memcpy(name, &data[NAME_INDEX], NAME_LENGTH_MAX);
    }

private:
    static constexpr uint8_t IS_MASTER_INDEX = NEXT_FREE_INDEX;
    static constexpr uint8_t MAX_VOLUME_INDEX = IS_MASTER_INDEX + sizeof(uint8_t);
    static constexpr uint8_t IS_MUTED_INDEX = MAX_VOLUME_INDEX + sizeof(uint8_t);
    static constexpr uint8_t PID_INDEX = IS_MUTED_INDEX + sizeof(uint8_t);
    static constexpr uint8_t NAME_INDEX = PID_INDEX + sizeof(uint32_t);
};
