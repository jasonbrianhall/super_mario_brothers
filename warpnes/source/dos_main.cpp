#include <stdio.h>
#include <stdlib.h>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include "AllegroMainWindow.hpp"
#include "SMB/SMBEmulator.hpp" 
#include "Emulation/Controller.hpp"
#include "Configuration.hpp"
#include "Constants.hpp"


// DOS-specific includes (will be ignored on Linux)
#ifdef __DJGPP__
#include <conio.h>
#include <dos.h>
#include <pc.h>
#else
#include <chrono>
#endif

//#define DEBUG_SCALING_CACHE 1

static AUDIOSTREAM* audiostream = NULL;

// Global variables for DOS compatibility
static SMBEmulator* smbEngine = NULL;
static uint16_t renderBuffer[RENDER_WIDTH * RENDER_HEIGHT];
AllegroMainWindow* g_mainWindow = NULL;

// Audio streaming variables for DOS
static int audioStreamBuffer[4096];  // Large stream buffer
static int audioStreamPos = 0;
static int audioStreamLen = 0;
static bool dosAudioInitialized = false;

// Wave file debugging
static FILE* waveFile = NULL;
static int waveSampleCount = 0;
static bool enableWaveOutput = true;  // Set to true to enable wave file output

// Forward declarations
void initWaveFile();
void writeWaveData(uint8_t* buffer, int length);
void closeWaveFile();


// Zapper
static bool zapperEnabled = false;
static int mouseX = 0, mouseY = 0;
static bool mousePressed = false;

// WAV file header structure
struct WaveHeader {
    char riff[4] = {'R','I','F','F'};
    uint32_t fileSize;
    char wave[4] = {'W','A','V','E'};
    char fmt[4] = {'f','m','t',' '};
    uint32_t fmtSize = 16;
    uint16_t audioFormat = 1;  // PCM
    uint16_t numChannels = 1;  // Mono
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign = 1;
    uint16_t bitsPerSample = 8;
    char data[4] = {'d','a','t','a'};
    uint32_t dataSize;
};

void initWaveFile() {
    if (!enableWaveOutput) return;
    
    waveFile = fopen("smb_audio_debug.wav", "wb");
    if (waveFile) {
        // Write placeholder header (we'll update it when closing)
        WaveHeader header;
        header.sampleRate = Configuration::getAudioFrequency();
        header.byteRate = header.sampleRate * header.numChannels * (header.bitsPerSample / 8);
        header.fileSize = 0;  // Will update later
        header.dataSize = 0;  // Will update later
        
        fwrite(&header, sizeof(header), 1, waveFile);
        waveSampleCount = 0;
        printf("Started recording audio to smb_audio_debug.wav\n");
    }
}

void writeWaveData(uint8_t* buffer, int length) {
    if (!enableWaveOutput || !waveFile) return;
    
    // Convert unsigned 8-bit to signed for standard WAV format
    static int8_t signedBuffer[2048];
    for (int i = 0; i < length && i < 2048; i++) {
        signedBuffer[i] = (int8_t)(buffer[i] - 128);
    }
    
    fwrite(signedBuffer, 1, length, waveFile);
    waveSampleCount += length;
    
    // Auto-close after ~10 seconds to prevent huge files
    if (waveSampleCount > Configuration::getAudioFrequency() * 10) {
        closeWaveFile();
    }
}

void closeWaveFile() {
    if (!waveFile) return;
    
    // Update header with actual sizes
    uint32_t dataSize = waveSampleCount;
    uint32_t fileSize = sizeof(WaveHeader) + dataSize - 8;
    
    fseek(waveFile, 4, SEEK_SET);
    fwrite(&fileSize, 4, 1, waveFile);
    
    fseek(waveFile, sizeof(WaveHeader) - 4, SEEK_SET);
    fwrite(&dataSize, 4, 1, waveFile);
    
    fclose(waveFile);
    waveFile = NULL;
    
    printf("Saved %d audio samples to smb_audio_debug.wav (%d seconds)\n", 
           waveSampleCount, waveSampleCount / Configuration::getAudioFrequency());
}

// Timer for frame rate control
volatile int timer_counter = 0;

void timer_callback() {
    timer_counter++;
}
END_OF_FUNCTION(timer_callback)

// Audio callback for Allegro
void audio_stream_callback(void* buffer, int len)
{
    static int underrunCount = 0;
    
    if (!smbEngine || !Configuration::getAudioEnabled()) {
        memset(buffer, 0, len);
        return;
    }
    
    static uint8_t temp_buffer[4096];
    int actual_len = (len > 4096) ? 4096 : len;
    
    // CHECK: Is the engine actually providing samples?
    smbEngine->audioCallback(temp_buffer, actual_len);
    
    // Debug: Check for silence (indicates buffer issues)
    bool allSilence = true;
    for (int i = 0; i < 10 && i < actual_len; i++) {
        if (temp_buffer[i] != 128) { // 128 = unsigned silence
            allSilence = false;
            break;
        }
    }
    
    if (allSilence) {
        underrunCount++;
        if (underrunCount % 60 == 0) { // Print every second
            printf("Audio underrun detected (%d)\n", underrunCount);
        }
    }
    
    // Convert to signed
    int8_t* signed_buffer = (int8_t*)buffer;
    for (int i = 0; i < actual_len; i++) {
        signed_buffer[i] = (int8_t)((int)temp_buffer[i] - 128);
    }
    
    if (len > actual_len) {
        memset(signed_buffer + actual_len, 0, len - actual_len);
    }
}
END_OF_FUNCTION(audio_stream_callback)

AllegroMainWindow::AllegroMainWindow() 
    : game_buffer(NULL), back_buffer(NULL), gameRunning(false), gamePaused(false), 
      showingMenu(false), selectedMenuItem(0), inMenu(false),
      isCapturingInput(false), currentDialog(DIALOG_NONE),
      statusMessageTimer(0), currentFrameBuffer(NULL),
      screenBuffer16(NULL), useDirectRendering(true),
      currentCaptureType(CAPTURE_NONE), currentConfigPlayer(PLAYER_1),
      lineStartOffsets(NULL), useOptimizedScaling(true),
      selectedVideoOption(0), numAvailableModes(0)  // ADD THESE TWO
#ifndef __DJGPP__
      , isFullscreen(false), windowedWidth(640), windowedHeight(480)
#endif
{
    strcpy(statusMessage, "Ready");
    strcpy(currentCaptureKey, "");
    menuCount = 0;
    g_mainWindow = this;
    gamePaused = false;
    showingMenu = false;
    currentDialog = DIALOG_NONE;
    setupDefaultControls();
    
    // Initialize video settings - ADD THIS BLOCK
    videoSettings.currentMode = 0;
    videoSettings.scalingMode = 0;  // Nearest neighbor
    videoSettings.maintainAspect = true;
    videoSettings.centerImage = true;
    videoSettings.brightness = 128;
    videoSettings.contrast = 128;
}

void AllegroMainWindow::drawGameCached(BITMAP* target)
{
    // No more caching needed in main window - PPU handles it
    drawGameBuffered(target);
}


#ifdef __DJGPP__
// DOS-specific optimization for 320x200
void AllegroMainWindow::drawGameDOS320x200(BITMAP* target)
{
    static uint16_t nesBuffer[256 * 240];
    smbEngine->render16(nesBuffer);
    
    // 320x200: Center 256x240 image with slight letterboxing
    // Scale factor is approximately 1.25x horizontally, 0.83x vertically
    // We'll use nearest neighbor for speed
    
    const int dest_x = (320 - 256) / 2;  // 32 pixels on each side
    const int dest_y = 0;  // Start at top, slight bottom crop
    
    if (bitmap_color_depth(target) == 16) {
        uint16_t* screen_buffer = (uint16_t*)target->line[0];
        
        // Clear screen
        memset(screen_buffer, 0, 320 * 200 * sizeof(uint16_t));
        
        // Copy with slight vertical scaling
        for (int y = 0; y < 200; y++) {
            int src_y = (y * 240) / 200;  // Map 200 lines to 240 source lines
            if (src_y >= 240) src_y = 239;
            
            uint16_t* src_row = &nesBuffer[src_y * 256];
            uint16_t* dest_row = &screen_buffer[y * 320 + dest_x];
            
            // Direct copy - no horizontal scaling needed for 256->256 portion
            memcpy(dest_row, src_row, 256 * sizeof(uint16_t));
        }
    } else {
        // Fallback for non-16-bit
        clear_to_color(target, makecol(0, 0, 0));
        
        for (int y = 0; y < 200; y++) {
            int src_y = (y * 240) / 200;
            if (src_y >= 240) src_y = 239;
            
            uint16_t* src_row = &nesBuffer[src_y * 256];
            
            for (int x = 0; x < 256; x++) {
                uint16_t pixel16 = src_row[x];
                int r = ((pixel16 >> 11) & 0x1F) << 3;
                int g = ((pixel16 >> 5) & 0x3F) << 2;
                int b = (pixel16 & 0x1F) << 3;
                
                putpixel(target, x + dest_x, y, makecol(r, g, b));
            }
        }
    }
}

// DOS optimized scaling for other resolutions
void AllegroMainWindow::drawGameDOSOptimized(BITMAP* target)
{
    // Use the cached scaling system
    drawGameBuffered(target);
}
#endif

AllegroMainWindow::~AllegroMainWindow() 
{
    scalingCache.cleanup();
    if (lineStartOffsets) {
        delete[] lineStartOffsets;
        lineStartOffsets = NULL;
    }
    shutdown();
}


bool AllegroMainWindow::initialize() 
{
    if (!initializeAllegro()) {
        return false;
    }
    
    if (!initializeGraphics()) {
        return false;
    }
    
    // Setup available video modes AFTER graphics are initialized
    setupVideoModes();
    loadVideoConfig();
    
    // If no config exists or invalid mode, set a good default for both DOS and Linux
    if (numAvailableModes > 0) {
        // Look for 640x480 as the preferred default (good NES scaling on both platforms)
        int defaultMode = 0;
        for (int i = 0; i < numAvailableModes; i++) {
            if (availableModes[i].width == 640 && availableModes[i].height == 480) {
                defaultMode = i;
                break;
            }
        }
        
        // Only change mode if current mode is invalid or we don't have a saved config
        if (videoSettings.currentMode < 0 || videoSettings.currentMode >= numAvailableModes) {
            printf("Setting default video mode to %s\n", availableModes[defaultMode].description);
            setVideoMode(defaultMode);
        }
    }
    
    if (!initializeInput()) {
        return false;
    }
    
    setupMenu();
    loadControlConfig();
    
    setStatusMessage("Super Mario Bros Virtualizer - Press ESC for menu");
    
    return true;
}

void AllegroMainWindow::setupVideoModes()
{
    numAvailableModes = 0;
    
    // Define modes to test (in order of preference - larger modes first for better NES scaling)
    struct { int w, h; const char* desc; } testModes[] = {
        {640, 480, "640x480 (VGA)"},      // Best DOS default - 2.5x NES scaling
        {800, 600, "800x600 (SVGA)"},     // Good scaling if available
        {1024, 768, "1024x768 (XGA)"},    // Excellent scaling if available
        {512, 384, "512x384 (Half VGA)"}, // Decent scaling
        {400, 300, "400x300 (Quarter SVGA)"}, // Moderate scaling
        {320, 240, "320x240 (QVGA)"},     // 1:1 NES scaling
        {320, 200, "320x200 (Mode 13h)"}, // Last resort - cramped
        {0, 0, NULL}
    };
    
    printf("Testing available video modes...\n");
    
    // Remember current mode
    int currentWidth = SCREEN_W;
    int currentHeight = SCREEN_H;
    
    #ifndef __DJGPP__
    // On Linux, determine graphics mode type based on fullscreen state
    int gfxMode = isFullscreen ? GFX_AUTODETECT : GFX_AUTODETECT_WINDOWED;
    printf("Testing %s modes...\n", isFullscreen ? "fullscreen" : "windowed");
    #else
    // On DOS, always use autodetect (fullscreen only)
    int gfxMode = GFX_AUTODETECT;
    printf("Testing DOS fullscreen modes...\n");
    #endif
    
    for (int i = 0; testModes[i].w != 0 && numAvailableModes < 10; i++) {
        printf("Testing %s...", testModes[i].desc);
        
        // Test if this mode is available with the appropriate graphics mode
        if (set_gfx_mode(gfxMode, testModes[i].w, testModes[i].h, 0, 0) == 0) {
            printf(" OK\n");
            
            availableModes[numAvailableModes].width = testModes[i].w;
            availableModes[numAvailableModes].height = testModes[i].h;
            strncpy(availableModes[numAvailableModes].description, testModes[i].desc, 31);
            availableModes[numAvailableModes].description[31] = '\0';
            availableModes[numAvailableModes].available = true;
            
            // Check if this is the current mode
            if (testModes[i].w == currentWidth && testModes[i].h == currentHeight) {
                videoSettings.currentMode = numAvailableModes;
            }
            
            numAvailableModes++;
        } else {
            printf(" FAILED: %s\n", allegro_error);
        }
    }
    
    printf("Found %d available video modes\n", numAvailableModes);
    
    // Restore original mode with appropriate graphics type
    #ifndef __DJGPP__
    set_gfx_mode(gfxMode, currentWidth, currentHeight, 0, 0);
    #else
    set_gfx_mode(GFX_AUTODETECT, currentWidth, currentHeight, 0, 0);
    #endif
}
// Complete DOS audio implementation with status bar removal

bool AllegroMainWindow::initializeAllegro()
{
    printf("Initializing Allegro...\n");
    
    if (allegro_init() != 0) {
        printf("Failed to initialize Allegro\n");
        return false;
    }
    printf("Allegro initialized successfully\n");
    
    if (install_keyboard() < 0) {
        printf("Failed to install keyboard\n");
        return false;
    }
    printf("Keyboard installed\n");
    
    if (install_timer() < 0) {
        printf("Failed to install timer\n");
        return false;
    }
    printf("Timer installed\n");
    
    if (install_joystick(JOY_TYPE_AUTODETECT) == 0) {
        printf("Joystick installed\n");
    } else {
        printf("No joystick detected\n");
    }
    
    dosAudioInitialized = false;
    
    if (Configuration::getAudioEnabled() && !showingMenu) {
        printf("Attempting to initialize audio...\n");
        
        if (install_sound(DIGI_AUTODETECT, MIDI_NONE, NULL) == 0) {
            printf("Audio initialized successfully\n");
            dosAudioInitialized = true;
            
            set_volume(255, 255);
            
            int freq = Configuration::getAudioFrequency();
            int samples = freq / Configuration::getFrameRate();
            
            audiostream = play_audio_stream(samples, 8, FALSE, freq, 200, 128);
            if (!audiostream) {
                printf("Failed to create audio stream\n");
                dosAudioInitialized = false;
            } else {
                printf("Audio stream started: %d Hz\n", freq);
            }
            
        } else {
            printf("Audio initialization failed: %s\n", allegro_error);
            dosAudioInitialized = false;
        }
    } else {
        printf("Audio disabled in configuration\n");
    }
    
    LOCK_VARIABLE(timer_counter);
    LOCK_FUNCTION(timer_callback);
    
    if (install_int_ex(timer_callback, BPS_TO_TIMER(Configuration::getFrameRate())) < 0) {
        printf("Failed to install timer interrupt\n");
        return false;
    }
    
    printf("Timer interrupt installed at %d FPS\n", Configuration::getFrameRate());
    
    return true;
}
bool AllegroMainWindow::isValidJoystickButton(int joyIndex, int buttonNum)
{
    if (joyIndex < 0 || joyIndex >= num_joysticks) return false;
    if (buttonNum < 0 || buttonNum >= joy[joyIndex].num_buttons) return false;
    return true;
}

