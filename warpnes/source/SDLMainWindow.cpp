#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <chrono>
#include <algorithm>

#include <SDL2/SDL.h>

#include "Emulation/ControllerSDL.hpp"
#include "SMB/SMBEmulator.hpp"
#include "Configuration.hpp"
#include "Constants.hpp"
#include "SDLCacheScaling.hpp"

// Forward declarations
static void audioCallback(void* userdata, uint8_t* buffer, int len);
static bool initialize();
static void shutdown();
static void handleInput();
static void handleMenuInput();
static void handleDialogInput();
static void handleGameInput();
static void updateAndDraw();
static void drawMenu();
static void drawDialog();
static void drawGame();
static void setStatusMessage(const char* msg);
static void updateStatusMessage();

// Global variables - following dos_main.cpp pattern
static SDL_Window* window = nullptr;
static SDL_Renderer* renderer = nullptr;
static SDL_Texture* gameTexture = nullptr;
static SDLScalingCache* scalingCache = nullptr;
static SMBEmulator* smbEngine = nullptr;
static uint16_t renderBuffer[RENDER_WIDTH * RENDER_HEIGHT];

// Game state variables
static bool gameRunning = false;
static bool gamePaused = false;
static bool showingMenu = false;
static bool zapperEnabled = false;

// Menu system
struct MenuItem {
    char text[32];
    int id;
    bool enabled;
};

static MenuItem mainMenu[10];
static int menuCount = 0;
static int selectedMenuItem = 0;

// Dialog system
enum DialogType {
    DIALOG_NONE,
    DIALOG_ABOUT,
    DIALOG_CONTROLS_P1,
    DIALOG_CONTROLS_P2,
    DIALOG_HELP,
    DIALOG_VIDEO_OPTIONS
};
static DialogType currentDialog = DIALOG_NONE;

// Control configuration
enum CaptureType {
    CAPTURE_NONE,
    CAPTURE_KEY_UP,
    CAPTURE_KEY_DOWN,
    CAPTURE_KEY_LEFT,
    CAPTURE_KEY_RIGHT,
    CAPTURE_KEY_A,
    CAPTURE_KEY_B,
    CAPTURE_KEY_START,
    CAPTURE_KEY_SELECT
};
static CaptureType currentCaptureType = CAPTURE_NONE;
static bool isCapturingInput = false;
static char currentCaptureKey[32] = "";

// Player control mappings
struct PlayerKeys {
    SDL_Scancode up, down, left, right;
    SDL_Scancode button_a, button_b;
    SDL_Scancode start, select;
};
static PlayerKeys player1Keys, player2Keys;

// Status message system
static char statusMessage[128] = "Ready";
static int statusMessageTimer = 0;

// Video settings
static bool isFullscreen = false;
static int windowedWidth = 640;
static int windowedHeight = 480;

// Mouse/Zapper support
static int mouseX = 0, mouseY = 0;
static bool mousePressed = false;

// SDL Audio callback
static void audioCallback(void* userdata, uint8_t* buffer, int len)
{
    if (!smbEngine || !Configuration::getAudioEnabled()) {
        memset(buffer, 128, len); // Unsigned 8-bit silence
        return;
    }
    
    smbEngine->audioCallback(buffer, len);
}

static void setupDefaultControls()
{
    // Default keyboard controls - Player 1
    player1Keys.up = SDL_SCANCODE_UP;
    player1Keys.down = SDL_SCANCODE_DOWN;
    player1Keys.left = SDL_SCANCODE_LEFT;
    player1Keys.right = SDL_SCANCODE_RIGHT;
    player1Keys.button_a = SDL_SCANCODE_X;
    player1Keys.button_b = SDL_SCANCODE_Z;
    player1Keys.start = SDL_SCANCODE_RIGHTBRACKET;    // "]"
    player1Keys.select = SDL_SCANCODE_LEFTBRACKET;    // "["
    
    // Default keyboard controls - Player 2
    player2Keys.up = SDL_SCANCODE_W;
    player2Keys.down = SDL_SCANCODE_S;
    player2Keys.left = SDL_SCANCODE_A;
    player2Keys.right = SDL_SCANCODE_D;
    player2Keys.button_a = SDL_SCANCODE_G;
    player2Keys.button_b = SDL_SCANCODE_F;
    player2Keys.start = SDL_SCANCODE_P;
    player2Keys.select = SDL_SCANCODE_O;
}

