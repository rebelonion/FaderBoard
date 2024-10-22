#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "icons.h"

#include "FaderChannel.h"

#define TimeOut 5000

// Functions
/**************************************************/
void init();
[[noreturn]] void uncaughtException();
void iconPacketsInit(char buf[PACKET_SIZE]);
void allCurrentProcesses(char buf[PACKET_SIZE]);
void processRequestsInit(char buf[PACKET_SIZE]);
uint8_t update(char *buf);
uint8_t requestIcon(uint32_t pid);
void requestAllProcesses();
void receiveCurrentVolumeLevels(const char buf[PACKET_SIZE]);
void sendCurrentSelectedProcesses();
void channelData(const char buf[PACKET_SIZE]);
void getMaxVolumes();
void changeOfMaxVolume(const char buf[PACKET_SIZE]);
void changeOfMasterMax(const char buf[PACKET_SIZE]);
void sendChangeOfMaxVolume(uint8_t _channelNumber);
void pidClosed(const char buf[PACKET_SIZE]);
void newPID(const char buf[PACKET_SIZE]);
void updateProcess(uint32_t i);


// Transitory Variables for passing data around
uint32_t iconPages = 0;
uint32_t currentIconPage = 0;
uint32_t iconPID = 0;
int numPages = 0;
int numChannels = 0;
/**************************************************/

// flags
int faderRequest = -1;

/**************************************************/


FaderChannel faderChannels[CHANNELS] = { // 8 fader channels
    FaderChannel(0, &LEDs, &analog, &capSensor, &tft, 1, 2, true),
    FaderChannel(1, &LEDs, &analog, &capSensor, &tft, 3, 4, false),
    FaderChannel(2, &LEDs, &analog, &capSensor, &tft, 5, 6, false),
    FaderChannel(3, &LEDs, &analog, &capSensor, &tft, 7, 8, false),
    FaderChannel(4, &LEDs, &analog, &capSensor, &tft, 24, 25, false),
    FaderChannel(5, &LEDs, &analog, &capSensor, &tft, 28, 29, false),
    FaderChannel(6, &LEDs, &analog, &capSensor, &tft, 14, 15, false),
    FaderChannel(7, &LEDs, &analog, &capSensor, &tft, 22, 23, false)};

void setup()
{
  pinMode(POT_INPUT, INPUT);
  Serial.begin(115200);
  LEDs.begin();
  pinMode(CS_LOCK, OUTPUT);
  digitalWrite(CS_LOCK, LOW);
  tft.init(SCREEN_WIDTH, SCREEN_HEIGHT, SPI_MODE2);
  tft.useFrameBuffer(true);
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
    if (i < CHANNELS)
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
  for (int i = 0; i < CHANNELS; i++) // set up the encoders and buttons
  {
    encoder[i].begin();
    enButton[i].begin();
    leButton[2 * i].begin();
    leButton[2 * i + 1].begin();
    faderChannels[i].begin();
  }

  // clear up any data that may be in the buffer (flush didn't work for some reason)
  while (Serial.available()) {
    char buf[PACKET_SIZE];
    Serial.readBytes(buf, PACKET_SIZE);
  }
  digitalWrite(CS_LOCK, HIGH); // unlock the screens
  LEDs.clear();
  LEDs.show();
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
  for (int i = 0; i < CHANNELS; i++) // update the fade channels
  {
    encoder[i].update(rotaryMux.digitalRead(i * 2), rotaryMux.digitalRead(i * 2 + 1), 2, LOW);
    enButton[i].update(reButtonMux.digitalRead(i), 50, LOW);
    leButton[2 * i].update(leButtonMux.digitalRead(i * 2), 50, LOW);
    leButton[2 * i + 1].update(leButtonMux.digitalRead(i * 2 + 1), 50, LOW);
    if (faderChannels[i].userChanged)
    {
      faderChannels[i].userChanged = false;
      sendChangeOfMaxVolume(i);
    }

    if (faderChannels[i].requestProcessRefresh)
    {
      char sendBuf[PACKET_SIZE];
      sendBuf[0] = REQUEST_ALL_PROCESSES;
      Serial.write(sendBuf, PACKET_SIZE);
      faderRequest = i;
      faderChannels[i].requestProcessRefresh = false;
    }

    if (encoder[i].read())
    {
      faderChannels[i].onRotaryTurn(encoder[i].increased());
    }
    if (enButton[i].pressed())
    {
      faderChannels[i].onRotaryPress();
    }
    if (leButton[2 * i].pressed())
    {
      faderChannels[i].onButtonPress(1);
    }
    if (leButton[2 * i + 1].pressed())
    {
      faderChannels[i].onButtonPress(0);
    }

    if (faderChannels[i].requestNewProcess)
    {
      updateProcess(i);
    }

    faderChannels[i].update();
  }
  if (Serial.available() >= PACKET_SIZE) // check for serial data
  {
    char buf[PACKET_SIZE];
    Serial.readBytes(buf, PACKET_SIZE);
    update(buf);
    LEDs.show();
  }
}

