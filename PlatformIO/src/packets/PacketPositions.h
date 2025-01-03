#pragma once

#include <cstdint>
#include "Globals.h"

namespace PacketPositions {
    /**
     * @brief Base packet field positions shared by all packet types
     *
     * Memory layout:
     * [VERSION 1B][COUNT 2B][STATUS 1B]
     *
     * All derived packet structures should start their fields at NEXT_FREE_INDEX
     */
    struct Base {
        /// Protocol version number (1 byte)
        static constexpr uint8_t VERSION_INDEX = 0;

        /// Number of items in packet (2 bytes)
        static constexpr uint8_t COUNT_INDEX = VERSION_INDEX + sizeof(uint8_t);

        /// Status/type field (1 byte)
        static constexpr uint8_t STATUS_INDEX = COUNT_INDEX + sizeof(uint16_t);

        /// First available index for derived packet fields
        static constexpr uint8_t NEXT_FREE_INDEX = STATUS_INDEX + 1;
    };

    /**
     * @brief Field positions for AllCurrentProcesses packet (C2F)
     *
     * Memory layout:
     * [Base Headers][PID 4B][NAME 20B][PID2 4B][NAME2 20B]
     *
     * Used to retrieve information about current processes.
     * Contains details for one or two processes.
     */
    struct AllCurrentProcesses {
        /// Process ID for first process (4 bytes)
        static constexpr uint8_t PID_INDEX = Base::NEXT_FREE_INDEX;

        /// Name of first process (NAME_LENGTH_MAX bytes)
        static constexpr uint8_t NAME_INDEX = PID_INDEX + sizeof(uint32_t);

        /// Process ID for second process (4 bytes)
        static constexpr uint8_t PID_INDEX_2 = NAME_INDEX + NAME_LENGTH_MAX;

        /// Name of second process (NAME_LENGTH_MAX bytes)
        static constexpr uint8_t NAME_INDEX_2 = PID_INDEX_2 + sizeof(uint32_t);
    };

    /**
     * @brief Field positions for ChannelData packet (C2F)
     *
     * Memory layout:
     * [Base Headers][IS_MASTER 1B][MAX_VOLUME 1B][IS_MUTED 1B][PID 4B][NAME 20B]
     *
     * Used to retrieve information about an audio channel.
     * Contains channel properties including master status, volume settings, and process details.
     */
    struct ChannelData {
        /// Master channel flag (1 byte - boolean)
        static constexpr uint8_t IS_MASTER_INDEX = Base::NEXT_FREE_INDEX;

        /// Maximum volume level (1 byte)
        static constexpr uint8_t MAX_VOLUME_INDEX = IS_MASTER_INDEX + sizeof(uint8_t);

        /// Mute status flag (1 byte - boolean)
        static constexpr uint8_t IS_MUTED_INDEX = MAX_VOLUME_INDEX + sizeof(uint8_t);

        /// Process ID associated with the channel (4 bytes)
        static constexpr uint8_t PID_INDEX = IS_MUTED_INDEX + sizeof(uint8_t);

        /// Name of the process (NAME_LENGTH_MAX bytes)
        static constexpr uint8_t NAME_INDEX = PID_INDEX + sizeof(uint32_t);
    };

    /**
     * @brief Field positions for CurrentVolumeLevels packet (C2F)
     *
     * Memory layout for each channel (repeats up to 7 times):
     * [Base Headers][Num Ch 1B][CH0_PID 4B][CH0_VOL 1B][CH1_PID 4B][CH1_VOL 1B]...[CH6_PID 4B][CH6_VOL 1B]
     *
     * Used to retrieve current volume levels for up to 7 audio channels.
     * Each channel entry contains its process ID and current volume level.
     */
    struct CurrentVolumeLevels {
        /// Size of data for each channel (PID + volume)
        static constexpr uint32_t CHANNEL_SIZE = sizeof(uint32_t) + sizeof(uint8_t);

        /// Number of channels to receive volume data for (1 byte)
        static constexpr uint8_t NUM_CHANNELS_INDEX = Base::NEXT_FREE_INDEX;

        /// Process ID for a channel (4 bytes)
        /// Access with: PID_INDEX + (channelIndex * CHANNEL_SIZE)
        static constexpr uint8_t PID_INDEX = NUM_CHANNELS_INDEX + sizeof(uint8_t);

