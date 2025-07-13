#include <cstring>
#include "../Configuration.hpp"
#include "../Emulation/APU.hpp"
#include "../Emulation/Controller.hpp"
#include "../Emulation/PPU.hpp"
#include "SMBEngine.hpp"

#define DATA_STORAGE_OFFSET 0x8000 // Starting address for storing constant data

//---------------------------------------------------------------------
// Public interface
//---------------------------------------------------------------------

uint8_t i;
uint8_t d;
uint8_t b;
uint8_t v;

SMBEngine::SMBEngine(uint8_t* romImage) :
    a(*this, &registerA),
    x(*this, &registerX),
    y(*this, &registerY),
    s(*this, &registerS)
{
    apu = new APU();
    ppu = new PPU(*this);
    controller1 = new Controller(1);  // Player 1
    controller2 = new Controller(2);  // Player 2

    // CHR Location in ROM: Header (16 bytes) + 2 PRG pages (16k each)
    chr = (romImage + 16 + (16384 * 2));

    returnIndexStackTop = 0;
}

SMBEngine::~SMBEngine()
{
    delete apu;
    delete ppu;
    delete controller1;
    delete controller2;
}

void SMBEngine::audioCallback(uint8_t* stream, int length)
{
    apu->output(stream, length);
}

Controller& SMBEngine::getController1()
{
    return *controller1;
}

Controller& SMBEngine::getController2()
{
    return *controller2;
}

void SMBEngine::render(uint32_t* buffer)
{
    ppu->render(buffer);
}

void SMBEngine::render16(uint16_t* buffer)
{
    ppu->render16(buffer);  // Direct 16-bit, no conversion
}

/*void SMBEngine::render16(uint16_t* buffer)
{
    // Temporary 32-bit buffer for existing render method
    static uint32_t tempBuffer[256 * 240];
    
    // Use existing 32-bit render
    ppu->render(tempBuffer);
    
    // Convert to 16-bit RGB565 format
    for (int i = 0; i < 256 * 240; i++) {
        uint32_t pixel32 = tempBuffer[i];
        
        // Extract RGB components from 32-bit ARGB
        uint8_t r = (pixel32 >> 16) & 0xFF;
        uint8_t g = (pixel32 >> 8) & 0xFF;
        uint8_t b = pixel32 & 0xFF;
        
        // Convert to 16-bit RGB565 format
        buffer[i] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    }
}*/

