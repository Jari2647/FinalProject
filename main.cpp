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

// === State ===
bool playing = true;

// === Helper: Volume Control ===
void updateVolume() {
    float level = volumePot.read();
    uint8_t vol = static_cast<uint8_t>(255 * (1.0f - level));
    vs1053.setVolume(vol);
    printf("[Volume] Set to %d (analog: %.2f)\n", vol, level);
}

// === Playback Logic ===
void playMP3(const char* filename) {
    printf("[INFO] Trying to open: %s\n", filename);
    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        printf("[ERROR] Cannot open file: %s\n", filename);
        return;
    }

    printf("[INFO] Opened file successfully!\n");

    playing = true;
    char buffer[512];
    size_t bytesRead;
    int blockCount = 0;

    while (playing && (bytesRead = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        vs1053.sendDataBlock(buffer, bytesRead);
        blockCount++;

        if (blockCount % 100 == 0) {
            updateVolume();
        }

        if (blockCount % 10 == 0) {
            printf("[INFO] Playing block %d...\n", blockCount);
        }

        if (playPauseBtn == 0) {
            ThisThread::sleep_for(200ms);
            playing = false;
            printf("[INFO] Playback paused by button.\n");
            break;
        }
    }

    fclose(fp);
    printf("[INFO] Finished playback after %d blocks.\n", blockCount);
}

int main() {
    printf("\n=== MP3 Player Boot ===\n");

    // Initialize VS1053
    printf("[INFO] Resetting VS1053...\n");
    vs1053.hardwareReset();
    vs1053.modeSwitch();
    vs1053.clockUp();
    vs1053.setSPIFrequency(8000000);  // << Set VS1053 SPI to 8MHz
    printf("[INFO] VS1053 initialized.\n");

    ThisThread::sleep_for(500ms);

    // Boost SD card frequency
    printf("[INFO] Setting SD card SPI frequency...\n");
    sd.frequency(4000000);  // << Set SD SPI to 4MHz

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

    while (true) {
        ThisThread::sleep_for(500ms);
    }
}
