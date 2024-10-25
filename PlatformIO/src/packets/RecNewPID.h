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
        memcpy(&value, data + Process::PID_INDEX, sizeof(uint32_t));
        return value;
    }

    void __attribute__((always_inline)) getName(char *name) const {
        memcpy(name, &data[Process::NAME_INDEX], NAME_LENGTH_MAX);
    }

    [[nodiscard]] __attribute__((always_inline)) uint8_t getVolume() const {
        return data[Process::VOL_INDEX];
    }

    [[nodiscard]] __attribute__((always_inline)) bool isMuted() const {
        return data[Process::MUTE_INDEX] == 1;
    }

private:
    using Process = PacketPositions::NewPID;
};