bool AllegroMainWindow::getJoystickDirection(int joyIndex, int* x_dir, int* y_dir)
{
    *x_dir = 0;
    *y_dir = 0;
    
    if (joyIndex >= num_joysticks) return false;
    if (joy[joyIndex].num_sticks == 0) return false;
    if (joy[joyIndex].stick[0].num_axis < 2) return false;
    
    // Poll joystick and check for errors
    if (poll_joystick() != 0) {
        return false;
    }
    
    int x = joy[joyIndex].stick[0].axis[0].pos;
    int y = joy[joyIndex].stick[0].axis[1].pos;
    
    // Use smaller deadzone for better responsiveness
    const int deadzone = 32;  // Reduced from 96
    
    if (x < -deadzone) *x_dir = -1;
    else if (x > deadzone) *x_dir = 1;
    
    if (y < -deadzone) *y_dir = -1;
    else if (y > deadzone) *y_dir = 1;
    
    return true;
}

void AllegroMainWindow::debugJoystickInfo(int joyIndex)
{
    if (joyIndex >= num_joysticks) {
        printf("Joystick %d not available\n", joyIndex);
        return;
    }
    
    printf("=== Joystick %d Debug Info ===\n", joyIndex);
    printf("Number of sticks: %d\n", joy[joyIndex].num_sticks);
    printf("Number of buttons: %d\n", joy[joyIndex].num_buttons);
    
    for (int stick = 0; stick < joy[joyIndex].num_sticks; stick++) {
        printf("Stick %d:\n", stick);
        printf("  Number of axes: %d\n", joy[joyIndex].stick[stick].num_axis);
        
        for (int axis = 0; axis < joy[joyIndex].stick[stick].num_axis; axis++) {
            printf("  Axis %d:\n", axis);
            printf("    Position: %d\n", joy[joyIndex].stick[stick].axis[axis].pos);
            printf("    Digital 1: %d\n", joy[joyIndex].stick[stick].axis[axis].d1);
            printf("    Digital 2: %d\n", joy[joyIndex].stick[stick].axis[axis].d2);
        }
    }
    
    printf("Button states: ");
    for (int btn = 0; btn < joy[joyIndex].num_buttons && btn < 16; btn++) {
        printf("%d", joy[joyIndex].button[btn].b ? 1 : 0);
    }
    printf("\n");
    printf("===============================\n");
}

bool AllegroMainWindow::initializeGraphics()
{
    set_color_depth(16); // 16-bit color for better DOS compatibility
    
    #ifdef __DJGPP__
    // DOS: Use classic VGA Mode 13h (320x200) - always fullscreen
    if (set_gfx_mode(GFX_AUTODETECT, 320, 200, 0, 0) != 0) {
        // Fallback to other DOS-compatible modes
        if (set_gfx_mode(GFX_AUTODETECT, 320, 240, 0, 0) != 0) {
            if (set_gfx_mode(GFX_AUTODETECT, 640, 480, 0, 0) != 0) {
                printf("Failed to set any DOS graphics mode: %s\n", allegro_error);
                return false;
            }
        }
    }
    
    printf("DOS Graphics mode: %dx%d (always fullscreen)\n", SCREEN_W, SCREEN_H);
    
    #else
    // Linux: Start in windowed mode, support fullscreen toggle
    isFullscreen = false;
    
    if (set_gfx_mode(GFX_AUTODETECT_WINDOWED, 640, 480, 0, 0) != 0) {
        if (set_gfx_mode(GFX_AUTODETECT_WINDOWED, 512, 384, 0, 0) != 0) {
            if (set_gfx_mode(GFX_AUTODETECT_WINDOWED, 320, 240, 0, 0) != 0) {
                printf("Failed to set windowed graphics mode: %s\n", allegro_error);
                return false;
            }
        }
    }
    
    windowedWidth = SCREEN_W;
    windowedHeight = SCREEN_H;
    
    printf("Linux Graphics mode: %dx%d (windowed)\n", SCREEN_W, SCREEN_H);
    #endif
    
    // Calculate optimal scaling info
    int scale_x = SCREEN_W / 256;
    int scale_y = SCREEN_H / 240;
    int optimal_scale = (scale_x < scale_y) ? scale_x : scale_y;
    if (optimal_scale < 1) optimal_scale = 1;
    
    printf("NES scaling: %dx (NES 256x240 -> %dx%d)\n", 
           optimal_scale, 256 * optimal_scale, 240 * optimal_scale);
    
    return createBuffers();
}

bool AllegroMainWindow::createBuffers()
{
    // Destroy existing buffers
    if (game_buffer) {
        destroy_bitmap(game_buffer);
        game_buffer = NULL;
    }
    
    if (back_buffer) {
        destroy_bitmap(back_buffer);
        back_buffer = NULL;
    }
    
    if (screenBuffer16) {
        delete[] screenBuffer16;
        screenBuffer16 = NULL;
    }
    
    // Create game buffer matching NES resolution
    game_buffer = create_bitmap(RENDER_WIDTH, RENDER_HEIGHT);
    if (!game_buffer) {
        printf("Failed to create game buffer\n");
        return false;
    }
    
    // Create back buffer for double buffering (same size as screen)
    back_buffer = create_bitmap(SCREEN_W, SCREEN_H);
    if (!back_buffer) {
        printf("Failed to create back buffer\n");
        return false;
    }
    
    // Allocate 16-bit screen buffer for direct rendering
    screenBuffer16 = new uint16_t[SCREEN_W * SCREEN_H];
    if (!screenBuffer16) {
        printf("Failed to allocate 16-bit screen buffer\n");
        useDirectRendering = false;
    } else {
        printf("16-bit screen buffer allocated: %dx%d\n", SCREEN_W, SCREEN_H);
    }
    
    clear_to_color(screen, makecol(0, 0, 0));
    return true;
}

AllegroMainWindow::ScalingCache::ScalingCache() 
    : scaledBuffer(NULL), sourceToDestX(NULL), sourceToDestY(NULL), 
      scaleFactor(0), destWidth(0), destHeight(0), 
      destOffsetX(0), destOffsetY(0), isValid(false) 
{
    // Constructor body is empty - initialization is done in member initializer list
}

// ScalingCache destructor implementation
AllegroMainWindow::ScalingCache::~ScalingCache() 
{
    cleanup();
}

// ScalingCache cleanup implementation
void AllegroMainWindow::ScalingCache::cleanup() 
{
    if (scaledBuffer) { 
        delete[] scaledBuffer; 
        scaledBuffer = NULL; 
    }
    if (sourceToDestX) { 
        delete[] sourceToDestX; 
        sourceToDestX = NULL; 
    }
    if (sourceToDestY) { 
        delete[] sourceToDestY; 
        sourceToDestY = NULL; 
    }
    isValid = false;
}

#ifndef __DJGPP__
// Fullscreen toggle function (Linux only):
bool AllegroMainWindow::toggleFullscreen()
{
    printf("Toggling fullscreen mode...\n");
    
    bool wasFullscreen = isFullscreen;
    int newWidth, newHeight, newMode;
    
    if (isFullscreen) {
        // Switch to windowed
        newWidth = windowedWidth;
        newHeight = windowedHeight;
        newMode = GFX_AUTODETECT_WINDOWED;
        printf("Switching to windowed: %dx%d\n", newWidth, newHeight);
    } else {
        // Switch to fullscreen - try to get desktop resolution
        newWidth = 1024;  // Default fullscreen resolution
        newHeight = 768;
        newMode = GFX_AUTODETECT;
        printf("Switching to fullscreen: %dx%d\n", newWidth, newHeight);
    }
    
    // Attempt to set new graphics mode
    if (set_gfx_mode(newMode, newWidth, newHeight, 0, 0) != 0) {
        printf("Failed to switch graphics mode: %s\n", allegro_error);
        
        // Try to restore previous mode
        int restoreMode = wasFullscreen ? GFX_AUTODETECT : GFX_AUTODETECT_WINDOWED;
        int restoreWidth = wasFullscreen ? 1024 : windowedWidth;
        int restoreHeight = wasFullscreen ? 768 : windowedHeight;
        
        if (set_gfx_mode(restoreMode, restoreWidth, restoreHeight, 0, 0) != 0) {
            printf("Critical: Failed to restore graphics mode!\n");
            return false;
        }
        
        setStatusMessage("Failed to toggle fullscreen");
        return false;
    }
    
    // Update state
    isFullscreen = !isFullscreen;
    
    // Store windowed dimensions when switching to fullscreen
    if (isFullscreen) {
        windowedWidth = wasFullscreen ? windowedWidth : SCREEN_W;
        windowedHeight = wasFullscreen ? windowedHeight : SCREEN_H;
    }
    
    // Recreate buffers for new screen size
    if (!createBuffers()) {
        printf("Failed to recreate buffers after mode change\n");
        return false;
    }
    
    // Clear screen
    clear_to_color(screen, makecol(0, 0, 0));
    
    printf("Successfully switched to %s mode: %dx%d\n", 
           isFullscreen ? "fullscreen" : "windowed", SCREEN_W, SCREEN_H);
    
    setStatusMessage(isFullscreen ? "Switched to fullscreen" : "Switched to windowed");
    
    return true;
}
#endif

bool AllegroMainWindow::initializeInput()
{
    // Clear keyboard buffer
    clear_keybuf();
    
    // Initialize joystick detection with fallback drivers
    printf("Initializing joystick...\n");
    
    if (install_joystick(JOY_TYPE_AUTODETECT) != 0) {
        printf("Autodetect failed: %s\n", allegro_error);
        printf("Trying specific drivers...\n");
        
        #ifdef __DJGPP__
        // Try DOS-specific drivers in order of compatibility
        if (install_joystick(JOY_TYPE_STANDARD) != 0 &&
            install_joystick(JOY_TYPE_2PADS) != 0 &&
            install_joystick(JOY_TYPE_4BUTTON) != 0 &&
            install_joystick(JOY_TYPE_6BUTTON) != 0) {
            printf("All joystick drivers failed\n");
            printf("Continuing without joystick support\n");
            return true; // Don't fail initialization
        } else {
            printf("Specific driver succeeded\n");
        }
        #else
        printf("No joystick support on this platform\n");
        return true;
        #endif
    } else {
        printf("Autodetect succeeded\n");
    }
    
    printf("Number of joysticks detected: %d\n", num_joysticks);
    
    if (num_joysticks > 0) {
        // Show joystick capabilities
        for (int i = 0; i < num_joysticks && i < 2; i++) {
            printf("Joystick %d: %d sticks, %d buttons\n", 
                   i, joy[i].num_sticks, joy[i].num_buttons);
        }
        
        // Calibrate all joysticks
        printf("Calibrating joysticks...\n");
        for (int i = 0; i < num_joysticks; i++) {
            if (calibrate_joystick(i) != 0) {
                printf("Warning: Calibration failed for joystick %d\n", i);
            }
        }
        
        // Test initial poll
        if (poll_joystick() != 0) {
            printf("Warning: Initial joystick poll failed\n");
        } else {
            printf("Joystick poll test successful\n");
        }
    }

    if (zapperEnabled) {
        printf("Initializing mouse for NES Zapper...\n");
        if (install_mouse() >= 0) {
            printf("Mouse initialized successfully\n");
            
            // Show mouse cursor
            show_mouse(screen);
            
            // Set initial mouse position to center
            position_mouse(SCREEN_W / 2, SCREEN_H / 2);
            
            printf("Mouse cursor enabled for NES Zapper\n");
        } else {
            printf("Failed to initialize mouse: %s\n", allegro_error);
            printf("Zapper will not work without mouse support\n");
            zapperEnabled = false;
        }
    }
    
    return true;
}

const char* AllegroMainWindow::getKeyName(int scancode)
{
    // Use a static array to avoid string length issues
    static char keyNameBuffer[16];
    
    switch (scancode) {
        case KEY_A: return "A";
        case KEY_B: return "B";
        case KEY_C: return "C";
        case KEY_D: return "D";
        case KEY_E: return "E";
        case KEY_F: return "F";
        case KEY_G: return "G";
        case KEY_H: return "H";
        case KEY_I: return "I";
        case KEY_J: return "J";
        case KEY_K: return "K";
        case KEY_L: return "L";
        case KEY_M: return "M";
        case KEY_N: return "N";
        case KEY_O: return "O";
        case KEY_P: return "P";
        case KEY_Q: return "Q";
        case KEY_R: return "R";
        case KEY_S: return "S";
        case KEY_T: return "T";
        case KEY_U: return "U";
        case KEY_V: return "V";
        case KEY_W: return "W";
        case KEY_X: return "X";
        case KEY_Y: return "Y";
        case KEY_Z: return "Z";
        case KEY_0: return "0";
        case KEY_1: return "1";
        case KEY_2: return "2";
        case KEY_3: return "3";
        case KEY_4: return "4";
        case KEY_5: return "5";
        case KEY_6: return "6";
        case KEY_7: return "7";
        case KEY_8: return "8";
        case KEY_9: return "9";
        case KEY_UP: return "UP";
        case KEY_DOWN: return "DOWN";
        case KEY_LEFT: return "LEFT";
        case KEY_RIGHT: return "RIGHT";
        case KEY_ENTER: return "ENTER";
        case KEY_SPACE: return "SPACE";
        case KEY_LSHIFT: return "LSHIFT";
        case KEY_RSHIFT: return "RSHIFT";
        case KEY_LCONTROL: return "LCTRL";
        case KEY_RCONTROL: return "RCTRL";
        case KEY_ALT: return "ALT";
        case KEY_TAB: return "TAB";
        case KEY_ESC: return "ESC";
        case KEY_BACKSPACE: return "BKSP";
        case KEY_INSERT: return "INS";
        case KEY_DEL: return "DEL";
        case KEY_HOME: return "HOME";
        case KEY_END: return "END";
        case KEY_PGUP: return "PGUP";
        case KEY_PGDN: return "PGDN";
        case KEY_F1: return "F1";
        case KEY_F2: return "F2";
        case KEY_F3: return "F3";
        case KEY_F4: return "F4";
        case KEY_F5: return "F5";
        case KEY_F6: return "F6";
        case KEY_F7: return "F7";
        case KEY_F8: return "F8";
        case KEY_F9: return "F9";
        case KEY_F10: return "F10";
        case KEY_F11: return "F11";
        case KEY_F12: return "F12";
        case KEY_MINUS: return "-";
        case KEY_EQUALS: return "=";
        case KEY_OPENBRACE: return "[";
        case KEY_CLOSEBRACE: return "]";
        case KEY_SEMICOLON: return ";";
        case KEY_QUOTE: return "'";
        case KEY_BACKSLASH: return "\\";
        case KEY_COMMA: return ",";
        case KEY_STOP: return ".";
        case KEY_SLASH: return "/";
        case KEY_TILDE: return "`";
        case KEY_ASTERISK: return "*";
        case KEY_PLUS_PAD: return "NUM+";
        case KEY_MINUS_PAD: return "NUM-";
        case KEY_SLASH_PAD: return "NUM/";
        case KEY_0_PAD: return "NUM0";
        case KEY_1_PAD: return "NUM1";
        case KEY_2_PAD: return "NUM2";
        case KEY_3_PAD: return "NUM3";
        case KEY_4_PAD: return "NUM4";
        case KEY_5_PAD: return "NUM5";
        case KEY_6_PAD: return "NUM6";
        case KEY_7_PAD: return "NUM7";
        case KEY_8_PAD: return "NUM8";
        case KEY_9_PAD: return "NUM9";
        case KEY_DEL_PAD: return "NUMDEL";
        case KEY_ENTER_PAD: return "NUMENTER";
        default: 
            sprintf(keyNameBuffer, "KEY%d", scancode);
            return keyNameBuffer;
    }
}


