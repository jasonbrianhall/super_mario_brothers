#include <cstring>
#include <cstdint>
#include <string>
#include <fstream>
#include "SMBEmulator.hpp"
#include "../Emulation/APU.hpp"
#include "../Emulation/PPU.hpp"
#include "../Emulation/Controller.hpp"
#include "../Configuration.hpp"

// 6502 instruction cycle counts
const uint8_t SMBEmulator::instructionCycles[256] = {
    // 0x00-0x0F
    7, 6, 0, 8, 3, 3, 5, 5, 3, 2, 2, 2, 4, 4, 6, 6,
    // 0x10-0x1F  
    2, 5, 0, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
    // 0x20-0x2F
    6, 6, 0, 8, 3, 3, 5, 5, 4, 2, 2, 2, 4, 4, 6, 6,
    // 0x30-0x3F
    2, 5, 0, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
    // 0x40-0x4F
    6, 6, 0, 8, 3, 3, 5, 5, 3, 2, 2, 2, 3, 4, 6, 6,
    // 0x50-0x5F
    2, 5, 0, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
    // 0x60-0x6F
    6, 6, 0, 8, 3, 3, 5, 5, 4, 2, 2, 2, 5, 4, 6, 6,
    // 0x70-0x7F
    2, 5, 0, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
    // 0x80-0x8F
    2, 6, 2, 6, 3, 3, 3, 3, 2, 2, 2, 2, 4, 4, 4, 4,
    // 0x90-0x9F
    2, 6, 0, 6, 4, 4, 4, 4, 2, 5, 2, 5, 5, 5, 5, 5,
    // 0xA0-0xAF
    2, 6, 2, 6, 3, 3, 3, 3, 2, 2, 2, 2, 4, 4, 4, 4,
    // 0xB0-0xBF
    2, 5, 0, 5, 4, 4, 4, 4, 2, 4, 2, 4, 4, 4, 4, 4,
    // 0xC0-0xCF
    2, 6, 2, 8, 3, 3, 5, 5, 2, 2, 2, 2, 4, 4, 6, 6,
    // 0xD0-0xDF
    2, 5, 0, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
    // 0xE0-0xEF
    2, 6, 2, 8, 3, 3, 5, 5, 2, 2, 2, 2, 4, 4, 6, 6,
    // 0xF0-0xFF
    2, 5, 0, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7
};

SMBEmulator::SMBEmulator() 
    : regA(0), regX(0), regY(0), regSP(0xFF), regPC(0), regP(0x24),
      totalCycles(0), frameCycles(0), prgROM(nullptr), chrROM(nullptr),
      prgSize(0), chrSize(0), romLoaded(false)
{
    // Initialize RAM
    memset(ram, 0, sizeof(ram));
    memset(&nesHeader, 0, sizeof(nesHeader));
    
    // Create components - they'll get CHR data when ROM is loaded
    apu = new APU();
    ppu = new PPU(*this);
    controller1 = new Controller();
    controller2 = new Controller();
}

SMBEmulator::~SMBEmulator()
{
    delete apu;
    delete ppu;
    delete controller1;
    delete controller2;
    unloadROM();
}

bool SMBEmulator::loadROM(const std::string& filename)
{
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open ROM file: " << filename << std::endl;
        return false;
    }
    
    // Parse NES header
    if (!parseNESHeader(file)) {
        std::cerr << "Invalid NES ROM header" << std::endl;
        return false;
    }
    
    // Skip trainer if present
    if (nesHeader.trainer) {
        file.seekg(512, std::ios::cur);
    }
    
    // Load PRG ROM
    if (!loadPRGROM(file)) {
        std::cerr << "Failed to load PRG ROM" << std::endl;
        return false;
    }
    
    // Load CHR ROM
    if (!loadCHRROM(file)) {
        std::cerr << "Failed to load CHR ROM" << std::endl;
        return false;
    }
    
    file.close();
    romLoaded = true;
    
    std::cout << "ROM loaded successfully: " << filename << std::endl;
    std::cout << "PRG ROM: " << (prgSize / 1024) << "KB, CHR ROM: " << (chrSize / 1024) << "KB" << std::endl;
    std::cout << "Mapper: " << (int)nesHeader.mapper << ", Mirroring: " << (nesHeader.mirroring ? "Vertical" : "Horizontal") << std::endl;
    
    // Reset emulator state
    reset();
    
    return true;
}

void SMBEmulator::unloadROM()
{
    if (prgROM) {
        delete[] prgROM;
        prgROM = nullptr;
    }
    if (chrROM) {
        delete[] chrROM;
        chrROM = nullptr;
    }
    prgSize = chrSize = 0;
    romLoaded = false;
}

bool SMBEmulator::parseNESHeader(std::ifstream& file)
{
    uint8_t header[16];
    file.read(reinterpret_cast<char*>(header), 16);
    
    if (!file.good()) return false;
    
    // Check "NES\x1A" signature
    if (header[0] != 'N' || header[1] != 'E' || header[2] != 'S' || header[3] != 0x1A) {
        return false;
    }
    
    nesHeader.prgROMPages = header[4];
    nesHeader.chrROMPages = header[5];
    nesHeader.mapper = (header[6] >> 4) | (header[7] & 0xF0);
    nesHeader.mirroring = header[6] & 0x01;
    nesHeader.battery = (header[6] & 0x02) != 0;
    nesHeader.trainer = (header[6] & 0x04) != 0;
    
    return true;
}

bool SMBEmulator::loadPRGROM(std::ifstream& file)
{
    prgSize = nesHeader.prgROMPages * 16384; // 16KB pages
    if (prgSize == 0) return false;
    
    prgROM = new uint8_t[prgSize];
    file.read(reinterpret_cast<char*>(prgROM), prgSize);
    
    return file.good();
}

