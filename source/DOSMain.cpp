/*
 * The MS-DOS front end (DJGPP + SDL3's DOS port).
 *
 * The same game as SDLMain.cpp, with what a DOS machine has instead of a
 * windowing system: a full-screen VESA/VGA mode (DOSVideo.cpp), the Sound
 * Blaster through SDL3's audio stream, the keyboard, and a gameport
 * joystick. Frames are skipped (the game keeps its speed) when the machine
 * cannot draw 60 of them a second.
 */
#ifdef __DJGPP__

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>   // on DOS: sets up the near pointers SDL needs

#include "Configuration.hpp"
#include "Constants.hpp"
#include "DOSVideo.hpp"
#include "Emulation/Controller.hpp"
#include "SMB/SMBEngine.hpp"
#include "SMBRom.hpp"
#include "Util/Video.hpp"
#include "Util/VideoFilters.hpp"

static SDL_Window*      window      = nullptr;
static SDL_AudioStream* audioStream = nullptr;
static SDL_Joystick*    joystick    = nullptr;
static SMBEngine*       smbEngine   = nullptr;

static uint32_t renderBuffer   [RENDER_WIDTH * RENDER_HEIGHT];
static uint32_t filteredBuffer [RENDER_WIDTH * RENDER_HEIGHT];
static uint32_t prevFrameBuffer[RENDER_WIDTH * RENDER_HEIGHT];

// Time spent per stage (for the summary printed on exit).
static Uint64 nsUpdate, nsRender, nsPresent;

// ---------------------------------------------------------------- audio
// SDL3 asks for bytes as it needs them; the APU has them buffered from the
// frames already run (8-bit, mono, at audio.frequency).
static void SDLCALL audioCallback(void*, SDL_AudioStream* stream, int additional, int)
{
    uint8_t buffer[1024];
    while (additional > 0) {
        int n = additional < (int)sizeof(buffer) ? additional : (int)sizeof(buffer);
        memset(buffer, 0, n);                   // silence where the APU has nothing
        if (smbEngine) smbEngine->audioCallback(buffer, n);
        SDL_PutAudioStreamData(stream, buffer, n);
        additional -= n;
    }
}

static void openAudio()
{
    if (!Configuration::getAudioEnabled()) return;
    if (getenv("SMB_NOSOUND") != nullptr) return;
    // (SDL_InitSubSystem): the real SDL3 one, not the SDL2-style macro from
    // compat/SDL2/SDL.h (pulled in by Controller.hpp), which returns 0 on success.
    if (!(SDL_InitSubSystem)(SDL_INIT_AUDIO)) {
        printf("Sound: none (%s)\n", SDL_GetError());
        return;
    }
    SDL_AudioSpec spec;
    spec.format   = SDL_AUDIO_S8;
    spec.channels = 1;
    spec.freq     = Configuration::getAudioFrequency();
    audioStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, audioCallback, nullptr);
    if (audioStream == nullptr) {
        printf("Sound: could not open (%s)\n", SDL_GetError());
        return;
    }
    printf("Sound: %s, %d Hz\n", SDL_GetCurrentAudioDriver(), spec.freq);
    SDL_ResumeAudioStreamDevice(audioStream);
}

// ---------------------------------------------------------------- joystick
// A gameport stick: two axes, two to four buttons.
//   button 1 = A (jump), button 2 = B (run/fire), 3 = Select, 4 = Start.
static void openJoystick()
{
    if (getenv("SMB_NOJOY") != nullptr) return;
    if (!(SDL_InitSubSystem)(SDL_INIT_JOYSTICK)) return;
    int count = 0;
    SDL_JoystickID* ids = SDL_GetJoysticks(&count);
    if (ids && count > 0) {
        joystick = SDL_OpenJoystick(ids[0]);
        if (joystick) {
            const char* name = SDL_GetJoystickName(joystick);
            printf("Joystick: %s, %d axes, %d buttons\n", name ? name : "gameport",
                   SDL_GetNumJoystickAxes(joystick), SDL_GetNumJoystickButtons(joystick));
        }
    }
    SDL_free(ids);
}

static bool joyButton(int b)
{
    return joystick && b < SDL_GetNumJoystickButtons(joystick) && SDL_GetJoystickButton(joystick, b);
}