void AllegroMainWindow::setupDefaultControls()
{
    // Default keyboard controls - Player 1
    player1Keys.up = KEY_UP;
    player1Keys.down = KEY_DOWN;
    player1Keys.left = KEY_LEFT;
    player1Keys.right = KEY_RIGHT;
    player1Keys.button_a = KEY_X;
    player1Keys.button_b = KEY_Z;
    player1Keys.start = KEY_CLOSEBRACE;    // Changed to "]"
    player1Keys.select = KEY_OPENBRACE;    // Changed to "["
    
    // Default keyboard controls - Player 2
    player2Keys.up = KEY_W;
    player2Keys.down = KEY_S;
    player2Keys.left = KEY_A;
    player2Keys.right = KEY_D;
    player2Keys.button_a = KEY_G;
    player2Keys.button_b = KEY_F;
    player2Keys.start = KEY_P;
    player2Keys.select = KEY_O;
    
    // Default joystick controls
    player1Joy.button_a = 0;
    player1Joy.button_b = 1;
    player1Joy.start = 9;
    player1Joy.select = 8;
    player1Joy.use_stick = true;
    
    player2Joy.button_a = 0;
    player2Joy.button_b = 1;
    player2Joy.start = 9;
    player2Joy.select = 8;
    player2Joy.use_stick = true;
}

void AllegroMainWindow::setupMenu()
{
    menuCount = 0;
    
    strcpy(mainMenu[menuCount].text, "Resume Game");
    mainMenu[menuCount].id = 1;
    mainMenu[menuCount].enabled = true;
    menuCount++;
    
    strcpy(mainMenu[menuCount].text, "Reset Game");
    mainMenu[menuCount].id = 2;
    mainMenu[menuCount].enabled = true;
    menuCount++;
    
    strcpy(mainMenu[menuCount].text, "Video Options");  // NEW
    mainMenu[menuCount].id = 9;
    mainMenu[menuCount].enabled = true;
    menuCount++;
    
    strcpy(mainMenu[menuCount].text, "Player 1 Controls");
    mainMenu[menuCount].id = 3;
    mainMenu[menuCount].enabled = true;
    menuCount++;
    
    strcpy(mainMenu[menuCount].text, "Player 2 Controls");
    mainMenu[menuCount].id = 4;
    mainMenu[menuCount].enabled = true;
    menuCount++;
    
    strcpy(mainMenu[menuCount].text, "Test Joystick");
    mainMenu[menuCount].id = 8;
    mainMenu[menuCount].enabled = (num_joysticks > 0);
    menuCount++;
    
    strcpy(mainMenu[menuCount].text, "Help");
    mainMenu[menuCount].id = 5;
    mainMenu[menuCount].enabled = true;
    menuCount++;
    
    strcpy(mainMenu[menuCount].text, "About");
    mainMenu[menuCount].id = 6;
    mainMenu[menuCount].enabled = true;
    menuCount++;
    
    strcpy(mainMenu[menuCount].text, "Quit");
    mainMenu[menuCount].id = 7;
    mainMenu[menuCount].enabled = true;
    menuCount++;
}

void AllegroMainWindow::run(const char* romFilename)
{

    printf("\n\nWelcome to WarpNES\n\n");

    gamePaused = false;
    showingMenu = false;
    currentDialog = DIALOG_NONE;

    SMBEmulator engine;
    smbEngine = &engine;

    printf("Loading ROM: %s\n", romFilename);
    if (!engine.loadROM(romFilename)) {
        printf("Failed to load ROM file: %s\n", romFilename);
        setStatusMessage("ROM loading failed");
        return;
    }

    printf("ROM loaded successfully\n");

    // Enable Zapper if requested
    if (zapperEnabled) {
        engine.enableZapper(true);
        setStatusMessage("NES Zapper enabled - Use mouse to aim, click to fire");
    }

    uint8_t mapper = engine.getMapper();
    printf("Detected mapper: %d\n", mapper);

    const char* updateMethod = "Fast Frame-Based";
    if (mapper == 2) updateMethod = "Cycle-Accurate (UxROM CHR-RAM)";
    else if (mapper == 4) updateMethod = "Cycle-Accurate (MMC3 IRQ)";
    else if (mapper == 66) updateMethod = "Cycle-Accurate (GxROM banking)";

    printf("Update method: %s\n", updateMethod);

    switch (mapper) {
        case 0:  printf("NROM: No banking, optimal performance\n"); break;
        case 1:  printf("MMC1: Banking but no mid-frame effects\n"); break;
        case 2:  printf("UxROM: CHR-RAM can be updated mid-frame\n"); break;
        case 3:  printf("CNROM: Simple CHR banking\n"); break;
        case 4:  printf("MMC3: IRQ timing requires cycle accuracy\n"); break;
        case 66: printf("GxROM: CHR banking can happen mid-frame\n"); break;
        default: printf("Unknown mapper: Using cycle-accurate for safety\n"); break;
    }

    engine.reset();

    gameRunning = true;
    setStatusMessage("Game started - Press ESC for menu");

    printf("Starting main game loop with %s...\n", updateMethod);
    printf("Resolution: %dx%d\n", SCREEN_W, SCREEN_H);
    printf("Audio: %s\n", dosAudioInitialized ? "Enabled" : "Disabled");

    static int frameCount = 0;
    static clock_t lastPerfCheck = clock();

    const double targetFrameMs = 1000.0 / Configuration::getFrameRate();

    #ifndef __DJGPP__
    auto lastFrameTime = std::chrono::high_resolution_clock::now();
    #endif

    while (gameRunning) {
        handleInput();

        if (!gamePaused && !showingMenu && currentDialog == DIALOG_NONE) {
            engine.update();
#ifndef __DJGPP__             
            frameCount++;
            if (frameCount % 300 == 0) {
                clock_t now = clock();
                double elapsed = ((double)(now - lastPerfCheck)) / CLOCKS_PER_SEC;
                double fps = 300.0 / elapsed;
                lastPerfCheck = now;
            }
#endif

            if (dosAudioInitialized && Configuration::getAudioEnabled() && audiostream) {
                void* audiobuf = get_audio_stream_buffer(audiostream);
                if (audiobuf) {
                    int samplesNeeded = Configuration::getAudioFrequency() / Configuration::getFrameRate();
                    if (samplesNeeded > 1024) samplesNeeded = 1024;
                    engine.audioCallback((uint8_t*)audiobuf, samplesNeeded);
                    free_audio_stream_buffer(audiostream);
                }
            }

            //engine.render16(renderBuffer);
            currentFrameBuffer = renderBuffer;
        }

        updateAndDraw();

        #ifdef __DJGPP__
        //vsync();
        #else
        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = now - lastFrameTime;
        if (elapsed.count() < targetFrameMs) {
            rest(static_cast<int>(targetFrameMs - elapsed.count()));
        }
        lastFrameTime = std::chrono::high_resolution_clock::now();
        #endif
    }

    if (dosAudioInitialized) {
        printf("Cleaning up audio...\n");
    }

    printf("Game loop ended normally\n");
}



void AllegroMainWindow::showJoystickStatus()
{
    if (num_joysticks == 0) {
        setStatusMessage("No joysticks detected");
        return;
    }
    
    static char statusBuffer[256];
    statusBuffer[0] = '\0';
    
    if (poll_joystick() != 0) {
        setStatusMessage("Joystick poll failed");
        return;
    }
    
    for (int i = 0; i < num_joysticks && i < 2; i++) {
        char joyInfo[128];
        
        if (joy[i].num_sticks > 0 && joy[i].stick[0].num_axis >= 2) {
            int x = joy[i].stick[0].axis[0].pos;
            int y = joy[i].stick[0].axis[1].pos;
            
            // Show pressed buttons
            char buttonStr[32] = "";
            for (int btn = 0; btn < joy[i].num_buttons && btn < 8; btn++) {
                if (joy[i].button[btn].b) {
                    char temp[8];
                    sprintf(temp, "%d ", btn);
                    strcat(buttonStr, temp);
                }
            }
            
            sprintf(joyInfo, "J%d:X%d Y%d B:%s| ", i, x, y, buttonStr);
        } else {
            sprintf(joyInfo, "J%d:NoStick| ", i);
        }
        
        strcat(statusBuffer, joyInfo);
    }
    
    setStatusMessage(statusBuffer);
}


void AllegroMainWindow::handleInput()
{
    // Poll input devices
    poll_keyboard();
    if (num_joysticks > 0) {
        poll_joystick();
    }

    if (zapperEnabled) {
        poll_mouse();
        mouseX = mouse_x;
        mouseY = mouse_y;
        mousePressed = (mouse_b & 1) != 0;  // Left mouse button
        
        // Convert screen coordinates to NES coordinates
        // This needs to account for scaling and centering
        int nesMouseX, nesMouseY;
        screenToNESCoordinates(mouseX, mouseY, &nesMouseX, &nesMouseY);
        
        // Update Zapper state in the engine
        if (smbEngine) {
            smbEngine->updateZapperInput(nesMouseX, nesMouseY, mousePressed);
        }
    }
    
    #ifndef __DJGPP__
    // Handle F11 key for fullscreen toggle (Linux only)
    static bool f11Pressed = false;
    bool f11CurrentlyPressed = (key[KEY_F11] != 0);
    
    if (f11CurrentlyPressed && !f11Pressed) {
        if (!toggleFullscreen()) {
            printf("Fullscreen toggle failed\n");
        }
    }
    f11Pressed = f11CurrentlyPressed;
    #endif
    
    // Handle ESC key globally with proper state tracking
    static bool escPressed = false;
    bool escCurrentlyPressed = (key[KEY_ESC] != 0);
    
    // ESC key was just pressed (transition from not pressed to pressed)
    if (escCurrentlyPressed && !escPressed) {
        if (currentDialog != DIALOG_NONE) {
            // Close dialog
            currentDialog = DIALOG_NONE;
            if (!showingMenu && gamePaused==true) {
                gamePaused = false;
                setStatusMessage("Game Resumed");
            }
        } else if (showingMenu && gamePaused==true) {
            // Close menu
            showingMenu = false;
            gamePaused = false;
            setStatusMessage("Game Resumed");
        } else {
            // Open menu
            showingMenu = true;
            gamePaused = true;
            setStatusMessage("Game Paused - Menu Active");
        }
    }
    escPressed = escCurrentlyPressed;
    
    // Handle other input based on current state
    if (currentDialog != DIALOG_NONE) {
        handleDialogInputNoEsc();
    } else if (showingMenu) {
        handleMenuInputNoEsc();
    } else {
        handleGameInputNoEsc();
    }
    
    updateStatusMessage();
}

void AllegroMainWindow::screenToNESCoordinates(int screenX, int screenY, int* nesX, int* nesY)
{
    // Calculate NES display area on screen
    int scale_x = SCREEN_W / 256;
    int scale_y = SCREEN_H / 240;
    int scale = (scale_x < scale_y) ? scale_x : scale_y;
    if (scale < 1) scale = 1;
    
    int dest_w = 256 * scale;
    int dest_h = 240 * scale;
    int dest_x = (SCREEN_W - dest_w) / 2;
    int dest_y = (SCREEN_H - dest_h) / 2;
    
    // Convert screen coordinates to NES coordinates
    int relativeX = screenX - dest_x;
    int relativeY = screenY - dest_y;
    
    *nesX = relativeX / scale;
    *nesY = relativeY / scale;
    
    // Clamp to NES screen bounds
    if (*nesX < 0) *nesX = 0;
    if (*nesX >= 256) *nesX = 255;
    if (*nesY < 0) *nesY = 0;
    if (*nesY >= 240) *nesY = 239;
}