bool SMBEmulator::loadCHRROM(std::ifstream& file)
{
    chrSize = nesHeader.chrROMPages * 8192; // 8KB pages
    if (chrSize == 0) {
        // CHR RAM instead of CHR ROM
        chrSize = 8192;
        chrROM = new uint8_t[chrSize];
        memset(chrROM, 0, chrSize);
        printf("Using CHR RAM (8KB)\n");  // ADD THIS LINE
        return true;
    }
    
    chrROM = new uint8_t[chrSize];
    file.read(reinterpret_cast<char*>(chrROM), chrSize);
    
    // ADD THESE DEBUG LINES:
    printf("Loaded CHR ROM: %d bytes\n", chrSize);
    
    // Debug: Print first few CHR bytes
    printf("First CHR bytes: ");
    for (int i = 0; i < 16 && i < chrSize; i++) {
        printf("%02X ", chrROM[i]);
    }
    printf("\n");
    
    return file.good();
}

void SMBEmulator::handleNMI()
{
    static bool debugNMI = true;
    static int nmiCount = 0;
    
    nmiCount++;
    if (debugNMI && nmiCount % 60 == 0) {
        printf("NMI #%d: PC=$%04X\n", nmiCount, regPC);
    }
    
    // Push PC and status to stack
    pushWord(regPC);
    pushByte(regP & ~FLAG_BREAK);
    setFlag(FLAG_INTERRUPT, true);
    
    // Jump to NMI vector
    regPC = readWord(0xFFFA);
    
    if (debugNMI && nmiCount % 60 == 0) {
        printf("NMI jumps to: $%04X\n", regPC);
    }
    
    totalCycles += 7;
    frameCycles += 7;
}

void SMBEmulator::update()
{
    if (!romLoaded) return;

    static int frameCount = 0;
    frameCycles = 0;

    const int VISIBLE_FRAME_CYCLES = 24100;     // Approximate end of scanline rendering
    const int CYCLES_PER_FRAME = 29780;         // Total cycles per NTSC frame

    // Emulate visible scanlines (pre-VBlank)
    while (frameCycles < VISIBLE_FRAME_CYCLES) {
        executeInstruction();
    }

    // Begin VBlank
    ppu->setVBlankFlag(true);

    // Trigger NMI if enabled
    if (ppu->getControl() & 0x80) {
        handleNMI();
    }

    // Emulate VBlank period (non-visible scanlines)
    while (frameCycles < CYCLES_PER_FRAME) {
        executeInstruction();
    }

    // End VBlank at end of frame
    ppu->setVBlankFlag(false);

    frameCount++;

    // Advance audio frame
    if (Configuration::getAudioEnabled()) {
        apu->stepFrame();
    }
}

void SMBEmulator::reset()
{
    if (!romLoaded) return;
    
    // Reset CPU state
    regA = regX = regY = 0;
    regSP = 0xFF;
    regP = 0x24; // Interrupt flag set, decimal flag clear
    totalCycles = frameCycles = 0;
    
    // Reset vector at $FFFC-$FFFD
    regPC = readWord(0xFFFC);
    
    // Clear RAM
    memset(ram, 0, sizeof(ram));
    
    std::cout << "Emulator reset, PC = $" << std::hex << regPC << std::dec << std::endl;
}

void SMBEmulator::step()
{
    if (!romLoaded) return;
    executeInstruction();
}

