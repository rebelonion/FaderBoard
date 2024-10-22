#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "icons.h"
#include "packets/RecNewPID.h"
#include "packets/RecChannelData.h"
#include "packets/RecIconPacketInit.h"
#include "packets/RecAllCurrentProcesses.h"
#include "packets/RecProcessRequestInit.h"
#include "packets/RecCurrentVolumeLevels.h"
#include "packets/RecMasterMaxChange.h"
#include "packets/RecMaxChange.h"
#include "packets/RecPIDClosed.h"
#include "FaderChannel.h"

// Functions
/**************************************************/
void init();

[[noreturn]] void uncaughtException(const String &message = "");

void iconPacketsInit(uint8_t buf[PACKET_SIZE]);

void allCurrentProcesses(uint8_t buf[PACKET_SIZE]);

void processRequestsInit(uint8_t buf[PACKET_SIZE]);

uint8_t update(uint8_t *buf);

uint8_t requestIcon(uint32_t pid);

void requestAllProcesses();

void receiveCurrentVolumeLevels(const uint8_t buf[PACKET_SIZE]);

void sendCurrentSelectedProcesses();

void channelData(const uint8_t buf[PACKET_SIZE]);

void getMaxVolumes();

void changeOfMaxVolume(const uint8_t buf[PACKET_SIZE]);

void changeOfMasterMax(const uint8_t buf[PACKET_SIZE]);

void sendChangeOfMaxVolume(uint8_t _channelNumber);

void pidClosed(uint8_t buf[PACKET_SIZE]);

void newPID(const uint8_t buf[PACKET_SIZE]);

void updateProcess(uint32_t i);


// Transitory Variables for passing data around
uint32_t iconPages = 0;
uint32_t currentIconPage = 0;
uint32_t iconPID = 0;
int numPackets = 0;
int numChannels = 0;
/**************************************************/

// flags
int faderRequest = -1;

/**************************************************/


FaderChannel faderChannels[CHANNELS] = {
    // 8 fader channels
    FaderChannel(0, &LEDs, &analog, &capSensor, &tft, 1, 2, true),
    FaderChannel(1, &LEDs, &analog, &capSensor, &tft, 3, 4, false),
    FaderChannel(2, &LEDs, &analog, &capSensor, &tft, 5, 6, false),
    FaderChannel(3, &LEDs, &analog, &capSensor, &tft, 7, 8, false),
    FaderChannel(4, &LEDs, &analog, &capSensor, &tft, 24, 25, false),
    FaderChannel(5, &LEDs, &analog, &capSensor, &tft, 28, 29, false),
    FaderChannel(6, &LEDs, &analog, &capSensor, &tft, 14, 15, false),
    FaderChannel(7, &LEDs, &analog, &capSensor, &tft, 22, 23, false)
};

