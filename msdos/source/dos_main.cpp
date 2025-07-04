#include <stdio.h>
#include <stdlib.h>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include "AllegroMainWindow.hpp"
#include "SMB/SMBEngine.hpp"
#include "Emulation/Controller.hpp"
#include "Configuration.hpp"
#include "Constants.hpp"
#include "SMBRom.hpp"

// DOS-specific includes (will be ignored on Linux)
#ifdef __DJGPP__
#include <conio.h>
#include <dos.h>
#include <pc.h>
#endif

// Global variables for DOS compatibility
static SMBEngine* smbEngine = NULL;
static uint32_t renderBuffer[RENDER_WIDTH * RENDER_HEIGHT];
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
    if (!smbEngine || !Configuration::getAudioEnabled()) {
        // Fill with silence
        memset(buffer, 128, len);
        return;
    }
    
    // Get audio data from the SMB engine
    smbEngine->audioCallback((uint8_t*)buffer, len);
}
END_OF_FUNCTION(audio_stream_callback)

AllegroMainWindow::AllegroMainWindow() 
    : game_buffer(NULL), back_buffer(NULL), gameRunning(false), gamePaused(false), 
      showingMenu(false), selectedMenuItem(0), inMenu(false),
      isCapturingInput(false), currentDialog(DIALOG_NONE),
      statusMessageTimer(0), currentFrameBuffer(NULL),
      screenBuffer16(NULL), useDirectRendering(true)
{
    strcpy(statusMessage, "Ready");
    strcpy(currentCaptureKey, "");
    menuCount = 0;
    g_mainWindow = this;
    
    // Initialize default controls
    setupDefaultControls();
}