        /// Volume level for a channel (1 byte)
        /// Access with: VOLUME_INDEX + (channelIndex * CHANNEL_SIZE)
        static constexpr uint8_t VOLUME_INDEX = PID_INDEX + sizeof(uint32_t);
    };

    /**
     * @brief Field positions for IconPacket packet (C2F)
     *
     * Memory layout:
     * [Base Headers][ICON_DATA (PACKET_SIZE - Base Headers)B]
     *
     * Used to receive chunks of icon data that will be stored in a compression buffer.
     * The icon data fills all remaining space in the packet after base headers.
     */
    struct IconPacket {
        /// Starting index of icon data bytes
        static constexpr uint8_t ICON_INDEX = Base::NEXT_FREE_INDEX;

        /// Number of icon bytes in this packet
        /// Fills remaining packet space after base headers
        static constexpr uint8_t NUM_ICON_BYTES_SENT = PACKET_SIZE - ICON_INDEX;
    };

    /**
     * @brief Field positions for IconPacketInit packet (C2F)
     *
     * Memory layout:
     * [Base Headers][PID 4B][PACKET_COUNT 4B]
     *
     * Used as the initial packet for icon data transfer.
     * Contains the process ID and the total number of icon packets that will follow.
     */
    struct IconPacketInit {
        /// Process ID associated with the icon (4 bytes)
        static constexpr uint8_t ICON_PID_INDEX = Base::NEXT_FREE_INDEX;

        /// Total number of icon packets to expect (4 bytes)
        static constexpr uint8_t ICON_PACKET_COUNT_INDEX = ICON_PID_INDEX + sizeof(uint32_t);

        /// Total number of bytes (4 bytes)
        static constexpr  uint8_t ICON_BYTE_COUNT_INDEX = ICON_PACKET_COUNT_INDEX + sizeof(uint32_t);
    };

    /**
     * @brief Field positions for NewPID packet (C2F)
     *
     * Memory layout:
     * [Base Headers][PID 4B][NAME 20B][VOLUME 1B][MUTE_STATUS 1B]
     *
     * Used to receive information about a newly detected process.
     * Contains process identification, name, and initial audio settings.
     */
    struct NewPID {
        /// Process ID for the new process (4 bytes)
        static constexpr uint8_t PID_INDEX = Base::NEXT_FREE_INDEX;

        /// Process name (NAME_LENGTH_MAX bytes)
        static constexpr uint8_t NAME_INDEX = PID_INDEX + sizeof(uint32_t);

        /// Initial volume setting (1 byte)
        static constexpr uint8_t VOL_INDEX = NAME_INDEX + NAME_LENGTH_MAX;

        /// Initial mute status (1 byte - boolean)
        static constexpr uint8_t MUTE_INDEX = VOL_INDEX + sizeof(uint8_t);
    };

    /**
     * @brief Field positions for PIDClosed packet (C2F)
     *
     * Memory layout:
     * [Base Headers][PID 4B]
     *
     * Used to receive notification that a process has closed/terminated.
     * Contains only the process ID of the terminated process.
     */
    struct PIDClosed {
        /// Process ID of the terminated process (4 bytes)
        static constexpr uint8_t PID_INDEX = Base::NEXT_FREE_INDEX;
    };

    /**
     * @brief Field positions for ProcessRequestInit packet (C2F)
     *
     * Memory layout:
     * [Base Headers][NUM_CHANNELS 1B]
     *
     * Used to initialize a process information request.
     * Contains the number of channels to expect information for.
     * This packet typically precedes detailed process/channel information packets.
     */
    struct ProcessRequestInit {
        /// Number of channels that will be described in following packets (1 byte)
        static constexpr uint8_t NUM_CHANNELS_INDEX = Base::NEXT_FREE_INDEX;
    };

    /**
     * @brief Field positions for Acknowledge packet (C2F and F2C)
     *
     * Memory layout:
     * [Base Headers][ACK_PACKET 1B]
     *
     * Used to acknowledge receipt of a specific packet type.
     */
    struct AcknowledgePacket {
        /// Packet index being acknowledged (1 byte)
        static constexpr uint8_t ACK_PACKET_INDEX = Base::NEXT_FREE_INDEX;
        /// packet tupe being acknowledged (1 byte)
        static constexpr uint8_t ACK_TYPE_INDEX = ACK_PACKET_INDEX + sizeof(uint8_t);

    };

