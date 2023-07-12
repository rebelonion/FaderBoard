#include <Arduino.h>
#include <ResponsiveAnalogRead.h>
#include <Adafruit_GFX.h>
#include "icons.h"
#include <ST7789_t3.h>
#include <CapacitiveSensor.h>
#include <SPI.h>
#include <Ws2812Serial.h>
#include <RoxMux.h>
#include "FaderChannel.h"
#include "smalloc.h"
#define TFT_CS -1
#define TFT_DC 9
#define TFT_RST 10
#define TFT_MOSI 11
#define TFT_SCLK 13

#define TimeOut 5000

// Functions
/**************************************************/
void init();
void uncaughtException();
void iconPacketsInit(char buf[64]);
void allCurrentProcesses(char buf[64]);
void processRequestsInit(char buf[64]);
void ping();
int update(char buf[64]);
int requestIcon(int pid);
void requestAllProcesses();
void receiveCurrentVolumeLevels(char buf[64]);
void sendCurrentSelectedProcesses();
void channelData(char buf[64]);
void getMaxVolumes();
void changeOfMaxVolume(char buf[64]);
void changeOfMasterMax(char buf[64]);
void sendChangeOfMaxVolume(int _channelNumber);
void pidClosed(char buf[64]);
void newPID(char buf[64]);
void updateProcess(int i);

/**************************************************/

// LED Strip
const int numled = 120;
const int pin = 35;
byte drawingMemory[numled * 3];         //  3 bytes per LED
DMAMEM byte displayMemory[numled * 12]; // 12 bytes per LED
WS2812Serial leds(numled, displayMemory, drawingMemory, pin, WS2812_GRB);

/**************************************************/

ST7789_t3 tft = ST7789_t3(TFT_CS, TFT_DC, TFT_RST);
uint16_t frameBuffer[16000]; // 32000 bytes of memory allocated for the frame buffer
int csLock = 27;             // Allows for all screens to be updated at once

/**************************************************/

CapacitiveSensor capSensor = CapacitiveSensor(A2, A3);

/**************************************************/

RoxMCP23017<0x20> rotaryMux;
RoxMCP23017<0x21> reButtonMux;
RoxMCP23017<0x22> leButtonMux;

RoxEncoder encoder[8];  // 8 encoders
RoxButton enButton[8];  // 8 encoder buttons
RoxButton leButton[16]; // 16 RGB LED buttons

/**************************************************/

// Transitory Variables for passing data around
int iconWidth = 0;
int iconHeight = 0;
uint32_t iconPages = 0;
uint32_t currentIconPage = 0;
int iconPID = 0;
int numPages = 0;
int numChannels = 0;
uint16_t bufferIcon[128][128]; // used for passing icon
char openProcessNames[50][20]; // an array of 20 character arrays of names of the open processes
uint32_t openProcessIDs[50];
int openProcessIndex = 0; // index after final element in openProcessNames (rip vector)
/**************************************************/

// flags
bool receivingIcon = false;
bool normalBroadcast = false;
int faderRequest = -1;

/**************************************************/

// Fader Variables
int potInpt = A6;
ResponsiveAnalogRead analog(33, true); // not currently used / needed

/**************************************************/

static StoredData storedData[25]; // used for storing data in PSRAM (not implemented yet)

FaderChannel faderChannel[8] = { // 8 fader channels
    FaderChannel(0, &leds, &analog, &capSensor, &tft, &reButtonMux, 1, 2, true),
    FaderChannel(1, &leds, &analog, &capSensor, &tft, &reButtonMux, 3, 4, false),
    FaderChannel(2, &leds, &analog, &capSensor, &tft, &reButtonMux, 5, 6, false),
    FaderChannel(3, &leds, &analog, &capSensor, &tft, &reButtonMux, 7, 8, false),
    FaderChannel(4, &leds, &analog, &capSensor, &tft, &reButtonMux, 24, 25, false),
    FaderChannel(5, &leds, &analog, &capSensor, &tft, &reButtonMux, 28, 29, false),
    FaderChannel(6, &leds, &analog, &capSensor, &tft, &reButtonMux, 14, 15, false),
    FaderChannel(7, &leds, &analog, &capSensor, &tft, &reButtonMux, 22, 23, false)};

