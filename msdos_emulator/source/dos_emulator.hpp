#ifndef DOS_EMULATOR_HPP
#define DOS_EMULATOR_HPP

#include "AllegroMainWindow.hpp"

// Forward declarations
class SMBEmulator;

/**
 * Emulator main window - uses composition instead of inheritance
 * Contains an AllegroMainWindow and SMBEmulator
 */
class EmulatorMainWindow {
public:
    EmulatorMainWindow();
    ~EmulatorMainWindow();
    
    // Main interface
    bool initialize();
    void run();
    void shutdown();
    
    // Emulator-specific methods
    bool loadROM(const std::string& filename);
    bool isEmulatorReady() const;
    
    // Debug methods
    void printCPUState();
    void printMemoryDump(uint16_t start, uint16_t length);

private:
    AllegroMainWindow* window;  // Composition instead of inheritance
    SMBEmulator* emulator;
    bool romLoaded;
    std::string currentROMPath;
    
    // Game state (our own copies)
    bool gameRunning;
    bool gamePaused;
    bool showingMenu;
    int currentDialog;
    uint32_t* currentFrameBuffer;
    
    // Helper methods
    void initializeEmulator();
    void shutdownEmulator();
    bool validateROMFile(const std::string& filename);
    
    // Our own input/rendering methods
    void handleInput();
    void handleGameInput();
    void checkPlayerInput(int player);
    void updateAndDraw();
    void drawGameBuffered(BITMAP* target);
    
    // Status and menu handling
    void setStatusMessage(const char* msg);
    void showEmulatorStatus();
    void showCPUDebugInfo();
};

// Global functions for emulator version
extern SMBEmulator* smbEmulator;
extern EmulatorMainWindow* g_emulatorWindow;

// Audio callback for emulator
void audio_stream_callback(void* buffer, int len);

#endif // DOS_EMULATOR_HPP
