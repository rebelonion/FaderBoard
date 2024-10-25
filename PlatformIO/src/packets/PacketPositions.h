#pragma once

#include <Globals.h>
#include <Arduino.h>

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
     * @brief Field positions for RecAllCurrentProcesses packet
     *
     * Memory layout:
     * [Base Headers][PID 4B][NAME 20B][PID2 4B][NAME2 20B]
     *
     * Used to retrieve information about current processes.
     * Contains details for one or two processes.
     */
    struct RecAllCurrentProcesses {
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
     * @brief Field positions for RecChannelData packet
     *
     * Memory layout:
     * [Base Headers][IS_MASTER 1B][MAX_VOLUME 1B][IS_MUTED 1B][PID 4B][NAME 20B]
     *
     * Used to retrieve information about an audio channel.
     * Contains channel properties including master status, volume settings, and process details.
     */
    struct RecChannelData {
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
     * @brief Field positions for RecCurrentVolumeLevels packet
     *
     * Memory layout for each channel (repeats up to 7 times):
     * [Base Headers][CH0_PID 4B][CH0_VOL 1B][CH1_PID 4B][CH1_VOL 1B]...[CH6_PID 4B][CH6_VOL 1B]
     *
     * Used to retrieve current volume levels for up to 7 audio channels.
     * Each channel entry contains its process ID and current volume level.
     */
    struct RecCurrentVolumeLevels {
        /// Size of data for each channel (PID + volume)
        static constexpr size_t CHANNEL_SIZE = sizeof(uint32_t) + sizeof(uint8_t);

        /// Process ID for a channel (4 bytes)
        /// Access with: PID_INDEX + (channelIndex * CHANNEL_SIZE)
        static constexpr uint8_t PID_INDEX = Base::NEXT_FREE_INDEX;

        /// Volume level for a channel (1 byte)
        /// Access with: VOLUME_INDEX + (channelIndex * CHANNEL_SIZE)
        static constexpr uint8_t VOLUME_INDEX = PID_INDEX + sizeof(uint32_t);
    };

    /**
     * @brief Field positions for RecIconPacket packet
     *
     * Memory layout:
     * [Base Headers][ICON_DATA (PACKET_SIZE - Base Headers)B]
     *
     * Used to receive chunks of icon data that will be stored in a compression buffer.
     * The icon data fills all remaining space in the packet after base headers.
     */
    struct RecIconPacket {
        /// Starting index of icon data bytes
        static constexpr uint8_t ICON_INDEX = Base::NEXT_FREE_INDEX;

        /// Number of icon bytes in this packet
        /// Fills remaining packet space after base headers
        static constexpr uint8_t NUM_ICON_BYTES_SENT = PACKET_SIZE - ICON_INDEX;
    };

    /**
     * @brief Field positions for RecIconPacketInit packet
     *
     * Memory layout:
     * [Base Headers][PID 4B][PACKET_COUNT 4B]
     *
     * Used as the initial packet for icon data transfer.
     * Contains the process ID and the total number of icon packets that will follow.
     */
    struct RecIconPacketInit {
        /// Process ID associated with the icon (4 bytes)
        static constexpr uint8_t ICON_PID_INDEX = Base::NEXT_FREE_INDEX;

        /// Total number of icon packets to expect (4 bytes)
        static constexpr uint8_t ICON_PACKET_COUNT_INDEX = ICON_PID_INDEX + sizeof(uint32_t);
    };

    /**
     * @brief Field positions for RecMasterMaxChange packet
     *
     * Memory layout:
     * [Base Headers][MAX_VOLUME 1B][MUTE_STATUS 1B]
     *
     * Used to receive updates to master channel settings.
     * Contains the master channel's maximum volume level and mute status.
     */
    struct RecMasterMaxChange {
        /// Maximum volume setting for master channel (1 byte)
        static constexpr uint8_t MASTER_MAX_VOLUME_INDEX = Base::NEXT_FREE_INDEX;

        /// Mute status for master channel (1 byte - boolean)
        static constexpr uint8_t MASTER_MAX_MUTE_INDEX = MASTER_MAX_VOLUME_INDEX + sizeof(uint8_t);
    };

    /**
     * @brief Field positions for RecMaxChange packet
     *
     * Memory layout:
     * [Base Headers][PID 4B][MAX_VOLUME 1B][MUTE_STATUS 1B]
     *
     * Used to receive volume setting changes for a specific process.
     * Similar to RecMasterMaxChange but includes process identification.
     */
    struct RecMaxChange {
        /// Process ID for the target channel (4 bytes)
        static constexpr uint8_t PID_INDEX = Base::NEXT_FREE_INDEX;

        /// Maximum volume setting (1 byte)
        static constexpr uint8_t MAX_VOLUME_INDEX = PID_INDEX + sizeof(uint32_t);

        /// Mute status (1 byte - boolean)
        static constexpr uint8_t IS_MUTED_INDEX = MAX_VOLUME_INDEX + sizeof(uint8_t);
    };

    /**
     * @brief Field positions for RecNewPID packet
     *
     * Memory layout:
     * [Base Headers][PID 4B][NAME 20B][VOLUME 1B][MUTE_STATUS 1B]
     *
     * Used to receive information about a newly detected process.
     * Contains process identification, name, and initial audio settings.
     */
    struct RecNewPID {
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
     * @brief Field positions for RecPIDClosed packet
     *
     * Memory layout:
     * [Base Headers][PID 4B]
     *
     * Used to receive notification that a process has closed/terminated.
     * Contains only the process ID of the terminated process.
     */
    struct RecPIDClosed {
        /// Process ID of the terminated process (4 bytes)
        static constexpr uint8_t PID_INDEX = Base::NEXT_FREE_INDEX;
    };

    /**
     * @brief Field positions for RecProcessRequestInit packet
     *
     * Memory layout:
     * [Base Headers][NUM_CHANNELS 1B]
     *
     * Used to initialize a process information request.
     * Contains the number of channels to expect information for.
     * This packet typically precedes detailed process/channel information packets.
     */
    struct RecProcessRequestInit {
        /// Number of channels that will be described in following packets (1 byte)
        static constexpr uint8_t NUMBER_OF_CHANNELS_INDEX = Base::NEXT_FREE_INDEX;
    };
}