void SMBEngine::renderDirect(uint16_t* buffer, int screenWidth, int screenHeight, int scale)
{
    // Get the base 16-bit NES frame
    static uint16_t nesBuffer[256 * 240];
    
    // Use our render16 method instead of calling PPU directly
    render16(nesBuffer);
    
    // Calculate scaling and centering
    int nesWidth = 256;
    int nesHeight = 240;
    
    // Determine the best scale that fits in the screen
    int maxScaleX = screenWidth / nesWidth;
    int maxScaleY = screenHeight / nesHeight;
    int actualScale = (maxScaleX < maxScaleY) ? maxScaleX : maxScaleY;
    
    // Use the requested scale if it fits, otherwise use the maximum
    if (scale > 0 && scale <= actualScale) {
        actualScale = scale;
    }
    
    // If no scaling fits, default to 1
    if (actualScale < 1) actualScale = 1;
    
    // Calculate the final dimensions and centering offset
    int finalWidth = nesWidth * actualScale;
    int finalHeight = nesHeight * actualScale;
    int offsetX = (screenWidth - finalWidth) / 2;
    int offsetY = (screenHeight - finalHeight) / 2;
    
    // Clear the screen buffer
    memset(buffer, 0, screenWidth * screenHeight * sizeof(uint16_t));
    
    // Copy and scale the NES buffer to the screen buffer
    if (actualScale == 1) {
        // No scaling - direct copy (fastest)
        for (int y = 0; y < nesHeight && (y + offsetY) < screenHeight; y++) {
            if (y + offsetY < 0) continue;
            
            uint16_t* srcRow = &nesBuffer[y * nesWidth];
            uint16_t* destRow = &buffer[(y + offsetY) * screenWidth + offsetX];
            
            int copyWidth = nesWidth;
            if (offsetX + copyWidth > screenWidth) {
                copyWidth = screenWidth - offsetX;
            }
            if (offsetX < 0) {
                srcRow -= offsetX;
                destRow -= offsetX;
                copyWidth += offsetX;
            }
            
            if (copyWidth > 0) {
                memcpy(destRow, srcRow, copyWidth * sizeof(uint16_t));
            }
        }
    } else {
        // Scaling required - pixel replication
        for (int y = 0; y < nesHeight; y++) {
            int destY = y * actualScale + offsetY;
            if (destY < 0 || destY >= screenHeight) continue;
            
            uint16_t* srcRow = &nesBuffer[y * nesWidth];
            
            // Replicate this row actualScale times
            for (int scaleY = 0; scaleY < actualScale; scaleY++) {
                int currentDestY = destY + scaleY;
                if (currentDestY >= screenHeight) break;
                
                uint16_t* destRow = &buffer[currentDestY * screenWidth + offsetX];
                
                // Replicate each pixel horizontally
                for (int x = 0; x < nesWidth; x++) {
                    int destX = x * actualScale;
                    if (destX + offsetX >= screenWidth) break;
                    
                    uint16_t pixel = srcRow[x];
                    
                    // Replicate the pixel actualScale times horizontally
                    for (int scaleX = 0; scaleX < actualScale; scaleX++) {
                        int currentDestX = destX + scaleX;
                        if (currentDestX < nesWidth * actualScale) {
                            destRow[currentDestX] = pixel;
                        }
                    }
                }
            }
        }
    }
}

void SMBEngine::renderDirectFast(uint16_t* buffer, int screenWidth, int screenHeight)
{
    // Fast path: render directly to 256x240 area of the buffer
    // Assumes buffer is at least 256x240 and we want to render at top-left
    
    const int nesWidth = 256;
    const int nesHeight = 240;
    
    if (screenWidth >= nesWidth && screenHeight >= nesHeight) {
        // Calculate centering offset
        int offsetX = (screenWidth - nesWidth) / 2;
        int offsetY = (screenHeight - nesHeight) / 2;
        
        // Create a temporary buffer for the NES frame
        uint16_t nesBuffer[nesWidth * nesHeight];
        render16(nesBuffer);
        
        // Clear screen
        memset(buffer, 0, screenWidth * screenHeight * sizeof(uint16_t));
        
        // Copy NES buffer to center of screen buffer
        for (int y = 0; y < nesHeight; y++) {
            uint16_t* srcRow = &nesBuffer[y * nesWidth];
            uint16_t* destRow = &buffer[(y + offsetY) * screenWidth + offsetX];
            memcpy(destRow, srcRow, nesWidth * sizeof(uint16_t));
        }
    } else {
        // Fallback to scaled rendering if screen is too small
        renderDirect(buffer, screenWidth, screenHeight, 1);
    }
}

void SMBEngine::reset()
{
    // Run the decompiled code for initialization
    code(0);
}

void SMBEngine::update()
{
    // Run the decompiled code for the NMI handler
    code(1);

    // Update the APU
    if (Configuration::getAudioEnabled())
    {
        apu->stepFrame();
    }
}

//---------------------------------------------------------------------
// Private methods
//---------------------------------------------------------------------

void SMBEngine::compare(uint8_t value1, uint8_t value2)
{
    uint8_t result = value1 - value2;
    c = (value1 >= value2);
    setZN(result);
}

void SMBEngine::bit(uint8_t value)
{
    n = (value & (1 << 7)) != 0;
    z = (registerA & value) == 0;
}

uint8_t* SMBEngine::getCHR()
{
    return chr;
}

