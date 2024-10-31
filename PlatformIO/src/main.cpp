#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "thirdparty/fastlz.h"
#include "icons.h"
#include "packets/RecNewPID.h"
#include "packets/RecChannelData.h"
#include "packets/RecIconPacketInit.h"
#include "packets/RecAllCurrentProcesses.h"
#include "packets/RecProcessRequestInit.h"
#include "packets/RecCurrentVolumeLevels.h"
#include "packets/RecPIDClosed.h"
#include "packets/PacketSender.h"
#include "packets/RecIconPacket.h"
#include "ByteArrayQueue.h"
#include "FaderChannel.h"

// Functions
/**************************************************/
void init();

[[noreturn]] void uncaughtException(const String &message = "");

void iconPacketsInit(const uint8_t buf[PACKET_SIZE]);

void allCurrentProcesses(const uint8_t buf[PACKET_SIZE]);

void processRequestsInit(uint8_t buf[PACKET_SIZE]);

void iconPacket(const uint8_t buf[PACKET_SIZE]);

void update(uint8_t *buf);

void requestIcon(uint32_t pid);

void iconIsDefault(const uint8_t buf[PACKET_SIZE]) ;

void requestAllProcesses();

void receiveCurrentVolumeLevels(const uint8_t buf[PACKET_SIZE]);

void sendCurrentSelectedProcesses();

void channelData(const uint8_t buf[PACKET_SIZE]);

void sendChangeOfMaxVolume(uint8_t _channelNumber);

void pidClosed(uint8_t buf[PACKET_SIZE]);

void newPID(const uint8_t buf[PACKET_SIZE]);

void updateProcess(uint32_t channel);


// Transitory Variables for passing data around
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
    while (!usb_configuration) {
        delay(100);
    }
    //while (usb_rawhid_available()) {
    //    uint8_t buf[PACKET_SIZE];
    //    usb_rawhid_recv(buf, 0);
    //}
    Serial.println("USB configured");
    digitalWrite(CS_LOCK, HIGH); // unlock the screens
    LEDs.clear();
    LEDs.show();
    capSensor.set_CS_AutocaL_Millis(0xFFFFFFFF); // set up the capacitive sensor
    capSensor.set_CS_Timeout_Millis(100);
    capSensor.reset_CS_AutoCal();
    capSensor.capacitiveSensor(30);
    init();
}


uint8_t buf[PACKET_SIZE];

void loop() {
    reButtonMux.update(); // update the encoders and buttons
    leButtonMux.update();
    rotaryMux.update();
    for (int i = 0; i < CHANNELS; i++) // update the fade channels
    {
    const auto start = millis();
        encoder[i].update(rotaryMux.digitalRead(i * 2), rotaryMux.digitalRead(i * 2 + 1), 2, LOW);
        enButton[i].update(reButtonMux.digitalRead(i), 50, LOW);
        leButton[2 * i].update(leButtonMux.digitalRead(i * 2), 50, LOW);
        leButton[2 * i + 1].update(leButtonMux.digitalRead(i * 2 + 1), 50, LOW);
        if (faderChannels[i].userChanged) {
            faderChannels[i].userChanged = false;
            sendChangeOfMaxVolume(i);
        }

        if (faderChannels[i].requestProcessRefresh) {
            packetSender.sendRequestAllProcesses();
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
        const auto end = millis();
        //Serial.println("Update time for channel " + String(i) + ": " + String(end - start));
    }
    if (!states.isReceivingChannels() && !states.isReceivingIcon() && !sendingQueue.isEmpty()) {
        uint8_t buf[PACKET_SIZE];
        if (sendingQueue.pop(buf)) {
            update(buf);
        }
    }
    if (const uint8_t res = RawHID.recv(buf, 0); res > 0) {
        Serial.println("Packet received: " + String(buf[PacketPositions::Base::STATUS_INDEX]));
        update(buf);
        LEDs.show();
    }
}

// set fader pot and touch values then get initial data from computer on startup
void init() {
    initializing = true;
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
    Serial.println("setting icon to default");
    faderChannels[MASTER_CHANNEL].setIcon(defaultIcon, ICON_SIZE, ICON_SIZE);
    for (uint8_t channel = FIRST_CHANNEL; channel < CHANNELS; channel++) {
        faderChannels[channel].setUnused(true);
    }
    Serial.println("finished init");
    requestAllProcesses();
}

// send the processes of the 7 fader channels to the computer
void sendCurrentSelectedProcesses() {
    uint32_t PIDs[CHANNELS - 1];
    uint8_t count = 0;
    for (int i = FIRST_CHANNEL; i < CHANNELS; i++) {
        if (!faderChannels[i].isUnused()) {
            PIDs[count] = faderChannels[i].appdata.PID;
            count++;
        }
    }
    packetSender.sendCurrentSelectedProcesses(PIDs, count);
}

// requests all sound processes from the computer
void requestAllProcesses() {
    packetSender.sendRequestAllProcesses();
}

// request the icon of a process from the computer
void requestIcon(const uint32_t pid) {
    packetSender.sendRequestIcon(pid);
}

// main update function
void update(uint8_t *buf) {
    switch (buf[PacketPositions::Base::STATUS_INDEX]) {
        // case UNDEFINED:
        // break;
        case ACK:
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
        //case REQUEST_CHANNEL_DATA:
        // should never receive this
        //    break;
        case CHANNEL_DATA:
            channelData(buf);
            break;
        case PID_CLOSED:
            pidClosed(buf);
            break;
        case SEND_CURRENT_VOLUME_LEVELS:
            receiveCurrentVolumeLevels(buf);
            break;
        // case CURRENT_SELECTED_PROCESSES:
        // break;
        case NEW_PID:
            newPID(buf);
            break;
        // case REQUEST_ICON:
        //  should never receive this
        // break;
        case ICON_PACKETS_INIT:
            iconPacketsInit(buf);
            break;
        case ICON_PACKET:
            iconPacket(buf);
            break;
        case THE_ICON_REQUESTED_IS_DEFAULT:
            iconIsDefault(buf);
            break;
        case BUTTON_PUSHED:
            break;
        default:
            Serial.println("Warning: Unknown packet received: " + String(buf[0]));
    }
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
        if (channel.appdata.PID == pid) {
            channel.setName(name);
            channel.targetVolume = volume;
            channel.setMute(mute);
            return;
        }
    }
    for (auto &channel: faderChannels) {
        if (channel.isUnused()) {
            channel.setUnused(false);
            channel.appdata.PID = pid;
            channel.setName(name);
            channel.targetVolume = volume;
            channel.setMute(mute);
            requestIcon(pid);
            sendCurrentSelectedProcesses();
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

    for (size_t i = 0; i < openProcessIDs.getSize(); i++) {
        if (openProcessIDs[i] == closedPID) {
            openProcessIDs.remove_at(i);
            openProcessNames.remove_at(i);
            break;
        }
    }

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
                requestIcon(candidatePID);
                packetSender.sendRequestChannelData(candidatePID);
                break;
            }
        }
    }
    sendCurrentSelectedProcesses();
}

