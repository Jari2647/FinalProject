/**
 *  ==================================================== Dec 21 2013, kayeks ==
 *  VS1053.cpp
 *  ===========================================================================
 *  Just a simple library for VLSI's mp3/midi codec chip
 *       - Minimal and simple implementation (and dirty too)
 *
 *  Modified on 05 September 2015 by Vassilis Serasidis.
 *       -   Added a patch for playing MP3 files on some "LC Technology" VS1053 boards.
 *
 *
 */
 
#ifndef KAYX_VS1053_H_
#define KAYX_VS1053_H_

/** Class VS1053. Drives VLSI's mp3/midi codec chip. */
class VS1053 {
private:
    SPI        spi;
    DigitalOut cs;
    DigitalOut bsync; //dcs pin
    DigitalIn  dreq;
    DigitalOut rst;

public:
    static const uint8_t SCI_MODE        = 0x00;
    static const uint8_t SCI_STATUS      = 0x01;
    static const uint8_t SCI_BASS        = 0x02;
    static const uint8_t SCI_CLOCKF      = 0x03;
    static const uint8_t SCI_DECODE_TIME = 0x04;
    static const uint8_t SCI_AUDATA      = 0x05;
    static const uint8_t SCI_WRAM        = 0x06;
    static const uint8_t SCI_WRAMADDR    = 0x07;
    static const uint8_t SCI_HDAT0       = 0x08;
    static const uint8_t SCI_HDAT1       = 0x09;
    static const uint8_t SCI_AIADDR      = 0x0a;
    static const uint8_t SCI_VOL         = 0x0b;
    static const uint8_t SCI_AICTRL0     = 0x0c;
    static const uint8_t SCI_AICTRL1     = 0x0d;
    static const uint8_t SCI_AICTRL2     = 0x0e;
    static const uint8_t SCI_AICTRL3     = 0x0f;
    
    static const uint8_t SM_RESET        = 2;
    static const uint8_t SM_SDINEW       = 11;
    
    VS1053(PinName mosiPin, PinName misoPin, PinName sckPin,
           PinName csPin, PinName bsyncPin, PinName dreqPin, PinName rstPin,
           uint32_t spiFrequency=1000000);
    ~VS1053();
    void hardwareReset();
    void modeSwitch(void);
    void sendDataByte(uint8_t data);
    size_t sendDataBlock(char* data, size_t length);
    void clockUp();
    bool sendCancel();
    bool stop();
    void setVolume(uint8_t vol);
    void setSPIFrequency(int hz);
    
private:
    void writeReg(uint8_t, uint16_t);
    uint16_t readReg(uint8_t);
};

#endif
