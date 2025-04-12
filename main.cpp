#include "mbed.h"
#include "SDBlockDevice.h"
#include "FATFileSystem.h"
#include "VS1053.h"
#include "uLCD_4DGL.h"
#include <vector>
#include <string>
#include <cstdio>
#include <cstdint>

// Hardware Configuration
VS1053 audio(p11, p12, p13, p14, p15, p16, p17);
SDBlockDevice sd(p5, p6, p7, p8);
FATFileSystem fs("sd");
uLCD_4DGL uLCD(p28, p27, p20);  // TX, RX, RESET

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

// Constants
const int BAR_WIDTH = 128;         // uLCD width
const int BAR_HEIGHT = 4;
const int BAR_Y = 72;              // ~9 lines down (6 title + 2 spacing)

const int BITRATE_BYTES_PER_SEC = 16000; // ~128 kbps

void drawProgressBar(float percent) {
    int filled = static_cast<int>(percent * BAR_WIDTH);
    uLCD.filled_rectangle(0, BAR_Y - BAR_HEIGHT, BAR_WIDTH, BAR_Y, BLACK);  // clear bar
    uLCD.filled_rectangle(0, BAR_Y - BAR_HEIGHT, filled, BAR_Y, GREEN);
}

void displayTrackTitle(const std::string& path, int durationSec) {
    uLCD.cls();

    std::string title = path.substr(path.find_last_of("/") + 1);
    if (title.length() > 20) title = title.substr(0, 20);

    int xPos = std::max(0, 8 - static_cast<int>(title.length() / 2));
    uLCD.locate(xPos, 6);
    uLCD.color(WHITE);
    uLCD.printf("%s", title.c_str());

    drawProgressBar(0.0f);
}

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

void playCurrentTrack() {
    if (tracks.empty()) return;

    FILE *file = fopen(tracks[currentTrack].c_str(), "rb");
    if (!file) return;

    fseek(file, 0, SEEK_END);
    long totalBytes = ftell(file);
    int estimatedDurationSec = totalBytes / BITRATE_BYTES_PER_SEC;
    fseek(file, 0, SEEK_SET);

    if (trackChanged) {
        displayTrackTitle(tracks[currentTrack], estimatedDurationSec);
        resumePosition = 0;
        trackChanged = false;
    } else {
        fseek(file, resumePosition, SEEK_SET);
    }

    char buffer[32];
    size_t bytesRead;
    uint32_t counter = 0;

    while (true) {
        if (!playButton) {
            ThisThread::sleep_for(200ms);
            isPaused = !isPaused;
            resumePosition = ftell(file);
            while (!playButton) ThisThread::sleep_for(50ms);
        }

        if (!isPaused) {
            if (!nextButton || !prevButton) break;

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

int main() {
    initializePlayer();
    if (tracks.empty()) {
        uLCD.cls();
        uLCD.locate(2, 6);
        uLCD.printf("No MP3s");
        return 1;
    }

    while (true) {
        if (trackChanged) {
            audio.hardwareReset();
            ThisThread::sleep_for(100ms);
            audio.modeSwitch();
            audio.clockUp();
        }

        playCurrentTrack();

        if (!nextButton) {
            ThisThread::sleep_for(200ms);
            currentTrack = (currentTrack + 1) % tracks.size();
            trackChanged = true;
            while (!nextButton) ThisThread::sleep_for(50ms);
        } else if (!prevButton) {
            ThisThread::sleep_for(200ms);
            currentTrack = (currentTrack - 1 + tracks.size()) % tracks.size();
            trackChanged = true;
            while (!prevButton) ThisThread::sleep_for(50ms);
        }

        ThisThread::sleep_for(50ms);
    }
}
