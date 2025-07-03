#include <stdio.h>
#include <stdlib.h>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include "AllegroMainWindow.hpp"
#include "SMB/SMBEngine.hpp"
#include "Emulation/Controller.hpp"
#include "Configuration.hpp"
#include "Constants.hpp"
#include "SMBRom.hpp"


// DOS-specific includes (will be ignored on Linux)
#ifdef __DJGPP__
#include <conio.h>
#include <dos.h>
#endif

// Global variables for DOS compatibility
static SMBEngine* smbEngine = NULL;
static uint32_t renderBuffer[RENDER_WIDTH * RENDER_HEIGHT];
AllegroMainWindow* g_mainWindow = NULL;

// Timer for frame rate control
volatile int timer_counter = 0;

void timer_callback() {
    timer_counter++;
}
END_OF_FUNCTION(timer_callback)

AllegroMainWindow::AllegroMainWindow() 
    : game_buffer(NULL), gameRunning(false), gamePaused(false), 
      showingMenu(false), selectedMenuItem(0), inMenu(false),
      isCapturingInput(false), currentDialog(DIALOG_NONE),
      statusMessageTimer(0), currentFrameBuffer(NULL)
{
    strcpy(statusMessage, "Ready");
    strcpy(currentCaptureKey, "");
    menuCount = 0;
    g_mainWindow = this;
    
    // Initialize default controls
    setupDefaultControls();
}

AllegroMainWindow::~AllegroMainWindow() 
{
    shutdown();
}

bool AllegroMainWindow::initialize() 
{
    if (!initializeAllegro()) {
        return false;
    }
    
    if (!initializeGraphics()) {
        return false;
    }
    
    if (!initializeInput()) {
        return false;
    }
    
    setupMenu();
    loadControlConfig();
    
    setStatusMessage("Super Mario Bros Virtualizer - Press ESC for menu");
    
    return true;
}

bool AllegroMainWindow::initializeAllegro()
{
    if (allegro_init() != 0) {
        printf("Failed to initialize Allegro\n");
        return false;
    }
    
    // Install keyboard
    if (install_keyboard() < 0) {
        printf("Failed to install keyboard\n");
        return false;
    }
    
    // Install timer
    if (install_timer() < 0) {
        printf("Failed to install timer\n");
        return false;
    }
    
    // Install joystick (optional, don't fail if not present)
    install_joystick(JOY_TYPE_AUTODETECT);
    
    // Lock timer function for DOS
    LOCK_VARIABLE(timer_counter);
    LOCK_FUNCTION(timer_callback);
    
    // Install timer at 60 FPS
    install_int_ex(timer_callback, BPS_TO_TIMER(60));
    
    return true;
}

bool AllegroMainWindow::initializeGraphics()
{
    // Set graphics mode - different approach for Linux vs DOS
    set_color_depth(16); // 16-bit color for better DOS compatibility
    
    #ifdef __DJGPP__
    // DOS: Try lower resolutions, can go fullscreen
    if (set_gfx_mode(GFX_AUTODETECT, 640, 480, 0, 0) != 0) {
        if (set_gfx_mode(GFX_AUTODETECT, 320, 240, 0, 0) != 0) {
            if (set_gfx_mode(GFX_AUTODETECT, 256, 224, 0, 0) != 0) {
                printf("Failed to set graphics mode: %s\n", allegro_error);
                return false;
            }
        }
    }
    #else
    // Linux: Force windowed mode for development
    if (set_gfx_mode(GFX_AUTODETECT_WINDOWED, 640, 480, 0, 0) != 0) {
        if (set_gfx_mode(GFX_AUTODETECT_WINDOWED, 512, 384, 0, 0) != 0) {
            if (set_gfx_mode(GFX_AUTODETECT_WINDOWED, 320, 240, 0, 0) != 0) {
                printf("Failed to set windowed graphics mode: %s\n", allegro_error);
                return false;
            }
        }
    }
    #endif
    
    printf("Graphics mode: %dx%d %s\n", SCREEN_W, SCREEN_H, 
           #ifdef __DJGPP__ 
           "(fullscreen)"
           #else
           "(windowed)"
           #endif
    );
    
    // Create game buffer matching NES resolution
    game_buffer = create_bitmap(RENDER_WIDTH, RENDER_HEIGHT);
    if (!game_buffer) {
        printf("Failed to create game buffer\n");
        return false;
    }
    
    // Create back buffer for double buffering (same size as screen)
    back_buffer = create_bitmap(SCREEN_W, SCREEN_H);
    if (!back_buffer) {
        printf("Failed to create back buffer\n");
        return false;
    }
    
    clear_to_color(screen, makecol(0, 0, 0));
    
    return true;
}

