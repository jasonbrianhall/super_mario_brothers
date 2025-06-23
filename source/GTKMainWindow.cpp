// Replace your GTKMainWindow.cpp with this corrected version:

#include "GTKMainWindow.hpp"
#include "SMB/SMBEngine.hpp"
#include "Emulation/Controller.hpp"
#include "Configuration.hpp"
#include "Constants.hpp"
#include "Util/Video.hpp"
#include "Util/VideoFilters.hpp"
#include "SMBRom.hpp"

#ifdef _WIN32
#include "WindowsAudio.hpp"
#endif

// Add this as a member variable in GTKMainWindow class (in header file):
#ifdef _WIN32
    WindowsAudio* windowsAudio;
#endif

// Global variables for the game engine
static SMBEngine* smbEngine = nullptr;
static uint32_t renderBuffer[RENDER_WIDTH * RENDER_HEIGHT];
static uint32_t filteredBuffer[RENDER_WIDTH * RENDER_HEIGHT];
static uint32_t* currentFrameBuffer = nullptr;

GTKMainWindow::GTKMainWindow() 
    : window(nullptr), vbox(nullptr), menubar(nullptr), 
      gameContainer(nullptr), statusbar(nullptr), configDialog(nullptr),
      sdlWindow(nullptr), sdlRenderer(nullptr), sdlTexture(nullptr),
      gameRunning(false), gamePaused(false),
      isCapturingJoystick(false), currentCaptureIsAxis(false),
      backBuffer(nullptr), backBufferData(nullptr), backBufferInitialized(false)
{
}

void GTKMainWindow::initializeBackBuffer() {
    if (backBufferInitialized) return;
    
    backBufferData = g_new(guchar, RENDER_WIDTH * RENDER_HEIGHT * 4);
    backBuffer = cairo_image_surface_create_for_data(
        backBufferData,
        CAIRO_FORMAT_ARGB32,
        RENDER_WIDTH,
        RENDER_HEIGHT,
        RENDER_WIDTH * 4
    );
    backBufferInitialized = true;
}

GTKMainWindow::~GTKMainWindow() 
{
    if (backBuffer) {
        cairo_surface_destroy(backBuffer);
    }
    if (backBufferData) {
        g_free(backBufferData);
    }
    shutdown();
}

bool GTKMainWindow::initialize() 
{
    // Initialize GTK
    gtk_init(nullptr, nullptr);
    
    // Create main window
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), APP_TITLE);
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    
    // Set window background to black
    GdkRGBA black_color;
    black_color.red = 0.0;
    black_color.green = 0.0;
    black_color.blue = 0.0;
    black_color.alpha = 1.0;
    gtk_widget_override_background_color(window, GTK_STATE_FLAG_NORMAL, &black_color);
    
    // Connect window close event
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), nullptr);
    
    // Create main vertical box
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    
    // Create menu bar
    createMenuBar();
    
    // Create game container
    createGameContainer();
    
    // Create status bar
    createStatusBar();
    
    // Initialize SDL
    if (!initializeSDL()) {
        return false;
    }
    
    // Show all widgets
    gtk_widget_show_all(window);
    
    return true;
}

void GTKMainWindow::captureJoystickButton(int button)
{
    std::cout << "Captured joystick button: " << button << std::endl;
    
    // Update the UI on the main thread
    gdk_threads_add_idle([](gpointer data) -> gboolean {
        auto* capture_data = static_cast<std::pair<GTKMainWindow*, int>*>(data);
        GTKMainWindow* window = capture_data->first;
        int button = capture_data->second;
        
        // Find and update the spin button
        if (window->controlWidgets.find(window->currentCaptureWidget) != window->controlWidgets.end()) {
            GtkWidget* spin = window->controlWidgets[window->currentCaptureWidget];
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), button);
            
            // Update button label to show success
            std::string detect_key = window->currentCaptureWidget + "_detect";
            if (window->controlWidgets.find(detect_key) != window->controlWidgets.end()) {
                GtkWidget* detect_button = window->controlWidgets[detect_key];
                gtk_button_set_label(GTK_BUTTON(detect_button), "Captured!");
            }
        }
        
        // Stop capture after a short delay
        g_timeout_add(1000, [](gpointer data) -> gboolean {
            static_cast<GTKMainWindow*>(data)->stopJoystickCapture();
            return G_SOURCE_REMOVE;
        }, window);
        
        delete capture_data;
        return G_SOURCE_REMOVE;
    }, new std::pair<GTKMainWindow*, int>(this, button));
}

void GTKMainWindow::captureJoystickAxis(int axis, int value)
{
    std::cout << "Captured joystick axis: " << axis << " value: " << value << std::endl;
    
    // Determine direction based on value
    bool inverted = value < 0;
    
    // Update the UI on the main thread
    gdk_threads_add_idle([](gpointer data) -> gboolean {
        auto* capture_data = static_cast<std::tuple<GTKMainWindow*, int, bool>*>(data);
        GTKMainWindow* window = std::get<0>(*capture_data);
        int axis = std::get<1>(*capture_data);
        bool inverted = std::get<2>(*capture_data);
        
        // Update axis spin button
        if (window->controlWidgets.find(window->currentCaptureWidget) != window->controlWidgets.end()) {
            GtkWidget* axis_spin = window->controlWidgets[window->currentCaptureWidget];
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(axis_spin), axis);
        }
        
        // Update direction combo
        std::string dir_key = window->currentCaptureWidget;
        size_t pos = dir_key.find("axis_");
        if (pos != std::string::npos) {
            dir_key = dir_key.substr(0, pos + 5) + dir_key.substr(pos + 6) + "_dir";
            if (window->controlWidgets.find(dir_key) != window->controlWidgets.end()) {
                GtkWidget* dir_combo = window->controlWidgets[dir_key];
                gtk_combo_box_set_active(GTK_COMBO_BOX(dir_combo), inverted ? 1 : 0);
            }
        }
        
        // Update button label to show success
        std::string detect_key = window->currentCaptureWidget + "_detect";
        if (window->controlWidgets.find(detect_key) != window->controlWidgets.end()) {
            GtkWidget* detect_button = window->controlWidgets[detect_key];
            gtk_button_set_label(GTK_BUTTON(detect_button), "Captured!");
        }
        
        // Stop capture after a short delay
        g_timeout_add(1000, [](gpointer data) -> gboolean {
            static_cast<GTKMainWindow*>(data)->stopJoystickCapture();
            return G_SOURCE_REMOVE;
        }, window);
        
        delete capture_data;
        return G_SOURCE_REMOVE;
    }, new std::tuple<GTKMainWindow*, int, bool>(this, axis, inverted));
}

void GTKMainWindow::captureJoystickHat(int hat, int value)
{
    std::cout << "Captured joystick hat: " << hat << " value: " << value << std::endl;
    
    // Update the UI on the main thread
    gdk_threads_add_idle([](gpointer data) -> gboolean {
        auto* capture_data = static_cast<std::pair<GTKMainWindow*, int>*>(data);
        GTKMainWindow* window = capture_data->first;
        int hat = capture_data->second;
        
        // Find the hat number widget for this player
        std::string hat_key = window->currentCaptureWidget;
        size_t pos = hat_key.find("axis_");
        if (pos != std::string::npos) {
            std::string player_prefix = hat_key.substr(0, 3); // "p1_" or "p2_"
            std::string hat_widget_key = player_prefix + "hat_number";
            
            if (window->controlWidgets.find(hat_widget_key) != window->controlWidgets.end()) {
                GtkWidget* hat_spin = window->controlWidgets[hat_widget_key];
                gtk_spin_button_set_value(GTK_SPIN_BUTTON(hat_spin), hat);
                
                // Also set movement type to D-Pad
                std::string movement_type_key = player_prefix + "movement_type";
                if (window->controlWidgets.find(movement_type_key) != window->controlWidgets.end()) {
                    GtkWidget* movement_combo = window->controlWidgets[movement_type_key];
                    gtk_combo_box_set_active(GTK_COMBO_BOX(movement_combo), 1); // D-Pad option
                }
                
                // Update button label to show success
                std::string detect_key = window->currentCaptureWidget + "_detect";
                if (window->controlWidgets.find(detect_key) != window->controlWidgets.end()) {
                    GtkWidget* detect_button = window->controlWidgets[detect_key];
                    gtk_button_set_label(GTK_BUTTON(detect_button), "Captured!");
                }
            }
        }
        
        // Stop capture after a short delay
        g_timeout_add(1000, [](gpointer data) -> gboolean {
            static_cast<GTKMainWindow*>(data)->stopJoystickCapture();
            return G_SOURCE_REMOVE;
        }, window);
        
        delete capture_data;
        return G_SOURCE_REMOVE;
    }, new std::pair<GTKMainWindow*, int>(this, hat));
}

void GTKMainWindow::captureControllerButton(int button)
{
    // Same as joystick button but for SDL Game Controller API
    captureJoystickButton(button);
}

void GTKMainWindow::captureControllerAxis(int axis, int value)
{
    // Same as joystick axis but for SDL Game Controller API
    captureJoystickAxis(axis, value);
}

