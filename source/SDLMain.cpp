#include <cstdio>
#include <iostream>
#include <cstring>
#include <ctime>
#ifdef _WIN32
#  include <windows.h>
#endif

#include <SDL2/SDL.h>

#include "Emulation/Controller.hpp"
#include "SMB/SMBEngine.hpp"
#include "Util/Video.hpp"

#include "Configuration.hpp"
#include "Constants.hpp"
#include "Util/VideoFilters.hpp"
#include "SMBRom.hpp"
#include "SDLCacheScaling.hpp"
#ifdef KITTY_ENABLED
#include "KittyRenderer.hpp"
#endif

// ─── globals ─────────────────────────────────────────────────────────────────
static SDLScalingCache* scalingCache     = nullptr;
static SDL_Window*      window           = nullptr;
static SDL_Renderer*    renderer         = nullptr;
static SDL_Texture*     texture          = nullptr;
static SDL_Texture*     scanlineTexture  = nullptr;
static SMBEngine*       smbEngine        = nullptr;
#ifdef KITTY_ENABLED
static KittyRenderer*   kittyRenderer    = nullptr;
static bool             useKittyMode     = false;
static int              kittyScale       = 2;
#else
static const bool       useKittyMode     = false;
#endif

static uint32_t renderBuffer  [RENDER_WIDTH * RENDER_HEIGHT];
static uint32_t filteredBuffer[RENDER_WIDTH * RENDER_HEIGHT];
static uint32_t prevFrameBuffer[RENDER_WIDTH * RENDER_HEIGHT];
static bool msaaEnabled = false;

// ─── audio ───────────────────────────────────────────────────────────────────
static void audioCallback(void* userdata, uint8_t* buffer, int len)
{
    if (smbEngine) smbEngine->audioCallback(buffer, len);
}

// ─── SDL initialize / shutdown ────────────────────────────────────────────────
#ifdef KITTY_ENABLED
static void initKittyKeyHoldMs();
static void enableKittyKeyboardProto();
static void disableKittyKeyboardProto();
#endif

static bool initialize()
{
    Configuration::initialize(CONFIG_FILE_NAME);

#ifdef KITTY_ENABLED
    if (useKittyMode) {
        // Kitty mode: init timer + optional audio; skip video entirely.
        Uint32 sdlFlags = SDL_INIT_TIMER;
        if (Configuration::getAudioEnabled())
            sdlFlags |= SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER;
        SDL_Init(sdlFlags); // best-effort, non-fatal

        // Check terminal support
        if (!KittyRenderer::isKittySupported()) {
            std::cerr << "Warning: terminal may not support the Kitty graphics protocol.\n"
                      << "  Set TERM=xterm-kitty or run inside Kitty / WezTerm.\n";
        }

        kittyRenderer = new KittyRenderer(RENDER_WIDTH, RENDER_HEIGHT, kittyScale);

        if (!KittyRenderer::enableRawMode()) {
            std::cerr << "Warning: could not enable terminal raw mode (stdin not a tty?).\n";
        }

        // Hide cursor for cleaner display
        fwrite("\x1b[?25l", 1, 6, stdout); fflush(stdout);
        // Clear screen
        fwrite("\x1b[2J\x1b[H", 1, 7, stdout); fflush(stdout);

        // Query X11 key-repeat rate to tune hold timeout
        initKittyKeyHoldMs();

        // Drain any bytes already in stdin before the game loop starts.
        {
            SDL_Delay(50);  // let terminal flush any responses to our escape sequences
            while (KittyRenderer::pollKey() != 0) { /* discard */ }
        }

        // Request kitty keyboard protocol (key-up events).
        // Falls back to timeout mode silently if terminal ignores it.
        enableKittyKeyboardProto();

    } else {
        // Normal SDL path (unchanged from original)
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) < 0) {
            std::cerr << "SDL_Init() failed: " << SDL_GetError() << "\n";
            return false;
        }

        window = SDL_CreateWindow(APP_TITLE,
                                  SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                  RENDER_WIDTH  * Configuration::getRenderScale(),
                                  RENDER_HEIGHT * Configuration::getRenderScale(),
                                  0);
        if (!window) {
            std::cerr << "SDL_CreateWindow() failed: " << SDL_GetError() << "\n";
            return false;
        }

        renderer = SDL_CreateRenderer(window, -1,
            (Configuration::getVsyncEnabled() ? SDL_RENDERER_PRESENTVSYNC : 0) |
            SDL_RENDERER_ACCELERATED);
        if (!renderer) {
            std::cerr << "SDL_CreateRenderer() failed: " << SDL_GetError() << "\n";
            return false;
        }

        if (SDL_RenderSetLogicalSize(renderer, RENDER_WIDTH, RENDER_HEIGHT) < 0) {
            std::cerr << "SDL_RenderSetLogicalSize() failed: " << SDL_GetError() << "\n";
            return false;
        }

        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                    SDL_TEXTUREACCESS_STREAMING,
                                    RENDER_WIDTH, RENDER_HEIGHT);
        if (!texture) {
            std::cerr << "SDL_CreateTexture() failed: " << SDL_GetError() << "\n";
            return false;
        }

        if (Configuration::getScanlinesEnabled())
            scanlineTexture = generateScanlineTexture(renderer);

        if (Configuration::getAntiAliasingEnabled() &&
            Configuration::getAntiAliasingMethod() == 1)
            msaaEnabled = initMSAA(renderer);

        scalingCache = new SDLScalingCache(renderer);
        scalingCache->initialize();
    }