uint8_t* SMBEngine::getDataPointer(uint16_t address)
{
    // Constant data
    if( address >= DATA_STORAGE_OFFSET )
    {
        return dataStorage + (address - DATA_STORAGE_OFFSET);
    }
    // RAM and Mirrors
    else if( address < 0x2000 )
    {
        return ram + (address & 0x7ff);
    }

    return nullptr;
}

MemoryAccess SMBEngine::getMemory(uint16_t address)
{
    uint8_t* dataPointer = getDataPointer(address);
    if( dataPointer != nullptr )
    {
        return MemoryAccess(*this, dataPointer);
    }
    else
    {
        return MemoryAccess(*this, readData(address));
    }
}

uint16_t SMBEngine::getMemoryWord(uint8_t address)
{
    return (uint16_t)readData(address) + ((uint16_t)(readData(address + 1)) << 8);
}

void SMBEngine::pha()
{
    writeData(0x100 | (uint16_t)registerS, registerA);
    registerS--;
}

void SMBEngine::pla()
{
    registerS++;
    a = readData(0x100 | (uint16_t)registerS);
}

int SMBEngine::popReturnIndex()
{
    return returnIndexStack[returnIndexStackTop--];
}

void SMBEngine::pushReturnIndex(int index)
{
    returnIndexStack[++returnIndexStackTop] = index;
}

uint8_t SMBEngine::readData(uint16_t address)
{
    // Constant data
    if( address >= DATA_STORAGE_OFFSET )
    {
        return dataStorage[address - DATA_STORAGE_OFFSET];
    }
    // RAM and Mirrors
    else if( address < 0x2000 )
    {
        return ram[address & 0x7ff];
    }
    // PPU Registers and Mirrors
    else if( address < 0x4000 )
    {
        return ppu->readRegister(0x2000 + (address & 0x7));
    }
    // IO registers
    else if( address < 0x4020 )
    {
        switch (address)
        {
        case 0x4016:
            return controller1->readByte(PLAYER_1);
        case 0x4017:
            return controller2->readByte(PLAYER_2);
        }
    }

    return 0;
}

// Push processor status to stack
void SMBEngine::php() {
    // Construct status byte from individual flags
    uint8_t status = 0;
    if (c) status |= 0x01;  // Carry flag
    if (z) status |= 0x02;  // Zero flag
    if (i) status |= 0x04;  // Interrupt disable
    if (d) status |= 0x08;  // Decimal mode
    if (b) status |= 0x10;  // Break command
    status |= 0x20;         // Bit 5 is always set
    if (v) status |= 0x40;  // Overflow flag
    if (n) status |= 0x80;  // Negative flag
    
    writeData(0x100 | (uint16_t)registerS, status);
    registerS--;
}

// Pull processor status from stack
void SMBEngine::plp() {
    registerS++;
    uint8_t status = readData(0x100 | (uint16_t)registerS);
    
    // Unpack status byte into flags
    c = (status & 0x01) != 0;  // Carry flag
    z = (status & 0x02) != 0;  // Zero flag
    i = (status & 0x04) != 0;  // Interrupt disable
    d = (status & 0x08) != 0;  // Decimal mode
    b = (status & 0x10) != 0;  // Break command
    // Bit 5 is ignored
    v = (status & 0x40) != 0;  // Overflow flag
    n = (status & 0x80) != 0;  // Negative flag
}

void SMBEngine::setZN(uint8_t value)
{
    z = (value == 0);
    n = (value & (1 << 7)) != 0;
}

void SMBEngine::writeData(uint16_t address, uint8_t value)
{
    // RAM and Mirrors
    if( address < 0x2000 )
    {
        ram[address & 0x7ff] = value;
    }
    // PPU Registers and Mirrors
    else if( address < 0x4000 )
    {
        ppu->writeRegister(0x2000 + (address & 0x7), value);
    }
    // IO registers
    else if( address < 0x4020 )
    {
        switch( address )
        {
        case 0x4014:
            ppu->writeDMA(value);
            break;
        case 0x4016:
            controller1->writeByte(value);
            controller2->writeByte(value);  // Both controllers get the latch signal
            break;
        default:
            apu->writeRegister(address, value);
            break;
        }
    }
}

