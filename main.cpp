#include "mbed.h"
#include "SDBlockDevice.h"
#include "FATFileSystem.h"
#include "VS1053.h"
#include <vector>
#include <string>
#include <cstdio> 
#include <cstdint> 

// Hardware Configuration
VS1053 audio(p11, p12, p13, p14, p15, p16, p17); // SPI pins + control
SDBlockDevice sd(p5, p6, p7, p8);                // SPI pins + CS
FATFileSystem fs("sd");

// User Controls
AnalogIn volumeKnob(p19);
DigitalIn playButton(p21, PullUp);
DigitalIn nextButton(p22, PullUp);
DigitalIn prevButton(p23, PullUp);

// Player State
std::vector<std::string> tracks;
int currentTrack = 0;
bool trackChanged = true;
bool isPaused = false;
uint32_t resumePosition = 0;

void initializePlayer() {
    // Reset and configure VS1053
    audio.hardwareReset();
    ThisThread::sleep_for(100ms);
    audio.modeSwitch();
    audio.clockUp();
    
    // Set initial volume
    uint8_t vol = static_cast<uint8_t>(255 * (1.0f - volumeKnob.read()));
    audio.setVolume(vol);
    
    // Configure SD card
    sd.frequency(4000000); // 4MHz
    if (fs.mount(&sd)) {
        printf("SD Card Mount Failed!\n");
        return;
    }
    
    // Load MP3 files
    DIR *dir = opendir("/sd");
    if (dir) {
        struct dirent *ent;
        while ((ent = readdir(dir)) != nullptr) {
            std::string filename = ent->d_name;
            if (filename.length() > 4 && 
                filename.substr(filename.length() - 4) == ".mp3") {
                tracks.push_back("/sd/" + filename);
                printf("Found: %s\n", filename.c_str());
            }
        }
        closedir(dir);
    }
}

void playCurrentTrack() {
    if (tracks.empty()) return;
    
    FILE *file = fopen(tracks[currentTrack].c_str(), "rb");
    if (!file) {
        printf("Failed to open: %s\n", tracks[currentTrack].c_str());
        return;
    }

    printf("Now Playing: %s\n", tracks[currentTrack].c_str());
    
    // Set file position
    if (trackChanged) {
        fseek(file, 0, SEEK_SET);
        resumePosition = 0;
    } else {
        fseek(file, resumePosition, SEEK_SET);
    }

    char buffer[32];
    size_t bytesRead;
    uint32_t updateCounter = 0;

    while (true) {
        // Check for button presses
        if (!playButton) {
            ThisThread::sleep_for(200ms);
            isPaused = !isPaused;
            if (isPaused) {
                resumePosition = ftell(file);
                printf("Paused at position: %u\n", resumePosition);
            } else {
                printf("Resuming playback\n");
            }
            while (!playButton) ThisThread::sleep_for(50ms);
        }

        if (!isPaused) {
            // Check for track change requests
            if (!nextButton || !prevButton) break;

            // Read and play audio data
            bytesRead = fread(buffer, 1, sizeof(buffer), file);
            if (bytesRead == 0) break; // End of file
            
            audio.sendDataBlock(buffer, bytesRead);
        }

        // Periodic updates
        if (++updateCounter % 100 == 0) {
            // Update volume
            uint8_t vol = static_cast<uint8_t>(255 * (1.0f - volumeKnob.read()));
            audio.setVolume(vol);
            
            // Small delay to prevent CPU overload
            ThisThread::sleep_for(1ms);
        }
    }

    fclose(file);
    if (!trackChanged) {
        // Only reset position if we finished normally
        resumePosition = 0;
    }
}

int main() {
    printf("\n=== MP3 Player Initializing ===\n");
    
    initializePlayer();
    if (tracks.empty()) {
        printf("No MP3 files found!\n");
        return 1;
    }

    printf("\n=== Ready to Play ===\n");
    
    while (true) {
        if (trackChanged) {
            audio.hardwareReset();
            ThisThread::sleep_for(100ms);
            audio.modeSwitch();
            audio.clockUp();
            trackChanged = false;
        }

        playCurrentTrack();

        // Handle track navigation
        if (!nextButton) {
            ThisThread::sleep_for(200ms);
            currentTrack = (currentTrack + 1) % tracks.size();
            trackChanged = true;
            while (!nextButton) ThisThread::sleep_for(50ms);
        }
        else if (!prevButton) {
            ThisThread::sleep_for(200ms);
            currentTrack = (currentTrack - 1 + tracks.size()) % tracks.size();
            trackChanged = true;
            while (!prevButton) ThisThread::sleep_for(50ms);
        }

        ThisThread::sleep_for(50ms);
    }
}