void setup()
{
  pinMode(potInpt, INPUT);
  Serial.begin(115200);
  leds.begin();
  pinMode(csLock, OUTPUT);
  digitalWrite(csLock, LOW);
  tft.init(240, 240, SPI_MODE2);
  tft.useFrameBuffer(1);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(0, 0);
  tft.println("Ready");
  tft.updateScreen();

  // set up the muxes
  rotaryMux.begin(true);
  reButtonMux.begin(true);
  leButtonMux.begin(true);
  for (int i = 0; i < 16; i++)
  {
    rotaryMux.pinMode(i, INPUT_PULLUP);
    if (i < 8)
    {
      reButtonMux.pinMode(i, INPUT_PULLUP);
    }
    else
    {
      reButtonMux.pinMode(i, INPUT);
    }
    leButtonMux.pinMode(i, INPUT_PULLUP);
  }
  reButtonMux.update();
  leButtonMux.update();
  rotaryMux.update();
  for (int i = 0; i < 8; i++) // set up the encoders and buttons
  {
    encoder[i].begin();
    enButton[i].begin();
    leButton[2 * i].begin();
    leButton[2 * i + 1].begin();
    faderChannel[i].begin();
  }

  // clear up any data that may be in the buffer (flush didn't work for some reason)
  char buf[64];
  while (Serial.available())
  {
    Serial.readBytes(buf, 64);
  }
  digitalWrite(csLock, HIGH); // unlock the screens
  leds.clear();
  leds.show();                                 // initialize the LEDs
  capSensor.set_CS_AutocaL_Millis(0xFFFFFFFF); // set up the capacitive sensor
  capSensor.set_CS_Timeout_Millis(100);
  capSensor.reset_CS_AutoCal();
  capSensor.capacitiveSensor(30);
  init();
}

void loop()
{
  reButtonMux.update(); // update the encoders and buttons
  leButtonMux.update();
  rotaryMux.update();
  char buf[64];
  for (int i = 0; i < 8; i++) // update the fade channels
  {
    encoder[i].update(rotaryMux.digitalRead(i * 2), rotaryMux.digitalRead(i * 2 + 1), 2, LOW);
    enButton[i].update(reButtonMux.digitalRead(i), 50, LOW);
    leButton[2 * i].update(leButtonMux.digitalRead(i * 2), 50, LOW);
    leButton[2 * i + 1].update(leButtonMux.digitalRead(i * 2 + 1), 50, LOW);
    if (faderChannel[i].userChanged)
    {
      faderChannel[i].userChanged = false;
      sendChangeOfMaxVolume(i);
    }

    if (faderChannel[i].requestProcessRefresh)
    {
      char sendBuf[64];
      sendBuf[0] = REQUEST_ALL_PROCESSES;
      Serial.write(sendBuf, 64);
      faderRequest = i;
      faderChannel[i].requestProcessRefresh = false;
    }

    if (encoder[i].read())
    {
      faderChannel[i].onRotaryTurn(encoder[i].increased());
    }
    if (enButton[i].pressed())
    {
      faderChannel[i].onRotaryPress();
    }
    if (leButton[2 * i].pressed())
    {
      faderChannel[i].onButtonPress(1);
    }
    if (leButton[2 * i + 1].pressed())
    {
      faderChannel[i].onButtonPress(0);
    }

    if (faderChannel[i].requestNewProcess)
    {
      updateProcess(i);
    }

    faderChannel[i].update();
  }
  if (Serial.available() >= 64) // check for serial data
  {
    Serial.readBytes(buf, 64);
    update(buf);
    leds.show();
  }
}

