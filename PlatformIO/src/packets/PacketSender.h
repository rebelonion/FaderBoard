#pragma once

#include <Arduino.h>
#include "Globals.h"
#include "PacketPositions.h"

class PacketSender {
public:
    PacketSender() = default;

    ~PacketSender() = default;

    void __attribute__((always_inline)) incrementCounter() {
        counter++;
    }

    void sendAcknowledge(const uint8_t ackPacket) {
        using Packet = PacketPositions::AcknowledgePacket;
        preparePacket();
        packet[Base::STATUS_INDEX] = ACK;
        packet[Packet::ACK_PACKET_INDEX] = ackPacket;
        sendPacket();
    }

    void sendStopNormalBroadcasts() {
        preparePacket();
        packet[Base::STATUS_INDEX] = STOP_NORMAL_BROADCASTS;
        sendPacket();
    }

    void sendStartNormalBroadcasts() {
        preparePacket();
        packet[Base::STATUS_INDEX] = START_NORMAL_BROADCASTS;
        sendPacket();
    }

    void sendRequestChannelData(const uint32_t PID) {
        using Packet = PacketPositions::RequestChannelData;
        preparePacket();
        packet[Base::STATUS_INDEX] = REQUEST_CHANNEL_DATA;
        memcpy(packet + Packet::PID_INDEX, &PID, sizeof(uint32_t));
        sendPacket();
    }

    void sendChangeOfChannel(const uint32_t PID, const uint8_t volume, const bool muted) {
        using Packet = PacketPositions::ChangeOfChannel;
        preparePacket();
        packet[Base::STATUS_INDEX] = CHANGE_OF_MAX_VOLUME;
        memcpy(packet + Packet::PID_INDEX, &PID, sizeof(uint32_t));;
        memcpy(packet + Packet::VOLUME_INDEX, &volume, sizeof(uint8_t));
        packet[Packet::MUTED_INDEX] = muted;
        sendPacket();
    }

    void sendChangeOfMasterChannel(const uint8_t volume, const bool muted) {
        using Packet = PacketPositions::ChangeOfMasterChannel;
        preparePacket();
        packet[Base::STATUS_INDEX] = CHANGE_OF_MASTER_MAX;
        memcpy(packet + Packet::VOLUME_INDEX, &volume, sizeof(uint8_t));
        packet[Packet::MUTED_INDEX] = muted;
        sendPacket();
    }

    void sendCurrentSelectedProcesses(const uint32_t *PIDs, const uint8_t count) {
        using Packet = PacketPositions::CurrentSelectedProcesses;
        preparePacket();
        packet[Base::STATUS_INDEX] = CURRENT_SELECTED_PROCESSES;
        memcpy(packet + Packet::COUNT_INDEX, &count, sizeof(uint8_t));
        memcpy(packet + Packet::PIDS_INDEX, PIDs, count * sizeof(uint32_t));
        sendPacket();
    }

    void sendRequestIcon(const uint32_t PID) {
        using Packet = PacketPositions::RequestIcon;
        preparePacket();
        packet[Base::STATUS_INDEX] = REQUEST_ICON;
        memcpy(packet + Packet::PID_INDEX, &PID, sizeof(uint32_t));
        sendPacket();
    }

    void sendRequestAllProcesses() {
        preparePacket();
        packet[Base::STATUS_INDEX] = REQUEST_ALL_PROCESSES;
        sendPacket();
    }

private:
    using Base = PacketPositions::Base;
    uint8_t counter = 0;
    uint8_t packet[PACKET_SIZE]{};

    void __attribute__((always_inline)) preparePacket() {
        incrementCounter();
        packet[Base::VERSION_INDEX] = API_VERSION;
        packet[Base::COUNT_INDEX] = counter;
    }

    __attribute__((always_inline)) void sendPacket() const {
        // 0 is timeout, -1 is usb not available, > 0 is success
        if (const int32_t result = usb_rawhid_send(packet, TIMEOUT); result <= 0) {
            Serial.println("Failed to send packet: " + String(result));
        }
    }
};
