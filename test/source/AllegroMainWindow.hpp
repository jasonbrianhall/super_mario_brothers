#ifndef ALLEGRO_MAIN_WINDOW_HPP
#define ALLEGRO_MAIN_WINDOW_HPP

#include <allegro.h>
#include <string.h>

// Forward declarations
class SMBEngine;

// Include Player enum from Controller
#ifndef PLAYER_ENUM_DEFINED
#define PLAYER_ENUM_DEFINED
enum Player
{
    PLAYER_1 = 0,
    PLAYER_2 = 1
};
#endif // PLAYER_ENUM_DEFINED

// Simple menu item structure
struct MenuItem {
    char text[32];
    int id;
    bool enabled;
};

class AllegroMainWindow {
public:
    AllegroMainWindow();
    ~AllegroMainWindow();

    bool initialize();
    void run();
    void shutdown();

private:
    // Allegro components
    BITMAP* game_buffer;
    BITMAP* back_buffer;
    // Game state
    bool gameRunning;
    bool gamePaused;
    bool showingMenu;
    
    // Simple menu system
    MenuItem mainMenu[8];
    int menuCount;
    int selectedMenuItem;
    bool inMenu;
    
    // Control configuration state
    bool isCapturingInput;
    char currentCaptureKey[32];
    Player currentConfigPlayer;
    
    // Simple dialog state
    enum DialogType {
        DIALOG_NONE,
        DIALOG_ABOUT,
        DIALOG_CONTROLS_P1,
        DIALOG_CONTROLS_P2,
        DIALOG_HELP
    };
    DialogType currentDialog;
    
    // Status message
    char statusMessage[128];
    int statusMessageTimer;
    
    // Game frame buffer pointer
    uint32_t* currentFrameBuffer;
    
    // Key mappings for players
    struct PlayerKeys {
        int up, down, left, right;
        int button_a, button_b;
        int start, select;
    };
    PlayerKeys player1Keys, player2Keys;
    
    // Joystick configuration
    struct PlayerJoy {
        int button_a, button_b;
        int start, select;
        bool use_stick;  // true for analog stick, false for digital
    };
    PlayerJoy player1Joy, player2Joy;
    
    // Initialization
    bool initializeAllegro();
    bool initializeGraphics();
    bool initializeInput();
    void setupDefaultControls();
    void setupMenu();
    
    // Main loop functions
    void handleInput();
    void handleMenuInput();
    void handleDialogInput();
    void handleGameInput();
    void updateAndDraw();
    void gameLoop();
    
    // Menu system
    void drawMenu();
    void drawDialog();
    void showAboutDialog();
    void showControlsDialog(Player player);
    void showHelpDialog();
    
    // Menu navigation
    void menuUp();
    void menuDown();
    void menuSelect();
    void menuEscape();
    
    // Control configuration
    void configurePlayerControls(Player player);
    void captureKeyForAction(const char* action, Player player);
    void captureJoyButtonForAction(const char* action, Player player);
    void saveControlConfig();
    void loadControlConfig();
    
    // Graphics helpers
    void clearScreen();
    void drawText(int x, int y, const char* text, int color);
    void drawTextCentered(int y, const char* text, int color);
    void drawStatusBar();
    void copyGameBuffer();
    void drawGame(BITMAP* target = screen);
void drawMenu(BITMAP* target = screen);
void drawDialog(BITMAP* target = screen);
void drawStatusBar(BITMAP* target = screen);
void drawText(BITMAP* target, int x, int y, const char* text, int color);
void drawTextCentered(BITMAP* target, int y, const char* text, int color);
    // Utility functions
    void setStatusMessage(const char* msg);
    void updateStatusMessage();
    const char* getKeyName(int scancode);
    bool isKeyPressed(int scancode);
    
    // Input conversion
    void processAllegroInput();
    void checkPlayerInput(Player player);
    
    // File I/O for config (simple DOS file operations)
    void writeConfigFile();
    void readConfigFile();
};

// Global callback for Allegro timer
void timer_callback();

// Global instance pointer for callbacks
extern AllegroMainWindow* g_mainWindow;

#endif // ALLEGRO_MAIN_WINDOW_HPP
