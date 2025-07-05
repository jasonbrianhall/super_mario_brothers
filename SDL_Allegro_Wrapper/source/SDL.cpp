#include "SDL.hpp"
#include <allegro.h>
#include <cstring>
#include <iostream>
#include <sys/time.h>

// Internal structures (actual implementations)
struct _SDL_Window {
    int width, height;
};

struct _SDL_Renderer {
    BITMAP* backbuffer;
    int logical_width, logical_height;
};

struct _SDL_Texture {
    BITMAP* bitmap;
    int width, height;
    int blend_mode;
};

struct _SDL_Joystick {
    int index;
    bool connected;
    int instance_id;
};

struct _SDL_GameController {
    int index;
    bool connected;
};

// Global state
static _SDL_Window* g_window = nullptr;
static _SDL_Renderer* g_renderer = nullptr;
static uint8_t g_keyboard_state[256];
static SDL_AudioSpec g_audio_spec;
static bool g_audio_playing = false;
static _SDL_Joystick* g_joysticks[4] = {nullptr};
static _SDL_GameController* g_controllers[4] = {nullptr};
static uint32_t g_initialized_subsystems = 0;
static int g_next_instance_id = 1;

// Global timing state using system time instead of clock()
static bool g_timing_initialized = false;
static struct timeval g_start_time;

// Audio timer callback
static void audio_timer_callback() {
    if (g_audio_playing && g_audio_spec.callback) {
        static uint8_t audio_buffer[4096];
        g_audio_spec.callback(g_audio_spec.userdata, audio_buffer, g_audio_spec.samples);
    }
}
END_OF_FUNCTION(audio_timer_callback)

int SDL_GetTicks() {
    if (!g_timing_initialized) {
        gettimeofday(&g_start_time, NULL);
        g_timing_initialized = true;
        return 0;
    }
    
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    
    // Calculate elapsed time in milliseconds
    long seconds = current_time.tv_sec - g_start_time.tv_sec;
    long microseconds = current_time.tv_usec - g_start_time.tv_usec;
    
    return (int)(seconds * 1000 + microseconds / 1000);
}

void SDL_Delay(int ms) {
    if (ms <= 0) return;
    rest(ms);
}

