#include "mbed.h"
#include "SDBlockDevice.h"
#include "FATFileSystem.h"
#include "VS1053.h"

using namespace std::chrono_literals;

// === VS1053 Setup (shared SPI) ===
VS1053 vs1053(p11, p12, p13, p14, p15, p16, p17);  // SPI + VS1053 control pins

// === SD Card Setup (shared SPI) ===
SDBlockDevice sd(p5, p6, p7, p8);  // SPI + CS
FATFileSystem fs("sd");

// === Controls ===
AnalogIn volumePot(p19);
DigitalIn playPauseBtn(p21, PullUp);
DigitalIn nextBtn(p22, PullUp);
DigitalIn prevBtn(p23, PullUp);

// === Global Playback Data ===
// We use a static variable to hold the resume offset across playbacks.
static long resumeOffset = 0;

// === Helper: Volume Control ===
void updateVolume() {
    float level = volumePot.read();
    uint8_t vol = static_cast<uint8_t>(255 * (1.0f - level));
    uint8_t percent = static_cast<uint8_t>(100 * level);
    vs1053.setVolume(vol);
    printf("[Volume] Set to %d%%\n", percent);
}

// === Playback Logic with Pause/Resume Support ===
// This function opens the file, seeks to the resumeOffset if any, and plays it block-by-block.
// Pressing the play/pause button toggles a "paused" state.
// When pausing, we save the current file position so that playback may resume from this point.
void playMP3(const char* filename) {
    printf("[INFO] Trying to open: %s\n", filename);
    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        printf("[ERROR] Cannot open file: %s\n", filename);
        return;
    }
    
    // If resuming from an earlier pause, seek to the saved offset.
    if (resumeOffset > 0) {
        fseek(fp, resumeOffset, SEEK_SET);
        printf("[INFO] Resuming from file offset: %ld\n", resumeOffset);
    } else {
        printf("[INFO] Starting playback from beginning.\n");
    }

    bool paused = false;   // Playback state flag: false = playing, true = paused.
    char buffer[512];
    size_t bytesRead;
    int blockCount = 0;

    while (true) {
        // Check if play/pause button is pressed to toggle pause/resume
        if (playPauseBtn == 0) {
            // Simple debounce delay
            ThisThread::sleep_for(200ms);
            // Toggle paused state
            paused = !paused;
            if (paused) {
                // Save current position before pausing.
                resumeOffset = ftell(fp);
                printf("[INFO] Playback paused at block %d, file offset: %ld.\n", blockCount, resumeOffset);
            } else {
                printf("[INFO] Resuming playback at block %d, file offset: %ld.\n", blockCount, resumeOffset);
            }
            // Wait until the button is released to avoid bouncing.
            while (playPauseBtn == 0) {
                ThisThread::sleep_for(50ms);
            }
        }

        // If paused, simply wait here until resumed.
        if (paused) {
            ThisThread::sleep_for(50ms);
            continue;
        }

        // Read a block of data from the file.
        bytesRead = fread(buffer, 1, sizeof(buffer), fp);
        if (bytesRead == 0) {
            // End of file reached.
            break;
        }
        
        // Send the data block to the VS1053 decoder.
        vs1053.sendDataBlock(buffer, bytesRead);
        blockCount++;

        // Update volume every 100 blocks.
        if (blockCount % 100 == 0) {
            updateVolume();
        }
        if (blockCount % 10 == 0) {
            printf("[INFO] Playing block %d...\n", blockCount);
        }
    }

    fclose(fp);
    // When playback reaches the end, reset resumeOffset.
    resumeOffset = 0;
    printf("[INFO] Finished playback after %d blocks.\n", blockCount);
}

int main() {
    printf("\n=== MP3 Player Boot ===\n");

    // Initialize VS1053
    printf("[INFO] Resetting VS1053...\n");
    vs1053.hardwareReset();
    vs1053.modeSwitch();
    vs1053.clockUp();
    vs1053.setSPIFrequency(8000000);  // Set VS1053 SPI to 8MHz
    printf("[INFO] VS1053 initialized.\n");

    ThisThread::sleep_for(500ms);

    // Boost SD card frequency
    printf("[INFO] Setting SD card SPI frequency...\n");
    sd.frequency(4000000);  // Set SD SPI to 4MHz

    // Mount the SD Card filesystem
    printf("[INFO] Mounting SD card filesystem...\n");
    int err = fs.mount(&sd);
    if (err) {
        printf("[ERROR] Filesystem mount failed with error: %d\n", err);
    } else {
        printf("[INFO] Filesystem mounted successfully.\n");
    }

    DIR* d = opendir("/sd");
    if (!d) {
        printf("[ERROR] SD card directory not found!\n");
    } else {
        printf("[INFO] SD card directory detected.\n");
        closedir(d);
    }

    updateVolume();

    printf("[INFO] Starting playback...\n");
    playMP3("/sd/track01.mp3");

    // Main thread idle loop.
    while (true) {
        ThisThread::sleep_for(500ms);
    }
}