void GTKMainWindow::createMenuBar() 
{
    menubar = gtk_menu_bar_new();
    
    // File menu
    GtkWidget* fileMenu = gtk_menu_new();
    GtkWidget* fileMenuItem = gtk_menu_item_new_with_label("File");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(fileMenuItem), fileMenu);
    
    // File -> Reset Game
    GtkWidget* resetGameItem = gtk_menu_item_new_with_label("Reset Game");
    g_signal_connect(resetGameItem, "activate", G_CALLBACK(onVirtualizationReset), this);
    gtk_menu_shell_append(GTK_MENU_SHELL(fileMenu), resetGameItem);
    
    // File -> Separator
    GtkWidget* separator1 = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(fileMenu), separator1);
    
    // File -> Quit
    GtkWidget* quitItem = gtk_menu_item_new_with_label("Quit");
    g_signal_connect(quitItem, "activate", G_CALLBACK(onFileExit), this);
    gtk_menu_shell_append(GTK_MENU_SHELL(fileMenu), quitItem);
    
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), fileMenuItem);
    
    // Settings menu - updated with fullscreen option
    GtkWidget* settingsMenu = gtk_menu_new();
    GtkWidget* settingsMenuItem = gtk_menu_item_new_with_label("Settings");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(settingsMenuItem), settingsMenu);
    
    // Settings -> Toggle Fullscreen (NEW)
    GtkWidget* fullscreenItem = gtk_menu_item_new_with_label("Toggle Fullscreen\tF11");
    g_signal_connect(fullscreenItem, "activate", G_CALLBACK(onSettingsFullscreen), this);
    gtk_menu_shell_append(GTK_MENU_SHELL(settingsMenu), fullscreenItem);
    
    // Settings -> Separator
    GtkWidget* separator2 = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(settingsMenu), separator2);
    
    // Settings -> Controls
    GtkWidget* controlsItem = gtk_menu_item_new_with_label("Controls");
    g_signal_connect(controlsItem, "activate", G_CALLBACK(onSettingsControls), this);
    gtk_menu_shell_append(GTK_MENU_SHELL(settingsMenu), controlsItem);
    
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), settingsMenuItem);
    
    // Help menu
    GtkWidget* helpMenu = gtk_menu_new();
    GtkWidget* helpMenuItem = gtk_menu_item_new_with_label("Help");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(helpMenuItem), helpMenu);
    
    // Help -> About
    GtkWidget* aboutItem = gtk_menu_item_new_with_label("About");
    g_signal_connect(aboutItem, "activate", G_CALLBACK(onHelpAbout), this);
    gtk_menu_shell_append(GTK_MENU_SHELL(helpMenu), aboutItem);
    
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), helpMenuItem);
    
    // Add menu bar to main container
    gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, FALSE, 0);
}

static void GTKMainWindow::onSettingsFullscreen(GtkMenuItem* item, gpointer user_data) 
{
    GTKMainWindow* window = static_cast<GTKMainWindow*>(user_data);
    window->toggleFullscreen();
}

void GTKMainWindow::createGameContainer() 
{
    gameContainer = gtk_drawing_area_new();
    gtk_widget_set_size_request(gameContainer, RENDER_WIDTH * 2, RENDER_HEIGHT * 2);
    
    // Connect the draw signal to render the game
    g_signal_connect(gameContainer, "draw", G_CALLBACK(onGameDraw), this);
    
    // Set up event mask for keyboard input
    gtk_widget_set_can_focus(gameContainer, TRUE);
    gtk_widget_add_events(gameContainer, GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK | GDK_FOCUS_CHANGE_MASK);
    g_signal_connect(gameContainer, "key-press-event", G_CALLBACK(onKeyPress), this);
    g_signal_connect(gameContainer, "key-release-event", G_CALLBACK(onKeyRelease), this);
    
    gtk_box_pack_start(GTK_BOX(vbox), gameContainer, TRUE, TRUE, 0);
}

void GTKMainWindow::createStatusBar() 
{
    statusbar = gtk_statusbar_new();
    gtk_box_pack_start(GTK_BOX(vbox), statusbar, FALSE, FALSE, 0);
    updateStatusBar("Ready");
}

void GTKMainWindow::toggleFullscreen() 
{
    GdkWindow* gdkWindow = gtk_widget_get_window(window);
    if (!gdkWindow) {
        // Window not realized yet, just return
        return;
    }
    
    GdkWindowState state = gdk_window_get_state(gdkWindow);
    
    if (state & GDK_WINDOW_STATE_FULLSCREEN) {
        // Currently fullscreen, exit it
        exitFullscreen();
    } else {
        // Not fullscreen, enter it
        gtk_widget_hide(menubar);
        gtk_widget_hide(statusbar);
        gtk_window_fullscreen(GTK_WINDOW(window));
        updateStatusBar("Fullscreen mode - Press F11 or Escape to exit");
    }
}

void GTKMainWindow::exitFullscreen() 
{
    GdkWindow* gdkWindow = gtk_widget_get_window(window);
    if (!gdkWindow) {
        // Window not realized yet, just return
        return;
    }
    
    GdkWindowState state = gdk_window_get_state(gdkWindow);
    
    if (state & GDK_WINDOW_STATE_FULLSCREEN) {
        // Exit fullscreen
        gtk_window_unfullscreen(GTK_WINDOW(window));
        
        // Show the menu bar and status bar
        gtk_widget_show(menubar);
        gtk_widget_show(statusbar);
        
        updateStatusBar("Windowed mode");
    }
}


// GTK drawing callback - renders the game frame
gboolean GTKMainWindow::onGameDraw(GtkWidget* widget, cairo_t* cr, gpointer user_data) 
{
    // Always fill with black background first to prevent artifacts
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_paint(cr);
    
    if (!currentFrameBuffer) {
        // No frame available yet - background is already black
        return TRUE;
    }
    
    // Get widget dimensions
    int widget_width = gtk_widget_get_allocated_width(widget);
    int widget_height = gtk_widget_get_allocated_height(widget);
    
    // Create a static buffer to avoid repeated allocations (Windows performance issue)
    static guchar* rgb_data = nullptr;
    static size_t buffer_size = 0;
    size_t required_size = RENDER_WIDTH * RENDER_HEIGHT * 4;
    
    if (!rgb_data || buffer_size < required_size) {
        if (rgb_data) g_free(rgb_data);
        rgb_data = g_new(guchar, required_size);
        buffer_size = required_size;
    }
    
    // Convert pixel format - optimize for Windows
    // The original buffer might be in different formats, ensure BGRA32
    for (int i = 0; i < RENDER_WIDTH * RENDER_HEIGHT; i++) {
        uint32_t pixel = currentFrameBuffer[i];
        
        // For Cairo ARGB32 format on Windows: Blue, Green, Red, Alpha
        // Ensure proper byte order for Windows
        rgb_data[i * 4 + 0] = pixel & 0xFF;         // Blue
        rgb_data[i * 4 + 1] = (pixel >> 8) & 0xFF;  // Green  
        rgb_data[i * 4 + 2] = (pixel >> 16) & 0xFF; // Red
        rgb_data[i * 4 + 3] = 255;                  // Alpha (fully opaque)
    }
    
    // Create Cairo surface with explicit format
    cairo_surface_t* surface = cairo_image_surface_create_for_data(
        rgb_data,
        CAIRO_FORMAT_ARGB32,  // Explicitly use ARGB32
        RENDER_WIDTH,
        RENDER_HEIGHT,
        RENDER_WIDTH * 4
    );
    
    // Check surface status to prevent rendering corrupted surfaces
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surface);
        return TRUE;
    }
    
    // Scale to fit the widget while maintaining aspect ratio
    double scale_x = (double)widget_width / RENDER_WIDTH;
    double scale_y = (double)widget_height / RENDER_HEIGHT;
    double scale = (scale_x < scale_y) ? scale_x : scale_y;
    
    // Center the image
    double offset_x = (widget_width - RENDER_WIDTH * scale) / 2;
    double offset_y = (widget_height - RENDER_HEIGHT * scale) / 2;
    
    // Use high-quality scaling to reduce artifacts
    cairo_save(cr);
    cairo_translate(cr, offset_x, offset_y);
    cairo_scale(cr, scale, scale);
    
    // Set high-quality filter for Windows
    cairo_pattern_t* pattern = cairo_pattern_create_for_surface(surface);
    //cairo_pattern_set_filter(pattern, CAIRO_FILTER_GOOD);  // Better than CAIRO_FILTER_FAST
    
    // Draw the surface with the pattern
    cairo_set_source(cr, pattern);
    cairo_paint(cr);
    
    // Cleanup
    cairo_pattern_destroy(pattern);
    cairo_restore(cr);
    cairo_surface_destroy(surface);
    
    return TRUE;
}

