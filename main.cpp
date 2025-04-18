#include "mbed.h"
#include "SDBlockDevice.h"
#include "FATFileSystem.h"
#include "VS1053.h"
#include "uLCD_4DGL.h"
#include <vector>
#include <string>
#include <cstdio> // for std namespace functions in file system
#include <cstdint> // for more integer types
#include <algorithm> // for min and max functions

// Pinouts
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

// Some variables for the song progress
std::vector<std::string> tracks;
int currentTrack = 0;
bool trackChanged = true;
bool isPaused = false;
// The key behind play and pause functionality
uint32_t resumePosition = 0;

// Constants for progress bar and song menu pages
const int BAR_WIDTH = 128;
const int BAR_HEIGHT = 4;
const int BAR_Y = 72;
const int BITRATE_BYTES_PER_SEC = 16000;
static const int ITEMS_PER_PAGE = 10;  // number of songs per page in menu

// Repeatedly called progress bar function
void drawProgressBar(float percent) {
    int filled = static_cast<int>(percent * BAR_WIDTH);
    uLCD.filled_rectangle(0, BAR_Y - BAR_HEIGHT, BAR_WIDTH, BAR_Y, BLACK);
    uLCD.filled_rectangle(0, BAR_Y - BAR_HEIGHT, filled, BAR_Y, GREEN);
}

// Call when user scrolls through songs
void updateTrackCountDisplay() {
    uLCD.locate(0, 0);
    uLCD.color(WHITE);
    uLCD.printf("Song %d/%d", currentTrack + 1, tracks.size());
}

// Simple text for play/pause in top right of lcd
void updatePlayPauseStatus() {
    uLCD.locate(11, 0);
    uLCD.color(WHITE);
    if (isPaused) {
        uLCD.printf("Paused ");
    } else {
        uLCD.printf("Playing");
    }
}

// Find the song title and display it
void displayTrackTitle(const std::string& path) {
    uLCD.cls();
    std::string title = path.substr(path.find_last_of("/") + 1);
    if (title.length() > 20) title = title.substr(0, 20);
    int xPos = std::max(0, 8 - static_cast<int>(title.length() / 2));
    uLCD.locate(xPos, 6);
    uLCD.color(WHITE);
    uLCD.printf("%s", title.c_str());

    // draw the progress bar and track info when new song plays
    drawProgressBar(0.0f);
    updateTrackCountDisplay();
    updatePlayPauseStatus();
}