void SMBEmulator::executeInstruction()
{
    // Debug the stuck PC  
    if (regPC == 0x8150) {
        static int count = 0;
        count++;
        if (count % 1000 == 0) {
            uint8_t opcode = readByte(0x8150);
            uint8_t operand = readByte(0x8151);
            printf("Stuck at $8150 - opcode=$%02X $%02X, A=$%02X, P=$%02X\n", 
                   opcode, operand, regA, regP);
        }
    }
    
    uint8_t opcode = fetchByte();
    uint8_t cycles = instructionCycles[opcode];
    
    // Decode and execute instruction
    switch (opcode) {
        // ADC - Add with Carry
        case 0x69: ADC(addrImmediate()); break;
        case 0x65: ADC(addrZeroPage()); break;
        case 0x75: ADC(addrZeroPageX()); break;
        case 0x6D: ADC(addrAbsolute()); break;
        case 0x7D: ADC(addrAbsoluteX()); break;
        case 0x79: ADC(addrAbsoluteY()); break;
        case 0x61: ADC(addrIndirectX()); break;
        case 0x71: ADC(addrIndirectY()); break;
        
        // AND - Logical AND
        case 0x29: AND(addrImmediate()); break;
        case 0x25: AND(addrZeroPage()); break;
        case 0x35: AND(addrZeroPageX()); break;
        case 0x2D: AND(addrAbsolute()); break;
        case 0x3D: AND(addrAbsoluteX()); break;
        case 0x39: AND(addrAbsoluteY()); break;
        case 0x21: AND(addrIndirectX()); break;
        case 0x31: AND(addrIndirectY()); break;
        
        // ASL - Arithmetic Shift Left
        case 0x0A: ASL_ACC(); break;
        case 0x06: ASL(addrZeroPage()); break;
        case 0x16: ASL(addrZeroPageX()); break;
        case 0x0E: ASL(addrAbsolute()); break;
        case 0x1E: ASL(addrAbsoluteX()); break;
        
        // Branch instructions
        case 0x90: BCC(); break;
        case 0xB0: BCS(); break;
        case 0xF0: BEQ(); break;
        case 0x30: BMI(); break;
        case 0xD0: BNE(); break;
        case 0x10: BPL(); break;
        case 0x50: BVC(); break;
        case 0x70: BVS(); break;
        
        // BIT - Bit Test
        case 0x24: BIT(addrZeroPage()); break;
        case 0x2C: BIT(addrAbsolute()); break;
        
        // BRK - Force Interrupt
        case 0x00: BRK(); break;
        
        // Flag instructions
        case 0x18: CLC(); break;
        case 0xD8: CLD(); break;
        case 0x58: CLI(); break;
        case 0xB8: CLV(); break;
        
        // CMP - Compare
        case 0xC9: CMP(addrImmediate()); break;
        case 0xC5: CMP(addrZeroPage()); break;
        case 0xD5: CMP(addrZeroPageX()); break;
        case 0xCD: CMP(addrAbsolute()); break;
        case 0xDD: CMP(addrAbsoluteX()); break;
        case 0xD9: CMP(addrAbsoluteY()); break;
        case 0xC1: CMP(addrIndirectX()); break;
        case 0xD1: CMP(addrIndirectY()); break;
        
        // CPX - Compare X
        case 0xE0: CPX(addrImmediate()); break;
        case 0xE4: CPX(addrZeroPage()); break;
        case 0xEC: CPX(addrAbsolute()); break;
        
        // CPY - Compare Y
        case 0xC0: CPY(addrImmediate()); break;
        case 0xC4: CPY(addrZeroPage()); break;
        case 0xCC: CPY(addrAbsolute()); break;
        
        // DEC - Decrement
        case 0xC6: DEC(addrZeroPage()); break;
        case 0xD6: DEC(addrZeroPageX()); break;
        case 0xCE: DEC(addrAbsolute()); break;
        case 0xDE: DEC(addrAbsoluteX()); break;
        
        // DEX, DEY - Decrement X, Y
        case 0xCA: DEX(); break;
        case 0x88: DEY(); break;
        
        // EOR - Exclusive OR
        case 0x49: EOR(addrImmediate()); break;
        case 0x45: EOR(addrZeroPage()); break;
        case 0x55: EOR(addrZeroPageX()); break;
        case 0x4D: EOR(addrAbsolute()); break;
        case 0x5D: EOR(addrAbsoluteX()); break;
        case 0x59: EOR(addrAbsoluteY()); break;
        case 0x41: EOR(addrIndirectX()); break;
        case 0x51: EOR(addrIndirectY()); break;
        
        // INC - Increment
        case 0xE6: INC(addrZeroPage()); break;
        case 0xF6: INC(addrZeroPageX()); break;
        case 0xEE: INC(addrAbsolute()); break;
        case 0xFE: INC(addrAbsoluteX()); break;
        
        // INX, INY - Increment X, Y
        case 0xE8: INX(); break;
        case 0xC8: INY(); break;
        
        // JMP - Jump
        case 0x4C: JMP(addrAbsolute()); break;
        case 0x6C: JMP(addrIndirect()); break;
        
        // JSR - Jump to Subroutine
        case 0x20: JSR(addrAbsolute()); break;
        
        // LDA - Load A
        case 0xA9: LDA(addrImmediate()); break;
        case 0xA5: LDA(addrZeroPage()); break;
        case 0xB5: LDA(addrZeroPageX()); break;
        case 0xAD: LDA(addrAbsolute()); break;
        case 0xBD: LDA(addrAbsoluteX()); break;
        case 0xB9: LDA(addrAbsoluteY()); break;
        case 0xA1: LDA(addrIndirectX()); break;
        case 0xB1: LDA(addrIndirectY()); break;
        
        // LDX - Load X
        case 0xA2: LDX(addrImmediate()); break;
        case 0xA6: LDX(addrZeroPage()); break;
        case 0xB6: LDX(addrZeroPageY()); break;
        case 0xAE: LDX(addrAbsolute()); break;
        case 0xBE: LDX(addrAbsoluteY()); break;
        
        // LDY - Load Y
        case 0xA0: LDY(addrImmediate()); break;
        case 0xA4: LDY(addrZeroPage()); break;
        case 0xB4: LDY(addrZeroPageX()); break;
        case 0xAC: LDY(addrAbsolute()); break;
        case 0xBC: LDY(addrAbsoluteX()); break;
        
        // LSR - Logical Shift Right
        case 0x4A: LSR_ACC(); break;
        case 0x46: LSR(addrZeroPage()); break;
        case 0x56: LSR(addrZeroPageX()); break;
        case 0x4E: LSR(addrAbsolute()); break;
        case 0x5E: LSR(addrAbsoluteX()); break;
        
        // NOP - No Operation
        case 0xEA: NOP(); break;
        
        // ORA - Logical OR
        case 0x09: ORA(addrImmediate()); break;
        case 0x05: ORA(addrZeroPage()); break;
        case 0x15: ORA(addrZeroPageX()); break;
        case 0x0D: ORA(addrAbsolute()); break;
        case 0x1D: ORA(addrAbsoluteX()); break;
        case 0x19: ORA(addrAbsoluteY()); break;
        case 0x01: ORA(addrIndirectX()); break;
        case 0x11: ORA(addrIndirectY()); break;
        
        // Stack operations
        case 0x48: PHA(); break;
        case 0x08: PHP(); break;
        case 0x68: PLA(); break;
        case 0x28: PLP(); break;
        
        // ROL - Rotate Left
        case 0x2A: ROL_ACC(); break;
        case 0x26: ROL(addrZeroPage()); break;
        case 0x36: ROL(addrZeroPageX()); break;
        case 0x2E: ROL(addrAbsolute()); break;
        case 0x3E: ROL(addrAbsoluteX()); break;
        
        // ROR - Rotate Right
        case 0x6A: ROR_ACC(); break;
        case 0x66: ROR(addrZeroPage()); break;
        case 0x76: ROR(addrZeroPageX()); break;
        case 0x6E: ROR(addrAbsolute()); break;
        case 0x7E: ROR(addrAbsoluteX()); break;
        
        // RTI - Return from Interrupt
        case 0x40: RTI(); break;
        
        // RTS - Return from Subroutine
        case 0x60: RTS(); break;
        
        // SBC - Subtract with Carry
        case 0xE9: SBC(addrImmediate()); break;
        case 0xE5: SBC(addrZeroPage()); break;
        case 0xF5: SBC(addrZeroPageX()); break;
        case 0xED: SBC(addrAbsolute()); break;
        case 0xFD: SBC(addrAbsoluteX()); break;
        case 0xF9: SBC(addrAbsoluteY()); break;
        case 0xE1: SBC(addrIndirectX()); break;
        case 0xF1: SBC(addrIndirectY()); break;
        
        // Set flag instructions
        case 0x38: SEC(); break;
        case 0xF8: SED(); break;
        case 0x78: SEI(); break;
        
        // STA - Store A
        case 0x85: STA(addrZeroPage()); break;
        case 0x95: STA(addrZeroPageX()); break;
        case 0x8D: STA(addrAbsolute()); break;
        case 0x9D: STA(addrAbsoluteX()); break;
        case 0x99: STA(addrAbsoluteY()); break;
        case 0x81: STA(addrIndirectX()); break;
        case 0x91: STA(addrIndirectY()); break;
        
        // STX - Store X
        case 0x86: STX(addrZeroPage()); break;
        case 0x96: STX(addrZeroPageY()); break;
        case 0x8E: STX(addrAbsolute()); break;
        
        // STY - Store Y
        case 0x84: STY(addrZeroPage()); break;
        case 0x94: STY(addrZeroPageX()); break;
        case 0x8C: STY(addrAbsolute()); break;
        
        // Transfer instructions
        case 0xAA: TAX(); break;
        case 0xA8: TAY(); break;
        case 0xBA: TSX(); break;
        case 0x8A: TXA(); break;
        case 0x9A: TXS(); break;
        case 0x98: TYA(); break;
        // Illegal/Undocumented opcodes commonly used by NES games
        case 0x0B: case 0x2B: ANC(addrImmediate()); break;  // ANC (AND + set carry)
        case 0x4B: ALR(addrImmediate()); break;             // ALR (AND + LSR)
        case 0x6B: ARR(addrImmediate()); break;             // ARR (AND + ROR)
        case 0x8B: XAA(addrImmediate()); break;             // XAA (unstable)
        case 0xAB: LAX(addrImmediate()); break;             // LAX (LDA + LDX)
        case 0xCB: AXS(addrImmediate()); break;             // AXS (A&X - immediate)
        case 0xEB: SBC(addrImmediate()); break;             // SBC (same as legal SBC)
        
        // ISC (INC + SBC) - very common
        case 0xE3: ISC(addrIndirectX()); break;
        case 0xE7: ISC(addrZeroPage()); break;
        case 0xEF: ISC(addrAbsolute()); break;
        case 0xF3: ISC(addrIndirectY()); break;
        case 0xF7: ISC(addrZeroPageX()); break;
        case 0xFB: ISC(addrAbsoluteY()); break;             // This is your $FB!
        case 0xFF: ISC(addrAbsoluteX()); break;
        
        // DCP (DEC + CMP) - common
        case 0xC3: DCP(addrIndirectX()); break;
        case 0xC7: DCP(addrZeroPage()); break;
        case 0xCF: DCP(addrAbsolute()); break;
        case 0xD3: DCP(addrIndirectY()); break;
        case 0xD7: DCP(addrZeroPageX()); break;
        case 0xDB: DCP(addrAbsoluteY()); break;
        case 0xDF: DCP(addrAbsoluteX()); break;
        
        // LAX (LDA + LDX) - common
        case 0xA3: LAX(addrIndirectX()); break;
        case 0xA7: LAX(addrZeroPage()); break;
        case 0xAF: LAX(addrAbsolute()); break;
        case 0xB3: LAX(addrIndirectY()); break;
        case 0xB7: LAX(addrZeroPageY()); break;
        case 0xBF: LAX(addrAbsoluteY()); break;
        
        // SAX (A & X)
        case 0x83: SAX(addrIndirectX()); break;
        case 0x87: SAX(addrZeroPage()); break;
        case 0x8F: SAX(addrAbsolute()); break;
        case 0x97: SAX(addrZeroPageY()); break;
        
        // SLO (ASL + ORA)
        case 0x03: SLO(addrIndirectX()); break;
        case 0x07: SLO(addrZeroPage()); break;
        case 0x0F: SLO(addrAbsolute()); break;
        case 0x13: SLO(addrIndirectY()); break;
        case 0x17: SLO(addrZeroPageX()); break;
        case 0x1B: SLO(addrAbsoluteY()); break;
        case 0x1F: SLO(addrAbsoluteX()); break;
        
        // RLA (ROL + AND)
        case 0x23: RLA(addrIndirectX()); break;
        case 0x27: RLA(addrZeroPage()); break;
        case 0x2F: RLA(addrAbsolute()); break;
        case 0x33: RLA(addrIndirectY()); break;
        case 0x37: RLA(addrZeroPageX()); break;
        case 0x3B: RLA(addrAbsoluteY()); break;
        case 0x3F: RLA(addrAbsoluteX()); break;
        
        // SRE (LSR + EOR)
        case 0x43: SRE(addrIndirectX()); break;
        case 0x47: SRE(addrZeroPage()); break;
        case 0x4F: SRE(addrAbsolute()); break;
        case 0x53: SRE(addrIndirectY()); break;
        case 0x57: SRE(addrZeroPageX()); break;
        case 0x5B: SRE(addrAbsoluteY()); break;
        case 0x5F: SRE(addrAbsoluteX()); break;
        
        // RRA (ROR + ADC)
        case 0x63: RRA(addrIndirectX()); break;
        case 0x67: RRA(addrZeroPage()); break;
        case 0x6F: RRA(addrAbsolute()); break;
        case 0x73: RRA(addrIndirectY()); break;
        case 0x77: RRA(addrZeroPageX()); break;
        case 0x7B: RRA(addrAbsoluteY()); break;
        case 0x7F: RRA(addrAbsoluteX()); break;
        
        // NOPs (various forms)
        case 0x1A: case 0x3A: case 0x5A: case 0x7A: 
        case 0xDA: case 0xFA: NOP(); break;
        case 0x80: case 0x82: case 0x89: case 0xC2: case 0xE2: 
            regPC++; cycles = 2; break; // NOP immediate
        case 0x04: case 0x44: case 0x64: 
            regPC++; cycles = 3; break; // NOP zero page
        case 0x0C: 
            regPC += 2; cycles = 4; break; // NOP absolute
        case 0x14: case 0x34: case 0x54: case 0x74: case 0xD4: case 0xF4:
            regPC++; cycles = 4; break; // NOP zero page,X
        case 0x1C: case 0x3C: case 0x5C: case 0x7C: case 0xDC: case 0xFC:
            regPC += 2; cycles = 4; break; // NOP absolute,X
        
        default:
            std::cerr << "Unknown opcode: $" << std::hex << (int)opcode << " at PC=$" << (regPC - 1) << std::dec << std::endl;
            cycles = 2; // Default cycle count for unknown instructions
            break;
    }
    
    // Update cycle counters
    totalCycles += cycles;
    frameCycles += cycles;
}