void SMBEngine::writeData(uint16_t address, const uint8_t* data, size_t length)
{
    address -= DATA_STORAGE_OFFSET;

    memcpy(dataStorage + (std::ptrdiff_t)address, data, length);
}

void SMBEngine::toggleAudioMode()
{
    if (apu) {
        apu->toggleAudioMode();
    }
}

bool SMBEngine::isUsingMIDIAudio() const
{
    if (apu) {
        return apu->isUsingMIDI();
    }
    return false;
}

void SMBEngine::debugAudioChannels()
{
    if (apu) {
        apu->debugAudio();
    }
}

void SMBEngine::saveState(const std::string& filename) {
    SaveState state;
    
    // Set header and version
    strcpy(state.header, "SMBSAVE");
    state.version = 2;  // Version 2 only - complete save state
    
    // Save CPU registers
    state.registerA = this->registerA;
    state.registerX = this->registerX;
    state.registerY = this->registerY;
    state.registerS = this->registerS;
    
    // Save CPU flags
    state.c = this->c;
    state.z = this->z;
    state.n = this->n;
    
    // Save global flags
    state.i = ::i;
    state.d = ::d;
    state.b = ::b;
    state.v = ::v;
    
    // Save call stack
    memcpy(state.returnIndexStack, this->returnIndexStack, sizeof(this->returnIndexStack));
    state.returnIndexStackTop = this->returnIndexStackTop;
    
    // Save 2KB RAM
    memcpy(state.ram, this->ram, sizeof(this->ram));
    
    // Save PPU state using the getter methods (uncomment after adding methods to PPU)
    if (ppu) {
        memcpy(state.nametable, ppu->getVRAM(), 2048);
        memcpy(state.oam, ppu->getOAM(), 256);
        memcpy(state.palette, ppu->getPaletteRAM(), 32);
        
        state.ppuCtrl = ppu->getControl();
        state.ppuMask = ppu->getMask();
        state.ppuStatus = ppu->getStatus();
        state.oamAddress = ppu->getOAMAddr();
        state.ppuScrollX = ppu->getScrollX();
        state.ppuScrollY = ppu->getScrollY();
        
        state.currentAddress = ppu->getVRAMAddress();
        state.writeToggle = ppu->getWriteToggle();
        state.vramBuffer = ppu->getDataBuffer();
    } else {
        // Fallback if PPU is null
        memset(state.nametable, 0, sizeof(state.nametable));
        memset(state.oam, 0, sizeof(state.oam));
        memset(state.palette, 0, sizeof(state.palette));
        state.ppuCtrl = 0;
        state.ppuMask = 0;
        state.ppuStatus = 0;
        state.oamAddress = 0;
        state.ppuScrollX = 0;
        state.ppuScrollY = 0;
        state.currentAddress = 0;
        state.writeToggle = false;
        state.vramBuffer = 0;
    }
    
    // Clear reserved space
    memset(state.reserved, 0, sizeof(state.reserved));
    
    // Create appropriate filename based on platform
    std::string actualFilename;
    #ifdef __DJGPP__
        // DOS 8.3 format - convert filename
        std::string baseName = filename;
        size_t dotPos = baseName.find_last_of('.');
        if (dotPos != std::string::npos) {
            baseName = baseName.substr(0, dotPos);
        }
        // Truncate to 8 characters max
        if (baseName.length() > 8) {
            baseName = baseName.substr(0, 8);
        }
        actualFilename = baseName + ".SAV";
    #else
        // Linux/Windows - use filename as-is
        actualFilename = filename;
    #endif
    
    // Write to file
    std::ofstream file(actualFilename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file for saving: " << actualFilename << std::endl;
        return;
    }
    
    file.write(reinterpret_cast<const char*>(&state), sizeof(SaveState));
    file.close();
    
    if (file.good()) {
        std::cout << "Complete save state written to: " << actualFilename << std::endl;
    } else {
        std::cerr << "Error: Failed to write save state to: " << actualFilename << std::endl;
    }
}

