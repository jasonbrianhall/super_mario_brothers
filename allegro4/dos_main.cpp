#include <cstdio>
#include <iostream>
#include <cstring>

#include <allegro.h>

#include "Emulation/Controller.hpp"
#include "SMB/SMBEngine.hpp"
#include "Util/Video.hpp"

#include "Configuration.hpp"
#include "Constants.hpp"
#include "Util/VideoFilters.hpp"
// Include the generated ROM header
#include "SMBRom.hpp" // This contains smbRomData array

// Global variables for the game engine
static SMBEngine* smbEngine = nullptr;
static uint32_t renderBuffer[RENDER_WIDTH * RENDER_HEIGHT];
static uint32_t filteredBuffer[RENDER_WIDTH * RENDER_HEIGHT];
static uint32_t prevFrameBuffer[RENDER_WIDTH * RENDER_HEIGHT];

// Allegro graphics objects
static BITMAP* gameScreen = nullptr;
static BITMAP* backBuffer = nullptr;

// Audio variables
static bool audioEnabled = false;
static int audioFrequency = 44100;

/**
 * Simple audio callback for Allegro
 * Note: Allegro 4 audio is different from SDL - this is a simplified approach
 */
void dosAudioCallback() {
    // TODO: Implement Allegro 4 audio streaming
    // For now, we'll run without audio to get graphics working first
}

/**
 * Convert SDL scancodes to Allegro key indices
 * This maintains compatibility with your existing Configuration system
 */
int sdlScancodeToAllegroKey(int scancode) {
    // Map common SDL scancodes to Allegro key array indices
    switch (scancode) {
        case 82:  return KEY_UP;        // SDL_SCANCODE_UP
        case 81:  return KEY_DOWN;      // SDL_SCANCODE_DOWN  
        case 80:  return KEY_LEFT;      // SDL_SCANCODE_LEFT
        case 79:  return KEY_RIGHT;     // SDL_SCANCODE_RIGHT
        case 27:  return KEY_X;         // SDL_SCANCODE_X
        case 29:  return KEY_Z;         // SDL_SCANCODE_Z
        case 229: return KEY_RSHIFT;    // SDL_SCANCODE_RSHIFT
        case 40:  return KEY_ENTER;     // SDL_SCANCODE_RETURN
        case 12:  return KEY_I;         // SDL_SCANCODE_I
        case 14:  return KEY_K;         // SDL_SCANCODE_K
        case 13:  return KEY_J;         // SDL_SCANCODE_J
        case 15:  return KEY_L;         // SDL_SCANCODE_L
        case 17:  return KEY_N;         // SDL_SCANCODE_N
        case 16:  return KEY_M;         // SDL_SCANCODE_M
        case 228: return KEY_RCONTROL;  // SDL_SCANCODE_RCTRL
        case 44:  return KEY_SPACE;     // SDL_SCANCODE_SPACE
        default:  return -1;
    }
}

/**
 * Initialize DOS graphics and input systems.
 */
