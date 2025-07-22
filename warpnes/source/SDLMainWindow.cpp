#include <cstdio>
#include <iostream>

#include <SDL2/SDL.h>

#include "SMB/SMBEmulator.hpp"

#include "Configuration.hpp"
#include "Constants.hpp"
// Include the generated ROM header
#include "SDLCacheScaling.hpp"

static SDLScalingCache* scalingCache = nullptr;
static SDL_Window* window;
static SDL_Renderer* renderer;
static SDL_Texture* texture;
static SDL_Texture* scanlineTexture;
static SMBEmulator* smbEngine = nullptr;
static uint16_t renderBuffer[RENDER_WIDTH * RENDER_HEIGHT];
static uint16_t filteredBuffer[RENDER_WIDTH * RENDER_HEIGHT];
static uint16_t prevFrameBuffer[RENDER_WIDTH * RENDER_HEIGHT];
static bool msaaEnabled = false;

// Internal controller state
struct InternalController {
    bool buttonA = false;
    bool buttonB = false;
    bool buttonSelect = false;
    bool buttonStart = false;
    bool buttonUp = false;
    bool buttonDown = false;
    bool buttonLeft = false;
    bool buttonRight = false;
    
    // Joystick/Gamepad support
    SDL_Joystick* joystick = nullptr;
    SDL_GameController* gameController = nullptr;
    bool hasController = false;
    
    void initializeController() {
        // Initialize joystick subsystem
        if (SDL_NumJoysticks() > 0) {
            // Try to open the first available game controller
            for (int i = 0; i < SDL_NumJoysticks(); i++) {
                if (SDL_IsGameController(i)) {
                    gameController = SDL_GameControllerOpen(i);
                    if (gameController) {
                        hasController = true;
                        std::cout << "Game controller initialized: " << SDL_GameControllerName(gameController) << std::endl;
                        break;
                    }
                }
            }
            
            // If no game controller, try to open as joystick
            if (!hasController) {
                joystick = SDL_JoystickOpen(0);
                if (joystick) {
                    hasController = true;
                    std::cout << "Joystick initialized: " << SDL_JoystickName(joystick) << std::endl;
                }
            }
        }
        
        if (!hasController) {
            std::cout << "No controller found. Using keyboard controls only." << std::endl;
        }
    }
    
    void cleanup() {
        if (gameController) {
            SDL_GameControllerClose(gameController);
            gameController = nullptr;
        }
        if (joystick) {
            SDL_JoystickClose(joystick);
            joystick = nullptr;
        }
        hasController = false;
    }
    
    void processControllerEvent(const SDL_Event& event) {
        if (!hasController) return;
        
        if (gameController) {
            // Handle game controller events
            switch (event.type) {
                case SDL_CONTROLLERBUTTONDOWN:
                case SDL_CONTROLLERBUTTONUP:
                    {
                        bool pressed = (event.type == SDL_CONTROLLERBUTTONDOWN);
                        switch (event.cbutton.button) {
                            case SDL_CONTROLLER_BUTTON_A:
                                buttonA = pressed;
                                break;
                            case SDL_CONTROLLER_BUTTON_B:
                            case SDL_CONTROLLER_BUTTON_X:
                                buttonB = pressed;
                                break;
                            case SDL_CONTROLLER_BUTTON_BACK:
                                buttonSelect = pressed;
                                break;
                            case SDL_CONTROLLER_BUTTON_START:
                                buttonStart = pressed;
                                break;
                            case SDL_CONTROLLER_BUTTON_DPAD_UP:
                                buttonUp = pressed;
                                break;
                            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                                buttonDown = pressed;
                                break;
                            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                                buttonLeft = pressed;
                                break;
                            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                                buttonRight = pressed;
                                break;
                        }
                    }
                    break;
                case SDL_CONTROLLERAXISMOTION:
                    {
                        const int DEADZONE = 8000;
                        switch (event.caxis.axis) {
                            case SDL_CONTROLLER_AXIS_LEFTX:
                                buttonLeft = event.caxis.value < -DEADZONE;
                                buttonRight = event.caxis.value > DEADZONE;
                                break;
                            case SDL_CONTROLLER_AXIS_LEFTY:
                                buttonUp = event.caxis.value < -DEADZONE;
                                buttonDown = event.caxis.value > DEADZONE;
                                break;
                        }
                    }
                    break;
            }
        } else if (joystick) {
            // Handle joystick events
            switch (event.type) {
                case SDL_JOYBUTTONDOWN:
                case SDL_JOYBUTTONUP:
                    {
                        bool pressed = (event.type == SDL_JOYBUTTONDOWN);
                        // Map buttons based on common joystick layouts
                        switch (event.jbutton.button) {
                            case 0: // Usually A button
                                buttonA = pressed;
                                break;
                            case 1: // Usually B button
                                buttonB = pressed;
                                break;
                            case 6: // Usually Select/Back
                                buttonSelect = pressed;
                                break;
                            case 7: // Usually Start
                                buttonStart = pressed;
                                break;
                        }
                    }
                    break;
                case SDL_JOYAXISMOTION:
                    {
                        const int DEADZONE = 8000;
                        switch (event.jaxis.axis) {
                            case 0: // X axis
                                buttonLeft = event.jaxis.value < -DEADZONE;
                                buttonRight = event.jaxis.value > DEADZONE;
                                break;
                            case 1: // Y axis
                                buttonUp = event.jaxis.value < -DEADZONE;
                                buttonDown = event.jaxis.value > DEADZONE;
                                break;
                        }
                    }
                    break;
            }
        }
    }
    
