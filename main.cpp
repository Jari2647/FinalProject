#include "mbed.h"
#include "SDFileSystem.h"  // SD card library
#include "VS1053.h"        // MP3 decoder library
#include "uLCD.hpp"        // Display library

// ----- Peripheral Configuration -----
// Adjust the pin names based on your hardware wiring
// SPI pins for VS1053 and SDCard:
#define SPI_MOSI  p11
#define SPI_MISO  p12
#define SPI_SCK   p13

// Chip selects for VS1053 and SD card:
#define MP3_CS    p8   // VS1053 control chip select
#define MP3_DCS   p9   // VS1053 data chip select (bsync in the library)
#define SD_CS     p10  // SD card chip select

// Other VS1053 control pins:
#define MP3_DREQ  p14
#define MP3_RST   p15

// uLCD pins:
#define LCD_TX    p16
#define LCD_RX    p17
#define LCD_RST   p18

// Control buttons & analog input
InterruptIn playPauseBtn(p19);
AnalogIn volumePot(p20);             // Reads voltage 0.0-1.0 (Mbed ADC)
InterruptIn audioSwitchBtn(p21);     // To toggle between outputs
DigitalOut audioSwitchCtrl(p22);     // Drives an external switching circuit

// SD card object; the name "sd" is used to open files (e.g. "/sd/song.mp3")
SDFileSystem sd(SPI_MOSI, SPI_MISO, SPI_SCK, SD_CS, "sd");

// VS1053 instance (SPI pins and control pins)
// You may pass a custom SPI frequency if needed.
VS1053 mp3(SPI_MOSI, SPI_MISO, SPI_SCK, MP3_CS, MP3_DCS, MP3_DREQ, MP3_RST);

// uLCD instance with a selected baud rate
uLCD display(LCD_TX, LCD_RX, LCD_RST, uLCD::BAUD_115200);

// Global flags
volatile bool playing = false;      // Tracks play/pause state
volatile bool audioToSpeaker = false; // false: headphone; true: speaker

// --- Interrupt Callbacks ---
// Called when the play/pause button is pressed
void togglePlayPause() {
    playing = !playing;
    // For feedback, you can update the display
    display.cls();
    display.printf("Play: %s\n", playing ? "ON" : "PAUSED");
}

// Called when the audio switch button is pressed
void toggleAudioOutput() {
    // Toggle the digital output that controls your external audio switching circuit.
    audioToSpeaker = !audioToSpeaker;
    audioSwitchCtrl = audioToSpeaker ? 1 : 0;
    // Update display with the current output
    display.locate(0, 20);
    display.printf("Output: %s", audioToSpeaker ? "Speaker" : "Headphones");
}

// --- Helper function: Map ADC reading to volume ----
// Here we assume the VS1053 volume register takes 16 bits (two 8-bit values)
// where lower numbers mean higher volume. For example, you may want 0xFEFE (quiet)
// at the low end and 0x0000 (max) at the high end. Adjust the mapping per your chip’s datasheet.
uint8_t mapVolume(float potValue) {
    // potValue will be in the range 0.0 (min) to 1.0 (max)
    // Choose a mapping as needed; here, 0.0 -> volume 0xFF (min) and 1.0 -> volume 0x00 (max)
    uint8_t vol = (uint8_t)((1.0f - potValue) * 255.0f);
    return vol;
}

// --- Audio streaming function ---
// This function demonstrates reading an MP3 file in blocks and sending data to the VS1053.
// In a real design, you would likely run this in a separate thread (or use nonblocking I/O) so that
// you can update volume and check for play/pause without interruption.
void playMP3(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        display.cls();
        display.printf("Error opening file");
        return;
    }
    
    // Display song name extracted from filename (or use metadata if available)
    display.cls();
    display.printf("Playing:\n%s", filename);
    
    char buffer[32]; // Block size for sending data (match the library’s sendDataBlock, 32 bytes per batch)
    size_t bytesRead;
    
    // Reset the VS1053 before playing:
    mp3.hardwareReset();
    mp3.clockUp();
    
    // Main streaming loop:
    while (!feof(fp)) {
        // If paused, halt streaming (you may want to sleep for a short time)
        while (!playing) {
            wait_ms(100);
        }
        
        // Optionally, update volume continuously
        uint8_t vol = mapVolume(volumePot.read());
        mp3.setVolume(vol);
        
        // Read a block of data from the file
        bytesRead = fread(buffer, 1, sizeof(buffer), fp);
        if (bytesRead > 0) {
            // Send the data block to the VS1053; this call blocks until the chip is ready.
            mp3.sendDataBlock(buffer, bytesRead);
        }
    }
    
    fclose(fp);
}

int main() {
    // Initialize display
    display.cls();
    display.printf("MP3 Player Init\n");
    
    // Configure pushbuttons with interrupts; set pull-ups if needed
    playPauseBtn.mode(PullUp);
    playPauseBtn.fall(callback(togglePlayPause));   // assuming active low button
    
    audioSwitchBtn.mode(PullUp);
    audioSwitchBtn.fall(callback(toggleAudioOutput));
    
    // Set initial state
    playing = true;         // Auto-play on start; press play/pause to toggle
    audioSwitchCtrl = 0;      // Start with headphones output (adjust as needed)
    
    // Give a short startup delay for peripherals to stabilize.
    wait_ms(500);
    
    // List files (not shown) or simply hardcode a file name.
    // For this example, assume you have a file "/sd/song.mp3"
    const char *songFilename = "/sd/song.mp3";
    
    // Start playing the song
    playMP3(songFilename);
    
    // Optionally, you could loop to allow selecting another file.
    while (1) {
        // In a full design you may allow file browsing, etc.
        wait_ms(1000);
    }
}