void AllegroMainWindow::handleDialogInputNoEsc()
{
    if (currentDialog == DIALOG_VIDEO_OPTIONS) {
        handleVideoOptionsInput();
        return;
    }
    
    if (isCapturingInput) {
        // Handle joystick button capture
        if (num_joysticks > 0 && (currentCaptureType >= CAPTURE_JOY_A && currentCaptureType <= CAPTURE_JOY_SELECT)) {
            if (poll_joystick() == 0) {
                int joyIndex = (currentConfigPlayer == PLAYER_1) ? 0 : (num_joysticks > 1 ? 1 : 0);
                
                // Simple button detection - check all buttons
                for (int i = 0; i < joy[joyIndex].num_buttons && i < 16; i++) {
                    if (joy[joyIndex].button[i].b) {
                        // Button pressed - assign it
                        assignCapturedJoyButton(i);
                        return;
                    }
                }
            }
        }
        
        // Handle keyboard capture
        if (currentCaptureType >= CAPTURE_KEY_UP && currentCaptureType <= CAPTURE_KEY_SELECT) {
            if (keypressed()) {
                int k = readkey();
                int scancode = k >> 8;
                
                if (scancode != KEY_ESC) {
                    assignCapturedKey(scancode);
                }
                // ESC is handled globally, so don't handle it here
            }
        }
        return;
    }
    
    // Handle menu navigation when not capturing
    if (currentDialog == DIALOG_CONTROLS_P1 || currentDialog == DIALOG_CONTROLS_P2) {
        if (keypressed()) {
            int k = readkey();
            int scancode = k >> 8;
            char ascii = k & 0xFF;
            
            // Convert to lowercase for easier handling
            if (ascii >= 'A' && ascii <= 'Z') {
                ascii = ascii - 'A' + 'a';
            }
            
            // Handle number keys for keyboard configuration
            if (ascii >= '1' && ascii <= '8') {
                switch (ascii) {
                    case '1': startKeyCapture(CAPTURE_KEY_UP, "Press key for UP"); break;
                    case '2': startKeyCapture(CAPTURE_KEY_DOWN, "Press key for DOWN"); break;
                    case '3': startKeyCapture(CAPTURE_KEY_LEFT, "Press key for LEFT"); break;
                    case '4': startKeyCapture(CAPTURE_KEY_RIGHT, "Press key for RIGHT"); break;
                    case '5': startKeyCapture(CAPTURE_KEY_A, "Press key for A BUTTON"); break;
                    case '6': startKeyCapture(CAPTURE_KEY_B, "Press key for B BUTTON"); break;
                    case '7': startKeyCapture(CAPTURE_KEY_START, "Press key for START"); break;
                    case '8': startKeyCapture(CAPTURE_KEY_SELECT, "Press key for SELECT"); break;
                }
                return;
            }
            
            // Handle letter keys for joystick configuration
            switch (ascii) {
                case 'a':
                    if (num_joysticks > 0) {
                        startKeyCapture(CAPTURE_JOY_A, "Press joystick button for A");
                    } else {
                        setStatusMessage("No joystick detected");
                    }
                    break;
                    
                case 'b':
                    if (num_joysticks > 0) {
                        startKeyCapture(CAPTURE_JOY_B, "Press joystick button for B");
                    } else {
                        setStatusMessage("No joystick detected");
                    }
                    break;
                    
                case 's':
                    if (num_joysticks > 0) {
                        startKeyCapture(CAPTURE_JOY_START, "Press joystick button for START");
                    } else {
                        setStatusMessage("No joystick detected");
                    }
                    break;
                    
                case 'e':
                    if (num_joysticks > 0) {
                        startKeyCapture(CAPTURE_JOY_SELECT, "Press joystick button for SELECT");
                    } else {
                        setStatusMessage("No joystick detected");
                    }
                    break;
                    
                case 'j':
                    if (num_joysticks > 0) {
                        if (currentDialog == DIALOG_CONTROLS_P1) {
                            player1Joy.use_stick = !player1Joy.use_stick;
                            setStatusMessage(player1Joy.use_stick ? "P1 using analog stick" : "P1 using digital pad");
                        } else {
                            player2Joy.use_stick = !player2Joy.use_stick;
                            setStatusMessage(player2Joy.use_stick ? "P2 using analog stick" : "P2 using digital pad");
                        }
                        saveControlConfig();
                    } else {
                        setStatusMessage("No joystick detected");
                    }
                    break;
                    
                case 't':
                    if (num_joysticks > 0) {
                        // Simple joystick test
                        showJoystickStatus();
                    } else {
                        setStatusMessage("No joystick detected");
                    }
                    break;
                    
                case 'r':
                    if (currentDialog == DIALOG_CONTROLS_P1) {
                        resetPlayer1ControlsToDefault();
                        setStatusMessage("Player 1 controls reset to defaults");
                    } else {
                        resetPlayer2ControlsToDefault();
                        setStatusMessage("Player 2 controls reset to defaults");
                    }
                    saveControlConfig();
                    break;
            }
        }
    }
}
void AllegroMainWindow::resetPlayer1ControlsToDefault()
{
    player1Keys.up = KEY_UP;
    player1Keys.down = KEY_DOWN;
    player1Keys.left = KEY_LEFT;
    player1Keys.right = KEY_RIGHT;
    player1Keys.button_a = KEY_X;
    player1Keys.button_b = KEY_Z;
    player1Keys.start = KEY_CLOSEBRACE;    // "]"
    player1Keys.select = KEY_OPENBRACE;    // "["
    
    player1Joy.button_a = 0;
    player1Joy.button_b = 1;
    player1Joy.start = 9;
    player1Joy.select = 8;
    player1Joy.use_stick = true;
}

void AllegroMainWindow::testCurrentJoystick()
{
    if (num_joysticks == 0) return;
    
    int joyIndex = (currentConfigPlayer == PLAYER_1) ? 0 : (num_joysticks > 1 ? 1 : 0);
    poll_joystick();
    
    char testMsg[128];
    if (joy[joyIndex].num_sticks > 0) {
        int x = joy[joyIndex].stick[0].axis[0].pos;
        int y = joy[joyIndex].stick[0].axis[1].pos;
        sprintf(testMsg, "Joy %d: X=%d Y=%d Btns:", joyIndex, x, y);
        
        char btnStr[32] = "";
        for (int i = 0; i < joy[joyIndex].num_buttons && i < 8; i++) {
            if (joy[joyIndex].button[i].b) {
                char temp[8];
                sprintf(temp, "%d ", i);
                strcat(btnStr, temp);
            }
        }
        strcat(testMsg, btnStr);
    } else {
        sprintf(testMsg, "Joystick %d: No sticks detected", joyIndex);
    }
    
    setStatusMessage(testMsg);
}

void AllegroMainWindow::displayJoystickInfo()
{
    // This would show detailed joystick information in the status message
    // For a full implementation, you might want to create a separate info dialog
    if (num_joysticks == 0) {
        setStatusMessage("No joysticks detected");
        return;
    }
    
    char infoMsg[128];
    sprintf(infoMsg, "%d joystick(s): Joy0 %d btns", 
            num_joysticks, joy[0].num_buttons);
    
    if (num_joysticks > 1) {
        char temp[32];
        sprintf(temp, ", Joy1 %d btns", joy[1].num_buttons);
        strcat(infoMsg, temp);
    }
    
    setStatusMessage(infoMsg);
}

void AllegroMainWindow::resetPlayer2ControlsToDefault()
{
    player2Keys.up = KEY_W;
    player2Keys.down = KEY_S;
    player2Keys.left = KEY_A;
    player2Keys.right = KEY_D;
    player2Keys.button_a = KEY_G;
    player2Keys.button_b = KEY_F;
    player2Keys.start = KEY_P;
    player2Keys.select = KEY_O;
    
    player2Joy.button_a = 0;
    player2Joy.button_b = 1;
    player2Joy.start = 9;
    player2Joy.select = 8;
    player2Joy.use_stick = true;
}

void AllegroMainWindow::handleGameInputNoEsc()
{
    // Check for pause (only when menu is not showing)
    static bool pPressed = false;
    if (key[KEY_P] && key[KEY_LCONTROL] && !pPressed) {
        gamePaused = !gamePaused;
        setStatusMessage(gamePaused ? "Game Paused" : "Game Resumed");
        pPressed = true;
        return;
    }
    if (!key[KEY_P]) {
        pPressed = false;
    }
    
    // Check for reset (only when menu is not showing)
    static bool rPressed = false;
    if (key[KEY_R] && key[KEY_LCONTROL] && !rPressed) {
        if (smbEngine) {
            smbEngine->reset();
            setStatusMessage("Game Reset");
        }
        rPressed = true;
        return;
    }
    if (!key[KEY_R]) {
        rPressed = false;
    }
    
    // Check for audio mode toggle (M key) - use SMBEngine methods instead of getAPU()
    static bool mPressed = false;
    if (key[KEY_M] && key[KEY_LCONTROL] && !mPressed) {
        if (smbEngine) {
            smbEngine->toggleAudioMode();
            setStatusMessage(smbEngine->isUsingMIDIAudio() ? "MIDI Audio Mode" : "APU Audio Mode");
        }
        mPressed = true;
        return;
    }
    if (!key[KEY_M]) {
        mPressed = false;
    }
    
    // Check for audio debug (N key) - use SMBEngine methods instead of getAPU()
    static bool nPressed = false;
    if (key[KEY_N] && key[KEY_LCONTROL] && !nPressed) {
        if (smbEngine) {
            smbEngine->debugAudioChannels();
            setStatusMessage("Audio debug info printed to console");
        }
        nPressed = true;
        return;
    }
    if (!key[KEY_N]) {
        nPressed = false;
    }
    
    // Save/Load state handling (F5-F8 keys)
    static bool f5Pressed = false;
    static bool f6Pressed = false;
    static bool f7Pressed = false;
    static bool f8Pressed = false;

    // F5 - Save/Load State 1
    if (key[KEY_F5] && !f5Pressed) {
        if (smbEngine) {
            if (key[KEY_LSHIFT] || key[KEY_RSHIFT]) {
                // Shift+F5 - Load State 1
                if (smbEngine->loadState("save1")) {
                    setStatusMessage("State 1 loaded");
                } else {
                    setStatusMessage("Failed to load state 1");
                }
            } else {
                // F5 - Save State 1
                smbEngine->saveState("save1");
                setStatusMessage("State 1 saved");
            }
        }
        f5Pressed = true;
        return;
    }
    if (!key[KEY_F5]) {
        f5Pressed = false;
    }

    // F6 - Save/Load State 2
    if (key[KEY_F6] && !f6Pressed) {
        if (smbEngine) {
            if (key[KEY_LSHIFT] || key[KEY_RSHIFT]) {
                // Shift+F6 - Load State 2
                if (smbEngine->loadState("save2")) {
                    setStatusMessage("State 2 loaded");
                } else {
                    setStatusMessage("Failed to load state 2");
                }
            } else {
                // F6 - Save State 2
                smbEngine->saveState("save2");
                setStatusMessage("State 2 saved");
            }
        }
        f6Pressed = true;
        return;
    }
    if (!key[KEY_F6]) {
        f6Pressed = false;
    }

    // F7 - Save/Load State 3
    if (key[KEY_F7] && !f7Pressed) {
        if (smbEngine) {
            if (key[KEY_LSHIFT] || key[KEY_RSHIFT]) {
                // Shift+F7 - Load State 3
                if (smbEngine->loadState("save3")) {
                    setStatusMessage("State 3 loaded");
                } else {
                    setStatusMessage("Failed to load state 3");
                }
            } else {
                // F7 - Save State 3
                smbEngine->saveState("save3");
                setStatusMessage("State 3 saved");
            }
        }
        f7Pressed = true;
        return;
    }
    if (!key[KEY_F7]) {
        f7Pressed = false;
    }

    // F8 - Save/Load State 4
    if (key[KEY_F8] && !f8Pressed) {
        if (smbEngine) {
            if (key[KEY_LSHIFT] || key[KEY_RSHIFT]) {
                // Shift+F8 - Load State 4
                if (smbEngine->loadState("save4")) {
                    setStatusMessage("State 4 loaded");
                } else {
                    setStatusMessage("Failed to load state 4");
                }
            } else {
                // F8 - Save State 4
                smbEngine->saveState("save4");
                setStatusMessage("State 4 saved");
            }
        }
        f8Pressed = true;
        return;
    }
    if (!key[KEY_F8]) {
        f8Pressed = false;
    }
    
    // Process player input ONLY if game is not paused
    if (!gamePaused) {
        checkPlayerInput(PLAYER_1);
        checkPlayerInput(PLAYER_2);
    }
}
void AllegroMainWindow::handleMenuInput()
{
    // Check for key presses
    if (keypressed()) {
        int k = readkey();
        int scancode = k >> 8;
        
        switch (scancode) {
            case KEY_UP:
                menuUp();
                break;
            case KEY_DOWN:
                menuDown();
                break;
            case KEY_ENTER:
                menuSelect();
                break;
            case KEY_ESC:
                menuEscape();
                break;
        }
    }
}

void AllegroMainWindow::handleDialogInput()
{
    if (isCapturingInput) {
        // Wait for key press to capture
        if (keypressed()) {
            int k = readkey();
            int scancode = k >> 8;
            
            if (scancode != KEY_ESC) {
                // Assign the captured key based on current capture context
                // This would be expanded based on which control is being configured
                isCapturingInput = false;
                strcpy(currentCaptureKey, "");
                setStatusMessage("Key captured");
            } else {
                isCapturingInput = false;
                strcpy(currentCaptureKey, "");
                setStatusMessage("Capture cancelled");
            }
        }
        return;
    }
    
    if (keypressed()) {
        int k = readkey();
        int scancode = k >> 8;
        
        if (scancode == KEY_ESC) {
            currentDialog = DIALOG_NONE;
            // When closing dialog, resume game if menu was not originally showing
            if (!showingMenu) {
                gamePaused = false;
                setStatusMessage("Game Resumed");
            }
        }
    }
}

void AllegroMainWindow::handleGameInput()
{
    // Check for menu toggle
    static bool escPressed = false;
    if (key[KEY_ESC] && !escPressed) {
        showingMenu = true;
        gamePaused = true;  // Explicitly pause the game when menu opens
        setStatusMessage("Game Paused - Menu Active");
        escPressed = true;
        return;
    }
    if (!key[KEY_ESC]) {
        escPressed = false;
    }
    
    // Check for pause (only when menu is not showing)
    static bool pPressed = false;
    if (key[KEY_P] && !pPressed) {
        gamePaused = !gamePaused;
        setStatusMessage(gamePaused ? "Game Paused" : "Game Resumed");
        pPressed = true;
        return;
    }
    if (!key[KEY_P]) {
        pPressed = false;
    }
    
    // Check for reset (only when menu is not showing)
    static bool rPressed = false;
    if (key[KEY_R] && key[KEY_LCONTROL] && !rPressed) {
        if (smbEngine) {
            smbEngine->reset();
            setStatusMessage("Game Reset");
        }
        rPressed = true;
        return;
    }
    if (!key[KEY_R]) {
        rPressed = false;
    }
    
    // Process player input ONLY if game is not paused
    if (!gamePaused) {
        checkPlayerInput(PLAYER_1);
        checkPlayerInput(PLAYER_2);
    }
}