static void setupMenu()
{
    menuCount = 0;
    
    strcpy(mainMenu[menuCount].text, "Resume Game");
    mainMenu[menuCount].id = 1;
    mainMenu[menuCount].enabled = true;
    menuCount++;
    
    strcpy(mainMenu[menuCount].text, "Reset Game");
    mainMenu[menuCount].id = 2;
    mainMenu[menuCount].enabled = true;
    menuCount++;
    
    strcpy(mainMenu[menuCount].text, "Video Options");
    mainMenu[menuCount].id = 9;
    mainMenu[menuCount].enabled = true;
    menuCount++;
    
    strcpy(mainMenu[menuCount].text, "Player 1 Controls");
    mainMenu[menuCount].id = 3;
    mainMenu[menuCount].enabled = true;
    menuCount++;
    
    strcpy(mainMenu[menuCount].text, "Player 2 Controls");
    mainMenu[menuCount].id = 4;
    mainMenu[menuCount].enabled = true;
    menuCount++;
    
    strcpy(mainMenu[menuCount].text, "Help");
    mainMenu[menuCount].id = 5;
    mainMenu[menuCount].enabled = true;
    menuCount++;
    
    strcpy(mainMenu[menuCount].text, "About");
    mainMenu[menuCount].id = 6;
    mainMenu[menuCount].enabled = true;
    menuCount++;
    
    strcpy(mainMenu[menuCount].text, "Quit");
    mainMenu[menuCount].id = 7;
    mainMenu[menuCount].enabled = true;
    menuCount++;
}

static bool initialize()
{
    printf("Initializing SDL...\n");
    
    // Load configuration
    Configuration::initialize(CONFIG_FILE_NAME);
    
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) < 0) {
        printf("Failed to initialize SDL: %s\n", SDL_GetError());
        return false;
    }
    
    // Create window
    window = SDL_CreateWindow(APP_TITLE,
                             SDL_WINDOWPOS_UNDEFINED,
                             SDL_WINDOWPOS_UNDEFINED,
                             windowedWidth, windowedHeight,
                             SDL_WINDOW_RESIZABLE);
    if (!window) {
        printf("Failed to create window: %s\n", SDL_GetError());
        return false;
    }
    
    // Create renderer
    Uint32 rendererFlags = SDL_RENDERER_ACCELERATED;
    if (Configuration::getVsyncEnabled()) {
        rendererFlags |= SDL_RENDERER_PRESENTVSYNC;
    }
    
    renderer = SDL_CreateRenderer(window, -1, rendererFlags);
    if (!renderer) {
        printf("Failed to create renderer: %s\n", SDL_GetError());
        return false;
    }
    
    // Set logical size for consistent rendering
    if (SDL_RenderSetLogicalSize(renderer, RENDER_WIDTH, RENDER_HEIGHT) < 0) {
        printf("Failed to set logical size: %s\n", SDL_GetError());
        return false;
    }
    
    // Create main game texture
    gameTexture = SDL_CreateTexture(renderer, 
                                   SDL_PIXELFORMAT_ARGB8888, 
                                   SDL_TEXTUREACCESS_STREAMING, 
                                   RENDER_WIDTH, RENDER_HEIGHT);
    if (!gameTexture) {
        printf("Failed to create game texture: %s\n", SDL_GetError());
        return false;
    }
    
    // Initialize scaling cache
    scalingCache = new SDLScalingCache(renderer);
    if (scalingCache) {
        scalingCache->initialize();
    }
    
    // Initialize audio
    if (Configuration::getAudioEnabled()) {
        SDL_AudioSpec desiredSpec = {};
        desiredSpec.freq = Configuration::getAudioFrequency();
        desiredSpec.format = AUDIO_U8;  // Match dos_main.cpp format
        desiredSpec.channels = 1;
        desiredSpec.samples = 2048;
        desiredSpec.callback = audioCallback;
        desiredSpec.userdata = nullptr;
        
        SDL_AudioSpec obtainedSpec;
        if (SDL_OpenAudio(&desiredSpec, &obtainedSpec) < 0) {
            printf("Failed to initialize audio: %s\n", SDL_GetError());
            printf("Continuing without audio...\n");
        } else {
            printf("Audio initialized: %d Hz\n", obtainedSpec.freq);
            SDL_PauseAudio(0); // Start audio
        }
    }
    
    // Initialize mouse if Zapper is enabled
    if (zapperEnabled) {
        printf("Initializing SDL mouse for NES Zapper...\n");
        SDL_ShowCursor(SDL_ENABLE);
        printf("Mouse cursor enabled for NES Zapper\n");
    }
    
    setupDefaultControls();
    setupMenu();
    
    printf("SDL initialized successfully\n");
    return true;
}