// Memory access functions
uint8_t SMBEmulator::readByte(uint16_t address)
{
    if (address < 0x2000) {
        // RAM (mirrored every 2KB)
        return ram[address & 0x7FF];
    }
    else if (address < 0x4000) {
        // PPU registers (mirrored every 8 bytes)
        uint16_t ppuAddr = 0x2000 + (address & 0x7);
        //printf("Reading PPU register $%04X (mapped to $%04X)\n", address, ppuAddr);
        return ppu->readRegister(ppuAddr);
    }
    else if (address < 0x4020) {
        // APU and I/O registers
        switch (address) {
case 0x4016:
    {
        static bool debugController = true;
        uint8_t result = controller1->readByte(PLAYER_1);
        if (debugController && result != 0) {
            printf("Controller 1 read: $%02X\n", result);
        }
        return result;
    }
                case 0x4017:
                return controller2->readByte(PLAYER_2);
            default:
                return 0; // Other APU registers are write-only
        }
    }
    else if (address >= 0x8000) {
        // PRG ROM
        uint32_t romAddr = address - 0x8000;
        if (prgSize == 16384) {
            // 16KB ROM mirrored
            romAddr &= 0x3FFF;
        }
        if (romAddr < prgSize) {
            return prgROM[romAddr];
        }
    }
    
    return 0; // Open bus
}