void init() // set fader pot and touch values then get initial data from computer on startup
{
  for (int i = 0; i < 8; i++)
  {
    faderChannel[i].motor->forward(65);
  }
  delay(500);
  for (int i = 0; i < 8; i++)
  {
    faderChannel[i].motor->stop();
    faderChannel[i].setPositionMax();
    faderChannel[i].motor->backward(65);
  }
  delay(500);
  for (int i = 0; i < 8; i++)
  {
    faderChannel[i].motor->stop();
    faderChannel[i].setPositionMin();
  }
  for (int i = 0; i < 8; i++)
  {
    faderChannel[i].motor->forward(80);
  }
  delay(50);
  for (int i = 0; i < 8; i++)
  {
    faderChannel[i].motor->stop();
  }
  delay(50);
  capSensor.reset_CS_AutoCal();
  for (int i = 0; i < 8; i++)
  {
    faderChannel[i].setUnTouched();
  }
  char buf[64];
  requestAllProcesses();
  // fill the 7 fader channels with the first 7 processes
  faderChannel[0].setIcon(defaultIcon, 128, 128);
  // faderChannel[0].setName(("Master             "));
  for (int i = 1; i < 8; i++)
  {
    if (openProcessIndex >= i)
    {
      faderChannel[i].appdata.PID = openProcessIDs[i - 1];
      // set the name
      faderChannel[i].setName(openProcessNames[i - 1]);
      // request the icon
      int a = requestIcon(faderChannel[i].appdata.PID);
      if (a != THE_ICON_REQUESTED_IS_DEFAULT)
      {
        faderChannel[i].setIcon(bufferIcon, iconWidth, iconHeight);
      }
      else
      {
        faderChannel[i].setIcon(defaultIcon, 128, 128);
      }
    }
    else
    {
      // set to uint32 max
      faderChannel[i].appdata.PID = 4294967295;
      // set the name
      faderChannel[i].appdata.name[0] = 'N';
      faderChannel[i].appdata.name[1] = 'o';
      faderChannel[i].appdata.name[2] = 'n';
      faderChannel[i].appdata.name[3] = 'e';
      faderChannel[i].isUnUsed = true;
    }
  }
  getMaxVolumes();
  for (int i = 0; i < 8; i++)
  {
    faderChannel[i].update();
  }
  sendCurrentSelectedProcesses();
  buf[0] = START_NORMAL_BROADCASTS;
  Serial.write(buf, 64);
  normalBroadcast = true;
}

void sendCurrentSelectedProcesses() // send the processes of the 7 fader channels to the computer
{
  char buf[64];
  buf[0] = CURRENT_SELECTED_PROCESSES;
  for (int i = 0; i < 7; i++)
  {
    buf[i * 4 + 1] = faderChannel[i + 1].appdata.PID;
    buf[i * 4 + 2] = faderChannel[i + 1].appdata.PID >> 8;
    buf[i * 4 + 3] = faderChannel[i + 1].appdata.PID >> 16;
    buf[i * 4 + 4] = faderChannel[i + 1].appdata.PID >> 24;
  }
  Serial.write(buf, 64);
}

void requestAllProcesses() // requests all sound processes from the computer
{
  uint32_t millisStart = millis() - 100;
  char buf[64];
  int a = 0;
  bool started = false;
  while (Serial.available())
  {
    Serial.readBytes(buf, 64);
  }

  while (true)
  {
    // send request at 10hz
    if (millis() - millisStart > 100 && !started)
    {
      millisStart = millis();
      buf[0] = REQUEST_ALL_PROCESSES;
      Serial.write(buf, 64);
    }

    if (Serial.available() >= 64)
    {
      started = true;
      Serial.readBytes(buf, 64);
      a = update(buf);
      if (a == 1)
      {
        break;
      }
    }
  }
}

void getMaxVolumes() // get the max volumes of the 7 fader channels from the computer
{
  char buf[64];
  int iterator = 1;
  buf[0] = REQUEST_CHANNEL_DATA;
  buf[5] = 1; // request master
  Serial.write(buf, 64);
  while (true)
  {
    if (Serial.available() >= 64)
    {
      Serial.readBytes(buf, 64);
      update(buf);
      if (iterator > 8)
      {
        break;
      }
      buf[0] = REQUEST_CHANNEL_DATA;
      buf[1] = faderChannel[iterator].appdata.PID;
      buf[2] = faderChannel[iterator].appdata.PID >> 8;
      buf[3] = faderChannel[iterator].appdata.PID >> 16;
      buf[4] = faderChannel[iterator].appdata.PID >> 24;
      buf[5] = 0; // request channel data
      Serial.write(buf, 64);
      iterator++;
    }
  }
}

