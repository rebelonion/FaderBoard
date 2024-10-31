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

    void sendAcknowledge(const uint8_t ackPacket, const AckType type) {
        using Packet = PacketPositions::AcknowledgePacket;
        preparePacket();
        packet[Base::STATUS_INDEX] = ACK;
        packet[Packet::ACK_PACKET_INDEX] = ackPacket;
        packet[Packet::ACK_TYPE_INDEX] = type;
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

    void sendChannelData(const bool isMaster, const uint8_t maxVolume, const bool isMuted, const uint32_t PID,
                         const char name[NAME_LENGTH_MAX]) {
        using Packet = PacketPositions::ChannelData;
        preparePacket();
        packet[Base::STATUS_INDEX] = CHANNEL_DATA;
        packet[Packet::IS_MASTER_INDEX] = isMaster;
        packet[Packet::MAX_VOLUME_INDEX] = maxVolume;
        packet[Packet::IS_MUTED_INDEX] = isMuted;
        memcpy(packet + Packet::PID_INDEX, &PID, sizeof(uint32_t));
        memcpy(packet + Packet::NAME_INDEX, name, NAME_LENGTH_MAX);
        sendPacket();
    }

    void sendCurrentSelectedProcesses(const uint32_t *PIDs, const uint8_t count) {
        if (count == 0) {
            return;
        }
        Serial.println("Sending " + String(count) + " selected processes");
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
        Serial.println("Sending packet: " + String(packet[Base::STATUS_INDEX]));
        if (const int32_t result = RawHID.send(packet, 0); result <= 0) {
            Serial.println("Failed to send packet: " + String(result));
        }
    }
};