    void updateFromKeyboard(const Uint8* keys) {
        // Keyboard input (combine with controller input)
        if (!buttonA) buttonA = keys[SDL_SCANCODE_X];
        if (!buttonB) buttonB = keys[SDL_SCANCODE_Z];
        if (!buttonSelect) buttonSelect = keys[SDL_SCANCODE_BACKSPACE];
        if (!buttonStart) buttonStart = keys[SDL_SCANCODE_RETURN];
        if (!buttonUp) buttonUp = keys[SDL_SCANCODE_UP];
        if (!buttonDown) buttonDown = keys[SDL_SCANCODE_DOWN];
        if (!buttonLeft) buttonLeft = keys[SDL_SCANCODE_LEFT];
        if (!buttonRight) buttonRight = keys[SDL_SCANCODE_RIGHT];
    }
    
    void printButtonStates() {
        std::cout << "Controller State - A:" << (buttonA ? "1" : "0") 
                  << " B:" << (buttonB ? "1" : "0")
                  << " Select:" << (buttonSelect ? "1" : "0")
                  << " Start:" << (buttonStart ? "1" : "0")
                  << " Up:" << (buttonUp ? "1" : "0")
                  << " Down:" << (buttonDown ? "1" : "0")
                  << " Left:" << (buttonLeft ? "1" : "0")
                  << " Right:" << (buttonRight ? "1" : "0") << std::endl;
    }
};

static InternalController controller;

/**
 * SDL Audio callback function.
 */
static void audioCallback(void* userdata, uint8_t* buffer, int len)
{
    if (smbEngine != nullptr)
    {
        smbEngine->audioCallback(buffer, len);
    }
}

/**
 * Initialize libraries for use.
 */
static bool initialize()
{
    // Load the configuration
    Configuration::initialize(CONFIG_FILE_NAME);

    // Initialize SDL with joystick support
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) < 0)
    {
        std::cout << "SDL_Init() failed during initialize(): " << SDL_GetError() << std::endl;
        return false;
    }

    // Create the window
    window = SDL_CreateWindow(APP_TITLE,
                              SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED,
                              RENDER_WIDTH * Configuration::getRenderScale(),
                              RENDER_HEIGHT * Configuration::getRenderScale(),
                              0);
    if (window == nullptr)
    {
        std::cout << "SDL_CreateWindow() failed during initialize(): " << SDL_GetError() << std::endl;
        return false;
    }

    // Setup the renderer and texture buffer
    renderer = SDL_CreateRenderer(window, -1, (Configuration::getVsyncEnabled() ? SDL_RENDERER_PRESENTVSYNC : 0) | SDL_RENDERER_ACCELERATED);
    if (renderer == nullptr)
    {
        std::cout << "SDL_CreateRenderer() failed during initialize(): " << SDL_GetError() << std::endl;
        return false;
    }

    if (SDL_RenderSetLogicalSize(renderer, RENDER_WIDTH, RENDER_HEIGHT) < 0)
    {
        std::cout << "SDL_RenderSetLogicalSize() failed during initialize(): " << SDL_GetError() << std::endl;
        return false;
    }

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, RENDER_WIDTH, RENDER_HEIGHT);
    if (texture == nullptr)
    {
        std::cout << "SDL_CreateTexture() failed during initialize(): " << SDL_GetError() << std::endl;
        return false;
    }

    if (Configuration::getAudioEnabled())
    {
        // Initialize audio
        SDL_AudioSpec desiredSpec;
        desiredSpec.freq = Configuration::getAudioFrequency();
        desiredSpec.format = AUDIO_S8;
        desiredSpec.channels = 1;
        desiredSpec.samples = 2048;
        desiredSpec.callback = audioCallback;
        desiredSpec.userdata = NULL;

        SDL_AudioSpec obtainedSpec;
        SDL_OpenAudio(&desiredSpec, &obtainedSpec);

        // Start playing audio
        SDL_PauseAudio(0);
    }

    scalingCache = new SDLScalingCache(renderer);
    scalingCache->initialize();

    // Initialize the internal controller
    controller.initializeController();

    return true;
}

