#include "mbed.h"
#include "SDBlockDevice.h"
#include "FATFileSystem.h"
#include "VS1053.h"
#include "uLCD_4DGL.h"
#include <vector>
#include <string>
#include <cstdio>
#include <cstdint>
#include <algorithm>

// Hardware Configuration
VS1053 audio(p11, p12, p13, p14, p15, p16, p17);
SDBlockDevice sd(p5, p6, p7, p8);
FATFileSystem fs("sd");
uLCD_4DGL uLCD(p28, p27, p20);  // TX, RX, RESET

// User Controls
AnalogIn volumeKnob(p19);
DigitalIn navRight(p29, PullUp);   // Next track
DigitalIn navLeft(p26, PullUp);    // Previous track
DigitalIn navUp(p24, PullUp);      // Menu up
DigitalIn navDown(p25, PullUp);    // Menu down
DigitalIn navCenter(p30, PullUp);  // Menu select + Play/Pause
DigitalIn menuButton(p21, PullUp); // Dedicated button to return to menu

// Player State
std::vector<std::string> tracks;
int currentTrack = 0;
bool trackChanged = true;
bool isPaused = false;
uint32_t resumePosition = 0;

// Constants
const int BAR_WIDTH = 128;
const int BAR_HEIGHT = 4;
const int BAR_Y = 72;
const int BITRATE_BYTES_PER_SEC = 16000;

// === UI Utilities ===
void drawProgressBar(float percent) {
    int filled = static_cast<int>(percent * BAR_WIDTH);
    uLCD.filled_rectangle(0, BAR_Y - BAR_HEIGHT, BAR_WIDTH, BAR_Y, BLACK);
    uLCD.filled_rectangle(0, BAR_Y - BAR_HEIGHT, filled, BAR_Y, GREEN);
}

void updateTrackCountDisplay() {
    uLCD.locate(0, 0);
    uLCD.color(WHITE);
    uLCD.printf("Song %d/%d", currentTrack + 1, tracks.size());
}

void updatePlayPauseStatus() {
    uLCD.locate(11, 0);
    uLCD.color(WHITE);
    if (isPaused) {
        uLCD.printf("Paused ");
    } else {
        uLCD.printf("Playing");
    }
}

void displayTrackTitle(const std::string& path) {
    uLCD.cls();
    std::string title = path.substr(path.find_last_of("/") + 1);
    if (title.length() > 20) title = title.substr(0, 20);
    int xPos = std::max(0, 8 - static_cast<int>(title.length() / 2));
    uLCD.locate(xPos, 6);
    uLCD.color(WHITE);
    uLCD.printf("%s", title.c_str());

    drawProgressBar(0.0f);
    updateTrackCountDisplay();
    updatePlayPauseStatus();
}

// === Menu ===
void displayMenu(int selectedIndex) {
    uLCD.cls();
    uLCD.color(WHITE);
    uLCD.locate(1, 0);
    uLCD.printf("Select a song:");
    for (int i = 0; i < std::min((int)tracks.size(), 13); i++) {
        std::string name = tracks[i].substr(tracks[i].find_last_of("/") + 1);
        if (name.length() > 12) name = name.substr(0, 12);

        uLCD.locate(1, i + 2);
        if (i == selectedIndex) {
            uLCD.color(GREEN);
            uLCD.printf("> %s", name.c_str());
        } else {
            uLCD.color(WHITE);
            uLCD.printf("  %s", name.c_str());
        }
    }
}

void selectTrackMenu() {
    int selection = 0;
    displayMenu(selection);

    while (true) {
        if (!navUp) {
            selection = (selection - 1 + tracks.size()) % tracks.size();
            displayMenu(selection);
            ThisThread::sleep_for(200ms);
            while (!navUp) ThisThread::sleep_for(20ms);
        }
        if (!navDown) {
            selection = (selection + 1) % tracks.size();
            displayMenu(selection);
            ThisThread::sleep_for(200ms);
            while (!navDown) ThisThread::sleep_for(20ms);
        }
        if (!navCenter) {
            currentTrack = selection;
            trackChanged = true;
            while (!navCenter) ThisThread::sleep_for(20ms);
            break;
        }
        ThisThread::sleep_for(50ms);
    }
}

