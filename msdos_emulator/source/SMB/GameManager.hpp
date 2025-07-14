#ifndef GAME_MANAGER_HPP
#define GAME_MANAGER_HPP

#include <string>

// Forward declarations
class SMBEngine;
class SMBEmulator;
class Controller;

/**
 * Manages both static and dynamic game engines
 * Provides unified interface for switching between them
 */
class GameManager {
public:
    enum EngineType {
        ENGINE_STATIC,   // SMBEngine - pre-translated code
        ENGINE_DYNAMIC   // SMBEmulator - 6502 emulator
    };
    
    GameManager();
    ~GameManager();
    
    // Engine management
    bool initializeStaticEngine();
    bool initializeDynamicEngine();
    bool loadROM(const std::string& filename); // For dynamic engine
    void switchEngine(EngineType type);
    EngineType getCurrentEngine() const { return currentEngine; }
    bool isEngineReady() const;
    
    // Game control (unified interface)
    void reset();
    void update();
    
    // Rendering (unified interface)
    void render(uint32_t* buffer);
    void render16(uint16_t* buffer);
    void renderScaled16(uint16_t* buffer, int screenWidth, int screenHeight);
    void renderScaled32(uint32_t* buffer, int screenWidth, int screenHeight);
    
    // Audio (unified interface)
    void audioCallback(uint8_t* stream, int length);
    void toggleAudioMode();
    bool isUsingMIDIAudio() const;
    void debugAudioChannels();
    
    // Controllers (unified interface)
    Controller& getController1();
    Controller& getController2();
    
    // Save states (unified interface)
    void saveState(const std::string& filename);
    bool loadState(const std::string& filename);
    
    // Engine-specific features
    bool isDynamicEngineLoaded() const;
    std::string getCurrentROMPath() const { return currentROMPath; }
    
    // Debug info (dynamic engine only)
    struct CPUDebugInfo {
        uint8_t A, X, Y, SP, P;
        uint16_t PC;
        uint64_t cycles;
        bool available;
    };
    
    CPUDebugInfo getCPUDebugInfo() const;
    uint8_t readMemory(uint16_t address) const;
    void writeMemory(uint16_t address, uint8_t value);

private:
    SMBEngine* staticEngine;
    SMBEmulator* dynamicEngine;
    EngineType currentEngine;
    std::string currentROMPath;
    
    // Helper methods
    bool isStaticEngineReady() const;
    bool isDynamicEngineReady() const;
};

#endif // GAME_MANAGER_HPP
