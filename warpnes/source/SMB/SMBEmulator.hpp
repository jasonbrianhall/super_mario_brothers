#ifndef SMB_EMULATOR_HPP
#define SMB_EMULATOR_HPP

#include "../Zapper.hpp"
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>

// Forward declarations
class APU;
class PPU;
class PPUCycleAccurate;

class Controller;

/**
 * Dynamic 6502 CPU emulator for NES/SMB
 * Loads .nes ROM files and executes actual 6502 machine code
 */
class SMBEmulator {
public:
  SMBEmulator();
  ~SMBEmulator();
  void checkCHRLatch(uint16_t address, uint8_t tileID);
  void checkSprite0Hit(int scanline, int cycle);
  uint8_t *getCHR();

  void enableZapper(bool enable);
  bool isZapperEnabled() const { return zapperEnabled; }
  Zapper &getZapper() { return *zapper; }
  void updateZapperInput(int mouseX, int mouseY, bool mousePressed);

  // Add these methods that PPU needs (same as SMBEngine)
  uint8_t readData(uint16_t address);
  void writeData(uint16_t address, uint8_t value);

  // ROM loading
  bool loadROM(const std::string &filename);
  void unloadROM();
  bool isROMLoaded() const { return romLoaded; }

  // Emulation control
  void reset();
  void update(); // Execute one frame worth of CPU cycles
  void step();   // Execute one instruction

  // Rendering
  void renderScaled16(uint16_t *buffer, int screenWidth, int screenHeight);
#ifndef __DJGPP__
  void render(uint32_t *buffer);
  void renderScaled32(uint32_t *buffer, int screenWidth, int screenHeight);
#else
  void render(unsigned int *buffer);
  void renderScaled32(unsigned int *buffer, int screenWidth, int screenHeight);
#endif
  void renderDirectFast(uint16_t *buffer, int screenWidth, int screenHeight);
  void render16(uint16_t *buffer);

  // Audio
  void audioCallback(uint8_t *stream, int length);
  void toggleAudioMode();
  bool isUsingMIDIAudio() const;
  void debugAudioChannels();

  // Controllers
  Controller &getController1();
  Controller &getController2();

  // Save states
  void saveState(const std::string &filename);
  bool loadState(const std::string &filename);

  // CPU state access (for debugging)
  struct CPUState {
    uint8_t A, X, Y, SP; // Registers
    uint16_t PC;         // Program Counter
    uint8_t P;           // Processor status flags
    uint64_t cycles;     // Total CPU cycles executed
  };

  CPUState getCPUState() const;
  uint8_t readMemory(uint16_t address) const;
  void writeMemory(uint16_t address, uint8_t value);
  uint8_t readCHRData(uint16_t address);
  uint8_t getCurrentCHRBank() const {
    if (nesHeader.mapper == 66)
      return gxrom.chrBank;
    return 0;
  }
  uint8_t getMapper() const { return nesHeader.mapper; }
  void writeCNROMRegister(uint16_t address, uint8_t value);
  uint8_t readCHRDataFromBank(uint16_t address, uint8_t bank);
  void writeCHRData(uint16_t address, uint8_t value);
  void updateCycleAccurate(); // For mappers that can change banks mid-frame
  void stepPPUCycle();        // Step PPU one cycle
  void checkMMC3IRQ();        // Check for MMC3 IRQ timing
  void forceSRAMSave();
  
  struct PPUCycleState {
    int scanline;
    int cycle;
    bool renderingEnabled;
    bool inVBlank;
    bool sprite0HitPending;
    int sprite0HitCycle;
    int sprite0HitScanline;

    // Additional timing state
    bool frameEven;
    int cpuCycleCounter;
    bool lastA12State; // For MMC3 A12 tracking
  } ppuCycleState;

    void scaleBuffer16(uint16_t *nesBuffer, uint16_t *screenBuffer, int screenWidth, int screenHeight);

    void initializeSRAM();
    void loadSRAM();
    void saveSRAM();
    void cleanupSRAM();

private:
  // 6502 CPU state
  uint8_t regA, regX, regY, regSP;
  uint16_t regPC;
  uint8_t regP; // Processor status: NV-BDIZC
  uint64_t totalCycles;
  uint64_t frameCycles;

  // NES Zapper
  Zapper *zapper;
  bool zapperEnabled;
  uint16_t *currentFrameBuffer;
  bool isPixelBright(uint16_t pixelColor);
  // Status flag helpers
  enum StatusFlags {
    FLAG_CARRY = 0x01,
    FLAG_ZERO = 0x02,
    FLAG_INTERRUPT = 0x04,
    FLAG_DECIMAL = 0x08,
    FLAG_BREAK = 0x10,
    FLAG_UNUSED = 0x20,
    FLAG_OVERFLOW = 0x40,
    FLAG_NEGATIVE = 0x80
  };