void init() // set fader pot and touch values then get initial data from computer on startup
{
  for (const auto & faderChannel : faderChannels)
  {
    faderChannel.motor->forward(65);
  }
  delay(500);
  for (auto & faderChannel : faderChannels)
  {
    faderChannel.motor->stop();
    faderChannel.setPositionMax();
    faderChannel.motor->backward(65);
  }
  delay(500);
  for (auto & faderChannel : faderChannels)
  {
    faderChannel.motor->stop();
    faderChannel.setPositionMin();
  }
  for (const auto & faderChannel : faderChannels)
  {
    faderChannel.motor->forward(80);
  }
  delay(50);
  for (const auto & faderChannel : faderChannels)
  {
    faderChannel.motor->stop();
  }
  delay(50);
  capSensor.reset_CS_AutoCal();
  for (auto & faderChannel : faderChannels)
  {
    faderChannel.setUnTouched();
  }
  char buf[PACKET_SIZE];
  requestAllProcesses();
  // fill the 7 fader channels with the first 7 processes
  faderChannels[0].setIcon(defaultIcon, ICON_SIZE, ICON_SIZE);
  // faderChannel[0].setName(("Master             "));
  for (uint8_t i = 1; i < CHANNELS; i++)
  {
    if (openProcessIndex >= i)
    {
      faderChannels[i].appdata.PID = openProcessIDs[i - 1];
      // set the name
      faderChannels[i].setName(openProcessNames[i - 1]);
      // request the icon
      if (const uint8_t a = requestIcon(faderChannels[i].appdata.PID); a != THE_ICON_REQUESTED_IS_DEFAULT)
      {
        faderChannels[i].setIcon(bufferIcon, ICON_SIZE, ICON_SIZE);
      }
      else
      {
        faderChannels[i].setIcon(defaultIcon, ICON_SIZE, ICON_SIZE);
      }
    }
    else
    {
      faderChannels[i].appdata.PID = UINT32_MAX;
      faderChannels[i].setName("None");
      faderChannels[i].isUnUsed = true;
    }
  }
  getMaxVolumes();
  for (auto & channel : faderChannels)
  {
    channel.update();
  }
  sendCurrentSelectedProcesses();
  buf[0] = START_NORMAL_BROADCASTS;
  Serial.write(buf, PACKET_SIZE);
  normalBroadcast = true;
}

void sendCurrentSelectedProcesses() // send the processes of the 7 fader channels to the computer
{
  char buf[PACKET_SIZE];
  buf[0] = CURRENT_SELECTED_PROCESSES;
  for (int i = 0; i < 7; i++)
  {
    buf[i * 4 + 1] = faderChannels[i + 1].appdata.PID;
    buf[i * 4 + 2] = faderChannels[i + 1].appdata.PID >> 8;
    buf[i * 4 + 3] = faderChannels[i + 1].appdata.PID >> 16;
    buf[i * 4 + 4] = faderChannels[i + 1].appdata.PID >> 24;
  }
  Serial.write(buf, PACKET_SIZE);
}