AllegroMainWindow::~AllegroMainWindow() 
{
    if (screenBuffer16) {
        delete[] screenBuffer16;
        screenBuffer16 = NULL;
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
    
    if (!initializeInput()) {
        return false;
    }
    
    setupMenu();
    loadControlConfig();
    
    setStatusMessage("Super Mario Bros Virtualizer - Press ESC for menu");
    
    return true;
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
    
    // Install keyboard
    if (install_keyboard() < 0) {
        printf("Failed to install keyboard\n");
        return false;
    }
    printf("Keyboard installed\n");
    
    // Install timer
    if (install_timer() < 0) {
        printf("Failed to install timer\n");
        return false;
    }
    printf("Timer installed\n");
    
    // Install joystick (optional, don't fail if not present)
    if (install_joystick(JOY_TYPE_AUTODETECT) == 0) {
        printf("Joystick installed\n");
    } else {
        printf("No joystick detected\n");
    }
    
    // Initialize audio flag
    dosAudioInitialized = false;
    
    // Install sound if audio is enabled
    if (Configuration::getAudioEnabled()) {
        printf("Attempting to initialize audio...\n");
        
        #ifdef __DJGPP__
        // DOS: Try simple autodetect first - safest approach
        if (install_sound(DIGI_AUTODETECT, MIDI_NONE, NULL) == 0) {
            printf("Audio initialized successfully with autodetect\n");
            dosAudioInitialized = true;
        } else {
            printf("Autodetect failed, trying specific drivers...\n");
            
            // Try Sound Blaster 16 (most compatible)
            if (install_sound(DIGI_SB16, MIDI_NONE, NULL) == 0) {
                printf("Audio initialized with SB16\n");
                dosAudioInitialized = true;
            } else if (install_sound(DIGI_SBPRO, MIDI_NONE, NULL) == 0) {
                printf("Audio initialized with SB Pro\n");
                dosAudioInitialized = true;
            } else if (install_sound(DIGI_SB20, MIDI_NONE, NULL) == 0) {
                printf("Audio initialized with SB 2.0\n");
                dosAudioInitialized = true;
            } else {
                printf("All audio drivers failed: %s\n", allegro_error);
                printf("Continuing without sound...\n");
                dosAudioInitialized = false;
            }
        }
        
        if (dosAudioInitialized) {
            printf("Audio frequency: %d Hz\n", Configuration::getAudioFrequency());
            
            // Set safe volume levels
            set_volume(96, 0);  // Lower volume to reduce distortion
            
            // Reserve a few voices for smoother playback
            reserve_voices(2, 0);  // Just 2 voices for safety
            
            printf("Audio initialization complete\n");
        }
        #else
        // Linux audio code (simplified)
        if (install_sound(DIGI_AUTODETECT, MIDI_NONE, NULL) == 0) {
            printf("Linux audio initialized\n");
        } else {
            printf("Linux audio failed, continuing without sound\n");
        }
        #endif
    } else {
        printf("Audio disabled in configuration\n");
    }
    
    // Lock timer function for DOS
    LOCK_VARIABLE(timer_counter);
    LOCK_FUNCTION(timer_callback);
    
    // Install timer at configured frame rate
    if (install_int_ex(timer_callback, BPS_TO_TIMER(Configuration::getFrameRate())) < 0) {
        printf("Failed to install timer interrupt\n");
        return false;
    }
    
    printf("Timer interrupt installed at %d FPS\n", Configuration::getFrameRate());
    
    return true;
}


bool AllegroMainWindow::initializeGraphics()
{
    // Set graphics mode - different approach for Linux vs DOS
    set_color_depth(16); // 16-bit color for better DOS compatibility
    
    #ifdef __DJGPP__
    // DOS: Use optimal resolutions for perfect NES scaling
    // NES is 256x240, so these give us perfect integer scaling:
    // 512x480 (2x), 768x720 (3x), 320x240 (1.25x), 320x200 (VGA Mode 13h compatible)
    
    if (set_gfx_mode(GFX_AUTODETECT, 512, 480, 0, 0) != 0) {        // 2x scaling
        if (set_gfx_mode(GFX_AUTODETECT, 320, 240, 0, 0) != 0) {    // 1.25x scaling  
            if (set_gfx_mode(GFX_AUTODETECT, 320, 200, 0, 0) != 0) { // VGA Mode 13h style
                if (set_gfx_mode(GFX_AUTODETECT, 640, 480, 0, 0) != 0) { // Fallback
                    printf("Failed to set graphics mode: %s\n", allegro_error);
                    return false;
                }
            }
        }
    }
    #else
    // Linux: Force windowed mode for development
    if (set_gfx_mode(GFX_AUTODETECT_WINDOWED, 640, 480, 0, 0) != 0) {
        if (set_gfx_mode(GFX_AUTODETECT_WINDOWED, 512, 384, 0, 0) != 0) {
            if (set_gfx_mode(GFX_AUTODETECT_WINDOWED, 320, 240, 0, 0) != 0) {
                printf("Failed to set windowed graphics mode: %s\n", allegro_error);
                return false;
            }
        }
    }
    #endif
    
    printf("Graphics mode: %dx%d %s\n", SCREEN_W, SCREEN_H, 
           #ifdef __DJGPP__ 
           "(fullscreen)"
           #else
           "(windowed)"
           #endif
    );
    
    // Calculate optimal scaling info
    int scale_x = SCREEN_W / 256;
    int scale_y = SCREEN_H / 240;
    int optimal_scale = (scale_x < scale_y) ? scale_x : scale_y;
    if (optimal_scale < 1) optimal_scale = 1;
    
    printf("NES scaling: %dx (NES 256x240 -> %dx%d)\n", 
           optimal_scale, 256 * optimal_scale, 240 * optimal_scale);
    
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

bool AllegroMainWindow::initializeInput()
{
    // Clear keyboard buffer
    clear_keybuf();
    
    // Poll joystick to get initial state
    if (num_joysticks > 0) {
        poll_joystick();
        printf("Joystick detected: %d joysticks found\n", num_joysticks);
    }
    
    return true;
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
    player1Keys.start = KEY_ENTER;
    player1Keys.select = KEY_SPACE;
    
    // Default keyboard controls - Player 2
    player2Keys.up = KEY_W;
    player2Keys.down = KEY_S;
    player2Keys.left = KEY_A;
    player2Keys.right = KEY_D;
    player2Keys.button_a = KEY_G;
    player2Keys.button_b = KEY_F;
    player2Keys.start = KEY_T;
    player2Keys.select = KEY_R;
    
    // Default joystick controls
    player1Joy.button_a = 0;
    player1Joy.button_b = 1;
    player1Joy.start = 2;
    player1Joy.select = 3;
    player1Joy.use_stick = true;
    
    player2Joy.button_a = 0;
    player2Joy.button_b = 1;
    player2Joy.start = 2;
    player2Joy.select = 3;
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
    
    strcpy(mainMenu[menuCount].text, "Player 1 Controls");
    mainMenu[menuCount].id = 3;
    mainMenu[menuCount].enabled = true;
    menuCount++;
    
    strcpy(mainMenu[menuCount].text, "Player 2 Controls");
    mainMenu[menuCount].id = 4;
    mainMenu[menuCount].enabled = true;
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

void AllegroMainWindow::run() 
{
    printf("=== Starting Super Mario Bros ===\n");
    
    // Initialize the SMB engine
    SMBEngine engine(const_cast<unsigned char*>(smbRomData));
    smbEngine = &engine;
    engine.reset();
    
    // Initialize wave file for debugging ONLY if audio is working
    if (Configuration::getAudioEnabled() && dosAudioInitialized) {
        initWaveFile();
        printf("Wave file debugging enabled\n");
    }
    
    gameRunning = true;
    setStatusMessage("Game started - Press ESC for menu");
    
    printf("Starting main game loop...\n");
    printf("Resolution: %dx%d\n", SCREEN_W, SCREEN_H);
    printf("Audio: %s\n", dosAudioInitialized ? "Enabled" : "Disabled");
    
    // Simple audio variables
    static uint8_t audioBuffer[1024];  // Smaller buffer for safety
    static int frameCounter = 0;
    
    // Main game loop
    while (gameRunning) {
        // Wait for timer tick
        #ifdef __DJGPP__
        while (timer_counter == 0) {
            rest(1);  // Give CPU time to other processes
        }
        timer_counter = 0;  // Reset counter (simplified)
        #else
        rest(1000 / Configuration::getFrameRate());
        #endif
        
        handleInput();
        
        if (!gamePaused && !showingMenu && currentDialog == DIALOG_NONE) {
            // Update game
            engine.update();
            
            // Simple audio that was working before
            if (dosAudioInitialized && Configuration::getAudioEnabled()) {
                frameCounter++;
                
                int samplesNeeded = Configuration::getAudioFrequency() / Configuration::getFrameRate();
                if (samplesNeeded > 1024) samplesNeeded = 1024;
                if (samplesNeeded < 100) samplesNeeded = 100;
                
                // Get audio from SMB engine
                engine.audioCallback(audioBuffer, samplesNeeded);
                
                #ifdef __DJGPP__
                // Create temporary sample and play it
                SAMPLE* tempSample = create_sample(8, 0, Configuration::getAudioFrequency(), samplesNeeded);
                if (tempSample && tempSample->data) {
                    // Convert unsigned to signed
                    int8_t* sampleData = (int8_t*)tempSample->data;
                    for (int i = 0; i < samplesNeeded; i++) {
                        sampleData[i] = (int8_t)(audioBuffer[i] - 128);
                    }
                    
                    // Play sample at low volume to prevent distortion
                    play_sample(tempSample, 64, 128, 1000, 0);
                    
                    // Clean up immediately
                    destroy_sample(tempSample);
                }
                #endif
            }
            
            // Render the frame
            engine.render(renderBuffer);
            currentFrameBuffer = renderBuffer;
        }
        
        updateAndDraw();
        
        // Emergency exit for Linux
        #ifndef __DJGPP__
        if (key[KEY_ALT] && key[KEY_F4]) {
            gameRunning = false;
        }
        #endif
    }
    
    // Cleanup audio properly
    if (dosAudioInitialized) {
        printf("Cleaning up audio...\n");
    }
    
    // Close wave file when done
    if (enableWaveOutput) {
        closeWaveFile();
    }
    
    printf("Game loop ended normally\n");
}

void AllegroMainWindow::handleInput()
{
    // Poll input devices
    poll_keyboard();
    if (num_joysticks > 0) {
        poll_joystick();
    }
    
    if (currentDialog != DIALOG_NONE) {
        handleDialogInput();
    } else if (showingMenu) {
        handleMenuInput();
    } else {
        handleGameInput();
    }
    
    updateStatusMessage();
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
        }
    }
}

void AllegroMainWindow::handleGameInput()
{
    // Check for menu toggle
    static bool escPressed = false;
    if (key[KEY_ESC] && !escPressed) {
        showingMenu = true;
        escPressed = true;
        return;
    }
    if (!key[KEY_ESC]) {
        escPressed = false;
    }
    
    // Check for pause
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
    
    // Check for reset
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
    
    // Process player input
    checkPlayerInput(PLAYER_1);
    checkPlayerInput(PLAYER_2);
}

void AllegroMainWindow::checkPlayerInput(Player player)
{
    if (!smbEngine) return;
    
    poll_keyboard();
    
    if (player == PLAYER_1) {
        Controller& controller = smbEngine->getController1();
        
        bool up = (key[KEY_UP] != 0);
        bool down = (key[KEY_DOWN] != 0);
        bool left = (key[KEY_LEFT] != 0);
        bool right = (key[KEY_RIGHT] != 0);
        bool a = (key[KEY_X] != 0);
        bool b = (key[KEY_Z] != 0);
        bool start = (key[KEY_ENTER] != 0);
        bool select = (key[KEY_SPACE] != 0);
        
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
        
        bool up = (key[KEY_W] != 0);
        bool down = (key[KEY_S] != 0);
        bool left = (key[KEY_A] != 0);
        bool right = (key[KEY_D] != 0);
        bool a = (key[KEY_G] != 0);
        bool b = (key[KEY_F] != 0);
        bool start = (key[KEY_T] != 0);
        bool select = (key[KEY_R] != 0);
        
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
    
    #ifdef __DJGPP__
    vsync();
    #endif
}

void AllegroMainWindow::drawGame(BITMAP* target)
{
    if (!smbEngine) return;
    
    // For now, force the buffered method which ensures proper scaling
    drawGameBuffered(target);
}

void AllegroMainWindow::drawGameDirect(BITMAP* target)
{
    // Clear the 16-bit buffer
    memset(screenBuffer16, 0, SCREEN_W * SCREEN_H * sizeof(uint16_t));
    
    // Render directly to screen-sized buffer with automatic scaling/centering
    smbEngine->renderDirect(screenBuffer16, SCREEN_W, SCREEN_H);
    
    // Convert 16-bit buffer to Allegro bitmap format with proper scaling
    convertBuffer16ToBitmapScaled(screenBuffer16, target, SCREEN_W, SCREEN_H);
}

void AllegroMainWindow::drawGameBuffered(BITMAP* target)
{
    // Get NES frame in 16-bit format
    static uint16_t nesBuffer[256 * 240];
    smbEngine->render16(nesBuffer);
    
    // Calculate proper scaling
    int scale_x = SCREEN_W / 256;
    int scale_y = SCREEN_H / 240;
    int scale = (scale_x < scale_y) ? scale_x : scale_y;
    
    if (scale < 1) scale = 1;
    
    int dest_w = 256 * scale;
    int dest_h = 240 * scale;
    int dest_x = (SCREEN_W - dest_w) / 2;
    int dest_y = (SCREEN_H - dest_h) / 2;
    
    // Clear target first
    clear_to_color(target, makecol(0, 0, 0));
    
    #ifdef __DJGPP__
    // DOS: Optimized paths for common resolutions
    if (SCREEN_W == 512 && SCREEN_H == 480) {
        // Perfect 2x scaling - fill the screen
        convertBuffer16ToBitmap16(nesBuffer, target, 0, 0, 512, 480, 2);
    } else if (SCREEN_W == 320 && SCREEN_H == 240) {
        // Perfect 1.25x scaling - center it
        convertBuffer16ToBitmap16(nesBuffer, target, 32, 0, 256, 240, 1);
    } else if (SCREEN_W == 320 && SCREEN_H == 200) {
        // VGA Mode 13h style - center and fit
        convertBuffer16ToBitmap16(nesBuffer, target, 32, -20, 256, 240, 1);
    } else {
        // Generic scaling
        if (bitmap_color_depth(target) == 16) {
            convertBuffer16ToBitmap16(nesBuffer, target, dest_x, dest_y, dest_w, dest_h, scale);
        } else {
            convertBuffer16ToBitmapGeneric(nesBuffer, target, dest_x, dest_y, dest_w, dest_h, scale);
        }
    }
    #else
    // Linux: Use generic scaling
    if (bitmap_color_depth(target) == 16) {
        convertBuffer16ToBitmap16(nesBuffer, target, dest_x, dest_y, dest_w, dest_h, scale);
    } else {
        convertBuffer16ToBitmapGeneric(nesBuffer, target, dest_x, dest_y, dest_w, dest_h, scale);
    }
    #endif
}

// Fast conversion from 16-bit buffer to Allegro bitmap with efficient scaling
void AllegroMainWindow::convertBuffer16ToBitmap(uint16_t* buffer16, BITMAP* bitmap, int width, int height)
{
    // Calculate scale factor
    int scale_x = SCREEN_W / 256;
    int scale_y = SCREEN_H / 240;
    int scale = (scale_x < scale_y) ? scale_x : scale_y;
    if (scale < 1) scale = 1;
    
    // Calculate destination dimensions and centering
    int dest_w = 256 * scale;
    int dest_h = 240 * scale;
    int dest_x = (SCREEN_W - dest_w) / 2;
    int dest_y = (SCREEN_H - dest_h) / 2;
    
    // Clear bitmap first
    clear_to_color(bitmap, makecol(0, 0, 0));
    
    if (bitmap_color_depth(bitmap) == 16) {
        // 16-bit bitmap - optimized scaling
        convertBuffer16ToBitmap16(buffer16, bitmap, dest_x, dest_y, dest_w, dest_h, scale);
    } else {
        // Other color depths
        convertBuffer16ToBitmapGeneric(buffer16, bitmap, dest_x, dest_y, dest_w, dest_h, scale);
    }
}

// Optimized 16-bit to 16-bit scaling
void AllegroMainWindow::convertBuffer16ToBitmap16(uint16_t* buffer16, BITMAP* bitmap, 
                                                 int dest_x, int dest_y, int dest_w, int dest_h, int scale)
{
    if (scale == 1) {
        // 1:1 copy - fastest
        for (int y = 0; y < 240; y++) {
            uint16_t* src_row = &buffer16[y * 256];
            uint16_t* dest_row = (uint16_t*)bitmap->line[y + dest_y];
            memcpy(&dest_row[dest_x], src_row, 256 * sizeof(uint16_t));
        }
    } else if (scale == 2) {
        // 2x scaling - optimized
        for (int y = 0; y < 240; y++) {
            uint16_t* src_row = &buffer16[y * 256];
            uint16_t* dest_row1 = (uint16_t*)bitmap->line[y * 2 + dest_y];
            uint16_t* dest_row2 = (uint16_t*)bitmap->line[y * 2 + 1 + dest_y];
            
            for (int x = 0; x < 256; x++) {
                uint16_t pixel = src_row[x];
                int dest_pos = (x * 2) + dest_x;
                dest_row1[dest_pos] = pixel;
                dest_row1[dest_pos + 1] = pixel;
                dest_row2[dest_pos] = pixel;
                dest_row2[dest_pos + 1] = pixel;
            }
        }
    } else if (scale == 3) {
        // 3x scaling - optimized
        for (int y = 0; y < 240; y++) {
            uint16_t* src_row = &buffer16[y * 256];
            uint16_t* dest_row1 = (uint16_t*)bitmap->line[y * 3 + dest_y];
            uint16_t* dest_row2 = (uint16_t*)bitmap->line[y * 3 + 1 + dest_y];
            uint16_t* dest_row3 = (uint16_t*)bitmap->line[y * 3 + 2 + dest_y];
            
            for (int x = 0; x < 256; x++) {
                uint16_t pixel = src_row[x];
                int dest_pos = (x * 3) + dest_x;
                dest_row1[dest_pos] = pixel;
                dest_row1[dest_pos + 1] = pixel;
                dest_row1[dest_pos + 2] = pixel;
                dest_row2[dest_pos] = pixel;
                dest_row2[dest_pos + 1] = pixel;
                dest_row2[dest_pos + 2] = pixel;
                dest_row3[dest_pos] = pixel;
                dest_row3[dest_pos + 1] = pixel;
                dest_row3[dest_pos + 2] = pixel;
            }
        }
    } else {
        // Generic scaling for other factors
        for (int y = 0; y < 240; y++) {
            uint16_t* src_row = &buffer16[y * 256];
            
            for (int scale_y = 0; scale_y < scale; scale_y++) {
                int dest_row_idx = y * scale + scale_y + dest_y;
                if (dest_row_idx >= SCREEN_H) break;
                
                uint16_t* dest_row = (uint16_t*)bitmap->line[dest_row_idx];
                
                for (int x = 0; x < 256; x++) {
                    uint16_t pixel = src_row[x];
                    int dest_start = x * scale + dest_x;
                    
                    for (int scale_x = 0; scale_x < scale; scale_x++) {
                        int dest_pos = dest_start + scale_x;
                        if (dest_pos < SCREEN_W) {
                            dest_row[dest_pos] = pixel;
                        }
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
            convertBuffer16ToBitmapGeneric(nesBuffer, bitmap, dest_x, dest_y, dest_w, dest_h, scale);
        }
    }
}

// Alternative ultra-fast version for systems that support it
/*void AllegroMainWindow::drawGameUltraFast(BITMAP* target)
{
    // This version requires the screen bitmap to be 16-bit and accessible
    if (bitmap_color_depth(target) == 16) {
        // Get pointer to screen memory
        uint16_t* screenMem = (uint16_t*)target->line[0];
        
        // Render directly to screen memory with centering
        smbEngine->renderDirectFast(screenMem, SCREEN_W, SCREEN_H);
    } else {
        // Fall back to buffered method
        drawGameDirect(target);
    }
}*/

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
        smbEngine->renderDirectFast(screenMem, SCREEN_W, SCREEN_H);
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
    int dialog_x = SCREEN_W / 4;
    int dialog_y = SCREEN_H / 4;
    int dialog_w = SCREEN_W / 2;
    int dialog_h = SCREEN_H / 2;
    
    // Draw dialog background
    rectfill(target, dialog_x, dialog_y, dialog_x + dialog_w, dialog_y + dialog_h, 
             makecol(32, 32, 32));
    
    // Draw dialog border
    rect(target, dialog_x, dialog_y, dialog_x + dialog_w, dialog_y + dialog_h, 
         makecol(255, 255, 255));
    
    switch (currentDialog) {
        case DIALOG_ABOUT:
            drawTextCentered(target, dialog_y + 20, "SUPER MARIO BROS Virtualizer", makecol(255, 255, 0));
            drawTextCentered(target, dialog_y + 40, "Version 1.0", makecol(255, 255, 255));
            drawTextCentered(target, dialog_y + 60, "Built with Allegro 4", makecol(255, 255, 255));
            drawTextCentered(target, dialog_y + 100, "Original game (c) Nintendo", makecol(255, 255, 255));
            drawTextCentered(target, dialog_y + dialog_h - 40, "Press ESC to close", makecol(255, 255, 0));
            break;
            
        case DIALOG_HELP:
            drawTextCentered(target, dialog_y + 20, "HELP", makecol(255, 255, 0));
            drawTextCentered(target, dialog_y + 50, "ESC - Show menu", makecol(255, 255, 255));
            drawTextCentered(target, dialog_y + 70, "P - Pause game", makecol(255, 255, 255));
            drawTextCentered(target, dialog_y + 90, "Ctrl+R - Reset game", makecol(255, 255, 255));
            drawTextCentered(target, dialog_y + dialog_h - 40, "Press ESC to close", makecol(255, 255, 0));
            break;
            
        case DIALOG_CONTROLS_P1:
        case DIALOG_CONTROLS_P2:
            drawTextCentered(target, dialog_y + 20, "CONTROL CONFIGURATION", makecol(255, 255, 0));
            drawTextCentered(target, dialog_y + 50, "Not yet implemented", makecol(255, 255, 255));
            drawTextCentered(target, dialog_y + dialog_h - 40, "Press ESC to close", makecol(255, 255, 0));
            break;
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

void AllegroMainWindow::menuSelect()
{
    switch (mainMenu[selectedMenuItem].id) {
        case 1: // Resume Game
            showingMenu = false;
            gamePaused = false;
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
    }
}

void AllegroMainWindow::menuEscape()
{
    showingMenu = false;
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
    // Close wave file first
    closeWaveFile();
    
    // Clean up DOS audio
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
    
    // Save configuration before shutdown
    saveControlConfig();
    
    allegro_exit();
}

void AllegroMainWindow::saveControlConfig()
{
    // Simple file I/O for DOS
    FILE* f = fopen("controls.cfg", "wb");
    if (f) {
        fwrite(&player1Keys, sizeof(PlayerKeys), 1, f);
        fwrite(&player2Keys, sizeof(PlayerKeys), 1, f);
        fwrite(&player1Joy, sizeof(PlayerJoy), 1, f);
        fwrite(&player2Joy, sizeof(PlayerJoy), 1, f);
        fclose(f);
    }
}

void AllegroMainWindow::loadControlConfig()
{
    FILE* f = fopen("controls.cfg", "rb");
    if (f) {
        fread(&player1Keys, sizeof(PlayerKeys), 1, f);
        fread(&player2Keys, sizeof(PlayerKeys), 1, f);
        fread(&player1Joy, sizeof(PlayerJoy), 1, f);
        fread(&player2Joy, sizeof(PlayerJoy), 1, f);
        fclose(f);
    }
}

// Main function for DOS
int main(int argc, char** argv) 
{
    printf("Super Mario Bros Virtualizer - DOS Version\n");
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
    
    printf("Starting game...\n");
    mainWindow.run();
    
    return 0;
}
END_OF_MAIN()
