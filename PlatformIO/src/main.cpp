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
#include "packets/PacketSender.h"
#include "FaderChannel.h"

// Functions
/**************************************************/
void init();

[[noreturn]] void uncaughtException(const String &message = "");

void iconPacketsInit(const uint8_t buf[PACKET_SIZE]);

void allCurrentProcesses(const uint8_t buf[PACKET_SIZE]);

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

void updateProcess(uint32_t channel);


// Transitory Variables for passing data around
uint32_t iconPages = 0;
uint32_t currentIconPage = 0;
uint32_t iconPID = 0;
int numPackets = 0;
size_t numChannels = 0;
PacketSender packetSender;
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
    requestAllProcesses();
    // fill the 7 fader channels with the first 7 processes
    faderChannels[MASTER_CHANNEL].setIcon(defaultIcon, ICON_SIZE, ICON_SIZE);
    for (uint8_t channel = FIRST_CHANNEL; channel < CHANNELS; channel++) {
        if (openProcessIDs.getSize() >= channel) {
            faderChannels[channel].appdata.PID = openProcessIDs[channel - 1];
            faderChannels[channel].setName(openProcessNames[channel - 1]);
            if (const uint8_t a = requestIcon(faderChannels[channel].appdata.PID); a != THE_ICON_REQUESTED_IS_DEFAULT) {
                faderChannels[channel].setIcon(bufferIcon, ICON_SIZE, ICON_SIZE);
            } else {
                faderChannels[channel].setIcon(defaultIcon, ICON_SIZE, ICON_SIZE);
            }
        } else {
            faderChannels[channel].setUnused(true);
        }
    }
    getMaxVolumes();
    for (auto &channel: faderChannels) {
        channel.update();
    }
    sendCurrentSelectedProcesses();\
    normalBroadcast = true;
}

// send the processes of the 7 fader channels to the computer
void sendCurrentSelectedProcesses() {
    uint32_t PIDs[CHANNELS - 1];
    for (int i = FIRST_CHANNEL; i < CHANNELS; i++) {
        PIDs[i - 1] = faderChannels[i].appdata.PID;
    }
    packetSender.sendCurrentSelectedProcesses(PIDs, CHANNELS - 1);
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
    int iterator = FIRST_CHANNEL;
    packetSender.sendRequestChannelData(MASTER_REQUEST);
    while (true) {
        if (usb_rawhid_available() >= PACKET_SIZE) {
            uint8_t buf[PACKET_SIZE];
            usb_rawhid_recv(buf, TIMEOUT);
            update(buf); //TODO: I'm not a fan of a blocking while loop to update
            if (iterator > CHANNELS - 1) {
                break;
            }
            packetSender.sendRequestChannelData(faderChannels[iterator].appdata.PID);
            iterator++;
        }
    }
}

// request the icon of a process from the computer UNSAFE, the computer cannot send any other data while this is happening
uint8_t requestIcon(const uint32_t pid) {
    iconPID = 0;
    iconPages = 0;
    currentIconPage = 0;
    packetSender.sendRequestIcon(pid);
    uint8_t buf[PACKET_SIZE];
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
                packetSender.sendAcknowledge();
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
        case ICON_PACKET:
            break; //TODO:
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
        if (channel.isUnused()) {
            channel.setUnused(false);
            channel.appdata.PID = pid;
            channel.setName(name);
            channel.targetVolume = volume;
            channel.setMute(mute);
            packetSender.sendStopNormalBroadcasts();
            if (const uint8_t a = requestIcon(pid); a == THE_ICON_REQUESTED_IS_DEFAULT) {
                channel.setIcon(defaultIcon, ICON_SIZE, ICON_SIZE);
            } else {
                channel.setIcon(bufferIcon, ICON_SIZE, ICON_SIZE);
            }
            sendCurrentSelectedProcesses();
            packetSender.sendStartNormalBroadcasts();
            return;
        }
    }
}