// ---------------------------------------------------------------- input
static void readController(Controller& controller)
{
    const bool* keys = SDL_GetKeyboardState(nullptr);
    auto key = [&](int scancode) { return scancode > 0 && scancode < SDL_SCANCODE_COUNT && keys[scancode]; };

    bool up     = key(Configuration::getPlayer1KeyUp());
    bool down   = key(Configuration::getPlayer1KeyDown());
    bool left   = key(Configuration::getPlayer1KeyLeft());
    bool right  = key(Configuration::getPlayer1KeyRight());
    bool a      = key(Configuration::getPlayer1KeyA());
    bool b      = key(Configuration::getPlayer1KeyB());
    bool select = key(Configuration::getPlayer1KeySelect()) || keys[SDL_SCANCODE_BACKSPACE];
    bool start  = key(Configuration::getPlayer1KeyStart());

    if (joystick) {
        int dead = Configuration::getJoystickDeadzone();
        if (dead <= 0 || dead > 30000) dead = 12000;
        int x = SDL_GetNumJoystickAxes(joystick) > 0 ? SDL_GetJoystickAxis(joystick, 0) : 0;
        int y = SDL_GetNumJoystickAxes(joystick) > 1 ? SDL_GetJoystickAxis(joystick, 1) : 0;
        Uint8 hat = SDL_GetNumJoystickHats(joystick) > 0 ? SDL_GetJoystickHat(joystick, 0) : SDL_HAT_CENTERED;
        left   |= x < -dead || (hat & SDL_HAT_LEFT);
        right  |= x >  dead || (hat & SDL_HAT_RIGHT);
        up     |= y < -dead || (hat & SDL_HAT_UP);
        down   |= y >  dead || (hat & SDL_HAT_DOWN);
        a      |= joyButton(0);
        b      |= joyButton(1);
        select |= joyButton(2);
        start  |= joyButton(3);
    }

    controller.setButtonState(BUTTON_UP,     up);
    controller.setButtonState(BUTTON_DOWN,   down);
    controller.setButtonState(BUTTON_LEFT,   left);
    controller.setButtonState(BUTTON_RIGHT,  right);
    controller.setButtonState(BUTTON_A,      a);
    controller.setButtonState(BUTTON_B,      b);
    controller.setButtonState(BUTTON_SELECT, select);
    controller.setButtonState(BUTTON_START,  start);
}

// One-shot keys: Esc quits, R resets, F5..F8 save (Shift+F5..F8 load).
static void handleKey(const SDL_KeyboardEvent& key, SMBEngine& engine, bool& running)
{
    if (key.repeat) return;
    static const char* slots[4] = { "save1", "save2", "save3", "save4" };
    switch (key.scancode) {
    case SDL_SCANCODE_ESCAPE:
        running = false;
        break;
    case SDL_SCANCODE_R:
        engine.reset();
        break;
    case SDL_SCANCODE_F5: case SDL_SCANCODE_F6: case SDL_SCANCODE_F7: case SDL_SCANCODE_F8: {
        const char* slot = slots[key.scancode - SDL_SCANCODE_F5];
        if (key.mod & SDL_KMOD_SHIFT) {
            if (audioStream) SDL_LockAudioStream(audioStream);
            bool ok = engine.loadState(slot);
            if (audioStream) SDL_UnlockAudioStream(audioStream);
            if (ok) printf("Loaded %s\n", slot);
        } else {
            engine.saveState(slot);
            printf("Saved %s\n", slot);
        }
    } break;
    default:
        break;
    }
}

// ---------------------------------------------------------------- frame
static void drawFrame(SMBEngine& engine)
{
    Uint64 t0 = SDL_GetTicksNS();
    engine.render(renderBuffer);

    uint32_t* source = renderBuffer;
    uint32_t* target = filteredBuffer;

    if (Configuration::getHqdn3dEnabled()) {
        applyHQDN3D(target, source, prevFrameBuffer, RENDER_WIDTH, RENDER_HEIGHT,
                    Configuration::getHqdn3dSpatialStrength(),
                    Configuration::getHqdn3dTemporalStrength());
        memcpy(prevFrameBuffer, source, sizeof(prevFrameBuffer));
        uint32_t* t = source; source = target; target = t;
    }
    if (Configuration::getAntiAliasingEnabled() && Configuration::getAntiAliasingMethod() == 0) {
        applyFXAA(target, source, RENDER_WIDTH, RENDER_HEIGHT);
        uint32_t* t = source; source = target; target = t;
    }

    Uint64 t1 = SDL_GetTicksNS();
    dosPresent(window, source);
    Uint64 t2 = SDL_GetTicksNS();
    nsRender  += t1 - t0;
    nsPresent += t2 - t1;
}