bool AllegroMainWindow::initializeInput()
{
    // Clear keyboard buffer
    clear_keybuf();
    
    // Poll joystick to get initial state
    if (num_joysticks > 0) {
        poll_joystick();
        printf("Joystick detected: %d joysticks found\n", num_joysticks);
    }
    
    return true;
}

void AllegroMainWindow::setupDefaultControls()
{
    // Default keyboard controls - Player 1
    player1Keys.up = KEY_UP;
    player1Keys.down = KEY_DOWN;
    player1Keys.left = KEY_LEFT;
    player1Keys.right = KEY_RIGHT;
    player1Keys.button_a = KEY_X;
    player1Keys.button_b = KEY_Z;
    player1Keys.start = KEY_ENTER;
    player1Keys.select = KEY_SPACE;
    
    // Default keyboard controls - Player 2
    player2Keys.up = KEY_W;
    player2Keys.down = KEY_S;
    player2Keys.left = KEY_A;
    player2Keys.right = KEY_D;
    player2Keys.button_a = KEY_G;
    player2Keys.button_b = KEY_F;
    player2Keys.start = KEY_T;
    player2Keys.select = KEY_R;
    
    // Default joystick controls
    player1Joy.button_a = 0;
    player1Joy.button_b = 1;
    player1Joy.start = 2;
    player1Joy.select = 3;
    player1Joy.use_stick = true;
    
    player2Joy.button_a = 0;
    player2Joy.button_b = 1;
    player2Joy.start = 2;
    player2Joy.select = 3;
    player2Joy.use_stick = true;
}

void AllegroMainWindow::setupMenu()
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

void AllegroMainWindow::run() 
{
    // Initialize the SMB engine
    SMBEngine engine(const_cast<unsigned char*>(smbRomData));
    smbEngine = &engine;
    engine.reset();
    
    gameRunning = true;
    setStatusMessage("Game started - Press ESC for menu");
    
    printf("Starting main game loop...\n");
    printf("Resolution: %dx%d\n", SCREEN_W, SCREEN_H);
    printf("Controls: ESC=Menu, P=Pause, Ctrl+R=Reset\n");
    printf("Player 1: Arrow keys, X=A, Z=B, Enter=Start, Space=Select\n");
    
    // Main game loop
    while (gameRunning) {
        // Wait for timer tick (60 FPS) or use rest for compatibility
        #ifdef __DJGPP__
        // DOS: Use timer-based approach
        while (timer_counter == 0) {
            rest(1);
        }
        timer_counter--;
        #else
        // Linux: Simple timing for testing
        rest(16); // ~60 FPS
        #endif
        
        handleInput();
        
        if (!gamePaused && !showingMenu && currentDialog == DIALOG_NONE) {
            // Update game
            engine.update();
            engine.render(renderBuffer);
            currentFrameBuffer = renderBuffer;
        }
        
        updateAndDraw();
        
        // In Linux, allow clean exit with window close
        #ifndef __DJGPP__
        if (key[KEY_ALT] && key[KEY_F4]) {
            gameRunning = false;
        }
        #endif
    }
    
    printf("Game loop ended\n");
}

void AllegroMainWindow::handleInput()
{
    // Poll input devices
    poll_keyboard();
    if (num_joysticks > 0) {
        poll_joystick();
    }
    
    if (currentDialog != DIALOG_NONE) {
        handleDialogInput();
    } else if (showingMenu) {
        handleMenuInput();
    } else {
        handleGameInput();
    }
    
    updateStatusMessage();
}

