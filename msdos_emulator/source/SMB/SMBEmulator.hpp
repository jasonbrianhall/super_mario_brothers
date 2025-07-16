#ifndef SMBEMULATOR_HPP
#define SMBEMULATOR_HPP

#include <cstdint>
#include <string>
#include <fstream>
#include <iostream>

// Forward declarations
class CPU;
class CycleAccuratePPU;
class APU;
class Mapper;
class Controller;

class SMBEmulator {
public:
    SMBEmulator();
    ~SMBEmulator();
    
    // Core emulation methods
    bool loadROM(const std::string& filename);
    void reset();
    void update();
    void step();
    
    // Rendering methods
    void render(uint32_t* buffer);
    void render16(uint16_t* buffer);
    void renderScaled16(uint16_t* buffer, int screenWidth, int screenHeight);
    void renderDirectFast(uint16_t* buffer, int screenWidth, int screenHeight);
    void renderScaled32(uint32_t* buffer, int screenWidth, int screenHeight);
    
    // PPU register access
    uint8_t readPPURegister(uint16_t address);
    void writePPURegister(uint16_t address, uint8_t value);
    void writePPUDMA(uint8_t page);
    
    // Memory access
    uint8_t readData(uint16_t address);
    void writeData(uint16_t address, uint8_t value);
    
    // CHR data access
    uint8_t readCHRData(uint16_t address);
    void writeCHRData(uint16_t address, uint8_t value);
    uint8_t readCHRDataFromBank(uint16_t address, uint8_t bank);
    
    // PRG data access
    uint8_t readPRG(uint16_t address);
    void writePRG(uint16_t address, uint8_t value);
    
    // Controller access
    Controller& getController1();
    Controller& getController2();
    
    // Audio
    void audioCallback(uint8_t* buffer, int length);
    void toggleAudioMode();
    bool isUsingMIDIAudio() const;
    void debugAudioChannels();
    
    // State management
    void saveState(const std::string& filename);
    bool loadState(const std::string& filename);
    
    // ROM data access
    uint8_t* getCHR();
    const uint8_t* getCHR() const { return chrROM; }
    const uint8_t* getPRG() const { return prgROM; }
    
    // Cycle-accurate system methods
    bool isFrameReady() const { return frameReady; }
    uint8_t getCurrentCHRBank() const { return currentCHRBank; }
    void setCHRBank(uint8_t bank) { currentCHRBank = bank; }
    void triggerNMI() { handleNMI(); }
    
    // PPU state access for compatibility
    uint8_t* getPaletteRAM();
    uint8_t* getOAM();
    uint8_t* getVRAM();
    uint8_t getPPUControl() const;
    uint8_t getPPUMask() const;

    // CPU state access
    struct CPUState {
        uint8_t A, X, Y, SP, P;
        uint16_t PC;
        uint64_t cycles;
    };
    
    CPUState getCPUState() const;
    uint8_t readMemory(uint16_t address) const;
    void writeMemory(uint16_t address, uint8_t value);

    // Memory mapping
    uint8_t readByte(uint16_t address);
    void writeByte(uint16_t address, uint8_t value);
    uint16_t readWord(uint16_t address);
    void writeWord(uint16_t address, uint16_t value);
    
    // Stack operations
    void pushByte(uint8_t value);
    uint8_t pullByte();
    void pushWord(uint16_t value);
    uint16_t pullWord();
    
    // 6502 instruction execution
    void executeInstruction();
    uint8_t fetchByte();
    uint16_t fetchWord();
    
    // Addressing modes
    uint16_t addrImmediate();
    uint16_t addrZeroPage();
    uint16_t addrZeroPageX();
    uint16_t addrZeroPageY();
    uint16_t addrAbsolute();
    uint16_t addrAbsoluteX();
    uint16_t addrAbsoluteY();
    uint16_t addrIndirect();
    uint16_t addrIndirectX();
    uint16_t addrIndirectY();
    uint16_t addrRelative();
    
    // Instruction implementations
    void ADC(uint16_t addr); void AND(uint16_t addr); void ASL(uint16_t addr);
    void BCC(); void BCS(); void BEQ(); void BIT(uint16_t addr);
    void BMI(); void BNE(); void BPL(); void BRK();
    void BVC(); void BVS(); void CLC(); void CLD();
    void CLI(); void CLV(); void CMP(uint16_t addr); void CPX(uint16_t addr);
    void CPY(uint16_t addr); void DEC(uint16_t addr); void DEX(); void DEY();
    void EOR(uint16_t addr); void INC(uint16_t addr); void INX(); void INY();
    void JMP(uint16_t addr); void JSR(uint16_t addr); void LDA(uint16_t addr);
    void LDX(uint16_t addr); void LDY(uint16_t addr); void LSR(uint16_t addr);
    void NOP(); void ORA(uint16_t addr); void PHA(); void PHP();
    void PLA(); void PLP(); void ROL(uint16_t addr); void ROR(uint16_t addr);
    void RTI(); void RTS(); void SBC(uint16_t addr); void SEC();
    void SED(); void SEI(); void STA(uint16_t addr); void STX(uint16_t addr);
    void STY(uint16_t addr); void TAX(); void TAY(); void TSX();
    void TXA(); void TXS(); void TYA();
    void SHA(uint16_t addr); void SHX(uint16_t addr); void SHY(uint16_t addr);
    void TAS(uint16_t addr); void LAS(uint16_t addr);
    