static bool initialize() {
    // Initialize Allegro
    if (allegro_init() != 0) {
        std::cout << "Failed to initialize Allegro!" << std::endl;
        return false;
    }
    
    // Install input systems
    if (install_keyboard() < 0) {
        std::cout << "Failed to install keyboard!" << std::endl;
        return false;
    }
    
    // Install joystick support
    if (install_joystick() < 0) {
        std::cout << "Warning: Failed to install joystick support" << std::endl;
        // Continue without joystick - not critical
    }
    
    // Install timer
    if (install_timer() < 0) {
        std::cout << "Failed to install timer!" << std::endl;
        return false;
    }
    
    // Set color depth to 32-bit (same as your SDL version)
    set_color_depth(32);
    
    // Try to set graphics mode - start with a reasonable resolution
    // We'll scale the NES resolution (256x240) up to 640x480 (2.5x scale)
    if (set_gfx_mode(GFX_AUTODETECT, 640, 480, 0, 0) != 0) {
        // If that fails, try 320x240 (1:1 scale)
        if (set_gfx_mode(GFX_AUTODETECT, 320, 240, 0, 0) != 0) {
            std::cout << "Failed to set graphics mode: " << allegro_error << std::endl;
            return false;
        }
    }
    
    std::cout << "Graphics mode set: " << SCREEN_W << "x" << SCREEN_H << " at " << bitmap_color_depth(screen) << " bpp" << std::endl;
    
    // Create our game rendering surface
    gameScreen = create_bitmap(RENDER_WIDTH, RENDER_HEIGHT);
    if (!gameScreen) {
        std::cout << "Failed to create game screen bitmap!" << std::endl;
        return false;
    }
    
    // Create back buffer for smooth rendering
    backBuffer = create_bitmap(SCREEN_W, SCREEN_H);
    if (!backBuffer) {
        std::cout << "Failed to create back buffer!" << std::endl;
        return false;
    }
    
    // Clear both buffers to black
    clear_to_color(gameScreen, makecol(0, 0, 0));
    clear_to_color(backBuffer, makecol(0, 0, 0));
    clear_to_color(screen, makecol(0, 0, 0));
    
    // Load configuration (this uses your existing system!)
    Configuration::initialize(CONFIG_FILE_NAME);
    
    // Initialize video filters if enabled
    if (Configuration::getHqdn3dEnabled()) {
        initHQDN3D(RENDER_WIDTH, RENDER_HEIGHT);
        memset(prevFrameBuffer, 0, RENDER_WIDTH * RENDER_HEIGHT * sizeof(uint32_t));
        std::cout << "HQDN3D filter initialized" << std::endl;
    }
    
    // Audio initialization (simplified for now)
    audioEnabled = Configuration::getAudioEnabled();
    audioFrequency = Configuration::getAudioFrequency();
    
    if (audioEnabled) {
        if (install_sound(DIGI_AUTODETECT, MIDI_NONE, NULL) != 0) {
            std::cout << "Warning: Failed to initialize audio: " << allegro_error << std::endl;
            std::cout << "Continuing without audio..." << std::endl;
            audioEnabled = false;
        } else {
            std::cout << "Audio initialized successfully" << std::endl;
        }
    }
    
    return true;
}

/**
 * Shutdown and cleanup.
 */
static void shutdown() {
    // Cleanup video filters
    if (Configuration::getHqdn3dEnabled()) {
        cleanupHQDN3D();
    }
    
    // Destroy bitmaps
    if (gameScreen) {
        destroy_bitmap(gameScreen);
    }
    if (backBuffer) {
        destroy_bitmap(backBuffer);
    }
    
    // Shutdown Allegro
    allegro_exit();
}

/**
 * Handle keyboard input using your existing configuration system
 */