void AllegroMainWindow::checkPlayerInput(Player player)
{
   //int debugInput=1;

   if (!smbEngine) return;
   
   poll_keyboard();
   
   #ifndef __DJGPP__
   // Only poll joystick on non-DOS systems
   bool joystick_available = false;
   if (num_joysticks > 0) {
       if (poll_joystick() == 0) {
           joystick_available = true;
       }
   }
   #endif
   
   if (player == PLAYER_1) {
       Controller& controller = smbEngine->getController1();
       
       bool up = (key[player1Keys.up] != 0);
       bool down = (key[player1Keys.down] != 0);
       bool left = (key[player1Keys.left] != 0);
       bool right = (key[player1Keys.right] != 0);
       bool a = (key[player1Keys.button_a] != 0);
       bool b = (key[player1Keys.button_b] != 0);
       bool start = (key[player1Keys.start] != 0);
       bool select = (key[player1Keys.select] != 0);
       //if (debugInput && (up || down || left || right || a || b || start || select)) {
       //    printf("Key input: U:%d D:%d L:%d R:%d A:%d B:%d Start:%d Select:%d\n",
       //           up, down, left, right, a, b, start, select);
       //}
       
       #ifndef __DJGPP__
       // Joystick input only on non-DOS systems
       if (joystick_available && num_joysticks > 0) {
           const int joyIndex = 0;
           
           if (joyIndex < num_joysticks && 
               joy[joyIndex].num_sticks > 0 && 
               joy[joyIndex].stick[0].num_axis >= 2) {
               
               if (player1Joy.use_stick) {
                   int x_dir, y_dir;
                   if (getJoystickDirection(joyIndex, &x_dir, &y_dir)) {
                       if (x_dir < 0) left = true;
                       if (x_dir > 0) right = true;
                       if (y_dir < 0) up = true;
                       if (y_dir > 0) down = true;
                   }
               } else {
                   if (joy[joyIndex].stick[0].axis[0].d1) left = true;
                   if (joy[joyIndex].stick[0].axis[0].d2) right = true;
                   if (joy[joyIndex].stick[0].axis[1].d1) up = true;
                   if (joy[joyIndex].stick[0].axis[1].d2) down = true;
               }
               
               if (player1Joy.button_a >= 0 && 
                   player1Joy.button_a < joy[joyIndex].num_buttons &&
                   joy[joyIndex].button[player1Joy.button_a].b) {
                   a = true;
               }
               
               if (player1Joy.button_b >= 0 && 
                   player1Joy.button_b < joy[joyIndex].num_buttons &&
                   joy[joyIndex].button[player1Joy.button_b].b) {
                   b = true;
               }
               
               if (player1Joy.start >= 0 && 
                   player1Joy.start < joy[joyIndex].num_buttons &&
                   joy[joyIndex].button[player1Joy.start].b) {
                   start = true;
               }
               
               if (player1Joy.select >= 0 && 
                   player1Joy.select < joy[joyIndex].num_buttons &&
                   joy[joyIndex].button[player1Joy.select].b) {
                   select = true;
               }
           }
       }
       #endif
       
       controller.setButtonState(BUTTON_UP, up);
       controller.setButtonState(BUTTON_DOWN, down);
       controller.setButtonState(BUTTON_LEFT, left);
       controller.setButtonState(BUTTON_RIGHT, right);
       controller.setButtonState(BUTTON_A, a);
       controller.setButtonState(BUTTON_B, b);
       controller.setButtonState(BUTTON_START, start);
       controller.setButtonState(BUTTON_SELECT, select);
   }
   else if (player == PLAYER_2) {
       Controller& controller = smbEngine->getController2();
       
       bool up = (key[player2Keys.up] != 0);
       bool down = (key[player2Keys.down] != 0);
       bool left = (key[player2Keys.left] != 0);
       bool right = (key[player2Keys.right] != 0);
       bool a = (key[player2Keys.button_a] != 0);
       bool b = (key[player2Keys.button_b] != 0);
       bool start = (key[player2Keys.start] != 0);
       bool select = (key[player2Keys.select] != 0);
       
       #ifndef __DJGPP__
       // Joystick input for Player 2 only on non-DOS systems
       if (joystick_available) {
           int joyIndex = (num_joysticks > 1) ? 1 : 0;
           
           if (joyIndex < num_joysticks && 
               joy[joyIndex].num_sticks > 0 && 
               joy[joyIndex].stick[0].num_axis >= 2) {
               
               int button_offset = (joyIndex == 0) ? 4 : 0;
               
               if (player2Joy.use_stick) {
                   int x_dir, y_dir;
                   if (getJoystickDirection(joyIndex, &x_dir, &y_dir)) {
                       if (x_dir < 0) left = true;
                       if (x_dir > 0) right = true;
                       if (y_dir < 0) up = true;
                       if (y_dir > 0) down = true;
                   }
               } else {
                   if (joy[joyIndex].stick[0].axis[0].d1) left = true;
                   if (joy[joyIndex].stick[0].axis[0].d2) right = true;
                   if (joy[joyIndex].stick[0].axis[1].d1) up = true;
                   if (joy[joyIndex].stick[0].axis[1].d2) down = true;
               }
               
               int btn_a = player2Joy.button_a + button_offset;
               int btn_b = player2Joy.button_b + button_offset;
               int btn_start = player2Joy.start + button_offset;
               int btn_select = player2Joy.select + button_offset;
               
               if (btn_a >= 0 && btn_a < joy[joyIndex].num_buttons &&
                   joy[joyIndex].button[btn_a].b) {
                   a = true;
               }
               
               if (btn_b >= 0 && btn_b < joy[joyIndex].num_buttons &&
                   joy[joyIndex].button[btn_b].b) {
                   b = true;
               }
               
               if (btn_start >= 0 && btn_start < joy[joyIndex].num_buttons &&
                   joy[joyIndex].button[btn_start].b) {
                   start = true;
               }
               
               if (btn_select >= 0 && btn_select < joy[joyIndex].num_buttons &&
                   joy[joyIndex].button[btn_select].b) {
                   select = true;
               }
           }
       }
       #endif
       
       controller.setButtonState(BUTTON_UP, up);
       controller.setButtonState(BUTTON_DOWN, down);
       controller.setButtonState(BUTTON_LEFT, left);
       controller.setButtonState(BUTTON_RIGHT, right);
       controller.setButtonState(BUTTON_A, a);
       controller.setButtonState(BUTTON_B, b);
       controller.setButtonState(BUTTON_START, start);
       controller.setButtonState(BUTTON_SELECT, select);
   }
}

void AllegroMainWindow::saveControlConfig()
{
    FILE* f = fopen("controls.cfg", "w");  // "w" = write text, 8.3 filename
    if (f) {
        // Write version header
        fprintf(f, "# Super Mario Bros Control Configuration\n");
        fprintf(f, "VERSION=1\n\n");
        
        // Write Player 1 keyboard controls
        fprintf(f, "# Player 1 Keyboard Controls\n");
        fprintf(f, "P1_KEY_UP=%d\n", player1Keys.up);
        fprintf(f, "P1_KEY_DOWN=%d\n", player1Keys.down);
        fprintf(f, "P1_KEY_LEFT=%d\n", player1Keys.left);
        fprintf(f, "P1_KEY_RIGHT=%d\n", player1Keys.right);
        fprintf(f, "P1_KEY_A=%d\n", player1Keys.button_a);
        fprintf(f, "P1_KEY_B=%d\n", player1Keys.button_b);
        fprintf(f, "P1_KEY_START=%d\n", player1Keys.start);
        fprintf(f, "P1_KEY_SELECT=%d\n", player1Keys.select);
        fprintf(f, "\n");
        
        // Write Player 1 joystick controls
        fprintf(f, "# Player 1 Joystick Controls\n");
        fprintf(f, "P1_JOY_A=%d\n", player1Joy.button_a);
        fprintf(f, "P1_JOY_B=%d\n", player1Joy.button_b);
        fprintf(f, "P1_JOY_START=%d\n", player1Joy.start);
        fprintf(f, "P1_JOY_SELECT=%d\n", player1Joy.select);
        fprintf(f, "P1_JOY_USE_STICK=%d\n", player1Joy.use_stick ? 1 : 0);
        fprintf(f, "\n");
        
        // Write Player 2 keyboard controls
        fprintf(f, "# Player 2 Keyboard Controls\n");
        fprintf(f, "P2_KEY_UP=%d\n", player2Keys.up);
        fprintf(f, "P2_KEY_DOWN=%d\n", player2Keys.down);
        fprintf(f, "P2_KEY_LEFT=%d\n", player2Keys.left);
        fprintf(f, "P2_KEY_RIGHT=%d\n", player2Keys.right);
        fprintf(f, "P2_KEY_A=%d\n", player2Keys.button_a);
        fprintf(f, "P2_KEY_B=%d\n", player2Keys.button_b);
        fprintf(f, "P2_KEY_START=%d\n", player2Keys.start);
        fprintf(f, "P2_KEY_SELECT=%d\n", player2Keys.select);
        fprintf(f, "\n");
        
        // Write Player 2 joystick controls
        fprintf(f, "# Player 2 Joystick Controls\n");
        fprintf(f, "P2_JOY_A=%d\n", player2Joy.button_a);
        fprintf(f, "P2_JOY_B=%d\n", player2Joy.button_b);
        fprintf(f, "P2_JOY_START=%d\n", player2Joy.start);
        fprintf(f, "P2_JOY_SELECT=%d\n", player2Joy.select);
        fprintf(f, "P2_JOY_USE_STICK=%d\n", player2Joy.use_stick ? 1 : 0);
        
        fclose(f);
        printf("Control configuration saved to controls.cfg\n");
    } else {
        printf("Warning: Could not save control configuration\n");
    }
}

void AllegroMainWindow::printControlMappings()
{
    printf("\n=== Current Control Mappings ===\n");
    
    printf("Player 1 Keyboard:\n");
    printf("  Up: %s (%d)\n", getKeyName(player1Keys.up), player1Keys.up);
    printf("  Down: %s (%d)\n", getKeyName(player1Keys.down), player1Keys.down);
    printf("  Left: %s (%d)\n", getKeyName(player1Keys.left), player1Keys.left);
    printf("  Right: %s (%d)\n", getKeyName(player1Keys.right), player1Keys.right);
    printf("  A: %s (%d)\n", getKeyName(player1Keys.button_a), player1Keys.button_a);
    printf("  B: %s (%d)\n", getKeyName(player1Keys.button_b), player1Keys.button_b);
    printf("  Start: %s (%d)\n", getKeyName(player1Keys.start), player1Keys.start);
    printf("  Select: %s (%d)\n", getKeyName(player1Keys.select), player1Keys.select);
    
    printf("\nPlayer 1 Joystick:\n");
    printf("  Use analog: %s\n", player1Joy.use_stick ? "Yes" : "No");
    printf("  Button A: %d\n", player1Joy.button_a);
    printf("  Button B: %d\n", player1Joy.button_b);
    printf("  Start: %d\n", player1Joy.start);
    printf("  Select: %d\n", player1Joy.select);
    
    printf("\nPlayer 2 Keyboard:\n");
    printf("  Up: %s (%d)\n", getKeyName(player2Keys.up), player2Keys.up);
    printf("  Down: %s (%d)\n", getKeyName(player2Keys.down), player2Keys.down);
    printf("  Left: %s (%d)\n", getKeyName(player2Keys.left), player2Keys.left);
    printf("  Right: %s (%d)\n", getKeyName(player2Keys.right), player2Keys.right);
    printf("  A: %s (%d)\n", getKeyName(player2Keys.button_a), player2Keys.button_a);
    printf("  B: %s (%d)\n", getKeyName(player2Keys.button_b), player2Keys.button_b);
    printf("  Start: %s (%d)\n", getKeyName(player2Keys.start), player2Keys.start);
    printf("  Select: %s (%d)\n", getKeyName(player2Keys.select), player2Keys.select);
    
    printf("\nPlayer 2 Joystick:\n");
    printf("  Use analog: %s\n", player2Joy.use_stick ? "Yes" : "No");
    printf("  Button A: %d\n", player2Joy.button_a);
    printf("  Button B: %d\n", player2Joy.button_b);
    printf("  Start: %d\n", player2Joy.start);
    printf("  Select: %d\n", player2Joy.select);
    printf("================================\n\n");
}


void AllegroMainWindow::runJoystickTest()
{
    if (num_joysticks == 0) {
        setStatusMessage("No joysticks to test");
        return;
    }
    
    // This could be expanded into a full joystick test mode
    // For now, just show current state
    static int testCounter = 0;
    testCounter++;
    
    if (testCounter % 30 == 0) { // Update every 30 frames (0.5 seconds at 60fps)
        showJoystickStatus();
    }
}

void AllegroMainWindow::loadControlConfig()
{
    FILE* f = fopen("controls.cfg", "r");  // "r" = read text
    if (f) {
        char line[256];
        char key[64];
        int value;
        
        printf("Loading control configuration from controls.cfg\n");
        
        while (fgets(line, sizeof(line), f)) {
            // Skip comments and empty lines
            if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
                continue;
            }
            
            // Parse KEY=VALUE format
            if (sscanf(line, "%63[^=]=%d", key, &value) == 2) {
                // Player 1 keyboard
                if (strcmp(key, "P1_KEY_UP") == 0) player1Keys.up = value;
                else if (strcmp(key, "P1_KEY_DOWN") == 0) player1Keys.down = value;
                else if (strcmp(key, "P1_KEY_LEFT") == 0) player1Keys.left = value;
                else if (strcmp(key, "P1_KEY_RIGHT") == 0) player1Keys.right = value;
                else if (strcmp(key, "P1_KEY_A") == 0) player1Keys.button_a = value;
                else if (strcmp(key, "P1_KEY_B") == 0) player1Keys.button_b = value;
                else if (strcmp(key, "P1_KEY_START") == 0) player1Keys.start = value;
                else if (strcmp(key, "P1_KEY_SELECT") == 0) player1Keys.select = value;
                
                // Player 1 joystick
                else if (strcmp(key, "P1_JOY_A") == 0) player1Joy.button_a = value;
                else if (strcmp(key, "P1_JOY_B") == 0) player1Joy.button_b = value;
                else if (strcmp(key, "P1_JOY_START") == 0) player1Joy.start = value;
                else if (strcmp(key, "P1_JOY_SELECT") == 0) player1Joy.select = value;
                else if (strcmp(key, "P1_JOY_USE_STICK") == 0) player1Joy.use_stick = (value != 0);
                
                // Player 2 keyboard
                else if (strcmp(key, "P2_KEY_UP") == 0) player2Keys.up = value;
                else if (strcmp(key, "P2_KEY_DOWN") == 0) player2Keys.down = value;
                else if (strcmp(key, "P2_KEY_LEFT") == 0) player2Keys.left = value;
                else if (strcmp(key, "P2_KEY_RIGHT") == 0) player2Keys.right = value;
                else if (strcmp(key, "P2_KEY_A") == 0) player2Keys.button_a = value;
                else if (strcmp(key, "P2_KEY_B") == 0) player2Keys.button_b = value;
                else if (strcmp(key, "P2_KEY_START") == 0) player2Keys.start = value;
                else if (strcmp(key, "P2_KEY_SELECT") == 0) player2Keys.select = value;
                
                // Player 2 joystick
                else if (strcmp(key, "P2_JOY_A") == 0) player2Joy.button_a = value;
                else if (strcmp(key, "P2_JOY_B") == 0) player2Joy.button_b = value;
                else if (strcmp(key, "P2_JOY_START") == 0) player2Joy.start = value;
                else if (strcmp(key, "P2_JOY_SELECT") == 0) player2Joy.select = value;
                else if (strcmp(key, "P2_JOY_USE_STICK") == 0) player2Joy.use_stick = (value != 0);
            }
        }
        
        fclose(f);
        printf("Control configuration loaded successfully\n");
    } else {
        printf("No control configuration file found, using defaults\n");
        setupDefaultControls();
    }
}


// Fix the control dialog display and input handling for DOS