  void setFlag(uint8_t flag, bool value);
  bool getFlag(uint8_t flag) const;
  void updateZN(uint8_t value);

  // Memory system
  uint8_t ram[0x2000]; // 8KB RAM (mirrored)
  uint8_t *prgROM;     // PRG ROM data
  uint8_t *chrROM;     // CHR ROM data
  uint32_t prgSize;    // PRG ROM size
  uint32_t chrSize;    // CHR ROM size
  bool romLoaded;

  // NES header info
  struct NESHeader {
    uint8_t prgROMPages; // 16KB pages
    uint8_t chrROMPages; // 8KB pages
    uint8_t mapper;      // Mapper number
    uint8_t mirroring;   // 0=horizontal, 1=vertical
    bool battery;        // Battery-backed RAM
    bool trainer;        // 512-byte trainer present
  } nesHeader;

  // Components
  APU *apu;
  PPU *ppu;
  PPUCycleAccurate *ppuCycleAccurate;

  Controller *controller1;
  Controller *controller2;

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
  void ADC(uint16_t addr);
  void AND(uint16_t addr);
  void ASL(uint16_t addr);
  void BCC();
  void BCS();
  void BEQ();
  void BIT(uint16_t addr);
  void BMI();
  void BNE();
  void BPL();
  void BRK();
  void BVC();
  void BVS();
  void CLC();
  void CLD();
  void CLI();
  void CLV();
  void CMP(uint16_t addr);
  void CPX(uint16_t addr);
  void CPY(uint16_t addr);
  void DEC(uint16_t addr);
  void DEX();
  void DEY();
  void EOR(uint16_t addr);
  void INC(uint16_t addr);
  void INX();
  void INY();
  void JMP(uint16_t addr);
  void JSR(uint16_t addr);
  void LDA(uint16_t addr);
  void LDX(uint16_t addr);
  void LDY(uint16_t addr);
  void LSR(uint16_t addr);
  void NOP();
  void ORA(uint16_t addr);
  void PHA();
  void PHP();
  void PLA();
  void PLP();
  void ROL(uint16_t addr);
  void ROR(uint16_t addr);
  void RTI();
  void RTS();
  void SBC(uint16_t addr);
  void SEC();
  void SED();
  void SEI();
  void STA(uint16_t addr);
  void STX(uint16_t addr);
  void STY(uint16_t addr);
  void TAX();
  void TAY();
  void TSX();
  void TXA();
  void TXS();
  void TYA();
  void SHA(uint16_t addr); // A & X & (high byte of address + 1)
  void SHX(uint16_t addr); // X & (high byte of address + 1)
  void SHY(uint16_t addr); // Y & (high byte of address + 1)
  void TAS(uint16_t addr); // A & X -> SP, then A & X & (high byte + 1)
  void LAS(uint16_t addr); // Load A,X,SP from memory & SP
  // Special addressing mode versions
  void ASL_ACC();
  void LSR_ACC();
  void ROL_ACC();
  void ROR_ACC();

  // ROM file parsing
  bool parseNESHeader(std::ifstream &file);
  bool loadPRGROM(std::ifstream &file);
  bool loadCHRROM(std::ifstream &file);

  // Interrupt handling
  void handleNMI();
  void handleIRQ();
  void handleRESET();

  // Timing
  static const int CYCLES_PER_FRAME = 29780; // NTSC timing

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
  void KIL();

  // Save state structure
  struct EmulatorSaveState {
    char header[8];  // "NESSAVE\0"
    uint8_t version; // Save state version

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

    uint8_t reserved[64]; // Future expansion
  };

struct MMC1State {
    uint8_t shiftRegister;
    uint8_t shiftCount;
    uint8_t control;
    uint8_t chrBank0;
    uint8_t chrBank1;
    uint8_t prgBank;
    uint8_t currentPRGBank;
    uint8_t currentCHRBank0;
    uint8_t currentCHRBank1;

    MMC1State() {
        shiftRegister = 0x10;
        shiftCount = 0;
        control = 0x0C;
        chrBank0 = 0;
        chrBank1 = 0;
        prgBank = 0;
        currentPRGBank = 0;
        currentCHRBank0 = 0;
        currentCHRBank1 = 1;
    }
} mmc1;

  void writeMMC1Register(uint16_t address, uint8_t value);
  void updateMMC1Banks();

  struct GxROMState {
    uint8_t prgBank; // Current PRG bank (32KB)
    uint8_t chrBank; // Current CHR bank (8KB)

    GxROMState() {
      prgBank = 0;
      chrBank = 0;
    }
  } gxrom;

  struct CNROMState {
    uint8_t chrBank; // Current CHR bank (8KB)