/**
 * Shutdown libraries for exit.
 */
static void shutdown()
{

    // Cleanup internal controller
    controller.cleanup();

    if (scalingCache) {
        delete scalingCache;
        scalingCache = nullptr;
    }

    SDL_CloseAudio();

    SDL_DestroyTexture(scanlineTexture);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_Quit();
}

static void mainLoop(const char* romFilename)
{
    // Load ROM from file
    SMBEmulator engine();
    smbEngine = &engine;

    printf("Loading ROM: %s\n", romFilename);
    if (!engine.loadROM(romFilename)) {
        printf("Failed to load ROM file: %s\n", romFilename);
        setStatusMessage("ROM loading failed");
        return;
    }

    printf("ROM loaded successfully\n");


    engine.reset();

    bool running = true;
    int progStartTime = SDL_GetTicks();
    int frame = 0;
    
    // Key state tracking for toggle functions
    static bool optimizedScalingKeyPressed = false;
    static bool f11KeyPressed = false;
    static bool fKeyPressed = false;
    // Save/Load state key tracking
    static bool f5KeyPressed = false;
    static bool f6KeyPressed = false;
    static bool f7KeyPressed = false;
    static bool f8KeyPressed = false;
    
    while (running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_QUIT:
                running = false;
                break;
            case SDL_WINDOWEVENT:
                switch (event.window.event)
                {
                case SDL_WINDOWEVENT_CLOSE:
                    running = false;
                    break;
                }
                break;
            // Process controller events internally
            case SDL_JOYAXISMOTION:
            case SDL_JOYBUTTONDOWN:
            case SDL_JOYBUTTONUP:
            case SDL_CONTROLLERBUTTONDOWN:
            case SDL_CONTROLLERBUTTONUP:
            case SDL_CONTROLLERAXISMOTION:
                controller.processControllerEvent(event);
                break;
            default:
                break;
            }
        }

        // Reset controller state (will be set by keyboard/controller)
        controller.buttonA = false;
        controller.buttonB = false;
        controller.buttonSelect = false;
        controller.buttonStart = false;
        controller.buttonUp = false;
        controller.buttonDown = false;
        controller.buttonLeft = false;
        controller.buttonRight = false;

        // Handle keyboard input and update controller state
        const Uint8* keys = SDL_GetKeyboardState(NULL);
        controller.updateFromKeyboard(keys);
        
        // Update the game engine with controller state
        // Note: You'll need to modify SMBEngine to accept button states directly
        // or create a method to set controller state
        /*engine.setControllerState(controller.buttonA, controller.buttonB, 
                                  controller.buttonSelect, controller.buttonStart,
                                  controller.buttonUp, controller.buttonDown,
                                  controller.buttonLeft, controller.buttonRight); */
        
        // Debug key to print controller state (press D key)
        if (keys[SDL_SCANCODE_D])
        {
            controller.printButtonStates();
        }

        // Game control keys
        if (keys[SDL_SCANCODE_R])
        {
            // Reset
            engine.reset();
        }
        if (keys[SDL_SCANCODE_ESCAPE])
        {
            // quit
            running = false;
            break;
        }
        
        // Save/Load state handling (F5-F8 keys)
        bool shiftPressed = (keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT]);
        
        // F5 - Save/Load State 1
        if (keys[SDL_SCANCODE_F5] && !f5KeyPressed) {
            if (shiftPressed) {
                // Shift+F5 - Load State 1
                if (engine.loadState("save1")) {
                    printf("State 1 loaded\n");
                } else {
                    printf("Failed to load state 1\n");
                }
            } else {
                // F5 - Save State 1
                engine.saveState("save1");
                printf("State 1 saved\n");
            }
            f5KeyPressed = true;
        } else if (!keys[SDL_SCANCODE_F5]) {
            f5KeyPressed = false;
        }

        // F6 - Save/Load State 2
        if (keys[SDL_SCANCODE_F6] && !f6KeyPressed) {
            if (shiftPressed) {
                // Shift+F6 - Load State 2
                if (engine.loadState("save2")) {
                    printf("State 2 loaded\n");
                } else {
                    printf("Failed to load state 2\n");
                }
            } else {
                // F6 - Save State 2
                engine.saveState("save2");
                printf("State 2 saved\n");
            }
            f6KeyPressed = true;
        } else if (!keys[SDL_SCANCODE_F6]) {
            f6KeyPressed = false;
        }

        // F7 - Save/Load State 3
        if (keys[SDL_SCANCODE_F7] && !f7KeyPressed) {
            if (shiftPressed) {
                // Shift+F7 - Load State 3
                if (engine.loadState("save3")) {
                    printf("State 3 loaded\n");
                } else {
                    printf("Failed to load state 3\n");
                }
            } else {
                // F7 - Save State 3
                engine.saveState("save3");
                printf("State 3 saved\n");
            }
            f7KeyPressed = true;
        } else if (!keys[SDL_SCANCODE_F7]) {
            f7KeyPressed = false;
        }

        // F8 - Save/Load State 4
        if (keys[SDL_SCANCODE_F8] && !f8KeyPressed) {
            if (shiftPressed) {
                // Shift+F8 - Load State 4
                if (engine.loadState("save4")) {
                    printf("State 4 loaded\n");
                } else {
                    printf("Failed to load state 4\n");
                }
            } else {
                // F8 - Save State 4
                engine.saveState("save4");
                printf("State 4 saved\n");
            }
            f8KeyPressed = true;
        } else if (!keys[SDL_SCANCODE_F8]) {
            f8KeyPressed = false;
        }
        
        // Toggle fullscreen with F11 (with proper key state tracking)
        if (keys[SDL_SCANCODE_F11] && !f11KeyPressed) {
            // Get current fullscreen state
            Uint32 windowFlags = SDL_GetWindowFlags(window);
            if (windowFlags & SDL_WINDOW_FULLSCREEN_DESKTOP) {
                // Currently fullscreen, switch to windowed
                SDL_SetWindowFullscreen(window, 0);
                printf("Switched to windowed mode\n");
            } else {
                // Currently windowed, switch to fullscreen
                SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
                printf("Switched to fullscreen mode\n");
            }
            f11KeyPressed = true;
        } else if (!keys[SDL_SCANCODE_F11]) {
            f11KeyPressed = false;
        }
        
        // Keep F key for legacy fullscreen (always switches to fullscreen)
        if (keys[SDL_SCANCODE_F] && !fKeyPressed) {
            SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
            printf("F key: Switched to fullscreen mode\n");
            fKeyPressed = true;
        } else if (!keys[SDL_SCANCODE_F]) {
            fKeyPressed = false;
        }
        
        // Toggle optimized scaling (press O key)
        if (keys[SDL_SCANCODE_O] && !optimizedScalingKeyPressed) {
            if (scalingCache) {
                bool enabled = scalingCache->isOptimizedScaling();
                scalingCache->setOptimizedScaling(!enabled);
                printf("Optimized scaling: %s\n", !enabled ? "Enabled" : "Disabled");
            }
            optimizedScalingKeyPressed = true;
        } else if (!keys[SDL_SCANCODE_O]) {
            optimizedScalingKeyPressed = false;
        }

        // Game engine update and rendering
        engine.update();
        engine.render16(renderBuffer);

        // Apply post-processing filters if enabled
        uint16_t* sourceBuffer = renderBuffer;
        uint16_t* targetBuffer = filteredBuffer;

        // Clear the renderer
        SDL_RenderClear(renderer);

        // Try optimized rendering first
        bool optimizedRenderingUsed = false;
        if (scalingCache && scalingCache->isOptimizedScaling()) {
            // Get current window size for optimized rendering
            int window_width, window_height;
            SDL_GetWindowSize(window, &window_width, &window_height);
            
            scalingCache->renderOptimized(sourceBuffer, window_width, window_height);
            optimizedRenderingUsed = true;
        }

        // Fallback to original rendering if optimized rendering is disabled
        if (!optimizedRenderingUsed) {
            // Original rendering code
            SDL_UpdateTexture(texture, NULL, sourceBuffer, sizeof(uint16_t) * RENDER_WIDTH);
            SDL_RenderSetLogicalSize(renderer, RENDER_WIDTH, RENDER_HEIGHT);
            SDL_RenderCopy(renderer, texture, NULL, NULL);
        }

        // Render scanlines if enabled
        if (Configuration::getScanlinesEnabled())
        {
            SDL_RenderSetLogicalSize(renderer, RENDER_WIDTH * 3, RENDER_HEIGHT * 3);
            SDL_RenderCopy(renderer, scanlineTexture, NULL, NULL);
        }

        // Present the rendered frame
        SDL_RenderPresent(renderer);

        /**
         * Ensure that the framerate stays as close to the desired FPS as possible. If the frame was rendered faster, then delay. 
         * If the frame was slower, reset time so that the game doesn't try to "catch up", going super-speed.
         */
        int now = SDL_GetTicks();
        int delay = progStartTime + int(double(frame) * double(MS_PER_SEC) / double(Configuration::getFrameRate())) - now;
        if(delay > 0) 
        {
            SDL_Delay(delay);
        }
        else 
        {
            frame = 0;
            progStartTime = now;
        }
        frame++;
    }
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <rom_file>" << std::endl;
        return -1;
    }

    if (!initialize())
    {
        std::cout << "Failed to initialize. Please check previous error messages for more information. The program will now exit.\n";
        return -1;
    }

    mainLoop(argv[1]);

    shutdown();

    return 0;
}
