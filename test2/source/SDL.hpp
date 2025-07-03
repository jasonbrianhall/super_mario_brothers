#ifndef SDL_ALLEGRO_WRAPPER_H
#define SDL_ALLEGRO_WRAPPER_H

#include <cstdint>

static int prev_button_state[4][32] = {0}; // 4 joysticks, 32 buttons max
static int prev_axis_state[4][8] = {0};    // 4 joysticks, 8 axes max
static int prev_hat_state[4][4] = {0};     // 4 joysticks, 4 hats max

// SDL constants used in your code
#define SDL_INIT_VIDEO          0x00000020
#define SDL_INIT_AUDIO          0x00000010
#define SDL_INIT_JOYSTICK       0x00000200
#define SDL_INIT_GAMECONTROLLER 0x00002000

#define SDL_WINDOWPOS_UNDEFINED 0x1FFF0000
#define SDL_WINDOW_FULLSCREEN_DESKTOP 0x00001001

#define SDL_RENDERER_ACCELERATED    0x00000002
#define SDL_RENDERER_PRESENTVSYNC   0x00000004
#define SDL_RENDERER_TARGETTEXTURE  0x00000008

#define SDL_PIXELFORMAT_ARGB8888    0x16362004
#define SDL_TEXTUREACCESS_STREAMING 1
#define SDL_TEXTUREACCESS_STATIC    0

#define AUDIO_S8    0x8008

// Blend modes
#define SDL_BLENDMODE_NONE          0x00000000
#define SDL_BLENDMODE_BLEND         0x00000001
#define SDL_BLENDMODE_ADD           0x00000002
#define SDL_BLENDMODE_MOD           0x00000004

// Hint names
#define SDL_HINT_RENDER_SCALE_QUALITY "SDL_RENDER_SCALE_QUALITY"

// Event types your code uses
#define SDL_QUIT                     0x100
#define SDL_WINDOWEVENT              0x200
#define SDL_WINDOWEVENT_CLOSE        0x00000200
#define SDL_KEYDOWN                  0x300
#define SDL_KEYUP                    0x301
#define SDL_JOYAXISMOTION            0x600
#define SDL_JOYBUTTONDOWN            0x603
#define SDL_JOYBUTTONUP              0x604
#define SDL_JOYHATMOTION             0x605
#define SDL_CONTROLLERBUTTONDOWN     0x650
#define SDL_CONTROLLERBUTTONUP       0x651
#define SDL_CONTROLLERAXISMOTION     0x654

// Joystick hat constants
#define SDL_HAT_CENTERED             0x00
#define SDL_HAT_UP                   0x01
#define SDL_HAT_RIGHT                0x02
#define SDL_HAT_DOWN                 0x04
#define SDL_HAT_LEFT                 0x08

// Event state constants
#define SDL_ENABLE                   1
#define SDL_DISABLE                  0

// Scancodes your code uses
#define SDL_SCANCODE_X          24  // KEY_X
#define SDL_SCANCODE_Z          26  // KEY_Z
#define SDL_SCANCODE_BACKSPACE  14  // KEY_BACKSPACE
#define SDL_SCANCODE_RETURN     28  // KEY_ENTER
#define SDL_SCANCODE_UP         72  // KEY_UP
#define SDL_SCANCODE_DOWN       80  // KEY_DOWN
#define SDL_SCANCODE_LEFT       75  // KEY_LEFT
#define SDL_SCANCODE_RIGHT      77  // KEY_RIGHT
#define SDL_SCANCODE_R          19  // KEY_R
#define SDL_SCANCODE_ESCAPE     1   // KEY_ESC
#define SDL_SCANCODE_F          33  // KEY_F
#define SDL_SCANCODE_D          32  // KEY_D
#define SDL_SCANCODE_RSHIFT     54  // KEY_RSHIFT
#define SDL_SCANCODE_I          23  // KEY_I
#define SDL_SCANCODE_K          37  // KEY_K
#define SDL_SCANCODE_J          36  // KEY_J
#define SDL_SCANCODE_L          38  // KEY_L
#define SDL_SCANCODE_N          49  // KEY_N
#define SDL_SCANCODE_M          50  // KEY_M
#define SDL_SCANCODE_RCTRL      97  // KEY_RCTRL
#define SDL_SCANCODE_SPACE      57  // KEY_SPACE

// Simple type definitions
typedef uint8_t Uint8;
typedef uint32_t Uint32;
typedef int16_t Sint16;
typedef uint32_t SDL_Scancode;

// SDL uses opaque pointers - just define them as void pointers
typedef void SDL_Window;
typedef void SDL_Renderer; 
typedef void SDL_Texture;
typedef void SDL_Joystick;
typedef void SDL_GameController;

// Joystick/Controller enums and types
typedef enum {
    SDL_CONTROLLER_BUTTON_A,
    SDL_CONTROLLER_BUTTON_B,
    SDL_CONTROLLER_BUTTON_X,
    SDL_CONTROLLER_BUTTON_Y,
    SDL_CONTROLLER_BUTTON_BACK,
    SDL_CONTROLLER_BUTTON_GUIDE,
    SDL_CONTROLLER_BUTTON_START,
    SDL_CONTROLLER_BUTTON_LEFTSTICK,
    SDL_CONTROLLER_BUTTON_RIGHTSTICK,
    SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
    SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
    SDL_CONTROLLER_BUTTON_DPAD_UP,
    SDL_CONTROLLER_BUTTON_DPAD_DOWN,
    SDL_CONTROLLER_BUTTON_DPAD_LEFT,
    SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
    SDL_CONTROLLER_BUTTON_MAX
} SDL_GameControllerButton;

