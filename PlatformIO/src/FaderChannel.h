
#ifndef __FADERCHANNEL_H__
#define __FADERCHANNEL_H__

#include <Arduino.h>
#include <ResponsiveAnalogRead.h>
#include <ST7789_t3.h>
#include <CapacitiveSensor.h>
#include <SPI.h>
#include <Ws2812Serial.h>
#include <RoxMux.h>
#include "FaderMotor.h"
#include "icons.h"

//these are the colors for the LEDs
#define ZERO 0x000000
#define ONE 0x000011
#define TWO 0x110011
#define THREE 0x111111
#define JUSTTWO 0x110000
#define JUSTTHREE 0x001100
#define MUTE 0x005500
#define UNMUTE 0x550000
#define SOLO 0x000055

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 240

class FaderChannel
{
private:
    int channelNumber;
    int position;
    int iconWidth;
    int iconHeight;
    int volBarMode = 0;
    int positionMin = 100;
    int positionMax = 950;
    int menuPage = 0;
    int menuIndex = 0;
    String name;
    WS2812Serial *leds;
    ResponsiveAnalogRead *pot;
    CapacitiveSensor *touch;
    ST7789_t3 *tft;
    RoxMCP23017<0x21> *CSSelect;
    uint32_t encoderColor = 0x000011;
    long baselineTouch = 0;
    long lastTouchChange = 0; 
    float touchSensitivity = 1.5f;  //TODO: allow this to be changed in the menu
    bool userTouching = false;
    bool isMaster = false;
    static const int potMuxPins[3];
    static const int touchMuxPins[3];
    static const int csMuxPins[3];
    void drawIcon(int x, int y, int width, int height);

public:
    bool updateScreen = false;
    bool userChanged = false;
    bool requestProcessRefresh = false;
    bool menuOpen = false;
    bool requestNewProcess = false;
    int maxVolume = 50;
    bool isMuted;
    bool isSoloed;
    bool isUnUsed = false;
    char (*processNames)[50][20];
    uint32_t (*processIDs)[50];
    int processIndex = 0;
    AppData appdata;
    FaderMotor *motor;
    FaderChannel(int _channelNumber, WS2812Serial *_leds, ResponsiveAnalogRead *_pot, CapacitiveSensor *_touch, ST7789_t3 *_tft, RoxMCP23017<0x21> *_CSSelect, int _forwardPin, int _backwardPin, bool _isMaster);
    ~FaderChannel();
    void update();
    void begin();
    void displayMenu();
    int getPosition();
    void setIcon(uint16_t (*_icon)[128], int _iconWidth, int _iconHeight);
    void setToCurrentChannel();
    void setMaxVolume(int volume);
    void setCurrentVolume(int volume);
    void setMute(bool mute);
    void setSolo(bool solo);
    void setName(const char _name[20]);
    void onButtonPress(int buttonNumber);
    void onRotaryPress();
    void onRotaryTurn(bool clockwise);
    void setUnused();
    void setVolumeBarMode(int mode);
    void setUnTouched();
    void setTouchSensitivity(int _sensitivity);
    void setPositionMin();
    void setPositionMax();
};

FaderChannel::FaderChannel(int _channelNumber, WS2812Serial *_leds, ResponsiveAnalogRead *_pot, CapacitiveSensor *_touch, ST7789_t3 *_tft, RoxMCP23017<0x21> *_CSSelect, int _forwardPin, int _backwardPin, bool _isMaster) : appdata(_isMaster)
{
    for (int i = 0; i < 3; i++)
    {
        pinMode(potMuxPins[i], OUTPUT);
        pinMode(touchMuxPins[i], OUTPUT);
        pinMode(csMuxPins[i], OUTPUT);
    }
    channelNumber = _channelNumber;
    leds = _leds;
    pot = _pot;
    touch = _touch;
    tft = _tft;
    CSSelect = _CSSelect;
    isMaster = _isMaster;
    maxVolume = 50;
    motor = new FaderMotor(_forwardPin, _backwardPin);
}

void FaderChannel::begin()
{
    setMute(false);
    setSolo(false);
}

FaderChannel::~FaderChannel()
{
}

int FaderChannel::getPosition()
{
    setToCurrentChannel();
    return (100 - map(analogRead(A6), positionMin + 10, positionMax - 10, 0, 100));
}