int requestIcon(int pid) // request the icon of a process from the computer UNSAFE, the computer cannot send any other data while this is happening
{

  iconWidth = 0;
  iconHeight = 0;
  iconPID = 0;
  iconPages = 0;
  currentIconPage = 0;
  char buf[64];
  buf[0] = REQUEST_ICON;
  buf[1] = pid;
  buf[2] = pid >> 8;
  buf[3] = pid >> 16;
  buf[4] = pid >> 24;
  Serial.write(buf, 64);
  while (true)
  {
    if (Serial.available() >= 64)
    {
      Serial.readBytes(buf, 64);

      if (receivingIcon)
      { // each pixel is 2 bytes and we are expecting a 128x128 icon
        // icons are sent left to right, top to bottom in 64 byte chunks
        for (int i = 0; i < 32; i++)
        {
          uint16_t color = buf[i * 2];
          color |= (static_cast<uint16_t>(buf[i * 2 + 1]) << 8);
          // icon is 128x128 matrix, but we are receiving it left to right, top to bottom in 64 byte chunks
          bufferIcon[(int)currentIconPage / 4][i + ((int)currentIconPage % 4) * 32] = color;
        }
        currentIconPage++;
        // send 2 in buf[0] to indicate that we are ready for the next page
        buf[0] = RECEIVED;
        Serial.write(buf, 64);
        if (currentIconPage == iconPages)
        {
          // we have received all the pages of the icon
          receivingIcon = false;
          return 0;
        }
      }
      else if (buf[0] == ICON_PACKETS_INIT)
      {
        iconPacketsInit(buf);
      }
      else if (buf[0] == THE_ICON_REQUESTED_IS_DEFAULT)
      {
        return THE_ICON_REQUESTED_IS_DEFAULT;
      }
    }
  }
  return 0;
}

int update(char buf[64]) // main update function, some functions use the return value to indicate that they are done
{

  switch (buf[0])
  {
  // case UNDEFINED:
  // break;
  case PING:
    return PING;
    break;
  case RECEIVED:
    break;
  case ERROR:
    break;
  // case REQUEST_ALL_PROCESSES:
  //  should never receive this
  // break;
  case PROCESS_REQUEST_INIT:
    processRequestsInit(buf);
    break;
  case ALL_CURRENT_PROCESSES:
    allCurrentProcesses(buf);
    break;
  // case START_NORMAL_BROADCASTS:
  //  should never receive this
  // break;
  // case STOP_NORMAL_BROADCASTS:
  //  should never receive this
  // break;
  case REQUEST_CHANNEL_DATA:
    // should never receive this
    break;
  case CHANNEL_DATA:
    channelData(buf);
    break;
  case PID_CLOSED:
    pidClosed(buf);
    break;
  // case REQUEST_CURRENT_VOLUME_LEVELS:
  //  should never receive this
  // break;
  case SEND_CURRENT_VOLUME_LEVELS:
    receiveCurrentVolumeLevels(buf);
    break;
  // case REQUEST_SELECTED_PROCESSES:
  //  should never receive this
  // break;
  // case CURRENT_SELECTED_PROCESSES:
  // break;
  // case READY:
  //  should never receive this
  // break;
  case NEW_PID:
    newPID(buf);
    break;
  case CHANGE_OF_MAX_VOLUME:
    changeOfMaxVolume(buf);
    break;
  // case MUTE_PROCESS:
  //  handled in changeOfMaxVolume
  // break;
  case CHANGE_OF_MASTER_MAX:
    changeOfMasterMax(buf);
    break;
  // case MUTE_MASTER:
  //  handled in changeOfMasterMax
  // break;
  // case MUTE_ALL_EXCEPT:
  // todo
  // break;
  // case UNMUTE_ALL:
  // todo
  // break;
  // case REQUEST_ICON:
  //  should never receive this
  // break;
  case ICON_PACKETS_INIT:
    iconPacketsInit(buf);
    break;
  case THE_ICON_REQUESTED_IS_DEFAULT:
    return THE_ICON_REQUESTED_IS_DEFAULT;
    break;
  case BUTTON_PUSHED:
    break;
  default:
    while (Serial.available()) // trt to get back in sync
    {
      Serial.read();
    }
    break;
  }
  return 0;
}

