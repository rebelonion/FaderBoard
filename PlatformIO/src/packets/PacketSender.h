#pragma once

#include <Arduino.h>
#include "Globals.h"

class PacketSender{
public:
    PacketSender() = default;
    ~PacketSender() = default;

    void __attribute__((always_inline)) incrementCounter() {
        counter++;
    }

    void sendAcknowledge(const uint8_t ackPacket) {
        preparePacket();
        packet[BasePacketPositions::STATUS_INDEX] = ACK;
        packet[BasePacketPositions::NEXT_FREE_INDEX] = ackPacket;
        sendPacket();
    }

    void sendStopNormalBroadcasts() {
        preparePacket();
        packet[BasePacketPositions::STATUS_INDEX] = STOP_NORMAL_BROADCASTS;
        sendPacket();
    }

    void sendStartNormalBroadcasts() {
        preparePacket();
        packet[BasePacketPositions::STATUS_INDEX] = START_NORMAL_BROADCASTS;
        sendPacket();
    }

    void sendReady() {
        preparePacket();
        packet[BasePacketPositions::STATUS_INDEX] = READY;
        sendPacket();
    }

    void sendRequestChannelData(const uint32_t PID) {
        preparePacket();
        packet[BasePacketPositions::STATUS_INDEX] = REQUEST_CHANNEL_DATA;
        memcpy(packet + BasePacketPositions::NEXT_FREE_INDEX, &PID, sizeof(uint32_t));
        sendPacket();
    }

    void sendChangeOfChannel(const uint32_t PID, const uint8_t volume, const bool muted) {
        preparePacket();
        packet[BasePacketPositions::STATUS_INDEX] = CHANGE_OF_MAX_VOLUME;
        uint8_t i = BasePacketPositions::NEXT_FREE_INDEX;
        memcpy(packet + i, &PID, sizeof(uint32_t));
        i += sizeof(uint32_t);
        memcpy(packet + i, &volume, sizeof(uint8_t));
        i += sizeof(uint8_t);
        packet[i] = muted;
        sendPacket();
    }

    void sendChangeOfMasterChannel(const uint8_t volume, const bool muted) {
        preparePacket();
        packet[BasePacketPositions::STATUS_INDEX] = CHANGE_OF_MASTER_MAX;
        uint8_t i = BasePacketPositions::NEXT_FREE_INDEX;
        memcpy(packet + i, &volume, sizeof(uint8_t));
        i += sizeof(uint8_t);
        packet[i] = muted;
        sendPacket();
    }

    void sendCurrentSelectedProcesses(const uint32_t* PIDs, const uint8_t count) {
        preparePacket();
        packet[BasePacketPositions::STATUS_INDEX] = CURRENT_SELECTED_PROCESSES;
        uint8_t i = BasePacketPositions::NEXT_FREE_INDEX;
        memcpy(packet + i, &count, sizeof(uint8_t));
        i += sizeof(uint8_t);
        memcpy(packet + i, PIDs, count * sizeof(uint32_t));
        sendPacket();
    }

    void sendRequestIcon(const uint32_t PID) {
        preparePacket();
        packet[BasePacketPositions::STATUS_INDEX] = REQUEST_ICON;
        memcpy(packet + BasePacketPositions::NEXT_FREE_INDEX, &PID, sizeof(uint32_t));
        sendPacket();
    }

    void sendRequestAllProcesses() {
        preparePacket();
        packet[BasePacketPositions::STATUS_INDEX] = REQUEST_ALL_PROCESSES;
        sendPacket();
    }

private:
    uint8_t counter = 0;
    uint8_t packet[PACKET_SIZE]{};

    void __attribute__((always_inline)) preparePacket() {
        incrementCounter();
        packet[BasePacketPositions::VERSION_INDEX] = API_VERSION;
        packet[BasePacketPositions::COUNT_INDEX] = counter;
    }

    __attribute__((always_inline)) void sendPacket() const {
        // 0 is timeout, -1 is usb not available, > 0 is success
        if (const int32_t result = usb_rawhid_send(packet, TIMEOUT); result <= 0) {
            Serial.println("Failed to send packet: " + String(result));
        }
    }
};