// GTK keyboard input callbacks
gboolean GTKMainWindow::onKeyPress(GtkWidget* widget, GdkEventKey* event, gpointer user_data) 
{
    GTKMainWindow* window = static_cast<GTKMainWindow*>(user_data);
    
    if (smbEngine) {
        Controller& controller1 = smbEngine->getController1();
        
        // Handle fullscreen toggle (F11 key - standard)
        if (event->keyval == GDK_KEY_F11) {
            window->toggleFullscreen();
            return TRUE;
        }
        
        // Handle escape key to exit fullscreen
        if (event->keyval == GDK_KEY_Escape) {
            window->exitFullscreen();
            return TRUE;
        }
        
        // Convert GDK key to SDL scancode for comparison with configuration
        GTKMainWindow temp;
        SDL_Scancode scancode = temp.gdk_keyval_to_sdl_scancode(event->keyval);
        
        if (scancode != SDL_SCANCODE_UNKNOWN) {
            // Check Player 1 controls
            if (scancode == Configuration::getPlayer1KeyUp()) {
                controller1.setButtonState(PLAYER_1, BUTTON_UP, true);
            } else if (scancode == Configuration::getPlayer1KeyDown()) {
                controller1.setButtonState(PLAYER_1, BUTTON_DOWN, true);
            } else if (scancode == Configuration::getPlayer1KeyLeft()) {
                controller1.setButtonState(PLAYER_1, BUTTON_LEFT, true);
            } else if (scancode == Configuration::getPlayer1KeyRight()) {
                controller1.setButtonState(PLAYER_1, BUTTON_RIGHT, true);
            } else if (scancode == Configuration::getPlayer1KeyA()) {
                controller1.setButtonState(PLAYER_1, BUTTON_A, true);
            } else if (scancode == Configuration::getPlayer1KeyB()) {
                controller1.setButtonState(PLAYER_1, BUTTON_B, true);
            } else if (scancode == Configuration::getPlayer1KeySelect()) {
                controller1.setButtonState(PLAYER_1, BUTTON_SELECT, true);
            } else if (scancode == Configuration::getPlayer1KeyStart()) {
                controller1.setButtonState(PLAYER_1, BUTTON_START, true);
            }
            
            // Check Player 2 controls
            else if (scancode == Configuration::getPlayer2KeyUp()) {
                controller1.setButtonState(PLAYER_2, BUTTON_UP, true);
            } else if (scancode == Configuration::getPlayer2KeyDown()) {
                controller1.setButtonState(PLAYER_2, BUTTON_DOWN, true);
            } else if (scancode == Configuration::getPlayer2KeyLeft()) {
                controller1.setButtonState(PLAYER_2, BUTTON_LEFT, true);
            } else if (scancode == Configuration::getPlayer2KeyRight()) {
                controller1.setButtonState(PLAYER_2, BUTTON_RIGHT, true);
            } else if (scancode == Configuration::getPlayer2KeyA()) {
                controller1.setButtonState(PLAYER_2, BUTTON_A, true);
            } else if (scancode == Configuration::getPlayer2KeyB()) {
                controller1.setButtonState(PLAYER_2, BUTTON_B, true);
            } else if (scancode == Configuration::getPlayer2KeySelect()) {
                controller1.setButtonState(PLAYER_2, BUTTON_SELECT, true);
            } else if (scancode == Configuration::getPlayer2KeyStart()) {
                controller1.setButtonState(PLAYER_2, BUTTON_START, true);
            }
        }
    }
    
    return TRUE;
}

gboolean GTKMainWindow::onKeyRelease(GtkWidget* widget, GdkEventKey* event, gpointer user_data) 
{
    if (smbEngine) {
        Controller& controller1 = smbEngine->getController1();
        
        // Convert GDK key to SDL scancode for comparison with configuration
        GTKMainWindow temp;
        SDL_Scancode scancode = temp.gdk_keyval_to_sdl_scancode(event->keyval);
        
        if (scancode != SDL_SCANCODE_UNKNOWN) {
            // Check Player 1 controls
            if (scancode == Configuration::getPlayer1KeyUp()) {
                controller1.setButtonState(PLAYER_1, BUTTON_UP, false);
            } else if (scancode == Configuration::getPlayer1KeyDown()) {
                controller1.setButtonState(PLAYER_1, BUTTON_DOWN, false);
            } else if (scancode == Configuration::getPlayer1KeyLeft()) {
                controller1.setButtonState(PLAYER_1, BUTTON_LEFT, false);
            } else if (scancode == Configuration::getPlayer1KeyRight()) {
                controller1.setButtonState(PLAYER_1, BUTTON_RIGHT, false);
            } else if (scancode == Configuration::getPlayer1KeyA()) {
                controller1.setButtonState(PLAYER_1, BUTTON_A, false);
            } else if (scancode == Configuration::getPlayer1KeyB()) {
                controller1.setButtonState(PLAYER_1, BUTTON_B, false);
            } else if (scancode == Configuration::getPlayer1KeySelect()) {
                controller1.setButtonState(PLAYER_1, BUTTON_SELECT, false);
            } else if (scancode == Configuration::getPlayer1KeyStart()) {
                controller1.setButtonState(PLAYER_1, BUTTON_START, false);
            }
            
            // Check Player 2 controls
            else if (scancode == Configuration::getPlayer2KeyUp()) {
                controller1.setButtonState(PLAYER_2, BUTTON_UP, false);
            } else if (scancode == Configuration::getPlayer2KeyDown()) {
                controller1.setButtonState(PLAYER_2, BUTTON_DOWN, false);
            } else if (scancode == Configuration::getPlayer2KeyLeft()) {
                controller1.setButtonState(PLAYER_2, BUTTON_LEFT, false);
            } else if (scancode == Configuration::getPlayer2KeyRight()) {
                controller1.setButtonState(PLAYER_2, BUTTON_RIGHT, false);
            } else if (scancode == Configuration::getPlayer2KeyA()) {
                controller1.setButtonState(PLAYER_2, BUTTON_A, false);
            } else if (scancode == Configuration::getPlayer2KeyB()) {
                controller1.setButtonState(PLAYER_2, BUTTON_B, false);
            } else if (scancode == Configuration::getPlayer2KeySelect()) {
                controller1.setButtonState(PLAYER_2, BUTTON_SELECT, false);
            } else if (scancode == Configuration::getPlayer2KeyStart()) {
                controller1.setButtonState(PLAYER_2, BUTTON_START, false);
            }
        }
    }
    
    return TRUE;
}

bool GTKMainWindow::initializeSDL() 
{
    // SDL will be initialized in the game thread
    // This method is kept for compatibility but doesn't need to do anything
    return true;
}

void GTKMainWindow::run() 
{
    // Show all widgets first
    gtk_widget_show_all(window);
    
    // Make sure the game container can receive focus
    gtk_widget_grab_focus(gameContainer);
    
    // Start the game thread
    gameRunning = true;
    gameThread = std::thread(&GTKMainWindow::gameLoop, this);
    
    // Run GTK main loop
    gtk_main();
}

void GTKMainWindow::shutdown() 
{
    // Stop game thread
    gameRunning = false;
    if (gameThread.joinable()) {
        gameThread.join();
    }
    
    // No SDL video cleanup needed since we're not using SDL video
}

void GTKMainWindow::gameLoop() 
{
#ifdef _WIN32
    // On Windows, force SDL to use DirectSound or WinMM to avoid WASAPI issues
    SDL_SetHint(SDL_HINT_AUDIODRIVER, "directsound");
    SDL_SetHint(SDL_HINT_AUDIO_RESAMPLING_MODE, "1"); // Linear resampling
    // Initialize SDL for joystick/gamecontroller only - we'll handle audio separately
    if (SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) < 0) {
        updateStatusBar("SDL initialization failed");
        return;
    }
    
    // Windows-specific optimizations
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    timeBeginPeriod(1);
#else
    // Initialize SDL for audio and controllers on non-Windows platforms
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) < 0) {
        updateStatusBar("SDL initialization failed");
        return;
    }