static void shutdown()
{
    if (scalingCache) {
        delete scalingCache;
        scalingCache = nullptr;
    }
    
    SDL_CloseAudio();
    
    if (gameTexture) {
        SDL_DestroyTexture(gameTexture);
        gameTexture = nullptr;
    }
    
    if (renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }
    
    if (window) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }
    
    SDL_Quit();
    printf("SDL shutdown complete\n");
}

static bool toggleFullscreen()
{
    printf("Toggling fullscreen mode...\n");
    
    Uint32 windowFlags = SDL_GetWindowFlags(window);
    if (windowFlags & SDL_WINDOW_FULLSCREEN_DESKTOP) {
        // Switch to windowed
        SDL_SetWindowFullscreen(window, 0);
        SDL_SetWindowSize(window, windowedWidth, windowedHeight);
        isFullscreen = false;
        printf("Switched to windowed mode: %dx%d\n", windowedWidth, windowedHeight);
    } else {
        // Switch to fullscreen
        SDL_GetWindowSize(window, &windowedWidth, &windowedHeight);
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
        isFullscreen = true;
        printf("Switched to fullscreen mode\n");
    }
    
    setStatusMessage(isFullscreen ? "Switched to fullscreen" : "Switched to windowed");
    return true;
}

static void screenToNESCoordinates(int screenX, int screenY, int* nesX, int* nesY)
{
    // Convert screen coordinates to logical coordinates
    float logicalX, logicalY;
    SDL_RenderWindowToLogical(renderer, screenX, screenY, &logicalX, &logicalY);
    
    *nesX = (int)logicalX;
    *nesY = (int)logicalY;
    
    // Clamp to NES screen bounds
    if (*nesX < 0) *nesX = 0;
    if (*nesX >= RENDER_WIDTH) *nesX = RENDER_WIDTH - 1;
    if (*nesY < 0) *nesY = 0;
    if (*nesY >= RENDER_HEIGHT) *nesY = RENDER_HEIGHT - 1;
}

static void checkPlayerInput(int player)
{
    if (!smbEngine) return;
    
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    
    PlayerKeys* playerKeys = (player == 0) ? &player1Keys : &player2Keys;
    Controller& controller = (player == 0) ? smbEngine->getController1() : smbEngine->getController2();
    
    bool up = keys[playerKeys->up];
    bool down = keys[playerKeys->down];
    bool left = keys[playerKeys->left];
    bool right = keys[playerKeys->right];
    bool a = keys[playerKeys->button_a];
    bool b = keys[playerKeys->button_b];
    bool start = keys[playerKeys->start];
    bool select = keys[playerKeys->select];
    
    controller.setButtonState(BUTTON_UP, up);
    controller.setButtonState(BUTTON_DOWN, down);
    controller.setButtonState(BUTTON_LEFT, left);
    controller.setButtonState(BUTTON_RIGHT, right);
    controller.setButtonState(BUTTON_A, a);
    controller.setButtonState(BUTTON_B, b);
    controller.setButtonState(BUTTON_START, start);
    controller.setButtonState(BUTTON_SELECT, select);
}