// === Initialization ===
void initializePlayer() {
    uLCD.cls();
    uLCD.color(WHITE);
    uLCD.locate(4, 6);
    uLCD.printf("Loading...");

    audio.hardwareReset();
    ThisThread::sleep_for(100ms);
    audio.modeSwitch();
    audio.clockUp();

    uint8_t vol = static_cast<uint8_t>(255 * (1.0f - volumeKnob.read()));
    audio.setVolume(vol);

    sd.frequency(4000000);
    if (fs.mount(&sd)) {
        uLCD.cls();
        uLCD.locate(2, 6);
        uLCD.printf("SD Fail");
        return;
    }

    DIR *dir = opendir("/sd");
    if (dir) {
        struct dirent *ent;
        while ((ent = readdir(dir)) != nullptr) {
            std::string filename = ent->d_name;
            if (filename.length() > 4 && filename.substr(filename.length() - 4) == ".mp3") {
                tracks.push_back("/sd/" + filename);
            }
        }
        closedir(dir);
    }
}

// === Playback ===
void playCurrentTrack() {
    if (tracks.empty()) return;

    FILE *file = fopen(tracks[currentTrack].c_str(), "rb");
    if (!file) return;

    fseek(file, 0, SEEK_END);
    long totalBytes = ftell(file);
    fseek(file, 0, SEEK_SET);
    fseek(file, resumePosition, SEEK_SET);

    char buffer[32];
    size_t bytesRead;
    uint32_t counter = 0;

    while (true) {
        // Return to menu if menu button is pressed
        if (!menuButton) {
            ThisThread::sleep_for(200ms);
            fclose(file);
            while (!menuButton) ThisThread::sleep_for(20ms);
            selectTrackMenu();
            displayTrackTitle(tracks[currentTrack]);
            resumePosition = 0;
            updatePlayPauseStatus();
            break;
        }

        // Toggle play/pause with center button
        if (!navCenter) {
            ThisThread::sleep_for(200ms);
            isPaused = !isPaused;
            resumePosition = ftell(file);
            updatePlayPauseStatus();
            while (!navCenter) ThisThread::sleep_for(50ms);
        }

        if (!isPaused) {
            // Handle next/prev during playback
            if (!navRight) {
                ThisThread::sleep_for(200ms);
                currentTrack = (currentTrack + 1) % tracks.size();
                trackChanged = true;
                fclose(file);
                while (!navRight) ThisThread::sleep_for(50ms);
                break;
            } else if (!navLeft) {
                ThisThread::sleep_for(200ms);
                currentTrack = (currentTrack - 1 + tracks.size()) % tracks.size();
                trackChanged = true;
                fclose(file);
                while (!navLeft) ThisThread::sleep_for(50ms);
                break;
            }

            bytesRead = fread(buffer, 1, sizeof(buffer), file);
            if (bytesRead == 0) break;
            audio.sendDataBlock(buffer, bytesRead);
        }

        if (++counter % 200 == 0) {
            uint8_t vol = static_cast<uint8_t>(255 * (1.0f - volumeKnob.read()));
            audio.setVolume(vol);

            long currentPos = ftell(file);
            float progress = static_cast<float>(currentPos) / totalBytes;
            drawProgressBar(progress);
        }
    }

    fclose(file);
    if (!trackChanged) resumePosition = 0;
}

// === Main ===
int main() {
    initializePlayer();
    if (tracks.empty()) {
        uLCD.cls();
        uLCD.locate(2, 6);
        uLCD.printf("No MP3s");
        return 1;
    }

    // Only show menu once at the beginning
    selectTrackMenu();

    while (true) {
        if (trackChanged) {
            audio.hardwareReset();
            ThisThread::sleep_for(100ms);
            audio.modeSwitch();
            audio.clockUp();
            displayTrackTitle(tracks[currentTrack]);
            resumePosition = 0;
            trackChanged = false;
        }

        playCurrentTrack();
        ThisThread::sleep_for(50ms);
    }
}