void FaderChannel::update()
{
    setToCurrentChannel();
    for (int i = 0; i < 4; i++)
    {
        leds->setPixel(i + 88 + 4 * channelNumber, encoderColor);
    }
    
    if (!isUnUsed)
    {
        position = 100 - map(analogRead(A6), positionMin + 10, positionMax - 10, 0, 100);
        if (touch->capacitiveSensor(25) > (float)baselineTouch * touchSensitivity && position > 5)
        {
            motor->stop();
            setSolo(true);
            // only set true if it has been 100ms since last change
            if (millis() - lastTouchChange > 100)
            {
                userChanged = true;
                lastTouchChange = millis();
            }
        }
        else
        {
            setSolo(false);
            if (position > maxVolume + 3)
            {
                motor->forward(60);
            }
            else if (position < maxVolume - 3)
            {
                motor->backward(60);
            }
            else
            {
                motor->stop();
            }
        }
    }
    if (updateScreen)
    {
        tft->fillScreen(0x000000);
        tft->setTextColor(0xFFFF);
        if (!menuOpen)
        {
            if (!isUnUsed)
            {
                drawIcon(SCREEN_WIDTH / 2 - iconWidth / 2, 0, iconWidth, iconHeight);
            }
            tft->setCursor(10, iconHeight + 10);
            tft->println(name);
            tft->println("PID: " + String(appdata.PID));
        }else{
            displayMenu();
        }
        tft->updateScreen();
        updateScreen = false;
    }
}

void FaderChannel::displayMenu()
{
    // 8 processes per page, display ► to the left of the selected process
    int numPages = processIndex / 8 + (processIndex % 8 != 0) - 1;
    if (menuIndex > 7 && menuPage < numPages)
    {
        menuIndex = 0;
        menuPage++;
    }else if (menuIndex > 7 && menuPage >= numPages){
        menuIndex = 7;
    }
    if (menuIndex < 0 && menuPage > 0)
    {
        menuPage--;
        menuIndex = 7;
    }else if (menuIndex < 0 && menuPage <= 0){
        menuIndex = 0;
        menuPage = 0;
    }
    if (menuPage < 0)
    {
        menuPage = 0;
    }
    if (menuPage > numPages)
    {
        menuPage = numPages;
    }
    if(menuPage * 8 + menuIndex >= processIndex){
        menuIndex--;
    }

    String temp = "";
    tft->setCursor(0, 0);
    for (int i = 0; i < 8; i++)
    {
        if (i < processIndex)
        {
            if (i == menuIndex)
            {
                temp = (char)16;
                temp += " ";
                for (int j = 0; j < 20; j++)
                {
                    temp += String((*processNames)[menuPage * 8 + i][j]);
                }
                tft->setTextColor(0x00FF);
            }
            else
            {
                temp = "  ";
                for (int j = 0; j < 20; j++)
                {
                    temp += String((*processNames)[menuPage * 8 + i][j]);
                }
                tft->setTextColor(0xFFFF);
            }
            tft->println(temp);
        }
    }
    //print all characters up to 255
    for (int i = 0; i < 256; i++)
    {
        temp = String(i) + ": " + (char)i + " ";
        tft->print(temp);
    }
}

void FaderChannel::onButtonPress(int buttonNumber)
{
    if (!isUnUsed)
    {
        if (buttonNumber == 1)
        {
            setMute(!isMuted);
            userChanged = true;
        }
        else if (buttonNumber == 0)
        {
            setSolo(!isSoloed);
        }
    }
}

void FaderChannel::onRotaryPress()
{
    if (!isMaster)
    {
        if (menuOpen)
        {
            encoderColor = 0x000011;
            menuOpen = false;
            updateScreen = true;
            if((*processIDs)[menuPage * 8 + menuIndex] != appdata.PID){
                requestNewProcess = true;
                appdata.PID = (*processIDs)[menuPage * 8 + menuIndex];
            }
        }
        else
        {
            encoderColor = 0x110000;
            requestProcessRefresh = true;
            menuPage = 0;
            menuIndex = 0;
        }
    }
}

void FaderChannel::onRotaryTurn(bool clockwise)
{
    if (clockwise)
    {
        // touchSensitivity += 0.02f;
        if (menuOpen)
        {
            menuIndex++;
            updateScreen = true;
        }
    }
    else
    {
        if (menuOpen)
        {
            menuIndex--;
            updateScreen = true;
        }
        // touchSensitivity -= 0.02f;
    }
}
void FaderChannel::drawIcon(int x, int y, int width, int height)
{
    for (int i = 0; i < width; i++)
    {
        for (int j = 0; j < height; j++)
        {
            tft->drawPixel(x + i, y + j, appdata.iconInUse[i][j]);
        }
    }
}

