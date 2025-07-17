#include <cstring>
#include <cstdint>
#include <string>
#include <fstream>
#include "SMBEmulator.hpp"
#include "../Emulation/APU.hpp"
#include "../Emulation/CycleAccuratePPU.hpp"
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

SMBEmulator::SMBEmulator() {
    ppu = new CycleAccuratePPU(*this);  
    apu = new APU();
    mapper = nullptr;
    controller1 = new Controller();
    controller2 = new Controller();
    
    prgROM = nullptr;  // Changed from prg
    chrROM = nullptr;  // Changed from chr
    prg = nullptr;     // Keep this too if needed
    chr = nullptr;     // Keep this too if needed
    prgSize = 0;
    chrSize = 0;
    romLoaded = false;
    
    mapperNumber = 0;
    hasBattery = false;
    hasTrainer = false;
    verticalMirroring = false;
    
    // Initialize cycle-accurate state
    frameReady = false;
    currentCHRBank = 0;
    memset(renderBuffer, 0, sizeof(renderBuffer));
    
    usingMIDIAudio = false;
    
    // Initialize CPU state
    regA = regX = regY = 0;
    regSP = 0xFD;
    regP = 0x34;
    regPC = 0;
    totalCycles = 0;
    frameCycles = 0;
    
    // Initialize RAM
    memset(ram, 0, sizeof(ram));
    
    // Initialize mapper states
    memset(&mmc1, 0, sizeof(mmc1));
    memset(&mmc3, 0, sizeof(mmc3));
    memset(&cnrom, 0, sizeof(cnrom));
    memset(&gxrom, 0, sizeof(gxrom));
    memset(&uxrom, 0, sizeof(uxrom));
    memset(&nesHeader, 0, sizeof(nesHeader));
}

SMBEmulator::~SMBEmulator() {
    delete cpu;
    delete ppu;
    delete apu;
    delete mapper;
    delete controller1;
    delete controller2;
    
    cleanupROM();
}

void SMBEmulator::writeCNROMRegister(uint16_t address, uint8_t value)
{
    uint8_t oldCHRBank = cnrom.chrBank;
    
    // Mapper 3: Write to $8000-$FFFF sets CHR bank
    cnrom.chrBank = value & 0x03;  // Only 2 bits for CHR bank
    
    // Invalidate cache if CHR bank changed
    /*if (oldCHRBank != cnrom.chrBank) {
        ppu->invalidateTileCache();
    }*/
    
}