    /**
     * @brief Field positions for StopNormalBroadcasts packet (F2C)
     *
     * Memory layout:
     * [Base Headers]
     *
     * Used to request stopping normal broadcast messages.
     * Contains no additional fields beyond base headers.
     */
    struct StopNormalBroadcasts {
        // No additional fields beyond base headers
    };

    /**
     * @brief Field positions for StartNormalBroadcasts packet (F2C)
     *
     * Memory layout:
     * [Base Headers]
     *
     * Used to request starting normal broadcast messages.
     * Contains no additional fields beyond base headers.
     */
    struct StartNormalBroadcasts {
        // No additional fields beyond base headers
    };

    /**
     * @brief Field positions for RequestChannelData packet (F2C)
     *
     * Memory layout:
     * [Base Headers][PID 4B]
     *
     * Used to request channel data for a specific process.
     */
    struct RequestChannelData {
        /// Process ID (4 bytes)
        static constexpr uint8_t PID_INDEX = Base::NEXT_FREE_INDEX;
    };

    /**
     * @brief Field positions for ChangeOfChannel packet (F2C)
     *
     * Memory layout:
     * [Base Headers][PID 4B][VOLUME 1B][MUTED 1B]
     *
     * Used to update channel settings for a specific process.
     */
    struct ChangeOfChannel {
        /// Process ID (4 bytes)
        static constexpr uint8_t PID_INDEX = Base::NEXT_FREE_INDEX;

        /// Volume level (1 byte)
        static constexpr uint8_t VOLUME_INDEX = PID_INDEX + sizeof(uint32_t);

        /// Muted flag (1 byte)
        static constexpr uint8_t MUTED_INDEX = VOLUME_INDEX + sizeof(uint8_t);
    };

    /**
     * @brief Field positions for ChangeOfMasterChannel packet (F2C)
     *
     * Memory layout:
     * [Base Headers][VOLUME 1B][MUTED 1B]
     *
     * Used to update master channel settings.
     */
    struct ChangeOfMasterChannel {
        /// Volume level (1 byte)
        static constexpr uint8_t VOLUME_INDEX = Base::NEXT_FREE_INDEX;

        /// Muted flag (1 byte)
        static constexpr uint8_t MUTED_INDEX = VOLUME_INDEX + sizeof(uint8_t);
    };

    /**
     * @brief Field positions for CurrentSelectedProcesses packet (F2C)
     *
     * Memory layout:
     * [Base Headers][COUNT 1B][PIDs Nx4B]
     *
     * Used to report currently selected processes.
     * Contains a count followed by an array of process IDs.
     */
    struct CurrentSelectedProcesses {
        /// Count of PIDs (1 byte)
        static constexpr uint8_t COUNT_INDEX = Base::NEXT_FREE_INDEX;

        /// Start of PID array (4 bytes per PID)
        static constexpr uint8_t PIDS_INDEX = COUNT_INDEX + sizeof(uint8_t);
    };

    /**
     * @brief Field positions for RequestIcon packet (F2C)
     *
     * Memory layout:
     * [Base Headers][PID 4B]
     *
     * Used to request an icon for a specific process.
     */
    struct RequestIcon {
        /// Process ID (4 bytes)
        static constexpr uint8_t PID_INDEX = Base::NEXT_FREE_INDEX;
    };

    /**
     * @brief Field positions for RequestAllProcesses packet (F2C)
     *
     * Memory layout:
     * [Base Headers]
     *
     * Used to request information about all processes.
     * Contains no additional fields beyond base headers.
     */
    struct RequestAllProcesses {
        // No additional fields beyond base headers
    };

    /**
     * @brief Field positions for IconIsDefault packet (C2F)
     *
     * Memory layout:
     * [Base Headers][PID 4B]
     *
     * Used to notify the computer that the requested icon is the default icon.
     * Contains no PID of process that the icon is for.
     */
    struct IconIsDefault {
        /// Process ID (4 bytes)
        static constexpr uint8_t PID_INDEX = Base::NEXT_FREE_INDEX;
    };
}