static void handleGameInput()
{
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    
    // Handle save/load states
    static bool f5Pressed = false, f6Pressed = false, f7Pressed = false, f8Pressed = false;
    bool shiftPressed = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
    
    // F5 - Save/Load State 1
    if (keys[SDL_SCANCODE_F5] && !f5Pressed) {
        if (smbEngine) {
            if (shiftPressed) {
                if (smbEngine->loadState("save1")) {
                    setStatusMessage("State 1 loaded");
                } else {
                    setStatusMessage("Failed to load state 1");
                }
            } else {
                smbEngine->saveState("save1");
                setStatusMessage("State 1 saved");
            }
        }
        f5Pressed = true;
    } else if (!keys[SDL_SCANCODE_F5]) {
        f5Pressed = false;
    }
    
    // F6 - Save/Load State 2
    if (keys[SDL_SCANCODE_F6] && !f6Pressed) {
        if (smbEngine) {
            if (shiftPressed) {
                if (smbEngine->loadState("save2")) {
                    setStatusMessage("State 2 loaded");
                } else {
                    setStatusMessage("Failed to load state 2");
                }
            } else {
                smbEngine->saveState("save2");
                setStatusMessage("State 2 saved");
            }
        }
        f6Pressed = true;
    } else if (!keys[SDL_SCANCODE_F6]) {
        f6Pressed = false;
    }
    
    // F7 - Save/Load State 3
    if (keys[SDL_SCANCODE_F7] && !f7Pressed) {
        if (smbEngine) {
            if (shiftPressed) {
                if (smbEngine->loadState("save3")) {
                    setStatusMessage("State 3 loaded");
                } else {
                    setStatusMessage("Failed to load state 3");
                }
            } else {
                smbEngine->saveState("save3");
                setStatusMessage("State 3 saved");
            }
        }
        f7Pressed = true;
    } else if (!keys[SDL_SCANCODE_F7]) {
        f7Pressed = false;
    }
    
    // F8 - Save/Load State 4
    if (keys[SDL_SCANCODE_F8] && !f8Pressed) {
        if (smbEngine) {
            if (shiftPressed) {
                if (smbEngine->loadState("save4")) {
                    setStatusMessage("State 4 loaded");
                } else {
                    setStatusMessage("Failed to load state 4");
                }
            } else {
                smbEngine->saveState("save4");
                setStatusMessage("State 4 saved");
            }
        }
        f8Pressed = true;
    } else if (!keys[SDL_SCANCODE_F8]) {
        f8Pressed = false;
    }
    
    // Check for pause (Ctrl+P)
    static bool pPressed = false;
    if (keys[SDL_SCANCODE_P] && (keys[SDL_SCANCODE_LCTRL] || keys[SDL_SCANCODE_RCTRL]) && !pPressed) {
        gamePaused = !gamePaused;
        setStatusMessage(gamePaused ? "Game Paused" : "Game Resumed");
        pPressed = true;
    } else if (!keys[SDL_SCANCODE_P]) {
        pPressed = false;
    }
    
    // Check for reset (Ctrl+R)
    static bool rPressed = false;
    if (keys[SDL_SCANCODE_R] && (keys[SDL_SCANCODE_LCTRL] || keys[SDL_SCANCODE_RCTRL]) && !rPressed) {
        if (smbEngine) {
            smbEngine->reset();
            setStatusMessage("Game Reset");
        }
        rPressed = true;
    } else if (!keys[SDL_SCANCODE_R]) {
        rPressed = false;
    }
    
    // Process player input only if game is not paused
    if (!gamePaused) {
        checkPlayerInput(0); // Player 1
        checkPlayerInput(1); // Player 2
    }
}

