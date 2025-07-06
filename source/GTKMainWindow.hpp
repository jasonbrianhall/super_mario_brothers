#ifndef GTK_MAIN_WINDOW_HPP
#define GTK_MAIN_WINDOW_HPP

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <atomic>
#include <gtk/gtk.h>
#include <map>
#include <string>
#include <thread>

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

class GTKMainWindow {
public:
  GTKMainWindow();
  ~GTKMainWindow();

  bool initialize();
  void run();
  void shutdown();

private:
  // GTK widgets
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *menubar;
  GtkWidget *gameContainer;
  GtkWidget *statusbar;
  GtkWidget *configDialog;

  // SDL integration
  SDL_Window *sdlWindow;
  SDL_Renderer *sdlRenderer;
  SDL_Texture *sdlTexture;

  // Threading
  std::thread gameThread;
  std::atomic<bool> gameRunning;
  std::atomic<bool> gamePaused;

  // Controller configuration widgets storage
  std::map<std::string, GtkWidget *> controlWidgets;

  // Menu creation
  void createMenuBar();
  void createGameContainer();
  void createStatusBar();
  void createConfigDialog() {} // Empty implementation for now

  // Menu callbacks
  static void onFileExit(GtkMenuItem *item, gpointer user_data);
  static void onVirtualizationStart(GtkMenuItem *item, gpointer user_data);
  static void onVirtualizationPause(GtkMenuItem *item, gpointer user_data);
  static void onVirtualizationReset(GtkMenuItem *item, gpointer user_data);
  static void onSettingsVideo(GtkMenuItem *item, gpointer user_data);
  static void onSettingsAudio(GtkMenuItem *item, gpointer user_data);
  static void onSettingsControls(GtkMenuItem *item, gpointer user_data);
  static void onHelpAbout(GtkMenuItem *item, gpointer user_data);

  // GTK drawing and input callbacks
  static gboolean onGameDraw(GtkWidget *widget, cairo_t *cr,
                             gpointer user_data);
  static gboolean onKeyPress(GtkWidget *widget, GdkEventKey *event,
                             gpointer user_data);
  static gboolean onKeyRelease(GtkWidget *widget, GdkEventKey *event,
                               gpointer user_data);

  // SDL integration
  bool initializeSDL();
  void gameLoop();
  bool embedSDLWindow();

  // Configuration dialogs
  void showVideoSettings();
  void showAudioSettings();
  void showControlSettings();
  void showAboutDialog();

  // Status updates
  void updateStatusBar(const std::string &message);

  // Fullscreen functionality
  void toggleFullscreen();
  void exitFullscreen();

  // Controller configuration methods
  GtkWidget *createPlayerControlTab(Player player);
  GtkWidget *createGeneralControlTab();
  void createKeyInputRow(GtkWidget *grid, int row, const char *label,
                         const std::string &key);
  void createButtonInputRow(GtkWidget *grid, int row, const char *label,
                            const std::string &key);
  void loadPlayerControlValues(Player player);
  void setKeyWidgetValue(const std::string &key, int scancode);
  void saveControllerSettings();
  int getKeyScancode(const std::string &key);
  SDL_Scancode gdk_keyval_to_sdl_scancode(guint keyval);

  // Static callback functions
  static gboolean onKeyCapture(GtkWidget *widget, GdkEventKey *event,
                               gpointer user_data);
  static void onRefreshControllers(GtkButton *button, gpointer user_data);
  GtkWidget *createJoystickConfigTab(Player player);
  void createJoystickButtonRow(GtkWidget *grid, int row, const char *label,
                               const std::string &key);
  void createJoystickAxisRow(GtkWidget *grid, int row, const char *label,
                             const std::string &axis_key,
                             const std::string &direction_key);
  static gboolean onJoystickButtonCapture(GtkWidget *widget,
                                          gpointer user_data);
  static void onJoystickDetectButton(GtkButton *button, gpointer user_data);
  static void onJoystickDetectAxis(GtkButton *button, gpointer user_data);
  void startJoystickCapture(const std::string &widgetKey, bool isAxis = false);
  void stopJoystickCapture();
  bool isCapturingJoystick;
  std::string currentCaptureWidget;
  bool currentCaptureIsAxis;
  void captureJoystickButton(int button);
  void captureJoystickAxis(int axis, int value);
  void captureJoystickHat(int hat, int value);
  void captureControllerButton(int button);
  void captureControllerAxis(int axis, int value);
  void testJoystickInput();
  cairo_surface_t* backBuffer;
  guchar* backBufferData;
  bool backBufferInitialized;
  void initializeBackBuffer();
  static void onSettingsFullscreen(GtkMenuItem* item, gpointer user_data);

class ScalingCache {
public:
    uint32_t* scaledBuffer;           // Pre-scaled buffer
    int* sourceToDestX;               // X coordinate mapping table
    int* sourceToDestY;               // Y coordinate mapping table
    int scaleFactor;                  // Current scale factor
    int destWidth, destHeight;        // Destination dimensions
    int destOffsetX, destOffsetY;     // Centering offsets
    bool isValid;                     // Cache validity flag
    
    ScalingCache();
    ~ScalingCache();
    void cleanup();
};

ScalingCache scalingCache;
bool useOptimizedScaling;

// Cache management methods
void initializeScalingCache();
void updateScalingCache(int widget_width, int widget_height);
bool isScalingCacheValid(int widget_width, int widget_height);
void drawGameCached(cairo_t* cr, int widget_width, int widget_height);

// Optimized scaling methods
void drawGame1x1(uint32_t* nesBuffer, cairo_t* cr, int widget_width, int widget_height);
void drawGame2x(uint32_t* nesBuffer, cairo_t* cr, int widget_width, int widget_height);
void drawGame3x(uint32_t* nesBuffer, cairo_t* cr, int widget_width, int widget_height);
void drawGameGenericScale(uint32_t* nesBuffer, cairo_t* cr, int widget_width, int widget_height, int scale);


};

#endif // GTK_MAIN_WINDOW_HPP