    CNROMState() { chrBank = 0; }
  } cnrom;

  void writeGxROMRegister(uint16_t address, uint8_t value);

  struct MMC3State {
    uint8_t bankSelect;    // $8000-$9FFE (even)
    uint8_t bankData[8];   // Bank data registers
    uint8_t mirroring;     // $A000-$BFFE (even)
    uint8_t prgRamProtect; // $A001-$BFFF (odd)
    uint8_t irqLatch;      // $C000-$DFFE (even)
    uint8_t irqCounter;    // Internal counter
    bool irqEnable;        // $E000-$FFFE (even)
    bool irqReload;        // Flag to reload counter
    bool irqPending; 
    // Current bank mappings
    uint8_t currentPRGBanks[4]; // 8KB banks at $8000, $A000, $C000, $E000
    uint8_t currentCHRBanks[8]; // 1KB banks at $0000-$1FFF

    MMC3State() {
      bankSelect = 0;
      for (int i = 0; i < 8; i++)
        bankData[i] = 0;
      mirroring = 0;
      prgRamProtect = 0;
      irqLatch = 0;
      irqCounter = 0;
      irqEnable = false;
      irqReload = false;
      irqPending = false;
      // Initialize bank mappings
      for (int i = 0; i < 4; i++)
        currentPRGBanks[i] = 0;
      for (int i = 0; i < 8; i++)
        currentCHRBanks[i] = 0;
    }
  } mmc3;

  void writeMMC3Register(uint16_t address, uint8_t value);
  void updateMMC3Banks();
  void stepMMC3IRQ(); // Call this during PPU rendering

  bool mmc3IRQPending() const { return mmc3.irqCounter == 0 && mmc3.irqEnable; }

  struct UxROMState {
    uint8_t prgBank; // Current switchable PRG bank (16KB)

    UxROMState() {
      prgBank = 0; // Start with bank 0
    }
  } uxrom;

  void writeUxROMRegister(uint16_t address, uint8_t value);
  bool needsCycleAccuracy() const;
  void updateFrameBased();

  // PPU cycle stepping methods
  void stepPPUFetchNametable();
  void stepPPUFetchAttribute();
  void stepPPUFetchPatternLow();
  void stepPPUFetchPatternHigh();
  void stepPPUSpriteEvaluation();
  void stepPPUEndOfScanline(int scanline);

  // Enhanced MMC3 methods
  void stepMMC3A12Transition(bool a12High);
  void checkMMC3IRQ(int scanline, int cycle); // Enhanced version

  // Additional timing state
  uint64_t ppuCycles; // Total PPU cycles executed
  bool nmiPending;    // NMI waiting to be processed
  int nmiDelay;       // Cycles until NMI triggers

  uint64_t masterCycles;
    
  void catchUpPPU();
  void checkPendingInterrupts();
  uint8_t *sram;        // 8KB SRAM for battery saves
  uint32_t sramSize;    // SRAM size (usually 8KB)
  bool sramEnabled;     // SRAM read/write enabled
  bool sramDirty;       // SRAM has been written to
  std::string romBaseName;  // Base ROM filename for save files


  struct MMC2State {
    uint8_t prgBank;           // PRG bank register (8KB switchable)
    uint8_t chrBank0FD;        // CHR bank when $FD is read from $0000-$0FFF
    uint8_t chrBank0FE;        // CHR bank when $FE is read from $0000-$0FFF  
    uint8_t chrBank1FD;        // CHR bank when $FD is read from $1000-$1FFF
    uint8_t chrBank1FE;        // CHR bank when $FE is read from $1000-$1FFF
    bool latch0;               // Current latch state for $0000-$0FFF (0=FD, 1=FE)
    bool latch1;               // Current latch state for $1000-$1FFF (0=FD, 1=FE)
    uint8_t mirroring;         // Mirroring control
    
    // Current effective CHR banks
    uint8_t currentCHRBank0;   // Current 4KB bank at $0000-$0FFF
    uint8_t currentCHRBank1;   // Current 4KB bank at $1000-$1FFF
    
    MMC2State() {
        prgBank = 0;
        chrBank0FD = 0;
        chrBank0FE = 0;
        chrBank1FD = 0;
        chrBank1FE = 0;
        latch0 = false;  // Start with FD state
        latch1 = false;  // Start with FD state
        mirroring = 0;
        latch0 = false;  // Start with FD state
        latch1 = false;  // Start with FD state
        currentCHRBank0 = 0;
        currentCHRBank1 = 0;
    }
} mmc2;

void writeMMC2Register(uint16_t address, uint8_t value);
void updateMMC2Banks();
void checkMMC2CHRLatch(uint16_t address, uint8_t tileID);

};

#endif // SMB_EMULATOR_HPP
