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
      gameRunning(false), gamePaused(false)
{
}

GTKMainWindow::~GTKMainWindow() 
{
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
    
    // Settings menu
    GtkWidget* settingsMenu = gtk_menu_new();
    GtkWidget* settingsMenuItem = gtk_menu_item_new_with_label("Settings");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(settingsMenuItem), settingsMenu);
    
    GtkWidget* videoItem = gtk_menu_item_new_with_label("Video");
    g_signal_connect(videoItem, "activate", G_CALLBACK(onSettingsVideo), this);
    gtk_menu_shell_append(GTK_MENU_SHELL(settingsMenu), videoItem);
    
    GtkWidget* audioItem = gtk_menu_item_new_with_label("Audio");
    g_signal_connect(audioItem, "activate", G_CALLBACK(onSettingsAudio), this);
    gtk_menu_shell_append(GTK_MENU_SHELL(settingsMenu), audioItem);
    
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
    static bool isFullscreen = false;
    
    if (!isFullscreen) {
        // Hide the menu bar and status bar
        gtk_widget_hide(menubar);
        gtk_widget_hide(statusbar);
        
        // Go fullscreen
        gtk_window_fullscreen(GTK_WINDOW(window));
        isFullscreen = true;
    } else {
        exitFullscreen();
    }
}

void GTKMainWindow::exitFullscreen() 
{
    // Show the menu bar and status bar
    gtk_widget_show(menubar);
    gtk_widget_show(statusbar);
    
    // Exit fullscreen
    gtk_window_unfullscreen(GTK_WINDOW(window));
}

// GTK drawing callback - renders the game frame
gboolean GTKMainWindow::onGameDraw(GtkWidget* widget, cairo_t* cr, gpointer user_data) 
{
    if (!currentFrameBuffer) {
        // Draw a black rectangle if no frame is available yet
        cairo_set_source_rgb(cr, 0, 0, 0);
        cairo_paint(cr);
        return TRUE;
    }
    
    // Get widget dimensions
    int widget_width = gtk_widget_get_allocated_width(widget);
    int widget_height = gtk_widget_get_allocated_height(widget);
    
    // Create a proper RGB buffer - the game buffer is BGRA format
    guchar* rgb_data = g_new(guchar, RENDER_WIDTH * RENDER_HEIGHT * 4);
    for (int i = 0; i < RENDER_WIDTH * RENDER_HEIGHT; i++) {
        uint32_t pixel = currentFrameBuffer[i];
        // Convert BGRA to BGRA for Cairo (swap red and blue)
        rgb_data[i * 4 + 0] = pixel & 0xFF;         // Blue
        rgb_data[i * 4 + 1] = (pixel >> 8) & 0xFF;  // Green  
        rgb_data[i * 4 + 2] = (pixel >> 16) & 0xFF; // Red
        rgb_data[i * 4 + 3] = 255;                  // Alpha
    }
    
    // Create Cairo surface from the converted RGB data
    cairo_surface_t* surface = cairo_image_surface_create_for_data(
        rgb_data,
        CAIRO_FORMAT_ARGB32,
        RENDER_WIDTH,
        RENDER_HEIGHT,
        RENDER_WIDTH * 4
    );
    
    // Scale to fit the widget while maintaining aspect ratio
    double scale_x = (double)widget_width / RENDER_WIDTH;
    double scale_y = (double)widget_height / RENDER_HEIGHT;
    double scale = (scale_x < scale_y) ? scale_x : scale_y;
    
    // Center the image
    double offset_x = (widget_width - RENDER_WIDTH * scale) / 2;
    double offset_y = (widget_height - RENDER_HEIGHT * scale) / 2;
    
    cairo_save(cr);
    cairo_translate(cr, offset_x, offset_y);
    cairo_scale(cr, scale, scale);
    
    // Draw the surface
    cairo_set_source_surface(cr, surface, 0, 0);
    cairo_paint(cr);
    
    cairo_restore(cr);
    cairo_surface_destroy(surface);
    g_free(rgb_data);
    
    return TRUE;
}