#endif

    // Initialize HQDN3D filter if enabled
    uint32_t* prevFrameBuffer = nullptr;
    if (Configuration::getHqdn3dEnabled()) {
        initHQDN3D(RENDER_WIDTH, RENDER_HEIGHT);
        prevFrameBuffer = new uint32_t[RENDER_WIDTH * RENDER_HEIGHT];
        memset(prevFrameBuffer, 0, RENDER_WIDTH * RENDER_HEIGHT * sizeof(uint32_t));
    }

    // Initialize the SMB engine with embedded ROM data
    SMBEngine engine(const_cast<uint8_t*>(smbRomData));
    smbEngine = &engine;
    engine.reset();

    // Initialize controller system for both players
    Controller& controller1 = engine.getController1();
    
    // IMPORTANT: Load controller configuration AFTER Configuration::initialize() has been called
    controller1.loadConfiguration();
    
    bool joystickInitialized = controller1.initJoystick();
    if (joystickInitialized) {
        std::cout << "Controller system initialized successfully!" << std::endl;
        if (controller1.isJoystickConnected(PLAYER_1))
            std::cout << "Player 1 joystick connected" << std::endl;
        if (controller1.isJoystickConnected(PLAYER_2))
            std::cout << "Player 2 joystick connected" << std::endl;
            
        // Use configuration setting for joystick polling
        controller1.setJoystickPolling(Configuration::getJoystickPollingEnabled());
        std::cout << "Joystick polling: " << (Configuration::getJoystickPollingEnabled() ? "enabled" : "disabled") << std::endl;
    } else {
        std::cout << "No joysticks found. Using keyboard controls only." << std::endl;
    }

    // Initialize audio AFTER engine is created
    bool audioInitialized = false;
    

    if (Configuration::getAudioEnabled()) {
        SDL_AudioSpec desiredSpec;
        desiredSpec.freq = Configuration::getAudioFrequency();
        desiredSpec.format = AUDIO_S8;
        desiredSpec.channels = 1;
        desiredSpec.samples = 2048;
        desiredSpec.callback = [](void* userdata, uint8_t* buffer, int len) {
            if (smbEngine != nullptr) {
                smbEngine->audioCallback(buffer, len);
            }
        };
        desiredSpec.userdata = NULL;

        SDL_AudioSpec obtainedSpec;
        if (SDL_OpenAudio(&desiredSpec, &obtainedSpec) < 0) {
            std::cout << "SDL_OpenAudio failed: " << SDL_GetError() << std::endl;
            std::cout << "Continuing without audio..." << std::endl;
        } else {
            audioInitialized = true;
            std::cout << "Audio initialized successfully:" << std::endl;
            std::cout << "  Format: 8-bit" << std::endl;
            std::cout << "  Frequency: " << obtainedSpec.freq << " Hz" << std::endl;
            std::cout << "  Channels: " << (int)obtainedSpec.channels << std::endl;
            std::cout << "  Buffer size: " << obtainedSpec.samples << std::endl;
            SDL_PauseAudio(0);
        }
    }


    updateStatusBar("Game started");

    // Main game loop with precise frame timing
    auto lastFrameTime = std::chrono::high_resolution_clock::now();
    const auto targetFrameTime = std::chrono::microseconds(1000000 / Configuration::getFrameRate());
    
    while (gameRunning) {
        auto frameStart = std::chrono::high_resolution_clock::now();
        
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            // First check if we're capturing joystick input for configuration
            if (isCapturingJoystick) {
                switch (event.type) {
                case SDL_JOYBUTTONDOWN:
                    captureJoystickButton(event.jbutton.button);
                    continue; // Skip normal processing
                    
                case SDL_JOYAXISMOTION:
                    // Only capture significant axis movement
                    if (abs(event.jaxis.value) > 16000) {
                        captureJoystickAxis(event.jaxis.axis, event.jaxis.value);
                        continue; // Skip normal processing
                    }
                    break;
                    
                case SDL_JOYHATMOTION:
                    // Only capture when hat is pressed (not centered)
                    if (event.jhat.value != SDL_HAT_CENTERED) {
                        captureJoystickHat(event.jhat.hat, event.jhat.value);
                        continue; // Skip normal processing
                    }
                    break;
                    
                case SDL_CONTROLLERBUTTONDOWN:
                    captureControllerButton(event.cbutton.button);
                    continue; // Skip normal processing
                    
                case SDL_CONTROLLERAXISMOTION:
                    // Only capture significant axis movement
                    if (abs(event.caxis.value) > 16000) {
                        captureControllerAxis(event.caxis.axis, event.caxis.value);
                        continue; // Skip normal processing
                    }
                    break;
                }
                
                // If we're capturing but this event wasn't handled, continue to skip normal processing
                continue;
            }
            
            // Normal game event processing (only when not capturing)
            switch (event.type) {
            // Process joystick events for both players
            case SDL_JOYAXISMOTION:
            case SDL_JOYHATMOTION:
            case SDL_JOYBUTTONDOWN:
            case SDL_JOYBUTTONUP:
            case SDL_CONTROLLERBUTTONDOWN:
            case SDL_CONTROLLERBUTTONUP:
            case SDL_CONTROLLERAXISMOTION:
                if (joystickInitialized) {
                    controller1.processJoystickEvent(event);
                }
                break;
                
            // Handle joystick connect/disconnect
            case SDL_JOYDEVICEADDED:
                std::cout << "Joystick connected - reinitializing controller system" << std::endl;
                controller1.initJoystick();
                break;
                
            case SDL_JOYDEVICEREMOVED:
                std::cout << "Joystick disconnected" << std::endl;
                break;
                
            default:
                break;
            }
        }

        if (!gamePaused) {
            // Update joystick state once per frame
            if (joystickInitialized) {
                controller1.updateJoystickState();
            }

            engine.update();
            engine.render(renderBuffer);

            // Apply post-processing filters if enabled
            uint32_t* sourceBuffer = renderBuffer;
            uint32_t* targetBuffer = filteredBuffer;

            // Apply HQDN3D filter if enabled
            if (Configuration::getHqdn3dEnabled()) {
                applyHQDN3D(targetBuffer, sourceBuffer, prevFrameBuffer, 
                            RENDER_WIDTH, RENDER_HEIGHT, 
                            Configuration::getHqdn3dSpatialStrength(), 
                            Configuration::getHqdn3dTemporalStrength());
                
                // Store the current frame for next time
                memcpy(prevFrameBuffer, sourceBuffer, RENDER_WIDTH * RENDER_HEIGHT * sizeof(uint32_t));
                
                // Swap buffers for potential next filter
                uint32_t* temp = sourceBuffer;
                sourceBuffer = targetBuffer;
                targetBuffer = temp;
            }

            // Apply FXAA if enabled and method is FXAA
            if (Configuration::getAntiAliasingEnabled() && Configuration::getAntiAliasingMethod() == 0) {
                applyFXAA(targetBuffer, sourceBuffer, RENDER_WIDTH, RENDER_HEIGHT);
                
                // Swap buffers for potential next filter
                uint32_t* temp = sourceBuffer;
                sourceBuffer = targetBuffer;
                targetBuffer = temp;
            }

            // Store the final buffer for GTK rendering
            currentFrameBuffer = sourceBuffer;
            
            // Force immediate redraw with frame synchronization
            gdk_threads_add_idle_full(G_PRIORITY_HIGH_IDLE, [](gpointer data) -> gboolean {
                GTKMainWindow* window = static_cast<GTKMainWindow*>(data);
                gtk_widget_queue_draw(window->gameContainer);
                return G_SOURCE_REMOVE;
            }, this, nullptr);
        }

        // Precise frame timing
        auto frameEnd = std::chrono::high_resolution_clock::now();
        auto frameDuration = frameEnd - frameStart;
        
        if (frameDuration < targetFrameTime) {
            auto sleepTime = targetFrameTime - frameDuration;
            std::this_thread::sleep_for(sleepTime);
        }
    }

    // Cleanup
#ifdef _WIN32
    if (windowsAudio) {
        windowsAudio->shutdown();
        delete windowsAudio;
        windowsAudio = nullptr;
    }
    timeEndPeriod(1);
#endif

    if (Configuration::getHqdn3dEnabled()) {
        cleanupHQDN3D();
        delete[] prevFrameBuffer;
    }

    SDL_CloseAudio();
    SDL_Quit();
}

void GTKMainWindow::updateStatusBar(const std::string& message) 
{
    if (statusbar) {
        guint context_id = gtk_statusbar_get_context_id(GTK_STATUSBAR(statusbar), "main");
        gtk_statusbar_pop(GTK_STATUSBAR(statusbar), context_id);
        gtk_statusbar_push(GTK_STATUSBAR(statusbar), context_id, message.c_str());
    }
}

// Menu callback implementations
void GTKMainWindow::onFileExit(GtkMenuItem* item, gpointer user_data) 
{
    GTKMainWindow* window = static_cast<GTKMainWindow*>(user_data);
    window->updateStatusBar("Exiting...");
    gtk_main_quit();
}

void GTKMainWindow::onVirtualizationStart(GtkMenuItem* item, gpointer user_data) 
{
    GTKMainWindow* window = static_cast<GTKMainWindow*>(user_data);
    window->gamePaused = false;
    window->updateStatusBar("Game resumed");
}

void GTKMainWindow::onVirtualizationPause(GtkMenuItem* item, gpointer user_data) 
{
    GTKMainWindow* window = static_cast<GTKMainWindow*>(user_data);
    window->gamePaused = true;
    window->updateStatusBar("Game paused");
}

void GTKMainWindow::onVirtualizationReset(GtkMenuItem* item, gpointer user_data) 
{
    GTKMainWindow* window = static_cast<GTKMainWindow*>(user_data);
    if (smbEngine) {
        smbEngine->reset();
        window->updateStatusBar("Game reset");
    }
}

void GTKMainWindow::onSettingsVideo(GtkMenuItem* item, gpointer user_data) 
{
    GTKMainWindow* window = static_cast<GTKMainWindow*>(user_data);
    window->showVideoSettings();
}

void GTKMainWindow::onSettingsAudio(GtkMenuItem* item, gpointer user_data) 
{
    GTKMainWindow* window = static_cast<GTKMainWindow*>(user_data);
    window->showAudioSettings();
}

void GTKMainWindow::onSettingsControls(GtkMenuItem* item, gpointer user_data) 
{
    GTKMainWindow* window = static_cast<GTKMainWindow*>(user_data);
    window->showControlSettings();
}

void GTKMainWindow::onHelpAbout(GtkMenuItem* item, gpointer user_data) 
{
    GTKMainWindow* window = static_cast<GTKMainWindow*>(user_data);
    window->showAboutDialog();
}

void GTKMainWindow::showVideoSettings() 
{
    updateStatusBar("Opening video settings...");
    // TODO: Implement video settings dialog
}

void GTKMainWindow::showAudioSettings() 
{
    updateStatusBar("Opening audio settings...");
    // TODO: Implement audio settings dialog
}

