#ifndef ALLEGRO_MAIN_WINDOW_HPP
#define ALLEGRO_MAIN_WINDOW_HPP

#include <allegro.h>
#include <string.h>

// Forward declarations
class SMBEngine;

// Include Player enum from Controller
#ifndef PLAYER_ENUM_DEFINED
#define PLAYER_ENUM_DEFINED
enum Player { PLAYER_1 = 0, PLAYER_2 = 1 };
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
  void run(const char* romFilename);
  void shutdown();
  enum CaptureType {
    CAPTURE_NONE,
    CAPTURE_KEY_UP,
    CAPTURE_KEY_DOWN,
    CAPTURE_KEY_LEFT,
    CAPTURE_KEY_RIGHT,
    CAPTURE_KEY_A,
    CAPTURE_KEY_B,
    CAPTURE_KEY_START,
    CAPTURE_KEY_SELECT,
    CAPTURE_JOY_A,
    CAPTURE_JOY_B,
    CAPTURE_JOY_START,
    CAPTURE_JOY_SELECT
  };
  CaptureType currentCaptureType;
void renderScaled(uint16_t* buffer, int screenWidth, int screenHeight);
void renderScaled32(uint32_t* buffer, int screenWidth, int screenHeight);

private:
  // Allegro components
  BITMAP *game_buffer;
  BITMAP *back_buffer;
  uint16_t *screenBuffer16; // 16-bit screen buffer for direct rendering
  bool useDirectRendering;  // Flag to enable optimized rendering
  // Game state
  bool gameRunning;
  bool gamePaused;
  bool showingMenu;
  void testKeyboardBasic();
  // Simple menu system
  MenuItem mainMenu[10];
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
    DIALOG_HELP,
    DIALOG_VIDEO_OPTIONS
  };
  DialogType currentDialog;

  // Status message
  char statusMessage[128];
  int statusMessageTimer;

  // Game frame buffer pointer
  uint16_t *currentFrameBuffer;

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
    bool use_stick; // true for analog stick, false for digital
  };
  PlayerJoy player1Joy, player2Joy;

struct VideoSettings {
    int currentMode;        // Index into available modes
    int scalingMode;        // 0=nearest, 1=smooth, 2=scanlines
    bool maintainAspect;    // Keep 4:3 aspect ratio
    bool centerImage;       // Center the game image
    int brightness;         // 0-255
    int contrast;          // 0-255
};

struct VideoMode {
    int width, height;
    char description[32];   // Fixed size for DOS compatibility
    bool available;
};

VideoSettings videoSettings;
VideoMode availableModes[10];
int numAvailableModes;
int selectedVideoOption;