void handleKeyboardInput(Controller& controller) {
    // Player 1 controls - use your configuration values!
    int p1_up = sdlScancodeToAllegroKey(Configuration::getPlayer1KeyUp());
    int p1_down = sdlScancodeToAllegroKey(Configuration::getPlayer1KeyDown());
    int p1_left = sdlScancodeToAllegroKey(Configuration::getPlayer1KeyLeft());
    int p1_right = sdlScancodeToAllegroKey(Configuration::getPlayer1KeyRight());
    int p1_a = sdlScancodeToAllegroKey(Configuration::getPlayer1KeyA());
    int p1_b = sdlScancodeToAllegroKey(Configuration::getPlayer1KeyB());
    int p1_select = sdlScancodeToAllegroKey(Configuration::getPlayer1KeySelect());
    int p1_start = sdlScancodeToAllegroKey(Configuration::getPlayer1KeyStart());
    
    // Set button states based on configuration
    if (p1_up >= 0 && key[p1_up])     controller.setButtonState(PLAYER_1, BUTTON_UP, true);
    else controller.setButtonState(PLAYER_1, BUTTON_UP, false);
    
    if (p1_down >= 0 && key[p1_down]) controller.setButtonState(PLAYER_1, BUTTON_DOWN, true);
    else controller.setButtonState(PLAYER_1, BUTTON_DOWN, false);
    
    if (p1_left >= 0 && key[p1_left]) controller.setButtonState(PLAYER_1, BUTTON_LEFT, true);
    else controller.setButtonState(PLAYER_1, BUTTON_LEFT, false);
    
    if (p1_right >= 0 && key[p1_right]) controller.setButtonState(PLAYER_1, BUTTON_RIGHT, true);
    else controller.setButtonState(PLAYER_1, BUTTON_RIGHT, false);
    
    if (p1_a >= 0 && key[p1_a])       controller.setButtonState(PLAYER_1, BUTTON_A, true);
    else controller.setButtonState(PLAYER_1, BUTTON_A, false);
    
    if (p1_b >= 0 && key[p1_b])       controller.setButtonState(PLAYER_1, BUTTON_B, true);
    else controller.setButtonState(PLAYER_1, BUTTON_B, false);
    
    if (p1_select >= 0 && key[p1_select]) controller.setButtonState(PLAYER_1, BUTTON_SELECT, true);
    else controller.setButtonState(PLAYER_1, BUTTON_SELECT, false);
    
    if (p1_start >= 0 && key[p1_start]) controller.setButtonState(PLAYER_1, BUTTON_START, true);
    else controller.setButtonState(PLAYER_1, BUTTON_START, false);
    
    // Player 2 controls - same pattern
    int p2_up = sdlScancodeToAllegroKey(Configuration::getPlayer2KeyUp());
    int p2_down = sdlScancodeToAllegroKey(Configuration::getPlayer2KeyDown());
    int p2_left = sdlScancodeToAllegroKey(Configuration::getPlayer2KeyLeft());
    int p2_right = sdlScancodeToAllegroKey(Configuration::getPlayer2KeyRight());
    int p2_a = sdlScancodeToAllegroKey(Configuration::getPlayer2KeyA());
    int p2_b = sdlScancodeToAllegroKey(Configuration::getPlayer2KeyB());
    int p2_select = sdlScancodeToAllegroKey(Configuration::getPlayer2KeySelect());
    int p2_start = sdlScancodeToAllegroKey(Configuration::getPlayer2KeyStart());
    
    if (p2_up >= 0 && key[p2_up])     controller.setButtonState(PLAYER_2, BUTTON_UP, true);
    else controller.setButtonState(PLAYER_2, BUTTON_UP, false);
    
    if (p2_down >= 0 && key[p2_down]) controller.setButtonState(PLAYER_2, BUTTON_DOWN, true);
    else controller.setButtonState(PLAYER_2, BUTTON_DOWN, false);
    
    if (p2_left >= 0 && key[p2_left]) controller.setButtonState(PLAYER_2, BUTTON_LEFT, true);
    else controller.setButtonState(PLAYER_2, BUTTON_LEFT, false);
    
    if (p2_right >= 0 && key[p2_right]) controller.setButtonState(PLAYER_2, BUTTON_RIGHT, true);
    else controller.setButtonState(PLAYER_2, BUTTON_RIGHT, false);
    
    if (p2_a >= 0 && key[p2_a])       controller.setButtonState(PLAYER_2, BUTTON_A, true);
    else controller.setButtonState(PLAYER_2, BUTTON_A, false);
    
    if (p2_b >= 0 && key[p2_b])       controller.setButtonState(PLAYER_2, BUTTON_B, true);
    else controller.setButtonState(PLAYER_2, BUTTON_B, false);
    
    if (p2_select >= 0 && key[p2_select]) controller.setButtonState(PLAYER_2, BUTTON_SELECT, true);
    else controller.setButtonState(PLAYER_2, BUTTON_SELECT, false);
    
    if (p2_start >= 0 && key[p2_start]) controller.setButtonState(PLAYER_2, BUTTON_START, true);
    else controller.setButtonState(PLAYER_2, BUTTON_START, false);
}

/**
 * Handle joystick input using Allegro
 */