bool SMBEngine::loadState(const std::string& filename) {
    // Create appropriate filename based on platform
    std::string actualFilename;
    #ifdef __DJGPP__
        // DOS 8.3 format - convert filename
        std::string baseName = filename;
        size_t dotPos = baseName.find_last_of('.');
        if (dotPos != std::string::npos) {
            baseName = baseName.substr(0, dotPos);
        }
        // Truncate to 8 characters max
        if (baseName.length() > 8) {
            baseName = baseName.substr(0, 8);
        }
        actualFilename = baseName + ".SAV";
    #else
        // Linux/Windows - use filename as-is
        actualFilename = filename;
    #endif

    std::ifstream file(actualFilename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file for loading: " << actualFilename << std::endl;
        return false;
    }
    
    SaveState state;
    file.read(reinterpret_cast<char*>(&state), sizeof(SaveState));
    
    if (!file.good()) {
        std::cerr << "Error: Failed to read save state from: " << actualFilename << std::endl;
        file.close();
        return false;
    }
    
    file.close();
    
    // Validate header
    if (strcmp(state.header, "SMBSAVE") != 0) {
        std::cerr << "Error: Invalid save state file (bad header): " << actualFilename << std::endl;
        return false;
    }
    
    // Check version - only support version 2
    if (state.version != 2) {
        std::cerr << "Error: Unsupported save state version: " << state.version << std::endl;
        return false;
    }
    
    // Restore CPU registers
    this->registerA = state.registerA;
    this->registerX = state.registerX;
    this->registerY = state.registerY;
    this->registerS = state.registerS;
    
    // Restore CPU flags
    this->c = state.c;
    this->z = state.z;
    this->n = state.n;
    
    // Restore global flags
    ::i = state.i;
    ::d = state.d;
    ::b = state.b;
    ::v = state.v;
    
    // Restore call stack
    memcpy(this->returnIndexStack, state.returnIndexStack, sizeof(this->returnIndexStack));
    this->returnIndexStackTop = state.returnIndexStackTop;
    
    // Restore 2KB RAM
    memcpy(this->ram, state.ram, sizeof(this->ram));
    
    // Restore PPU state
    if (ppu) {
        ppu->setVRAM(state.nametable);
        ppu->setOAM(state.oam);
        ppu->setPaletteRAM(state.palette);  // This will invalidate the tile cache
        
        ppu->setControl(state.ppuCtrl);
        ppu->setMask(state.ppuMask);
        ppu->setStatus(state.ppuStatus);
        ppu->setOAMAddr(state.oamAddress);
        ppu->setScrollX(state.ppuScrollX);
        ppu->setScrollY(state.ppuScrollY);
        
        ppu->setVRAMAddress(state.currentAddress);
        ppu->setWriteToggle(state.writeToggle);
        ppu->setDataBuffer(state.vramBuffer);
        
        std::cout << "Complete save state loaded from: " << actualFilename << std::endl;
    } else {
        std::cout << "Warning: PPU not available, partial state loaded from: " << actualFilename << std::endl;
    }
    
    return true;
}

void SMBEngine::renderScaled16(uint16_t* buffer, int screenWidth, int screenHeight)
{
    // Delegate to PPU
    ppu->renderScaled(buffer, screenWidth, screenHeight);
}

void SMBEngine::renderScaled32(uint32_t* buffer, int screenWidth, int screenHeight)
{
    // Delegate to PPU
    ppu->renderScaled32(buffer, screenWidth, screenHeight);
}