#endif // KITTY_ENABLED

    // ── Shared init (both SDL and Kitty modes) ────────────────────────────────

    // Custom palette (paletteRGB used by SMBEngine::render regardless of mode)
    if (!Configuration::getPaletteFileName().empty()) {
        const uint32_t* palette = loadPalette(Configuration::getPaletteFileName());
        if (palette) paletteRGB = palette;
    }

    // Temporal filter
    if (Configuration::getHqdn3dEnabled()) {
        initHQDN3D(RENDER_WIDTH, RENDER_HEIGHT);
        memset(prevFrameBuffer, 0, sizeof(prevFrameBuffer));
    }

    // Audio (shared between both modes)
    if (Configuration::getAudioEnabled()) {
        SDL_AudioSpec desired{};
        desired.freq     = Configuration::getAudioFrequency();
        desired.format   = AUDIO_S8;
        desired.channels = 1;
        desired.samples  = 2048;
        desired.callback = audioCallback;
        SDL_AudioSpec obtained;
        SDL_OpenAudio(&desired, &obtained);
        SDL_PauseAudio(0);
    }

    return true;
}

static void shutdown()
{
#ifdef KITTY_ENABLED
    if (useKittyMode) {
        // Restore terminal
        disableKittyKeyboardProto();         // pop keyboard protocol flags
        KittyRenderer::disableRawMode();
        fwrite("\x1b[?25h", 1, 6, stdout); fflush(stdout);   // show cursor
        fwrite("\x1b[2J\x1b[H", 1, 7, stdout); fflush(stdout); // clear screen
        delete kittyRenderer;
        kittyRenderer = nullptr;
    } else {
        if (Configuration::getHqdn3dEnabled()) cleanupHQDN3D();
        delete scalingCache;
        scalingCache = nullptr;
        SDL_DestroyTexture(scanlineTexture);
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
    }

#endif // KITTY_ENABLED
    SDL_CloseAudio();
    SDL_Quit();
}

