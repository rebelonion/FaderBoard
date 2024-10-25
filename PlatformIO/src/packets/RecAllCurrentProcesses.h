#pragma once

#include <Arduino.h>
#include "BasePacket.h"

class RecAllCurrentProcesses final : public BasePacket {
public:
    explicit RecAllCurrentProcesses(const uint8_t *_data) : BasePacket(_data) {
    }

    [[nodiscard]] __attribute__((always_inline)) uint32_t getPID() const {
        uint32_t value;
        memcpy(&value, data + Positions::PID_INDEX, sizeof(uint32_t));
        return value;
    }

    void __attribute__((always_inline)) getName(char *name) const {
        memcpy(name, &data[Positions::NAME_INDEX], NAME_LENGTH_MAX);
    }

    [[nodiscard]] __attribute__((always_inline)) uint32_t getPID2() const {
        uint32_t value;
        memcpy(&value, data + Positions::PID_INDEX_2, sizeof(uint32_t));
        return value;
    }

    void __attribute__((always_inline)) getName2(char *name) const {
        memcpy(name, &data[Positions::NAME_INDEX_2], NAME_LENGTH_MAX);
    }
private:
    using Positions = PacketPositions::AllCurrentProcesses;
};