void newPID(char buf[64]) // triggered when new volume process is opened up on the computer
{
  int pid = buf[1];
  pid |= (static_cast<int>(buf[2]) << 8);
  pid |= (static_cast<int>(buf[3]) << 16);
  pid |= (static_cast<int>(buf[4]) << 24);
  char name[20]; // 20 bytes for name
  for (int i = 0; i < 20; i++)
  {
    name[i] = buf[i + 5];
  }
  // next byte is volume
  int volume = buf[25];
  // next byte is mute
  bool mute = buf[26];
  for (int i = 0; i < 8; i++)
  {
    if (faderChannel[i].isUnUsed)
    {
      faderChannel[i].isUnUsed = false;
      faderChannel[i].appdata.PID = pid;
      faderChannel[i].setName(name);
      faderChannel[i].maxVolume = volume;
      faderChannel[i].setMute(mute);
      char buf2[64];
      buf2[0] = STOP_NORMAL_BROADCASTS;
      Serial.write(buf2, 64);
      int a = requestIcon(pid);
      if (a == THE_ICON_REQUESTED_IS_DEFAULT)
      {
        faderChannel[i].setIcon(defaultIcon, 128, 128);
      }
      else
      {
        faderChannel[i].setIcon(bufferIcon, 128, 128);
      }
      sendCurrentSelectedProcesses();
      buf2[0] = START_NORMAL_BROADCASTS;
      Serial.write(buf2, 64);
      return;
    }
  }
}

void pidClosed(char buf[64]) // triggered when volume process is closed on the computer
{
  int channel = 0;
  int pid = buf[1];
  pid |= (static_cast<int>(buf[2]) << 8);
  pid |= (static_cast<int>(buf[3]) << 16);
  pid |= (static_cast<int>(buf[4]) << 24);
  for (int i = 0; i < 8; i++)
  {
    if (faderChannel[i].appdata.PID == pid)
    {
      faderChannel[i].setUnused();
      channel = i;
    }
  }
  char buf2[64];
  buf2[0] = STOP_NORMAL_BROADCASTS;
  Serial.write(buf2, 64);
  requestAllProcesses();
  if (channel != 0)
  {
    for (int i = 0; i < openProcessIndex; i++)
    {
      bool inUse = false;
      for (int j = 0; j < 8; j++)
      {
        if (faderChannel[j].appdata.PID == openProcessIDs[i])
        {
          inUse = true;
        }
      }
      if (!inUse)
      {
        faderChannel[channel].appdata.PID = openProcessIDs[i];
        int a = requestIcon(openProcessIDs[i]);
        if (a == THE_ICON_REQUESTED_IS_DEFAULT)
        {
          faderChannel[channel].setIcon(defaultIcon, 128, 128);
        }
        else
        {
          faderChannel[channel].setIcon(bufferIcon, 128, 128);
        }
        char buf[64];
        buf[0] = REQUEST_CHANNEL_DATA;
        buf[1] = faderChannel[channel].appdata.PID;
        buf[2] = faderChannel[channel].appdata.PID >> 8;
        buf[3] = faderChannel[channel].appdata.PID >> 16;
        buf[4] = faderChannel[channel].appdata.PID >> 24;
        Serial.write(buf, 64);
        break;
      }
    }
  }
  sendCurrentSelectedProcesses();
  buf2[0] = START_NORMAL_BROADCASTS;
  Serial.write(buf2, 64);
  buf2[0] = READY;
  Serial.write(buf2, 64);
}