typedef enum {
    SDL_CONTROLLER_AXIS_LEFTX,
    SDL_CONTROLLER_AXIS_LEFTY,
    SDL_CONTROLLER_AXIS_RIGHTX,
    SDL_CONTROLLER_AXIS_RIGHTY,
    SDL_CONTROLLER_AXIS_TRIGGERLEFT,
    SDL_CONTROLLER_AXIS_TRIGGERRIGHT,
    SDL_CONTROLLER_AXIS_MAX
} SDL_GameControllerAxis;

// SDL structure for renderer info
struct SDL_RendererInfo {
    const char* name;
    uint32_t flags;
    uint32_t num_texture_formats;
    uint32_t texture_formats[16];
    int max_texture_width;
    int max_texture_height;
};

// Structures for the functions you use
struct SDL_Event {
    uint32_t type;
    struct {
        uint32_t event;
    } window;
    struct {
        int axis;
        int value;
        int which;
    } jaxis;
    struct {
        int button;
        int which;
    } jbutton;
    struct {
        int hat;
        int value;
        int which;
    } jhat;
    struct {
        int button;
        int which;
    } cbutton;
    struct {
        int axis;
        int value;
        int which;
    } caxis;
    struct {
        struct {
            SDL_Scancode scancode;
        } keysym;
    } key;
};

struct SDL_AudioSpec {
    int freq;
    uint16_t format;
    uint8_t channels;
    uint16_t samples;
    void (*callback)(void* userdata, uint8_t* stream, int len);
    void* userdata;
};

// Function declarations
int SDL_Init(uint32_t flags);
void SDL_Quit();

SDL_Window* SDL_CreateWindow(const char* title, int x, int y, int w, int h, uint32_t flags);
void SDL_DestroyWindow(SDL_Window* window);

SDL_Renderer* SDL_CreateRenderer(SDL_Window* window, int index, uint32_t flags);
void SDL_DestroyRenderer(SDL_Renderer* renderer);
int SDL_RenderSetLogicalSize(SDL_Renderer* renderer, int w, int h);
int SDL_RenderClear(SDL_Renderer* renderer);
int SDL_RenderCopy(SDL_Renderer* renderer, SDL_Texture* texture, const void* srcrect, const void* dstrect);
void SDL_RenderPresent(SDL_Renderer* renderer);
int SDL_GetRendererInfo(SDL_Renderer* renderer, SDL_RendererInfo* info);

SDL_Texture* SDL_CreateTexture(SDL_Renderer* renderer, uint32_t format, int access, int w, int h);
void SDL_DestroyTexture(SDL_Texture* texture);
int SDL_UpdateTexture(SDL_Texture* texture, const void* rect, const void* pixels, int pitch);
int SDL_SetTextureBlendMode(SDL_Texture* texture, int blendMode);

int SDL_PollEvent(SDL_Event* event);
const Uint8* SDL_GetKeyboardState(int* numkeys);

int SDL_GetTicks();
void SDL_Delay(int ms);

const char* SDL_GetError();

int SDL_OpenAudio(SDL_AudioSpec* desired, SDL_AudioSpec* obtained);
void SDL_PauseAudio(int pause_on);
void SDL_CloseAudio();
void SDL_LockAudio();
void SDL_UnlockAudio();

int SDL_SetWindowFullscreen(SDL_Window* window, uint32_t flags);

// Joystick/Controller functions
int SDL_NumJoysticks();
SDL_Joystick* SDL_JoystickOpen(int device_index);
void SDL_JoystickClose(SDL_Joystick* joystick);
const char* SDL_JoystickName(SDL_Joystick* joystick);
const char* SDL_JoystickNameForIndex(int device_index);
int SDL_JoystickNumAxes(SDL_Joystick* joystick);
int SDL_JoystickNumButtons(SDL_Joystick* joystick);
int SDL_JoystickNumHats(SDL_Joystick* joystick);
Sint16 SDL_JoystickGetAxis(SDL_Joystick* joystick, int axis);
uint8_t SDL_JoystickGetButton(SDL_Joystick* joystick, int button);
uint8_t SDL_JoystickGetHat(SDL_Joystick* joystick, int hat);
int SDL_JoystickInstanceID(SDL_Joystick* joystick);
void SDL_JoystickEventState(int state);

SDL_GameController* SDL_GameControllerOpen(int joystick_index);
void SDL_GameControllerClose(SDL_GameController* gamecontroller);
const char* SDL_GameControllerName(SDL_GameController* gamecontroller);
Sint16 SDL_GameControllerGetAxis(SDL_GameController* gamecontroller, SDL_GameControllerAxis axis);
uint8_t SDL_GameControllerGetButton(SDL_GameController* gamecontroller, SDL_GameControllerButton button);
int SDL_IsGameController(int joystick_index);
SDL_Joystick* SDL_GameControllerGetJoystick(SDL_GameController* gamecontroller);
void SDL_GameControllerEventState(int state);
int SDL_GameControllerAddMappingsFromFile(const char* file);

// SDL subsystem functions
int SDL_WasInit(uint32_t flags);
int SDL_InitSubSystem(uint32_t flags);
void SDL_QuitSubSystem(uint32_t flags);
int SDL_SetHint(const char* name, const char* value);

#endif // SDL_ALLEGRO_WRAPPER_H
