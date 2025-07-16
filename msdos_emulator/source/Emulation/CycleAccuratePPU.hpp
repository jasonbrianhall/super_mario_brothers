#ifndef CYCLE_ACCURATE_PPU_HPP
#define CYCLE_ACCURATE_PPU_HPP

#include <cstdint>
#include <cstring>

class SMBEmulator;  // Forward declaration

class CycleAccuratePPU {
private:
    SMBEmulator& engine;
    
    // Timing state
    int currentScanline;
    int currentCycle;
    bool frameComplete;
    
    // Rendering state
    uint16_t* frameBuffer;
    uint8_t chrBankHistory[262];  // CHR bank for each scanline
    
    // PPU registers
    uint8_t ppuCtrl;
    uint8_t ppuMask;
    uint8_t ppuStatus;
    uint8_t oamAddress;
    uint8_t ppuScrollX;
    uint8_t ppuScrollY;
    uint16_t currentAddress;
    bool writeToggle;
    uint8_t vramBuffer;
    
    // Memory
    uint8_t palette[32];
    uint8_t nametable[2048];
    uint8_t oam[256];
    
    // Frame capture
    uint8_t frameScrollX;
    uint8_t frameCtrl;
    bool sprite0Hit;
    
public:
    CycleAccuratePPU(SMBEmulator& engine);
    
    // Main execution method
    bool executeCycle(uint16_t* buffer);
    
    // Register interface (same as old PPU)
    uint8_t readRegister(uint16_t address);
    void writeRegister(uint16_t address, uint8_t value);
    void writeDMA(uint8_t page);
    
    // Getters for compatibility
    uint8_t getControl() const { return ppuCtrl; }
    uint8_t getMask() const { return ppuMask; }
    uint8_t* getPaletteRAM() { return palette; }
    uint8_t* getOAM() { return oam; }
    uint8_t* getVRAM() { return nametable; }
    void render(uint32_t* buffer);
    void renderScaled(uint16_t* buffer, int screenWidth, int screenHeight);
    void renderScaled32(uint32_t* buffer, int screenWidth, int screenHeight);

private:
    // Helper methods
    uint32_t convert16BitTo32Bit(uint16_t color16);
    void scaleBuffer16(uint16_t* nesBuffer, uint16_t* outputBuffer, int screenWidth, int screenHeight);
    void scaleBuffer32(uint32_t* nesBuffer, uint32_t* outputBuffer, int screenWidth, int screenHeight);    

    void executeVisibleScanline();
    void renderPixel(int x, int y);
    uint16_t renderBackgroundPixel(int x, int y);
    uint16_t renderSpritePixel(int x, int y);
    uint16_t renderTilePixel(uint16_t nametableAddr, int pixelX, int pixelY, int scanline);
    
    // Helper methods
    uint8_t readByte(uint16_t address);
    void writeByte(uint16_t address, uint8_t value);
    uint16_t getNametableIndex(uint16_t address);
    uint8_t getAttributeTableValue(uint16_t nametableAddress);
    uint16_t convertColorTo16Bit(uint8_t colorIndex);
    uint16_t getBackgroundColor16();
    bool isSpriteZeroAtPixel(int x, int y);
    
    uint8_t readDataRegister();
    void writeAddressRegister(uint8_t value);
    void writeDataRegister(uint8_t value);
};

#endif // CYCLE_ACCURATE_PPU_HPP