void SMBEmulator::writeByte(uint16_t address, uint8_t value)
{
    if (address < 0x2000) {
        // RAM (mirrored every 2KB)
        ram[address & 0x7FF] = value;
    }
    else if (address < 0x4000) {
        // PPU registers (mirrored every 8 bytes)
        ppu->writeRegister(0x2000 + (address & 0x7), value);
    }
    else if (address < 0x4020) {
        // APU and I/O registers
        switch (address) {
            case 0x4014:
                ppu->writeDMA(value);
                break;
case 0x4016:
    {
        static bool debugController = true;
        if (debugController) {
            printf("Controller strobe write: $%02X\n", value);
        }
        controller1->writeByte(value);
        controller2->writeByte(value);
    }
    break;
    
                default:
                apu->writeRegister(address, value);
                break;
        }
    }
    // ROM area is read-only
}

uint16_t SMBEmulator::readWord(uint16_t address)
{
    return readByte(address) | (readByte(address + 1) << 8);
}

void SMBEmulator::writeWord(uint16_t address, uint16_t value)
{
    writeByte(address, value & 0xFF);
    writeByte(address + 1, value >> 8);
}

// Stack operations
void SMBEmulator::pushByte(uint8_t value)
{
    writeByte(0x100 + regSP, value);
    regSP--;
}

uint8_t SMBEmulator::pullByte()
{
    regSP++;
    return readByte(0x100 + regSP);
}

void SMBEmulator::pushWord(uint16_t value)
{
    pushByte(value >> 8);
    pushByte(value & 0xFF);
}

uint16_t SMBEmulator::pullWord()
{
    uint8_t lo = pullByte();
    uint8_t hi = pullByte();
    return lo | (hi << 8);
}

// Instruction fetch
uint8_t SMBEmulator::fetchByte()
{
    return readByte(regPC++);
}

uint16_t SMBEmulator::fetchWord()
{
    uint16_t value = readWord(regPC);
    regPC += 2;
    return value;
}

// Status flag helpers
void SMBEmulator::setFlag(uint8_t flag, bool value)
{
    if (value) {
        regP |= flag;
    } else {
        regP &= ~flag;
    }
}

bool SMBEmulator::getFlag(uint8_t flag) const
{
    return (regP & flag) != 0;
}

void SMBEmulator::updateZN(uint8_t value)
{
    setFlag(FLAG_ZERO, value == 0);
    setFlag(FLAG_NEGATIVE, (value & 0x80) != 0);
}

// Addressing modes
uint16_t SMBEmulator::addrImmediate()
{
    return regPC++;
}

uint16_t SMBEmulator::addrZeroPage()
{
    return fetchByte();
}

uint16_t SMBEmulator::addrZeroPageX()
{
    return (fetchByte() + regX) & 0xFF;
}

uint16_t SMBEmulator::addrZeroPageY()
{
    return (fetchByte() + regY) & 0xFF;
}

uint16_t SMBEmulator::addrAbsolute()
{
    return fetchWord();
}

uint16_t SMBEmulator::addrAbsoluteX()
{
    return fetchWord() + regX;
}

uint16_t SMBEmulator::addrAbsoluteY()
{
    return fetchWord() + regY;
}

uint16_t SMBEmulator::addrIndirect()
{
    uint16_t addr = fetchWord();
    // 6502 bug: if address is $xxFF, high byte is fetched from $xx00
    if ((addr & 0xFF) == 0xFF) {
        return readByte(addr) | (readByte(addr & 0xFF00) << 8);
    } else {
        return readWord(addr);
    }
}

uint16_t SMBEmulator::addrIndirectX()
{
    uint8_t addr = (fetchByte() + regX) & 0xFF;
    return readByte(addr) | (readByte((addr + 1) & 0xFF) << 8);
}

uint16_t SMBEmulator::addrIndirectY()
{
    uint8_t addr = fetchByte();
    uint16_t base = readByte(addr) | (readByte((addr + 1) & 0xFF) << 8);
    return base + regY;
}

uint16_t SMBEmulator::addrRelative()
{
    int8_t offset = fetchByte();
    return regPC + offset;
}

// Instruction implementations
void SMBEmulator::ADC(uint16_t addr)
{
    uint8_t value = readByte(addr);
    uint16_t result = regA + value + (getFlag(FLAG_CARRY) ? 1 : 0);
    
    setFlag(FLAG_CARRY, result > 0xFF);
    setFlag(FLAG_OVERFLOW, ((regA ^ result) & (value ^ result) & 0x80) != 0);
    
    regA = result & 0xFF;
    updateZN(regA);
}

void SMBEmulator::AND(uint16_t addr)
{
    regA &= readByte(addr);
    updateZN(regA);
}