void requestAllProcesses() // requests all sound processes from the computer
{
  uint32_t millisStart = millis() - 100;
  char buf[PACKET_SIZE];
  int a = 0;
  bool started = false;
  while (Serial.available())
  {
    Serial.readBytes(buf, PACKET_SIZE);
  }

  while (true)
  {
    // send request at 10hz
    if (millis() - millisStart > 100 && !started)
    {
      millisStart = millis();
      buf[0] = REQUEST_ALL_PROCESSES;
      Serial.write(buf, PACKET_SIZE);
    }

    if (Serial.available() >= PACKET_SIZE)
    {
      started = true;
      Serial.readBytes(buf, PACKET_SIZE);
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
  char buf[PACKET_SIZE];
  int iterator = 1;
  buf[0] = REQUEST_CHANNEL_DATA;
  buf[5] = 1; // request master
  Serial.write(buf, PACKET_SIZE);
  while (true)
  {
    if (Serial.available() >= PACKET_SIZE)
    {
      Serial.readBytes(buf, PACKET_SIZE);
      update(buf);
      if (iterator > 7)
      {
        break;
      }
      buf[0] = REQUEST_CHANNEL_DATA;
      buf[1] = faderChannels[iterator].appdata.PID;
      buf[2] = faderChannels[iterator].appdata.PID >> 8;
      buf[3] = faderChannels[iterator].appdata.PID >> 16;
      buf[4] = faderChannels[iterator].appdata.PID >> 24;
      buf[5] = 0; // request channel data
      Serial.write(buf, PACKET_SIZE);
      iterator++;
    }
  }
}

uint8_t requestIcon(const uint32_t pid) // request the icon of a process from the computer UNSAFE, the computer cannot send any other data while this is happening
{
  iconPID = 0;
  iconPages = 0;
  currentIconPage = 0;
  char buf[PACKET_SIZE];
  buf[0] = REQUEST_ICON;
  buf[1] = pid;
  buf[2] = pid >> 8;
  buf[3] = pid >> 16;
  buf[4] = pid >> 24;
  Serial.write(buf, PACKET_SIZE);
  while (true)
  {
    if (Serial.available() >= PACKET_SIZE)
    {
      Serial.readBytes(buf, PACKET_SIZE);

      if (receivingIcon)
      { // each pixel is 2 bytes and we are expecting a 128x128 icon
        // icons are sent left to right, top to bottom in PACKET_SIZE byte chunks
        for (uint8_t i = 0; i < 32; i++)
        {
          uint16_t color = buf[i * 2];
          color |= (static_cast<uint16_t>(buf[i * 2 + 1]) << 8);
          // icon is 128x128 matrix, but we are receiving it left to right, top to bottom in 64 byte chunks
          bufferIcon[currentIconPage / 4][i + (currentIconPage % 4) * 32] = color;
        }
        currentIconPage++;
        // send 2 in buf[0] to indicate that we are ready for the next page
        buf[0] = ACK;
        Serial.write(buf, PACKET_SIZE);
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
}

uint8_t update(char *buf) // main update function, some functions use the return value to indicate that they are done
{

  switch (buf[0])
  {
  // case UNDEFINED:
  // break;
  case ACK | NACK:
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

void newPID(const char buf[PACKET_SIZE]) // triggered when new volume process is opened up on the computer
{
  uint32_t pid = buf[1];
  pid |= (static_cast<uint32_t>(buf[2]) << 8);
  pid |= (static_cast<uint32_t>(buf[3]) << 16);
  pid |= (static_cast<uint32_t>(buf[4]) << 24);
  char name[20]; // 20 bytes for name
  for (uint8_t i = 0; i < 20; i++)
  {
    name[i] = buf[i + 5];
  }
  // next byte is volume
  const uint8_t volume = buf[25];
  // next byte is mute
  const bool mute = buf[26];
  for (auto & channel : faderChannels)
  {
    if (channel.isUnUsed)
    {
      channel.isUnUsed = false;
      channel.appdata.PID = pid;
      channel.setName(name);
      channel.targetVolume = volume;
      channel.setMute(mute);
      char buf2[PACKET_SIZE];
      buf2[0] = STOP_NORMAL_BROADCASTS;
      Serial.write(buf2, PACKET_SIZE);
      if (const uint8_t a = requestIcon(pid); a == THE_ICON_REQUESTED_IS_DEFAULT)
      {
        channel.setIcon(defaultIcon, ICON_SIZE, ICON_SIZE);
      }
      else
      {
        channel.setIcon(bufferIcon, ICON_SIZE, ICON_SIZE);
      }
      sendCurrentSelectedProcesses();
      buf2[0] = START_NORMAL_BROADCASTS;
      Serial.write(buf2, PACKET_SIZE);
      return;
    }
  }
}

void pidClosed(const char buf[PACKET_SIZE]) // triggered when volume process is closed on the computer
{
  uint8_t channelIndex = 0;
  uint32_t pid = buf[1];
  pid |= static_cast<uint32_t>(buf[2]) << 8;
  pid |= static_cast<uint32_t>(buf[3]) << 16;
  pid |= static_cast<uint32_t>(buf[4]) << 24;
  for (uint8_t i = 0; i < CHANNELS; i++)
  {
    if (faderChannels[i].appdata.PID == pid)
    {
      faderChannels[i].setUnused();
      channelIndex = i;
    }
  }
  char buf2[PACKET_SIZE];
  buf2[0] = STOP_NORMAL_BROADCASTS;
  Serial.write(buf2, PACKET_SIZE);
  if (channelIndex != 0)
  {
    for (uint8_t i = 0; i < openProcessIndex; i++)
    {
      bool inUse = false;
      for (const auto & channel : faderChannels)
      {
        if (channel.appdata.PID == openProcessIDs[i])
        {
          inUse = true;
        }
      }
      if (!inUse)
      {
        faderChannels[channelIndex].appdata.PID = openProcessIDs[i];
        if (const uint8_t a = requestIcon(openProcessIDs[i]); a == THE_ICON_REQUESTED_IS_DEFAULT)
        {
          faderChannels[channelIndex].setIcon(defaultIcon, ICON_SIZE, ICON_SIZE);
        }
        else
        {
          faderChannels[channelIndex].setIcon(bufferIcon, ICON_SIZE, ICON_SIZE);
        }
        char buf3[PACKET_SIZE];  //TODO: optimize buf creation
        buf3[0] = REQUEST_CHANNEL_DATA;
        buf3[1] = faderChannels[channelIndex].appdata.PID;
        buf3[2] = faderChannels[channelIndex].appdata.PID >> 8;
        buf3[3] = faderChannels[channelIndex].appdata.PID >> 16;
        buf3[4] = faderChannels[channelIndex].appdata.PID >> 24;
        Serial.write(buf3, PACKET_SIZE);
        break;
      }
    }
  }
  sendCurrentSelectedProcesses();
  buf2[0] = START_NORMAL_BROADCASTS;
  Serial.write(buf2, PACKET_SIZE);
  buf2[0] = READY;
  Serial.write(buf2, PACKET_SIZE);
}

void updateProcess(const uint32_t i) // triggered when a fader manually selected a new process
{
  char buf2[PACKET_SIZE];
  buf2[0] = STOP_NORMAL_BROADCASTS;
  Serial.write(buf2, PACKET_SIZE);
  if (const uint8_t a = requestIcon(faderChannels[i].appdata.PID); a == THE_ICON_REQUESTED_IS_DEFAULT)
  {
    faderChannels[i].setIcon(defaultIcon, ICON_SIZE, ICON_SIZE);
  }
  else
  {
    faderChannels[i].setIcon(bufferIcon, ICON_SIZE, ICON_SIZE);
  }
  char buf[PACKET_SIZE];
  buf[0] = REQUEST_CHANNEL_DATA;
  buf[1] = faderChannels[i].appdata.PID;
  buf[2] = faderChannels[i].appdata.PID >> 8;
  buf[3] = faderChannels[i].appdata.PID >> 16;
  buf[4] = faderChannels[i].appdata.PID >> 24;
  Serial.write(buf, PACKET_SIZE);
  faderChannels[i].requestNewProcess = false;
  sendCurrentSelectedProcesses();
  buf2[0] = START_NORMAL_BROADCASTS;
  Serial.write(buf2, PACKET_SIZE);
}

void sendChangeOfMaxVolume(const uint8_t _channelNumber) // sends the current max volume of a channel to the computer
{
  char buf[PACKET_SIZE];
  if (_channelNumber == 0)
  {
    buf[0] = CHANGE_OF_MASTER_MAX;
    buf[1] = faderChannels[0].getFaderPosition();
    buf[2] = faderChannels[0].isMuted;
  }
  else
  {
    buf[0] = CHANGE_OF_MAX_VOLUME;
    buf[1] = faderChannels[_channelNumber].appdata.PID;
    buf[2] = faderChannels[_channelNumber].appdata.PID >> 8;
    buf[3] = faderChannels[_channelNumber].appdata.PID >> 16;
    buf[4] = faderChannels[_channelNumber].appdata.PID >> 24;
    buf[5] = faderChannels[_channelNumber].getFaderPosition();
    buf[6] = faderChannels[_channelNumber].isMuted;
  }
  Serial.write(buf, PACKET_SIZE);
}

void changeOfMaxVolume(const char buf[PACKET_SIZE]) // triggered when the max volume of a channel is changed on the computer
{
  uint32_t pid = buf[1];
  pid |= static_cast<uint32_t>(buf[2]) << 8;
  pid |= static_cast<uint32_t>(buf[3]) << 16;
  pid |= static_cast<uint32_t>(buf[4]) << 24;
  const uint8_t maxVolume = buf[5];
  const bool mute = buf[6];
  for (int i = 1; i < CHANNELS; i++)
  {
    if (faderChannels[i].appdata.PID == pid)
    {
      faderChannels[i].setMaxVolume(maxVolume);
      faderChannels[i].setMute(mute);
      break;
    }
  }
}

void changeOfMasterMax(const char buf[PACKET_SIZE]) // triggered when the max volume of the master channel is changed on the computer
{
  const uint8_t maxVolume = buf[1];
  const bool mute = buf[2];
  faderChannels[0].setMaxVolume(maxVolume);
  faderChannels[0].setMute(mute);
}

void receiveCurrentVolumeLevels(const char buf[PACKET_SIZE]) // computer sends volume levels of all channels
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
    for (int j = 1; j < CHANNELS; j++)
    {
      if (faderChannels[j].appdata.PID == pid)
      {
        faderChannels[j].setCurrentVolume(volumeLevel);
      }
    }
  }
}

void processRequestsInit(char buf[PACKET_SIZE]) // tells the compuuter we want to receive all current processes
{
  numChannels = buf[1];
  numPages = buf[2];
  openProcessIndex = 0;
  buf[0] = ACK;
  Serial.write(buf, PACKET_SIZE);
}

void allCurrentProcesses(char buf[PACKET_SIZE]) // triggered when the computer sends all current processes
{
  uint32_t pid = 0;
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
      faderChannels[faderRequest].menuOpen = true;
      faderChannels[faderRequest].updateScreen = true;
      faderChannels[faderRequest].processNames = &openProcessNames;
      faderChannels[faderRequest].processIDs = &openProcessIDs;
      faderChannels[faderRequest].processIndex = openProcessIndex;
      faderRequest = -1;
    }
  }

  // send received
  buf[0] = ACK;
  Serial.write(buf, PACKET_SIZE);
}

void iconPacketsInit(char buf[PACKET_SIZE]) // computer sends info about the icon it is about to send
{
  receivingIcon = true;
  iconPID |= buf[1];
  iconPID |= (static_cast<uint32_t>(buf[2]) << 8);
  iconPID |= (static_cast<uint32_t>(buf[3]) << 16);
  iconPID |= (static_cast<uint32_t>(buf[4]) << 24);
  //iconWidth = buf[5];  deprecated, all icons will be sent as 128x128
  //iconHeight = buf[6];
  iconPages |= buf[7];
  iconPages |= (static_cast<uint32_t>(buf[8]) << 8);
  iconPages |= (static_cast<uint32_t>(buf[9]) << 16);
  iconPages |= (static_cast<uint32_t>(buf[10]) << 24);
  // send ACK in buf[0] to indicate that we are ready for the first page
  buf[0] = ACK;
  Serial.write(buf, PACKET_SIZE);
}

void channelData(const char buf[PACKET_SIZE]) // computer sends info about a process
{
  const uint8_t isMaster = buf[1];
  const uint8_t maxVolume = buf[2];
  const bool isMuted = buf[3];
  uint32_t pid = 0;
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
    faderChannels[0].setMute(isMuted);
    faderChannels[0].setMaxVolume(maxVolume);
    faderChannels[0].setName(name);
  }
  else
  {
    // find the matching channel from the pid
    for (int i = 1; i < CHANNELS; i++)
    {
      if (faderChannels[i].appdata.PID == pid)
      {
        faderChannels[i].setMute(isMuted);
        faderChannels[i].setMaxVolume(maxVolume);
        faderChannels[i].setName(name);
      }
    }
  }
}

[[noreturn]] void uncaughtException() // something horrible happened
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
