#include "SDL.h"
#include <allegro.h>
#include <cstring>
#include <iostream>

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

// Audio timer callback
static void audio_timer_callback() {
    if (g_audio_playing && g_audio_spec.callback) {
        static uint8_t audio_buffer[4096];
        g_audio_spec.callback(g_audio_spec.userdata, audio_buffer, g_audio_spec.samples);
    }
}
END_OF_FUNCTION(audio_timer_callback)

// Keyboard update
static void update_keyboard() {
    // Map Allegro key array to SDL scancode format
    g_keyboard_state[SDL_SCANCODE_X] = key[KEY_X] ? 1 : 0;
    g_keyboard_state[SDL_SCANCODE_Z] = key[KEY_Z] ? 1 : 0;
    g_keyboard_state[SDL_SCANCODE_BACKSPACE] = key[KEY_BACKSPACE] ? 1 : 0;
    g_keyboard_state[SDL_SCANCODE_RETURN] = key[KEY_ENTER] ? 1 : 0;
    g_keyboard_state[SDL_SCANCODE_UP] = key[KEY_UP] ? 1 : 0;
    g_keyboard_state[SDL_SCANCODE_DOWN] = key[KEY_DOWN] ? 1 : 0;
    g_keyboard_state[SDL_SCANCODE_LEFT] = key[KEY_LEFT] ? 1 : 0;
    g_keyboard_state[SDL_SCANCODE_RIGHT] = key[KEY_RIGHT] ? 1 : 0;
    g_keyboard_state[SDL_SCANCODE_R] = key[KEY_R] ? 1 : 0;
    g_keyboard_state[SDL_SCANCODE_ESCAPE] = key[KEY_ESC] ? 1 : 0;
    g_keyboard_state[SDL_SCANCODE_F] = key[KEY_F] ? 1 : 0;
    g_keyboard_state[SDL_SCANCODE_D] = key[KEY_D] ? 1 : 0;
    g_keyboard_state[SDL_SCANCODE_RSHIFT] = key[KEY_RSHIFT] ? 1 : 0;
    g_keyboard_state[SDL_SCANCODE_I] = key[KEY_I] ? 1 : 0;
    g_keyboard_state[SDL_SCANCODE_K] = key[KEY_K] ? 1 : 0;
    g_keyboard_state[SDL_SCANCODE_J] = key[KEY_J] ? 1 : 0;
    g_keyboard_state[SDL_SCANCODE_L] = key[KEY_L] ? 1 : 0;
    g_keyboard_state[SDL_SCANCODE_N] = key[KEY_N] ? 1 : 0;
    g_keyboard_state[SDL_SCANCODE_M] = key[KEY_M] ? 1 : 0;
    g_keyboard_state[SDL_SCANCODE_RCTRL] = key[KEY_RCONTROL] ? 1 : 0;
    g_keyboard_state[SDL_SCANCODE_SPACE] = key[KEY_SPACE] ? 1 : 0;
}