// ─── Kitty input map ─────────────────────────────────────────────────────────
// Maps raw bytes to NES controller buttons via the Controller API.
// Arrow keys send ESC [ A/B/C/D; everything else is a single byte.
// ─── Kitty keyboard protocol input ────────────────────────────────────────────
// We request the kitty keyboard protocol (progressive enhancement mode) which
// sends real key-down AND key-up events — no timeout hacks needed.
//
// Enable:  ESC [ = 1 ; 1 u   (push flags: report key-up events)
// Disable: ESC [ < 1 u       (pop flags, restore on exit)
//
// Key events arrive as: ESC [ <keycode> ; <mods> ; <event> u
//   event: 1=press, 2=repeat, 3=release
//
// Keycodes we care about (from the kitty keyboard spec):
//   Arrow keys: Up=57352  Down=57353  Right=57355  Left=57354
//   Regular keys use their Unicode codepoint directly.
//
// If the terminal does not support the protocol the enable sequence is ignored
// and we fall back to the legacy timeout method automatically.

static bool s_kittyKbProto = false;  // true once we confirmed protocol is active
static bool s_buttonHeld[8] = {};    // indexed by BUTTON_*

// Fallback timeout for terminals that ignore the keyboard protocol request.
// Only used if we never receive a CSI...u style event.
static const int64_t KEY_HOLD_MS_FALLBACK = 35;
static int64_t KEY_HOLD_MS = KEY_HOLD_MS_FALLBACK;
static int64_t s_lastSeen[8] = {};

#ifdef KITTY_ENABLED
static void initKittyKeyHoldMs() {
#ifdef _WIN32
    // On Windows, query key repeat rate via SystemParametersInfo
    DWORD repeatSpeed = 0; // 0=slowest(~2Hz), 31=fastest(~30Hz)
    if (SystemParametersInfo(SPI_GETKEYBOARDSPEED, 0, &repeatSpeed, 0)) {
        // Map 0-31 range to ~2Hz-30Hz
        int hz = 2 + (int)(repeatSpeed * 28 / 31);
        KEY_HOLD_MS = 1000 / hz + 10;
    }
#else
    FILE* f = popen("xset q 2>/dev/null", "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        int delay_ms, rate_hz;
        if (sscanf(line, " auto repeat delay: %d repeat rate: %d", &delay_ms, &rate_hz) == 2) {
            if (rate_hz > 0) KEY_HOLD_MS = 1000 / rate_hz + 10;
            break;
        }
    }
    pclose(f);
#endif
}

static void enableKittyKeyboardProto() {
    fwrite("\x1b[=1;1u", 1, 7, stdout); fflush(stdout);
}

static void disableKittyKeyboardProto() {
    fwrite("\x1b[<1u", 1, 5, stdout); fflush(stdout);
}

// Map a kitty keyboard protocol keycode to a BUTTON_* index, or -1.
static int kitkeyToButton(int code, bool isArrow) {
    if (isArrow) {
        // Arrow keys: CSI A/B/C/D (legacy) or CSI 57352..57355 u (kitty proto)
        switch (code) {
        case 57352: return BUTTON_UP;
        case 57353: return BUTTON_DOWN;
        case 57354: return BUTTON_LEFT;
        case 57355: return BUTTON_RIGHT;
        }
    } else {
        switch (code) {
        case 'x': case 'X': return BUTTON_A;
        case 'z': case 'Z': return BUTTON_B;
        case 13:             return BUTTON_START;   // Enter
        case 8: case 127:   return BUTTON_SELECT;  // Backspace
        }
    }
    return -1;
}

// Parse a CSI sequence already past the ESC [.
// Reads bytes from pollKey() until the terminator.
// Returns the terminator char, fills params[0..2].
static int parseCsiParams(int params[3], int& nparams) {
    nparams = 0;
    params[0] = params[1] = params[2] = 0;
    int cur = 0;
    for (;;) {
        int c = KittyRenderer::pollKey();
        if (c == 0) break;  // nothing more right now
        if (c >= '0' && c <= '9') {
            cur = cur * 10 + (c - '0');
        } else if (c == ';') {
            if (nparams < 3) params[nparams++] = cur;
            cur = 0;
        } else {
            // Terminator
            if (nparams < 3) params[nparams++] = cur;
            return c;
        }
    }
    return 0;
}

