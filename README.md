# ECE4180 Final Project
Walkman-style mp3 player created with Mbed LPC1768 microcontroller. Written in C++.
Version: MBED OS6

This portable device plays mp3 audio files off a 32GB micro-sd card. Connect your listening device of choice through the integrated 3.5mm audio jack
and take your music on the go. Features include uLCD display UI, volume control, on-device menu navigation, and volume control.

Usage

    Menu Navigation

        Up/Down: Scroll through the list of songs.

        Center: Select highlighted song → begins playback.

    Playback Controls

        Center (during playback): Pause / Resume.

        Left/Right: Skip to previous / next track.

        Menu (dedicated): Back to song selection menu.

        Volume Potentiometer: Turn to adjust volume.

    Display

        Song title is shown center‐screen.

        Progress bar fills as the song plays.

        Top line: “Track X/Y” and “Playing/Paused” status.


Hardware Requirements

    Mbed LPC1768 microcontroller

    Sparkfun MP3 and MIDI Breakout VS1053 (BOB-09943)
        MP3 audio decoder module that communicates over SPI

    Sparkfun MicroSD card Breakout

    MicroSD card formatted with FAT32 filesystem

    Sparkfun TRRS breakout
        3.5mm audio jack

    4D Systems uLCD (serial interface)

    5 way tactile Nav Switch
        UI control: song selection in menu and playback

    1 pushbutton
        UI control: returns to song selection menu

    10 kΩ potentiometer
        Volume control

    5V-6V power supply
        This project uses 4 AA batteries in series with a barrel jack adapter
        Shared between LPC1768, VS1053, uLCD, and MicroSD card breakout

Issues

    Wire choice: Standard male-to male jumper wires cause connectivity and stability issues when debugging. With power frequently cutting off
    to the VS1053 and Mbed. Thus, the circuit had to be connected with a pre-bent wire kit to reduce failure in testing.
    
    Displaying Volume: Showing real-time volume updates requires the potentiometer to be polled. When implemented alongside the
    progress bar, audio playback became stuttery, despite the polling rate being lowered. We decided to keep the progress bar over
    volume display.

    uLCD library choice. The uLCD_4DGL was the only library that would not fail upon initialization across the 3 uLCD boards we used
    throughout the project.

    

  