// triggered when a fader manually selected a new process
void updateProcess(const uint32_t channel) {
    requestIcon(faderChannels[channel].appdata.PID);
    packetSender.sendRequestChannelData(faderChannels[channel].appdata.PID);
    faderChannels[channel].requestNewProcess = false;
    sendCurrentSelectedProcesses();
}

// sends the current max volume of a channel to the computer
void sendChangeOfMaxVolume(const uint8_t _channelNumber) {
    packetSender.sendChannelData(
        faderChannels[_channelNumber].appdata.isMaster,
        faderChannels[_channelNumber].getFaderPosition(),
        faderChannels[_channelNumber].isMuted,
        faderChannels[_channelNumber].appdata.PID,
        faderChannels[_channelNumber].appdata.name
    );
}

// computer sends volume levels of all channels
void receiveCurrentVolumeLevels(const uint8_t buf[PACKET_SIZE]) {
    const RecCurrentVolumeLevels recCurrentVolumeLevels(buf);
    const uint8_t numChannels = recCurrentVolumeLevels.getChannelCount();
    for (uint8_t pos = 0; pos < numChannels; pos++) {
        for (uint8_t channel = FIRST_CHANNEL; channel < CHANNELS; channel++) {
            if (faderChannels[channel].appdata.PID == recCurrentVolumeLevels.getPID(pos)) {
                faderChannels[channel].setCurrentVolume(recCurrentVolumeLevels.getVolume(pos));
            }
        }
    }
}

// triggered when the computer sends a process request init
void processRequestsInit(uint8_t buf[PACKET_SIZE]) {
    if (states.isReceivingChannels()) {
        Serial.println("Warning: Received process request init while already receiving channels");
        if (!sendingQueue.push(buf)) {
            uncaughtException("Failed to push process request init to queue");
        }
    }
    const RecProcessRequestInit recProcessRequestInit(buf);
    states.setReceivingChannels(true);
    numSentChannels = recProcessRequestInit.getNumChannels();
    openProcessIDs.clear();
    openProcessNames.clear();
    packetSender.sendAcknowledge(buf[PacketPositions::Base::COUNT_INDEX], CHANNEL_ACK);
}

