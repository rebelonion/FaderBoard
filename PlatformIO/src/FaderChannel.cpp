#include "FaderChannel.h"
#include <Arduino.h>
#include "Globals.h"


FaderChannel::FaderChannel(const uint8_t _channelNumber, WS2812Serial *_leds, ResponsiveAnalogRead *_pot,
                           CapacitiveSensor *_touch, ST7789_t3 *_tft,
                           const uint8_t _forwardPin, const uint8_t _backwardPin,
                           const bool _isMaster) : appdata(_isMaster) {
    for (int i = 0; i < 3; i++) {
        pinMode(potMuxPins[i], OUTPUT);
        pinMode(touchMuxPins[i], OUTPUT);
        pinMode(csMuxPins[i], OUTPUT);
    }
    channelNumber = _channelNumber;
    leds = _leds;
    pot = _pot;
    touch = _touch;
    tft = _tft;
    isMaster = _isMaster;
    targetVolume = 50;
    motor = new FaderMotor(_forwardPin, _backwardPin);
}

void FaderChannel::begin() {
    setMute(false);
    setSelected(false);
}

FaderChannel::~FaderChannel() = default;

uint8_t FaderChannel::getFaderPosition() const {
    setToCurrentChannel();
    return (100 - map(analogRead(A6), positionMin + 10, positionMax - 10, 0, 100));
}

void FaderChannel::update() {
    setToCurrentChannel();
    for (int i = 0; i < 4; i++) {
        leds->setPixel(i + 88 + 4 * channelNumber, encoderColor);
    }

    if (!isUnUsed) {
        faderPosition = 100 - map(analogRead(A6), positionMin + 10, positionMax - 10, 0, 100);
        if (static_cast<float>(touch->capacitiveSensor(25)) > static_cast<float>(baselineTouch) * touchSensitivity &&
            faderPosition > TOUCH_THRESHOLD) {
            motor->stop();
            setSelected(true);
            if (millis() - lastTouchChange > TOUCH_DEBOUNCE_TIME) {
                userChanged = true;
                lastTouchChange = millis();
            }
        } else {
            setSelected(false);
            if (faderPosition > targetVolume + POSITION_DEADZONE) {
                motor->forward(abs(faderPosition - targetVolume) < SPEED_THRESHOLD ? SLOW_SPEED : FAST_SPEED);
            } else if (faderPosition < targetVolume - POSITION_DEADZONE) {
                motor->backward(abs(faderPosition - targetVolume) < SPEED_THRESHOLD ? SLOW_SPEED : FAST_SPEED);
            } else {
                motor->stop();
            }
        }
    }
    if (updateScreen) {
        tft->fillScreen(0x000000);
        tft->setTextColor(0xFFFF);
        if (!menuOpen) {
            if (!isUnUsed) {
                drawIcon(SCREEN_WIDTH / 2 - iconWidth / 2, 0, iconWidth, iconHeight);
            }
            tft->setCursor(10, iconHeight + 10);
            tft->println(name);
            tft->println("PID: " + String(appdata.PID));
        } else {
            displayMenu();
        }
        tft->updateScreen();
        updateScreen = false;
    }
}

