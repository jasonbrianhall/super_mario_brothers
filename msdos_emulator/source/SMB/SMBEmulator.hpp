#ifndef SMB_EMULATOR_HPP
#define SMB_EMULATOR_HPP

#include <cstdint>
#include <string>
#include <fstream>

// Forward declarations
class APU;
class PPU;
class Controller;

/**
 * Dynamic 6502 CPU emulator for NES/SMB
 * Loads .nes ROM files and executes actual 6502 machine code
 */
class SMBEmulator {
public:
    SMBEmulator();
    ~SMBEmulator();

    uint8_t* getCHR();
    
    // Add these methods that PPU needs (same as SMBEngine)
    uint8_t readData(uint16_t address);
    void writeData(uint16_t address, uint8_t value);
    
    // ROM loading
    bool loadROM(const std::string& filename);
    void unloadROM();
    bool isROMLoaded() const { return romLoaded; }
    
    // Emulation control
    void reset();
    void update();  // Execute one frame worth of CPU cycles
    void step();    // Execute one instruction
    
    // Rendering
    void render(uint32_t* buffer);
    void render16(uint16_t* buffer);
    void renderScaled16(uint16_t* buffer, int screenWidth, int screenHeight);
    void renderScaled32(uint32_t* buffer, int screenWidth, int screenHeight);
    void renderDirectFast(uint16_t* buffer, int screenWidth, int screenHeight);
    
    // Audio
    void audioCallback(uint8_t* stream, int length);
    void toggleAudioMode();
    bool isUsingMIDIAudio() const;
    void debugAudioChannels();
    
    // Controllers
    Controller& getController1();
    Controller& getController2();
    
    // Save states
    void saveState(const std::string& filename);
    bool loadState(const std::string& filename);
    
    // CPU state access (for debugging)
    struct CPUState {
        uint8_t A, X, Y, SP;    // Registers
        uint16_t PC;            // Program Counter
        uint8_t P;              // Processor status flags
        uint64_t cycles;        // Total CPU cycles executed
    };
    
    CPUState getCPUState() const;
    uint8_t readMemory(uint16_t address) const;
    void writeMemory(uint16_t address, uint8_t value);
    
private:
    // 6502 CPU state
    uint8_t regA, regX, regY, regSP;
    uint16_t regPC;
    uint8_t regP;  // Processor status: NV-BDIZC
    uint64_t totalCycles;
    uint64_t frameCycles;
    
    // Status flag helpers
    enum StatusFlags {
        FLAG_CARRY     = 0x01,
        FLAG_ZERO      = 0x02,
        FLAG_INTERRUPT = 0x04,
        FLAG_DECIMAL   = 0x08,
        FLAG_BREAK     = 0x10,
        FLAG_UNUSED    = 0x20,
        FLAG_OVERFLOW  = 0x40,
        FLAG_NEGATIVE  = 0x80
    };
    
    void setFlag(uint8_t flag, bool value);
    bool getFlag(uint8_t flag) const;
    void updateZN(uint8_t value);
    
    // Memory system
    uint8_t ram[0x2000];        // 8KB RAM (mirrored)
    uint8_t* prgROM;            // PRG ROM data
    uint8_t* chrROM;            // CHR ROM data
    uint32_t prgSize;           // PRG ROM size
    uint32_t chrSize;           // CHR ROM size
    bool romLoaded;
    
    // NES header info
    struct NESHeader {
        uint8_t prgROMPages;    // 16KB pages
        uint8_t chrROMPages;    // 8KB pages
        uint8_t mapper;         // Mapper number
        uint8_t mirroring;      // 0=horizontal, 1=vertical
        bool battery;           // Battery-backed RAM
        bool trainer;           // 512-byte trainer present
    } nesHeader;
    
    // Components
    APU* apu;
    PPU* ppu;
    Controller* controller1;
    Controller* controller2;
    
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
    
    // Special addressing mode versions
    void ASL_ACC(); void LSR_ACC(); void ROL_ACC(); void ROR_ACC();
    
    // ROM file parsing
    bool parseNESHeader(std::ifstream& file);
    bool loadPRGROM(std::ifstream& file);
    bool loadCHRROM(std::ifstream& file);
    
    // Interrupt handling
    void handleNMI();
    void handleIRQ();
    void handleRESET();
    
    // Timing
    static const int CYCLES_PER_FRAME = 29780;  // NTSC timing
    
    // Instruction cycle counts (for accurate timing)
    static const uint8_t instructionCycles[256];

// Illegal opcode implementations
void ISC(uint16_t addr); // INC + SBC
void DCP(uint16_t addr); // DEC + CMP  
void LAX(uint16_t addr); // LDA + LDX
void SAX(uint16_t addr); // Store A & X
void SLO(uint16_t addr); // ASL + ORA
void RLA(uint16_t addr); // ROL + AND
void SRE(uint16_t addr); // LSR + EOR
void RRA(uint16_t addr); // ROR + ADC
void ANC(uint16_t addr); // AND + set carry
void ALR(uint16_t addr); // AND + LSR
void ARR(uint16_t addr); // AND + ROR
void XAA(uint16_t addr); // Unstable AND
void AXS(uint16_t addr); // (A & X) - immediate
    
    // Save state structure
    struct EmulatorSaveState {
        char header[8];         // "NESSAVE\0"
        uint8_t version;        // Save state version
        
        // CPU state
        uint8_t cpu_A, cpu_X, cpu_Y, cpu_SP, cpu_P;
        uint16_t cpu_PC;
        uint64_t cpu_cycles;
        
        // RAM
        uint8_t ram[0x2000];
        
        // PPU state (will be expanded)
        uint8_t ppu_registers[8];
        uint8_t ppu_nametable[2048];
        uint8_t ppu_oam[256];
        uint8_t ppu_palette[32];
        
        // APU state (basic)
        uint8_t apu_registers[24];
        
        uint8_t reserved[64];   // Future expansion
    };
};

#endif // SMB_EMULATOR_HPP