void FaderChannel::setIcon(uint16_t _icon[128][128], int _iconWidth, int _iconHeight)
{
    isUnUsed = false;
    memcpy(appdata.iconInUse, _icon, 128 * 128 * sizeof(uint16_t));
    iconWidth = _iconWidth;
    iconHeight = _iconHeight;
    updateScreen = true;
}

void FaderChannel::setToCurrentChannel()
{
    for (int i = 0; i < 3; i++)
    {
        digitalWrite(potMuxPins[i], bitRead(channelNumber, i));
        digitalWrite(touchMuxPins[i], bitRead(channelNumber, i));
        digitalWrite(csMuxPins[i], bitRead(channelNumber, i));
    }
    delayMicroseconds(50);
}

void FaderChannel::setMaxVolume(int volume)
{
    if (volume > 100)
    {
        volume = 100;
    }
    if (volume < 0)
    {
        volume = 0;
    }
    maxVolume = volume;
}

void FaderChannel::setCurrentVolume(int volume)
{
    // value is 0 to 100.
    // we need to map it to 8 rgb leds, so 24 leds total.
    int mappedVolume = map(volume, 0, 100, 0, 24);
    int i = 0;
    if (!isMuted)
    {
        while (mappedVolume > 0)
        {
            if (mappedVolume >= 3)
            {
                if (volBarMode == 1)
                {
                    if (mappedVolume == 3)
                    {
                        leds->setPixel(channelNumber * 8 + i, JUSTTHREE);
                    }
                    else
                    {
                        leds->setPixel(channelNumber * 8 + i, ZERO);
                    }
                }
                else
                {
                    leds->setPixel(channelNumber * 8 + i, THREE);
                }
                mappedVolume -= 3;
            }
            else if (mappedVolume == 2)
            {
                if (volBarMode == 1)
                {
                    leds->setPixel(channelNumber * 8 + i, JUSTTWO);
                }
                else
                {
                    leds->setPixel(channelNumber * 8 + i, TWO);
                }
                mappedVolume -= 2;
            }
            else if (mappedVolume == 1)
            {
                leds->setPixel(channelNumber * 8 + i, ONE);
                mappedVolume -= 1;
            }
            i++;
        }
    }
    // use i to fill the rest of the leds with 0.
    while (i < 8)
    {
        leds->setPixel(channelNumber * 8 + i, ZERO);
        i++;
    }
}

void FaderChannel::setMute(bool mute)
{
    isMuted = mute;
    if (isMuted)
    {
        leds->setPixel(64 + 2 * channelNumber, MUTE);
    }
    else
    {
        leds->setPixel(64 + 2 * channelNumber, UNMUTE);
    }
}

void FaderChannel::setSolo(bool solo)
{
    isSoloed = solo;
    if (isSoloed)
    {
        leds->setPixel(64 + 2 * channelNumber + 1, SOLO);
    }
    else
    {
        leds->setPixel(64 + 2 * channelNumber + 1, ZERO);
    }
}

void FaderChannel::setName(const char _name[20])
{
    updateScreen = true;
    name = "";
    for (int i = 0; i < 20; i++)
    {
        // appdata.name[i] = _name[i];
        strcpy(&appdata.name[i], &_name[i]);
        name += _name[i];
    }
}

void FaderChannel::setUnused()
{
    maxVolume = 0;
    setName("None               ");
    // set pid to uint32 max
    appdata.PID = 4294967295;
    isUnUsed = true;
}

void FaderChannel::setVolumeBarMode(int mode)
{
    volBarMode = mode;
}

void FaderChannel::setUnTouched()
{
    setToCurrentChannel();
    touch->capacitiveSensor(1);
    baselineTouch = touch->capacitiveSensor(25);
}

void FaderChannel::setTouchSensitivity(int _sensitivity)
{
    touchSensitivity = _sensitivity;
}

void FaderChannel::setPositionMin()
{
    setToCurrentChannel();
    positionMin = analogRead(A6);
}

void FaderChannel::setPositionMax()
{
    setToCurrentChannel();
    positionMax = analogRead(A6);
}

const int FaderChannel::potMuxPins[3] = {36, 37, 38};
const int FaderChannel::touchMuxPins[3] = {39, 40, 41};
const int FaderChannel::csMuxPins[3] = {30, 31, 32};

#endif