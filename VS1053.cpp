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

#include "mbed.h"
#include "VS1053.h"

/** Constructor of class VS1053. */
VS1053::VS1053(PinName mosiPin, PinName misoPin, PinName sckPin,
               PinName csPin, PinName bsyncPin, PinName dreqPin,
               PinName rstPin, uint32_t spiFrequency)
:
    spi(mosiPin, misoPin, sckPin),
    cs(csPin),
    bsync(bsyncPin), //dcs pin
    dreq(dreqPin),
    rst(rstPin)
{
    //spi.format(8, 0);
    //spi.frequency(spiFrequency);

    // Initialize outputs
    cs = 1;
    bsync = 1;
    rst = 1;
}

/** Destructor of class VS1053. */
VS1053::~VS1053() {
}

/** Make a hardware reset by hitting VS1053's RESET pin. */
void VS1053::hardwareReset() {
    rst = 0;
    wait_us(50000);
    rst = 1;
    wait_us(50000);
}

/** Patch for some LC Technology VS1053 board with "no sound" problem. 
 *  5 September 2015 bby Vassilis Serasidis 
 */
void VS1053::modeSwitch(void)
{
    //GPIO_DDR
    writeReg(SCI_WRAMADDR, 0xc017);
    writeReg(SCI_WRAM, 0x0003);
  
    wait_us(50000);
    writeReg(SCI_MODE, (1<<SM_SDINEW) | (1<<SM_RESET));
    wait_us(50000);
}

/** Send a data byte to VS1053. */
void VS1053::sendDataByte(uint8_t data) {
    while (!dreq);
    bsync = 0;
    spi.write(data);
    bsync = 1;
}

/** Send a data block specified as a pointer to VS1053.
 *  @return Data length successfully sent.
 */
size_t VS1053::sendDataBlock(char* data, size_t length) {
    size_t n, sizeSent = 0;
    
    if (!data || !length) return 0;
    while (length) {
        n = length < 32 ? length : 32;
        while (!dreq);
        bsync = 0;
        for (uint32_t i = 0; i < n; i++) {
            spi.write(*data++);
            sizeSent++; length--;
        }
        bsync = 1;
    }
    return sizeSent;
}

/** Change VS1053's PLL setting for speedup. */
void VS1053::clockUp() {
    // Set CLKI to 43.0-55.3 MHz
    writeReg(SCI_CLOCKF, 0x8800);  // SC_MULT=4 (3.5x), SC_ADD=1 (+1.0x)
    wait_us(10000);
}

/** Send cancel request to VS1053.
 *  @return Zero at failure, non-zero at success.
 */
bool VS1053::sendCancel() {
    uint16_t reg;
    
    // Set SM_CANCEL bit
    reg = readReg(SCI_MODE);
    if (reg & 0x0008) {
        // Abort if SM_CANCEL is still set
        return false;
    }
    writeReg(SCI_MODE, reg | 0x0008);
    return true;
}

/** Attempt a termination of playing.
 *  Please call this repeatedly during data stream tramsission until it successes.
 *  @return Zero at failure, non-zero at success.
 */
bool VS1053::stop() {
    uint16_t reg;
    uint8_t  endFillByte;
    size_t   n, length;
    
    // If SM_CANCEL is still set, do nothing
    reg = readReg(SCI_MODE);
    if (reg & 0x0008) {
        return false;
    }
    
    // Read endFillByte from XRAM <1E06h>
    writeReg(SCI_WRAMADDR, 0x1e06);
    reg = readReg(SCI_WRAM);
    
    // Send lower 8 bits of endFillByte 2,052 times
    endFillByte = reg & 0xff;
    length = 2052;
    while (length) {
        n = length < 32 ? length : 32;
        while (!dreq);
        bsync = 0;
        for (uint32_t i = 0; i < n; i++) {
            spi.write(endFillByte);
            length--;
        }
        bsync = 1;
    }
    // Check if both HDAT0 and HDAT1 are cleared
    return readReg(SCI_HDAT0) == 0x0000 && readReg(SCI_HDAT1) == 0x0000;
}

/**
 * Set the VS1053 volume.
 *
 */
void VS1053::setVolume(uint8_t vol)
{
  uint16_t value = vol;
  value <<= 8;
  value |= vol;

  writeReg(SCI_VOL,value); // VOL
}

/** Write to an SCI (Serial Control Interface) register entry. */
void VS1053::writeReg(uint8_t addr, uint16_t word) {
    // If addr is out-of-range, do nothing
    if (addr > 0x0f) {
        return;
    }

    while (!dreq);
    cs = 0;
    spi.write(0x02);         // Send a "Write SCI" instruction (02h),
    spi.write(addr);         // target address,
    spi.write(word >> 8);    // high byte,
    spi.write(word & 0xff);  // then low byte
    while (!dreq);
    cs = 1;
}

/** Read an SCI (Serial Control Interface) register entry.
 *  @return Register value or 0000h when invalid address was specified.
 */
uint16_t VS1053::readReg(uint8_t addr) {
    uint16_t word;
    
    // If addr is out-of-range, return 0000h
    if (addr > 0x0f) {
        return 0x0000;
    }

    while (!dreq);
    cs = 0;
    spi.write(0x03);              // Send a "Read SCI" instruction (03h)
    spi.write(addr);              // and target address
    word = spi.write(0xff) << 8;  // Receive high byte with dummy data FFh
    word |= spi.write(0xff);      // Receive low byte
    while (!dreq);
    cs = 1;
    return word;
}

void VS1053::setSPIFrequency(int hz) {
    spi.frequency(hz);
}