void GTKMainWindow::showAboutDialog() 
{
    GtkWidget* dialog = gtk_about_dialog_new();
    
    gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(dialog), APP_TITLE);
    gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(dialog), "1.0");
    
    // Updated comments with Nintendo acknowledgment and fair use notice
    gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(dialog), 
        "A Super Mario Bros virtualizer built with SDL2 and GTK+\n\n"
        "Super Mario Bros is a trademark of Nintendo Co., Ltd.\n"
        "This virtualizer is an independent implementation and is not affiliated with Nintendo.\n\n"
        "IMPORTANT LEGAL NOTICE:\n"
        "You must legally own a copy of Super Mario Bros to use this virtualizer.\n"
        "This software is intended for fair use and educational purposes only.\n"
        "Game data is embedded within this software for convenience.\n\n"
        "Special thanks to Nintendo for creating such an iconic and beloved game!"
    );
    
    gtk_about_dialog_set_license_type(GTK_ABOUT_DIALOG(dialog), GTK_LICENSE_MIT_X11);
    
    // Updated copyright to include Nintendo acknowledgment
    gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(dialog), 
        "Copyright © 2024 Jason Hall (Virtualizer)\n"
        "Super Mario Bros © 1985 Nintendo Co., Ltd."
    );
    
    const char* authors[] = {
        "Jason Hall (Virtualizer Developer)", 
        "",
        "Original Game:",
        "Shigeru Miyamoto (Game Designer)",
        "Nintendo Co., Ltd. (Publisher)",
        NULL
    };
    gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG(dialog), authors);
    
    // Add website link (optional)
    gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(dialog), "https://www.nintendo.com");
    gtk_about_dialog_set_website_label(GTK_ABOUT_DIALOG(dialog), "Nintendo Official Website");
    
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(window));
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    
    updateStatusBar("Ready");
}

void GTKMainWindow::showControlSettings() 
{
    updateStatusBar("Opening control settings...");
    
    // Create the dialog
    GtkWidget* dialog = gtk_dialog_new_with_buttons(
        "Controller Settings",
        GTK_WINDOW(window),
        GTK_DIALOG_MODAL,
        "Cancel", GTK_RESPONSE_CANCEL,
        "Apply", GTK_RESPONSE_APPLY,
        "OK", GTK_RESPONSE_OK,
        NULL
    );
    
    gtk_window_set_default_size(GTK_WINDOW(dialog), 600, 500);
    
    // Get the content area
    GtkWidget* content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    
    // Create notebook for tabs
    GtkWidget* notebook = gtk_notebook_new();
    gtk_container_add(GTK_CONTAINER(content_area), notebook);
    
    // Create Player 1 tab
    GtkWidget* player1_tab = createPlayerControlTab(PLAYER_1);
    GtkWidget* player1_label = gtk_label_new("Player 1");
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), player1_tab, player1_label);
    
    // Create Player 2 tab
    GtkWidget* player2_tab = createPlayerControlTab(PLAYER_2);
    GtkWidget* player2_label = gtk_label_new("Player 2");
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), player2_tab, player2_label);
    
    // Create General Settings tab
    GtkWidget* general_tab = createGeneralControlTab();
    GtkWidget* general_label = gtk_label_new("General");
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), general_tab, general_label);
    
    gtk_widget_show_all(dialog);
    
    // Run the dialog
    gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    
    if (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY) {
        // Save the settings
        saveControllerSettings();
        
        if (smbEngine) {
            Controller& controller = smbEngine->getController1();
            
            // First, shutdown any existing joystick connections
            controller.shutdownJoystick();
            
            // Reload configuration from the saved values
            controller.loadConfiguration();
            
            // Reinitialize joystick system with new configuration
            bool joystickInitialized = controller.initJoystick();
            
            if (joystickInitialized) {
                std::cout << "Controller system reinitialized successfully!" << std::endl;
                if (controller.isJoystickConnected(PLAYER_1))
                    std::cout << "Player 1 joystick reconnected" << std::endl;
                if (controller.isJoystickConnected(PLAYER_2))
                    std::cout << "Player 2 joystick reconnected" << std::endl;
                    
                // Apply the new polling setting
                controller.setJoystickPolling(Configuration::getJoystickPollingEnabled());
                std::cout << "Joystick polling: " << (Configuration::getJoystickPollingEnabled() ? "enabled" : "disabled") << std::endl;
                
                updateStatusBar("Controller settings saved and applied - controllers reinitialized");
            } else {
                std::cout << "No joysticks found after reinitialization." << std::endl;
                updateStatusBar("Controller settings saved - no joysticks detected");
            }
        } else {
            updateStatusBar("Controller settings saved");
        }
        
        // Save configuration to file
        Configuration::save();
    }
    
    if (response == GTK_RESPONSE_APPLY) {
        // Don't close dialog on Apply, just save
        gtk_widget_destroy(dialog);
        showControlSettings(); // Reopen the dialog
        return;
    }
    
    gtk_widget_destroy(dialog);
    updateStatusBar("Ready");
}

GtkWidget* GTKMainWindow::createPlayerControlTab(Player player)
{
    GtkWidget* notebook = gtk_notebook_new();
    
    // Create keyboard tab
    GtkWidget* keyboard_tab = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(keyboard_tab), 10);
    
    const char* playerName = (player == PLAYER_1) ? "Player 1" : "Player 2";
    
    // Player label for keyboard tab
    GtkWidget* kb_player_label = gtk_label_new(NULL);
    gchar* kb_markup = g_markup_printf_escaped("<b>%s Keyboard Controls</b>", playerName);
    gtk_label_set_markup(GTK_LABEL(kb_player_label), kb_markup);
    g_free(kb_markup);
    gtk_box_pack_start(GTK_BOX(keyboard_tab), kb_player_label, FALSE, FALSE, 0);
    
    // Create keyboard section
    GtkWidget* keyboard_grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(keyboard_grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(keyboard_grid), 10);
    gtk_container_set_border_width(GTK_CONTAINER(keyboard_grid), 10);
    
    // Store widgets for later access
    std::string prefix = (player == PLAYER_1) ? "p1_" : "p2_";
    
    // Create keyboard input fields
    createKeyInputRow(keyboard_grid, 0, "Up:", prefix + "key_up");
    createKeyInputRow(keyboard_grid, 1, "Down:", prefix + "key_down");
    createKeyInputRow(keyboard_grid, 2, "Left:", prefix + "key_left");
    createKeyInputRow(keyboard_grid, 3, "Right:", prefix + "key_right");
    createKeyInputRow(keyboard_grid, 4, "A Button:", prefix + "key_a");
    createKeyInputRow(keyboard_grid, 5, "B Button:", prefix + "key_b");
    createKeyInputRow(keyboard_grid, 6, "Select:", prefix + "key_select");
    createKeyInputRow(keyboard_grid, 7, "Start:", prefix + "key_start");
    
    gtk_box_pack_start(GTK_BOX(keyboard_tab), keyboard_grid, FALSE, FALSE, 0);
    
    // Create joystick tab
    GtkWidget* joystick_tab = createJoystickConfigTab(player);
    
    // Add tabs to notebook
    GtkWidget* kb_label = gtk_label_new("Keyboard");
    GtkWidget* joy_label = gtk_label_new("Joystick/Gamepad");
    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), keyboard_tab, kb_label);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), joystick_tab, joy_label);
    
    // Load current values
    loadPlayerControlValues(player);
    
    return notebook;
}