// triggered when volume process is closed on the computer
void pidClosed(uint8_t buf[PACKET_SIZE]) {
    const RecPIDClosed recPIDClosed(buf);
    const uint32_t closedPID = recPIDClosed.getPID();
    uint8_t channelIndex = FIRST_CHANNEL - 1;
    for (uint8_t channel = FIRST_CHANNEL; channel < CHANNELS; channel++) {
        if (faderChannels[channel].appdata.PID == closedPID) {
            faderChannels[channel].setUnused(true);
            channelIndex = channel;
            break;
        }
    }

    packetSender.sendStopNormalBroadcasts();
    if (channelIndex != FIRST_CHANNEL - 1) {
        for (size_t i = 0; i < openProcessIDs.getSize(); i++) {
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
                    iconResult == THE_ICON_REQUESTED_IS_DEFAULT ? defaultIcon : bufferIcon,
                    ICON_SIZE,
                    ICON_SIZE
                );
                packetSender.sendRequestChannelData(candidatePID);
                break;
            }
        }
    }
    sendCurrentSelectedProcesses();
    packetSender.sendStartNormalBroadcasts();
    packetSender.sendReady();
}

// triggered when a fader manually selected a new process
void updateProcess(const uint32_t channel) {
    packetSender.sendStopNormalBroadcasts();
    if (const uint8_t res = requestIcon(faderChannels[channel].appdata.PID); res == THE_ICON_REQUESTED_IS_DEFAULT) {
        faderChannels[channel].setIcon(defaultIcon, ICON_SIZE, ICON_SIZE);
    } else {
        faderChannels[channel].setIcon(bufferIcon, ICON_SIZE, ICON_SIZE);
    }
    packetSender.sendRequestChannelData(faderChannels[channel].appdata.PID);
    faderChannels[channel].requestNewProcess = false;
    sendCurrentSelectedProcesses();
    packetSender.sendStartNormalBroadcasts();
}

// sends the current max volume of a channel to the computer
void sendChangeOfMaxVolume(const uint8_t _channelNumber) {
    if (_channelNumber == MASTER_CHANNEL) {
        packetSender.sendChangeOfMasterChannel(
            faderChannels[MASTER_CHANNEL].getFaderPosition(),
            faderChannels[MASTER_CHANNEL].isMuted
        );
    } else {
        packetSender.sendChangeOfChannel(
            faderChannels[_channelNumber].appdata.PID,
            faderChannels[_channelNumber].getFaderPosition(),
            faderChannels[_channelNumber].isMuted
        );
    }
}

// triggered when the max volume of a channel is changed on the computer
void changeOfMaxVolume(const uint8_t buf[PACKET_SIZE]) {
    const RecMaxChange recMaxChange(buf);
    const uint32_t pid = recMaxChange.getPID();
    for (int channel = FIRST_CHANNEL; channel < CHANNELS; channel++) {
        if (faderChannels[channel].appdata.PID == pid) {
            faderChannels[channel].setMaxVolume(recMaxChange.getMaxVolume());
            faderChannels[channel].setMute(recMaxChange.isMuted());
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
    openProcessIDs.clear();
    openProcessNames.clear();
    packetSender.sendAcknowledge();
}

// triggered when the computer sends all current processes
void allCurrentProcesses(const uint8_t buf[PACKET_SIZE]) {
    const RecAllCurrentProcesses recAllCurrentProcesses(buf);
    uint8_t i = 0; //only run while loop at most twice
    while (openProcessIDs.getSize() < numChannels && i < 2) {
        char name[NAME_LENGTH_MAX];
        openProcessIDs.push_back(recAllCurrentProcesses.getPID());
        recAllCurrentProcesses.getName(name);
        openProcessNames.push_back<NAME_LENGTH_MAX>(name);
        i++;
    }

    if (numChannels == openProcessIDs.getSize()) {
        if (faderRequest != -1) {
            faderChannels[faderRequest].menuOpen = true;
            faderChannels[faderRequest].updateScreen = true;
            faderRequest = -1;
        }
    }

    // send received
    packetSender.sendAcknowledge();
}

// computer sends info about the icon it is about to send
void iconPacketsInit(const uint8_t buf[PACKET_SIZE]) {
    const RecIconPacketInit recIconPacketInit(buf);
    receivingIcon = true;
    iconPID = recIconPacketInit.getPID();
    iconPages = recIconPacketInit.getPacketCount();
    // send ACK in buf[0] to indicate that we are ready for the first page
    packetSender.sendAcknowledge();
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
