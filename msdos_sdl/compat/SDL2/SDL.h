/*
 * SDL2 -> SDL3 compatibility for the MS-DOS build.
 *
 * The game sources include <SDL2/SDL.h> and are written for SDL2; SDL3 is the
 * one with a DOS port. On DOS this header is found first (-Icompat): it
 * includes SDL3 and maps the SDL2 calls, types and constants the shared
 * sources use (Emulation/Controller.cpp, Util/Video.cpp, Util/VideoFilters.cpp)
 * onto SDL3. The DOS front end (DOSMain.cpp, DOSVideo.cpp) uses SDL3 directly.
 *
 * Only names that do not exist in SDL3 are defined here, so SDL3 code in the
 * same file is unaffected.
 */
#ifndef SMB_SDL2_COMPAT_H
#define SMB_SDL2_COMPAT_H

#include <SDL3/SDL.h>

/* ---------------------------------------------------------------- init */
#undef SDL_INIT_GAMECONTROLLER
#define SDL_INIT_GAMECONTROLLER SDL_INIT_GAMEPAD

#ifndef SDL_ENABLE
#define SDL_ENABLE  1
#define SDL_DISABLE 0
#endif

/* ---------------------------------------------------------------- events */
#undef SDL_QUIT
#undef SDL_KEYDOWN
#undef SDL_KEYUP
#undef SDL_JOYAXISMOTION
#undef SDL_JOYHATMOTION
#undef SDL_JOYBUTTONDOWN
#undef SDL_JOYBUTTONUP
#undef SDL_CONTROLLERBUTTONDOWN
#undef SDL_CONTROLLERBUTTONUP
#undef SDL_CONTROLLERAXISMOTION
#define SDL_QUIT                  SDL_EVENT_QUIT
#define SDL_KEYDOWN               SDL_EVENT_KEY_DOWN
#define SDL_KEYUP                 SDL_EVENT_KEY_UP
#define SDL_JOYAXISMOTION         SDL_EVENT_JOYSTICK_AXIS_MOTION
#define SDL_JOYHATMOTION          SDL_EVENT_JOYSTICK_HAT_MOTION
#define SDL_JOYBUTTONDOWN         SDL_EVENT_JOYSTICK_BUTTON_DOWN
#define SDL_JOYBUTTONUP           SDL_EVENT_JOYSTICK_BUTTON_UP
#define SDL_CONTROLLERBUTTONDOWN  SDL_EVENT_GAMEPAD_BUTTON_DOWN
#define SDL_CONTROLLERBUTTONUP    SDL_EVENT_GAMEPAD_BUTTON_UP
#define SDL_CONTROLLERAXISMOTION  SDL_EVENT_GAMEPAD_AXIS_MOTION

/* SDL_Event members that were renamed (event.cbutton -> event.gbutton, ...) */
#define cbutton gbutton
#define caxis   gaxis

/* ---------------------------------------------------------------- gamepads */
#undef SDL_GameController
#undef SDL_GameControllerButton
#undef SDL_GameControllerAxis
typedef SDL_Gamepad       SDL_GameController;
typedef SDL_GamepadButton SDL_GameControllerButton;
typedef SDL_GamepadAxis   SDL_GameControllerAxis;

#undef SDL_CONTROLLER_BUTTON_A
#undef SDL_CONTROLLER_BUTTON_B
#undef SDL_CONTROLLER_BUTTON_BACK
#undef SDL_CONTROLLER_BUTTON_START
#undef SDL_CONTROLLER_BUTTON_DPAD_UP
#undef SDL_CONTROLLER_BUTTON_DPAD_DOWN
#undef SDL_CONTROLLER_BUTTON_DPAD_LEFT
#undef SDL_CONTROLLER_BUTTON_DPAD_RIGHT
#undef SDL_CONTROLLER_AXIS_LEFTX
#undef SDL_CONTROLLER_AXIS_LEFTY
#define SDL_CONTROLLER_BUTTON_A          SDL_GAMEPAD_BUTTON_SOUTH
#define SDL_CONTROLLER_BUTTON_B          SDL_GAMEPAD_BUTTON_EAST
#define SDL_CONTROLLER_BUTTON_BACK       SDL_GAMEPAD_BUTTON_BACK
#define SDL_CONTROLLER_BUTTON_START      SDL_GAMEPAD_BUTTON_START
#define SDL_CONTROLLER_BUTTON_DPAD_UP    SDL_GAMEPAD_BUTTON_DPAD_UP
#define SDL_CONTROLLER_BUTTON_DPAD_DOWN  SDL_GAMEPAD_BUTTON_DPAD_DOWN
#define SDL_CONTROLLER_BUTTON_DPAD_LEFT  SDL_GAMEPAD_BUTTON_DPAD_LEFT
#define SDL_CONTROLLER_BUTTON_DPAD_RIGHT SDL_GAMEPAD_BUTTON_DPAD_RIGHT
#define SDL_CONTROLLER_AXIS_LEFTX        SDL_GAMEPAD_AXIS_LEFTX
#define SDL_CONTROLLER_AXIS_LEFTY        SDL_GAMEPAD_AXIS_LEFTY

/* SDL2 opened devices by index (0..SDL_NumJoysticks()-1); SDL3 by instance
 * ID. SDL2_NumJoysticks() takes a snapshot of the ID list that the index
 * functions below look up. */
static SDL_JoystickID* sdl2_joystick_ids = NULL;
static int sdl2_joystick_count = 0;