void AllegroMainWindow::handleMenuInput()
{
    // Check for key presses
    if (keypressed()) {
        int k = readkey();
        int scancode = k >> 8;
        
        switch (scancode) {
            case KEY_UP:
                menuUp();
                break;
            case KEY_DOWN:
                menuDown();
                break;
            case KEY_ENTER:
                menuSelect();
                break;
            case KEY_ESC:
                menuEscape();
                break;
        }
    }
}

void AllegroMainWindow::handleDialogInput()
{
    if (isCapturingInput) {
        // Wait for key press to capture
        if (keypressed()) {
            int k = readkey();
            int scancode = k >> 8;
            
            if (scancode != KEY_ESC) {
                // Assign the captured key based on current capture context
                // This would be expanded based on which control is being configured
                isCapturingInput = false;
                strcpy(currentCaptureKey, "");
                setStatusMessage("Key captured");
            } else {
                isCapturingInput = false;
                strcpy(currentCaptureKey, "");
                setStatusMessage("Capture cancelled");
            }
        }
        return;
    }
    
    if (keypressed()) {
        int k = readkey();
        int scancode = k >> 8;
        
        if (scancode == KEY_ESC) {
            currentDialog = DIALOG_NONE;
        }
    }
}

void AllegroMainWindow::handleGameInput()
{
    // Check for menu toggle - use keypressed for single key events
    static bool escPressed = false;
    if (key[KEY_ESC] && !escPressed) {
        showingMenu = true;
        escPressed = true;
        return;
    }
    if (!key[KEY_ESC]) {
        escPressed = false;
    }
    
    // Check for pause
    static bool pPressed = false;
    if (key[KEY_P] && !pPressed) {
        gamePaused = !gamePaused;
        setStatusMessage(gamePaused ? "Game Paused" : "Game Resumed");
        pPressed = true;
        return;
    }
    if (!key[KEY_P]) {
        pPressed = false;
    }
    
    // Check for reset
    static bool rPressed = false;
    if (key[KEY_R] && key[KEY_LCONTROL] && !rPressed) {
        if (smbEngine) {
            smbEngine->reset();
            setStatusMessage("Game Reset");
        }
        rPressed = true;
        return;
    }
    if (!key[KEY_R]) {
        rPressed = false;
    }
    
    // Process player input
    checkPlayerInput(PLAYER_1);
    checkPlayerInput(PLAYER_2);
}

void AllegroMainWindow::testKeyboardBasic()
{
    static int testCounter = 0;
    if (testCounter++ % 60 == 0) {  // Every second
        poll_keyboard();
        
        // Test a bunch of common keys to see if ANY are detected
        printf("Raw key test: ");
        printf("ESC=%d ", key[KEY_ESC] ? 1 : 0);
        printf("UP=%d ", key[KEY_UP] ? 1 : 0);
        printf("DOWN=%d ", key[KEY_DOWN] ? 1 : 0);
        printf("LEFT=%d ", key[KEY_LEFT] ? 1 : 0);
        printf("RIGHT=%d ", key[KEY_RIGHT] ? 1 : 0);
        printf("X=%d ", key[KEY_X] ? 1 : 0);
        printf("Z=%d ", key[KEY_Z] ? 1 : 0);
        printf("SPACE=%d ", key[KEY_SPACE] ? 1 : 0);
        printf("ENTER=%d ", key[KEY_ENTER] ? 1 : 0);
        printf("W=%d ", key[KEY_W] ? 1 : 0);
        printf("A=%d ", key[KEY_A] ? 1 : 0);
        printf("S=%d ", key[KEY_S] ? 1 : 0);
        printf("D=%d ", key[KEY_D] ? 1 : 0);
        printf("\n");
        
        // Also check if keyboard buffer has anything
        if (keypressed()) {
            int k = readkey();
            printf("Keypressed detected: scancode=%d ascii=%d\n", k >> 8, k & 0xFF);
        }
    }
}