void SMBEmulator::ASL(uint16_t addr)
{
    uint8_t value = readByte(addr);
    setFlag(FLAG_CARRY, (value & 0x80) != 0);
    value <<= 1;
    writeByte(addr, value);
    updateZN(value);
}

void SMBEmulator::ASL_ACC()
{
    setFlag(FLAG_CARRY, (regA & 0x80) != 0);
    regA <<= 1;
    updateZN(regA);
}

void SMBEmulator::BCC()
{
    if (!getFlag(FLAG_CARRY)) {
        regPC = addrRelative();
    } else {
        regPC++; // Skip the offset byte
    }
}

void SMBEmulator::BCS()
{
    if (getFlag(FLAG_CARRY)) {
        regPC = addrRelative();
    } else {
        regPC++;
    }
}

void SMBEmulator::BEQ()
{
    if (getFlag(FLAG_ZERO)) {
        regPC = addrRelative();
    } else {
        regPC++;
    }
}

void SMBEmulator::BIT(uint16_t addr)
{
    uint8_t value = readByte(addr);
    setFlag(FLAG_ZERO, (regA & value) == 0);
    setFlag(FLAG_OVERFLOW, (value & 0x40) != 0);
    setFlag(FLAG_NEGATIVE, (value & 0x80) != 0);
}

void SMBEmulator::BMI()
{
    if (getFlag(FLAG_NEGATIVE)) {
        regPC = addrRelative();
    } else {
        regPC++;
    }
}

void SMBEmulator::BNE()
{
    if (!getFlag(FLAG_ZERO)) {
        regPC = addrRelative();
    } else {
        regPC++;
    }
}

void SMBEmulator::BPL()
{
    if (!getFlag(FLAG_NEGATIVE)) {
        regPC = addrRelative();
    } else {
        regPC++;
    }
}

void SMBEmulator::BRK()
{
    regPC++; // BRK is 2 bytes
    pushWord(regPC);
    pushByte(regP | FLAG_BREAK);
    setFlag(FLAG_INTERRUPT, true);
    regPC = readWord(0xFFFE); // IRQ vector
}

void SMBEmulator::BVC()
{
    if (!getFlag(FLAG_OVERFLOW)) {
        regPC = addrRelative();
    } else {
        regPC++;
    }
}

void SMBEmulator::BVS()
{
    if (getFlag(FLAG_OVERFLOW)) {
        regPC = addrRelative();
    } else {
        regPC++;
    }
}

void SMBEmulator::CLC()
{
    setFlag(FLAG_CARRY, false);
}

void SMBEmulator::CLD()
{
    setFlag(FLAG_DECIMAL, false);
}

void SMBEmulator::CLI()
{
    setFlag(FLAG_INTERRUPT, false);
}

void SMBEmulator::CLV()
{
    setFlag(FLAG_OVERFLOW, false);
}

void SMBEmulator::CMP(uint16_t addr)
{
    uint8_t value = readByte(addr);
    uint8_t result = regA - value;
    setFlag(FLAG_CARRY, regA >= value);
    updateZN(result);
}

void SMBEmulator::CPX(uint16_t addr)
{
    uint8_t value = readByte(addr);
    uint8_t result = regX - value;
    setFlag(FLAG_CARRY, regX >= value);
    updateZN(result);
}

void SMBEmulator::CPY(uint16_t addr)
{
    uint8_t value = readByte(addr);
    uint8_t result = regY - value;
    setFlag(FLAG_CARRY, regY >= value);
    updateZN(result);
}

void SMBEmulator::DEC(uint16_t addr)
{
    uint8_t value = readByte(addr) - 1;
    writeByte(addr, value);
    updateZN(value);
}

void SMBEmulator::DEX()
{
    regX--;
    updateZN(regX);
}

void SMBEmulator::DEY()
{
    regY--;
    updateZN(regY);
}

void SMBEmulator::EOR(uint16_t addr)
{
    regA ^= readByte(addr);
    updateZN(regA);
}

void SMBEmulator::INC(uint16_t addr)
{
    uint8_t value = readByte(addr) + 1;
    writeByte(addr, value);
    updateZN(value);
}

void SMBEmulator::INX()
{
    regX++;
    updateZN(regX);
}

void SMBEmulator::INY()
{
    regY++;
    updateZN(regY);
}

void SMBEmulator::JMP(uint16_t addr)
{
    regPC = addr;
}

void SMBEmulator::JSR(uint16_t addr)
{
    pushWord(regPC - 1);
    regPC = addr;
}

void SMBEmulator::LDA(uint16_t addr)
{
    regA = readByte(addr);
    updateZN(regA);
}

void SMBEmulator::LDX(uint16_t addr)
{
    regX = readByte(addr);
    updateZN(regX);
}

void SMBEmulator::LDY(uint16_t addr)
{
    regY = readByte(addr);
    updateZN(regY);
}

void SMBEmulator::LSR(uint16_t addr)
{
    uint8_t value = readByte(addr);
    setFlag(FLAG_CARRY, (value & 0x01) != 0);
    value >>= 1;
    writeByte(addr, value);
    updateZN(value);
}

void SMBEmulator::LSR_ACC()
{
    setFlag(FLAG_CARRY, (regA & 0x01) != 0);
    regA >>= 1;
    updateZN(regA);
}

void SMBEmulator::NOP()
{
    // Do nothing
}

void SMBEmulator::ORA(uint16_t addr)
{
    regA |= readByte(addr);
    updateZN(regA);
}

void SMBEmulator::PHA()
{
    pushByte(regA);
}

void SMBEmulator::PHP()
{
    pushByte(regP | FLAG_BREAK | FLAG_UNUSED);
}

void SMBEmulator::PLA()
{
    regA = pullByte();
    updateZN(regA);
}

void SMBEmulator::PLP()
{
    regP = pullByte() | FLAG_UNUSED;
    regP &= ~FLAG_BREAK;
}

void SMBEmulator::ROL(uint16_t addr)
{
    uint8_t value = readByte(addr);
    bool oldCarry = getFlag(FLAG_CARRY);
    setFlag(FLAG_CARRY, (value & 0x80) != 0);
    value = (value << 1) | (oldCarry ? 1 : 0);
    writeByte(addr, value);
    updateZN(value);
}