void setupVideoModes();
void testVideoMode(int index);
bool setVideoMode(int index);
void drawVideoOptionsDialog(BITMAP* target);
void handleVideoOptionsInput();
void saveVideoConfig();
void loadVideoConfig();
void resetVideoDefaults();

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
  void captureKeyForAction(const char *action, Player player);
  void captureJoyButtonForAction(const char *action, Player player);
  void saveControlConfig();
  void loadControlConfig();

  // Graphics helpers
  void clearScreen();
  void drawText(int x, int y, const char *text, int color);
  void drawTextCentered(int y, const char *text, int color);
  void drawStatusBar();
  void copyGameBuffer();
  void drawGame(BITMAP *target = screen);
  void drawMenu(BITMAP *target = screen);
  void drawDialog(BITMAP *target = screen);
  void drawStatusBar(BITMAP *target = screen);
  void drawText(BITMAP *target, int x, int y, const char *text, int color);
  void drawTextCentered(BITMAP *target, int y, const char *text, int color);
  // Utility functions
  void setStatusMessage(const char *msg);
  void updateStatusMessage();
  const char *getKeyName(int scancode);
  bool isKeyPressed(int scancode);

  // Input conversion
  void processAllegroInput();
  void checkPlayerInput(Player player);

  // File I/O for config (simple DOS file operations)
  void writeConfigFile();
  void readConfigFile();
  void drawGameDirect(BITMAP *target);    // Direct 16-bit rendering
  void drawGameBuffered(BITMAP *target);  // Fallback buffered rendering
  void drawGameUltraFast(BITMAP *target); // Ultra-fast direct to screen

  // 16-bit conversion utilities
  void convertBuffer16ToBitmap(uint16_t *buffer16, BITMAP *bitmap, int width,
                               int height);
  void convertNESBuffer16ToBitmap(uint16_t *nesBuffer, BITMAP *bitmap);
  void convertBuffer16ToBitmap16(uint16_t *buffer16, BITMAP *bitmap, int dest_x,
                                 int dest_y, int dest_w, int dest_h, int scale);
  void convertBuffer16ToBitmapGeneric(uint16_t *buffer16, BITMAP *bitmap,
                                      int dest_x, int dest_y, int dest_w,
                                      int dest_h, int scale);

  void handleMenuInputNoEsc();
  void handleGameInputNoEsc();
  void handleDialogInputNoEsc();
  void convertBuffer16ToBitmapScaled(uint16_t *buffer16, BITMAP *bitmap,
                                     int width, int height);

  void assignCapturedKey(int scancode);
  void assignCapturedJoyButton(int buttonNum);
  void resetPlayer1ControlsToDefault();
  void resetPlayer2ControlsToDefault();
  void testCurrentJoystick();
  
  void displayJoystickInfo();
  void drawControlsDialog(BITMAP* target, Player player);
  void showJoystickStatus();
  bool isValidJoystickButton(int joyIndex, int buttonNum);
  void runJoystickTest();
  bool getJoystickDirection(int joyIndex, int* x_dir, int* y_dir);
  void printControlMappings();
  void startKeyCapture(CaptureType captureType, const char* promptText);
  void debugJoystickInfo(int joyIndex);
  void convertBuffer16ToBitmap16_2x(uint16_t* buffer16, BITMAP* bitmap, 
                                                    int dest_x, int dest_y);
    bool isFullscreen;
    int windowedWidth, windowedHeight;
    int fullscreenWidth, fullscreenHeight;
    bool createBuffers();
#ifndef __DJGPP__
    bool toggleFullscreen();
#endif


    struct ScalingCache {
        uint16_t* scaledBuffer;          // Pre-scaled buffer
        int* sourceToDestX;              // X coordinate mapping table
        int* sourceToDestY;              // Y coordinate mapping table
        int scaleFactor;                 // Current scale factor
        int destWidth, destHeight;       // Destination dimensions
        int destOffsetX, destOffsetY;    // Centering offsets
        bool isValid;                    // Cache validity flag
        
        ScalingCache();
        ~ScalingCache();
        void cleanup();
    };
    
    ScalingCache scalingCache;
    uint32_t* lineStartOffsets;         // Pre-calculated line start offsets
    bool useOptimizedScaling;           // Enable/disable optimization
    
    // Scaling methods
    void initializeScalingCache();
    void updateScalingCache();
    bool isScalingCacheValid();
    void drawGameCached(BITMAP* target);
    void drawGameFastScale(BITMAP* target);
    
    // Optimized scaling methods
    void drawGame1x1(uint16_t* nesBuffer, BITMAP* target);
    void drawGame2x(uint16_t* nesBuffer, BITMAP* target);
    void drawGame3x(uint16_t* nesBuffer, BITMAP* target);
    void drawGameGenericScale(uint16_t* nesBuffer, BITMAP* target, int scale);
    
    #ifdef __DJGPP__
    // DOS-specific optimizations
    void drawGameDOS320x200(BITMAP* target);
    void drawGameDOSOptimized(BITMAP* target);
    #endif
    void convertScreenBuffer16ToBitmap(uint16_t* buffer16, BITMAP* bitmap);
    void convertScreenBuffer32ToBitmap(uint32_t* buffer32, BITMAP* bitmap);
};

void debugMapperInfo(const char* romFilename);

#endif // ALLEGRO_MAIN_WINDOW_HPP