static void handleMenuInput()
{
    // This would be called from main event loop when menu keys are pressed
    // For now, just basic navigation
}

static void handleDialogInput()
{
    // This would be called from main event loop when dialog keys are pressed
}

static void handleInput()
{
    SDL_Event event;
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                gameRunning = false;
                break;
                
            case SDL_KEYDOWN:
                switch (event.key.keysym.scancode) {
                    case SDL_SCANCODE_ESCAPE:
                        if (currentDialog != DIALOG_NONE) {
                            currentDialog = DIALOG_NONE;
                            if (!showingMenu && gamePaused) {
                                gamePaused = false;
                                setStatusMessage("Game Resumed");
                            }
                        } else if (showingMenu && gamePaused) {
                            showingMenu = false;
                            gamePaused = false;
                            setStatusMessage("Game Resumed");
                        } else {
                            showingMenu = true;
                            gamePaused = true;
                            setStatusMessage("Game Paused - Menu Active");
                        }
                        break;
                        
                    case SDL_SCANCODE_F11:
                        toggleFullscreen();
                        break;
                        
                    case SDL_SCANCODE_UP:
                        if (showingMenu) {
                            selectedMenuItem--;
                            if (selectedMenuItem < 0) selectedMenuItem = menuCount - 1;
                        }
                        break;
                        
                    case SDL_SCANCODE_DOWN:
                        if (showingMenu) {
                            selectedMenuItem++;
                            if (selectedMenuItem >= menuCount) selectedMenuItem = 0;
                        }
                        break;
                        
                    case SDL_SCANCODE_RETURN:
                        if (showingMenu) {
                            // Handle menu selection
                            switch (mainMenu[selectedMenuItem].id) {
                                case 1: // Resume Game
                                    showingMenu = false;
                                    gamePaused = false;
                                    setStatusMessage("Game Resumed");
                                    break;
                                case 2: // Reset Game
                                    if (smbEngine) {
                                        smbEngine->reset();
                                        setStatusMessage("Game Reset");
                                    }
                                    showingMenu = false;
                                    gamePaused = false;
                                    break;
                                case 7: // Quit
                                    gameRunning = false;
                                    break;
                                default:
                                    setStatusMessage("Feature not yet implemented");
                                    break;
                            }
                        }
                        break;
                        
                    default:
                        break;
                }
                break;
                
            case SDL_MOUSEBUTTONDOWN:
                if (zapperEnabled && event.button.button == SDL_BUTTON_LEFT) {
                    mousePressed = true;
                    mouseX = event.button.x;
                    mouseY = event.button.y;
                }
                break;
                
            case SDL_MOUSEBUTTONUP:
                if (zapperEnabled && event.button.button == SDL_BUTTON_LEFT) {
                    mousePressed = false;
                }
                break;
                
            case SDL_MOUSEMOTION:
                if (zapperEnabled) {
                    mouseX = event.motion.x;
                    mouseY = event.motion.y;
                }
                break;
                
            default:
                break;
        }
    }
    
    // Update Zapper state
    if (zapperEnabled && smbEngine) {
        int nesMouseX, nesMouseY;
        screenToNESCoordinates(mouseX, mouseY, &nesMouseX, &nesMouseY);
        smbEngine->updateZapperInput(nesMouseX, nesMouseY, mousePressed);
    }
    
    // Handle game input when not in menu/dialog
    if (currentDialog == DIALOG_NONE && !showingMenu) {
        handleGameInput();
    }
}

