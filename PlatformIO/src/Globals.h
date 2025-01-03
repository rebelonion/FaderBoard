#pragma once

#include <Arduino.h>
#include <ByteArrayQueue.h>
#include <ST7789_t3.h>
#include <CapacitiveSensor.h>
#include <ResponsiveAnalogRead.h>
#include <WS2812Serial.h>
#include <RoxMux.h>
#include "StaticVector.h"
#include "smalloc.h"


// Constants
/***************************************************/
static constexpr uint8_t API_VERSION = 1;
static constexpr uint8_t NAME_LENGTH_MAX = 20;
static constexpr uint8_t MAX_PROCESSES = 50;
static constexpr uint8_t ICON_SIZE = 128;
static constexpr uint8_t CHANNELS = 8;
static constexpr uint8_t MASTER_CHANNEL = 0;
static constexpr uint32_t MASTER_REQUEST = 1;
static constexpr uint8_t FIRST_CHANNEL = 1;
static constexpr uint8_t PACKET_SIZE = 64;
static constexpr size_t TIMEOUT = 50;
static constexpr size_t SCREEN_WIDTH = 240;
static constexpr size_t SCREEN_HEIGHT = 240;

#define ZERO 0x000000
#define ONE 0x000011
#define TWO 0x110011
#define THREE 0x111111
#define JUSTTWO 0x110000
#define JUSTTHREE 0x001100
#define MUTE 0x005500
#define UNMUTE 0x550000
#define SELECTED 0x000055


 //inline uint16_t globalIconBuffer[CHANNELS][ICON_SIZE][ICON_SIZE];

// Data Structures
/***************************************************/
struct AppData {
    explicit AppData(const bool _isMaster, const uint8_t index) : isMaster(_isMaster) {
        memset(name, 0, sizeof(name));
    }

    bool isMaster = false;
    bool isDefaultIcon = false;
    uint32_t PID = 0;
    char name[NAME_LENGTH_MAX]{};
    uint16_t iconBuffer[ICON_SIZE][ICON_SIZE]{};
};

struct StoredData {
    StoredData() : iconInUse(nullptr) {
        iconInUse = static_cast<uint16_t *>(extmem_malloc(ICON_SIZE * ICON_SIZE * sizeof(uint16_t)));
        memset(name, 0, sizeof(name));
    }

    ~StoredData() {
        if (iconInUse != nullptr) {
            extmem_free(iconInUse);
            iconInUse = nullptr;
        }
    }

    uint16_t *iconInUse;
    uint32_t iconPID = 0;
    char name[NAME_LENGTH_MAX]{};

    void storeIcon(uint16_t iconData[ICON_SIZE][ICON_SIZE]) const {
        if (iconInUse != nullptr) {
            memcpy(iconInUse, iconData, ICON_SIZE * ICON_SIZE * sizeof(uint16_t));
        }
    }

    void retrieveIcon(uint16_t iconData[ICON_SIZE][ICON_SIZE]) const {
        if (iconInUse != nullptr) {
            memcpy(iconData, iconInUse, ICON_SIZE * ICON_SIZE * sizeof(uint16_t));
        }
    }
};

inline smalloc_pool EXTM_Pool;
DMAMEM inline uint8_t compressionBuffer[ICON_SIZE * ICON_SIZE * 2 * 21 / 20 + 66]; // 5% larger than input + 66 bytes
inline uint32_t compressionIndex = 0;
inline uint32_t compressionSize = 0;
inline StoredData storedData[25]; //TODO: used for storing data in PSRAM (not implemented yet)

// LED Strip
/***************************************************/
static constexpr uint8_t LED_PIN = 35;
static constexpr uint8_t LED_COUNT = 120;
inline byte LEDDrawingMemory[LED_COUNT * 3];
DMAMEM inline byte LEDDisplayMemory[LED_COUNT * 12];
inline WS2812Serial LEDs(LED_COUNT, LEDDisplayMemory, LEDDrawingMemory, LED_PIN, WS2812_GRB);