GtkWidget* GTKMainWindow::createJoystickConfigTab(Player player)
{
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);
    
    const char* playerName = (player == PLAYER_1) ? "Player 1" : "Player 2";
    std::string prefix = (player == PLAYER_1) ? "p1_" : "p2_";
    
    // Player label
    GtkWidget* player_label = gtk_label_new(NULL);
    gchar* markup = g_markup_printf_escaped("<b>%s Joystick/Gamepad Configuration</b>", playerName);
    gtk_label_set_markup(GTK_LABEL(player_label), markup);
    g_free(markup);
    gtk_box_pack_start(GTK_BOX(vbox), player_label, FALSE, FALSE, 0);
    
    // Movement section
    GtkWidget* movement_frame = gtk_frame_new("Movement Controls");
    GtkWidget* movement_grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(movement_grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(movement_grid), 10);
    gtk_container_set_border_width(GTK_CONTAINER(movement_grid), 10);
    gtk_container_add(GTK_CONTAINER(movement_frame), movement_grid);
    
    // Movement type selection
    GtkWidget* movement_type_label = gtk_label_new("Movement Type:");
    GtkWidget* movement_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(movement_combo), "Analog Stick (Axis)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(movement_combo), "D-Pad (Hat)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(movement_combo), "Buttons");
    gtk_combo_box_set_active(GTK_COMBO_BOX(movement_combo), 0);
    controlWidgets[prefix + "movement_type"] = movement_combo;
    
    gtk_grid_attach(GTK_GRID(movement_grid), movement_type_label, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(movement_grid), movement_combo, 1, 0, 2, 1);
    
    // Analog stick configuration
    createJoystickAxisRow(movement_grid, 1, "X-Axis (Left/Right):", prefix + "axis_x", prefix + "axis_x_dir");
    createJoystickAxisRow(movement_grid, 2, "Y-Axis (Up/Down):", prefix + "axis_y", prefix + "axis_y_dir");
    
    // D-Pad configuration
    GtkWidget* hat_label = gtk_label_new("D-Pad/Hat Number:");
    GtkWidget* hat_spin = gtk_spin_button_new_with_range(0, 3, 1);
    controlWidgets[prefix + "hat_number"] = hat_spin;
    gtk_grid_attach(GTK_GRID(movement_grid), hat_label, 0, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(movement_grid), hat_spin, 1, 3, 1, 1);
    
    // Movement buttons (as alternative)
    createJoystickButtonRow(movement_grid, 4, "Up Button:", prefix + "btn_up");
    createJoystickButtonRow(movement_grid, 5, "Down Button:", prefix + "btn_down");
    createJoystickButtonRow(movement_grid, 6, "Left Button:", prefix + "btn_left");
    createJoystickButtonRow(movement_grid, 7, "Right Button:", prefix + "btn_right");
    
    gtk_box_pack_start(GTK_BOX(vbox), movement_frame, FALSE, FALSE, 0);
    
    // Action buttons section
    GtkWidget* buttons_frame = gtk_frame_new("Action Buttons");
    GtkWidget* buttons_grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(buttons_grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(buttons_grid), 10);
    gtk_container_set_border_width(GTK_CONTAINER(buttons_grid), 10);
    gtk_container_add(GTK_CONTAINER(buttons_frame), buttons_grid);
    
    createJoystickButtonRow(buttons_grid, 0, "A Button:", prefix + "joy_a");
    createJoystickButtonRow(buttons_grid, 1, "B Button:", prefix + "joy_b");
    createJoystickButtonRow(buttons_grid, 2, "Start Button:", prefix + "joy_start");
    createJoystickButtonRow(buttons_grid, 3, "Select Button:", prefix + "joy_select");
    
    gtk_box_pack_start(GTK_BOX(vbox), buttons_frame, FALSE, FALSE, 0);
    
    // Deadzone configuration
    GtkWidget* deadzone_frame = gtk_frame_new("Analog Stick Settings");
    GtkWidget* deadzone_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(deadzone_vbox), 10);
    gtk_container_add(GTK_CONTAINER(deadzone_frame), deadzone_vbox);
    
    GtkWidget* deadzone_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget* deadzone_label = gtk_label_new("Deadzone:");
    GtkWidget* deadzone_spin = gtk_spin_button_new_with_range(1000, 20000, 500);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(deadzone_spin), 8000);
    controlWidgets[prefix + "deadzone"] = deadzone_spin;
    
    gtk_box_pack_start(GTK_BOX(deadzone_hbox), deadzone_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(deadzone_hbox), deadzone_spin, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(deadzone_vbox), deadzone_hbox, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(vbox), deadzone_frame, FALSE, FALSE, 0);
    
    // Test section
    GtkWidget* test_frame = gtk_frame_new("Controller Test");
    GtkWidget* test_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(test_vbox), 10);
    gtk_container_add(GTK_CONTAINER(test_frame), test_vbox);
    
    GtkWidget* test_button = gtk_button_new_with_label("Test Controller Input");
    GtkWidget* test_label = gtk_label_new("Press 'Test Controller Input' and use your controller");
    controlWidgets[prefix + "test_label"] = test_label;
    
    gtk_box_pack_start(GTK_BOX(test_vbox), test_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(test_vbox), test_label, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(vbox), test_frame, FALSE, FALSE, 0);
    
    return vbox;
}

// Add these new methods to GTKMainWindow.cpp:

void GTKMainWindow::createJoystickButtonRow(GtkWidget* grid, int row, const char* label, const std::string& key)
{
    GtkWidget* label_widget = gtk_label_new(label);
    gtk_widget_set_halign(label_widget, GTK_ALIGN_START);
    
    GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    
    GtkWidget* spin = gtk_spin_button_new_with_range(-1, 31, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), -1); // -1 = not assigned
    gtk_widget_set_size_request(spin, 60, -1);
    
    GtkWidget* detect_button = gtk_button_new_with_label("Detect");
    gtk_widget_set_size_request(detect_button, 80, -1);
    
    // Store the key as button data
    g_object_set_data_full(G_OBJECT(detect_button), "config_key", 
                          g_strdup(key.c_str()), g_free);
    g_object_set_data(G_OBJECT(detect_button), "spin_button", spin);
    g_object_set_data(G_OBJECT(detect_button), "window", this);
    
    g_signal_connect(detect_button, "clicked", G_CALLBACK(onJoystickDetectButton), this);
    
    gtk_box_pack_start(GTK_BOX(hbox), spin, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), detect_button, FALSE, FALSE, 0);
    
    // Store widget for later access
    controlWidgets[key] = spin;
    controlWidgets[key + "_detect"] = detect_button;
    
    gtk_grid_attach(GTK_GRID(grid), label_widget, 0, row, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), hbox, 1, row, 2, 1);
}

void GTKMainWindow::createJoystickAxisRow(GtkWidget* grid, int row, const char* label, 
                                         const std::string& axis_key, const std::string& direction_key)
{
    GtkWidget* label_widget = gtk_label_new(label);
    gtk_widget_set_halign(label_widget, GTK_ALIGN_START);
    
    GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    
    // Axis number
    GtkWidget* axis_spin = gtk_spin_button_new_with_range(-1, 5, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(axis_spin), -1); // -1 = not assigned
    gtk_widget_set_size_request(axis_spin, 60, -1);
    
    // Direction
    GtkWidget* dir_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(dir_combo), "Normal");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(dir_combo), "Inverted");
    gtk_combo_box_set_active(GTK_COMBO_BOX(dir_combo), 0);
    gtk_widget_set_size_request(dir_combo, 80, -1);
    
    GtkWidget* detect_button = gtk_button_new_with_label("Detect");
    gtk_widget_set_size_request(detect_button, 80, -1);
    
    // Store references
    g_object_set_data_full(G_OBJECT(detect_button), "config_key", 
                          g_strdup(axis_key.c_str()), g_free);
    g_object_set_data(G_OBJECT(detect_button), "axis_spin", axis_spin);
    g_object_set_data(G_OBJECT(detect_button), "dir_combo", dir_combo);
    g_object_set_data(G_OBJECT(detect_button), "window", this);
    
    g_signal_connect(detect_button, "clicked", G_CALLBACK(onJoystickDetectAxis), this);
    
    gtk_box_pack_start(GTK_BOX(hbox), axis_spin, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), dir_combo, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), detect_button, FALSE, FALSE, 0);
    
    // Store widgets
    controlWidgets[axis_key] = axis_spin;
    controlWidgets[direction_key] = dir_combo;
    controlWidgets[axis_key + "_detect"] = detect_button;
    
    gtk_grid_attach(GTK_GRID(grid), label_widget, 0, row, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), hbox, 1, row, 2, 1);
}

// Add these callback methods:

void GTKMainWindow::onJoystickDetectButton(GtkButton* button, gpointer user_data)
{
    GTKMainWindow* window = static_cast<GTKMainWindow*>(user_data);
    
    const char* config_key = static_cast<const char*>(g_object_get_data(G_OBJECT(button), "config_key"));
    GtkWidget* spin_button = static_cast<GtkWidget*>(g_object_get_data(G_OBJECT(button), "spin_button"));
    
    gtk_button_set_label(button, "Press Button...");
    gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);
    
    window->startJoystickCapture(config_key, false);
    
    // Set timer to timeout after 5 seconds
    g_timeout_add(5000, [](gpointer data) -> gboolean {
        GTKMainWindow* win = static_cast<GTKMainWindow*>(data);
        win->stopJoystickCapture();
        return G_SOURCE_REMOVE;
    }, window);
}

void GTKMainWindow::onJoystickDetectAxis(GtkButton* button, gpointer user_data)
{
    GTKMainWindow* window = static_cast<GTKMainWindow*>(user_data);
    
    const char* config_key = static_cast<const char*>(g_object_get_data(G_OBJECT(button), "config_key"));
    
    gtk_button_set_label(button, "Move Stick...");
    gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);
    
    window->startJoystickCapture(config_key, true);
    
    // Set timer to timeout after 5 seconds
    g_timeout_add(5000, [](gpointer data) -> gboolean {
        GTKMainWindow* win = static_cast<GTKMainWindow*>(data);
        win->stopJoystickCapture();
        return G_SOURCE_REMOVE;
    }, window);
}

void GTKMainWindow::startJoystickCapture(const std::string& widgetKey, bool isAxis)
{
    isCapturingJoystick = true;
    currentCaptureWidget = widgetKey;
    currentCaptureIsAxis = isAxis;
}

void GTKMainWindow::stopJoystickCapture()
{
    if (!isCapturingJoystick) return;
    
    std::cout << "Stopping joystick capture for: " << currentCaptureWidget << std::endl;
    
    isCapturingJoystick = false;
    
    // Reset button labels and re-enable buttons
    std::string detect_key = currentCaptureWidget + "_detect";
    if (controlWidgets.find(detect_key) != controlWidgets.end()) {
        GtkWidget* button = controlWidgets[detect_key];
        if (button) {
            gtk_button_set_label(GTK_BUTTON(button), "Detect");
            gtk_widget_set_sensitive(button, TRUE);
        }
    }
    
    currentCaptureWidget.clear();
    currentCaptureIsAxis = false;
}