// SDL Function implementations
int SDL_Init(uint32_t flags) {
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
        if (install_sound(DIGI_AUTODETECT, MIDI_NONE, NULL) != 0) {
            std::cout << "Warning: Could not initialize audio" << std::endl;
        }
        g_initialized_subsystems |= SDL_INIT_AUDIO;
    }
    
    if (flags & (SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER)) {
        install_joystick(JOY_TYPE_AUTODETECT);
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

SDL_Window* SDL_CreateWindow(const char* title, int x, int y, int w, int h, uint32_t flags) {
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

SDL_Renderer* SDL_CreateRenderer(SDL_Window* window, int index, uint32_t flags) {
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

SDL_Texture* SDL_CreateTexture(SDL_Renderer* renderer, uint32_t format, int access, int w, int h) {
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
    
    // Simple quit detection
    if (key[KEY_ESC]) {
        event->type = SDL_QUIT;
        return 1;
    }
    
    // Basic joystick polling
    poll_joystick();
    
    return 0;
}

const Uint8* SDL_GetKeyboardState(int* numkeys) {
    update_keyboard();
    if (numkeys) *numkeys = 256;
    return g_keyboard_state;
}

int SDL_GetTicks() {
    static int start_time = -1;
    if (start_time == -1) start_time = clock();
    return (clock() - start_time) * 1000 / CLOCKS_PER_SEC;
}

void SDL_Delay(int ms) {
    rest(ms);
}

const char* SDL_GetError() {
    return "SDL wrapper error";
}

int SDL_OpenAudio(SDL_AudioSpec* desired, SDL_AudioSpec* obtained) {
    if (!desired) return -1;
    
    g_audio_spec = *desired;
    if (obtained) *obtained = *desired;
    
    LOCK_VARIABLE(g_audio_playing);
    LOCK_VARIABLE(g_audio_spec);
    LOCK_FUNCTION(audio_timer_callback);
    
    // Calculate timer frequency for audio callback
    int timer_freq = 1000000 / (desired->freq / desired->samples);
    install_int(audio_timer_callback, timer_freq);
    
    return 0;
}

void SDL_PauseAudio(int pause_on) {
    g_audio_playing = !pause_on;
}

void SDL_CloseAudio() {
    g_audio_playing = false;
    remove_int(audio_timer_callback);
}

void SDL_LockAudio() {
    // In a real implementation, this would lock audio thread access
    // For Allegro, we can disable the timer temporarily
    remove_int(audio_timer_callback);
}

void SDL_UnlockAudio() {
    // Re-enable the audio timer
    if (g_audio_spec.callback) {
        int timer_freq = 1000000 / (g_audio_spec.freq / g_audio_spec.samples);
        install_int(audio_timer_callback, timer_freq);
    }
}

int SDL_SetWindowFullscreen(SDL_Window* window, uint32_t flags) {
    if (!window) return -1;
    _SDL_Window* win = (_SDL_Window*)window;
    
    if (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) {
        set_gfx_mode(GFX_AUTODETECT_FULLSCREEN, win->width, win->height, 0, 0);
    } else {
        set_gfx_mode(GFX_AUTODETECT_WINDOWED, win->width, win->height, 0, 0);
    }
    
    return 0;
}

// Joystick/Controller functions
int SDL_NumJoysticks() {
    return num_joysticks;
}

SDL_Joystick* SDL_JoystickOpen(int device_index) {
    if (device_index >= 0 && device_index < num_joysticks && device_index < 4) {
        if (!g_joysticks[device_index]) {
            g_joysticks[device_index] = new _SDL_Joystick();
            g_joysticks[device_index]->index = device_index;
            g_joysticks[device_index]->connected = true;
            g_joysticks[device_index]->instance_id = g_next_instance_id++;
        }
        return (SDL_Joystick*)g_joysticks[device_index];
    }
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
    // Allegro doesn't really have hats, return 1 if we have sticks (simulate with stick)
    if (joystick) {
        _SDL_Joystick* joy_ptr = (_SDL_Joystick*)joystick;
        return joy[joy_ptr->index].num_sticks > 0 ? 1 : 0;
    }
    return 0;
}

Sint16 SDL_JoystickGetAxis(SDL_Joystick* joystick, int axis) {
    if (joystick && axis >= 0) {
        _SDL_Joystick* jstick = (_SDL_Joystick*)joystick;
        poll_joystick();
        int stick = axis / 2;
        int axis_type = axis % 2;
        
        if (stick < joy[jstick->index].num_sticks) {
            if (axis_type == 0) {
                return joy[jstick->index].stick[stick].axis[0].pos * 128;
            } else {
                return joy[jstick->index].stick[stick].axis[1].pos * 128;
            }
        }
    }
    return 0;
}

uint8_t SDL_JoystickGetButton(SDL_Joystick* joystick, int button) {
    if (joystick && button >= 0) {
        _SDL_Joystick* jstick = (_SDL_Joystick*)joystick;
        if (button < joy[jstick->index].num_buttons) {
            poll_joystick();
            return joy[jstick->index].button[button].b ? 1 : 0;
        }
    }
    return 0;
}

uint8_t SDL_JoystickGetHat(SDL_Joystick* joystick, int hat) {
    if (joystick && hat == 0) {
        _SDL_Joystick* jstick = (_SDL_Joystick*)joystick;
        poll_joystick();
        
        // Simulate hat with first stick
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
    // In a real implementation, this would enable/disable joystick events
    // For our wrapper, we'll just ignore it since we poll manually
}

SDL_GameController* SDL_GameControllerOpen(int joystick_index) {
    if (joystick_index >= 0 && joystick_index < num_joysticks && joystick_index < 4) {
        if (!g_controllers[joystick_index]) {
            g_controllers[joystick_index] = new _SDL_GameController();
            g_controllers[joystick_index]->index = joystick_index;
            g_controllers[joystick_index]->connected = true;
        }
        return (SDL_GameController*)g_controllers[joystick_index];
    }
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
    if (gamecontroller) {
        _SDL_GameController* ctrl = (_SDL_GameController*)gamecontroller;
        poll_joystick();
        int joy_index = ctrl->index;
        
        // Map controller buttons to joystick buttons (basic mapping)
        switch (button) {
            case SDL_CONTROLLER_BUTTON_A:
                return (0 < joy[joy_index].num_buttons) ? joy[joy_index].button[0].b : 0;
            case SDL_CONTROLLER_BUTTON_B:
                return (1 < joy[joy_index].num_buttons) ? joy[joy_index].button[1].b : 0;
            case SDL_CONTROLLER_BUTTON_X:
                return (2 < joy[joy_index].num_buttons) ? joy[joy_index].button[2].b : 0;
            case SDL_CONTROLLER_BUTTON_Y:
                return (3 < joy[joy_index].num_buttons) ? joy[joy_index].button[3].b : 0;
            default:
                if (button < joy[joy_index].num_buttons)
                    return joy[joy_index].button[button].b;
                break;
        }
    }
    return 0;
}

int SDL_IsGameController(int joystick_index) {
    // For simplicity, assume all joysticks can be game controllers
    return (joystick_index >= 0 && joystick_index < num_joysticks) ? 1 : 0;
}

SDL_Joystick* SDL_GameControllerGetJoystick(SDL_GameController* gamecontroller) {
    if (gamecontroller) {
        _SDL_GameController* ctrl = (_SDL_GameController*)gamecontroller;
        return SDL_JoystickOpen(ctrl->index);
    }
    return nullptr;
}

void SDL_GameControllerEventState(int state) {
    // Similar to joystick event state, we'll ignore this
}

int SDL_GameControllerAddMappingsFromFile(const char* file) {
    // For now, just return 0 (no mappings added)
    return 0;
}

// SDL subsystem functions
int SDL_WasInit(uint32_t flags) {
    return g_initialized_subsystems & flags;
}

int SDL_InitSubSystem(uint32_t flags) {
    return SDL_Init(flags);
}

void SDL_QuitSubSystem(uint32_t flags) {
    g_initialized_subsystems &= ~flags;
}

int SDL_SetTextureBlendMode(SDL_Texture* texture, int blendMode) {
    if (!texture) return -1;
    
    _SDL_Texture* tex = (_SDL_Texture*)texture;
    tex->blend_mode = blendMode;
    
    // In a full implementation, this would affect how the texture is rendered
    // For now, just store the blend mode
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
    // In a real implementation, this would store hint values
    return 1; // SDL_TRUE
}
