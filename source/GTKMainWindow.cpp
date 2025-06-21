#include "Main.hpp"
#include "SMB/SMBEngine.hpp"
#include "Emulation/Controller.hpp"
#include "Configuration.hpp"
#include "Constants.hpp"
#include "Util/Video.hpp"
#include "Util/VideoFilters.hpp"
#include "SMBRom.hpp"

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
        
        // Handle game controls directly (Player 1)
        switch (event->keyval) {
            case GDK_KEY_Up:
                controller1.setButtonState(PLAYER_1, BUTTON_UP, true);
                break;
            case GDK_KEY_Down:
                controller1.setButtonState(PLAYER_1, BUTTON_DOWN, true);
                break;
            case GDK_KEY_Left:
                controller1.setButtonState(PLAYER_1, BUTTON_LEFT, true);
                break;
            case GDK_KEY_Right:
                controller1.setButtonState(PLAYER_1, BUTTON_RIGHT, true);
                break;
            case GDK_KEY_x:
            case GDK_KEY_X:
                controller1.setButtonState(PLAYER_1, BUTTON_A, true);
                break;
            case GDK_KEY_z:
            case GDK_KEY_Z:
                controller1.setButtonState(PLAYER_1, BUTTON_B, true);
                break;
            case GDK_KEY_Shift_R:
                controller1.setButtonState(PLAYER_1, BUTTON_SELECT, true);
                break;
            case GDK_KEY_Return:
                controller1.setButtonState(PLAYER_1, BUTTON_START, true);
                break;
            
            // Player 2 controls
            case GDK_KEY_i:
            case GDK_KEY_I:
                controller1.setButtonState(PLAYER_2, BUTTON_UP, true);
                break;
            case GDK_KEY_k:
            case GDK_KEY_K:
                controller1.setButtonState(PLAYER_2, BUTTON_DOWN, true);
                break;
            case GDK_KEY_j:
            case GDK_KEY_J:
                controller1.setButtonState(PLAYER_2, BUTTON_LEFT, true);
                break;
            case GDK_KEY_l:
            case GDK_KEY_L:
                controller1.setButtonState(PLAYER_2, BUTTON_RIGHT, true);
                break;
            case GDK_KEY_n:
            case GDK_KEY_N:
                controller1.setButtonState(PLAYER_2, BUTTON_A, true);
                break;
            case GDK_KEY_m:
            case GDK_KEY_M:
                controller1.setButtonState(PLAYER_2, BUTTON_B, true);
                break;
            case GDK_KEY_Control_R:
                controller1.setButtonState(PLAYER_2, BUTTON_SELECT, true);
                break;
            case GDK_KEY_space:
                controller1.setButtonState(PLAYER_2, BUTTON_START, true);
                break;
        }
    }
    
    return TRUE;
}

gboolean GTKMainWindow::onKeyRelease(GtkWidget* widget, GdkEventKey* event, gpointer user_data) 
{
    if (smbEngine) {
        Controller& controller1 = smbEngine->getController1();
        
        // Handle game controls directly (Player 1)
        switch (event->keyval) {
            case GDK_KEY_Up:
                controller1.setButtonState(PLAYER_1, BUTTON_UP, false);
                break;
            case GDK_KEY_Down:
                controller1.setButtonState(PLAYER_1, BUTTON_DOWN, false);
                break;
            case GDK_KEY_Left:
                controller1.setButtonState(PLAYER_1, BUTTON_LEFT, false);
                break;
            case GDK_KEY_Right:
                controller1.setButtonState(PLAYER_1, BUTTON_RIGHT, false);
                break;
            case GDK_KEY_x:
            case GDK_KEY_X:
                controller1.setButtonState(PLAYER_1, BUTTON_A, false);
                break;
            case GDK_KEY_z:
            case GDK_KEY_Z:
                controller1.setButtonState(PLAYER_1, BUTTON_B, false);
                break;
            case GDK_KEY_Shift_R:
                controller1.setButtonState(PLAYER_1, BUTTON_SELECT, false);
                break;
            case GDK_KEY_Return:
                controller1.setButtonState(PLAYER_1, BUTTON_START, false);
                break;
            
            // Player 2 controls
            case GDK_KEY_i:
            case GDK_KEY_I:
                controller1.setButtonState(PLAYER_2, BUTTON_UP, false);
                break;
            case GDK_KEY_k:
            case GDK_KEY_K:
                controller1.setButtonState(PLAYER_2, BUTTON_DOWN, false);
                break;
            case GDK_KEY_j:
            case GDK_KEY_J:
                controller1.setButtonState(PLAYER_2, BUTTON_LEFT, false);
                break;
            case GDK_KEY_l:
            case GDK_KEY_L:
                controller1.setButtonState(PLAYER_2, BUTTON_RIGHT, false);
                break;
            case GDK_KEY_n:
            case GDK_KEY_N:
                controller1.setButtonState(PLAYER_2, BUTTON_A, false);
                break;
            case GDK_KEY_m:
            case GDK_KEY_M:
                controller1.setButtonState(PLAYER_2, BUTTON_B, false);
                break;
            case GDK_KEY_Control_R:
                controller1.setButtonState(PLAYER_2, BUTTON_SELECT, false);
                break;
            case GDK_KEY_space:
                controller1.setButtonState(PLAYER_2, BUTTON_START, false);
                break;
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
    // Initialize SDL for audio and controllers only - NO VIDEO AT ALL
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) < 0) {
        updateStatusBar("SDL initialization failed");
        return;
    }

    // Initialize audio
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
        SDL_OpenAudio(&desiredSpec, &obtainedSpec);
        SDL_PauseAudio(0);
    }

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
    bool joystickInitialized = controller1.initJoystick();
    if (joystickInitialized) {
        std::cout << "Controller system initialized successfully!" << std::endl;
        if (controller1.isJoystickConnected(PLAYER_1))
            std::cout << "Player 1 joystick connected" << std::endl;
        if (controller1.isJoystickConnected(PLAYER_2))
            std::cout << "Player 2 joystick connected" << std::endl;
            
        controller1.setJoystickPolling(false);
        std::cout << "Using event-driven joystick input only" << std::endl;
    } else {
        std::cout << "No joysticks found. Using keyboard controls only." << std::endl;
    }

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
            
            // Force immediate redraw on the main thread - don't use g_idle_add
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

void GTKMainWindow::showControlSettings() 
{
    updateStatusBar("Opening control settings...");
    // TODO: Implement control settings dialog
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