// GTK keyboard input callbacks
gboolean GTKMainWindow::onKeyPress(GtkWidget* widget, GdkEventKey* event, gpointer user_data) 
{
    GTKMainWindow* window = static_cast<GTKMainWindow*>(user_data);
    
    if (smbEngine) {
        Controller& controller1 = smbEngine->getController1();
        
        // Handle special keys first
        if (event->keyval == GDK_KEY_r || event->keyval == GDK_KEY_R) {
            smbEngine->reset();
            window->updateStatusBar("Game reset");
            return TRUE;
        }
        
        // Handle fullscreen toggle (F key)
        if (event->keyval == GDK_KEY_f || event->keyval == GDK_KEY_F) {
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
    SDL_SetHint(SDL_HINT_AUDIODRIVER, "directsound,winmm");
    
    // Initialize SDL for joystick/gamecontroller only - we'll handle audio separately
    if (SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) < 0) {
        updateStatusBar("SDL initialization failed");
        return;
    }
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
    
#ifdef _WIN32
    // Try WinMM first on Windows
    if (Configuration::getAudioEnabled()) {
        windowsAudio = new WindowsAudio();
        if (windowsAudio->initialize(Configuration::getAudioFrequency(), &engine)) {
            audioInitialized = true;
        } else {
            delete windowsAudio;
            windowsAudio = nullptr;
        }
    }
    
    // If WinMM failed, fall back to SDL with safe drivers
    if (Configuration::getAudioEnabled() && !audioInitialized) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0) {
            SDL_AudioSpec desiredSpec;
            desiredSpec.freq = Configuration::getAudioFrequency();
            desiredSpec.format = AUDIO_S16;
            desiredSpec.channels = 1;
            desiredSpec.samples = 1024;
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
                std::cout << "SDL Audio initialized (fallback):" << std::endl;
                std::cout << "  Format: 16-bit" << std::endl;
                std::cout << "  Frequency: " << obtainedSpec.freq << " Hz" << std::endl;
                std::cout << "  Channels: " << (int)obtainedSpec.channels << std::endl;
                std::cout << "  Buffer size: " << obtainedSpec.samples << std::endl;
                SDL_PauseAudio(0);
            }
        }
    }
#else
    // Non-Windows SDL audio initialization
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
#endif

#ifdef _WIN32
    // Start WinMM audio if it was initialized
    if (windowsAudio) {
        windowsAudio->start();
    }
#endif

    updateStatusBar("Game started");

    // Main game loop
    int progStartTime = SDL_GetTicks();
    int frame = 0;
    
    while (gameRunning) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
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
            
            // Force immediate redraw on the main thread
            gdk_threads_add_idle([](gpointer data) -> gboolean {
                GTKMainWindow* window = static_cast<GTKMainWindow*>(data);
                gtk_widget_queue_draw(window->gameContainer);
                return G_SOURCE_REMOVE;
            }, this);
        }

        // Ensure that the framerate stays as close to the desired FPS as possible
        int now = SDL_GetTicks();
        int delay = progStartTime + int(double(frame) * double(MS_PER_SEC) / double(Configuration::getFrameRate())) - now;
        if (delay > 0) {
            SDL_Delay(delay);
        } else {
            frame = 0;
            progStartTime = now;
        }
        frame++;
    }

    // Cleanup
#ifdef _WIN32
    if (windowsAudio) {
        windowsAudio->shutdown();
        delete windowsAudio;
        windowsAudio = nullptr;
    }
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
    gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(dialog), 
        "A Super Mario Bros virtualizer built with SDL2 and GTK+");
    gtk_about_dialog_set_license_type(GTK_ABOUT_DIALOG(dialog), GTK_LICENSE_MIT_X11);
    
    const char* authors[] = {"Jason Hall", NULL};
    gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG(dialog), authors);
    
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
        
        // Reload controller configuration
        if (smbEngine) {
            Controller& controller = smbEngine->getController1();
            controller.loadConfiguration();
            updateStatusBar("Controller settings saved and applied");
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
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);
    
    const char* playerName = (player == PLAYER_1) ? "Player 1" : "Player 2";
    
    // Player label
    GtkWidget* player_label = gtk_label_new(NULL);
    gchar* markup = g_markup_printf_escaped("<b>%s Controls</b>", playerName);
    gtk_label_set_markup(GTK_LABEL(player_label), markup);
    g_free(markup);
    gtk_box_pack_start(GTK_BOX(vbox), player_label, FALSE, FALSE, 0);
    
    // Create keyboard section
    GtkWidget* keyboard_frame = gtk_frame_new("Keyboard Controls");
    GtkWidget* keyboard_grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(keyboard_grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(keyboard_grid), 10);
    gtk_container_set_border_width(GTK_CONTAINER(keyboard_grid), 10);
    gtk_container_add(GTK_CONTAINER(keyboard_frame), keyboard_grid);
    
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
    
    gtk_box_pack_start(GTK_BOX(vbox), keyboard_frame, FALSE, FALSE, 0);
    
    // Create joystick section
    GtkWidget* joystick_frame = gtk_frame_new("Joystick/Gamepad Controls");
    GtkWidget* joystick_grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(joystick_grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(joystick_grid), 10);
    gtk_container_set_border_width(GTK_CONTAINER(joystick_grid), 10);
    gtk_container_add(GTK_CONTAINER(joystick_frame), joystick_grid);
    
    // Create joystick button input fields
    createButtonInputRow(joystick_grid, 0, "A Button:", prefix + "joy_a");
    createButtonInputRow(joystick_grid, 1, "B Button:", prefix + "joy_b");
    createButtonInputRow(joystick_grid, 2, "Start:", prefix + "joy_start");
    createButtonInputRow(joystick_grid, 3, "Select:", prefix + "joy_select");
    
    gtk_box_pack_start(GTK_BOX(vbox), joystick_frame, FALSE, FALSE, 0);
    
    // Load current values
    loadPlayerControlValues(player);
    
    return vbox;
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
    // This is a simplified mapping - you may want to expand this
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