void handleJoystickInput(Controller& controller) {
    if (num_joysticks > 0) {
        poll_joystick();
        
        // Handle Player 1 joystick (joystick 0)
        if (joy[0].flags & JOYFLAG_ANALOGUE) {
            // Analog stick with deadzone (use your configuration!)
            int deadzone = Configuration::getJoystickDeadzone() / 4; // Scale to Allegro range
            
            // X axis
            if (joy[0].stick[0].axis[0].pos > deadzone) {
                controller.setButtonState(PLAYER_1, BUTTON_RIGHT, true);
                controller.setButtonState(PLAYER_1, BUTTON_LEFT, false);
            } else if (joy[0].stick[0].axis[0].pos < -deadzone) {
                controller.setButtonState(PLAYER_1, BUTTON_LEFT, true);
                controller.setButtonState(PLAYER_1, BUTTON_RIGHT, false);
            } else {
                controller.setButtonState(PLAYER_1, BUTTON_LEFT, false);
                controller.setButtonState(PLAYER_1, BUTTON_RIGHT, false);
            }
            
            // Y axis
            if (joy[0].stick[0].axis[1].pos > deadzone) {
                controller.setButtonState(PLAYER_1, BUTTON_DOWN, true);
                controller.setButtonState(PLAYER_1, BUTTON_UP, false);
            } else if (joy[0].stick[0].axis[1].pos < -deadzone) {
                controller.setButtonState(PLAYER_1, BUTTON_UP, true);
                controller.setButtonState(PLAYER_1, BUTTON_DOWN, false);
            } else {
                controller.setButtonState(PLAYER_1, BUTTON_UP, false);
                controller.setButtonState(PLAYER_1, BUTTON_DOWN, false);
            }
        }
        
        // Digital buttons - use your configuration!
        int buttonA = Configuration::getPlayer1JoystickButtonA();
        int buttonB = Configuration::getPlayer1JoystickButtonB();
        int buttonStart = Configuration::getPlayer1JoystickButtonStart();
        int buttonSelect = Configuration::getPlayer1JoystickButtonSelect();
        
        if (buttonA < joy[0].num_buttons) {
            controller.setButtonState(PLAYER_1, BUTTON_A, joy[0].button[buttonA].b != 0);
        }
        if (buttonB < joy[0].num_buttons) {
            controller.setButtonState(PLAYER_1, BUTTON_B, joy[0].button[buttonB].b != 0);
        }
        if (buttonStart < joy[0].num_buttons) {
            controller.setButtonState(PLAYER_1, BUTTON_START, joy[0].button[buttonStart].b != 0);
        }
        if (buttonSelect < joy[0].num_buttons) {
            controller.setButtonState(PLAYER_1, BUTTON_SELECT, joy[0].button[buttonSelect].b != 0);
        }
        
        // Handle Player 2 joystick (joystick 1) if available
        if (num_joysticks > 1) {
            // Similar logic for Player 2...
            // (abbreviated for space, but same pattern)
        }
    }
}

/**
 * Render the game to screen
 */
void renderToScreen() {
    // Apply your existing video filters!
    uint32_t* sourceBuffer = renderBuffer;
    uint32_t* targetBuffer = filteredBuffer;
    
    // Apply HQDN3D filter if enabled (your existing code!)
    if (Configuration::getHqdn3dEnabled()) {
        applyHQDN3D(targetBuffer, sourceBuffer, prevFrameBuffer, 
                    RENDER_WIDTH, RENDER_HEIGHT, 
                    Configuration::getHqdn3dSpatialStrength(), 
                    Configuration::getHqdn3dTemporalStrength());
        
        // Store current frame for next time
        memcpy(prevFrameBuffer, sourceBuffer, RENDER_WIDTH * RENDER_HEIGHT * sizeof(uint32_t));
        
        // Swap buffers
        uint32_t* temp = sourceBuffer;
        sourceBuffer = targetBuffer;
        targetBuffer = temp;
    }
    
    // Apply FXAA if enabled (your existing code!)
    if (Configuration::getAntiAliasingEnabled() && Configuration::getAntiAliasingMethod() == 0) {
        applyFXAA(targetBuffer, sourceBuffer, RENDER_WIDTH, RENDER_HEIGHT);
        
        // Swap buffers
        uint32_t* temp = sourceBuffer;
        sourceBuffer = targetBuffer;
        targetBuffer = temp;
    }
    
    // Convert 32-bit RGBA buffer to Allegro bitmap
    for (int y = 0; y < RENDER_HEIGHT; y++) {
        for (int x = 0; x < RENDER_WIDTH; x++) {
            uint32_t pixel = sourceBuffer[y * RENDER_WIDTH + x];
            
            // Extract RGB components (skip alpha for DOS)
            int r = (pixel >> 16) & 0xFF;
            int g = (pixel >> 8) & 0xFF;
            int b = pixel & 0xFF;
            
            // Convert to Allegro color format
            int color = makecol(r, g, b);
            
            putpixel(gameScreen, x, y, color);
        }
    }
    
    // Scale game screen to back buffer with respect for aspect ratio
    if (SCREEN_W >= RENDER_WIDTH * 2 && SCREEN_H >= RENDER_HEIGHT * 2) {
        // 2x or higher scaling
        int scale = std::min(SCREEN_W / RENDER_WIDTH, SCREEN_H / RENDER_HEIGHT);
        int scaled_w = RENDER_WIDTH * scale;
        int scaled_h = RENDER_HEIGHT * scale;
        int offset_x = (SCREEN_W - scaled_w) / 2;
        int offset_y = (SCREEN_H - scaled_h) / 2;
        
        // Clear back buffer to black
        clear_to_color(backBuffer, makecol(0, 0, 0));
        
        // Stretch blit with scaling
        stretch_blit(gameScreen, backBuffer, 
                    0, 0, RENDER_WIDTH, RENDER_HEIGHT,
                    offset_x, offset_y, scaled_w, scaled_h);
    } else {
        // 1:1 scaling or smaller
        int offset_x = (SCREEN_W - RENDER_WIDTH) / 2;
        int offset_y = (SCREEN_H - RENDER_HEIGHT) / 2;
        
        clear_to_color(backBuffer, makecol(0, 0, 0));
        blit(gameScreen, backBuffer, 0, 0, offset_x, offset_y, RENDER_WIDTH, RENDER_HEIGHT);
    }
    
    // Copy back buffer to screen
    blit(backBuffer, screen, 0, 0, 0, 0, SCREEN_W, SCREEN_H);
}