void AllegroMainWindow::checkPlayerInput(Player player)
{
    if (!smbEngine) return;
    
    // First, test if keyboard works at all
    //testKeyboardBasic();
    
    // Force keyboard polling
    poll_keyboard();
    
    if (player == PLAYER_1) {
        Controller& controller = smbEngine->getController1();
        
        // Try the most basic approach - hardcoded keys
        bool up = (key[KEY_UP] != 0);
        bool down = (key[KEY_DOWN] != 0);
        bool left = (key[KEY_LEFT] != 0);
        bool right = (key[KEY_RIGHT] != 0);
        bool a = (key[KEY_X] != 0);
        bool b = (key[KEY_Z] != 0);
        bool start = (key[KEY_ENTER] != 0);
        bool select = (key[KEY_SPACE] != 0);
        
        // Set the controller states
        controller.setButtonState(BUTTON_UP, up);
        controller.setButtonState(BUTTON_DOWN, down);
        controller.setButtonState(BUTTON_LEFT, left);
        controller.setButtonState(BUTTON_RIGHT, right);
        controller.setButtonState(BUTTON_A, a);
        controller.setButtonState(BUTTON_B, b);
        controller.setButtonState(BUTTON_START, start);
        controller.setButtonState(BUTTON_SELECT, select);
        
        // Debug what we're actually sending to the controller
        static int debugCounter = 0;
        if (debugCounter++ % 60 == 0) {
            printf("Controller states being set: UP=%d DOWN=%d LEFT=%d RIGHT=%d A=%d B=%d START=%d SELECT=%d\n",
                   up ? 1 : 0, down ? 1 : 0, left ? 1 : 0, right ? 1 : 0,
                   a ? 1 : 0, b ? 1 : 0, start ? 1 : 0, select ? 1 : 0);
        }
    }
    
    // Skip Player 2 for now to isolate the problem
}

void AllegroMainWindow::updateAndDraw()
{
    // Clear the back buffer instead of screen
    clear_to_color(back_buffer, makecol(0, 0, 0));
    
    // Draw everything to the back buffer
    if (currentDialog != DIALOG_NONE) {
        drawDialog(back_buffer);
    } else if (showingMenu) {
        drawMenu(back_buffer);
    } else {
        drawGame(back_buffer);
    }
    
    drawStatusBar(back_buffer);
    
    // Copy back buffer to screen in one operation
    blit(back_buffer, screen, 0, 0, 0, 0, SCREEN_W, SCREEN_H);
    
    // Optional: Add vsync to prevent tearing
    #ifdef __DJGPP__
    vsync();
    #endif
}

void AllegroMainWindow::clearScreen()
{
    clear_to_color(screen, makecol(0, 0, 0));
}

void AllegroMainWindow::drawGame(BITMAP* target)
{
    if (currentFrameBuffer) {
        // Convert 32-bit RGBA to 16-bit RGB for Allegro
        for (int y = 0; y < RENDER_HEIGHT; y++) {
            for (int x = 0; x < RENDER_WIDTH; x++) {
                uint32_t pixel = currentFrameBuffer[y * RENDER_WIDTH + x];
                
                // Extract RGB components
                int r = (pixel >> 16) & 0xFF;
                int g = (pixel >> 8) & 0xFF;
                int b = pixel & 0xFF;
                
                // Convert to 16-bit color
                int color16 = makecol(r, g, b);
                
                putpixel(game_buffer, x, y, color16);
            }
        }
        
        // Scale and blit to target buffer
        int scale_x = SCREEN_W / RENDER_WIDTH;
        int scale_y = SCREEN_H / RENDER_HEIGHT;
        int scale = (scale_x < scale_y) ? scale_x : scale_y;
        
        if (scale < 1) scale = 1;
        
        int dest_w = RENDER_WIDTH * scale;
        int dest_h = RENDER_HEIGHT * scale;
        int dest_x = (SCREEN_W - dest_w) / 2;
        int dest_y = (SCREEN_H - dest_h) / 2;
        
        stretch_blit(game_buffer, target, 0, 0, RENDER_WIDTH, RENDER_HEIGHT,
                    dest_x, dest_y, dest_w, dest_h);
    }
}