void FaderChannel::displayMenu() {
    constexpr int ITEMS_PER_PAGE = 8;
    const uint8_t numPages = (openProcessIDs.getSize() + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE;

    if (menuIndex >= ITEMS_PER_PAGE) {
        menuPage = min(menuPage + 1, numPages - 1);
        menuIndex = 0;
    } else if (menuIndex < 0) {
        menuPage = max(menuPage - 1, 0);
        menuIndex = (menuPage < numPages - 1)
                        ? ITEMS_PER_PAGE - 1
                        : min(openProcessIDs.getSize() % ITEMS_PER_PAGE - 1, ITEMS_PER_PAGE - 1);
    }

    menuIndex = min(menuIndex, min(ITEMS_PER_PAGE - 1, openProcessIDs.getSize() - menuPage * ITEMS_PER_PAGE - 1));

    tft->setCursor(0, 0);
    for (uint32_t i = 0; i < ITEMS_PER_PAGE; i++) {
        if (const uint32_t processIdx = menuPage * ITEMS_PER_PAGE + i; processIdx < openProcessIDs.getSize()) {
            if (i == menuIndex) {
                constexpr char SELECTION_INDICATOR = 16;
                constexpr uint16_t SELECTED_COLOR = 0x00FF;
                tft->setTextColor(SELECTED_COLOR);
                tft->print(SELECTION_INDICATOR);
                tft->print(' ');
            } else {
                constexpr uint16_t NORMAL_COLOR = 0xFFFF;
                tft->setTextColor(NORMAL_COLOR);
                tft->print("  ");
            }

            for (uint8_t j = 0; j < NAME_LENGTH_MAX && openProcessNames[processIdx][j] != '\0'; j++) {
                tft->print(openProcessNames[processIdx][j]);
            }
            tft->println();
        }
    }
}

void FaderChannel::onButtonPress(const uint8_t buttonNumber) {
    if (!isUnUsed) {
        if (buttonNumber == 1) {
            setMute(!isMuted);
            userChanged = true;
        } else if (buttonNumber == 0) {
            //TODO: macros?
        }
    }
}

void FaderChannel::onRotaryPress() {
    if (!isMaster) {
        if (menuOpen) {
            encoderColor = 0x000011;
            menuOpen = false;
            updateScreen = true;
            if (openProcessIDs[menuPage * 8 + menuIndex] != appdata.PID) {
                requestNewProcess = true;
                appdata.PID = openProcessIDs[menuPage * 8 + menuIndex];
            }
        } else {
            encoderColor = 0x110000;
            requestProcessRefresh = true;
            menuPage = 0;
            menuIndex = 0;
        }
    }
}

void FaderChannel::onRotaryTurn(const bool clockwise) {
    if (clockwise) {
        // touchSensitivity += 0.02f;
        if (menuOpen) {
            menuIndex++;
            updateScreen = true;
        }
    } else {
        if (menuOpen) {
            menuIndex--;
            updateScreen = true;
        }
        // touchSensitivity -= 0.02f;
    }
}

void FaderChannel::drawIcon(const uint16_t x, const uint16_t y, const uint16_t width, const uint16_t height) const {
    for (uint16_t i = 0; i < width; i++) {
        for (uint16_t j = 0; j < height; j++) {
            tft->drawPixel(x + i, y + j, appdata.iconInUse[i + j * width]);
        }
    }
}

void FaderChannel::setIcon(const uint16_t _icon[ICON_SIZE][ICON_SIZE], const uint16_t _iconWidth,
                           const uint16_t _iconHeight) {
    isUnUsed = false;
    memcpy(appdata.iconInUse, _icon, ICON_SIZE * ICON_SIZE * sizeof(uint16_t));
    iconWidth = _iconWidth;
    iconHeight = _iconHeight;
    updateScreen = true;
}

void FaderChannel::setToCurrentChannel() const {
    for (int i = 0; i < 3; i++) {
        digitalWrite(potMuxPins[i], bitRead(channelNumber, i));
        digitalWrite(touchMuxPins[i], bitRead(channelNumber, i));
        digitalWrite(csMuxPins[i], bitRead(channelNumber, i));
    }
    delayMicroseconds(50);
}

void FaderChannel::setMaxVolume(uint8_t volume) {
    if (volume > 100) {
        volume = 100;
    }
    if (volume < 0) {
        volume = 0;
    }
    targetVolume = volume;
}

void FaderChannel::setCurrentVolume(const uint8_t volume) const {
    uint8_t mappedVolume = map(volume, 0, 100, 0, 24);
    int i = 0;

    if (!isMuted) {
        for (; i < 8 && mappedVolume > 0; i++) {
            const uint8_t ledVolume = min(mappedVolume, static_cast<uint8_t>(3));
            if (volBarMode == 1 && ledVolume == 3 && mappedVolume > 3) {
                leds->setPixel(channelNumber * 8 + i, ZERO);
            } else {
                leds->setPixel(channelNumber * 8 + i, ledStates[volBarMode][ledVolume]);
            }
            mappedVolume -= ledVolume;
        }
    }

    for (; i < 8; i++) {
        leds->setPixel(channelNumber * 8 + i, ZERO);
    }
}

void FaderChannel::setMute(const bool mute) {
    isMuted = mute;
    if (isMuted) {
        leds->setPixel(64 + 2 * channelNumber, MUTE);
    } else {
        leds->setPixel(64 + 2 * channelNumber, UNMUTE);
    }
}

void FaderChannel::setSelected(const bool selected) const {
    if (selected) {
        leds->setPixel(64 + 2 * channelNumber + 1, SELECTED);
    } else {
        leds->setPixel(64 + 2 * channelNumber + 1, ZERO);
    }
}

void FaderChannel::setName(const char _name[20]) {
    updateScreen = true;
    name = "";
    for (int i = 0; i < 20; i++) {
        strcpy(&appdata.name[i], &_name[i]);
        name += _name[i];
    }
}

void FaderChannel::setUnused(const bool _isUnused) {
    isUnUsed = _isUnused;
    if (isUnUsed) {
        targetVolume = 0;
        setName("None               ");
        appdata.PID = UINT32_MAX;
    }
}

void FaderChannel::setVolumeBarMode(const uint8_t mode) {
    volBarMode = mode;
}

void FaderChannel::setUnTouched() {
    setToCurrentChannel();
    touch->capacitiveSensor(1);
    baselineTouch = touch->capacitiveSensor(25);
}

void FaderChannel::setTouchSensitivity(const float _sensitivity) {
    touchSensitivity = _sensitivity;
}

void FaderChannel::setPositionMin() {
    setToCurrentChannel();
    positionMin = analogRead(A6);
}

void FaderChannel::setPositionMax() {
    setToCurrentChannel();
    positionMax = analogRead(A6);
}

bool FaderChannel::isUnused() const {
    return isUnUsed;
}
