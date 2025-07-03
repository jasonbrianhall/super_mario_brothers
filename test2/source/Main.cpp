#include <cstdio>
#include <iostream>
#include <cstring>
#include "SDL.hpp"

#include "Emulation/Controller.hpp"
#include "SMB/SMBEngine.hpp"
#include "Util/Video.hpp"

#include "Configuration.hpp"
#include "Constants.hpp"
#include "Util/VideoFilters.hpp"
// Include the generated ROM header
#include "SMBRom.hpp" // This contains smbRomData array

static SDL_Window* window;
static SDL_Renderer* renderer;
static SDL_Texture* texture;
static SDL_Texture* scanlineTexture;
static SMBEngine* smbEngine = nullptr;
static uint32_t renderBuffer[RENDER_WIDTH * RENDER_HEIGHT];
static uint32_t filteredBuffer[RENDER_WIDTH * RENDER_HEIGHT];
static uint32_t prevFrameBuffer[RENDER_WIDTH * RENDER_HEIGHT];
static bool msaaEnabled = false;

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

    if (Configuration::getScanlinesEnabled())
    {
        scanlineTexture = generateScanlineTexture(renderer);
    }

    // Set up custom palette, if configured
    if (!Configuration::getPaletteFileName().empty())
    {
        const uint32_t* palette = loadPalette(Configuration::getPaletteFileName());
        if (palette)
        {
            paletteRGB = palette;
        }
    }

    // Initialize HQDN3D filter if enabled
    if (Configuration::getHqdn3dEnabled())
    {
        initHQDN3D(RENDER_WIDTH, RENDER_HEIGHT);
        // Initialize the previous frame buffer
        memset(prevFrameBuffer, 0, RENDER_WIDTH * RENDER_HEIGHT * sizeof(uint32_t));
    }

    // Initialize MSAA if enabled and method is MSAA
    if (Configuration::getAntiAliasingEnabled() && Configuration::getAntiAliasingMethod() == 1)
    {
        msaaEnabled = initMSAA(renderer);
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

    return true;
}

/**
 * Shutdown libraries for exit.
 */
static void shutdown()
{
    // Cleanup HQDN3D filter if it was initialized
    if (Configuration::getHqdn3dEnabled())
    {
        cleanupHQDN3D();
    }

    SDL_CloseAudio();

    SDL_DestroyTexture(scanlineTexture);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_Quit();
}

static void mainLoop()
{
    // Use the embedded ROM data directly
    //SMBEngine engine(smbRomData);
    SMBEngine engine(const_cast<uint8_t*>(smbRomData));
    smbEngine = &engine;
    engine.reset();

    // Initialize controller's joystick
    Controller& controller1 = engine.getController1();
    bool joystickInitialized = controller1.initJoystick();
    if (joystickInitialized)
    {
        std::cout << "Joystick initialized successfully!" << std::endl;
    }
    else
    {
        std::cout << "No joystick found or initialization failed. Using keyboard controls only." << std::endl;
    }

    bool running = true;
    int progStartTime = SDL_GetTicks();
    int frame = 0;
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
            // Process joystick events
            case SDL_JOYAXISMOTION:
            case SDL_JOYBUTTONDOWN:
            case SDL_JOYBUTTONUP:
            case SDL_CONTROLLERBUTTONDOWN:
            case SDL_CONTROLLERBUTTONUP:
            case SDL_CONTROLLERAXISMOTION:
                if (joystickInitialized)
                {
                    controller1.processJoystickEvent(event);
                }
                break;
            default:
                break;
            }
        }

        // Handle keyboard input
        const Uint8* keys = SDL_GetKeyboardState(NULL);
        controller1.setButtonState(BUTTON_A, keys[SDL_SCANCODE_X]);
        controller1.setButtonState(BUTTON_B, keys[SDL_SCANCODE_Z]);
        controller1.setButtonState(BUTTON_SELECT, keys[SDL_SCANCODE_BACKSPACE]);
        controller1.setButtonState(BUTTON_START, keys[SDL_SCANCODE_RETURN]);
        controller1.setButtonState(BUTTON_UP, keys[SDL_SCANCODE_UP]);
        controller1.setButtonState(BUTTON_DOWN, keys[SDL_SCANCODE_DOWN]);
        controller1.setButtonState(BUTTON_LEFT, keys[SDL_SCANCODE_LEFT]);
        controller1.setButtonState(BUTTON_RIGHT, keys[SDL_SCANCODE_RIGHT]);
        
        // Debug key to print controller state (press D key)
        if (keys[SDL_SCANCODE_D])
        {
            controller1.printButtonStates();
        }

        // Update joystick state (optional, if you want to poll the joystick state directly)
        if (joystickInitialized)
        {
            controller1.updateJoystickState();
        }

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
        if (keys[SDL_SCANCODE_F])
        {
            SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
        }

        engine.update();
        engine.render(renderBuffer);

        // Apply post-processing filters if enabled
        uint32_t* sourceBuffer = renderBuffer;
        uint32_t* targetBuffer = filteredBuffer;

        // Apply HQDN3D filter if enabled
        if (Configuration::getHqdn3dEnabled())
        {
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

        // Apply FXAA if enabled and method is FXAA
        if (Configuration::getAntiAliasingEnabled() && Configuration::getAntiAliasingMethod() == 0)
        {
            applyFXAA(targetBuffer, sourceBuffer, RENDER_WIDTH, RENDER_HEIGHT);
            
            // Swap buffers for potential next filter
            uint32_t* temp = sourceBuffer;
            sourceBuffer = targetBuffer;
            targetBuffer = temp;
        }

        // Update the texture with the filtered buffer
        SDL_UpdateTexture(texture, NULL, sourceBuffer, sizeof(uint32_t) * RENDER_WIDTH);

        SDL_RenderClear(renderer);

        // Render the screen
        SDL_RenderSetLogicalSize(renderer, RENDER_WIDTH, RENDER_HEIGHT);
        SDL_RenderCopy(renderer, texture, NULL, NULL);

        // Render scanlines
        if (Configuration::getScanlinesEnabled())
        {
            SDL_RenderSetLogicalSize(renderer, RENDER_WIDTH * 3, RENDER_HEIGHT * 3);
            SDL_RenderCopy(renderer, scanlineTexture, NULL, NULL);
        }

        SDL_RenderPresent(renderer);

        /**
         * Ensure that the framerate stays as close to the desired FPS as possible. If the frame was rendered faster, then delay. 
         * If the frame was slower, reset time so that the game doesn't try to "catch up", going super-speed.
         */
        int now = SDL_GetTicks();
        int delay = progStartTime + int(double(frame) * double(MS_PER_SEC) / double(Configuration::getFrameRate())) - now;
if (delay > 0) {
    static int debug_count = 0;
    SDL_Delay(delay);
}        else 
        {
            frame = 0;
            progStartTime = now;
        }
        frame++;
    }
}

int main(int argc, char** argv)
{
    if (!initialize())
    {
        std::cout << "Failed to initialize. Please check previous error messages for more information. The program will now exit.\n";
        return -1;
    }

    mainLoop();

    shutdown();

    return 0;
}