static void drawText(int x, int y, const char* text, Uint8 r, Uint8 g, Uint8 b)
{
    // Simple text rendering placeholder
    // In a real implementation, you'd use TTF_RenderText or similar
    // For now, just draw colored rectangles as placeholders
    SDL_SetRenderDrawColor(renderer, r, g, b, 255);
    SDL_Rect textRect = {x, y, (int)strlen(text) * 8, 16};
    SDL_RenderDrawRect(renderer, &textRect);
}

static void drawMenu()
{
    int menu_x = RENDER_WIDTH / 2 - 100;
    int menu_y = RENDER_HEIGHT / 2 - (menuCount * 10);
    
    // Draw menu background
    SDL_SetRenderDrawColor(renderer, 64, 64, 64, 255);
    SDL_Rect menuBg = {menu_x - 20, menu_y - 20, 200, menuCount * 20 + 40};
    SDL_RenderFillRect(renderer, &menuBg);
    
    // Draw menu border
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &menuBg);
    
    // Draw menu title
    drawText(menu_x, menu_y - 10, "GAME MENU", 255, 255, 0);
    
    // Draw menu items
    for (int i = 0; i < menuCount; i++) {
        Uint8 r = (i == selectedMenuItem) ? 255 : 255;
        Uint8 g = (i == selectedMenuItem) ? 255 : 255;
        Uint8 b = (i == selectedMenuItem) ? 0 : 255;
        
        if (i == selectedMenuItem) {
            drawText(menu_x - 10, menu_y + i * 20, ">", r, g, b);
        }
        drawText(menu_x, menu_y + i * 20, mainMenu[i].text, r, g, b);
    }
}

static void drawDialog()
{
    switch (currentDialog) {
        case DIALOG_ABOUT:
            {
                int dialog_x = RENDER_WIDTH / 4;
                int dialog_y = RENDER_HEIGHT / 4;
                int dialog_w = RENDER_WIDTH / 2;
                int dialog_h = RENDER_HEIGHT / 2;
                
                SDL_SetRenderDrawColor(renderer, 32, 32, 32, 255);
                SDL_Rect dialogBg = {dialog_x, dialog_y, dialog_w, dialog_h};
                SDL_RenderFillRect(renderer, &dialogBg);
                
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                SDL_RenderDrawRect(renderer, &dialogBg);
                
                drawText(dialog_x + 20, dialog_y + 20, "SUPER MARIO BROS Emulator", 255, 255, 0);
                drawText(dialog_x + 20, dialog_y + 40, "SDL Version", 255, 255, 255);
                drawText(dialog_x + 20, dialog_y + 60, "Built with SDL 2", 255, 255, 255);
                drawText(dialog_x + 20, dialog_y + dialog_h - 40, "Press ESC to close", 255, 255, 0);
            }
            break;
            
        default:
            break;
    }
}

static void drawGame()
{
    if (!smbEngine) return;
    
    // Try optimized rendering first
    if (scalingCache && scalingCache->isOptimizedScaling()) {
        int window_width, window_height;
        SDL_GetWindowSize(window, &window_width, &window_height);
        scalingCache->renderOptimized(renderBuffer, window_width, window_height);
    } else {
        // Fallback to standard rendering
        SDL_UpdateTexture(gameTexture, nullptr, renderBuffer, sizeof(uint32_t) * RENDER_WIDTH);
        SDL_RenderCopy(renderer, gameTexture, nullptr, nullptr);
    }
}

static void updateAndDraw()
{
    // Clear screen
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    
    // Draw based on current state
    if (currentDialog != DIALOG_NONE) {
        drawDialog();
    } else if (showingMenu) {
        drawGame(); // Draw game in background
        drawMenu();
    } else {
        drawGame();
    }
    
    // Draw status message if active
    if (statusMessageTimer > 0) {
        drawText(10, RENDER_HEIGHT - 25, statusMessage, 255, 255, 255);
    }
    
    // Present frame
    SDL_RenderPresent(renderer);
}

