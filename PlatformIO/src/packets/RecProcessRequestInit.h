#pragma once

#include <Arduino.h>
#include "BasePacket.h"

class RecProcessRequestInit final : public BasePacket {
public:
  explicit RecProcessRequestInit(const uint8_t *_data) : BasePacket(_data) {
  }

  [[nodiscard]] __attribute__((always_inline)) uint8_t getNumChannels() const {
    return data[Process::NUMBER_OF_CHANNELS_INDEX];
  }

private:
  using Process = PacketPositions::ProcessRequestInit;
};