// SDL Function implementations with conditional compilation
#ifdef __DJGPP__
int SDL_Init(unsigned long flags) {
#else
int SDL_Init(uint32_t flags) {
#endif
    if (allegro_init() != 0) return -1;
    
    if (flags & SDL_INIT_VIDEO) {
        set_color_depth(32);
        if (set_gfx_mode(GFX_AUTODETECT_WINDOWED, 800, 600, 0, 0) != 0) {
            if (set_gfx_mode(GFX_SAFE, 320, 200, 0, 0) != 0) {
                return -1;
            }
        }
        g_initialized_subsystems |= SDL_INIT_VIDEO;
    }
    
    if (flags & SDL_INIT_AUDIO) {
        std::cout << "Audio init - installing sound but disabling callbacks" << std::endl;
        if (install_sound(DIGI_AUTODETECT, MIDI_NONE, NULL) != 0) {
            std::cout << "Warning: Could not initialize audio" << std::endl;
        }
        g_initialized_subsystems |= SDL_INIT_AUDIO;
    }
    
    if (flags & (SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER)) {
        std::cout << "Initializing joystick support..." << std::endl;
        install_joystick(JOY_TYPE_AUTODETECT);
        std::cout << "Allegro detected " << num_joysticks << " joysticks" << std::endl;
        g_initialized_subsystems |= (flags & (SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER));
    }
    
    install_keyboard();
    install_timer();
    
    // Initialize keyboard state
    memset(g_keyboard_state, 0, sizeof(g_keyboard_state));
    
    return 0;
}

void SDL_Quit() {
    if (g_renderer) {
        SDL_DestroyRenderer(g_renderer);
        g_renderer = nullptr;
    }
    if (g_window) {
        SDL_DestroyWindow(g_window);
        g_window = nullptr;
    }
    allegro_exit();
}

#ifdef __DJGPP__
SDL_Window* SDL_CreateWindow(const char* title, int x, int y, int w, int h, unsigned long flags) {
#else
SDL_Window* SDL_CreateWindow(const char* title, int x, int y, int w, int h, uint32_t flags) {
#endif
    g_window = new _SDL_Window();
    g_window->width = w;
    g_window->height = h;
    set_window_title(title);
    return (SDL_Window*)g_window;
}

void SDL_DestroyWindow(SDL_Window* window) {
    if (window) {
        delete (_SDL_Window*)window;
    }
}

#ifdef __DJGPP__
SDL_Renderer* SDL_CreateRenderer(SDL_Window* window, int index, unsigned long flags) {
#else
SDL_Renderer* SDL_CreateRenderer(SDL_Window* window, int index, uint32_t flags) {
#endif
    _SDL_Window* win = (_SDL_Window*)window;
    if (!win) return nullptr;
    
    g_renderer = new _SDL_Renderer();
    g_renderer->backbuffer = create_bitmap(win->width, win->height);
    g_renderer->logical_width = win->width;
    g_renderer->logical_height = win->height;
    
    if (!g_renderer->backbuffer) {
        delete g_renderer;
        g_renderer = nullptr;
        return nullptr;
    }
    
    return (SDL_Renderer*)g_renderer;
}

void SDL_DestroyRenderer(SDL_Renderer* renderer) {
    if (renderer) {
        _SDL_Renderer* rend = (_SDL_Renderer*)renderer;
        if (rend->backbuffer) {
            destroy_bitmap(rend->backbuffer);
        }
        delete rend;
    }
}

int SDL_RenderSetLogicalSize(SDL_Renderer* renderer, int w, int h) {
    if (!renderer) return -1;
    _SDL_Renderer* rend = (_SDL_Renderer*)renderer;
    rend->logical_width = w;
    rend->logical_height = h;
    return 0;
}

int SDL_RenderClear(SDL_Renderer* renderer) {
    if (!renderer) return -1;
    _SDL_Renderer* rend = (_SDL_Renderer*)renderer;
    if (!rend->backbuffer) return -1;
    clear_bitmap(rend->backbuffer);
    return 0;
}

int SDL_RenderCopy(SDL_Renderer* renderer, SDL_Texture* texture, const void* srcrect, const void* dstrect) {
    if (!renderer || !texture) return -1;
    _SDL_Renderer* rend = (_SDL_Renderer*)renderer;
    _SDL_Texture* tex = (_SDL_Texture*)texture;
    if (!rend->backbuffer || !tex->bitmap) return -1;
    
    stretch_blit(tex->bitmap, rend->backbuffer, 
                 0, 0, tex->width, tex->height,
                 0, 0, rend->logical_width, rend->logical_height);
    
    return 0;
}

void SDL_RenderPresent(SDL_Renderer* renderer) {
    if (!renderer) return;
    _SDL_Renderer* rend = (_SDL_Renderer*)renderer;
    if (!rend->backbuffer) return;
    
    stretch_blit(rend->backbuffer, screen,
                 0, 0, rend->logical_width, rend->logical_height,
                 0, 0, SCREEN_W, SCREEN_H);
}

#ifdef __DJGPP__
SDL_Texture* SDL_CreateTexture(SDL_Renderer* renderer, unsigned long format, int access, int w, int h) {
#else
SDL_Texture* SDL_CreateTexture(SDL_Renderer* renderer, uint32_t format, int access, int w, int h) {
#endif
    if (!renderer) return nullptr;
    
    _SDL_Texture* texture = new _SDL_Texture();
    texture->bitmap = create_bitmap(w, h);
    texture->width = w;
    texture->height = h;
    texture->blend_mode = 0; // SDL_BLENDMODE_NONE
    
    if (!texture->bitmap) {
        delete texture;
        return nullptr;
    }
    
    return (SDL_Texture*)texture;
}

void SDL_DestroyTexture(SDL_Texture* texture) {
    if (texture) {
        _SDL_Texture* tex = (_SDL_Texture*)texture;
        if (tex->bitmap) {
            destroy_bitmap(tex->bitmap);
        }
        delete tex;
    }
}

int SDL_UpdateTexture(SDL_Texture* texture, const void* rect, const void* pixels, int pitch) {
    if (!texture || !pixels) return -1;
    _SDL_Texture* tex = (_SDL_Texture*)texture;
    if (!tex->bitmap) return -1;
    
    uint32_t* src = (uint32_t*)pixels;
    int width = tex->width;
    int height = tex->height;
    
    // Convert ARGB to Allegro's RGB format
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint32_t pixel = src[y * width + x];
            int r = (pixel >> 16) & 0xFF;
            int g = (pixel >> 8) & 0xFF;
            int b = pixel & 0xFF;
            putpixel(tex->bitmap, x, y, makecol(r, g, b));
        }
    }
    
    return 0;
}

int SDL_PollEvent(SDL_Event* event) {
    if (!event) return 0;
    
    // Poll input devices
    if (keyboard_needs_poll()) {
        poll_keyboard();
    }
    poll_joystick();
    
    // Check for joystick events by comparing current state with previous state
    for (int joy_index = 0; joy_index < num_joysticks && joy_index < 4; joy_index++) {
        // Check button state changes
        for (int button = 0; button < joy[joy_index].num_buttons && button < 32; button++) {
            int current_state = joy[joy_index].button[button].b ? 1 : 0;
            int previous_state = prev_button_state[joy_index][button];
            
            if (current_state != previous_state) {
                event->type = current_state ? SDL_JOYBUTTONDOWN : SDL_JOYBUTTONUP;
                event->jbutton.which = joy_index;
                event->jbutton.button = button;
                prev_button_state[joy_index][button] = current_state;
                return 1; // Event generated
            }
        }
        
        // Check axis state changes (with deadzone)
        const int AXIS_DEADZONE = 8000; // Adjust as needed
        for (int stick = 0; stick < joy[joy_index].num_sticks && stick < 4; stick++) {
            for (int axis_type = 0; axis_type < 2; axis_type++) {
                int axis_index = stick * 2 + axis_type;
                if (axis_index >= 8) break;
                
                int current_value = joy[joy_index].stick[stick].axis[axis_type].pos * 128;
                int previous_value = prev_axis_state[joy_index][axis_index];
                
                // Only generate event if change is significant
                if (abs(current_value - previous_value) > AXIS_DEADZONE) {
                    event->type = SDL_JOYAXISMOTION;
                    event->jaxis.which = joy_index;
                    event->jaxis.axis = axis_index;
                    event->jaxis.value = current_value;
                    prev_axis_state[joy_index][axis_index] = current_value;
                    return 1; // Event generated
                }
            }
        }
        
        // Check hat state changes
        if (joy[joy_index].num_sticks > 0) {
            uint8_t current_hat = SDL_HAT_CENTERED;
            int x = joy[joy_index].stick[0].axis[0].pos;
            int y = joy[joy_index].stick[0].axis[1].pos;
            
            if (y < -64) current_hat |= SDL_HAT_UP;
            if (y > 64) current_hat |= SDL_HAT_DOWN;
            if (x < -64) current_hat |= SDL_HAT_LEFT;
            if (x > 64) current_hat |= SDL_HAT_RIGHT;
            
            if (current_hat != prev_hat_state[joy_index][0]) {
                event->type = SDL_JOYHATMOTION;
                event->jhat.which = joy_index;
                event->jhat.hat = 0;
                event->jhat.value = current_hat;
                prev_hat_state[joy_index][0] = current_hat;
                return 1; // Event generated
            }
        }
    }
    
    // No events generated
    return 0;
}

const Uint8* SDL_GetKeyboardState(int* numkeys) {
    if (numkeys) *numkeys = 256;
    
    // Poll keyboard first to ensure we have current state
    if (keyboard_needs_poll()) {
        poll_keyboard();
    }
    
    // Clear the keyboard state array
    memset(g_keyboard_state, 0, sizeof(g_keyboard_state));
    
    // Map Allegro key states to SDL scancodes
    if (key[KEY_X]) g_keyboard_state[SDL_SCANCODE_X] = 1;
    if (key[KEY_Z]) g_keyboard_state[SDL_SCANCODE_Z] = 1;
    if (key[KEY_BACKSPACE]) g_keyboard_state[SDL_SCANCODE_BACKSPACE] = 1;
    if (key[KEY_ENTER]) g_keyboard_state[SDL_SCANCODE_RETURN] = 1;
    if (key[KEY_UP]) g_keyboard_state[SDL_SCANCODE_UP] = 1;
    if (key[KEY_DOWN]) g_keyboard_state[SDL_SCANCODE_DOWN] = 1;
    if (key[KEY_LEFT]) g_keyboard_state[SDL_SCANCODE_LEFT] = 1;
    if (key[KEY_RIGHT]) g_keyboard_state[SDL_SCANCODE_RIGHT] = 1;
    if (key[KEY_R]) g_keyboard_state[SDL_SCANCODE_R] = 1;
    if (key[KEY_ESC]) g_keyboard_state[SDL_SCANCODE_ESCAPE] = 1;
    if (key[KEY_F]) g_keyboard_state[SDL_SCANCODE_F] = 1;
    if (key[KEY_D]) g_keyboard_state[SDL_SCANCODE_D] = 1;
    if (key[KEY_RSHIFT]) g_keyboard_state[SDL_SCANCODE_RSHIFT] = 1;
    if (key[KEY_I]) g_keyboard_state[SDL_SCANCODE_I] = 1;
    if (key[KEY_K]) g_keyboard_state[SDL_SCANCODE_K] = 1;
    if (key[KEY_J]) g_keyboard_state[SDL_SCANCODE_J] = 1;
    if (key[KEY_L]) g_keyboard_state[SDL_SCANCODE_L] = 1;
    if (key[KEY_N]) g_keyboard_state[SDL_SCANCODE_N] = 1;
    if (key[KEY_M]) g_keyboard_state[SDL_SCANCODE_M] = 1;
    if (key[KEY_RCONTROL]) g_keyboard_state[SDL_SCANCODE_RCTRL] = 1;
    if (key[KEY_SPACE]) g_keyboard_state[SDL_SCANCODE_SPACE] = 1;
    
    return g_keyboard_state;
}

const char* SDL_GetError() {
    return "SDL wrapper error";
}

int SDL_OpenAudio(SDL_AudioSpec* desired, SDL_AudioSpec* obtained) {
    if (!desired) return -1;
    
    std::cout << "SDL_OpenAudio called - DISABLED for debugging" << std::endl;
    
    // Store the spec but don't actually set up audio timer
    g_audio_spec = *desired;
    if (obtained) *obtained = *desired;
    
    // DON'T install any audio timer - this might be causing memory corruption
    
    return 0;
}

void SDL_PauseAudio(int pause_on) {
    std::cout << "SDL_PauseAudio called: " << pause_on << std::endl;
    // Don't change audio state
}

void SDL_CloseAudio() {
    std::cout << "SDL_CloseAudio called" << std::endl;
    // Don't remove any timers since we didn't install any
}

void SDL_LockAudio() {
    // Complete no-op - audio is disabled
}

void SDL_UnlockAudio() {
    // Complete no-op - audio is disabled  
}

#ifdef __DJGPP__
int SDL_SetWindowFullscreen(SDL_Window* window, unsigned long flags) {
#else
int SDL_SetWindowFullscreen(SDL_Window* window, uint32_t flags) {
#endif
    if (!window) return -1;
    _SDL_Window* win = (_SDL_Window*)window;
    
    if (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) {
        set_gfx_mode(GFX_AUTODETECT_FULLSCREEN, win->width, win->height, 0, 0);
    } else {
        set_gfx_mode(GFX_AUTODETECT_WINDOWED, win->width, win->height, 0, 0);
    }
    
    return 0;
}

int SDL_SetTextureBlendMode(SDL_Texture* texture, int blendMode) {
    if (!texture) return -1;
    
    _SDL_Texture* tex = (_SDL_Texture*)texture;
    tex->blend_mode = blendMode;
    
    return 0;
}

int SDL_GetRendererInfo(SDL_Renderer* renderer, SDL_RendererInfo* info) {
    if (!renderer || !info) return -1;
    
    // Fill in some basic renderer info
    info->name = "Allegro Wrapper";
    info->flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE;
    info->num_texture_formats = 1;
    info->texture_formats[0] = SDL_PIXELFORMAT_ARGB8888;
    info->max_texture_width = 4096;
    info->max_texture_height = 4096;
    
    return 0;
}

int SDL_SetHint(const char* name, const char* value) {
    // For now, just return success without actually setting any hints
    return 1; // SDL_TRUE
}

// Joystick/Controller functions with debug output
int SDL_NumJoysticks() {
    std::cout << "SDL_NumJoysticks() called, returning: " << num_joysticks << std::endl;
    return num_joysticks;
}

SDL_Joystick* SDL_JoystickOpen(int device_index) {
    std::cout << "SDL_JoystickOpen(" << device_index << ") called" << std::endl;
    
    if (device_index >= 0 && device_index < num_joysticks && device_index < 4) {
        if (!g_joysticks[device_index]) {
            g_joysticks[device_index] = new _SDL_Joystick();
            g_joysticks[device_index]->index = device_index;
            g_joysticks[device_index]->connected = true;
            g_joysticks[device_index]->instance_id = g_next_instance_id++;
            std::cout << "Created joystick object for device " << device_index << std::endl;
        }
        return (SDL_Joystick*)g_joysticks[device_index];
    }
    
    std::cout << "SDL_JoystickOpen failed - invalid device index" << std::endl;
    return nullptr;
}

void SDL_JoystickClose(SDL_Joystick* joystick) {
    if (joystick) {
        _SDL_Joystick* joy_ptr = (_SDL_Joystick*)joystick;
        if (joy_ptr->index >= 0 && joy_ptr->index < 4) {
            delete joy_ptr;
            g_joysticks[joy_ptr->index] = nullptr;
        }
    }
}

const char* SDL_JoystickName(SDL_Joystick* joystick) {
    if (joystick) {
        _SDL_Joystick* joy_ptr = (_SDL_Joystick*)joystick;
        static char joy_name[32];
        sprintf(joy_name, "Joystick %d", joy_ptr->index);
        return joy_name;
    }
    return "Unknown";
}

const char* SDL_JoystickNameForIndex(int device_index) {
    static char joy_name[32];
    sprintf(joy_name, "Joystick %d", device_index);
    return joy_name;
}

int SDL_JoystickNumAxes(SDL_Joystick* joystick) {
    if (joystick) {
        _SDL_Joystick* joy_ptr = (_SDL_Joystick*)joystick;
        return joy[joy_ptr->index].num_sticks * 2;
    }
    return 0;
}

int SDL_JoystickNumButtons(SDL_Joystick* joystick) {
    if (joystick) {
        _SDL_Joystick* joy_ptr = (_SDL_Joystick*)joystick;
        return joy[joy_ptr->index].num_buttons;
    }
    return 0;
}

int SDL_JoystickNumHats(SDL_Joystick* joystick) {
    if (joystick) {
        _SDL_Joystick* joy_ptr = (_SDL_Joystick*)joystick;
        return joy[joy_ptr->index].num_sticks > 0 ? 1 : 0;
    }
    return 0;
}

Sint16 SDL_JoystickGetAxis(SDL_Joystick* joystick, int axis) {
    if (!joystick || axis < 0) return 0;
    
    _SDL_Joystick* jstick = (_SDL_Joystick*)joystick;
    if (jstick->index < 0 || jstick->index >= num_joysticks) return 0;
    
    poll_joystick();
    int stick = axis / 2;
    int axis_type = axis % 2;
    
    if (stick >= 0 && stick < joy[jstick->index].num_sticks) {
        if (axis_type == 0) {
            return joy[jstick->index].stick[stick].axis[0].pos * 128;
        } else {
            return joy[jstick->index].stick[stick].axis[1].pos * 128;
        }
    }
    
    return 0;
}

uint8_t SDL_JoystickGetButton(SDL_Joystick* joystick, int button) {
    if (!joystick || button < 0) {
        std::cout << "SDL_JoystickGetButton: Invalid joystick or button" << std::endl;
        return 0;
    }
    
    _SDL_Joystick* jstick = (_SDL_Joystick*)joystick;
    if (jstick->index < 0 || jstick->index >= num_joysticks) {
        std::cout << "SDL_JoystickGetButton: Invalid joystick index " << jstick->index << std::endl;
        return 0;
    }
    
    if (button >= joy[jstick->index].num_buttons) {
        std::cout << "SDL_JoystickGetButton: Button " << button << " >= num_buttons " << joy[jstick->index].num_buttons << std::endl;
        return 0;
    }
    
    // Poll joystick to ensure fresh data
    poll_joystick();
    
    uint8_t result = joy[jstick->index].button[button].b ? 1 : 0;
    
    // Debug output (can be removed later)
    static int debug_counter = 0;
    if (result && (debug_counter++ % 30 == 0)) { // Print every 30th button press to avoid spam
        std::cout << "Button " << button << " pressed on joystick " << jstick->index << std::endl;
    }
    
    return result;
}

uint8_t SDL_JoystickGetHat(SDL_Joystick* joystick, int hat) {
    if (joystick && hat == 0) {
        _SDL_Joystick* jstick = (_SDL_Joystick*)joystick;
        poll_joystick();
        
        if (joy[jstick->index].num_sticks > 0) {
            uint8_t hatval = SDL_HAT_CENTERED;
            int x = joy[jstick->index].stick[0].axis[0].pos;
            int y = joy[jstick->index].stick[0].axis[1].pos;
            
            if (y < -64) hatval |= SDL_HAT_UP;
            if (y > 64) hatval |= SDL_HAT_DOWN;
            if (x < -64) hatval |= SDL_HAT_LEFT;
            if (x > 64) hatval |= SDL_HAT_RIGHT;
            
            return hatval;
        }
    }
    return SDL_HAT_CENTERED;
}

int SDL_JoystickInstanceID(SDL_Joystick* joystick) {
    if (joystick) {
        _SDL_Joystick* jstick = (_SDL_Joystick*)joystick;
        return jstick->instance_id;
    }
    return -1;
}

void SDL_JoystickEventState(int state) {
    // No-op
}

int SDL_IsGameController(int joystick_index) {
    std::cout << "SDL_IsGameController(" << joystick_index << ") called" << std::endl;
    bool result = (joystick_index >= 0 && joystick_index < num_joysticks);
    std::cout << "SDL_IsGameController returning: " << result << std::endl;
    return result ? 1 : 0;
}

SDL_GameController* SDL_GameControllerOpen(int joystick_index) {
    std::cout << "SDL_GameControllerOpen(" << joystick_index << ") called" << std::endl;
    
    if (joystick_index >= 0 && joystick_index < num_joysticks && joystick_index < 4) {
        if (!g_controllers[joystick_index]) {
            g_controllers[joystick_index] = new _SDL_GameController();
            g_controllers[joystick_index]->index = joystick_index;
            g_controllers[joystick_index]->connected = true;
            std::cout << "Created game controller object for device " << joystick_index << std::endl;
        }
        return (SDL_GameController*)g_controllers[joystick_index];
    }
    
    std::cout << "SDL_GameControllerOpen failed - invalid joystick index" << std::endl;
    return nullptr;
}

void SDL_GameControllerClose(SDL_GameController* gamecontroller) {
    if (gamecontroller) {
        _SDL_GameController* ctrl = (_SDL_GameController*)gamecontroller;
        if (ctrl->index >= 0 && ctrl->index < 4) {
            delete ctrl;
            g_controllers[ctrl->index] = nullptr;
        }
    }
}

const char* SDL_GameControllerName(SDL_GameController* gamecontroller) {
    if (gamecontroller) {
        _SDL_GameController* ctrl = (_SDL_GameController*)gamecontroller;
        static char ctrl_name[32];
        sprintf(ctrl_name, "Controller %d", ctrl->index);
        return ctrl_name;
    }
    return "Unknown";
}

Sint16 SDL_GameControllerGetAxis(SDL_GameController* gamecontroller, SDL_GameControllerAxis axis) {
    if (gamecontroller) {
        _SDL_GameController* ctrl = (_SDL_GameController*)gamecontroller;
        poll_joystick();
        int joy_index = ctrl->index;
        
        switch (axis) {
            case SDL_CONTROLLER_AXIS_LEFTX:
                return joy[joy_index].stick[0].axis[0].pos * 128;
            case SDL_CONTROLLER_AXIS_LEFTY:
                return joy[joy_index].stick[0].axis[1].pos * 128;
            case SDL_CONTROLLER_AXIS_RIGHTX:
                if (joy[joy_index].num_sticks > 1)
                    return joy[joy_index].stick[1].axis[0].pos * 128;
                break;
            case SDL_CONTROLLER_AXIS_RIGHTY:
                if (joy[joy_index].num_sticks > 1)
                    return joy[joy_index].stick[1].axis[1].pos * 128;
                break;
            default:
                break;
        }
    }
    return 0;
}

uint8_t SDL_GameControllerGetButton(SDL_GameController* gamecontroller, SDL_GameControllerButton button) {
    if (!gamecontroller) {
        std::cout << "SDL_GameControllerGetButton: Invalid controller" << std::endl;
        return 0;
    }
    
    _SDL_GameController* ctrl = (_SDL_GameController*)gamecontroller;
    if (ctrl->index < 0 || ctrl->index >= num_joysticks) {
        std::cout << "SDL_GameControllerGetButton: Invalid controller index " << ctrl->index << std::endl;
        return 0;
    }
    
    poll_joystick();
    int joy_index = ctrl->index;
    
    // Map SDL controller buttons to Allegro joystick buttons
    switch (button) {
        case SDL_CONTROLLER_BUTTON_A:
            return (0 < joy[joy_index].num_buttons) ? joy[joy_index].button[0].b : 0;
        case SDL_CONTROLLER_BUTTON_B:
            return (1 < joy[joy_index].num_buttons) ? joy[joy_index].button[1].b : 0;
        case SDL_CONTROLLER_BUTTON_X:
            return (2 < joy[joy_index].num_buttons) ? joy[joy_index].button[2].b : 0;
        case SDL_CONTROLLER_BUTTON_Y:
            return (3 < joy[joy_index].num_buttons) ? joy[joy_index].button[3].b : 0;
        case SDL_CONTROLLER_BUTTON_BACK:
            return (4 < joy[joy_index].num_buttons) ? joy[joy_index].button[4].b : 0;
        case SDL_CONTROLLER_BUTTON_START:
            return (5 < joy[joy_index].num_buttons) ? joy[joy_index].button[5].b : 0;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
            return (6 < joy[joy_index].num_buttons) ? joy[joy_index].button[6].b : 0;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
            return (7 < joy[joy_index].num_buttons) ? joy[joy_index].button[7].b : 0;
        default:
            if (button < joy[joy_index].num_buttons) {
                return joy[joy_index].button[button].b;
            }
            break;
    }
    
    return 0;
}

// Add initialization function to clear previous states
void initializeJoystickStates() {
    memset(prev_button_state, 0, sizeof(prev_button_state));
    memset(prev_axis_state, 0, sizeof(prev_axis_state));
    memset(prev_hat_state, 0, sizeof(prev_hat_state));
}

SDL_Joystick* SDL_GameControllerGetJoystick(SDL_GameController* gamecontroller) {
    if (gamecontroller) {
        _SDL_GameController* ctrl = (_SDL_GameController*)gamecontroller;
        return SDL_JoystickOpen(ctrl->index);
    }
    return nullptr;
}

void SDL_GameControllerEventState(int state) {
    // No-op
}

int SDL_GameControllerAddMappingsFromFile(const char* file) {
    return 0;
}

// SDL subsystem functions with conditional compilation
#ifdef __DJGPP__
int SDL_WasInit(unsigned long flags) {
#else
int SDL_WasInit(uint32_t flags) {
#endif
    return g_initialized_subsystems & flags;
}

#ifdef __DJGPP__
int SDL_InitSubSystem(unsigned long flags) {
#else
int SDL_InitSubSystem(uint32_t flags) {
#endif
    return SDL_Init(flags);
}

#ifdef __DJGPP__
void SDL_QuitSubSystem(unsigned long flags) {
#else
void SDL_QuitSubSystem(uint32_t flags) {
#endif
    g_initialized_subsystems &= ~flags;
}
