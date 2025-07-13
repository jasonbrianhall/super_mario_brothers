#include <stdio.h>
#include <stdlib.h>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <allegro.h>
#include "SMB/SMBEmulator.hpp"
#include "Emulation/Controller.hpp"
#include "Configuration.hpp"
#include "Constants.hpp"

// DOS-specific includes (will be ignored on Linux)
#ifdef __DJGPP__
#include <conio.h>
#include <dos.h>
#include <pc.h>
#endif

static AUDIOSTREAM* audiostream = NULL;

// Global variables for emulator version
static SMBEmulator* smbEmulator = NULL;
static uint32_t renderBuffer[RENDER_WIDTH * RENDER_HEIGHT];

// Game state
static bool gameRunning = false;
static bool gamePaused = false;
static BITMAP* back_buffer = NULL;

// Audio streaming variables for DOS
static bool dosAudioInitialized = false;

// Timer for frame rate control
volatile int timer_counter = 0;

void timer_callback() {
    timer_counter++;
}
END_OF_FUNCTION(timer_callback)

// Audio callback for Allegro
void audio_stream_callback(void* buffer, int len)
{
    if (!smbEmulator || !Configuration::getAudioEnabled()) {
        memset(buffer, 0, len);
        return;
    }
    
    static uint8_t temp_buffer[4096];
    int actual_len = (len > 4096) ? 4096 : len;
    
    // Get audio from emulator
    smbEmulator->audioCallback(temp_buffer, actual_len);
    
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

bool initializeAllegro()
{
    printf("Initializing Allegro...\n");
    
    if (allegro_init() != 0) {
        printf("Failed to initialize Allegro\n");
        return false;
    }
    
    if (install_keyboard() < 0) {
        printf("Failed to install keyboard\n");
        return false;
    }
    
    if (install_timer() < 0) {
        printf("Failed to install timer\n");
        return false;
    }
    
    // Initialize graphics
    set_color_depth(16);
    
    #ifdef __DJGPP__
    // DOS: Use VGA mode
    if (set_gfx_mode(GFX_AUTODETECT, 320, 200, 0, 0) != 0) {
        if (set_gfx_mode(GFX_AUTODETECT, 640, 480, 0, 0) != 0) {
            printf("Failed to set DOS graphics mode: %s\n", allegro_error);
            return false;
        }
    }
    #else
    // Linux: Use windowed mode
    if (set_gfx_mode(GFX_AUTODETECT_WINDOWED, 640, 480, 0, 0) != 0) {
        if (set_gfx_mode(GFX_AUTODETECT_WINDOWED, 512, 384, 0, 0) != 0) {
            printf("Failed to set graphics mode: %s\n", allegro_error);
            return false;
        }
    }
    #endif
    
    printf("Graphics mode: %dx%d\n", SCREEN_W, SCREEN_H);
    
    // Create back buffer
    back_buffer = create_bitmap(SCREEN_W, SCREEN_H);
    if (!back_buffer) {
        printf("Failed to create back buffer\n");
        return false;
    }
    
    // DISABLE AUDIO FOR NOW TO ISOLATE THE ISSUE
    printf("Skipping audio initialization for debugging...\n");
    dosAudioInitialized = false;
    
    /*
    // Initialize audio
    dosAudioInitialized = false;
    if (Configuration::getAudioEnabled()) {
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
        }
    }
    */
    
    // Setup timer
    LOCK_VARIABLE(timer_counter);
    LOCK_FUNCTION(timer_callback);
    
    if (install_int_ex(timer_callback, BPS_TO_TIMER(Configuration::getFrameRate())) < 0) {
        printf("Failed to install timer interrupt\n");
        return false;
    }
    
    printf("Allegro initialized successfully\n");
    return true;
}

void handleInput()
{
    poll_keyboard();
    
    // ESC to quit
    static bool escPressed = false;
    if (key[KEY_ESC] && !escPressed) {
        gameRunning = false;
        escPressed = true;
    }
    if (!key[KEY_ESC]) {
        escPressed = false;
    }
    
    // P to pause
    static bool pPressed = false;
    if (key[KEY_P] && !pPressed) {
        gamePaused = !gamePaused;
        printf("Game %s\n", gamePaused ? "Paused" : "Resumed");
        pPressed = true;
    }
    if (!key[KEY_P]) {
        pPressed = false;
    }
    
    // R to reset
    static bool rPressed = false;
    if (key[KEY_R] && !rPressed) {
        if (smbEmulator) {
            smbEmulator->reset();
            printf("Emulator Reset\n");
        }
        rPressed = true;
    }
    if (!key[KEY_R]) {
        rPressed = false;
    }
    
    // Game input (only if not paused)
    if (!gamePaused && smbEmulator) {
        Controller& controller = smbEmulator->getController1();
        
        // Simple keyboard mapping
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
}

void drawGame(BITMAP* target)
{
    if (!smbEmulator) {
        clear_to_color(target, makecol(255, 0, 0)); // Red screen = no emulator
        return;
    }
    
    // Use 16-bit rendering - this is what works!
    static uint16_t render16Buffer[RENDER_WIDTH * RENDER_HEIGHT];
    
    // Render using the working method
    smbEmulator->render16(render16Buffer);
    
    // Convert 16-bit to screen with scaling
    int scale_x = SCREEN_W / RENDER_WIDTH;
    int scale_y = SCREEN_H / RENDER_HEIGHT;
    int scale = std::min(scale_x, scale_y);
    if (scale < 1) scale = 1;
    
    // Calculate centered position
    int dest_w = RENDER_WIDTH * scale;
    int dest_h = RENDER_HEIGHT * scale;
    int dest_x = (SCREEN_W - dest_w) / 2;
    int dest_y = (SCREEN_H - dest_h) / 2;
    
    // Clear screen to black
    clear_to_color(target, makecol(0, 0, 0));
    
    // Draw scaled NES graphics
    for (int y = 0; y < RENDER_HEIGHT; y++) {
        for (int x = 0; x < RENDER_WIDTH; x++) {
            uint16_t pixel16 = render16Buffer[y * RENDER_WIDTH + x];
            
            // Convert RGB565 to RGB888
            int r = ((pixel16 >> 11) & 0x1F) << 3;
            int g = ((pixel16 >> 5) & 0x3F) << 2;
            int b = (pixel16 & 0x1F) << 3;
            
            int color = makecol(r, g, b);
            
            // Draw scaled pixel
            for (int sy = 0; sy < scale; sy++) {
                for (int sx = 0; sx < scale; sx++) {
                    int screen_x = dest_x + x * scale + sx;
                    int screen_y = dest_y + y * scale + sy;
                    
                    if (screen_x >= 0 && screen_x < SCREEN_W && 
                        screen_y >= 0 && screen_y < SCREEN_H) {
                        putpixel(target, screen_x, screen_y, color);
                    }
                }
            }
        }
    }
}

void updateAndDraw()
{
    // Clear back buffer
    clear_to_color(back_buffer, makecol(0, 0, 0));
    
    // Draw game
    drawGame(back_buffer);
    
    // Show pause message
    if (gamePaused) {
        textout_centre(back_buffer, font, "PAUSED - Press P to resume", 
                      SCREEN_W/2, SCREEN_H/2, makecol(255, 255, 255));
    }
    
    // Copy to screen
    blit(back_buffer, screen, 0, 0, 0, 0, SCREEN_W, SCREEN_H);
    
    #ifdef __DJGPP__
    vsync();
    #endif
}

void run()
{
    printf("=== Starting Super Mario Bros Emulator ===\n");
    
    gameRunning = true;
    gamePaused = false;
    
    printf("Controls:\n");
    printf("  Arrow keys - D-pad\n");
    printf("  Z/X - A/B buttons\n");
    printf("  Enter - Start\n");
    printf("  Space - Select\n");
    printf("  P - Pause/Resume\n");
    printf("  R - Reset\n");
    printf("  ESC - Quit\n");
    printf("Starting game...\n");
    
    // Timing variables
    clock_t frameStart, frameEnd;
    int frameCount = 0;
    
    try {
        while (gameRunning) {
            frameStart = clock();
            frameCount++;
            
            handleInput();
            
            if (!gamePaused && smbEmulator) {
                smbEmulator->update();
            }
            
            updateAndDraw();
            
            // Print status every 300 frames (5 seconds)
            if (frameCount % 300 == 0) {
                printf("Running... Frame %d\n", frameCount);
            }
            
            // Frame rate limiting
            frameEnd = clock();
            double frameTime = ((double)(frameEnd - frameStart)) / CLOCKS_PER_SEC * 1000.0;
            double targetFrameTime = 1000.0 / Configuration::getFrameRate();
            double sleepTime = targetFrameTime - frameTime;
            if (sleepTime > 0) {
                rest(sleepTime);
            }
        }
    } catch (...) {
        printf("EXCEPTION in main loop at frame %d!\n", frameCount);
    }
    
    printf("Main loop ended\n");
}

void shutdown()
{
    if (audiostream) {
        stop_audio_stream(audiostream);
        audiostream = NULL;
    }
    
    if (back_buffer) {
        destroy_bitmap(back_buffer);
        back_buffer = NULL;
    }
    
    if (smbEmulator) {
        delete smbEmulator;
        smbEmulator = NULL;
    }
    
    allegro_exit();
}

// Main function
int main(int argc, char** argv) 
{
    printf("Super Mario Bros 6502 Emulator (Standalone)\n");
    
    if (argc < 2) {
        printf("Usage: %s <rom_file.nes>\n", argv[0]);
        printf("Example: %s smb.nes\n", argv[0]);
        return -1;
    }
    
    printf("ROM file: %s\n", argv[1]);
    
    // Initialize configuration
    printf("Initializing configuration...\n");
    Configuration::initialize(CONFIG_FILE_NAME);
    
    printf("Configuration loaded:\n");
    printf("Audio enabled: %s\n", Configuration::getAudioEnabled() ? "Yes" : "No");
    printf("Audio frequency: %d Hz\n", Configuration::getAudioFrequency());
    printf("Frame rate: %d FPS\n", Configuration::getFrameRate());
    
    // Initialize emulator BEFORE Allegro
    printf("Creating SMBEmulator BEFORE Allegro...\n");
    smbEmulator = new SMBEmulator();
    printf("SMBEmulator created successfully\n");
    
    printf("Loading ROM: %s\n", argv[1]);
    if (!smbEmulator->loadROM(argv[1])) {
        printf("Failed to load ROM: %s\n", argv[1]);
        printf("Make sure the file exists and is a valid NES ROM\n");
        return -1;
    }
    printf("ROM loaded successfully\n");
    
    // Test basic emulator functions BEFORE Allegro
    printf("Testing emulator basic functions BEFORE Allegro...\n");
    
    try {
        printf("Testing update()...\n");
        smbEmulator->update();
        printf("update() OK\n");
        
        printf("Testing render16()...\n");
        static uint16_t testBuffer[256 * 240];
        smbEmulator->render16(testBuffer);
        printf("render16() OK\n");
        
        // Check if buffer has data
        bool hasData = false;
        for (int i = 0; i < 1000; i++) {
            if (testBuffer[i] != 0) {
                hasData = true;
                break;
            }
        }
        printf("Render buffer has data: %s\n", hasData ? "Yes" : "No");
        
        printf("Testing controller access...\n");
        Controller& ctrl = smbEmulator->getController1();
        ctrl.setButtonState(BUTTON_A, false);
        printf("Controller access OK\n");
        
    } catch (...) {
        printf("EXCEPTION during emulator testing!\n");
        return -1;
    }
    
    printf("All emulator tests passed BEFORE Allegro!\n");
    
    // Now initialize Allegro
    printf("Now initializing Allegro...\n");
    if (!initializeAllegro()) {
        printf("Failed to initialize Allegro\n");
        return -1;
    }
    printf("Allegro initialization complete\n");
    
    // Test emulator functions AFTER Allegro
    printf("Testing emulator functions AFTER Allegro...\n");
    
    try {
        printf("Testing update() after Allegro...\n");
        smbEmulator->update();
        printf("update() OK after Allegro\n");
        
        printf("Testing render16() after Allegro...\n");
        static uint16_t testBuffer2[256 * 240];
        smbEmulator->render16(testBuffer2);
        printf("render16() OK after Allegro\n");
        
    } catch (...) {
        printf("EXCEPTION during emulator testing AFTER Allegro!\n");
        shutdown();
        return -1;
    }
    
    printf("Emulator still works after Allegro init!\n");
    
    // Test a single frame render to screen
    printf("Testing single frame render to screen...\n");
    try {
        static uint16_t frameBuffer[256 * 240];
        smbEmulator->render16(frameBuffer);
        
        // Convert just a few pixels to screen
        for (int y = 0; y < 10; y++) {
            for (int x = 0; x < 10; x++) {
                uint16_t pixel = frameBuffer[y * 256 + x];
                int r = ((pixel >> 11) & 0x1F) << 3;
                int g = ((pixel >> 5) & 0x3F) << 2;
                int b = (pixel & 0x1F) << 3;
                putpixel(back_buffer, x, y, makecol(r, g, b));
            }
        }
        printf("Single frame render test OK\n");
        
    } catch (...) {
        printf("EXCEPTION during single frame render!\n");
        shutdown();
        return -1;
    }
    
    printf("All tests passed! Starting main loop...\n");
    
    // Run the emulator
    run();
    
    // Cleanup
    shutdown();
    
    return 0;
}
END_OF_MAIN()