static void handleKittyInput(Controller& ctrl, bool& running, SMBEngine& engine, int64_t now)
{
    // Drain ALL pending bytes this frame — key-repeat fires at ~30ms but we
    // run at 16ms, so we must consume every queued byte before checking state.
    int ch;
    while ((ch = KittyRenderer::pollKey()) != 0) {
        if (ch == 0x1b) {
            int ch2 = KittyRenderer::pollKey();
            if (ch2 == '[') {
                int params[3]; int np;
                int term = parseCsiParams(params, np);

                if (term == 'u' && np >= 1) {
                    // Kitty keyboard protocol: real press/release events
                    s_kittyKbProto = true;
                    int code  = params[0];
                    int event = (np >= 3) ? params[2] : 1;
                    bool pressed = (event == 1 || event == 2);
                    int btn = kitkeyToButton(code, true);
                    if (btn < 0) btn = kitkeyToButton(code, false);
                    if (btn >= 0) s_buttonHeld[btn] = pressed;
                } else if (term == 'A') { s_lastSeen[BUTTON_UP]    = now;
                } else if (term == 'B') { s_lastSeen[BUTTON_DOWN]  = now;
                } else if (term == 'C') { s_lastSeen[BUTTON_RIGHT] = now;
                } else if (term == 'D') { s_lastSeen[BUTTON_LEFT]  = now;
                }
            }
        } else {
            int btn = kitkeyToButton(ch, false);
            if (btn >= 0) {
                s_lastSeen[btn] = now;
                s_buttonHeld[btn] = true;
            } else {
                switch (ch) {
                case 'r': case 'R': engine.reset();  break;
                case 'q': case 'Q': running = false; break;
                default: break;
                }
            }
        }
    }

    // Apply states: kitty proto uses explicit held flags;
    // legacy uses timeout but does NOT reset held state — only expires it.
    for (int i = 0; i < 8; i++) {
        bool held;
        if (s_kittyKbProto) {
            held = s_buttonHeld[i];
        } else {
            if ((now - s_lastSeen[i]) >= KEY_HOLD_MS) {
                s_buttonHeld[i] = false;  // expired
            }
            held = s_buttonHeld[i];
        }
        ctrl.setButtonState((ControllerButton)i, held);
    }
}


#endif // KITTY_ENABLED