void SMBEmulator::ROL_ACC()
{
    bool oldCarry = getFlag(FLAG_CARRY);
    setFlag(FLAG_CARRY, (regA & 0x80) != 0);
    regA = (regA << 1) | (oldCarry ? 1 : 0);
    updateZN(regA);
}

void SMBEmulator::ROR(uint16_t addr)
{
    uint8_t value = readByte(addr);
    bool oldCarry = getFlag(FLAG_CARRY);
    setFlag(FLAG_CARRY, (value & 0x01) != 0);
    value = (value >> 1) | (oldCarry ? 0x80 : 0);
    writeByte(addr, value);
    updateZN(value);
}

void SMBEmulator::ROR_ACC()
{
    bool oldCarry = getFlag(FLAG_CARRY);
    setFlag(FLAG_CARRY, (regA & 0x01) != 0);
    regA = (regA >> 1) | (oldCarry ? 0x80 : 0);
    updateZN(regA);
}

void SMBEmulator::RTI()
{
    regP = pullByte() | FLAG_UNUSED;
    regP &= ~FLAG_BREAK;
    regPC = pullWord();
}

void SMBEmulator::RTS()
{
    regPC = pullWord() + 1;
}

void SMBEmulator::SBC(uint16_t addr)
{
    uint8_t value = readByte(addr);
    uint16_t result = regA - value - (getFlag(FLAG_CARRY) ? 0 : 1);
    
    setFlag(FLAG_CARRY, result <= 0xFF);
    setFlag(FLAG_OVERFLOW, ((regA ^ result) & (~value ^ result) & 0x80) != 0);
    
    regA = result & 0xFF;
    updateZN(regA);
}

void SMBEmulator::SEC()
{
    setFlag(FLAG_CARRY, true);
}

void SMBEmulator::SED()
{
    setFlag(FLAG_DECIMAL, true);
}

void SMBEmulator::SEI()
{
    setFlag(FLAG_INTERRUPT, true);
}

void SMBEmulator::STA(uint16_t addr)
{
    writeByte(addr, regA);
}

void SMBEmulator::STX(uint16_t addr)
{
    writeByte(addr, regX);
}

void SMBEmulator::STY(uint16_t addr)
{
    writeByte(addr, regY);
}

void SMBEmulator::TAX()
{
    regX = regA;
    updateZN(regX);
}

void SMBEmulator::TAY()
{
    regY = regA;
    updateZN(regY);
}

void SMBEmulator::TSX()
{
    regX = regSP;
    updateZN(regX);
}

void SMBEmulator::TXA()
{
    regA = regX;
    updateZN(regA);
}

void SMBEmulator::TXS()
{
    regSP = regX;
}

void SMBEmulator::TYA()
{
    regA = regY;
    updateZN(regA);
}

// Rendering functions (delegate to PPU)
void SMBEmulator::render(uint32_t* buffer)
{
    ppu->render(buffer);
}

void SMBEmulator::render16(uint16_t* buffer)
{
    ppu->render16(buffer);
}

void SMBEmulator::renderDirectFast(uint16_t* buffer, int screenWidth, int screenHeight)
{
    ppu->renderScaled(buffer, screenWidth, screenHeight);
}


void SMBEmulator::renderScaled16(uint16_t* buffer, int screenWidth, int screenHeight)
{
    ppu->renderScaled(buffer, screenWidth, screenHeight);
}

void SMBEmulator::renderScaled32(uint32_t* buffer, int screenWidth, int screenHeight)
{
    ppu->renderScaled32(buffer, screenWidth, screenHeight);
}

// Audio functions (delegate to APU)
void SMBEmulator::audioCallback(uint8_t* stream, int length)
{
    apu->output(stream, length);
}

void SMBEmulator::toggleAudioMode()
{
    apu->toggleAudioMode();
}

bool SMBEmulator::isUsingMIDIAudio() const
{
    return apu->isUsingMIDI();
}

void SMBEmulator::debugAudioChannels()
{
    apu->debugAudio();
}

// Controller access
Controller& SMBEmulator::getController1()
{
    return *controller1;
}

Controller& SMBEmulator::getController2()
{
    return *controller2;
}

// CPU state access
SMBEmulator::CPUState SMBEmulator::getCPUState() const
{
    CPUState state;
    state.A = regA;
    state.X = regX;
    state.Y = regY;
    state.SP = regSP;
    state.PC = regPC;
    state.P = regP;
    state.cycles = totalCycles;
    return state;
}

uint8_t SMBEmulator::readMemory(uint16_t address) const
{
    return const_cast<SMBEmulator*>(this)->readByte(address);
}

void SMBEmulator::writeMemory(uint16_t address, uint8_t value)
{
    writeByte(address, value);
}

// Save states
void SMBEmulator::saveState(const std::string& filename)
{
    EmulatorSaveState state;
    memset(&state, 0, sizeof(state));
    
    // Header
    strcpy(state.header, "NESSAVE");
    state.version = 1;
    
    // CPU state
    state.cpu_A = regA;
    state.cpu_X = regX;
    state.cpu_Y = regY;
    state.cpu_SP = regSP;
    state.cpu_P = regP;
    state.cpu_PC = regPC;
    state.cpu_cycles = totalCycles;
    
    // RAM
    memcpy(state.ram, ram, sizeof(ram));
    
    // TODO: Add PPU and APU state
    
    // Create appropriate filename based on platform
    std::string actualFilename;
    #ifdef __DJGPP__
        // DOS 8.3 format
        std::string baseName = filename;
        size_t dotPos = baseName.find_last_of('.');
        if (dotPos != std::string::npos) {
            baseName = baseName.substr(0, dotPos);
        }
        if (baseName.length() > 8) {
            baseName = baseName.substr(0, 8);
        }
        actualFilename = baseName + ".SAV";
    #else
        actualFilename = filename;
    #endif
    
    std::ofstream file(actualFilename, std::ios::binary);
    if (file.is_open()) {
        file.write(reinterpret_cast<const char*>(&state), sizeof(state));
        file.close();
        std::cout << "Emulator save state written to: " << actualFilename << std::endl;
    } else {
        std::cerr << "Error: Could not save state to: " << actualFilename << std::endl;
    }
}