void updateProcess(int i) // triggered when a fader manually selected a new process
{
  char buf2[64];
  buf2[0] = STOP_NORMAL_BROADCASTS;
  Serial.write(buf2, 64);
  int a = requestIcon(faderChannel[i].appdata.PID);
  if (a == THE_ICON_REQUESTED_IS_DEFAULT)
  {
    faderChannel[i].setIcon(defaultIcon, 128, 128);
  }
  else
  {
    faderChannel[i].setIcon(bufferIcon, 128, 128);
  }
  char buf[64];
  buf[0] = REQUEST_CHANNEL_DATA;
  buf[1] = faderChannel[i].appdata.PID;
  buf[2] = faderChannel[i].appdata.PID >> 8;
  buf[3] = faderChannel[i].appdata.PID >> 16;
  buf[4] = faderChannel[i].appdata.PID >> 24;
  Serial.write(buf, 64);
  faderChannel[i].requestNewProcess = false;
  sendCurrentSelectedProcesses();
  buf2[0] = START_NORMAL_BROADCASTS;
  Serial.write(buf2, 64);
}

void sendChangeOfMaxVolume(int _channelNumber) // sends the current max volume of a channel to the computer
{
  char buf[64];
  if (_channelNumber == 0)
  {
    buf[0] = CHANGE_OF_MASTER_MAX;
    buf[1] = faderChannel[0].getPosition();
    buf[2] = faderChannel[0].isMuted;
  }
  else
  {
    buf[0] = CHANGE_OF_MAX_VOLUME;
    buf[1] = faderChannel[_channelNumber].appdata.PID;
    buf[2] = faderChannel[_channelNumber].appdata.PID >> 8;
    buf[3] = faderChannel[_channelNumber].appdata.PID >> 16;
    buf[4] = faderChannel[_channelNumber].appdata.PID >> 24;
    buf[5] = faderChannel[_channelNumber].getPosition();
    buf[6] = faderChannel[_channelNumber].isMuted;
  }
  Serial.write(buf, 64);
}

void changeOfMaxVolume(char buf[64]) // triggered when the max volume of a channel is changed on the computer
{
  uint32_t pid = buf[1];
  pid |= (static_cast<uint32_t>(buf[2]) << 8);
  pid |= (static_cast<uint32_t>(buf[3]) << 16);
  pid |= (static_cast<uint32_t>(buf[4]) << 24);
  int maxVolume = buf[5];
  bool mute = buf[6];
  for (int i = 1; i < 8; i++)
  {
    if (faderChannel[i].appdata.PID == pid)
    {
      faderChannel[i].setMaxVolume(maxVolume);
      faderChannel[i].setMute(mute);
      break;
    }
  }
}

void changeOfMasterMax(char buf[64]) // triggered when the max volume of the master channel is changed on the computer
{
  int maxVolume = buf[1];
  bool mute = buf[2];
  faderChannel[0].setMaxVolume(maxVolume);
  faderChannel[0].setMute(mute);
}

void receiveCurrentVolumeLevels(char buf[64]) // computer sends volume levels of all channels
{
  uint32_t pid = 0; // byte 1 through 4 is the pid, byte 5 is the volume level, then repeat
  int volumeLevel = 0;
  for (int i = 0; i < 7; i++)
  {
    pid = buf[i * 5 + 1];
    pid |= (static_cast<uint32_t>(buf[i * 5 + 2]) << 8);
    pid |= (static_cast<uint32_t>(buf[i * 5 + 3]) << 16);
    pid |= (static_cast<uint32_t>(buf[i * 5 + 4]) << 24);
    volumeLevel = buf[i * 5 + 5];
    for (int j = 1; j < 8; j++)
    {
      if (faderChannel[j].appdata.PID == pid)
      {
        faderChannel[j].setCurrentVolume(volumeLevel);
      }
    }
  }
}

void ping() // not implemented yet (not sure if it will be)
{
}