void AllegroMainWindow::drawControlsDialog(BITMAP* target, Player player)
{
    const char* player_name = (player == PLAYER_1) ? "PLAYER 1" : "PLAYER 2";
    PlayerKeys* keys = (player == PLAYER_1) ? &player1Keys : &player2Keys;
    PlayerJoy* joy = (player == PLAYER_1) ? &player1Joy : &player2Joy;
    
    // Clear entire screen to black
    clear_to_color(target, makecol(0, 0, 0));
    
    int y = 5;  // Start from top
    int x = 5;  // Left margin
    
    // Title - simple and safe
    drawText(target, x, y, player_name, makecol(255, 255, 0));
    y += 12;
    drawText(target, x, y, "CONTROL CONFIGURATION", makecol(255, 255, 0));
    y += 20;
    
    // Keyboard section
    drawText(target, x, y, "KEYBOARD CONTROLS:", makecol(255, 255, 0));
    y += 15;
    
    // Use shorter, safer strings
    char line[40];  // Smaller buffer
    
    sprintf(line, "1. Up:     %s", getKeyName(keys->up));
    line[39] = '\0';  // Ensure null termination
    drawText(target, x, y, line, makecol(255, 255, 255));
    y += 12;
    
    sprintf(line, "2. Down:   %s", getKeyName(keys->down));
    line[39] = '\0';
    drawText(target, x, y, line, makecol(255, 255, 255));
    y += 12;
    
    sprintf(line, "3. Left:   %s", getKeyName(keys->left));
    line[39] = '\0';
    drawText(target, x, y, line, makecol(255, 255, 255));
    y += 12;
    
    sprintf(line, "4. Right:  %s", getKeyName(keys->right));
    line[39] = '\0';
    drawText(target, x, y, line, makecol(255, 255, 255));
    y += 12;
    
    sprintf(line, "5. A:      %s", getKeyName(keys->button_a));
    line[39] = '\0';
    drawText(target, x, y, line, makecol(255, 255, 255));
    y += 12;
    
    sprintf(line, "6. B:      %s", getKeyName(keys->button_b));
    line[39] = '\0';
    drawText(target, x, y, line, makecol(255, 255, 255));
    y += 12;
    
    sprintf(line, "7. Start:  %s", getKeyName(keys->start));
    line[39] = '\0';
    drawText(target, x, y, line, makecol(255, 255, 255));
    y += 12;
    
    sprintf(line, "8. Select: %s", getKeyName(keys->select));
    line[39] = '\0';
    drawText(target, x, y, line, makecol(255, 255, 255));
    y += 20;
    
    // Joystick section
    drawText(target, x, y, "JOYSTICK CONTROLS:", makecol(255, 255, 0));
    y += 15;
    
    if (num_joysticks > 0) {
        sprintf(line, "Detected: %d joystick(s)", num_joysticks);
        line[39] = '\0';
        drawText(target, x, y, line, makecol(255, 255, 255));
        y += 12;
        
        sprintf(line, "J. Use analog: %s", joy->use_stick ? "Yes" : "No");
        line[39] = '\0';
        drawText(target, x, y, line, makecol(255, 255, 255));
        y += 12;
        
        sprintf(line, "A. Button A: %d", joy->button_a);
        line[39] = '\0';
        drawText(target, x, y, line, makecol(255, 255, 255));
        y += 12;
        
        sprintf(line, "B. Button B: %d", joy->button_b);
        line[39] = '\0';
        drawText(target, x, y, line, makecol(255, 255, 255));
        y += 12;
        
        sprintf(line, "S. Start:    %d", joy->start);
        line[39] = '\0';
        drawText(target, x, y, line, makecol(255, 255, 255));
        y += 12;
        
        sprintf(line, "E. Select:   %d", joy->select);
        line[39] = '\0';
        drawText(target, x, y, line, makecol(255, 255, 255));
        y += 12;
        
        drawText(target, x, y, "T. Test joystick", makecol(128, 255, 128));
        y += 12;
    } else {
        drawText(target, x, y, "No joystick detected", makecol(255, 128, 128));
        y += 12;
    }
    
    y += 10;
    drawText(target, x, y, "R: Reset to defaults", makecol(128, 255, 128));
    y += 12;
    
    if (isCapturingInput) {
        drawText(target, x, y, "PRESS A KEY OR BUTTON...", makecol(255, 255, 0));
        y += 12;
        // Safely display capture prompt
        strncpy(line, currentCaptureKey, 35);
        line[35] = '\0';
        drawText(target, x, y, line, makecol(255, 255, 255));
        y += 12;
        drawText(target, x, y, "ESC to cancel", makecol(255, 128, 128));
    } else {
        drawText(target, x, y, "Numbers/Letters: Configure", makecol(255, 255, 255));
        y += 12;
        drawText(target, x, y, "ESC: Close", makecol(255, 255, 0));
    }
}

void AllegroMainWindow::updateAndDraw()
{
    // Clear the back buffer
    clear_to_color(back_buffer, makecol(0, 0, 0));
    
    // Draw everything to the back buffer
    if (currentDialog != DIALOG_NONE) {
        drawDialog(back_buffer);
    } else if (showingMenu) {
        drawMenu(back_buffer);
    } else {
        // Draw the game
        drawGameBuffered(back_buffer);
    }
        
    // Only draw status message temporarily (3 seconds) as overlay on the game
    if (statusMessageTimer > 0) {
        int msg_y = SCREEN_H - 25;
        
        // Black background for text readability
        rectfill(back_buffer, 5, msg_y - 5, 
                text_length(font, statusMessage) + 15, msg_y + 15, 
                makecol(0, 0, 0));
        
        // White text
        drawText(back_buffer, 10, msg_y, statusMessage, makecol(255, 255, 255));
    }
    
    // Copy back buffer to screen
    blit(back_buffer, screen, 0, 0, 0, 0, SCREEN_W, SCREEN_H);
    
}

void AllegroMainWindow::drawGame(BITMAP* target)
{
    if (!smbEngine) return;
    
    // For now, force the buffered method which ensures proper scaling
    drawGameBuffered(target);
}

void AllegroMainWindow::drawGameDirect(BITMAP* target)
{
    // Even simpler - just call the PPU
    drawGameBuffered(target);
}

void AllegroMainWindow::drawGameBuffered(BITMAP* target)
{
    if (!smbEngine) return;
    clear_to_color(target, makecol(0, 0, 0)); // Use black instead of blue
    if (bitmap_color_depth(target) == 16) {
        // Direct 16-bit rendering - this SHOULD use your cached PPU scaling
        smbEngine->renderScaled16((uint16_t*)target->line[0], SCREEN_W, SCREEN_H);
    } else {
        // For non-16-bit targets, use temporary buffer
        static uint16_t tempBuffer[1024 * 768];
        
        if (SCREEN_W * SCREEN_H <= 1024 * 768) {
            // Render with PPU cached scaling to temp buffer
            smbEngine->renderScaled16(tempBuffer, SCREEN_W, SCREEN_H);
            // Then convert to target format
            convertScreenBuffer16ToBitmap(tempBuffer, target);
        } else {
            // Fallback only for huge screens
            printf("Fallback should never be reached\n");
            //smbEngine->render(renderBuffer);
            //convertScreenBuffer32ToBitmap(renderBuffer, target);
        }
    }
}

/*void AllegroMainWindow::drawGameBuffered(BITMAP* target)
{
    if (!smbEngine) return;
    
    // Temporary fallback - use existing render methods
    static uint16_t nesBuffer[256 * 240];
    smbEngine->render16(nesBuffer);
    
    // Simple scaling logic (temporary)
    clear_to_color(target, makecol(0, 0, 0)); // Use black instead of blue
    
    int scale_x = SCREEN_W / 256;
    int scale_y = SCREEN_H / 240;
    int scale = (scale_x < scale_y) ? scale_x : scale_y;
    if (scale < 1) scale = 1;
    
    int dest_w = 256 * scale;
    int dest_h = 240 * scale;
    int dest_x = (SCREEN_W - dest_w) / 2;
    int dest_y = (SCREEN_H - dest_h) / 2;
    
    if (bitmap_color_depth(target) == 16) {
        convertBuffer16ToBitmap16(nesBuffer, target, dest_x, dest_y, dest_w, dest_h, scale);
    } else {
        convertBuffer16ToBitmapGeneric(nesBuffer, target, dest_x, dest_y, dest_w, dest_h, scale);
    }
}*/

void AllegroMainWindow::convertScreenBuffer16ToBitmap(uint16_t* buffer16, BITMAP* bitmap)
{
    // Simple color format conversion only
    for (int y = 0; y < SCREEN_H && y < bitmap->h; y++) {
        uint16_t* src_row = &buffer16[y * SCREEN_W];
        
        for (int x = 0; x < SCREEN_W && x < bitmap->w; x++) {
            uint16_t pixel16 = src_row[x];
            
            // Convert RGB565 to RGB888
            int r = ((pixel16 >> 11) & 0x1F) << 3;
            int g = ((pixel16 >> 5) & 0x3F) << 2;
            int b = (pixel16 & 0x1F) << 3;
            
            putpixel(bitmap, x, y, makecol(r, g, b));
        }
    }
}

void AllegroMainWindow::convertScreenBuffer32ToBitmap(uint32_t* buffer32, BITMAP* bitmap)
{
    // Convert 32-bit buffer to bitmap format
    for (int y = 0; y < SCREEN_H && y < bitmap->h; y++) {
        uint32_t* src_row = &buffer32[y * SCREEN_W];
        
        for (int x = 0; x < SCREEN_W && x < bitmap->w; x++) {
            uint32_t pixel32 = src_row[x];
            
            int r = (pixel32 >> 16) & 0xFF;
            int g = (pixel32 >> 8) & 0xFF;
            int b = pixel32 & 0xFF;
            
            putpixel(bitmap, x, y, makecol(r, g, b));
        }
    }
}

// Optimized 16-bit to 16-bit scaling
void AllegroMainWindow::convertBuffer16ToBitmap16(uint16_t* buffer16, BITMAP* bitmap, 
                                                 int dest_x, int dest_y, int dest_w, int dest_h, int scale)
{
    if (scale == 1) {
        // 1:1 copy - optimized
        for (int y = 0; y < 240 && y + dest_y < bitmap->h; y++) {
            if (y + dest_y < 0) continue;
            
            uint16_t* src_row = &buffer16[y * 256];
            uint16_t* dest_row = (uint16_t*)bitmap->line[y + dest_y] + dest_x;
            
            int copy_width = std::min(256, bitmap->w - dest_x);
            if (copy_width > 0) {
                memcpy(dest_row, src_row, copy_width * sizeof(uint16_t));
            }
        }
        return;
    }
    
    if (scale == 2) {
        // Use optimized 2x scaling
        convertBuffer16ToBitmap16_2x(buffer16, bitmap, dest_x, dest_y);
        return;
    }
    
    // Generic scaling for other factors (keep your existing code for this part)
    for (int y = 0; y < 240; y++) {
        uint16_t* src_row = &buffer16[y * 256];
        
        for (int scale_y = 0; scale_y < scale; scale_y++) {
            int dest_row_idx = y * scale + scale_y + dest_y;
            if (dest_row_idx >= bitmap->h) break;
            if (dest_row_idx < 0) continue;
            
            uint16_t* dest_row = (uint16_t*)bitmap->line[dest_row_idx];
            
            for (int x = 0; x < 256; x++) {
                uint16_t pixel = src_row[x];
                int dest_start = x * scale + dest_x;
                
                for (int scale_x = 0; scale_x < scale; scale_x++) {
                    int dest_pos = dest_start + scale_x;
                    if (dest_pos < bitmap->w && dest_pos >= 0) {
                        dest_row[dest_pos] = pixel;
                    }
                }
            }
        }
    }
}

// Generic scaling for non-16-bit bitmaps
void AllegroMainWindow::convertBuffer16ToBitmapGeneric(uint16_t* buffer16, BITMAP* bitmap, 
                                                      int dest_x, int dest_y, int dest_w, int dest_h, int scale)
{
    for (int y = 0; y < 240; y++) {
        uint16_t* src_row = &buffer16[y * 256];
        
        for (int x = 0; x < 256; x++) {
            uint16_t pixel16 = src_row[x];
            
            // Convert RGB565 to 8-bit RGB components
            int r = (pixel16 >> 11) & 0x1F;
            int g = (pixel16 >> 5) & 0x3F;
            int b = pixel16 & 0x1F;
            
            // Scale to 8-bit
            r = (r << 3) | (r >> 2);
            g = (g << 2) | (g >> 4);
            b = (b << 3) | (b >> 2);
            
            int color = makecol(r, g, b);
            
            // Draw scaled pixel
            for (int scale_y = 0; scale_y < scale; scale_y++) {
                int dest_row = y * scale + scale_y + dest_y;
                if (dest_row >= SCREEN_H) break;
                
                for (int scale_x = 0; scale_x < scale; scale_x++) {
                    int dest_col = x * scale + scale_x + dest_x;
                    if (dest_col < SCREEN_W) {
                        putpixel(bitmap, dest_col, dest_row, color);
                    }
                }
            }
        }
    }
}

// Fast conversion from NES 16-bit buffer to game bitmap with efficient scaling
void AllegroMainWindow::convertNESBuffer16ToBitmap(uint16_t* nesBuffer, BITMAP* bitmap)
{
    const int nesWidth = 256;
    const int nesHeight = 240;
    
    // Calculate scale factor for the game buffer
    int scale_x = bitmap->w / nesWidth;
    int scale_y = bitmap->h / nesHeight;
    int scale = (scale_x < scale_y) ? scale_x : scale_y;
    if (scale < 1) scale = 1;
    
    // For game_buffer, we usually want to fill it completely or center the image
    if (bitmap->w == nesWidth && bitmap->h == nesHeight) {
        // 1:1 - direct copy
        if (bitmap_color_depth(bitmap) == 16) {
            for (int y = 0; y < nesHeight; y++) {
                uint16_t* src_row = &nesBuffer[y * nesWidth];
                uint16_t* dest_row = (uint16_t*)bitmap->line[y];
                memcpy(dest_row, src_row, nesWidth * sizeof(uint16_t));
            }
        } else {
            // Convert color depth
            for (int y = 0; y < nesHeight; y++) {
                uint16_t* src_row = &nesBuffer[y * nesWidth];
                
                for (int x = 0; x < nesWidth; x++) {
                    uint16_t pixel16 = src_row[x];
                    
                    // Convert RGB565 to RGB components
                    int r = (pixel16 >> 11) & 0x1F;
                    int g = (pixel16 >> 5) & 0x3F;
                    int b = pixel16 & 0x1F;
                    
                    // Scale to 8-bit
                    r = (r << 3) | (r >> 2);
                    g = (g << 2) | (g >> 4);
                    b = (b << 3) | (b >> 2);
                    
                    putpixel(bitmap, x, y, makecol(r, g, b));
                }
            }
        }
    } else {
        // Scaled copy - use the optimized scaling functions
        int dest_w = nesWidth * scale;
        int dest_h = nesHeight * scale;
        int dest_x = (bitmap->w - dest_w) / 2;
        int dest_y = (bitmap->h - dest_h) / 2;
        
        clear_to_color(bitmap, makecol(0, 0, 0));
        
        if (bitmap_color_depth(bitmap) == 16) {
            convertBuffer16ToBitmap16(nesBuffer, bitmap, dest_x, dest_y, dest_w, dest_h, scale);
        } else {
            //convertBuffer16ToBitmapGeneric(nesBuffer, bitmap, dest_x, dest_y, dest_w, dest_h, scale);
        }
    }
}

