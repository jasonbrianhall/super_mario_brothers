#ifndef GTK_MAIN_WINDOW_HPP
#define GTK_MAIN_WINDOW_HPP

#include <gtk/gtk.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <thread>
#include <atomic>
#include <string>

// Forward declarations
class SMBEngine;

class GTKMainWindow 
{
public:
    GTKMainWindow();
    ~GTKMainWindow();
    
    bool initialize();
    void run();
    void shutdown();

private:
    // GTK widgets
    GtkWidget* window;
    GtkWidget* vbox;
    GtkWidget* menubar;
    GtkWidget* gameContainer;
    GtkWidget* statusbar;
    GtkWidget* configDialog;
    
    // SDL integration
    SDL_Window* sdlWindow;
    SDL_Renderer* sdlRenderer;
    SDL_Texture* sdlTexture;
    
    // Threading
    std::thread gameThread;
    std::atomic<bool> gameRunning;
    std::atomic<bool> gamePaused;
    
    // Menu creation
    void createMenuBar();
    void createGameContainer();
    void createStatusBar();
    void createConfigDialog() {} // Empty implementation for now
    
    // Menu callbacks
    static void onFileExit(GtkMenuItem* item, gpointer user_data);
    static void onVirtualizationStart(GtkMenuItem* item, gpointer user_data);
    static void onVirtualizationPause(GtkMenuItem* item, gpointer user_data);
    static void onVirtualizationReset(GtkMenuItem* item, gpointer user_data);
    static void onSettingsVideo(GtkMenuItem* item, gpointer user_data);
    static void onSettingsAudio(GtkMenuItem* item, gpointer user_data);
    static void onSettingsControls(GtkMenuItem* item, gpointer user_data);
    static void onHelpAbout(GtkMenuItem* item, gpointer user_data);
    
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
    void updateStatusBar(const std::string& message);
};

#endif // GTK_MAIN_WINDOW_HPP