void AllegroMainWindow::drawMenu(BITMAP* target)
{
    int menu_x = SCREEN_W / 2 - 100;
    int menu_y = SCREEN_H / 2 - (menuCount * 10);
    
    // Draw menu background
    rectfill(target, menu_x - 20, menu_y - 20, 
             menu_x + 200, menu_y + menuCount * 20 + 20, 
             makecol(64, 64, 64));
    
    // Draw menu border
    rect(target, menu_x - 20, menu_y - 20, 
         menu_x + 200, menu_y + menuCount * 20 + 20, 
         makecol(255, 255, 255));
    
    // Draw menu title
    drawTextCentered(target, menu_y - 10, "GAME MENU", makecol(255, 255, 0));
    
    // Draw menu items
    for (int i = 0; i < menuCount; i++) {
        int color = (i == selectedMenuItem) ? makecol(255, 255, 0) : makecol(255, 255, 255);
        if (i == selectedMenuItem) {
            drawText(target, menu_x - 10, menu_y + i * 20, ">", color);
        }
        drawText(target, menu_x, menu_y + i * 20, mainMenu[i].text, color);
    }
}

void AllegroMainWindow::drawDialog(BITMAP* target)
{
    int dialog_x = SCREEN_W / 4;
    int dialog_y = SCREEN_H / 4;
    int dialog_w = SCREEN_W / 2;
    int dialog_h = SCREEN_H / 2;
    
    // Draw dialog background
    rectfill(target, dialog_x, dialog_y, dialog_x + dialog_w, dialog_y + dialog_h, 
             makecol(32, 32, 32));
    
    // Draw dialog border
    rect(target, dialog_x, dialog_y, dialog_x + dialog_w, dialog_y + dialog_h, 
         makecol(255, 255, 255));
    
    switch (currentDialog) {
        case DIALOG_ABOUT:
            drawTextCentered(target, dialog_y + 20, "SUPER MARIO BROS Virtualizer", makecol(255, 255, 0));
            drawTextCentered(target, dialog_y + 40, "Version 1.0", makecol(255, 255, 255));
            drawTextCentered(target, dialog_y + 60, "Built with Allegro 4", makecol(255, 255, 255));
            drawTextCentered(target, dialog_y + 100, "Original game (c) Nintendo", makecol(255, 255, 255));
            drawTextCentered(target, dialog_y + dialog_h - 40, "Press ESC to close", makecol(255, 255, 0));
            break;
            
        case DIALOG_HELP:
            drawTextCentered(target, dialog_y + 20, "HELP", makecol(255, 255, 0));
            drawTextCentered(target, dialog_y + 50, "ESC - Show menu", makecol(255, 255, 255));
            drawTextCentered(target, dialog_y + 70, "P - Pause game", makecol(255, 255, 255));
            drawTextCentered(target, dialog_y + 90, "Ctrl+R - Reset game", makecol(255, 255, 255));
            drawTextCentered(target, dialog_y + dialog_h - 40, "Press ESC to close", makecol(255, 255, 0));
            break;
            
        case DIALOG_CONTROLS_P1:
        case DIALOG_CONTROLS_P2:
            drawTextCentered(target, dialog_y + 20, "CONTROL CONFIGURATION", makecol(255, 255, 0));
            drawTextCentered(target, dialog_y + 50, "Not yet implemented", makecol(255, 255, 255));
            drawTextCentered(target, dialog_y + dialog_h - 40, "Press ESC to close", makecol(255, 255, 0));
            break;
    }
}

void AllegroMainWindow::drawStatusBar(BITMAP* target)
{
    int status_y = SCREEN_H - 20;
    rectfill(target, 0, status_y, SCREEN_W, SCREEN_H, makecol(64, 64, 64));
    drawText(target, 10, status_y + 5, statusMessage, makecol(255, 255, 255));
}


void AllegroMainWindow::drawText(BITMAP* target, int x, int y, const char* text, int color)
{
    textout(target, font, text, x, y, color);
}