void AllegroMainWindow::convertBuffer16ToBitmapScaled(uint16_t* buffer16, BITMAP* bitmap, int width, int height)
{
    // This buffer already contains the scaled and centered image from SMBEngine
    // Just convert the color format
    if (bitmap_color_depth(bitmap) == 16) {
        // Direct memory copy for 16-bit bitmaps
        for (int y = 0; y < height && y < bitmap->h; y++) {
            uint16_t* src_row = &buffer16[y * width];
            uint16_t* dest_row = (uint16_t*)bitmap->line[y];
            int copy_width = (width < bitmap->w) ? width : bitmap->w;
            memcpy(dest_row, src_row, copy_width * sizeof(uint16_t));
        }
    } else {
        // Convert for other color depths
        for (int y = 0; y < height && y < bitmap->h; y++) {
            uint16_t* src_row = &buffer16[y * width];
            
            for (int x = 0; x < width && x < bitmap->w; x++) {
                uint16_t pixel16 = src_row[x];
                
                // Convert RGB565 to 8-bit RGB components
                int r = (pixel16 >> 11) & 0x1F;
                int g = (pixel16 >> 5) & 0x3F;
                int b = pixel16 & 0x1F;
                
                // Scale to 8-bit
                r = (r << 3) | (r >> 2);
                g = (g << 2) | (g >> 4);
                b = (b << 3) | (b >> 2);
                
                putpixel(bitmap, x, y, makecol(r, g, b));
            }
        }
    }
}
void AllegroMainWindow::drawGameUltraFast(BITMAP* target)
{
    // This version requires the screen bitmap to be 16-bit and accessible
    if (bitmap_color_depth(target) == 16) {
        // Get pointer to screen memory
        uint16_t* screenMem = (uint16_t*)target->line[0];
        
        // Render directly to screen memory with centering
        //smbEngine->renderDirectFast(screenMem, SCREEN_W, SCREEN_H);
        smbEngine->render16(screenMem);
    } else {
        // Fall back to buffered method
        drawGameDirect(target);
    }
}

void AllegroMainWindow::drawMenu(BITMAP* target)
{
    int menu_x = SCREEN_W / 2 - 100;
    int menu_y = SCREEN_H / 2 - (menuCount * 10);
    
    // Draw menu background
    rectfill(target, menu_x - 20, menu_y - 20, 
             menu_x + 200, menu_y + menuCount * 20 + 20, 
             makecol(64, 64, 64));
    
    // Draw menu border
    rect(target, menu_x - 20, menu_y - 20, 
         menu_x + 200, menu_y + menuCount * 20 + 20, 
         makecol(255, 255, 255));
    
    // Draw menu title
    drawTextCentered(target, menu_y - 10, "GAME MENU", makecol(255, 255, 0));
    
    // Draw menu items
    for (int i = 0; i < menuCount; i++) {
        int color = (i == selectedMenuItem) ? makecol(255, 255, 0) : makecol(255, 255, 255);
        if (i == selectedMenuItem) {
            drawText(target, menu_x - 10, menu_y + i * 20, ">", color);
        }
        drawText(target, menu_x, menu_y + i * 20, mainMenu[i].text, color);
    }
}

void AllegroMainWindow::drawDialog(BITMAP* target)
{
    switch (currentDialog) {
        case DIALOG_ABOUT:
            {
                int dialog_x = SCREEN_W / 4;
                int dialog_y = SCREEN_H / 4;
                int dialog_w = SCREEN_W / 2;
                int dialog_h = SCREEN_H / 2;
                
                rectfill(target, dialog_x, dialog_y, dialog_x + dialog_w, dialog_y + dialog_h, 
                         makecol(32, 32, 32));
                rect(target, dialog_x, dialog_y, dialog_x + dialog_w, dialog_y + dialog_h, 
                     makecol(255, 255, 255));
                
                drawTextCentered(target, dialog_y + 20, "SUPER MARIO BROS Virtualizer", makecol(255, 255, 0));
                drawTextCentered(target, dialog_y + 40, "Version 1.0", makecol(255, 255, 255));
                drawTextCentered(target, dialog_y + 60, "Built with Allegro 4", makecol(255, 255, 255));
                
                #ifdef __DJGPP__
                drawTextCentered(target, dialog_y + 80, "DOS Version", makecol(255, 255, 255));
                #else
                drawTextCentered(target, dialog_y + 80, "Linux Version", makecol(255, 255, 255));
                #endif
                
                drawTextCentered(target, dialog_y + 100, "Original game (c) Nintendo", makecol(255, 255, 255));
                drawTextCentered(target, dialog_y + dialog_h - 40, "Press ESC to close", makecol(255, 255, 0));
            }
            break;
            
        case DIALOG_HELP:
            {
                int dialog_x = SCREEN_W / 4;
                int dialog_y = SCREEN_H / 4;
                int dialog_w = SCREEN_W / 2;
                int dialog_h = SCREEN_H / 2;
                
                rectfill(target, dialog_x, dialog_y, dialog_x + dialog_w, dialog_y + dialog_h, 
                         makecol(32, 32, 32));
                rect(target, dialog_x, dialog_y, dialog_x + dialog_w, dialog_y + dialog_h, 
                     makecol(255, 255, 255));
                
                drawTextCentered(target, dialog_y + 20, "HELP", makecol(255, 255, 0));
                
                int y_offset = dialog_y + 50;
                drawText(target, dialog_x + 20, y_offset, "ESC - Show menu", makecol(255, 255, 255));
                y_offset += 20;
                
                #ifndef __DJGPP__
                drawText(target, dialog_x + 20, y_offset, "F11 - Toggle fullscreen", makecol(255, 255, 255));
                y_offset += 20;
                #endif
                
                drawText(target, dialog_x + 20, y_offset, "P - Pause game", makecol(255, 255, 255));
                y_offset += 20;
                drawText(target, dialog_x + 20, y_offset, "Ctrl+R - Reset game", makecol(255, 255, 255));
                y_offset += 30;
                
                drawText(target, dialog_x + 20, y_offset, "Default Player 1:", makecol(255, 255, 0));
                y_offset += 20;
                drawText(target, dialog_x + 20, y_offset, "Arrow Keys, Z/X, [/]", makecol(255, 255, 255));
                y_offset += 20;
                drawText(target, dialog_x + 20, y_offset, "Default Player 2:", makecol(255, 255, 0));
                y_offset += 20;
                drawText(target, dialog_x + 20, y_offset, "WASD, F/G, O/P", makecol(255, 255, 255));
                
                drawTextCentered(target, dialog_y + dialog_h - 40, "Press ESC to close", makecol(255, 255, 0));
            }
            break;
            
        case DIALOG_CONTROLS_P1:
            drawControlsDialog(target, PLAYER_1);
            break;
            
        case DIALOG_CONTROLS_P2:
            drawControlsDialog(target, PLAYER_2);
            break;
            
        case DIALOG_VIDEO_OPTIONS:  // NEW
            drawVideoOptionsDialog(target);
            break;
    }
}

void AllegroMainWindow::drawVideoOptionsDialog(BITMAP* target)
{
    clear_to_color(target, makecol(0, 0, 0));
    
    int y = 5;
    int x = 5;
    
    // Title
    drawText(target, x, y, "VIDEO OPTIONS", makecol(255, 255, 0));
    y += 20;
    
    // Current mode
    char currentModeText[64];
    if (numAvailableModes > 0 && videoSettings.currentMode < numAvailableModes) {
        sprintf(currentModeText, "Current: %s", availableModes[videoSettings.currentMode].description);
    } else {
        sprintf(currentModeText, "Current: %dx%d", SCREEN_W, SCREEN_H);
    }
    drawText(target, x, y, currentModeText, makecol(255, 255, 255));
    y += 20;
    
    // Available modes
    drawText(target, x, y, "AVAILABLE RESOLUTIONS:", makecol(255, 255, 0));
    y += 15;
    
    for (int i = 0; i < numAvailableModes; i++) {
        char modeText[64];
        sprintf(modeText, "%d. %s", i + 1, availableModes[i].description);
        
        int color = (selectedVideoOption == i) ? makecol(255, 255, 0) : makecol(255, 255, 255);
        if (selectedVideoOption == i) {
            drawText(target, x - 5, y, ">", color);
        }
        drawText(target, x + 10, y, modeText, color);
        
        if (i == videoSettings.currentMode) {
            drawText(target, x + 250, y, "(current)", makecol(128, 255, 128));
        }
        y += 12;
    }
    
    y += 10;
    
    // Scaling options
    drawText(target, x, y, "SCALING OPTIONS:", makecol(255, 255, 0));
    y += 15;
    
    char scalingText[64];
    const char* scalingNames[] = {"Nearest", "Smooth", "Scanlines"};
    sprintf(scalingText, "S. Scaling: %s", scalingNames[videoSettings.scalingMode]);
    int scalingColor = (selectedVideoOption == numAvailableModes) ? makecol(255, 255, 0) : makecol(255, 255, 255);
    if (selectedVideoOption == numAvailableModes) {
        drawText(target, x - 5, y, ">", scalingColor);
    }
    drawText(target, x + 10, y, scalingText, scalingColor);
    y += 12;
    
    // Aspect ratio
    char aspectText[64];
    sprintf(aspectText, "A. Maintain Aspect: %s", videoSettings.maintainAspect ? "Yes" : "No");
    int aspectColor = (selectedVideoOption == numAvailableModes + 1) ? makecol(255, 255, 0) : makecol(255, 255, 255);
    if (selectedVideoOption == numAvailableModes + 1) {
        drawText(target, x - 5, y, ">", aspectColor);
    }
    drawText(target, x + 10, y, aspectText, aspectColor);
    y += 12;
    
    // Center image
    char centerText[64];
    sprintf(centerText, "C. Center Image: %s", videoSettings.centerImage ? "Yes" : "No");
    int centerColor = (selectedVideoOption == numAvailableModes + 2) ? makecol(255, 255, 0) : makecol(255, 255, 255);
    if (selectedVideoOption == numAvailableModes + 2) {
        drawText(target, x - 5, y, ">", centerColor);
    }
    drawText(target, x + 10, y, centerText, centerColor);
    y += 15;
    
    // Instructions
    y += 10;
    drawText(target, x, y, "1-9: Select resolution", makecol(255, 255, 255));
    y += 12;
    drawText(target, x, y, "UP/DOWN: Navigate", makecol(255, 255, 255));
    y += 12;
    drawText(target, x, y, "ENTER: Apply setting", makecol(255, 255, 255));
    y += 12;
    drawText(target, x, y, "T: Test mode", makecol(128, 255, 128));
    y += 12;
    drawText(target, x, y, "R: Reset defaults", makecol(128, 255, 128));
    y += 12;
    drawText(target, x, y, "ESC: Close", makecol(255, 255, 0));
}

void AllegroMainWindow::handleVideoOptionsInput()
{
    if (keypressed()) {
        int k = readkey();
        int scancode = k >> 8;
        char ascii = k & 0xFF;
        
        // Convert to lowercase
        if (ascii >= 'A' && ascii <= 'Z') {
            ascii = ascii - 'A' + 'a';
        }
        
        switch (scancode) {
            case KEY_UP:
                selectedVideoOption--;
                if (selectedVideoOption < 0) {
                    selectedVideoOption = numAvailableModes + 2; // Last option
                }
                break;
                
            case KEY_DOWN:
                selectedVideoOption++;
                if (selectedVideoOption > numAvailableModes + 2) {
                    selectedVideoOption = 0;
                }
                break;
                
            case KEY_ENTER:
                if (selectedVideoOption < numAvailableModes) {
                    // Change resolution
                    if (setVideoMode(selectedVideoOption)) {
                        setStatusMessage("Video mode changed");
                        saveVideoConfig();
                    } else {
                        setStatusMessage("Failed to change video mode");
                    }
                } else if (selectedVideoOption == numAvailableModes) {
                    // Toggle scaling mode
                    videoSettings.scalingMode = (videoSettings.scalingMode + 1) % 3;
                    saveVideoConfig();
                } else if (selectedVideoOption == numAvailableModes + 1) {
                    // Toggle aspect ratio
                    videoSettings.maintainAspect = !videoSettings.maintainAspect;
                    saveVideoConfig();
                } else if (selectedVideoOption == numAvailableModes + 2) {
                    // Toggle center image
                    videoSettings.centerImage = !videoSettings.centerImage;
                    saveVideoConfig();
                }
                break;
        }
        
        // Handle number keys for direct mode selection
        if (ascii >= '1' && ascii <= '9') {
            int modeIndex = ascii - '1';
            if (modeIndex < numAvailableModes) {
                selectedVideoOption = modeIndex;
                if (setVideoMode(modeIndex)) {
                    setStatusMessage("Video mode changed");
                    saveVideoConfig();
                } else {
                    setStatusMessage("Failed to change video mode");
                }
            }
        }
        
        // Handle letter commands
        switch (ascii) {
            case 't':
                testVideoMode(videoSettings.currentMode);
                break;
                
            case 'r':
                resetVideoDefaults();
                setStatusMessage("Video settings reset");
                saveVideoConfig();
                break;
                
            case 's':
                videoSettings.scalingMode = (videoSettings.scalingMode + 1) % 3;
                saveVideoConfig();
                break;
                
            case 'a':
                videoSettings.maintainAspect = !videoSettings.maintainAspect;
                saveVideoConfig();
                break;
                
            case 'c':
                videoSettings.centerImage = !videoSettings.centerImage;
                saveVideoConfig();
                break;
        }
    }
}

bool AllegroMainWindow::setVideoMode(int index)
{
    if (index < 0 || index >= numAvailableModes) return false;
    
    int newWidth = availableModes[index].width;
    int newHeight = availableModes[index].height;
    
    printf("Switching to %dx%d...\n", newWidth, newHeight);
    
    #ifndef __DJGPP__
    // On Linux, use appropriate graphics mode based on fullscreen state
    int gfxMode = isFullscreen ? GFX_AUTODETECT : GFX_AUTODETECT_WINDOWED;
    #else
    // On DOS, always use autodetect (fullscreen only)
    int gfxMode = GFX_AUTODETECT;
    #endif
    
    if (set_gfx_mode(gfxMode, newWidth, newHeight, 0, 0) != 0) {
        printf("Failed to set %dx%d: %s\n", newWidth, newHeight, allegro_error);
        
        // Try to restore previous mode
        if (videoSettings.currentMode >= 0 && videoSettings.currentMode < numAvailableModes) {
            int oldWidth = availableModes[videoSettings.currentMode].width;
            int oldHeight = availableModes[videoSettings.currentMode].height;
            set_gfx_mode(gfxMode, oldWidth, oldHeight, 0, 0);
        }
        
        return false;
    }
    
    videoSettings.currentMode = index;
    
    // Recreate buffers for new screen size
    if (!createBuffers()) {
        printf("Failed to recreate buffers\n");
        return false;
    }
    
    printf("Successfully changed to %dx%d\n", SCREEN_W, SCREEN_H);
    return true;
}

void AllegroMainWindow::testVideoMode(int index)
{
    if (index >= 0 && index < numAvailableModes) {
        char testMsg[64];
        sprintf(testMsg, "Testing %s...", availableModes[index].description);
        setStatusMessage(testMsg);
    }
}