// triggered when the computer sends all current processes
void allCurrentProcesses(const uint8_t buf[PACKET_SIZE]) {
    const RecAllCurrentProcesses recAllCurrentProcesses(buf);
    uint8_t i = 0; //only run while loop at most twice
    while (openProcessIDs.getSize() < numSentChannels && i < 2) {
        char name[NAME_LENGTH_MAX];
        openProcessIDs.push_back(recAllCurrentProcesses.getPID(i));
        recAllCurrentProcesses.getName(name, i);
        Serial.println("Received process: " + String(openProcessIDs[openProcessIDs.getSize() - 1]) + " " + name);
        openProcessNames.push_back<NAME_LENGTH_MAX>(name);
        i++;
    }

    if (numSentChannels == openProcessIDs.getSize()) {
        Serial.println("Received all current processes");
        if (faderRequest != -1 && faderRequest < CHANNELS) {
            faderChannels[faderRequest].menuOpen = true;
            faderChannels[faderRequest].updateScreen = true;
            faderRequest = -1;
        }
        states.setReceivingChannels(false);
        if (initializing) {
            uint32_t PIDs[CHANNELS - 1];
            uint8_t count = 0;
            for (uint8_t channel = FIRST_CHANNEL; channel < CHANNELS; channel++) {
                if (channel - 1 < openProcessIDs.getSize()) {
                    faderChannels[channel].setUnused(false);
                    faderChannels[channel].appdata.PID = openProcessIDs[channel - 1];
                    faderChannels[channel].setName(openProcessNames[channel - 1]);
                    packetSender.sendRequestChannelData(openProcessIDs[channel - 1]);
                    requestIcon(openProcessIDs[channel - 1]);
                    PIDs[count] = openProcessIDs[channel - 1];
                    count++;
                } else {
                    faderChannels[channel].setUnused(true);
                }
            }
            packetSender.sendCurrentSelectedProcesses(PIDs, count);
            packetSender.sendStartNormalBroadcasts();
            initializing = false;
        }
    }
}

// computer sends info about the icon it is about to send
void iconPacketsInit(const uint8_t buf[PACKET_SIZE]) {
    if (states.isReceivingIcon()) {
        Serial.println("Warning: Received icon packet init while already receiving icon");
        if (!sendingQueue.push(buf)) {
            uncaughtException("Failed to push icon packet init to queue");
        }
    }
    const RecIconPacketInit recIconPacketInit(buf);
    states.setReceivingIcon(true);
    sentIconPID = recIconPacketInit.getPID();
    totalIconPackets = recIconPacketInit.getPacketCount();
    compressionSize = recIconPacketInit.getByteCount();
    // send ACK in to indicate that we are ready for the first page
    packetSender.sendAcknowledge(buf[PacketPositions::Base::COUNT_INDEX], ICON_ACK);
}

// computer sends a page of the icon
void iconPacket(const uint8_t buf[PACKET_SIZE]) {
    const RecIconPacket recIconPacket(buf);
    currentIconPacket++;
    recIconPacket.emplaceIconData();
    if (currentIconPacket == totalIconPackets) {
        uint32_t decompressedSize = fastlz_decompress(compressionBuffer, (int) compressionSize, bufferIcon,
                                                      ICON_SIZE * ICON_SIZE * sizeof(uint16_t));
        if (decompressedSize != ICON_SIZE * ICON_SIZE * sizeof(uint16_t)) {
            Serial.println("Error: Decompression failed");
            uncaughtException("Decompression failed");
        } else {
            for (uint8_t channel = FIRST_CHANNEL; channel < CHANNELS; channel++) {
                if (faderChannels[channel].appdata.PID == sentIconPID) {
                    faderChannels[channel].setIcon(bufferIcon, ICON_SIZE, ICON_SIZE);
                }
            }
        }
        states.setReceivingIcon(false);
    }
}

// default icon
void iconIsDefault(const uint8_t buf[PACKET_SIZE]) {
    uint32_t iconPID;
    memcpy(&iconPID, buf + PacketPositions::IconIsDefault::PID_INDEX, sizeof(uint32_t));
    for (uint8_t channel = FIRST_CHANNEL; channel < CHANNELS; channel++) {
        if (faderChannels[channel].appdata.PID == iconPID) {
            Serial.println("Setting icon to default for PID: " + String(iconPID));
            faderChannels[channel].setIcon(defaultIcon, ICON_SIZE, ICON_SIZE);
        }
    }
    states.setReceivingIcon(false);
}

// computer sends info about a process
void channelData(const uint8_t buf[PACKET_SIZE]) {
    const RecChannelData recChannelData(buf);
    const uint8_t maxVolume = recChannelData.getMaxVolume();
    const bool isMuted = recChannelData.isMuted();
    const uint32_t pid = recChannelData.getPID();
    Serial.println("Received channel data for PID: " + String(pid));
    Serial.println("Max volume: " + String(maxVolume));
    Serial.println("Muted: " + String(isMuted));
    char name[NAME_LENGTH_MAX];
    recChannelData.getName(name);
    if (recChannelData.isMaster() == true) {
        faderChannels[MASTER_CHANNEL].setMute(isMuted);
        faderChannels[MASTER_CHANNEL].setMaxVolume(maxVolume);
        faderChannels[MASTER_CHANNEL].setName(name);
    } else {
        for (uint8_t channel = FIRST_CHANNEL; channel < CHANNELS; channel++) {
            if (faderChannels[channel].appdata.PID == pid) {
                Serial.println("Setting channel data for channel: " + String(channel));
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
