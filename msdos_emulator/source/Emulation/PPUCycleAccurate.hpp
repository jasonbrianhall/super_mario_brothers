#ifndef PPU_CYCLE_ACCURATE_HPP
#define PPU_CYCLE_ACCURATE_HPP

#include <cstdint>
#include <cstring>

class SMBEmulator; // Forward declaration

// Adapted from nesticle's nesvideo.cpp
struct bitmap8x8 {
    uint8_t s[8][8];
    
    void create(uint8_t* patternLow, uint8_t* patternHigh);
    void draw_tile(uint16_t* dest, int x, int y, uint8_t palette, uint8_t* paletteRAM);
    void draw_sprite(uint16_t* dest, int x, int y, uint8_t attributes, uint8_t* spritePalette);
    
private:
    uint16_t convertToRGB565(uint8_t colorIndex);
};

// 1K pattern bank (64 patterns)
struct pattern1k {
    bitmap8x8 p[64];
    bool updated[64];
    
    void create(SMBEmulator& engine, int bankIndex);
    void markAllUpdated();
    void markUpdated(int patternIndex);
};

// Pattern table (4 x 1K banks)
struct patterntable {
    uint8_t bank[4];        // Bank indices
    pattern1k* pbank[4];    // Pointers to actual banks
    
    void setbank(int banknum, int pnum, pattern1k* pattern1kArray);
    bitmap8x8& operator[](uint8_t idx);
    bool operator==(const patterntable& other) const;
};

// Nametable cache (adapted from nesticle)
class natablecache {
private:
    uint16_t* surface;      // 256x240 surface
    uint8_t nametable[32 * 30];
    uint8_t attribute[8 * 8];
    patterntable pt;
    bool lineupdated[30];
    bool updated[30][32];
    
    void drawtile(int tx, int ty, uint8_t* paletteRAM);
    
public:
    natablecache();
    ~natablecache();
    
    void totalupdate();
    void write(uint16_t addr, uint8_t value);
    uint8_t read(uint16_t addr);
    void setpatterntable(const patterntable& newpt);
    void refresh(int startY, int endY, uint8_t* paletteRAM);
    void draw(uint16_t* dest, int sx, int sy, int clipX1, int clipY1, int clipX2, int clipY2, uint8_t* paletteRAM);
};

// Main nesticle-style PPU (replaces your existing PPUCycleAccurate)
class PPUCycleAccurate {
private:
    SMBEmulator& engine;
    
    // Pattern tables
    int numpattern1k;
    pattern1k* pt1k;
    bool pt1kupdated[64]; // Track which 1K banks need updating
    
    // Nametable caches
    natablecache* ntc[4];  // 4 nametable caches
    int mirroring;         // 0=horizontal, 1=vertical
    
    // Current pattern tables for rendering
    patterntable bgPatternTable;
    patterntable spritePatternTable;
    
    // PPU registers
    uint8_t ppuCtrl;
    uint8_t ppuMask; 
    uint8_t ppuStatus;
    uint8_t scrollX, scrollY;
    
    // Memory
    uint8_t paletteRAM[32];
    uint8_t oam[256];
    
    void drawBackground(uint16_t* frameBuffer);
    void drawSprites(uint16_t* frameBuffer);
    void drawTileLikeWorkingPPU(uint16_t* frameBuffer, uint16_t tile, int xOffset, int yOffset, uint8_t attribute);
    uint32_t convertToRGB32(uint8_t colorIndex);
    uint16_t convertToRGB565(uint8_t colorIndex);
    
public:
    PPUCycleAccurate(SMBEmulator& eng);
    ~PPUCycleAccurate();
    
    void reset();
    void setMirroring(int type);
    void updatePatternTables();
    void writeRegister(uint16_t addr, uint8_t value);
    uint8_t readRegister(uint16_t addr);
    void markCHRUpdated(uint16_t addr);
    void render(uint16_t* frameBuffer);
    
    void stepCycle(int scanline, int cycle);
    void getFrameBuffer(uint16_t* buffer);
    
    // Accessors for compatibility
    uint8_t* getPaletteRAM() { return paletteRAM; }
    uint8_t* getOAM() { return oam; }
    uint8_t getControl() const { return ppuCtrl; }
    uint8_t getMask() const { return ppuMask; }
    uint8_t getStatus() const { return ppuStatus; }
    uint8_t getScrollX() const { return scrollX; }
    uint8_t getScrollY() const { return scrollY; }
    void setVBlankFlag(bool flag);
    void captureFrameScroll();
    void setSprite0Hit(bool hit);
    void writeAddressRegister(uint8_t value);
    void writeDataRegister(uint8_t value);
    void writeByte(uint16_t address, uint8_t value);
    uint16_t getNametableIndex(uint16_t address);
    int getNametableCacheIndex(uint16_t address);
    uint8_t frameScrollX; 
    uint8_t frameCtrl;
    bool sprite0Hit;
    uint8_t oamAddress; /**< $2003 */
    uint8_t ppuScrollX; /**< $2005 */
    uint8_t ppuScrollY; /**< $2005 */
    uint8_t cachedCtrl;
    uint16_t currentAddress; /**< Address that will be accessed on the next PPU read/write. */
    bool writeToggle; /**< Toggles whether the low or high bit of the current address will be set on the next write to PPUADDR. */
    uint8_t gameAreaScrollX;  // The "real" scroll value for the game area
    void writeDMA(uint8_t page);

};

#endif // PPU_CYCLE_ACCURATE_HPP