void AllegroMainWindow::resetVideoDefaults()
{
    videoSettings.scalingMode = 0;
    videoSettings.maintainAspect = true;
    videoSettings.centerImage = true;
    videoSettings.brightness = 128;
    videoSettings.contrast = 128;
}

void AllegroMainWindow::saveVideoConfig()
{
    FILE* f = fopen("video.cfg", "w");
    if (f) {
        fprintf(f, "# Video Configuration\n");
        fprintf(f, "VIDEO_MODE=%d\n", videoSettings.currentMode);
        fprintf(f, "SCALING_MODE=%d\n", videoSettings.scalingMode);
        fprintf(f, "MAINTAIN_ASPECT=%d\n", videoSettings.maintainAspect ? 1 : 0);
        fprintf(f, "CENTER_IMAGE=%d\n", videoSettings.centerImage ? 1 : 0);
        fclose(f);
    }
}

void AllegroMainWindow::loadVideoConfig()
{
    FILE* f = fopen("video.cfg", "r");
    if (f) {
        char line[256];
        char key[64];
        int value;
        
        while (fgets(line, sizeof(line), f)) {
            if (line[0] == '#' || line[0] == '\n') continue;
            
            if (sscanf(line, "%63[^=]=%d", key, &value) == 2) {
                if (strcmp(key, "VIDEO_MODE") == 0 && value >= 0 && value < numAvailableModes) {
                    videoSettings.currentMode = value;
                } else if (strcmp(key, "SCALING_MODE") == 0 && value >= 0 && value <= 2) {
                    videoSettings.scalingMode = value;
                } else if (strcmp(key, "MAINTAIN_ASPECT") == 0) {
                    videoSettings.maintainAspect = (value != 0);
                } else if (strcmp(key, "CENTER_IMAGE") == 0) {
                    videoSettings.centerImage = (value != 0);
                }
            }
        }
        fclose(f);
    }
}


void AllegroMainWindow::drawStatusBar(BITMAP* target)
{
    int status_y = SCREEN_H - 20;
    rectfill(target, 0, status_y, SCREEN_W, SCREEN_H, makecol(64, 64, 64));
    drawText(target, 10, status_y + 5, statusMessage, makecol(255, 255, 255));
}

void AllegroMainWindow::drawText(BITMAP* target, int x, int y, const char* text, int color)
{
    textout(target, font, text, x, y, color);
}

void AllegroMainWindow::drawTextCentered(BITMAP* target, int y, const char* text, int color)
{
    int x = (SCREEN_W - text_length(font, text)) / 2;
    textout(target, font, text, x, y, color);
}

void AllegroMainWindow::menuUp()
{
    selectedMenuItem--;
    if (selectedMenuItem < 0) {
        selectedMenuItem = menuCount - 1;
    }
}

void AllegroMainWindow::menuDown()
{
    selectedMenuItem++;
    if (selectedMenuItem >= menuCount) {
        selectedMenuItem = 0;
    }
}

void AllegroMainWindow::handleMenuInputNoEsc()
{
    // Check for key presses
    if (keypressed()) {
        int k = readkey();
        int scancode = k >> 8;
        
        switch (scancode) {
            case KEY_UP:
                menuUp();
                break;
            case KEY_DOWN:
                menuDown();
                break;
            case KEY_ENTER:
                menuSelect();
                break;
            // ESC is handled globally, don't handle it here
        }
    }
}

void AllegroMainWindow::menuSelect()
{
    switch (mainMenu[selectedMenuItem].id) {
        case 1: // Resume Game
            showingMenu = false;
            gamePaused = false;
            setStatusMessage("Game Resumed");
            break;
            
        case 2: // Reset Game
            if (smbEngine) {
                smbEngine->reset();
                setStatusMessage("Game Reset");
            }
            showingMenu = false;
            gamePaused = false;
            break;
            
        case 3: // Player 1 Controls
            currentDialog = DIALOG_CONTROLS_P1;
            currentConfigPlayer = PLAYER_1;
            showingMenu = false;
            break;
            
        case 4: // Player 2 Controls
            currentDialog = DIALOG_CONTROLS_P2;
            currentConfigPlayer = PLAYER_2;
            showingMenu = false;
            break;
            
        case 5: // Help
            currentDialog = DIALOG_HELP;
            showingMenu = false;
            break;
            
        case 6: // About
            currentDialog = DIALOG_ABOUT;
            showingMenu = false;
            break;
            
        case 7: // Quit
            gameRunning = false;
            break;
            
        case 8: // Test Joystick
            if (num_joysticks > 0) {
                showJoystickStatus();
            } else {
                setStatusMessage("No joystick detected");
            }
            break;
            
        case 9: // Video Options - NEW
            currentDialog = DIALOG_VIDEO_OPTIONS;
            selectedVideoOption = 0;
            showingMenu = false;
            break;
    }
}

void AllegroMainWindow::menuEscape()
{
    showingMenu = false;
    gamePaused = false;  // Resume the game when exiting menu with ESC
    setStatusMessage("Game Resumed");
}

void AllegroMainWindow::setStatusMessage(const char* msg)
{
    strncpy(statusMessage, msg, sizeof(statusMessage) - 1);
    statusMessage[sizeof(statusMessage) - 1] = '\0';
    statusMessageTimer = 180; // 3 seconds at 60 FPS - will disappear automatically
}

void AllegroMainWindow::updateStatusMessage()
{
    if (statusMessageTimer > 0) {
        statusMessageTimer--;
        if (statusMessageTimer == 0) {
            strcpy(statusMessage, "Ready");
        }
    }
}

void AllegroMainWindow::shutdown() 
{
    if (audiostream) {
        stop_audio_stream(audiostream);
        audiostream = NULL;
    }
    
    dosAudioInitialized = false;
    audioStreamLen = 0;
    audioStreamPos = 0;
    
    if (game_buffer) {
        destroy_bitmap(game_buffer);
        game_buffer = NULL;
    }
    
    if (back_buffer) {
        destroy_bitmap(back_buffer);
        back_buffer = NULL;
    }

    if (zapperEnabled) {
        show_mouse(NULL);  // Hide mouse cursor
        printf("Mouse support disabled\n");
    }
    saveControlConfig();
    
    allegro_exit();
}

void AllegroMainWindow::startKeyCapture(CaptureType captureType, const char* promptText)
{
    currentCaptureType = captureType;
    isCapturingInput = true;
    strncpy(currentCaptureKey, promptText, sizeof(currentCaptureKey) - 1);
    currentCaptureKey[sizeof(currentCaptureKey) - 1] = '\0';
}

void AllegroMainWindow::assignCapturedKey(int scancode)
{
    PlayerKeys* keys = (currentConfigPlayer == PLAYER_1) ? &player1Keys : &player2Keys;
    
    switch (currentCaptureType) {
        case CAPTURE_KEY_UP:
            keys->up = scancode;
            setStatusMessage("UP key assigned");
            break;
        case CAPTURE_KEY_DOWN:
            keys->down = scancode;
            setStatusMessage("DOWN key assigned");
            break;
        case CAPTURE_KEY_LEFT:
            keys->left = scancode;
            setStatusMessage("LEFT key assigned");
            break;
        case CAPTURE_KEY_RIGHT:
            keys->right = scancode;
            setStatusMessage("RIGHT key assigned");
            break;
        case CAPTURE_KEY_A:
            keys->button_a = scancode;
            setStatusMessage("A BUTTON key assigned");
            break;
        case CAPTURE_KEY_B:
            keys->button_b = scancode;
            setStatusMessage("B BUTTON key assigned");
            break;
        case CAPTURE_KEY_START:
            keys->start = scancode;
            setStatusMessage("START key assigned");
            break;
        case CAPTURE_KEY_SELECT:
            keys->select = scancode;
            setStatusMessage("SELECT key assigned");
            break;
        default:
            setStatusMessage("Unknown key assignment");
            break;
    }
    
    isCapturingInput = false;
    currentCaptureType = CAPTURE_NONE;
    strcpy(currentCaptureKey, "");
    saveControlConfig();
}

void AllegroMainWindow::assignCapturedJoyButton(int buttonNum)
{
    PlayerJoy* joy = (currentConfigPlayer == PLAYER_1) ? &player1Joy : &player2Joy;
    
    switch (currentCaptureType) {
        case CAPTURE_JOY_A:
            joy->button_a = buttonNum;
            setStatusMessage("A BUTTON (joystick) assigned");
            break;
        case CAPTURE_JOY_B:
            joy->button_b = buttonNum;
            setStatusMessage("B BUTTON (joystick) assigned");
            break;
        case CAPTURE_JOY_START:
            joy->start = buttonNum;
            setStatusMessage("START (joystick) assigned");
            break;
        case CAPTURE_JOY_SELECT:
            joy->select = buttonNum;
            setStatusMessage("SELECT (joystick) assigned");
            break;
        default:
            setStatusMessage("Unknown joystick assignment");
            break;
    }
    
    isCapturingInput = false;
    currentCaptureType = CAPTURE_NONE;
    strcpy(currentCaptureKey, "");
    saveControlConfig();
}

void AllegroMainWindow::convertBuffer16ToBitmap16_2x(uint16_t* buffer16, BITMAP* bitmap, 
                                                    int dest_x, int dest_y) {
    // Optimized 2x scaling - no arithmetic in inner loops
    for (int y = 0; y < 240; y++) {
        int dest_y1 = y * 2 + dest_y;
        int dest_y2 = dest_y1 + 1;
        
        if (dest_y2 >= bitmap->h) break;
        if (dest_y1 < 0) continue;
        
        uint16_t* src_row = &buffer16[y * 256];
        uint16_t* dest_row1 = (uint16_t*)bitmap->line[dest_y1] + dest_x;
        uint16_t* dest_row2 = (uint16_t*)bitmap->line[dest_y2] + dest_x;
        
        // Process 4 pixels at a time for better performance
        for (int x = 0; x < 256; x += 4) {
            if ((x * 2 + dest_x + 8) > bitmap->w) break;
            
            uint16_t p1 = src_row[x];
            uint16_t p2 = src_row[x + 1];
            uint16_t p3 = src_row[x + 2];
            uint16_t p4 = src_row[x + 3];
            
            int dest_base = x * 2;
            
            // First row - unrolled
            dest_row1[dest_base] = p1;
            dest_row1[dest_base + 1] = p1;
            dest_row1[dest_base + 2] = p2;
            dest_row1[dest_base + 3] = p2;
            dest_row1[dest_base + 4] = p3;
            dest_row1[dest_base + 5] = p3;
            dest_row1[dest_base + 6] = p4;
            dest_row1[dest_base + 7] = p4;
            
            // Second row - duplicate
            dest_row2[dest_base] = p1;
            dest_row2[dest_base + 1] = p1;
            dest_row2[dest_base + 2] = p2;
            dest_row2[dest_base + 3] = p2;
            dest_row2[dest_base + 4] = p3;
            dest_row2[dest_base + 5] = p3;
            dest_row2[dest_base + 6] = p4;
            dest_row2[dest_base + 7] = p4;
        }
    }
}

void debugMapperInfo(const char* romFilename) 
{
    printf("\n=== ROM Analysis ===\n");
    printf("ROM file: %s\n", romFilename);
    
    // Quick NES header analysis
    FILE* f = fopen(romFilename, "rb");
    if (f) {
        uint8_t header[16];
        if (fread(header, 1, 16, f) == 16) {
            if (header[0] == 'N' && header[1] == 'E' && header[2] == 'S' && header[3] == 0x1A) {
                uint8_t mapper = (header[6] >> 4) | (header[7] & 0xF0);
                uint8_t prgPages = header[4];
                uint8_t chrPages = header[5];

                bool needsCycleAccuracy = false;
                const char* reason = "";
                
                printf("Mapper: %d\n", mapper);
                printf("PRG ROM: %d pages (16KB each)\n", prgPages);
                printf("CHR ROM: %d pages (8KB each)\n", chrPages);

                if (zapperEnabled) {
                        needsCycleAccuracy = true;
                        reason="Zapper";
                }                
                // Predict update method
                
                switch (mapper) {
                    case 2:
                        needsCycleAccuracy = true;
                        reason = "UxROM uses CHR-RAM";
                        break;
                    case 4:
                        needsCycleAccuracy = true;
                        reason = "MMC3 has IRQ timing";
                        break;
                    case 66:
                        needsCycleAccuracy = true;
                        reason = "GxROM has CHR banking";
                        break;
                    default:
                        needsCycleAccuracy = false;
                        reason = "No mid-frame banking";
                        break;
                }
                
                printf("Will use: %s (%s)\n", 
                       needsCycleAccuracy ? "Cycle-Accurate" : "Fast Frame-Based", 
                       reason);
            } else {
                printf("Invalid NES ROM format\n");
            }
        }
        fclose(f);
    }
    printf("===================\n\n");
}

int main(int argc, char** argv) 
{
    printf("WarpNES\n");
    
    // Parse command line arguments
    const char* romFilename = nullptr;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--neszapper") == 0) {
            zapperEnabled = true;
            printf("NES Zapper enabled\n");
        } else if (argv[i][0] != '-') {
            // Assume this is the ROM filename
            romFilename = argv[i];
        }
    }
    
    // Check for ROM file argument
    if (!romFilename) {
        printf("Usage: %s [--neszapper] <rom_file.nes>\n", argv[0]);
        printf("Example: %s --neszapper duckhunt.nes\n", argv[0]);
        printf("Options:\n");
        printf("  --neszapper    Enable NES Zapper (light gun) support\n");
        return -1;
    }
    
    // Validate ROM file exists
    FILE* romTest = fopen(romFilename, "rb");
    if (!romTest) {
        printf("Error: Cannot open ROM file '%s'\n", romFilename);
        printf("Please check the file path and try again\n");
        return -1;
    }
    fclose(romTest);
    
    // Show Zapper status
    if (zapperEnabled) {
        printf("=== NES ZAPPER ENABLED ===\n");
        printf("Mouse controls:\n");
        printf("  - Move mouse to aim\n");
        printf("  - Left click to fire\n");
        printf("  - Red crosshair shows aim point\n");
        printf("Compatible games: Duck Hunt, Wild Gunman, Hogan's Alley\n");
        printf("========================\n");
    }
    
    // Add ROM analysis before starting
    debugMapperInfo(romFilename);
    
    printf("ROM file: %s\n", romFilename);
    printf("Initializing...\n");
    
    // Initialize Configuration first (CRITICAL for sound)
    Configuration::initialize(CONFIG_FILE_NAME);
    
    printf("Configuration loaded:\n");
    printf("Audio enabled: %s\n", Configuration::getAudioEnabled() ? "Yes" : "No");
    printf("Audio frequency: %d Hz\n", Configuration::getAudioFrequency());
    printf("Frame rate: %d FPS\n", Configuration::getFrameRate());
    
    AllegroMainWindow mainWindow;
    
    if (!mainWindow.initialize()) {
        printf("Failed to initialize\n");
        return -1;
    }
    
    printf("Starting game with ROM: %s\n", romFilename);
    mainWindow.run(romFilename);  // Pass ROM filename to run method
    
    return 0;
}
END_OF_MAIN()
