# Motorized Volume Controller

An 8 channel (1 main, 7 programmable) motorized volume controller for a pc. Each of the 7 channels can be set to any process that outputs volume.

I've always had a problem where one piece of software is louder that I'd like (especially while gaming), and having to continually open the volume mixer just got annoying. I started looking into physical options that could help me and I found some really cool projects, just none of them really suited all my requirements. Projects like [deej](https://github.com/omriharel/deej) and [MIDI Mixer](https://www.midi-mixer.com/) are great and relatively easy to set up, but I wanted more. My 3 requirements were:

1. Motorized Faders
2. A screen to see what is being controlled
3. Setting the controlled software on the controller

To meet these requirements I came to realize I needed to build it myself. 10 months later, here we are.

## Features

- Each channel has a display to see what it is controlling
- Motorized Faders stay in sync with any volume changes on the pc side
- Live volume output next to each fader showing current levels
- User can dynamically switch controlled processes on the controller itself
- Mute Buttons for quickly muting

## PC-Side Software
Not released yet

## PCBs
|||
|:-------------:|:-------------:|
|Main PCB|[OSHWLab link](https://oshwlab.com/somdahlfinnley/mainboard)|
|||
|Power PCB|[OSHWLab link](https://oshwlab.com/somdahlfinnley/powerprovider)|
|||
|Fader Adapter|[OSHWLab link](https://oshwlab.com/somdahlfinnley/simplefaderboard)|
|||

## 3D Print Files
A .step file is included in the 3D Files directory. There are 2 separate objects that need to be printed.

## Uploading Code
For this project, I used [PlatformIO](https://platformio.org/) with a Teensy 4.1.