void AllegroMainWindow::drawTextCentered(BITMAP* target, int y, const char* text, int color)
{
    int x = (SCREEN_W - text_length(font, text)) / 2;
    textout(target, font, text, x, y, color);
}

void AllegroMainWindow::menuUp()
{
    selectedMenuItem--;
    if (selectedMenuItem < 0) {
        selectedMenuItem = menuCount - 1;
    }
}

void AllegroMainWindow::menuDown()
{
    selectedMenuItem++;
    if (selectedMenuItem >= menuCount) {
        selectedMenuItem = 0;
    }
}

void AllegroMainWindow::menuSelect()
{
    switch (mainMenu[selectedMenuItem].id) {
        case 1: // Resume Game
            showingMenu = false;
            gamePaused = false;
            break;
            
        case 2: // Reset Game
            if (smbEngine) {
                smbEngine->reset();
                setStatusMessage("Game Reset");
            }
            showingMenu = false;
            gamePaused = false;
            break;
            
        case 3: // Player 1 Controls
            currentDialog = DIALOG_CONTROLS_P1;
            currentConfigPlayer = PLAYER_1;
            showingMenu = false;
            break;
            
        case 4: // Player 2 Controls
            currentDialog = DIALOG_CONTROLS_P2;
            currentConfigPlayer = PLAYER_2;
            showingMenu = false;
            break;
            
        case 5: // Help
            currentDialog = DIALOG_HELP;
            showingMenu = false;
            break;
            
        case 6: // About
            currentDialog = DIALOG_ABOUT;
            showingMenu = false;
            break;
            
        case 7: // Quit
            gameRunning = false;
            break;
    }
}

void AllegroMainWindow::menuEscape()
{
    showingMenu = false;
}

void AllegroMainWindow::setStatusMessage(const char* msg)
{
    strncpy(statusMessage, msg, sizeof(statusMessage) - 1);
    statusMessage[sizeof(statusMessage) - 1] = '\0';
    statusMessageTimer = 180; // 3 seconds at 60 FPS
}

void AllegroMainWindow::updateStatusMessage()
{
    if (statusMessageTimer > 0) {
        statusMessageTimer--;
        if (statusMessageTimer == 0) {
            strcpy(statusMessage, "Ready");
        }
    }
}

void AllegroMainWindow::shutdown() 
{
    if (game_buffer) {
        destroy_bitmap(game_buffer);
        game_buffer = NULL;
    }
    
    if (back_buffer) {
        destroy_bitmap(back_buffer);
        back_buffer = NULL;
    }
    
    // Save configuration before shutdown
    saveControlConfig();
    
    allegro_exit();
}

void AllegroMainWindow::saveControlConfig()
{
    // Simple file I/O for DOS
    FILE* f = fopen("controls.cfg", "wb");
    if (f) {
        fwrite(&player1Keys, sizeof(PlayerKeys), 1, f);
        fwrite(&player2Keys, sizeof(PlayerKeys), 1, f);
        fwrite(&player1Joy, sizeof(PlayerJoy), 1, f);
        fwrite(&player2Joy, sizeof(PlayerJoy), 1, f);
        fclose(f);
    }
}

void AllegroMainWindow::loadControlConfig()
{
    FILE* f = fopen("controls.cfg", "rb");
    if (f) {
        fread(&player1Keys, sizeof(PlayerKeys), 1, f);
        fread(&player2Keys, sizeof(PlayerKeys), 1, f);
        fread(&player1Joy, sizeof(PlayerJoy), 1, f);
        fread(&player2Joy, sizeof(PlayerJoy), 1, f);
        fclose(f);
    }
}

// Main function for DOS
int main(int argc, char** argv) 
{
    printf("Super Mario Bros Virtualizer - DOS Version\n");
    printf("Initializing...\n");
    
    AllegroMainWindow mainWindow;
    
    if (!mainWindow.initialize()) {
        printf("Failed to initialize\n");
        return -1;
    }
    
    printf("Starting game...\n");
    mainWindow.run();
    
    return 0;
}
END_OF_MAIN()