static void setStatusMessage(const char* msg)
{
    strncpy(statusMessage, msg, sizeof(statusMessage) - 1);
    statusMessage[sizeof(statusMessage) - 1] = '\0';
    statusMessageTimer = 180; // 3 seconds at 60 FPS
}

static void updateStatusMessage()
{
    if (statusMessageTimer > 0) {
        statusMessageTimer--;
        if (statusMessageTimer == 0) {
            strcpy(statusMessage, "Ready");
        }
    }
}

static void run(const char* romFilename)
{
    gamePaused = false;
    showingMenu = false;
    currentDialog = DIALOG_NONE;
    
    SMBEmulator engine;
    smbEngine = &engine;
    
    printf("Loading ROM: %s\n", romFilename);
    if (!engine.loadROM(romFilename)) {
        printf("Failed to load ROM file: %s\n", romFilename);
        setStatusMessage("ROM loading failed");
        return;
    }
    
    printf("ROM loaded successfully\n");
    
    // Enable Zapper if requested
    if (zapperEnabled) {
        engine.enableZapper(true);
        setStatusMessage("NES Zapper enabled - Use mouse to aim, click to fire");
    }
    
    engine.reset();
    gameRunning = true;
    setStatusMessage("Game started - Press ESC for menu");
    
    printf("Starting main game loop...\n");
    printf("Resolution: %dx%d (logical)\n", RENDER_WIDTH, RENDER_HEIGHT);
    printf("Audio: %s\n", Configuration::getAudioEnabled() ? "Enabled" : "Disabled");
    
    auto lastFrameTime = std::chrono::high_resolution_clock::now();
    const double targetFrameMs = 1000.0 / Configuration::getFrameRate();
    
    while (gameRunning) {
        handleInput();
        
        if (!gamePaused && !showingMenu && currentDialog == DIALOG_NONE) {
            engine.update();
        }
        
        // Always render for smooth menu/dialog display
        engine.render16(renderBuffer);
        
        updateAndDraw();
        updateStatusMessage();
        
        // Frame rate limiting
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFrameTime);
        
        if (elapsed.count() < targetFrameMs) {
            SDL_Delay(static_cast<Uint32>(targetFrameMs - elapsed.count()));
        }
        lastFrameTime = std::chrono::high_resolution_clock::now();
    }
    
    printf("Game loop ended normally\n");
}

int main(int argc, char** argv)
{
    printf("Welcome to Warp NES Emulator - SDL Edition\n");
    
    // Parse command line arguments
    const char* romFilename = nullptr;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--neszapper") == 0) {
            zapperEnabled = true;
            printf("NES Zapper enabled\n");
        } else if (argv[i][0] != '-') {
            romFilename = argv[i];
        }
    }
    
    if (!romFilename) {
        printf("Usage: %s [--neszapper] <rom_file.nes>\n", argv[0]);
        printf("Example: %s --neszapper duckhunt.nes\n", argv[0]);
        printf("Options:\n");
        printf("  --neszapper    Enable NES Zapper (light gun) support\n");
        return -1;
    }
    
    // Validate ROM file
    FILE* romTest = fopen(romFilename, "rb");
    if (!romTest) {
        printf("Error: Cannot open ROM file '%s'\n", romFilename);
        return -1;
    }
    fclose(romTest);
    
    if (zapperEnabled) {
        printf("=== NES ZAPPER ENABLED ===\n");
        printf("Mouse controls:\n");
        printf("  - Move mouse to aim\n");
        printf("  - Left click to fire\n");
        printf("Compatible games: Duck Hunt, Wild Gunman, Hogan's Alley\n");
        printf("========================\n");
    }
    
    printf("ROM file: %s\n", romFilename);
    printf("Initializing...\n");
    
    if (!initialize()) {
        printf("Failed to initialize\n");
        return -1;
    }
    
    printf("Starting game with ROM: %s\n", romFilename);
    run(romFilename);
    
    shutdown();
    return 0;
}
