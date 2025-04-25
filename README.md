# ECE4180 Final Project
Walkman-style mp3 player created with Mbed LPC1768 microcontroller. Written in C++.
Version: MBED OS6

This portable device plays mp3 audio files off of a 32GB MicroSD card. Connect your listening device of choice through the integrated 3.5mm audio jack
and take your music on the go. Features include uLCD display UI, volume control, on-device menu navigation, and volume control. 

The MicroSD card is accessed through the SDBlockDevice API in Mbed OS6.

The device is built upon a 3D printed breadboard box by Thingiverse user lorddeath.
https://www.thingiverse.com/thing:4824137/files
![large_display_Box_2](https://github.com/user-attachments/assets/d55376af-1e8e-428b-b894-258cfe231380)

The uLCD display operates off of a library written by Stephane Rochon and modified by Jim Hamblen on the Mbed forums. https://os.mbed.com/users/4180_1/code/uLCD144G2_demo/diff/31e63caf37e2/main.cpp/

The VS1053 operates off the following library from Vassilis Serasidis. https://os.mbed.com/users/silis/code/VS1053/

![IMG_0257](https://github.com/user-attachments/assets/d50fe24e-eb86-4c68-906d-330137ddd846)
![IMG_0258](https://github.com/user-attachments/assets/28f80b61-4a9d-45fb-ad6e-b66a6a9f1e81)

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

    4D Systems uLCD (serial interface)

    MicroSD card formatted with FAT32 filesystem

    Sparkfun MicroSD card Breakout

    Sparkfun MP3 and MIDI Breakout VS1053 (BOB-09943)
        MP3 audio decoder module that communicates over SPI.

    Sparkfun TRRS breakout
        3.5mm audio jack

    5 way tactile Nav Switch
        UI control: Song selection in menu and playback.

    1 pushbutton
      UI control: Returns to song selection menu.

    10 kΩ potentiometer
        Volume control

    5V-6V power supply
        This project uses 4 AA batteries in series with a barrel jack adapter.
        Shared between LPC1768, VS1053, uLCD, and MicroSD card breakout.


![FinalProject](https://github.com/user-attachments/assets/0c8aa452-e543-4b4a-a88e-e977982b91fb)



Issues

    Speaker: Our initial design approach would allow the user to switch between 3.5mm audio and an on-board speaker.
    However, the VS1053 was unable to drive the speaker without an amplifier. Wiring the amplifier module increased the
    complexity of the project more than expected, so we abandoned it in favor of more UI features.
    
    Displaying Volume: Showing real-time volume updates requires the potentiometer to be polled. When implemented alongside the
    progress bar, audio playback became stuttery, despite the polling rate being lowered. We decided to keep the progress bar over
    volume display.

    uLCD library choice. The uLCD_4DGL was the only library that would not fail upon initialization across the 3 uLCD boards we used
    throughout the project.

Improvements

    Further customizations to the breadboard box. Additional enclosures could be 3D printed for the small pcb that holds the UI peripherals and the
    AA battery adapter.

    A modernized VS1053 breakout board.
        Sparkfun has discontinued the VS1053 board we used in this project. So much so that no documentation/datasheets exist for it
        online anymore :(
        The board that replaced it is the MP3 Player Shield. https://www.sparkfun.com/sparkfun-mp3-player-shield.html
            It combines the VS1053, TRRS board, and MicroSD card reader into one peripheral. 
             - It even has solder points for speakers, and a reset button :O
        We decided against ordering it because we already had every component we needed on hand.
        However, this peripheral would have made our Walkman even more compact (and a lot easier).

    Some quality of life changes:
        A bigger lcd screen, better wiring, higher quality pushbuttons, etc.

Similar project: The MP3 Player from Mbed's cookbook. https://os.mbed.com/cookbook/MP3-Player

    Inspiration was taken from this project directly from Mbed's forums. This served as a template that ensure to us that
    we could build this project entirely with the parts we had in the lab and in our kits.

    We decided to improve on this project in 3 key ways:
        - Compactness (2 small breadboards vs 1 very large one)
        - Better components (ulcd, nav switch, potentiometer)
        - Portability (make it battery powered and partially enclosed)

    

    

  