void SMBEmulator::writeCHRData(uint16_t address, uint8_t value)
{
    if (address >= 0x2000) return;
    
    // Only allow writes for mappers that use CHR-RAM
    if (nesHeader.mapper == 2) {  // UxROM uses CHR-RAM
        if (address < chrSize) {
            chrROM[address] = value;  // Note: chrROM is actually CHR-RAM for UxROM
        }
    }
    // Add other CHR-RAM mappers here if needed
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
        std::cout << "Invalid NES signature" << std::endl;
        return false;
    }
    
    // Detect iNES format version
    bool isINES2 = false;
    if ((header[7] & 0x0C) == 0x08) {
        isINES2 = true;
        std::cout << "ROM Format: iNES 2.0" << std::endl;
    } else if ((header[7] & 0x0C) == 0x00) {
        // Check if bytes 12-15 are zero (archaic iNES vs iNES 1.0)
        bool hasTrailingZeros = (header[12] == 0 && header[13] == 0 && header[14] == 0 && header[15] == 0);
        if (hasTrailingZeros) {
            std::cout << "ROM Format: iNES 1.0" << std::endl;
        } else {
            std::cout << "ROM Format: Archaic iNES" << std::endl;
        }
    } else {
        std::cout << "ROM Format: Unknown/Invalid" << std::endl;
        return false;
    }
    
    // Parse basic header info
    nesHeader.prgROMPages = header[4];
    nesHeader.chrROMPages = header[5];
    
    // Parse mapper number
    if (isINES2) {
        // iNES 2.0 mapper parsing (12-bit mapper number)
        nesHeader.mapper = (header[6] >> 4) | (header[7] & 0xF0) | ((header[8] & 0x0F) << 8);
    } else {
        // iNES 1.0 mapper parsing (8-bit mapper number)
        nesHeader.mapper = (header[6] >> 4) | (header[7] & 0xF0);
    }
    
    nesHeader.mirroring = header[6] & 0x01;
    nesHeader.battery = (header[6] & 0x02) != 0;
    nesHeader.trainer = (header[6] & 0x04) != 0;
    
    // Print detailed header info
    std::cout << "=== ROM Header Info ===" << std::endl;
    std::cout << "PRG ROM Pages: " << (int)nesHeader.prgROMPages << " (16KB each)" << std::endl;
    std::cout << "CHR ROM Pages: " << (int)nesHeader.chrROMPages << " (8KB each)" << std::endl;
    std::cout << "Mapper: " << (int)nesHeader.mapper << std::endl;
    std::cout << "Mirroring: " << (nesHeader.mirroring ? "Vertical" : "Horizontal") << std::endl;
    std::cout << "Battery: " << (nesHeader.battery ? "Yes" : "No") << std::endl;
    std::cout << "Trainer: " << (nesHeader.trainer ? "Yes" : "No") << std::endl;
    
    // Check for unsupported features
    if (nesHeader.mapper != 0 && nesHeader.mapper != 1 && nesHeader.mapper != 2 && 
        nesHeader.mapper != 3 && nesHeader.mapper != 4 && nesHeader.mapper != 66) {
        std::cout << "WARNING: Mapper " << (int)nesHeader.mapper << " not supported" << std::endl;
    }
    
    // Mapper-specific notes
    switch (nesHeader.mapper) {
        case 0:
            std::cout << "Mapper: NROM (simple)" << std::endl;
            break;
        case 1:
            std::cout << "Mapper: MMC1 (complex)" << std::endl;
            break;
        case 2:
            std::cout << "Mapper: UxROM (Contra, Mega Man, Duck Tales)" << std::endl;
            break;
        case 3:
            std::cout << "Mapper: CNROM (simple CHR banking)" << std::endl;
            break;
        case 4:
            std::cout << "Mapper: MMC3 (complex)" << std::endl;
            break;
        case 66:
            std::cout << "Mapper: GxROM" << std::endl;
            break;
        default:
            std::cout << "Mapper: Unknown/Unsupported (" << (int)nesHeader.mapper << ")" << std::endl;
            break;
    }
    
    if (isINES2) {
        std::cout << "WARNING: iNES 2.0 features not fully implemented" << std::endl;
    }
    
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
        chrSize = 8192;
        chrROM = new uint8_t[chrSize];
        memset(chrROM, 0, chrSize);
        printf("Using CHR RAM (8KB) for mapper %d\n", nesHeader.mapper);
        return true;
    }
    
    chrROM = new uint8_t[chrSize];
    file.read(reinterpret_cast<char*>(chrROM), chrSize);
    
    // Debug output
    printf("Loaded CHR ROM: %d bytes for mapper %d\n", chrSize, nesHeader.mapper);
    
    // CRITICAL DEBUG INFO
    uint32_t totalCHRBanks = chrSize / 0x400;  // Number of 1KB banks
    printf("=== CHR ROM DEBUG ===\n");
    printf("CHR ROM Size: %d bytes (%d KB)\n", chrSize, chrSize/1024);
    printf("Total 1KB CHR banks: %d (0x00 - 0x%02X)\n", totalCHRBanks, totalCHRBanks-1);
    printf("CHR ROM Pages: %d\n", nesHeader.chrROMPages);
    
    // Check if common bank numbers are valid
    printf("Bank validity check:\n");
    for (int bank : {0x00, 0x08, 0x09, 0x0C, 0x10, 0x11, 0x18, 0x19}) {
        bool valid = (bank < totalCHRBanks);
        printf("  Bank 0x%02X: %s\n", bank, valid ? "VALID" : "OUT OF BOUNDS!");
    }
    printf("=== END CHR DEBUG ===\n");
    
    return file.good();
}

void SMBEmulator::handleNMI()
{
    static int nmiCount = 0;
    
    nmiCount++;
    
    // Push PC and status to stack
    pushWord(regPC);
    pushByte(regP & ~FLAG_BREAK);
    setFlag(FLAG_INTERRUPT, true);
    
    // Jump to NMI vector
    regPC = readWord(0xFFFA);
    
    totalCycles += 7;
    frameCycles += 7;
}

void SMBEmulator::setFrameRendered()
{
    frameReady = false;
}


void SMBEmulator::update() {
    if (frameReady) return;
    
    const int CYCLES_PER_FRAME = 29781;
    int frameCyclesExecuted = 0;
    
    while (frameCyclesExecuted < CYCLES_PER_FRAME && !frameReady) {
        // Check for pending IRQ before executing instruction
        if (pendingIRQ && !getFlag(FLAG_INTERRUPT)) {
            pendingIRQ = false;
            handleIRQ();
        }
        
        uint8_t opcode = readByte(regPC);
        uint8_t cpuCycles = instructionCycles[opcode];
        
        executeInstruction();
        
        for (int i = 0; i < cpuCycles * 3; i++) {
            bool frameComplete = ppu->executeCycle(renderBuffer);
            
            // Step MMC3 IRQ counter during PPU rendering
            if (nesHeader.mapper == 4) {
                stepMMC3IRQ();
            }
            
            if (frameComplete) {
                frameReady = true;
                if (apu) {
                    apu->stepFrame();
                }
                break;
            }
        }
        
        frameCyclesExecuted += cpuCycles;
        totalCycles += cpuCycles;
    }
}