// ─── main loop ────────────────────────────────────────────────────────────────
static void mainLoop()
{
    SMBEngine engine(const_cast<uint8_t*>(smbRomData));
    smbEngine = &engine;
    engine.reset();

    Controller& controller1 = engine.getController1();

    // Joystick only relevant in SDL mode
    bool joystickInitialized = false;
    if (!useKittyMode) {
} else {
        joystickInitialized = controller1.initJoystick();
        if (joystickInitialized)
            std::cout << "Joystick initialized.\n";
        else
            std::cout << "No joystick found. Keyboard only.\n";
    }

    bool running     = true;
    int  frame       = 0;

    // SDL_GetTicks() requires SDL_INIT_VIDEO which we skip in kitty mode.
    // Use clock_gettime for a reliable monotonic clock in both modes.
    auto getMs = []() -> int64_t {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
    };
    int64_t progStart = getMs();

    // Key state tracking (SDL mode)
    static bool optimizedScalingKeyPressed = false;
    static bool f11KeyPressed = false, fKeyPressed = false;
    static bool f5KeyPressed  = false, f6KeyPressed = false;
    static bool f7KeyPressed  = false, f8KeyPressed = false;


    while (running) {

        // ── Kitty mode: input via raw terminal ────────────────────────────
#ifdef KITTY_ENABLED
        if (useKittyMode) {
            // handleKittyInput manages button state via timestamp-based hold detection.
            // No manual clear needed — applyHeldKeys() sets each button true/false.
            handleKittyInput(controller1, running, engine, getMs());
            if (!running) break;

        } else
#endif // KITTY_ENABLED
        {
            // ── SDL mode: events and keyboard ─────────────────────────────
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                switch (event.type) {
                case SDL_QUIT:
                    running = false; break;
                case SDL_WINDOWEVENT:
                    if (event.window.event == SDL_WINDOWEVENT_CLOSE)
                        running = false;
                    break;
                case SDL_JOYAXISMOTION:
                case SDL_JOYBUTTONDOWN: case SDL_JOYBUTTONUP:
                case SDL_CONTROLLERBUTTONDOWN: case SDL_CONTROLLERBUTTONUP:
                case SDL_CONTROLLERAXISMOTION:
                    if (joystickInitialized)
                        controller1.processJoystickEvent(event);
                    break;
                default: break;
                }
            }

            const Uint8* keys = SDL_GetKeyboardState(nullptr);
            controller1.setButtonState(BUTTON_A,      keys[SDL_SCANCODE_X]);
            controller1.setButtonState(BUTTON_B,      keys[SDL_SCANCODE_Z]);
            controller1.setButtonState(BUTTON_SELECT, keys[SDL_SCANCODE_BACKSPACE]);
            controller1.setButtonState(BUTTON_START,  keys[SDL_SCANCODE_RETURN]);
            controller1.setButtonState(BUTTON_UP,     keys[SDL_SCANCODE_UP]);
            controller1.setButtonState(BUTTON_DOWN,   keys[SDL_SCANCODE_DOWN]);
            controller1.setButtonState(BUTTON_LEFT,   keys[SDL_SCANCODE_LEFT]);
            controller1.setButtonState(BUTTON_RIGHT,  keys[SDL_SCANCODE_RIGHT]);

            if (joystickInitialized) controller1.updateJoystickState();

            if (keys[SDL_SCANCODE_R])      engine.reset();
            if (keys[SDL_SCANCODE_ESCAPE]) { running = false; break; }

            // Save/Load states
            bool shift = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
            auto doSaveLoad = [&](int scancode, bool& pressed, const char* slot) {
                if (keys[scancode] && !pressed) {
                    if (shift) { if (engine.loadState(slot)) printf("Loaded %s\n", slot); }
                    else        { engine.saveState(slot); printf("Saved %s\n", slot); }
                    pressed = true;
                } else if (!keys[scancode]) pressed = false;
            };
            doSaveLoad(SDL_SCANCODE_F5, f5KeyPressed, "save1");
            doSaveLoad(SDL_SCANCODE_F6, f6KeyPressed, "save2");
            doSaveLoad(SDL_SCANCODE_F7, f7KeyPressed, "save3");
            doSaveLoad(SDL_SCANCODE_F8, f8KeyPressed, "save4");

            // F11 fullscreen toggle
            if (keys[SDL_SCANCODE_F11] && !f11KeyPressed) {
                Uint32 flags = SDL_GetWindowFlags(window);
                SDL_SetWindowFullscreen(window,
                    (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
                f11KeyPressed = true;
            } else if (!keys[SDL_SCANCODE_F11]) f11KeyPressed = false;

            if (keys[SDL_SCANCODE_F] && !fKeyPressed) {
                SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
                fKeyPressed = true;
            } else if (!keys[SDL_SCANCODE_F]) fKeyPressed = false;

            // O: toggle optimized scaling
            if (keys[SDL_SCANCODE_O] && !optimizedScalingKeyPressed) {
                if (scalingCache) {
                    bool e = scalingCache->isOptimizedScaling();
                    scalingCache->setOptimizedScaling(!e);
                    printf("Optimized scaling: %s\n", !e ? "on" : "off");
                }
                optimizedScalingKeyPressed = true;
            } else if (!keys[SDL_SCANCODE_O]) optimizedScalingKeyPressed = false;
        }

        // ── Update engine ─────────────────────────────────────────────────
        engine.update();
        engine.render(renderBuffer);

        // ── Post-processing filters ───────────────────────────────────────
        uint32_t* sourceBuffer = renderBuffer;
        uint32_t* targetBuffer = filteredBuffer;

        if (Configuration::getHqdn3dEnabled()) {
            applyHQDN3D(targetBuffer, sourceBuffer, prevFrameBuffer,
                        RENDER_WIDTH, RENDER_HEIGHT,
                        Configuration::getHqdn3dSpatialStrength(),
                        Configuration::getHqdn3dTemporalStrength());
            memcpy(prevFrameBuffer, sourceBuffer, sizeof(uint32_t) * RENDER_WIDTH * RENDER_HEIGHT);
            std::swap(sourceBuffer, targetBuffer);
        }

        if (Configuration::getAntiAliasingEnabled() &&
            Configuration::getAntiAliasingMethod() == 0) {
            applyFXAA(targetBuffer, sourceBuffer, RENDER_WIDTH, RENDER_HEIGHT);
            std::swap(sourceBuffer, targetBuffer);
        }

        // ── Render ────────────────────────────────────────────────────────
#ifdef KITTY_ENABLED
        if (useKittyMode) {
            kittyRenderer->renderFrame(sourceBuffer);
        } else
#endif // KITTY_ENABLED
        {
            SDL_RenderClear(renderer);

            bool optimizedUsed = false;
            if (scalingCache && scalingCache->isOptimizedScaling()) {
                int ww, wh;
                SDL_GetWindowSize(window, &ww, &wh);
                scalingCache->renderOptimized(sourceBuffer, ww, wh);
                optimizedUsed = true;
            }
            if (!optimizedUsed) {
                SDL_UpdateTexture(texture, nullptr, sourceBuffer, sizeof(uint32_t) * RENDER_WIDTH);
                SDL_RenderSetLogicalSize(renderer, RENDER_WIDTH, RENDER_HEIGHT);
                SDL_RenderCopy(renderer, texture, nullptr, nullptr);
            }

            if (Configuration::getScanlinesEnabled()) {
                SDL_RenderSetLogicalSize(renderer, RENDER_WIDTH * 3, RENDER_HEIGHT * 3);
                SDL_RenderCopy(renderer, scanlineTexture, nullptr, nullptr);
            }

            SDL_RenderPresent(renderer);
        }

        // ── Frame timing ──────────────────────────────────────────────────
        int64_t now   = getMs();
        int64_t delay = progStart + (int64_t)((double)frame * (double)MS_PER_SEC /
                                               (double)Configuration::getFrameRate()) - now;
        if (delay > 0) {
            SDL_Delay((int)delay);
        } else {
            frame     = 0;
            progStart = now;
        }
        frame++;
    }
}

// ─── main ─────────────────────────────────────────────────────────────────────
static void printHelp(const char* prog)
{
    printf("Usage: %s [options]\n"
           #ifdef KITTY_ENABLED
           "  --kitty              Render frames to terminal via Kitty graphics protocol\n"
           "  --kitty-scale <N>    Pixel scale factor for kitty mode (default: 2)\n"
#endif
           "  --help               Show this message\n",
           prog);
}

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
#ifdef KITTY_ENABLED
        if (strcmp(argv[i], "--kitty") == 0) {
            useKittyMode = true;
        } else if (strcmp(argv[i], "--kitty-scale") == 0 && i + 1 < argc) {
            kittyScale = atoi(argv[++i]);
            if (kittyScale < 1) kittyScale = 1;
            if (kittyScale > 8) kittyScale = 8;
        } else
#endif // KITTY_ENABLED
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printHelp(argv[0]);
            return 0;
        }
    }

    if (!initialize()) {
        std::cerr << "Failed to initialize. See above errors.\n";
        return -1;
    }

    mainLoop();
    shutdown();
    return 0;
}