static void mainLoop()
{
    SMBEngine engine(const_cast<uint8_t*>(smbRomData));
    smbEngine = &engine;
    engine.reset();
    Controller& controller1 = engine.getController1();

    int fps = Configuration::getFrameRate();
    if (fps <= 0) fps = 60;
    const Uint64 frameNS = SDL_NS_PER_SECOND / (Uint64)fps;

    // SMB_FRAMESKIP=n: draw every (n+1)th frame at most; default: as many
    // frames as the machine manages, skipping up to 4 in a row to keep speed.
    int maxSkip = 4, fixedSkip = 0;
    if (const char* fs = getenv("SMB_FRAMESKIP")) { fixedSkip = atoi(fs); if (fixedSkip < 0) fixedSkip = 0; }

    Uint64 started = SDL_GetTicksNS();
    Uint64 next = started;
    unsigned long framesRun = 0, framesDrawn = 0;
    int skipped = 0;
    bool running = true;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;
            else if (event.type == SDL_EVENT_KEY_DOWN) handleKey(event.key, engine, running);
        }
        if (!running) break;

        readController(controller1);

        Uint64 tu = SDL_GetTicksNS();
        if (audioStream) SDL_LockAudioStream(audioStream);
        engine.update();
        if (audioStream) SDL_UnlockAudioStream(audioStream);
        nsUpdate += SDL_GetTicksNS() - tu;

        next += frameNS;
        Uint64 now = SDL_GetTicksNS();
        bool behind = now > next;
        bool draw;
        if (fixedSkip > 0) draw = skipped >= fixedSkip;
        else               draw = !behind || skipped >= maxSkip;

        ++framesRun;
        if (draw) {
            drawFrame(engine);
            ++framesDrawn;
            skipped = 0;
        } else {
            ++skipped;
        }

        now = SDL_GetTicksNS();
        if (now < next) {
            SDL_DelayNS(next - now);
        } else if (now - next > frameNS * 8) {
            next = now;     // far behind (or the clock jumped): don't race to catch up
        }
    }
    smbEngine = nullptr;

    double seconds = (double)(SDL_GetTicksNS() - started) / SDL_NS_PER_SECOND;
    if (seconds > 0 && framesRun > 0) {
        printf("%lu frames in %.1f s, %lu drawn (%.1f per second)\n", framesRun, seconds,
               framesDrawn, framesDrawn / seconds);
        printf("Per frame: game %.1f ms", nsUpdate / 1e6 / framesRun);
        if (framesDrawn > 0)
            printf(", picture %.1f ms, to screen %.1f ms", nsRender / 1e6 / framesDrawn,
                   nsPresent / 1e6 / framesDrawn);
        printf(" (60 per second needs 16.7 ms in all)\n");
    }
}

// ---------------------------------------------------------------- main
static void printHelp()
{
    printf("Super Mario Bros. for MS-DOS\n"
           "\n"
           "  SMB.EXE              play\n"
           "  SMB.EXE /?           this text\n"
           "\n"
           "Keys: arrows move, X jump (A), Z run/fire (B), Enter start,\n"
           "      Backspace/Right Shift select, R reset, F5-F8 save,\n"
           "      Shift+F5-F8 load, Esc quit.\n"
           "\n"
           "Environment:\n"
           "  SET SMB_VIDEO=640x480x16   video mode (WxHxBITS); 640x480x8, 320x240x16,\n"
           "                             320x200x8 (VGA mode 13h) ...\n"
           "  SET SMB_FRAMESKIP=1        draw every 2nd frame (default: automatic)\n"
           "  SET SMB_NOSOUND=1          no sound\n"
           "  SET SMB_NOJOY=1            ignore the gameport\n"
           "  SET BLASTER=A220 I7 D1 H5 T6   the Sound Blaster\n"
           "\n"
           "Settings: SMBC.CON (audio.frequency, video.palette_file, keys ...).\n");
}

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "/?") || !strcmp(argv[i], "-h") || !strcmp(argv[i], "--help") ||
            !strcmp(argv[i], "-?")) {
            printHelp();
            return 0;
        }
    }

    Configuration::initialize(CONFIG_FILE_NAME);

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        printf("SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    // Custom palette (before the window: an 8-bit mode's palette is made from it).
    if (!Configuration::getPaletteFileName().empty()) {
        const uint32_t* palette = loadPalette(Configuration::getPaletteFileName());
        if (palette) paletteRGB = palette;
    }
    if (Configuration::getHqdn3dEnabled()) {
        initHQDN3D(RENDER_WIDTH, RENDER_HEIGHT);
        memset(prevFrameBuffer, 0, sizeof(prevFrameBuffer));
    }

    window = dosCreateWindow(APP_TITLE);
    if (window == nullptr) {
        printf("Could not set a video mode: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_HideCursor();

    openAudio();
    openJoystick();

    mainLoop();

    if (audioStream) SDL_DestroyAudioStream(audioStream);
    if (joystick) SDL_CloseJoystick(joystick);
    if (Configuration::getHqdn3dEnabled()) cleanupHQDN3D();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

#endif // __DJGPP__
