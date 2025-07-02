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

static BITMAP* screen_buffer;
static BITMAP* render_bitmap;
static SMBEngine* smbEngine = nullptr;
static uint32_t renderBuffer[RENDER_WIDTH * RENDER_HEIGHT];
static uint32_t filteredBuffer[RENDER_WIDTH * RENDER_HEIGHT];
static uint32_t prevFrameBuffer[RENDER_WIDTH * RENDER_HEIGHT];
static bool msaaEnabled = false;
static volatile int timer_counter = 0;
static int target_fps = 60;

// Timer callback for frame rate control
void timer_callback()
{
    timer_counter++;
}
END_OF_FUNCTION(timer_callback)

/**
 * Convert 32-bit ARGB color to Allegro RGB format
 */
static int convertColor(uint32_t argb_color)
{
    int r = (argb_color >> 16) & 0xFF;
    int g = (argb_color >> 8) & 0xFF;
    int b = argb_color & 0xFF;
    return makecol(r, g, b);
}

/**
 * Update the screen with the rendered buffer
 */
static void updateScreen()
{
    if (!render_bitmap || !screen_buffer) return;
    
    // Convert the render buffer to Allegro bitmap
    for (int y = 0; y < RENDER_HEIGHT; y++) {
        for (int x = 0; x < RENDER_WIDTH; x++) {
            int color = convertColor(renderBuffer[y * RENDER_WIDTH + x]);
            putpixel(render_bitmap, x, y, color);
        }
    }
    
    // Scale and blit to screen
    int scale = Configuration::getRenderScale();
    stretch_blit(render_bitmap, screen_buffer, 
                 0, 0, RENDER_WIDTH, RENDER_HEIGHT,
                 0, 0, RENDER_WIDTH * scale, RENDER_HEIGHT * scale);
    
    // Apply scanlines if enabled
    if (Configuration::getScanlinesEnabled()) {
        // Simple scanline effect - darken every other line
        for (int y = 1; y < RENDER_HEIGHT * scale; y += 2) {
            hline(screen_buffer, 0, y, RENDER_WIDTH * scale - 1, makecol(0, 0, 0));
        }
    }
    
    blit(screen_buffer, screen, 0, 0, 0, 0, SCREEN_W, SCREEN_H);
}

/**
 * Initialize libraries for use.
 */
static bool initialize()
{
    // Load the configuration
    Configuration::initialize(CONFIG_FILE_NAME);
    target_fps = Configuration::getFrameRate();

    // Initialize Allegro
    if (allegro_init() != 0) {
        std::cout << "Failed to initialize Allegro" << std::endl;
        return false;
    }

    // Install keyboard handler
    if (install_keyboard() != 0) {
        std::cout << "Failed to install keyboard handler" << std::endl;
        return false;
    }

    // Install mouse handler (optional)
    install_mouse();

    // Install joystick handler
    if (install_joystick(JOY_TYPE_AUTODETECT) != 0) {
        std::cout << "Failed to install joystick handler, continuing without joystick support" << std::endl;
    }

    // Install timer
    if (install_timer() != 0) {
        std::cout << "Failed to install timer" << std::endl;
        return false;
    }

    // Set color depth
    set_color_depth(32);

    // Set graphics mode
    int scale = Configuration::getRenderScale();
    int screen_width = RENDER_WIDTH * scale;
    int screen_height = RENDER_HEIGHT * scale;
    
    if (set_gfx_mode(GFX_AUTODETECT_WINDOWED, screen_width, screen_height, 0, 0) != 0) {
        // Try fullscreen if windowed fails
        if (set_gfx_mode(GFX_AUTODETECT, screen_width, screen_height, 0, 0) != 0) {
            std::cout << "Failed to set graphics mode: " << allegro_error << std::endl;
            return false;
        }
    }

    // Set window title
    set_window_title(APP_TITLE);

    // Create bitmaps
    render_bitmap = create_bitmap(RENDER_WIDTH, RENDER_HEIGHT);
    if (!render_bitmap) {
        std::cout << "Failed to create render bitmap" << std::endl;
        return false;
    }

    screen_buffer = create_bitmap(screen_width, screen_height);
    if (!screen_buffer) {
        std::cout << "Failed to create screen buffer" << std::endl;
        return false;
    }

    // Set up custom palette, if configured
    if (!Configuration::getPaletteFileName().empty()) {
        const uint32_t* palette = loadPalette(Configuration::getPaletteFileName());
        if (palette) {
            paletteRGB = palette;
        }
    }

    // Initialize HQDN3D filter if enabled
    if (Configuration::getHqdn3dEnabled()) {
        initHQDN3D(RENDER_WIDTH, RENDER_HEIGHT);
        // Initialize the previous frame buffer
        memset(prevFrameBuffer, 0, RENDER_WIDTH * RENDER_HEIGHT * sizeof(uint32_t));
    }

    // Note: MSAA is not easily supported in Allegro 4, so we'll skip it
    if (Configuration::getAntiAliasingEnabled() && Configuration::getAntiAliasingMethod() == 1) {
        std::cout << "MSAA not supported in Allegro 4, using FXAA instead" << std::endl;
    }

    // Install timer for frame rate control
    LOCK_VARIABLE(timer_counter);
    LOCK_FUNCTION(timer_callback);
    install_int_ex(timer_callback, BPS_TO_TIMER(target_fps));

    // Audio support in Allegro 4 is more limited
    if (Configuration::getAudioEnabled()) {
        if (install_sound(DIGI_AUTODETECT, MIDI_NONE, NULL) != 0) {
            std::cout << "Failed to initialize sound: " << allegro_error << std::endl;
            // Continue without sound
        }
    }

    return true;
}

