#pragma once

#include <Arduino.h>

class RecProcessRequestInit final : public BasePacket {
public:
  explicit RecProcessRequestInit(const uint8_t *_data) : BasePacket(_data) {
  }

  [[nodiscard]] __attribute__((always_inline)) uint8_t getNumChannels() const {
    return data[NUMBER_OF_CHANNELS_INDEX];
  }

  [[nodiscard]] __attribute__((always_inline)) uint8_t getNumPackets() const {
    return data[NUMBER_OF_PACKETS_INDEX];
  }

private:
  static constexpr uint8_t NUMBER_OF_CHANNELS_INDEX = BasePacketPositions::NEXT_FREE_INDEX;
  static constexpr uint8_t NUMBER_OF_PACKETS_INDEX = NUMBER_OF_CHANNELS_INDEX + sizeof(uint8_t);
};