static inline int SDL2_NumJoysticks(void) {
    SDL_free(sdl2_joystick_ids);
    sdl2_joystick_ids = SDL_GetJoysticks(&sdl2_joystick_count);
    if (sdl2_joystick_ids == NULL) sdl2_joystick_count = 0;
    return sdl2_joystick_count;
}
static inline SDL_JoystickID SDL2_JoystickIndexToID(int index) {
    if (sdl2_joystick_ids == NULL) SDL2_NumJoysticks();
    return (index >= 0 && index < sdl2_joystick_count) ? sdl2_joystick_ids[index] : 0;
}
static inline const char* SDL2_JoystickNameForIndex(int index) {
    const char* name = SDL_GetJoystickNameForID(SDL2_JoystickIndexToID(index));
    return name ? name : "(unnamed)";
}
static inline bool SDL2_IsGameController(int index) {
    return SDL_IsGamepad(SDL2_JoystickIndexToID(index));
}
static inline SDL_Gamepad* SDL2_GameControllerOpen(int index) {
    return SDL_OpenGamepad(SDL2_JoystickIndexToID(index));
}
static inline SDL_Joystick* SDL2_JoystickOpen(int index) {
    return SDL_OpenJoystick(SDL2_JoystickIndexToID(index));
}
static inline const char* SDL2_GameControllerName(SDL_Gamepad* g) {
    const char* name = SDL_GetGamepadName(g);
    return name ? name : "(unnamed)";
}
static inline const char* SDL2_JoystickName(SDL_Joystick* j) {
    const char* name = SDL_GetJoystickName(j);
    return name ? name : "(unnamed)";
}
static inline void SDL2_JoystickEventState(int state) {
    SDL_SetJoystickEventsEnabled(state == SDL_ENABLE);
}
static inline void SDL2_GameControllerEventState(int state) {
    SDL_SetGamepadEventsEnabled(state == SDL_ENABLE);
}
/* SDL2 returned 0 on success, negative on error; SDL3 returns true/false. */
static inline int SDL2_InitSubSystem(SDL_InitFlags flags) {
    return SDL_InitSubSystem(flags) ? 0 : -1;
}

#undef SDL_NumJoysticks
#undef SDL_JoystickNameForIndex
#undef SDL_IsGameController
#undef SDL_GameControllerOpen
#undef SDL_JoystickOpen
#undef SDL_GameControllerName
#undef SDL_JoystickName
#undef SDL_JoystickEventState
#undef SDL_GameControllerEventState
#undef SDL_GameControllerAddMappingsFromFile
#undef SDL_GameControllerClose
#undef SDL_GameControllerGetJoystick
#undef SDL_JoystickClose
#undef SDL_JoystickInstanceID
#undef SDL_JoystickNumButtons
#undef SDL_JoystickNumAxes
#undef SDL_JoystickNumHats
#undef SDL_JoystickGetButton
#undef SDL_JoystickGetHat
#undef SDL_JoystickGetAxis
#define SDL_NumJoysticks                     SDL2_NumJoysticks
#define SDL_JoystickNameForIndex             SDL2_JoystickNameForIndex
#define SDL_IsGameController                 SDL2_IsGameController
#define SDL_GameControllerOpen               SDL2_GameControllerOpen
#define SDL_JoystickOpen                     SDL2_JoystickOpen
#define SDL_GameControllerName               SDL2_GameControllerName
#define SDL_JoystickName                     SDL2_JoystickName
#define SDL_JoystickEventState               SDL2_JoystickEventState
#define SDL_GameControllerEventState         SDL2_GameControllerEventState
#define SDL_GameControllerAddMappingsFromFile SDL_AddGamepadMappingsFromFile
#define SDL_GameControllerClose              SDL_CloseGamepad
#define SDL_GameControllerGetJoystick        SDL_GetGamepadJoystick
#define SDL_JoystickClose                    SDL_CloseJoystick
#define SDL_JoystickInstanceID               SDL_GetJoystickID
#define SDL_JoystickNumButtons               SDL_GetNumJoystickButtons
#define SDL_JoystickNumAxes                  SDL_GetNumJoystickAxes
#define SDL_JoystickNumHats                  SDL_GetNumJoystickHats
#define SDL_JoystickGetButton                SDL_GetJoystickButton
#define SDL_JoystickGetHat                   SDL_GetJoystickHat
#define SDL_JoystickGetAxis                  SDL_GetJoystickAxis
#define SDL_InitSubSystem(flags)             SDL2_InitSubSystem(flags)

/* ---------------------------------------------------------------- renderer
 * Only referenced by the windowed front end's helpers (scanline texture,
 * MSAA check); the DOS front end does not call them, but they must build. */
#undef SDL_RendererInfo
#undef SDL_GetRendererInfo
typedef struct SDL_RendererInfo { const char* name; Uint32 flags; } SDL_RendererInfo;
#ifndef SDL_RENDERER_TARGETTEXTURE
#define SDL_RENDERER_TARGETTEXTURE 0x00000008
#endif
static inline int SDL_GetRendererInfo(SDL_Renderer* renderer, SDL_RendererInfo* info) {
    info->name = renderer ? SDL_GetRendererName(renderer) : "";
    info->flags = 0; /* no render targets worth using on DOS */
    return 0;
}
#undef SDL_HINT_RENDER_SCALE_QUALITY
#define SDL_HINT_RENDER_SCALE_QUALITY "SDL_RENDER_SCALE_QUALITY"

#endif /* SMB_SDL2_COMPAT_H */
