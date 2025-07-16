#ifndef SMBEMULATOR_HPP
#define SMBEMULATOR_HPP

#include <cstdint>
#include <string>
#include <fstream>

// Forward declarations
class CPU;
class CycleAccuratePPU;  // CHANGED: From PPU to CycleAccuratePPU
class APU;
class Mapper;
class Controller;

class SMBEmulator {
public:
    SMBEmulator();
    ~SMBEmulator();
    
    // Core emulation methods
    bool loadROM(const char* filename);
    void reset();
    void update();  // CHANGED: Now cycle-accurate, no parameters
    
    // Rendering methods
    void render16(uint16_t* buffer);  // CHANGED: Now just copies pre-rendered buffer
    void renderScaled16(uint16_t* buffer, int screenWidth, int screenHeight);  // NEW: Scaling method
    
    // PPU register access - CHANGED: Now goes through cycle-accurate PPU
    uint8_t readPPURegister(uint16_t address);
    void writePPURegister(uint16_t address, uint8_t value);
    void writePPUDMA(uint8_t page);
    
    // Memory access
    uint8_t readData(uint16_t address);
    void writeData(uint16_t address, uint8_t value);
    
    // CHR data access - NEW: With bank support
    uint8_t readCHRData(uint16_t address);
    void writeCHRData(uint16_t address, uint8_t value);
    uint8_t readCHRDataFromBank(uint16_t address, uint8_t bank);  // NEW: Bank-specific CHR access
    
    // Controller access
    Controller& getController1();
    Controller& getController2();
    
    // Audio
    void audioCallback(uint8_t* buffer, int length);
    void toggleAudioMode();
    bool isUsingMIDIAudio() const;
    void debugAudioChannels();
    
    // State management
    bool saveState(const char* filename);
    bool loadState(const char* filename);
    
    // ROM data access
    const uint8_t* getCHR() const { return chr; }
    const uint8_t* getPRG() const { return prg; }
    
    // NEW: Cycle-accurate system methods
    bool isFrameReady() const;
    uint8_t getCurrentCHRBank() const;
    void setCHRBank(uint8_t bank);
    void triggerNMI();
    
    // PPU state access for compatibility
    uint8_t* getPaletteRAM();
    uint8_t* getOAM();
    uint8_t* getVRAM();
    uint8_t getPPUControl() const;
    uint8_t getPPUMask() const;
    
private:
    // Core components
    CPU* cpu;
    CycleAccuratePPU* cycleAccuratePPU;  // CHANGED: From PPU* ppu to CycleAccuratePPU*
    APU* apu;
    Mapper* mapper;
    Controller* controller1;
    Controller* controller2;
    
    // ROM data
    uint8_t* prg;
    uint8_t* chr;
    size_t prgSize;
    size_t chrSize;
    
    // ROM header info
    uint8_t mapperNumber;
    bool hasBattery;
    bool hasTrainer;
    bool verticalMirroring;
    
    // NEW: Cycle-accurate rendering state
    bool frameReady;
    uint16_t renderBuffer[256 * 240];
    uint8_t currentCHRBank;
    
    // Audio state
    bool usingMIDIAudio;
    
    // Internal methods
    bool parseROMHeader(std::ifstream& file);
    bool loadPRGData(std::ifstream& file);
    bool loadCHRData(std::ifstream& file);
    void setupMapper();
    void cleanupROM();
    
    // State serialization helpers
    void serializeCPUState(std::ofstream& file);
    void serializePPUState(std::ofstream& file);
    void serializeAPUState(std::ofstream& file);
    void serializeMapperState(std::ofstream& file);
    
    void deserializeCPUState(std::ifstream& file);
    void deserializePPUState(std::ifstream& file);
    void deserializeAPUState(std::ifstream& file);
    void deserializeMapperState(std::ifstream& file);
    
    // Memory mapping
    uint8_t readPRG(uint16_t address);
    void writePRG(uint16_t address, uint8_t value);
    
    // Constants
    static const size_t HEADER_SIZE = 16;
    static const size_t TRAINER_SIZE = 512;
    static const size_t PRG_BANK_SIZE = 16384;  // 16KB
    static const size_t CHR_BANK_SIZE = 8192;   // 8KB
    
    // Magic numbers for ROM format
    static const uint8_t NES_MAGIC[4];
};

#endif // SMBEMULATOR_HPP