void processRequestsInit(char buf[64]) // tells the compuuter we want to receive all current processes
{
  numChannels = buf[1];
  numPages = buf[2];
  openProcessIndex = 0;
  buf[0] = RECEIVED;
  Serial.write(buf, 64);
}

void allCurrentProcesses(char buf[64]) // triggered when the computer sends all current processes
{
  int pid = 0;
  pid |= buf[1];
  pid |= (static_cast<uint32_t>(buf[2]) << 8);
  pid |= (static_cast<uint32_t>(buf[3]) << 16);
  pid |= (static_cast<uint32_t>(buf[4]) << 24);
  openProcessIDs[openProcessIndex] = pid;
  for (int i = 0; i < 20; i++)
  {
    openProcessNames[openProcessIndex][i] = buf[i + 5];
  }
  openProcessIndex++;
  if (openProcessIndex != numChannels)
  {
    pid = 0;
    pid |= buf[25];
    pid |= (static_cast<uint32_t>(buf[26]) << 8);
    pid |= (static_cast<uint32_t>(buf[27]) << 16);
    pid |= (static_cast<uint32_t>(buf[28]) << 24);
    openProcessIDs[openProcessIndex] = pid;
    for (int i = 0; i < 20; i++)
    {
      openProcessNames[openProcessIndex][i] = buf[i + 29];
    }
    openProcessIndex++;
  }

  if (numChannels == openProcessIndex)
  {

    if (faderRequest != -1)
    {
      faderChannel[faderRequest].menuOpen = true;
      faderChannel[faderRequest].updateScreen = true;
      faderChannel[faderRequest].processNames = &openProcessNames;
      faderChannel[faderRequest].processIDs = &openProcessIDs;
      faderChannel[faderRequest].processIndex = openProcessIndex;
      faderRequest = -1;
    }
  }

  // send received
  buf[0] = RECEIVED;
  Serial.write(buf, 64);
}

void iconPacketsInit(char buf[64]) // computer sends info about the icon it is about to send
{
  receivingIcon = true;
  iconPID |= buf[1];
  iconPID |= (static_cast<uint32_t>(buf[2]) << 8);
  iconPID |= (static_cast<uint32_t>(buf[3]) << 16);
  iconPID |= (static_cast<uint32_t>(buf[4]) << 24);
  iconWidth = buf[5];
  iconHeight = buf[6];
  iconPages |= buf[7];
  iconPages |= (static_cast<uint32_t>(buf[8]) << 8);
  iconPages |= (static_cast<uint32_t>(buf[9]) << 16);
  iconPages |= (static_cast<uint32_t>(buf[10]) << 24);
  // send 2 in buf[0] to indicate that we are ready for the first page
  buf[0] = RECEIVED;
  Serial.write(buf, 64);
}

void channelData(char buf[64]) // computer sends info about a process
{
  int isMaster = buf[1];
  int maxVolume = buf[2];
  bool isMuted = buf[3];
  int pid = 0;
  pid |= buf[4];
  pid |= (static_cast<uint32_t>(buf[5]) << 8);
  pid |= (static_cast<uint32_t>(buf[6]) << 16);
  pid |= (static_cast<uint32_t>(buf[7]) << 24);
  char name[20];
  for (int i = 0; i < 20; i++)
  {
    name[i] = buf[i + 8];
  }
  if (isMaster == 1)
  {
    faderChannel[0].setMute(isMuted);
    faderChannel[0].setMaxVolume(maxVolume);
    faderChannel[0].setName(name);
  }
  else
  {
    // find the matching channel from the pid
    for (int i = 1; i < 8; i++)
    {
      if (faderChannel[i].appdata.PID == pid)
      {
        faderChannel[i].setMute(isMuted);
        faderChannel[i].setMaxVolume(maxVolume);
        faderChannel[i].setName(name);
      }
    }
  }
}

void uncaughtException() // something horrible happened
{
  tft.fillScreen(ST77XX_RED);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(0, 0);
  tft.println("Uncaught Error.");
  tft.println("Please restart.");
  tft.updateScreen();
  while (true)
    ;
}