void GTKMainWindow::testJoystickInput()
{
    if (!smbEngine) {
        std::cout << "Engine not initialized" << std::endl;
        return;
    }
    
    Controller& controller = smbEngine->getController1();
    
    std::cout << "=== Joystick Test ===" << std::endl;
    std::cout << "Total joysticks: " << SDL_NumJoysticks() << std::endl;
    
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        SDL_Joystick* joy = SDL_JoystickOpen(i);
        if (joy) {
            std::cout << "Joystick " << i << ": " << SDL_JoystickName(joy) << std::endl;
            std::cout << "  Axes: " << SDL_JoystickNumAxes(joy) << std::endl;
            std::cout << "  Buttons: " << SDL_JoystickNumButtons(joy) << std::endl;
            std::cout << "  Hats: " << SDL_JoystickNumHats(joy) << std::endl;
            SDL_JoystickClose(joy);
        }
    }
    
    std::cout << "Player 1 connected: " << (controller.isJoystickConnected(PLAYER_1) ? "Yes" : "No") << std::endl;
    std::cout << "Player 2 connected: " << (controller.isJoystickConnected(PLAYER_2) ? "Yes" : "No") << std::endl;
}

GtkWidget* GTKMainWindow::createGeneralControlTab()
{
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);
    
    // General settings label
    GtkWidget* general_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(general_label), "<b>General Controller Settings</b>");
    gtk_box_pack_start(GTK_BOX(vbox), general_label, FALSE, FALSE, 0);
    
    // Joystick polling checkbox
    GtkWidget* polling_check = gtk_check_button_new_with_label("Enable joystick polling (compatibility mode)");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(polling_check), Configuration::getJoystickPollingEnabled());
    controlWidgets["joystick_polling"] = polling_check;
    gtk_box_pack_start(GTK_BOX(vbox), polling_check, FALSE, FALSE, 0);
    
    // Deadzone setting
    GtkWidget* deadzone_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget* deadzone_label = gtk_label_new("Joystick Deadzone:");
    GtkWidget* deadzone_spin = gtk_spin_button_new_with_range(1000, 20000, 500);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(deadzone_spin), Configuration::getJoystickDeadzone());
    controlWidgets["joystick_deadzone"] = deadzone_spin;
    gtk_box_pack_start(GTK_BOX(deadzone_hbox), deadzone_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(deadzone_hbox), deadzone_spin, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), deadzone_hbox, FALSE, FALSE, 0);
    
    // Info label
    GtkWidget* info_label = gtk_label_new(
        "Joystick polling mode is for older controllers that don't send proper events.\n"
        "Modern gamepads should work fine with polling disabled.\n\n"
        "Deadzone controls how far the analog stick must be moved before registering input.\n"
        "Higher values = less sensitive, Lower values = more sensitive."
    );
    gtk_label_set_line_wrap(GTK_LABEL(info_label), TRUE);
    gtk_widget_set_margin_top(info_label, 20);
    gtk_box_pack_start(GTK_BOX(vbox), info_label, FALSE, FALSE, 0);
    
    // Controller detection section
    GtkWidget* detection_frame = gtk_frame_new("Connected Controllers");
    GtkWidget* detection_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(detection_vbox), 10);
    gtk_container_add(GTK_CONTAINER(detection_frame), detection_vbox);
    
    // Refresh button
    GtkWidget* refresh_button = gtk_button_new_with_label("Refresh Controller List");
    g_signal_connect(refresh_button, "clicked", G_CALLBACK(onRefreshControllers), this);
    gtk_box_pack_start(GTK_BOX(detection_vbox), refresh_button, FALSE, FALSE, 0);
    
    // Controller list
    GtkWidget* controller_list = gtk_label_new("Click 'Refresh Controller List' to detect controllers");
    controlWidgets["controller_list"] = controller_list;
    gtk_label_set_line_wrap(GTK_LABEL(controller_list), TRUE);
    gtk_box_pack_start(GTK_BOX(detection_vbox), controller_list, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(vbox), detection_frame, TRUE, TRUE, 0);
    
    return vbox;
}

void GTKMainWindow::createKeyInputRow(GtkWidget* grid, int row, const char* label, const std::string& key)
{
    GtkWidget* label_widget = gtk_label_new(label);
    gtk_widget_set_halign(label_widget, GTK_ALIGN_START);
    
    GtkWidget* entry = gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(entry), 15);
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Click and press key");
    
    // Make entry non-editable but focusable for key capture
    gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
    g_signal_connect(entry, "key-press-event", G_CALLBACK(onKeyCapture), g_strdup(key.c_str()));
    
    // Store widget for later access
    controlWidgets[key] = entry;
    
    gtk_grid_attach(GTK_GRID(grid), label_widget, 0, row, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), entry, 1, row, 1, 1);
}

void GTKMainWindow::createButtonInputRow(GtkWidget* grid, int row, const char* label, const std::string& key)
{
    GtkWidget* label_widget = gtk_label_new(label);
    gtk_widget_set_halign(label_widget, GTK_ALIGN_START);
    
    GtkWidget* spin = gtk_spin_button_new_with_range(0, 31, 1);
    
    // Store widget for later access
    controlWidgets[key] = spin;
    
    gtk_grid_attach(GTK_GRID(grid), label_widget, 0, row, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), spin, 1, row, 1, 1);
}

void GTKMainWindow::loadPlayerControlValues(Player player)
{
    std::string prefix = (player == PLAYER_1) ? "p1_" : "p2_";
    
    // Load keyboard values
    if (player == PLAYER_1) {
        setKeyWidgetValue(prefix + "key_up", Configuration::getPlayer1KeyUp());
        setKeyWidgetValue(prefix + "key_down", Configuration::getPlayer1KeyDown());
        setKeyWidgetValue(prefix + "key_left", Configuration::getPlayer1KeyLeft());
        setKeyWidgetValue(prefix + "key_right", Configuration::getPlayer1KeyRight());
        setKeyWidgetValue(prefix + "key_a", Configuration::getPlayer1KeyA());
        setKeyWidgetValue(prefix + "key_b", Configuration::getPlayer1KeyB());
        setKeyWidgetValue(prefix + "key_select", Configuration::getPlayer1KeySelect());
        setKeyWidgetValue(prefix + "key_start", Configuration::getPlayer1KeyStart());
        
        // Load joystick values
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(controlWidgets[prefix + "joy_a"]), Configuration::getPlayer1JoystickButtonA());
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(controlWidgets[prefix + "joy_b"]), Configuration::getPlayer1JoystickButtonB());
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(controlWidgets[prefix + "joy_start"]), Configuration::getPlayer1JoystickButtonStart());
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(controlWidgets[prefix + "joy_select"]), Configuration::getPlayer1JoystickButtonSelect());
    } else {
        setKeyWidgetValue(prefix + "key_up", Configuration::getPlayer2KeyUp());
        setKeyWidgetValue(prefix + "key_down", Configuration::getPlayer2KeyDown());
        setKeyWidgetValue(prefix + "key_left", Configuration::getPlayer2KeyLeft());
        setKeyWidgetValue(prefix + "key_right", Configuration::getPlayer2KeyRight());
        setKeyWidgetValue(prefix + "key_a", Configuration::getPlayer2KeyA());
        setKeyWidgetValue(prefix + "key_b", Configuration::getPlayer2KeyB());
        setKeyWidgetValue(prefix + "key_select", Configuration::getPlayer2KeySelect());
        setKeyWidgetValue(prefix + "key_start", Configuration::getPlayer2KeyStart());
        
        // Load joystick values
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(controlWidgets[prefix + "joy_a"]), Configuration::getPlayer2JoystickButtonA());
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(controlWidgets[prefix + "joy_b"]), Configuration::getPlayer2JoystickButtonB());
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(controlWidgets[prefix + "joy_start"]), Configuration::getPlayer2JoystickButtonStart());
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(controlWidgets[prefix + "joy_select"]), Configuration::getPlayer2JoystickButtonSelect());
    }
}

void GTKMainWindow::setKeyWidgetValue(const std::string& key, int scancode)
{
    if (controlWidgets.find(key) != controlWidgets.end()) {
        const char* keyName = SDL_GetScancodeName(static_cast<SDL_Scancode>(scancode));
        gtk_entry_set_text(GTK_ENTRY(controlWidgets[key]), keyName);
        
        // Store the scancode as widget data
        g_object_set_data(G_OBJECT(controlWidgets[key]), "scancode", GINT_TO_POINTER(scancode));
    }
}