// LCDs
/***************************************************/
static constexpr int8_t TFT_CS = -1;
static constexpr uint8_t TFT_DC = 9;
static constexpr uint8_t TFT_RST = 10;
static constexpr uint8_t TFT_MOSI = 11;
static constexpr uint8_t TFT_SCLK = 13;
static constexpr uint8_t CS_LOCK = 27; // Allows for all screens to be updated at once
inline auto tft = ST7789_t3(TFT_CS, TFT_DC, TFT_RST);
inline uint16_t frameBuffer[16000]; // 32000 bytes of memory allocated for the frame buffer

// Capacitive Touch
/***************************************************/
static constexpr uint8_t TOUCH_SEND = A2;
static constexpr uint8_t TOUCH_RECEIVE = A3;
inline auto capSensor = CapacitiveSensor(TOUCH_SEND, TOUCH_RECEIVE);

// Rotary Encoders
/***************************************************/
inline RoxMCP23017<0x20> rotaryMux;
inline RoxMCP23017<0x21> reButtonMux;
inline RoxMCP23017<0x22> leButtonMux;

inline RoxEncoder encoder[8];  // 8 encoders
inline RoxButton enButton[8];  // 8 encoder buttons
inline RoxButton leButton[16]; // 16 RGB LED buttons

// Faders
/***************************************************/
static constexpr uint8_t POT_INPUT = A6;
inline ResponsiveAnalogRead analog(33, true); // not currently used / needed

// Mux Pins
/***************************************************/
static constexpr uint8_t potMuxPins[3] = {36, 37, 38};
static constexpr uint8_t touchMuxPins[3] = {39, 40, 41};
static constexpr uint8_t csMuxPins[3] = {30, 31, 32};

// Flags
/***************************************************/
//inline bool normalBroadcast = false;
inline size_t numSentChannels = 0;
inline uint32_t currentIconPacket = 0;
inline uint32_t totalIconPackets = 0;
inline uint32_t sentIconPID = 0;
inline bool initializing = false;
// Transitory Variables for passing data around
/***************************************************/
inline uint16_t bufferIcon[ICON_SIZE][ICON_SIZE]; // used for passing icon
inline StaticVector<uint32_t, MAX_PROCESSES> openProcessIDs;
inline StaticVector<char[NAME_LENGTH_MAX], MAX_PROCESSES> openProcessNames;
inline ByteArrayQueue<10, PACKET_SIZE> sendingQueue;

enum SerialCodes {
    UNDEFINED,
    ACK,
    REQUEST_ALL_PROCESSES,
    PROCESS_REQUEST_INIT,
    ALL_CURRENT_PROCESSES,
    START_NORMAL_BROADCASTS,
    STOP_NORMAL_BROADCASTS,
    REQUEST_CHANNEL_DATA,
    CHANNEL_DATA,
    PID_CLOSED,
    SEND_CURRENT_VOLUME_LEVELS,
    CURRENT_SELECTED_PROCESSES,
    NEW_PID,
    REQUEST_ICON,
    ICON_PACKETS_INIT,
    ICON_PACKET,
    THE_ICON_REQUESTED_IS_DEFAULT,
    BUTTON_PUSHED
};

enum AckType {
    ICON_ACK,
    CHANNEL_ACK,
};


inline struct States {
    void setReceivingIcon(const bool _receivingIcon) {
        receivingIcon = _receivingIcon;
        if (receivingIcon) {
            currentIconPacket = 0;
            compressionIndex = 0;
            compressionSize = 0;
            totalIconPackets = 0;
            sentIconPID = 0;
        }
    }

    [[nodiscard]] bool isReceivingIcon() const {
        return receivingIcon;
    }

    void setReceivingChannels(const bool _receivingChannels) {
        receivingChannels = _receivingChannels;
        if (!receivingChannels) {
            numSentChannels = 0;
        }
    }

    [[nodiscard]] bool isReceivingChannels() const {
        return receivingChannels;
    }
private:
    bool receivingIcon = false;
    bool receivingChannels = false;
} states;
