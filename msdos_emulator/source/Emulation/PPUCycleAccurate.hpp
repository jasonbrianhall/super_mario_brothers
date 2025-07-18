#ifndef PPU_CYCLE_ACCURATE_HPP
#define PPU_CYCLE_ACCURATE_HPP

#include <cstdint>
#include <cstring>

class SMBEmulator; // Forward declaration

/**
 * Cycle-accurate PPU implementation for mappers that require mid-frame precision
 * This works alongside the existing PPU for games that need cycle-level accuracy
 */
class PPUCycleAccurate
{
public:
    PPUCycleAccurate(SMBEmulator& engine);
    ~PPUCycleAccurate();
    
    // Main cycle stepping
    void stepCycle(int scanline, int cycle);
    
    // Frame buffer access
    void getFrameBuffer(uint16_t* buffer);
    void clearFrameBuffer();
    
    // PPU register interface (mirrors main PPU)
    void setControl(uint8_t value) { ppuCtrl = value; }
    void setMask(uint8_t value) { ppuMask = value; }
    void setStatus(uint8_t value) { ppuStatus = value; }
    void setScrollX(uint8_t value) { ppuScrollX = value; }
    void setScrollY(uint8_t value) { ppuScrollY = value; }
    void setPalette(uint8_t* paletteData) { memcpy(palette, paletteData, 32); }
    void setOAM(uint8_t* oamData) { memcpy(oam, oamData, 256); }
    void setNametable(uint8_t* nametableData) { memcpy(nametable, nametableData, 2048); }
    
    uint8_t getControl() const { return ppuCtrl; }
    uint8_t getMask() const { return ppuMask; }
    uint8_t getStatus() const { return ppuStatus; }
    
    // State management
    void reset();
    void syncWithMainPPU(class PPU* mainPPU);
    
private:
    SMBEmulator& engine;
    
    // Frame buffer
    uint16_t frameBuffer[256 * 240];
    
    // PPU registers (synced from main PPU)
    uint8_t ppuCtrl;
    uint8_t ppuMask;  
    uint8_t ppuStatus;
    uint8_t ppuScrollX;
    uint8_t ppuScrollY;
    
    // PPU memory (synced from main PPU)
    uint8_t palette[32];
    uint8_t nametable[2048];
    uint8_t oam[256];
    
    // Internal PPU state for cycle-accurate rendering
    struct InternalState {
        // Current fetch data
        uint8_t bgTileIndex;      
        uint8_t bgAttribute;      
        uint8_t bgPatternLow;     
        uint8_t bgPatternHigh;    
        
        // Background shift registers
        uint16_t bgPatternShiftLow;   
        uint16_t bgPatternShiftHigh;  
        uint8_t bgAttributeLatch0;    // Attribute latch for current tile
        uint8_t bgAttributeLatch1;    // Attribute latch for next tile
        
        // Sprite data for current scanline
        uint8_t spriteCount;
        struct SpriteData {
            uint8_t x;
            uint8_t patternLow;
            uint8_t patternHigh;
            uint8_t attribute;
            uint8_t index; // Original sprite index for sprite 0 detection
        } sprites[8];
        bool sprite0Present;
        
        // Scroll/address state (simplified)
        int effectiveScrollX;
        int effectiveScrollY;
        int currentTileX;
        int currentTileY;
        
    } state;
    
    // Scanline type handlers
    void stepVisibleScanline(int scanline, int cycle);
    void stepVBlankScanline(int scanline, int cycle);
    void stepPreRenderScanline(int scanline, int cycle);
    
    // Background rendering pipeline
    void stepBackgroundFetch(int scanline, int cycle);
    void fetchNametableByte(int scanline, int cycle);
    void fetchAttributeByte(int scanline, int cycle);
    void fetchPatternLow(int scanline, int cycle);
    void fetchPatternHigh(int scanline, int cycle);
    void loadShiftRegisters();
    void shiftBackgroundRegisters();
    
    // Sprite handling
    void evaluateSprites(int scanline);
    void fetchSpriteData(int scanline, int spriteIndex, int fetchCycle);
    
    // Pixel output
    void renderPixel(int scanline, int cycle);
    uint16_t getBackgroundPixel(int fineX);
    uint16_t getSpritePixel(int pixelX, bool& sprite0Hit, bool& spritePriority);
    
    // Utility methods
    uint16_t getNametableAddress(int tileX, int tileY);
    uint16_t getAttributeAddress(int tileX, int tileY);
    uint8_t getAttributeBits(int tileX, int tileY, uint8_t attrByte);
    uint16_t convertToRGB565(uint8_t colorIndex);
    
    // Mirroring
    uint16_t applyNametableMirroring(uint16_t address);
};

#endif // PPU_CYCLE_ACCURATE_HPP