void SMBEmulator::reset() {
    // Reset CPU state
    regA = regX = regY = 0;
    regSP = 0xFD;
    regP = 0x34;
    regPC = readWord(0xFFFC);  // Read reset vector
    totalCycles = 0;
    frameCycles = 0;
    
    // Reset PPU state
    if (ppu) {
        // Reset PPU registers to power-up state
        ppu->writeRegister(0x2000, 0x00);  // PPUCTRL
        ppu->writeRegister(0x2001, 0x00);  // PPUMASK
        ppu->writeRegister(0x2003, 0x00);  // OAMADDR
        ppu->writeRegister(0x2005, 0x00);  // PPUSCROLL X
        ppu->writeRegister(0x2005, 0x00);  // PPUSCROLL Y
        
        // Clear nametables
        for (int addr = 0x2000; addr < 0x3000; addr++) {
            ppu->writeByte(addr, 0x00);
        }
        
        // Initialize default palette
        ppu->writeByte(0x3F00, 0x09);  // Universal background (dark blue)
        ppu->writeByte(0x3F01, 0x01);  // Background palette 0, color 1
        ppu->writeByte(0x3F02, 0x00);  // Background palette 0, color 2  
        ppu->writeByte(0x3F03, 0x01);  // Background palette 0, color 3
    }
    
    // Reset APU
    apu->reset();
    
    // Reset cycle-accurate state
    frameReady = false;
    currentCHRBank = 0;
    memset(renderBuffer, 0, sizeof(renderBuffer));

    if (nesHeader.mapper == 4) {
        memset(&mmc3, 0, sizeof(mmc3));
        mmc3.bankSelect = 0;  // Normal PRG mode, normal CHR mode
        
        // Power-up state: all registers start at 0
        for (int i = 0; i < 8; i++) {
            mmc3.bankData[i] = 0;
        }
        
        updateMMC3Banks();
        
    }
    
    // Read reset vector AFTER mapper is initialized
    regPC = readWord(0xFFFC);
}

void SMBEmulator::step()
{
    if (!romLoaded) return;
    executeInstruction();
}