void GTKMainWindow::saveControllerSettings()
{
    // Save Player 1 keyboard settings
    Configuration::setPlayer1KeyUp(getKeyScancode("p1_key_up"));
    Configuration::setPlayer1KeyDown(getKeyScancode("p1_key_down"));
    Configuration::setPlayer1KeyLeft(getKeyScancode("p1_key_left"));
    Configuration::setPlayer1KeyRight(getKeyScancode("p1_key_right"));
    Configuration::setPlayer1KeyA(getKeyScancode("p1_key_a"));
    Configuration::setPlayer1KeyB(getKeyScancode("p1_key_b"));
    Configuration::setPlayer1KeySelect(getKeyScancode("p1_key_select"));
    Configuration::setPlayer1KeyStart(getKeyScancode("p1_key_start"));
    
    // Save Player 1 joystick settings
    Configuration::setPlayer1JoystickButtonA(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(controlWidgets["p1_joy_a"])));
    Configuration::setPlayer1JoystickButtonB(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(controlWidgets["p1_joy_b"])));
    Configuration::setPlayer1JoystickButtonStart(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(controlWidgets["p1_joy_start"])));
    Configuration::setPlayer1JoystickButtonSelect(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(controlWidgets["p1_joy_select"])));
    
    // Save Player 2 keyboard settings
    Configuration::setPlayer2KeyUp(getKeyScancode("p2_key_up"));
    Configuration::setPlayer2KeyDown(getKeyScancode("p2_key_down"));
    Configuration::setPlayer2KeyLeft(getKeyScancode("p2_key_left"));
    Configuration::setPlayer2KeyRight(getKeyScancode("p2_key_right"));
    Configuration::setPlayer2KeyA(getKeyScancode("p2_key_a"));
    Configuration::setPlayer2KeyB(getKeyScancode("p2_key_b"));
    Configuration::setPlayer2KeySelect(getKeyScancode("p2_key_select"));
    Configuration::setPlayer2KeyStart(getKeyScancode("p2_key_start"));
    
    // Save Player 2 joystick settings
    Configuration::setPlayer2JoystickButtonA(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(controlWidgets["p2_joy_a"])));
    Configuration::setPlayer2JoystickButtonB(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(controlWidgets["p2_joy_b"])));
    Configuration::setPlayer2JoystickButtonStart(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(controlWidgets["p2_joy_start"])));
    Configuration::setPlayer2JoystickButtonSelect(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(controlWidgets["p2_joy_select"])));
    
    // Save general settings
    Configuration::setJoystickPollingEnabled(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controlWidgets["joystick_polling"])));
    Configuration::setJoystickDeadzone(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(controlWidgets["joystick_deadzone"])));
}

int GTKMainWindow::getKeyScancode(const std::string& key)
{
    if (controlWidgets.find(key) != controlWidgets.end()) {
        return GPOINTER_TO_INT(g_object_get_data(G_OBJECT(controlWidgets[key]), "scancode"));
    }
    return 0;
}

// Static callback functions
gboolean GTKMainWindow::onKeyCapture(GtkWidget* widget, GdkEventKey* event, gpointer user_data)
{
    char* key = static_cast<char*>(user_data);
    
    // Convert GDK key to SDL scancode using the helper function
    GTKMainWindow* window = static_cast<GTKMainWindow*>(g_object_get_data(G_OBJECT(widget), "window"));
    if (!window) {
        // Create a temporary instance to call the helper function
        GTKMainWindow temp;
        SDL_Scancode scancode = temp.gdk_keyval_to_sdl_scancode(event->keyval);
        
        if (scancode != SDL_SCANCODE_UNKNOWN) {
            const char* keyName = SDL_GetScancodeName(scancode);
            gtk_entry_set_text(GTK_ENTRY(widget), keyName);
            
            // Store the scancode as widget data
            g_object_set_data(G_OBJECT(widget), "scancode", GINT_TO_POINTER(scancode));
        }
    }
    
    return TRUE; // Consume the event
}

void GTKMainWindow::onRefreshControllers(GtkButton* button, gpointer user_data)
{
    GTKMainWindow* window = static_cast<GTKMainWindow*>(user_data);
    
    if (smbEngine) {
        Controller& controller = smbEngine->getController1();
        
        // Reinitialize joysticks
        bool found = controller.initJoystick();
        
        std::string info = "Connected Controllers:\n";
        
        if (found) {
            if (controller.isJoystickConnected(PLAYER_1)) {
                info += "• Player 1: Connected\n";
            }
            if (controller.isJoystickConnected(PLAYER_2)) {
                info += "• Player 2: Connected\n";
            }
        } else {
            info += "No controllers detected.\n";
        }
        
        info += "\nNote: Controllers may need to be reconnected\nafter changing settings.";
        
        if (window->controlWidgets.find("controller_list") != window->controlWidgets.end()) {
            gtk_label_set_text(GTK_LABEL(window->controlWidgets["controller_list"]), info.c_str());
        }
    }
}

// Helper function to convert GDK keyval to SDL scancode
SDL_Scancode GTKMainWindow::gdk_keyval_to_sdl_scancode(guint keyval)
{
    switch (keyval) {
        case GDK_KEY_Up: return SDL_SCANCODE_UP;
        case GDK_KEY_Down: return SDL_SCANCODE_DOWN;
        case GDK_KEY_Left: return SDL_SCANCODE_LEFT;
        case GDK_KEY_Right: return SDL_SCANCODE_RIGHT;
        case GDK_KEY_space: return SDL_SCANCODE_SPACE;
        case GDK_KEY_Return: return SDL_SCANCODE_RETURN;
        case GDK_KEY_Escape: return SDL_SCANCODE_ESCAPE;
        case GDK_KEY_Tab: return SDL_SCANCODE_TAB;
        case GDK_KEY_Shift_L: return SDL_SCANCODE_LSHIFT;
        case GDK_KEY_Shift_R: return SDL_SCANCODE_RSHIFT;
        case GDK_KEY_Control_L: return SDL_SCANCODE_LCTRL;
        case GDK_KEY_Control_R: return SDL_SCANCODE_RCTRL;
        case GDK_KEY_Alt_L: return SDL_SCANCODE_LALT;
        case GDK_KEY_Alt_R: return SDL_SCANCODE_RALT;
        case GDK_KEY_BackSpace: return SDL_SCANCODE_BACKSPACE;
        case GDK_KEY_Delete: return SDL_SCANCODE_DELETE;
        case GDK_KEY_Home: return SDL_SCANCODE_HOME;
        case GDK_KEY_End: return SDL_SCANCODE_END;
        case GDK_KEY_Page_Up: return SDL_SCANCODE_PAGEUP;
        case GDK_KEY_Page_Down: return SDL_SCANCODE_PAGEDOWN;
        case GDK_KEY_Insert: return SDL_SCANCODE_INSERT;
        
        // Function keys
        case GDK_KEY_F1: return SDL_SCANCODE_F1;
        case GDK_KEY_F2: return SDL_SCANCODE_F2;
        case GDK_KEY_F3: return SDL_SCANCODE_F3;
        case GDK_KEY_F4: return SDL_SCANCODE_F4;
        case GDK_KEY_F5: return SDL_SCANCODE_F5;
        case GDK_KEY_F6: return SDL_SCANCODE_F6;
        case GDK_KEY_F7: return SDL_SCANCODE_F7;
        case GDK_KEY_F8: return SDL_SCANCODE_F8;
        case GDK_KEY_F9: return SDL_SCANCODE_F9;
        case GDK_KEY_F10: return SDL_SCANCODE_F10;
        case GDK_KEY_F11: return SDL_SCANCODE_F11;
        case GDK_KEY_F12: return SDL_SCANCODE_F12;
        
        default:
            // For letter keys
            if (keyval >= GDK_KEY_a && keyval <= GDK_KEY_z) {
                return static_cast<SDL_Scancode>(SDL_SCANCODE_A + (keyval - GDK_KEY_a));
            }
            if (keyval >= GDK_KEY_A && keyval <= GDK_KEY_Z) {
                return static_cast<SDL_Scancode>(SDL_SCANCODE_A + (keyval - GDK_KEY_A));
            }
            // For number keys
            if (keyval >= GDK_KEY_0 && keyval <= GDK_KEY_9) {
                return static_cast<SDL_Scancode>(SDL_SCANCODE_0 + (keyval - GDK_KEY_0));
            }
            // Special characters
            switch (keyval) {
                case GDK_KEY_minus: return SDL_SCANCODE_MINUS;
                case GDK_KEY_equal: return SDL_SCANCODE_EQUALS;
                case GDK_KEY_bracketleft: return SDL_SCANCODE_LEFTBRACKET;
                case GDK_KEY_bracketright: return SDL_SCANCODE_RIGHTBRACKET;
                case GDK_KEY_backslash: return SDL_SCANCODE_BACKSLASH;
                case GDK_KEY_semicolon: return SDL_SCANCODE_SEMICOLON;
                case GDK_KEY_apostrophe: return SDL_SCANCODE_APOSTROPHE;
                case GDK_KEY_grave: return SDL_SCANCODE_GRAVE;
                case GDK_KEY_comma: return SDL_SCANCODE_COMMA;
                case GDK_KEY_period: return SDL_SCANCODE_PERIOD;
                case GDK_KEY_slash: return SDL_SCANCODE_SLASH;
            }
            return SDL_SCANCODE_UNKNOWN;
    }
}

// Main function to use GTK version
int main(int argc, char** argv) 
{
    // Initialize configuration
    Configuration::initialize(CONFIG_FILE_NAME);
    
    GTKMainWindow mainWindow;
    
    if (!mainWindow.initialize()) {
        g_print("Failed to initialize GTK main window\n");
        return -1;
    }
    
    mainWindow.run();
    
    return 0;
}