    // Special addressing mode versions
    void ASL_ACC(); void LSR_ACC(); void ROL_ACC(); void ROR_ACC();
    
    // Illegal opcode implementations
    void ISC(uint16_t addr); void DCP(uint16_t addr); void LAX(uint16_t addr);
    void SAX(uint16_t addr); void SLO(uint16_t addr); void RLA(uint16_t addr);
    void SRE(uint16_t addr); void RRA(uint16_t addr); void ANC(uint16_t addr);
    void ALR(uint16_t addr); void ARR(uint16_t addr); void XAA(uint16_t addr);
    void AXS(uint16_t addr);
    
    // ROM file parsing
    bool parseNESHeader(std::ifstream& file);
    bool loadPRGROM(std::ifstream& file);
    bool loadCHRROM(std::ifstream& file);
    void unloadROM();
    void cleanupROM();
    
    // Interrupt handling
    void handleNMI();
    void handleIRQ();
    void handleRESET();
    
    // Mapper-specific methods
    void writeMMC1Register(uint16_t address, uint8_t value);
    void updateMMC1Banks();
    void writeGxROMRegister(uint16_t address, uint8_t value);
    void writeCNROMRegister(uint16_t address, uint8_t value);
    void writeMMC3Register(uint16_t address, uint8_t value);
    void updateMMC3Banks();
    void stepMMC3IRQ();
    void writeUxROMRegister(uint16_t address, uint8_t value);
    
    // Timing
    static const int CYCLES_PER_FRAME = 29780;
    static const uint8_t instructionCycles[256];
    
private:
    // Core components
    CPU* cpu;
    CycleAccuratePPU* cycleAccuratePPU;
    APU* apu;
    Mapper* mapper;
    Controller* controller1;
    Controller* controller2;
    
    // ROM data
    uint8_t* prgROM;
    uint8_t* chrROM;
    uint8_t* prg;  // Added missing member
    uint8_t* chr;  // Added missing member
    size_t prgSize;
    size_t chrSize;
    bool romLoaded;
    
    // Added missing mapper-related members
    uint8_t mapperNumber;
    bool hasBattery;
    bool hasTrainer;
    bool verticalMirroring;
    bool usingMIDIAudio;
    
    // CPU registers
    uint8_t regA, regX, regY, regSP, regP;
    uint16_t regPC;
    uint64_t totalCycles;
    uint32_t frameCycles;
    
    // CPU flags
    static const uint8_t FLAG_CARRY = 0x01;
    static const uint8_t FLAG_ZERO = 0x02;
    static const uint8_t FLAG_INTERRUPT = 0x04;
    static const uint8_t FLAG_DECIMAL = 0x08;
    static const uint8_t FLAG_BREAK = 0x10;
    static const uint8_t FLAG_UNUSED = 0x20;
    static const uint8_t FLAG_OVERFLOW = 0x40;
    static const uint8_t FLAG_NEGATIVE = 0x80;
    
    // Memory
    uint8_t ram[0x800];  // 2KB internal RAM
    
    // NES header structure
    struct NESHeader {
        uint8_t prgROMPages;
        uint8_t chrROMPages;
        uint8_t mapper;
        bool mirroring;
        bool battery;
        bool trainer;
    } nesHeader;
    
    // Mapper state structures
    struct MMC1State {
        uint8_t shiftRegister;
        uint8_t shiftCount;
        uint8_t control;
        uint8_t chrBank0, chrBank1;
        uint8_t prgBank;
        uint8_t currentPRGBank;
        uint8_t currentCHRBank0, currentCHRBank1;
    } mmc1;
    
    struct MMC3State {
        uint8_t bankSelect;
        uint8_t bankData[8];
        uint8_t currentPRGBanks[4];
        uint8_t currentCHRBanks[8];
        uint8_t mirroring;
        uint8_t prgRamProtect;
        uint8_t irqLatch;
        uint8_t irqCounter;
        bool irqReload;
        bool irqEnable;
    } mmc3;
    
    struct CNROMState {
        uint8_t chrBank;
    } cnrom;
    
    struct GxROMState {
        uint8_t prgBank;
        uint8_t chrBank;
    } gxrom;
    
    struct UxROMState {
        uint8_t prgBank;
    } uxrom;
    
    // Cycle-accurate rendering state
    bool frameReady;
    uint16_t renderBuffer[256 * 240];
    uint8_t currentCHRBank;
    
    // Save state structure
    struct EmulatorSaveState {
        char header[8];
        uint32_t version;
        uint8_t cpu_A, cpu_X, cpu_Y, cpu_SP, cpu_P;
        uint16_t cpu_PC;
        uint64_t cpu_cycles;
        uint8_t ram[0x800];
        // PPU and APU state would go here
    };
    
    // Helper methods
    void setFlag(uint8_t flag, bool value);
    bool getFlag(uint8_t flag) const;
    void updateZN(uint8_t value);
    
    // Constants
    static const size_t HEADER_SIZE = 16;
    static const size_t TRAINER_SIZE = 512;
    static const size_t PRG_BANK_SIZE = 16384;
    static const size_t CHR_BANK_SIZE = 8192;
    static const uint8_t NES_MAGIC[4];
};

#endif // SMBEMULATOR_HPP