/**
 * Shutdown libraries for exit.
 */
static void shutdown()
{
    // Cleanup HQDN3D filter if it was initialized
    if (Configuration::getHqdn3dEnabled()) {
        cleanupHQDN3D();
    }

    // Destroy bitmaps
    if (render_bitmap) {
        destroy_bitmap(render_bitmap);
    }
    if (screen_buffer) {
        destroy_bitmap(screen_buffer);
    }

    // Shutdown Allegro
    allegro_exit();
}



static void mainLoop()
{
    // Use the embedded ROM data directly
    SMBEngine engine(smbRomData);
    smbEngine = &engine;
    engine.reset();

    // Initialize controller
    Controller& controller1 = engine.getController1();
    
    // Initialize joystick for controller
    bool joystickInitialized = controller1.initJoystick();
    if (joystickInitialized) {
        std::cout << "Joystick initialized successfully for Player 1!" << std::endl;
    } else {
        std::cout << "No joystick found. Using keyboard controls only." << std::endl;
    }

    bool running = true;
    int frame_time = 0;
    
    while (running && !key[KEY_ESC]) {
        // Wait for next frame
        while (timer_counter <= frame_time) {
            // Yield CPU time
            rest(1);
        }
        frame_time = timer_counter;

        // Handle input using the controller's methods
        controller1.processKeyboardInput();
        
        if (joystickInitialized && Configuration::getJoystickPollingEnabled()) {
            controller1.processJoystickInput();
        }

        // Debug key to print controller state
        if (key[KEY_D]) {
            controller1.printButtonStates();
        }

        // Reset game
        if (key[KEY_R]) {
            engine.reset();
        }

        // Toggle fullscreen (F key)
        if (key[KEY_F]) {
            // Simple fullscreen toggle - would need more sophisticated implementation
            static bool fullscreen = false;
            fullscreen = !fullscreen;
            if (fullscreen) {
                set_gfx_mode(GFX_AUTODETECT, SCREEN_W, SCREEN_H, 0, 0);
            } else {
                int scale = Configuration::getRenderScale();
                set_gfx_mode(GFX_AUTODETECT_WINDOWED, 
                           RENDER_WIDTH * scale, RENDER_HEIGHT * scale, 0, 0);
            }
        }

        // Update game logic
        engine.update();
        engine.render(renderBuffer);

        // Apply post-processing filters if enabled
        uint32_t* sourceBuffer = renderBuffer;
        uint32_t* targetBuffer = filteredBuffer;

        // Apply HQDN3D filter if enabled
        if (Configuration::getHqdn3dEnabled()) {
            applyHQDN3D(targetBuffer, sourceBuffer, prevFrameBuffer, 
                        RENDER_WIDTH, RENDER_HEIGHT, 
                        Configuration::getHqdn3dSpatialStrength(), 
                        Configuration::getHqdn3dTemporalStrength());
            
            // Store the current frame for next time
            memcpy(prevFrameBuffer, sourceBuffer, RENDER_WIDTH * RENDER_HEIGHT * sizeof(uint32_t));
            
            // Swap buffers for potential next filter
            uint32_t* temp = sourceBuffer;
            sourceBuffer = targetBuffer;
            targetBuffer = temp;
        }

        // Apply FXAA if enabled
        if (Configuration::getAntiAliasingEnabled()) {
            applyFXAA(targetBuffer, sourceBuffer, RENDER_WIDTH, RENDER_HEIGHT);
            
            // Swap buffers
            uint32_t* temp = sourceBuffer;
            sourceBuffer = targetBuffer;
            targetBuffer = temp;
        }

        // Copy the final buffer back to renderBuffer for display
        if (sourceBuffer != renderBuffer) {
            memcpy(renderBuffer, sourceBuffer, RENDER_WIDTH * RENDER_HEIGHT * sizeof(uint32_t));
        }

        // Update the screen
        updateScreen();
    }
}

int main(int argc, char** argv)
{
    if (!initialize()) {
        std::cout << "Failed to initialize. Please check previous error messages for more information. The program will now exit.\n";
        return -1;
    }

    mainLoop();

    shutdown();

    return 0;
}
END_OF_MAIN()