/**
 * Main game loop
 */
static void mainLoop() {
    // Use the embedded ROM data directly (your existing system!)
    SMBEngine engine(smbRomData);
    smbEngine = &engine;
    engine.reset();
    
    // Initialize controller system (your existing code!)
    Controller& controller1 = engine.getController1();
    controller1.loadConfiguration(); // Uses your existing config system!
    
    bool joystickInitialized = false;
    if (num_joysticks > 0) {
        std::cout << "Found " << num_joysticks << " joystick(s)" << std::endl;
        joystickInitialized = true;
    } else {
        std::cout << "No joysticks detected. Using keyboard only." << std::endl;
    }
    
    std::cout << "Game initialized! Press ESC to quit, R to reset." << std::endl;
    
    // Frame timing variables
    int frame = 0;
    int progStartTime = 0;
    int targetFrameRate = Configuration::getFrameRate();
    
    // Main game loop
    while (!key[KEY_ESC]) {
        // Handle input
        handleKeyboardInput(controller1);
        
        if (joystickInitialized) {
            handleJoystickInput(controller1);
        }
        
        // Debug key - print controller state
        if (key[KEY_D]) {
            controller1.printButtonStates();
            rest(100); // Prevent spam
        }
        
        // Reset game
        if (key[KEY_R]) {
            engine.reset();
            std::cout << "Game reset!" << std::endl;
            rest(100); // Prevent spam
        }
        
        // Update game logic (your existing engine!)
        engine.update();
        engine.render(renderBuffer);
        
        // Render to screen with filters
        renderToScreen();
        
        // Frame rate limiting (similar to your SDL version)
        int now = retrace_count; // Allegro's frame counter
        int delay = progStartTime + (frame * 60) / targetFrameRate - now;
        
        if (delay > 0) {
            rest(delay * (1000/60)); // Convert to milliseconds
        } else {
            frame = 0;
            progStartTime = now;
        }
        frame++;
        
        // Yield CPU to prevent 100% usage
        if (delay <= 0) {
            rest(1);
        }
    }
}

/**
 * DOS main function
 */
int main(int argc, char** argv) {
    std::cout << "Super Mario Bros DOS Emulator" << std::endl;
    std::cout << "DJGPP + Allegro 4 Version" << std::endl;
    std::cout << "==============================" << std::endl;
    
    if (!initialize()) {
        std::cout << "Failed to initialize. Check error messages above." << std::endl;
        std::cout << "Press any key to exit..." << std::endl;
        readkey();
        return -1;
    }
    
    std::cout << "Initialization successful!" << std::endl;
    std::cout << "Starting game..." << std::endl;
    
    mainLoop();
    
    shutdown();
    
    std::cout << "Game exited normally. Goodbye!" << std::endl;
    return 0;
}
END_OF_MAIN() // Required by Allegro