// Song menu with control for multiple pages
// Theres 10 songs in a page, which fits well on the lcd
void displayMenu(int selectedIndex) {
    uLCD.cls();
    uLCD.color(WHITE);
    uLCD.locate(1, 0);
    uLCD.printf("Select a song:");

    int page = selectedIndex / ITEMS_PER_PAGE;
    int start = page * ITEMS_PER_PAGE;
    int end = std::min(start + ITEMS_PER_PAGE, (int)tracks.size());

    for (int i = start; i < end; i++) {
        std::string name = tracks[i].substr(tracks[i].find_last_of("/") + 1);
        if (name.length() > 12) name = name.substr(0, 12);

        int row = (i - start) + 2;  // first entry at row 2
        uLCD.locate(1, row);
        // Highlight the selected song in green, every other song in white
        if (i == selectedIndex) {
            uLCD.color(GREEN);
            uLCD.printf("> %s", name.c_str());
        } else {
            uLCD.color(WHITE);
            uLCD.printf("  %s", name.c_str());
        }
    }

    // Track which page we're on in the bottom left of the screen
    int totalPages = (tracks.size() + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE;
    uLCD.locate(0, 15);
    uLCD.color(WHITE);
    uLCD.printf("Page %d/%d", page + 1, totalPages);
}

// Nav switch controls for scrubbing through the menu
void selectTrackMenu() {
    int selection = 0;
    displayMenu(selection);

    while (true) {
        if (!navUp) {
            selection = (selection - 1 + tracks.size()) % tracks.size();
            displayMenu(selection);
            // Call some sleeps so the mbed can keep up
            // This makes the menu display a bit slow as it goes through all 10 songs
            ThisThread::sleep_for(200ms);
            // Some delay to avoid double-inputs
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

// On reset, put up a simple loading screen and initialize with function calls
void initializePlayer() {
    uLCD.cls();
    uLCD.color(WHITE);
    uLCD.locate(4, 6);
    uLCD.printf("Loading...");

    audio.hardwareReset();
    // Delay a little after reset so that the VS1053 initializes properly
    ThisThread::sleep_for(100ms);
    audio.modeSwitch();
    audio.clockUp();

    uint8_t vol = static_cast<uint8_t>(255 * (1.0f - volumeKnob.read()));
    audio.setVolume(vol); // initial volume reading from potentiometer

    // Attempt to mount the sd with the file system so we can start reading song files
    sd.frequency(4000000);
    if (fs.mount(&sd)) {
        uLCD.cls();
        uLCD.locate(2, 6);
        uLCD.printf("SD Fail");
        return;
    }

    // Once mounted, create the directory that the mbed can read
    DIR *dir = opendir("/sd");
    if (dir) {
        // file system entry
        struct dirent *ent;
        while ((ent = readdir(dir)) != nullptr) {
            std::string filename = ent->d_name;
            // Check the file name and ensure it is a type mp3 before continuing
            if (filename.length() > 4 && filename.substr(filename.length() - 4) == ".mp3") {
                tracks.push_back("/sd/" + filename);
            }
        }
        // Close the directory once read
        closedir(dir);
    }
}

// Playback
void playCurrentTrack() {
    if (tracks.empty()) return;

    FILE *file = fopen(tracks[currentTrack].c_str(), "rb");
    if (!file) return;

    // File seek functionality with SDBlockDevice API
    // It lets us keep track of which chunk of the song we're in
    // This is the core of the play and pause functionality.
    fseek(file, 0, SEEK_END);
    long totalBytes = ftell(file);
    fseek(file, 0, SEEK_SET);
    /* The resume position variable, initialized to zero at the beginning, is maintained through the
    program to let us go back to a certain point in songs
    */
    fseek(file, resumePosition, SEEK_SET);

    // An array that holds 32 bytes of audio data
    // We will play our songs in chunks at a time
    char buffer[32];
    // The amount of bytes read by the buffer
    // Should be 32 bytes up until near the end of a song, where it can fill with less
    size_t bytesRead;
    // A counter that serves essentially lets us poll volume changes
    uint32_t counter = 0;

    while (true) {
        // Return to menu
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

        // Pause/Resume
        if (!navCenter) {
            ThisThread::sleep_for(200ms);
            isPaused = !isPaused;
            resumePosition = ftell(file);
            updatePlayPauseStatus();
            while (!navCenter) ThisThread::sleep_for(50ms);
        }

        if (!isPaused) {
            // Skip tracks
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
            // This is where we fill the buffer with song data
            bytesRead = fread(buffer, 1, sizeof(buffer), file);
            if (bytesRead == 0) break;
            // Send the song data to the VS1053 for decoding
            audio.sendDataBlock(buffer, bytesRead);
        }

        // Only check potentiometer and update progess bar every 200 iterations
        /* This reduces strain on the mbed and should ensure that updating volume/progress doesn't hurt
        audio quality. An issue that persisted when we tried to display volume.
        */
        if (++counter % 200 == 0) {
            uint8_t vol = static_cast<uint8_t>(255 * (1.0f - volumeKnob.read()));
            audio.setVolume(vol);

            long currentPos = ftell(file);
            float progress = static_cast<float>(currentPos) / totalBytes;
            drawProgressBar(progress);
        }
    }
    // Close the current song file
    fclose(file);
    // Reset the track position to the beginning when a new song starts.
    if (!trackChanged) resumePosition = 0;
}

// The main program
int main() {
    initializePlayer();
    // If the SD card goes unread, throw up some text on the lcd
    if (tracks.empty()) {
        uLCD.cls();
        uLCD.locate(2, 6);
        uLCD.printf("No MP3s");
        return 1;
    }

    // Load the song menu
    selectTrackMenu();

    // Program loop to initialize audio. Contingent on song changes.
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