void setup() {
    pinMode(POT_INPUT, INPUT);
    Serial.begin(9600);
    usb_rawhid_configure();
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
    for (int i = 0; i < 16; i++) {
        rotaryMux.pinMode(i, INPUT_PULLUP);
        if (i < CHANNELS) {
            reButtonMux.pinMode(i, INPUT_PULLUP);
        } else {
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

    // clear up any data that may be in the buffer
    while (usb_rawhid_available()) {
        uint8_t buf[PACKET_SIZE];
        usb_rawhid_recv(buf, 0);
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

void loop() {
    reButtonMux.update(); // update the encoders and buttons
    leButtonMux.update();
    rotaryMux.update();
    for (int i = 0; i < CHANNELS; i++) // update the fade channels
    {
        encoder[i].update(rotaryMux.digitalRead(i * 2), rotaryMux.digitalRead(i * 2 + 1), 2, LOW);
        enButton[i].update(reButtonMux.digitalRead(i), 50, LOW);
        leButton[2 * i].update(leButtonMux.digitalRead(i * 2), 50, LOW);
        leButton[2 * i + 1].update(leButtonMux.digitalRead(i * 2 + 1), 50, LOW);
        if (faderChannels[i].userChanged) {
            faderChannels[i].userChanged = false;
            sendChangeOfMaxVolume(i);
        }

        if (faderChannels[i].requestProcessRefresh) {
            char sendBuf[PACKET_SIZE];
            sendBuf[0] = REQUEST_ALL_PROCESSES;
            usb_rawhid_send(sendBuf, TIMEOUT);
            faderRequest = i;
            faderChannels[i].requestProcessRefresh = false;
        }

        if (encoder[i].read()) {
            faderChannels[i].onRotaryTurn(encoder[i].increased());
        }
        if (enButton[i].pressed()) {
            faderChannels[i].onRotaryPress();
        }
        if (leButton[2 * i].pressed()) {
            faderChannels[i].onButtonPress(1);
        }
        if (leButton[2 * i + 1].pressed()) {
            faderChannels[i].onButtonPress(0);
        }

        if (faderChannels[i].requestNewProcess) {
            updateProcess(i);
        }

        faderChannels[i].update();
    }
    if (usb_rawhid_available() >= PACKET_SIZE) {
        uint8_t buf[PACKET_SIZE];
        usb_rawhid_recv(buf, TIMEOUT);
        update(buf);
        LEDs.show();
    }
}

// set fader pot and touch values then get initial data from computer on startup
void init() {
    for (const auto &faderChannel: faderChannels) {
        faderChannel.motor->forward(65);
    }
    delay(500);
    for (auto &faderChannel: faderChannels) {
        faderChannel.motor->stop();
        faderChannel.setPositionMax();
        faderChannel.motor->backward(65);
    }
    delay(500);
    for (auto &faderChannel: faderChannels) {
        faderChannel.motor->stop();
        faderChannel.setPositionMin();
    }
    for (const auto &faderChannel: faderChannels) {
        faderChannel.motor->forward(80);
    }
    delay(50);
    for (const auto &faderChannel: faderChannels) {
        faderChannel.motor->stop();
    }
    delay(50);
    capSensor.reset_CS_AutoCal();
    for (auto &faderChannel: faderChannels) {
        faderChannel.setUnTouched();
    }
    uint8_t buf[PACKET_SIZE];
    requestAllProcesses();
    // fill the 7 fader channels with the first 7 processes
    faderChannels[0].setIcon(defaultIcon, ICON_SIZE, ICON_SIZE);
    // faderChannel[0].setName(("Master             "));
    for (uint8_t i = 1; i < CHANNELS; i++) {
        if (openProcessIndex >= i) {
            faderChannels[i].appdata.PID = openProcessIDs[i - 1];
            // set the name
            faderChannels[i].setName(openProcessNames[i - 1]);
            // request the icon
            if (const uint8_t a = requestIcon(faderChannels[i].appdata.PID); a != THE_ICON_REQUESTED_IS_DEFAULT) {
                faderChannels[i].setIcon(bufferIcon, ICON_SIZE, ICON_SIZE);
            } else {
                faderChannels[i].setIcon(defaultIcon, ICON_SIZE, ICON_SIZE);
            }
        } else {
            faderChannels[i].appdata.PID = UINT32_MAX;
            faderChannels[i].setName("None");
            faderChannels[i].isUnUsed = true;
        }
    }
    getMaxVolumes();
    for (auto &channel: faderChannels) {
        channel.update();
    }
    sendCurrentSelectedProcesses();
    buf[0] = START_NORMAL_BROADCASTS;
    usb_rawhid_send(buf, TIMEOUT);
    normalBroadcast = true;
}

// send the processes of the 7 fader channels to the computer
void sendCurrentSelectedProcesses() {
    uint8_t buf[PACKET_SIZE];
    buf[0] = CURRENT_SELECTED_PROCESSES;
    for (int i = 0; i < 7; i++) {
        buf[i * 4 + 1] = faderChannels[i + 1].appdata.PID;
        buf[i * 4 + 2] = faderChannels[i + 1].appdata.PID >> 8;
        buf[i * 4 + 3] = faderChannels[i + 1].appdata.PID >> 16;
        buf[i * 4 + 4] = faderChannels[i + 1].appdata.PID >> 24;
    }
    usb_rawhid_send(buf, TIMEOUT);
}

// requests all sound processes from the computer
void requestAllProcesses() {
    uint32_t millisStart = millis() - 100;
    uint8_t buf[PACKET_SIZE];
    int a = 0;
    bool started = false;

    while (true) {
        // send request at 10hz
        if (millis() - millisStart > 100 && !started) {
            millisStart = millis();
            buf[0] = REQUEST_ALL_PROCESSES;
            usb_rawhid_send(buf, TIMEOUT);
        }

        if (usb_rawhid_available() >= PACKET_SIZE) {
            started = true;
            usb_rawhid_recv(buf, TIMEOUT);
            a = update(buf);
            if (a == 1) //TODO: use a different funtion
            {
                break;
            }
        }
    }
}

// get the max volumes of the 7 fader channels from the computer
void getMaxVolumes() {
    uint8_t buf[PACKET_SIZE];
    int iterator = 1;
    buf[0] = REQUEST_CHANNEL_DATA;
    buf[5] = 1; // request master
    usb_rawhid_send(buf, TIMEOUT);
    while (true) {
        if (usb_rawhid_available() >= PACKET_SIZE) {
            usb_rawhid_recv(buf, TIMEOUT);
            update(buf);
            if (iterator > 7) {
                break;
            }
            buf[0] = REQUEST_CHANNEL_DATA;
            buf[1] = faderChannels[iterator].appdata.PID;
            buf[2] = faderChannels[iterator].appdata.PID >> 8;
            buf[3] = faderChannels[iterator].appdata.PID >> 16;
            buf[4] = faderChannels[iterator].appdata.PID >> 24;
            buf[5] = 0; // request channel data
            usb_rawhid_send(buf, TIMEOUT);
            iterator++;
        }
    }
}

// request the icon of a process from the computer UNSAFE, the computer cannot send any other data while this is happening
uint8_t requestIcon(const uint32_t pid) {
    iconPID = 0;
    iconPages = 0;
    currentIconPage = 0;
    uint8_t buf[PACKET_SIZE];
    buf[0] = REQUEST_ICON;
    buf[1] = pid;
    buf[2] = pid >> 8;
    buf[3] = pid >> 16;
    buf[4] = pid >> 24;
    usb_rawhid_send(buf, TIMEOUT);
    while (true) {
        if (usb_rawhid_available() >= PACKET_SIZE) {
            usb_rawhid_recv(buf, TIMEOUT);

            if (receivingIcon) {
                // each pixel is 2 bytes and we are expecting a 128x128 icon
                // icons are sent left to right, top to bottom in PACKET_SIZE byte chunks
                for (uint8_t i = 0; i < 32; i++) {
                    uint16_t color = buf[i * 2];
                    color |= (static_cast<uint16_t>(buf[i * 2 + 1]) << 8);
                    // icon is 128x128 matrix, but we are receiving it left to right, top to bottom in 64 byte chunks
                    bufferIcon[currentIconPage / 4][i + (currentIconPage % 4) * 32] = color;
                }
                currentIconPage++;
                // send 2 in buf[0] to indicate that we are ready for the next page
                buf[0] = ACK;
                usb_rawhid_send(buf, TIMEOUT);
                if (currentIconPage == iconPages) {
                    // we have received all the pages of the icon
                    receivingIcon = false;
                    return 0;
                }
            } else if (buf[0] == ICON_PACKETS_INIT) {
                iconPacketsInit(buf);
            } else if (buf[0] == THE_ICON_REQUESTED_IS_DEFAULT) {
                return THE_ICON_REQUESTED_IS_DEFAULT;
            }
        }
    }
}

// main update function, some functions use the return value to indicate that they are done
uint8_t update(uint8_t *buf) {
    switch (buf[0]) {
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
            Serial.println("Warning: Unknown packet received: " + String(buf[0]));
            return 1;
    }
    return 0;
}

// triggered when new volume process is opened up on the computer
void newPID(const uint8_t buf[PACKET_SIZE]) {
    const RecNewPID recNewPID(buf);
    const uint32_t pid = recNewPID.getPID();
    char name[NAME_LENGTH_MAX];
    recNewPID.getName(name);
    const uint8_t volume = recNewPID.getVolume();
    const bool mute = recNewPID.isMuted();
    for (auto &channel: faderChannels) {
        if (channel.isUnUsed) {
            channel.isUnUsed = false;
            channel.appdata.PID = pid;
            channel.setName(name);
            channel.targetVolume = volume;
            channel.setMute(mute);
            uint8_t buf2[PACKET_SIZE];
            buf2[0] = STOP_NORMAL_BROADCASTS;
            usb_rawhid_send(buf2, TIMEOUT);
            if (const uint8_t a = requestIcon(pid); a == THE_ICON_REQUESTED_IS_DEFAULT) {
                channel.setIcon(defaultIcon, ICON_SIZE, ICON_SIZE);
            } else {
                channel.setIcon(bufferIcon, ICON_SIZE, ICON_SIZE);
            }
            sendCurrentSelectedProcesses();
            buf2[0] = START_NORMAL_BROADCASTS;
            usb_rawhid_send(buf2, TIMEOUT);
            return;
        }
    }
}

// triggered when volume process is closed on the computer
void pidClosed(uint8_t buf[PACKET_SIZE]) {
    const RecPIDClosed recPIDClosed(buf);
    const uint32_t closedPID = recPIDClosed.getPID();
    uint8_t channelIndex = 0;
    for (uint8_t channel = FIRST_CHANNEL; channel < CHANNELS; channel++) {
        if (faderChannels[channel].appdata.PID == closedPID) {
            faderChannels[channel].setUnused();
            channelIndex = channel;
            break;
        }
    }

    buf[0] = STOP_NORMAL_BROADCASTS;
    usb_rawhid_send(buf, TIMEOUT);
    if (channelIndex != 0) {
        for (uint8_t i = 0; i < openProcessIndex; i++) {
            const uint32_t candidatePID = openProcessIDs[i];
            bool inUse = false;
            for (const auto &channel: faderChannels) {
                if (channel.appdata.PID == candidatePID) {
                    inUse = true;
                    break;
                }
            }
            if (!inUse) {
                faderChannels[channelIndex].appdata.PID = candidatePID;
                const uint8_t iconResult = requestIcon(candidatePID);
                faderChannels[channelIndex].setIcon(
                    (iconResult == THE_ICON_REQUESTED_IS_DEFAULT) ? defaultIcon : bufferIcon,
                    ICON_SIZE,
                    ICON_SIZE
                );
                buf[0] = REQUEST_CHANNEL_DATA;
                for (uint8_t byte = 0; byte < static_cast<uint8_t>(sizeof(uint32_t)); byte++) {
                    buf[byte + 1] = static_cast<uint8_t>(candidatePID >> byte * 8);
                }
                usb_rawhid_send(buf, TIMEOUT);
                break;
            }
        }
    }
    sendCurrentSelectedProcesses();
    buf[0] = START_NORMAL_BROADCASTS;
    usb_rawhid_send(buf, TIMEOUT);
    buf[0] = READY;
    usb_rawhid_send(buf, TIMEOUT);
}

//TODO: clean up
// triggered when a fader manually selected a new process
void updateProcess(const uint32_t i) {
    uint8_t buf2[PACKET_SIZE];
    buf2[0] = STOP_NORMAL_BROADCASTS;
    usb_rawhid_send(buf2, TIMEOUT);
    if (const uint8_t a = requestIcon(faderChannels[i].appdata.PID); a == THE_ICON_REQUESTED_IS_DEFAULT) {
        faderChannels[i].setIcon(defaultIcon, ICON_SIZE, ICON_SIZE);
    } else {
        faderChannels[i].setIcon(bufferIcon, ICON_SIZE, ICON_SIZE);
    }
    uint8_t buf[PACKET_SIZE];
    buf[0] = REQUEST_CHANNEL_DATA;
    buf[1] = faderChannels[i].appdata.PID;
    buf[2] = faderChannels[i].appdata.PID >> 8;
    buf[3] = faderChannels[i].appdata.PID >> 16;
    buf[4] = faderChannels[i].appdata.PID >> 24;
    usb_rawhid_send(buf, TIMEOUT);
    faderChannels[i].requestNewProcess = false;
    sendCurrentSelectedProcesses();
    buf2[0] = START_NORMAL_BROADCASTS;
    usb_rawhid_send(buf2, TIMEOUT);
}

//TODO: clean up
// sends the current max volume of a channel to the computer
void sendChangeOfMaxVolume(const uint8_t _channelNumber) {
    uint8_t buf[PACKET_SIZE];
    if (_channelNumber == 0) {
        buf[0] = CHANGE_OF_MASTER_MAX;
        buf[1] = faderChannels[0].getFaderPosition();
        buf[2] = faderChannels[0].isMuted;
    } else {
        buf[0] = CHANGE_OF_MAX_VOLUME;
        buf[1] = faderChannels[_channelNumber].appdata.PID;
        buf[2] = faderChannels[_channelNumber].appdata.PID >> 8;
        buf[3] = faderChannels[_channelNumber].appdata.PID >> 16;
        buf[4] = faderChannels[_channelNumber].appdata.PID >> 24;
        buf[5] = faderChannels[_channelNumber].getFaderPosition();
        buf[6] = faderChannels[_channelNumber].isMuted;
    }
    usb_rawhid_send(buf, TIMEOUT);
}

// triggered when the max volume of a channel is changed on the computer
void changeOfMaxVolume(const uint8_t buf[PACKET_SIZE]) {
    const RecMaxChange recMaxChange(buf);
    const uint32_t pid = recMaxChange.getPID();
    for (int i = FIRST_CHANNEL; i < CHANNELS; i++) {
        if (faderChannels[i].appdata.PID == pid) {
            faderChannels[i].setMaxVolume(recMaxChange.getMaxVolume());
            faderChannels[i].setMute(recMaxChange.isMuted());
            break;
        }
    }
}

// triggered when the max volume of the master channel is changed on the computer
void changeOfMasterMax(const uint8_t buf[PACKET_SIZE]) {
    const RecMasterMaxChange recMasterMaxChange(buf);
    faderChannels[MASTER_CHANNEL].setMaxVolume(recMasterMaxChange.getMasterMaxVolume());
    faderChannels[MASTER_CHANNEL].setMute(recMasterMaxChange.isMasterMuted());
}

// computer sends volume levels of all channels
void receiveCurrentVolumeLevels(const uint8_t buf[PACKET_SIZE]) {
    const RecCurrentVolumeLevels recCurrentVolumeLevels(buf);
    for (uint8_t channel = FIRST_CHANNEL; channel < CHANNELS; channel++) {
        if (const uint8_t packetIndex = channel - FIRST_CHANNEL;
            faderChannels[channel].appdata.PID == recCurrentVolumeLevels.getPID(packetIndex)) {
            faderChannels[channel].setCurrentVolume(recCurrentVolumeLevels.getVolume(packetIndex));
        }
    }
}

// tells the computer we want to receive all current processes
void processRequestsInit(uint8_t buf[PACKET_SIZE]) {
    const RecProcessRequestInit recProcessRequestInit(buf);
    numChannels = recProcessRequestInit.getNumChannels();
    numPackets = recProcessRequestInit.getNumPackets();
    openProcessIndex = 0;
    buf[0] = ACK;
    usb_rawhid_send(buf, TIMEOUT);
}

// triggered when the computer sends all current processes
void allCurrentProcesses(uint8_t buf[PACKET_SIZE]) {
    const RecAllCurrentProcesses recAllCurrentProcesses(buf);
    openProcessIDs[openProcessIndex] = recAllCurrentProcesses.getPID();;
    recAllCurrentProcesses.getName(openProcessNames[openProcessIndex]);
    openProcessIndex++;
    if (openProcessIndex != numChannels) {
        openProcessIDs[openProcessIndex] = recAllCurrentProcesses.getPID();
        recAllCurrentProcesses.getName(openProcessNames[openProcessIndex]);
        openProcessIndex++;
    }

    if (numChannels == openProcessIndex) {
        if (faderRequest != -1) {
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
    usb_rawhid_send(buf, TIMEOUT);
}

// computer sends info about the icon it is about to send
void iconPacketsInit(uint8_t buf[PACKET_SIZE]) {
    const RecIconPacketInit recIconPacketInit(buf);
    receivingIcon = true;
    iconPID = recIconPacketInit.getPID();
    iconPages = recIconPacketInit.getPacketCount();
    // send ACK in buf[0] to indicate that we are ready for the first page
    buf[0] = ACK;
    usb_rawhid_send(buf, TIMEOUT);
}

// computer sends info about a process
void channelData(const uint8_t buf[PACKET_SIZE]) {
    const RecChannelData recChannelData(buf);
    const uint8_t maxVolume = recChannelData.getMaxVolume();
    const bool isMuted = recChannelData.isMuted();
    const uint32_t pid = recChannelData.getPID();
    char name[NAME_LENGTH_MAX];
    recChannelData.getName(name);
    if (recChannelData.isMaster() == true) {
        faderChannels[MASTER_CHANNEL].setMute(isMuted);
        faderChannels[MASTER_CHANNEL].setMaxVolume(maxVolume);
        faderChannels[MASTER_CHANNEL].setName(name);
    } else {
        for (uint8_t channel = FIRST_CHANNEL; channel < CHANNELS; channel++) {
            if (faderChannels[channel].appdata.PID == pid) {
                faderChannels[channel].setMute(isMuted);
                faderChannels[channel].setMaxVolume(maxVolume);
                faderChannels[channel].setName(name);
            }
        }
    }
}

[[noreturn]] void uncaughtException(const String &message) {
    digitalWrite(CS_LOCK, LOW);
    tft.fillScreen(ST77XX_RED);
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(2);
    tft.setCursor(0, 0);
    tft.println("Uncaught Error.");
    tft.println(message);
    tft.println("Please restart.");
    tft.updateScreen();
    while (true);
}