void SMBEmulator::executeInstruction()
{
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

        case 0x93: SHA(addrIndirectY()); break;     // SHA (A&X&H) indirect,Y
        case 0x9F: SHA(addrAbsoluteY()); break;     // SHA (A&X&H) absolute,Y

        // SHX (X & high byte)
        case 0x9E: SHX(addrAbsoluteY()); break;     // SHX (X&H) absolute,Y

        // SHY (Y & high byte)  
        case 0x9C: SHY(addrAbsoluteX()); break;     // SHY (Y&H) absolute,X

        // TAS (A & X -> SP, then A & X & H)
        case 0x9B: TAS(addrAbsoluteY()); break;     // TAS absolute,Y

        // LAS (load A,X,SP from memory & SP)
        case 0xBB: LAS(addrAbsoluteY()); break;     // LAS absolute,Y
        
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

int SMBEmulator::getMirroringMode() const {
    return nesHeader.mirroring ? 0 : 1;  // 0 = vertical, 1 = horizontal
}


// Memory access functions
uint8_t SMBEmulator::readByte(uint16_t address)
{
    if (address < 0x2000) {
        return ram[address & 0x7FF];
    }
    else if (address < 0x4000) {
        uint16_t ppuAddr = 0x2000 + (address & 0x7);
        return ppu->readRegister(ppuAddr);
    }
    else if (address < 0x4020) {
        switch (address) {
            case 0x4016:
                return controller1->readByte(1);
            case 0x4017:
                return controller2->readByte(2);
            default:
                return 0;
        }
    }
    else if (address >= 0x8000) {
        // Add your existing PRG ROM mapping logic here
        return readPRG(address);
    }
    return 0;
}

void SMBEmulator::triggerIRQ() {
    if (!getFlag(FLAG_INTERRUPT)) {  // Only if IRQs not disabled
        pendingIRQ = true;
    }
}

void SMBEmulator::handleIRQ() {
    // Push PC and status to stack
    pushWord(regPC);
    pushByte(regP & ~FLAG_BREAK);
    setFlag(FLAG_INTERRUPT, true);
    
    // Jump to IRQ vector
    regPC = readWord(0xFFFE);
    
    totalCycles += 7;
    frameCycles += 7;
}


void SMBEmulator::writeMMC3Register(uint16_t address, uint8_t value)
{
    switch (address & 0xE001) {
        case 0x8000: // Bank select
            mmc3.bankSelect = value;
            updateMMC3Banks();
            break;
            
        case 0x8001: // Bank data
            {
                uint8_t bank = mmc3.bankSelect & 7;
                mmc3.bankData[bank] = value;
                updateMMC3Banks();
            }
            break;
            
        case 0xA000: // Mirroring
            mmc3.mirroring = value & 1;
            break;
            
        case 0xA001: // PRG RAM protect
            mmc3.prgRamProtect = value;
            break;
            
        case 0xC000: // IRQ latch
            mmc3.irqLatch = value;
            break;
            
        case 0xC001: // IRQ reload
            mmc3.irqReload = true;
            break;
            
        case 0xE000: // IRQ disable
            mmc3.irqEnable = false;
            break;
            
        case 0xE001: // IRQ enable
            mmc3.irqEnable = true;
            break;
    }
}


void SMBEmulator::updateMMC3Banks()
{
    uint8_t totalPRGBanks = prgSize / 0x2000;  // Number of 8KB banks
    uint8_t totalCHRBanks = chrSize / 0x400;   // Number of 1KB banks
    
    // FIXED PRG banking logic
    bool prgSwap = (mmc3.bankSelect & 0x40) != 0;
    
    // Ensure bank numbers are valid
    uint8_t bankR6 = mmc3.bankData[6] % totalPRGBanks;
    uint8_t bankR7 = mmc3.bankData[7] % totalPRGBanks;
    
    if (prgSwap) {
        // PRG mode 1: R6 at $C000, R7 at $A000, fixed banks at $8000/$E000
        mmc3.currentPRGBanks[0] = totalPRGBanks - 2;  // $8000: Second-to-last bank (fixed)
        mmc3.currentPRGBanks[1] = bankR7;             // $A000: R7 (switchable)
        mmc3.currentPRGBanks[2] = bankR6;             // $C000: R6 (switchable)
        mmc3.currentPRGBanks[3] = totalPRGBanks - 1;  // $E000: Last bank (fixed)
    } else {
        // PRG mode 0: R6 at $8000, R7 at $A000, fixed banks at $C000/$E000
        mmc3.currentPRGBanks[0] = bankR6;             // $8000: R6 (switchable)
        mmc3.currentPRGBanks[1] = bankR7;             // $A000: R7 (switchable)  
        mmc3.currentPRGBanks[2] = totalPRGBanks - 2;  // $C000: Second-to-last bank (fixed)
        mmc3.currentPRGBanks[3] = totalPRGBanks - 1;  // $E000: Last bank (fixed)
    }
    
    
    // CHR banking (your existing code is fine)
    bool chrA12Invert = (mmc3.bankSelect & 0x80) != 0;
    
    if (chrA12Invert) {
        mmc3.currentCHRBanks[0] = mmc3.bankData[2] % totalCHRBanks;
        mmc3.currentCHRBanks[1] = mmc3.bankData[3] % totalCHRBanks;
        mmc3.currentCHRBanks[2] = mmc3.bankData[4] % totalCHRBanks;
        mmc3.currentCHRBanks[3] = mmc3.bankData[5] % totalCHRBanks;
        
        uint8_t r0_base = mmc3.bankData[0] & 0xFE;
        mmc3.currentCHRBanks[4] = r0_base % totalCHRBanks;
        mmc3.currentCHRBanks[5] = (r0_base + 1) % totalCHRBanks;
        
        uint8_t r1_base = mmc3.bankData[1] & 0xFE;
        mmc3.currentCHRBanks[6] = r1_base % totalCHRBanks;
        mmc3.currentCHRBanks[7] = (r1_base + 1) % totalCHRBanks;
    } else {
        uint8_t r0_base = mmc3.bankData[0] & 0xFE;
        mmc3.currentCHRBanks[0] = r0_base % totalCHRBanks;
        mmc3.currentCHRBanks[1] = (r0_base + 1) % totalCHRBanks;
        
        uint8_t r1_base = mmc3.bankData[1] & 0xFE;
        mmc3.currentCHRBanks[2] = r1_base % totalCHRBanks;
        mmc3.currentCHRBanks[3] = (r1_base + 1) % totalCHRBanks;
        
        mmc3.currentCHRBanks[4] = mmc3.bankData[2] % totalCHRBanks;
        mmc3.currentCHRBanks[5] = mmc3.bankData[3] % totalCHRBanks;
        mmc3.currentCHRBanks[6] = mmc3.bankData[4] % totalCHRBanks;
        mmc3.currentCHRBanks[7] = mmc3.bankData[5] % totalCHRBanks;
    }
}


void SMBEmulator::stepMMC3IRQ() {
    // MMC3 IRQ counter decrements on A12 rising edges
    // For simplicity, step it during certain PPU cycles
    
    static int irqStepCounter = 0;
    irqStepCounter++;
    
    // Step IRQ counter every few PPU cycles (approximate A12 toggles)
    if (irqStepCounter % 8 == 0) {  // Adjust this timing as needed
        if (mmc3.irqReload) {
            mmc3.irqCounter = mmc3.irqLatch;
            mmc3.irqReload = false;
        } else if (mmc3.irqCounter > 0) {
            mmc3.irqCounter--;
        }
        
        // Trigger IRQ when counter reaches 0 and IRQ is enabled
        if (mmc3.irqCounter == 0 && mmc3.irqEnable) {
            triggerIRQ();
        }
    }
}



void SMBEmulator::writeByte(uint16_t address, uint8_t value)
{
    if (address < 0x2000) {
        ram[address & 0x7FF] = value;
    }
    else if (address < 0x4000) {
        ppu->writeRegister(0x2000 + (address & 0x7), value);
    }
    else if (address < 0x4020) {
        switch (address) {
            case 0x4014:
                ppu->writeDMA(value);
                break;
            case 0x4016:
                controller1->writeByte(value);
                controller2->writeByte(value);
                break;
            default:
                apu->writeRegister(address, value);
                break;
        }
    }
    else if (address >= 0x8000) {
        writePRG(address, value);
    }
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

void SMBEmulator::SHA(uint16_t addr)
{
    // SHA: Store A & X & (high byte of address + 1)
    // This is an unstable instruction - the high byte interaction is complex
    uint8_t highByte = (addr >> 8) + 1;
    uint8_t result = regA & regX & highByte;
    writeByte(addr, result);
}

void SMBEmulator::SHX(uint16_t addr) 
{
    // SHX: Store X & (high byte of address + 1)
    uint8_t highByte = (addr >> 8) + 1;
    uint8_t result = regX & highByte;
    writeByte(addr, result);
}

void SMBEmulator::SHY(uint16_t addr)
{
    // SHY: Store Y & (high byte of address + 1) 
    uint8_t highByte = (addr >> 8) + 1;
    uint8_t result = regY & highByte;
    writeByte(addr, result);
}

void SMBEmulator::TAS(uint16_t addr)
{
    // TAS: Transfer A & X to SP, then store A & X & (high byte + 1)
    regSP = regA & regX;
    uint8_t highByte = (addr >> 8) + 1;
    uint8_t result = regA & regX & highByte;
    writeByte(addr, result);
}

void SMBEmulator::LAS(uint16_t addr)
{
    // LAS: Load A, X, and SP with memory value & SP
    uint8_t value = readByte(addr);
    uint8_t result = value & regSP;
    regA = result;
    regX = result;
    regSP = result;
    updateZN(result);
}

// Rendering functions (delegate to PPU)
void SMBEmulator::render(uint32_t* buffer)
{
    ppu->render(buffer);
}

void SMBEmulator::clearRenderBuffer() {
    if (renderBuffer && ppu) {
        uint16_t bgColor = ppu->getBackgroundColor16();
        for (int i = 0; i < 256 * 240; i++) {
            renderBuffer[i] = bgColor;
        }
    }
}

void SMBEmulator::render16(uint16_t* buffer) {
    // Since renderBuffer is now updated directly by the PPU,
    // just copy it when frame is ready
    if (frameReady && buffer) {
        memcpy(buffer, renderBuffer, 256 * 240 * sizeof(uint16_t));
    } 
}

void SMBEmulator::renderDirectFast(uint16_t* buffer, int screenWidth, int screenHeight)
{
    ppu->renderScaled(buffer, screenWidth, screenHeight);
}

void SMBEmulator::renderScaled16(uint16_t* buffer, int screenWidth, int screenHeight) {
    if (!frameReady || !buffer) return;
    // Calculate scaling
    int scale_x = screenWidth / 256;
    int scale_y = screenHeight / 240;
    int scale = (scale_x < scale_y) ? scale_x : scale_y;
    if (scale < 1) scale = 1;
    
    int dest_w = 256 * scale;
    int dest_h = 240 * scale;
    int dest_x = (screenWidth - dest_w) / 2;
    int dest_y = (screenHeight - dest_h) / 2;
    
    // Only clear letterbox areas if they exist
    if (dest_x > 0 || dest_y > 0) {
        // Clear borders only
        for (int y = 0; y < screenHeight; y++) {
            for (int x = 0; x < screenWidth; x++) {
                if (x < dest_x || x >= dest_x + dest_w || 
                    y < dest_y || y >= dest_y + dest_h) {
                    buffer[y * screenWidth + x] = 0x0000;
                }
            }
        }
    }
    
    // Scale the pixels from renderBuffer to output buffer
    if (scale == 1) {
        for (int y = 0; y < 240; y++) {
            int screen_y = y + dest_y;
            if (screen_y >= 0 && screen_y < screenHeight) {
                uint16_t* src_row = &renderBuffer[y * 256];
                uint16_t* dest_row = &buffer[screen_y * screenWidth + dest_x];
                int copy_width = (dest_x + 256 > screenWidth) ? screenWidth - dest_x : 256;
                if (copy_width > 0) {
                    memcpy(dest_row, src_row, copy_width * sizeof(uint16_t));
                }
            }
        }
    } else if (scale == 2) {
        for (int y = 0; y < 240; y++) {
            int dest_y1 = y * 2 + dest_y;
            int dest_y2 = dest_y1 + 1;
            
            if (dest_y2 >= screenHeight) break;
            if (dest_y1 < 0) continue;
            
            uint16_t* src_row = &renderBuffer[y * 256];
            uint16_t* dest_row1 = &buffer[dest_y1 * screenWidth + dest_x];
            uint16_t* dest_row2 = &buffer[dest_y2 * screenWidth + dest_x];
            
            for (int x = 0; x < 256; x++) {
                if ((x * 2 + dest_x + 1) >= screenWidth) break;
                
                uint16_t pixel = src_row[x];
                int dest_base = x * 2;
                
                dest_row1[dest_base] = dest_row1[dest_base + 1] = pixel;
                dest_row2[dest_base] = dest_row2[dest_base + 1] = pixel;
            }
        }
    } else {
        for (int y = 0; y < 240; y++) {
            uint16_t* src_row = &renderBuffer[y * 256];
            
            for (int scale_y = 0; scale_y < scale; scale_y++) {
                int dest_row_idx = y * scale + scale_y + dest_y;
                if (dest_row_idx >= screenHeight || dest_row_idx < 0) continue;
                
                uint16_t* dest_row = &buffer[dest_row_idx * screenWidth];
                
                for (int x = 0; x < 256; x++) {
                    uint16_t pixel = src_row[x];
                    
                    for (int scale_x = 0; scale_x < scale; scale_x++) {
                        int dest_col = x * scale + scale_x + dest_x;
                        if (dest_col >= 0 && dest_col < screenWidth) {
                            dest_row[dest_col] = pixel;
                        }
                    }
                }
            }
        }
    }
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

uint8_t SMBEmulator::readData(uint16_t address) {
    // CPU memory map
    if (address < 0x2000) {
        // RAM (with mirroring)
        return ram[address & 0x7FF];
    } else if (address < 0x4000) {
        // PPU registers (with mirroring)
        return ppu->readRegister(0x2000 + (address & 0x7));  // CHANGED: readRegister instead of readPPURegister
    } else if (address == 0x4014) {
        // OAM DMA - write only
        return 0;
    } else if (address == 0x4016) {
        // Controller 1 - use readByte method instead of read
        return controller1->readByte(1);
    } else if (address == 0x4017) {
        // Controller 2 - use readByte method instead of read
        return controller2->readByte(2);
    } else if (address >= 0x4000 && address < 0x4020) {
        // APU registers
        return apu->readRegister(address);
    } else if (address >= 0x8000) {
        // PRG ROM
        return readPRG(address);
    }
    
    return 0;
}

void SMBEmulator::writeData(uint16_t address, uint8_t value) {
    // CPU memory map
    if (address < 0x2000) {
        // RAM (with mirroring)
        ram[address & 0x7FF] = value;
    } else if (address < 0x4000) {
        // PPU registers (with mirroring)
        ppu->writeRegister(0x2000 + (address & 0x7), value);  // CHANGED: writeRegister instead of writePPURegister
    } else if (address == 0x4014) {
        // OAM DMA
        ppu->writeDMA(value);  // CHANGED: writeDMA instead of writePPUDMA
    } else if (address == 0x4016) {
        // Controller strobe - use writeByte method instead of write
        controller1->writeByte(value);
        controller2->writeByte(value);
    } else if (address >= 0x4000 && address < 0x4020) {
        // APU registers
        apu->writeRegister(address, value);
    } else if (address >= 0x8000) {
        // PRG ROM area - let mapper handle it
        writePRG(address, value);
    }
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

void SMBEmulator::writeMMC1Register(uint16_t address, uint8_t value)
{
    // MMC1 uses a shift register for writes
    if (value & 0x80) {
        // Reset shift register
        mmc1.shiftRegister = 0x10;
        mmc1.shiftCount = 0;
        mmc1.control |= 0x0C;  // Set to 16KB PRG mode
        return;
    }
    
    // Shift in the bit
    mmc1.shiftRegister >>= 1;
    mmc1.shiftRegister |= (value & 1) << 4;
    mmc1.shiftCount++;
    
    // After 5 writes, update the appropriate register
    if (mmc1.shiftCount == 5) {
        uint8_t data = mmc1.shiftRegister;
        mmc1.shiftRegister = 0x10;
        mmc1.shiftCount = 0;
        
        if (address < 0xA000) {
            // Control register ($8000-$9FFF)
            mmc1.control = data;
        } else if (address < 0xC000) {
            // CHR bank 0 ($A000-$BFFF)
            mmc1.chrBank0 = data;
        } else if (address < 0xE000) {
            // CHR bank 1 ($C000-$DFFF)  
            mmc1.chrBank1 = data;
        } else {
            // PRG bank ($E000-$FFFF)
            mmc1.prgBank = data;
        }
        
        updateMMC1Banks();
    }
}

void SMBEmulator::updateMMC1Banks()
{
    // Update PRG banking
    if (mmc1.control & 0x08) {
        // 16KB PRG mode
        mmc1.currentPRGBank = mmc1.prgBank & 0x0F;
    } else {
        // 32KB PRG mode - ignore low bit
        mmc1.currentPRGBank = (mmc1.prgBank >> 1) & 0x07;
    }
    
    // Update CHR banking
    uint8_t oldCHRBank0 = mmc1.currentCHRBank0;
    uint8_t oldCHRBank1 = mmc1.currentCHRBank1;
    
    if (mmc1.control & 0x10) {
        // 4KB CHR mode
        mmc1.currentCHRBank0 = mmc1.chrBank0;
        mmc1.currentCHRBank1 = mmc1.chrBank1;
    } else {
        // 8KB CHR mode - ignore low bit of chrBank0
        mmc1.currentCHRBank0 = mmc1.chrBank0 & 0xFE;
        mmc1.currentCHRBank1 = mmc1.currentCHRBank0 + 1;
    }
    
    // INVALIDATE CACHES IF CHR BANKS CHANGED
    /*if (oldCHRBank0 != mmc1.currentCHRBank0 || oldCHRBank1 != mmc1.currentCHRBank1) {
        ppu->invalidateTileCache();
    }*/
    
}

void SMBEmulator::writeGxROMRegister(uint16_t address, uint8_t value) {
    uint8_t oldCHRBank = gxrom.chrBank;
    uint8_t oldPRGBank = gxrom.prgBank;
    
    // Mapper 66: Write to $8000-$FFFF sets both PRG and CHR banks
    gxrom.prgBank = (value >> 4) & 0x03;  // Bits 4-5 (2 bits = 4 banks)
    gxrom.chrBank = value & 0x03;         // Bits 0-1 (2 bits = 4 banks)
        
}


uint8_t SMBEmulator::readCHRData(uint16_t address)
{
    if (address >= 0x2000) return 0;
    
    if (nesHeader.mapper == 4) {
        // MMC3 CHR banking
        uint8_t bankIndex = address / 0x400;  // Which 1KB bank (0-7)
        uint16_t bankOffset = address % 0x400; // Offset within bank
        uint8_t physicalBank = mmc3.currentCHRBanks[bankIndex];
        
        // CRITICAL: Check bounds before accessing
        uint8_t totalCHRBanks = chrSize / 0x400;
        if (physicalBank >= totalCHRBanks) {
            static int errorCount = 0;
            if (errorCount < 10) {
                errorCount++;
            }
            return 0;
        }
        
        uint32_t chrAddr = (physicalBank * 0x400) + bankOffset;
        
        if (chrAddr < chrSize) {
            return chrROM[chrAddr];
        } else {
            printf("CHR address out of bounds: $%08X >= $%08X\n", chrAddr, chrSize);
            return 0;
        }
    }
    else if (nesHeader.mapper == 66) {
        uint32_t chrAddr = (gxrom.chrBank * 0x2000) + address;
        
        if (chrAddr < chrSize) {
            return chrROM[chrAddr];
        }        
        return 0;  // Out of bounds
    }
    else if (nesHeader.mapper == 1) {
        // MMC1 - your existing code
        if (mmc1.control & 0x10) {
            if (address < 0x1000) {
                uint32_t chrAddr = (mmc1.currentCHRBank0 * 0x1000) + address;
                if (chrAddr < chrSize) {
                    return chrROM[chrAddr];
                }
            } else {
                uint32_t chrAddr = (mmc1.currentCHRBank1 * 0x1000) + (address - 0x1000);
                if (chrAddr < chrSize) {
                    return chrROM[chrAddr];
                }
            }
        } else {
            uint32_t chrAddr = (mmc1.currentCHRBank0 * 0x2000) + address;
            if (chrAddr < chrSize) {
                return chrROM[chrAddr];
            }
        }
    }
    else if (nesHeader.mapper == 0) {
        // NROM
        if (address < chrSize) {
            return chrROM[address];
        }
    }
    else if (nesHeader.mapper == 3) {
        // CNROM
        uint32_t chrAddr = (cnrom.chrBank * 0x2000) + address;
        if (chrAddr < chrSize) {
            return chrROM[chrAddr];
        }
    }
    else if (nesHeader.mapper == 2) {
        // UxROM - CHR-RAM
        if (address < chrSize) {
            return chrROM[address];
        }
    }
    
    return 0;
}

uint8_t SMBEmulator::readCHRDataFromBank(uint16_t address, uint8_t bank) {

    if (address < chrSize) {
        return chr[address];
    }
    
    return 0;
}

void SMBEmulator::writeUxROMRegister(uint16_t address, uint8_t value)
{
    // UxROM: Any write to $8000-$FFFF sets the PRG bank
    // Only the lower bits are used (depends on ROM size)
    uint8_t totalBanks = prgSize / 0x4000;  // Number of 16KB banks
    uint8_t bankMask = totalBanks - 1;      // Create mask for valid banks
    
    uxrom.prgBank = value & bankMask;
    
    // Debug output
    //printf("UxROM: Set PRG bank to %d (total banks: %d)\n", uxrom.prgBank, totalBanks);
}

void SMBEmulator::cleanupROM() {
    if (prgROM) {
        delete[] prgROM;
        prgROM = nullptr;
    }
    if (chrROM) {
        delete[] chrROM;
        chrROM = nullptr;
    }
    if (prg) {
        delete[] prg;
        prg = nullptr;
    }
    if (chr) {
        delete[] chr;
        chr = nullptr;
    }
    prgSize = chrSize = 0;
    romLoaded = false;
}

uint8_t SMBEmulator::readPRG(uint16_t address) {
    if (address >= 0x8000) {
        if (nesHeader.mapper == 0) {
            // NROM
            uint32_t romAddr = address - 0x8000;
            if (prgSize == 16384) {
                romAddr &= 0x3FFF;  // Mirror for 16KB ROMs
            }
            if (romAddr < prgSize) {
                return prgROM[romAddr];
            }
        }
        else if (nesHeader.mapper == 1) {
           // MMC1 - COMPLETE IMPLEMENTATION
            uint32_t romAddr = address - 0x8000;
            uint8_t totalBanks = prgSize / 0x4000;  // Number of 16KB banks
            
            if (mmc1.control & 0x08) {
                // 16KB PRG mode
                if (address < 0xC000) {
                    // $8000-$BFFF: Switchable bank OR fixed first bank
                    uint8_t bank;
                    if (mmc1.control & 0x04) {
                        // Fix first bank at $8000, switch at $C000
                        bank = 0;
                    } else {
                        // Switch at $8000, fix last at $C000
                        bank = mmc1.currentPRGBank;
                    }
                    
                    uint32_t prgAddr = (bank * 0x4000) + (address - 0x8000);
                    if (prgAddr < prgSize) {
                        return prgROM[prgAddr];
                    }
                } else {
                    // $C000-$FFFF: Switchable bank OR fixed last bank
                    uint8_t bank;
                    if (mmc1.control & 0x04) {
                        // Fix first bank at $8000, switch at $C000
                        bank = mmc1.currentPRGBank;
                    } else {
                        // Switch at $8000, fix last at $C000
                        bank = totalBanks - 1;  // Last bank
                    }
                    
                    uint32_t prgAddr = (bank * 0x4000) + (address - 0xC000);
                    if (prgAddr < prgSize) {
                        return prgROM[prgAddr];
                    }
                }
            } else {
                // 32KB PRG mode - ignore low bit of bank number
                uint8_t bank32 = mmc1.currentPRGBank & 0xFE;  // Force even bank
                uint32_t prgAddr = (bank32 * 0x4000) + romAddr;
                if (prgAddr < prgSize) {
                    return prgROM[prgAddr];
                }
            }
            
 }
        else if (nesHeader.mapper == 2) {
            // UxROM - your existing code
            uint32_t romAddr = address - 0x8000;
            if (address < 0xC000) {
                // $8000-$BFFF: Switchable bank
                romAddr = (uxrom.prgBank * 0x4000) + (address - 0x8000);
            } else {
                // $C000-$FFFF: Fixed to last bank
                uint8_t lastBank = (prgSize / 0x4000) - 1;
                romAddr = (lastBank * 0x4000) + (address - 0xC000);
            }
            if (romAddr < prgSize) {
                return prgROM[romAddr];
            }
        }
        else if (nesHeader.mapper == 3) {
            // CNROM - your existing code
            uint32_t romAddr = address - 0x8000;
            if (prgSize == 16384) {
                romAddr &= 0x3FFF;
            }
            if (romAddr < prgSize) {
                return prgROM[romAddr];
            }
        }
        else if (nesHeader.mapper == 4) {
            // MMC3 - FIXED VERSION
            uint32_t romAddr = address - 0x8000;
            uint8_t bankIndex = romAddr / 0x2000;  // 0-3 for $8000-$FFFF
            uint16_t offset = romAddr % 0x2000;    // Offset within 8KB bank
            
            if (bankIndex >= 4) {
                printf("Invalid MMC3 bank index: %d\n", bankIndex);
                return 0;
            }
            
            uint8_t physicalBank = mmc3.currentPRGBanks[bankIndex];
            uint32_t prgAddr = (physicalBank * 0x2000) + offset;
            
            if (prgAddr < prgSize) {
                return prgROM[prgAddr];
            } else {
                return 0;
            }
        }
        else if (nesHeader.mapper == 66) {
            // MAPPER 66 (GxROM) - ADD THIS CASE
            uint32_t romAddr = address - 0x8000;  // $8000-$FFFF = 32KB
            
            // GxROM has 32KB PRG banks
            uint32_t prgAddr = (gxrom.prgBank * 0x8000) + romAddr;
            
            if (prgAddr < prgSize) {
                return prgROM[prgAddr];
            }
            
            printf("GxROM PRG read out of bounds: bank %d, addr $%04X, prgAddr $%08X, prgSize %d\n",
                   gxrom.prgBank, address, prgAddr, prgSize);
        }
    }
    return 0;
}

void SMBEmulator::writePRG(uint16_t address, uint8_t value) {
    // Handle mapper writes
    if (address >= 0x8000) {
        if (nesHeader.mapper == 1) {
            writeMMC1Register(address, value);
        } else if (nesHeader.mapper == 2) {
            writeUxROMRegister(address, value);
        } else if (nesHeader.mapper == 3) {
            writeCNROMRegister(address, value);
        } else if (nesHeader.mapper == 4) {
            writeMMC3Register(address, value);
        } else if (nesHeader.mapper == 66) {
            writeGxROMRegister(address, value);
        }
    }
}

