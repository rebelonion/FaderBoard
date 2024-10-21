#pragma once

#include <Arduino.h>
#include "Globals.h"
#include "FaderMotor.h"

class FaderChannel
{
private:
    static constexpr uint32_t ledStates[2][4] = {
        {ZERO, ONE, TWO, THREE},         // volBarMode 0
        {ZERO, ONE, JUSTTWO, JUSTTHREE}  // volBarMode 1
    };
    uint8_t channelNumber;
    uint16_t iconWidth = 0;
    uint16_t iconHeight = 0;
    uint8_t volBarMode = 0;
    uint16_t positionMin = 100;
    uint16_t positionMax = 950;
    uint8_t menuPage = 0;
    uint8_t menuIndex = 0;
    String name;
    WS2812Serial *leds;
    ResponsiveAnalogRead *pot; //TODO: see if this is necessary
    CapacitiveSensor *touch;
    ST7789_t3 *tft;
    uint32_t encoderColor = 0x000011;
    uint32_t baselineTouch = 0;
    uint32_t lastTouchChange = 0;
    float touchSensitivity = 1.5f;  //TODO: allow this to be changed in the menu
    bool userTouching = false;
    bool isMaster = false;

    const uint8_t TOUCH_THRESHOLD = 5;
    const uint8_t POSITION_DEADZONE = 3;
    const uint8_t SPEED_THRESHOLD = 20;
    const uint8_t SLOW_SPEED = 40;
    const uint8_t FAST_SPEED = 60;
    const uint32_t TOUCH_DEBOUNCE_TIME = 100;

    void drawIcon(uint16_t x, uint16_t y, uint16_t width, uint16_t height) const;

public:
    bool updateScreen = false;
    bool userChanged = false;
    bool requestProcessRefresh = false;
    bool menuOpen = false;
    bool requestNewProcess = false;
    uint8_t targetVolume = 50;
    uint16_t faderPosition{};
    bool isMuted{};
    bool isSoloed{};
    bool isUnUsed = false;
    char (*processNames)[50][20]{};
    uint32_t (*processIDs)[50]{};
    uint32_t processIndex = 0;
    AppData appdata;
    FaderMotor *motor;
    FaderChannel(uint8_t _channelNumber, WS2812Serial *_leds, ResponsiveAnalogRead *_pot, CapacitiveSensor *_touch, ST7789_t3 *_tft, uint8_t _forwardPin, uint8_t _backwardPin, bool _isMaster);
    ~FaderChannel();
    void update();
    void begin();
    void displayMenu();
    [[nodiscard]] uint16_t getFaderPosition() const;
    void setIcon(const uint16_t _icon[ICON_SIZE][ICON_SIZE], uint16_t _iconWidth, uint16_t _iconHeight);
    void setToCurrentChannel() const;
    void setMaxVolume(uint8_t volume);
    void setCurrentVolume(uint8_t volume) const;
    void setMute(bool mute);
    void setSolo(bool solo);
    void setName(const char _name[20]);
    void onButtonPress(uint8_t buttonNumber);
    void onRotaryPress();
    void onRotaryTurn(bool clockwise);
    void setUnused();
    void setVolumeBarMode(uint8_t mode);
    void setUnTouched();
    void setTouchSensitivity(float _sensitivity);
    void setPositionMin();
    void setPositionMax();
};