bool SMBEmulator::loadState(const std::string& filename)
{
    std::string actualFilename;
    #ifdef __DJGPP__
        // DOS 8.3 format
        std::string baseName = filename;
        size_t dotPos = baseName.find_last_of('.');
        if (dotPos != std::string::npos) {
            baseName = baseName.substr(0, dotPos);
        }
        if (baseName.length() > 8) {
            baseName = baseName.substr(0, 8);
        }
        actualFilename = baseName + ".SAV";
    #else
        actualFilename = filename;
    #endif

    std::ifstream file(actualFilename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open save state: " << actualFilename << std::endl;
        return false;
    }
    
    EmulatorSaveState state;
    file.read(reinterpret_cast<char*>(&state), sizeof(state));
    file.close();
    
    // Validate header
    if (strcmp(state.header, "NESSAVE") != 0) {
        std::cerr << "Error: Invalid save state file" << std::endl;
        return false;
    }
    
    // Restore CPU state
    regA = state.cpu_A;
    regX = state.cpu_X;
    regY = state.cpu_Y;
    regSP = state.cpu_SP;
    regP = state.cpu_P;
    regPC = state.cpu_PC;
    totalCycles = state.cpu_cycles;
    
    // Restore RAM
    memcpy(ram, state.ram, sizeof(ram));
    
    // TODO: Restore PPU and APU state
    
    std::cout << "Emulator save state loaded from: " << actualFilename << std::endl;
    return true;
}

// CHR ROM access for PPU
uint8_t* SMBEmulator::getCHR()
{
    return chrROM;
}

uint8_t SMBEmulator::readData(uint16_t address)
{
    return readByte(address);
}

void SMBEmulator::writeData(uint16_t address, uint8_t value)
{
    writeByte(address, value);
}

// Illegal opcode implementations
void SMBEmulator::ISC(uint16_t addr)
{
    // INC + SBC
    uint8_t value = readByte(addr) + 1;
    writeByte(addr, value);
    
    // Then do SBC
    uint16_t result = regA - value - (getFlag(FLAG_CARRY) ? 0 : 1);
    setFlag(FLAG_CARRY, result <= 0xFF);
    setFlag(FLAG_OVERFLOW, ((regA ^ result) & (~value ^ result) & 0x80) != 0);
    regA = result & 0xFF;
    updateZN(regA);
}

void SMBEmulator::DCP(uint16_t addr)
{
    // DEC + CMP
    uint8_t value = readByte(addr) - 1;
    writeByte(addr, value);
    
    // Then do CMP
    uint8_t result = regA - value;
    setFlag(FLAG_CARRY, regA >= value);
    updateZN(result);
}

void SMBEmulator::LAX(uint16_t addr)
{
    // LDA + LDX
    uint8_t value = readByte(addr);
    regA = value;
    regX = value;
    updateZN(regA);
}

void SMBEmulator::SAX(uint16_t addr)
{
    // Store A & X
    writeByte(addr, regA & regX);
}

void SMBEmulator::SLO(uint16_t addr)
{
    // ASL + ORA
    uint8_t value = readByte(addr);
    setFlag(FLAG_CARRY, (value & 0x80) != 0);
    value <<= 1;
    writeByte(addr, value);
    regA |= value;
    updateZN(regA);
}

void SMBEmulator::RLA(uint16_t addr)
{
    // ROL + AND
    uint8_t value = readByte(addr);
    bool oldCarry = getFlag(FLAG_CARRY);
    setFlag(FLAG_CARRY, (value & 0x80) != 0);
    value = (value << 1) | (oldCarry ? 1 : 0);
    writeByte(addr, value);
    regA &= value;
    updateZN(regA);
}

void SMBEmulator::SRE(uint16_t addr)
{
    // LSR + EOR
    uint8_t value = readByte(addr);
    setFlag(FLAG_CARRY, (value & 0x01) != 0);
    value >>= 1;
    writeByte(addr, value);
    regA ^= value;
    updateZN(regA);
}

void SMBEmulator::RRA(uint16_t addr)
{
    // ROR + ADC
    uint8_t value = readByte(addr);
    bool oldCarry = getFlag(FLAG_CARRY);
    setFlag(FLAG_CARRY, (value & 0x01) != 0);
    value = (value >> 1) | (oldCarry ? 0x80 : 0);
    writeByte(addr, value);
    
    uint16_t result = regA + value + (getFlag(FLAG_CARRY) ? 1 : 0);
    setFlag(FLAG_CARRY, result > 0xFF);
    setFlag(FLAG_OVERFLOW, ((regA ^ result) & (value ^ result) & 0x80) != 0);
    regA = result & 0xFF;
    updateZN(regA);
}

void SMBEmulator::ANC(uint16_t addr)
{
    // AND + set carry to bit 7
    regA &= readByte(addr);
    updateZN(regA);
    setFlag(FLAG_CARRY, (regA & 0x80) != 0);
}

void SMBEmulator::ALR(uint16_t addr)
{
    // AND + LSR
    regA &= readByte(addr);
    setFlag(FLAG_CARRY, (regA & 0x01) != 0);
    regA >>= 1;
    updateZN(regA);
}

void SMBEmulator::ARR(uint16_t addr)
{
    // AND + ROR
    regA &= readByte(addr);
    bool oldCarry = getFlag(FLAG_CARRY);
    setFlag(FLAG_CARRY, (regA & 0x01) != 0);
    regA = (regA >> 1) | (oldCarry ? 0x80 : 0);
    updateZN(regA);
    setFlag(FLAG_OVERFLOW, ((regA >> 6) ^ (regA >> 5)) & 1);
}

void SMBEmulator::XAA(uint16_t addr)
{
    // Unstable - just do AND
    regA &= readByte(addr);
    updateZN(regA);
}

void SMBEmulator::AXS(uint16_t addr)
{
    // (A & X) - immediate
    uint8_t value = readByte(addr);
    uint8_t result = (regA & regX) - value;
    setFlag(FLAG_CARRY, (regA & regX) >= value);
    regX = result;
    updateZN(regX);
}
