#pragma once

#include "BasePacket.h"

#include "Globals.h"
#include <Arduino.h>

class RecNewPID final : public BasePacket {
public:
    explicit RecNewPID(const uint8_t *_data) : BasePacket(_data) {
    }

    [[nodiscard]] __attribute__((always_inline)) uint32_t getPID() const {
        uint32_t value;
        memcpy(&value, data + PID_INDEX, sizeof(uint32_t));
        return __REV(value);
    }

    void __attribute__((always_inline)) getName(char *name) const {
        memcpy(name, &data[NAME_INDEX], NAME_LENGTH_MAX);
    }

    [[nodiscard]] __attribute__((always_inline)) uint8_t getVolume() const {
        return data[VOL_INDEX];
    }

    [[nodiscard]] __attribute__((always_inline)) bool isMuted() const {
        return data[MUTE_INDEX] == 1;
    }

private:
    static constexpr uint8_t PID_INDEX = NEXT_FREE_INDEX;
    static constexpr uint8_t NAME_INDEX = PID_INDEX + sizeof(uint32_t);
    static constexpr uint8_t VOL_INDEX = NAME_INDEX + NAME_LENGTH_MAX;
    static constexpr uint8_t MUTE_INDEX = VOL_INDEX + sizeof(uint8_t);
};