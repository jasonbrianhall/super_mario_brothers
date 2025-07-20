#ifndef PPU_HPP
#define PPU_HPP

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cstring>

struct ComprehensiveTileCache {
    uint16_t pixels[64];        // Normal orientation only
    uint16_t tile_id;
    uint8_t palette_type;       
    uint8_t attribute;
    bool is_valid;
};

// Dynamic storage for flip variations
struct FlipCacheEntry {
    uint16_t pixels[64];
    uint16_t tile_id;
    uint8_t palette_type;
    uint8_t attribute;
    uint8_t flip_flags;  // 1=flipX, 2=flipY, 3=flipXY
};

class SMBEmulator;  // Forward declaration

/**
 * Emulates the NES Picture Processing Unit.
 */
class PPU
{
public:
    PPU(SMBEmulator& engine);

    uint8_t readRegister(uint16_t address);

    /**
     * Render to a frame buffer.
     */
    void render(uint32_t* buffer);

    void writeDMA(uint8_t page);

    void writeRegister(uint16_t address, uint8_t value);
    void render16(uint16_t* buffer);
    
    // Getter methods
    uint8_t* getVRAM() { return nametable; }
    uint8_t* getOAM() { return oam; }
    uint8_t* getPaletteRAM() { return palette; }

    uint8_t getControl() { return ppuCtrl; }
    uint8_t getMask() { return ppuMask; }
    uint8_t getStatus() { return ppuStatus; }
    uint8_t getOAMAddr() { return oamAddress; }
    uint8_t getScrollX() { return ppuScrollX; }
    uint8_t getScrollY() { return ppuScrollY; }

    uint16_t getVRAMAddress() { return currentAddress; }
    bool getWriteToggle() { return writeToggle; }
    uint8_t getDataBuffer() { return vramBuffer; }

    // Setter methods for load state
    void setVRAM(uint8_t* data) { memcpy(nametable, data, 2048); }
    void setOAM(uint8_t* data) { memcpy(oam, data, 256); }
    void setPaletteRAM(uint8_t* data);

    void setControl(uint8_t val) { ppuCtrl = val; }
    void setMask(uint8_t val) { ppuMask = val; }
    void setStatus(uint8_t val) { ppuStatus = val; }
    void setOAMAddr(uint8_t val) { oamAddress = val; }
    void setScrollX(uint8_t val) { ppuScrollX = val; }
    void setScrollY(uint8_t val) { ppuScrollY = val; }

    void setVRAMAddress(uint16_t val) { currentAddress = val; }
    void setWriteToggle(bool val) { writeToggle = val; }
    void setDataBuffer(uint8_t val) { vramBuffer = val; }
    
    // Scaling methods
    void renderScaled(uint16_t* buffer, int screenWidth, int screenHeight);
    void renderScaled32(uint32_t* buffer, int screenWidth, int screenHeight);
    
    // PPU state methods
    void setVBlankFlag(bool flag);
    uint8_t getControl() const { return ppuCtrl; }
    void setSprite0Hit(bool hit);
    uint8_t getMask() const { return ppuMask; }
    void updateRenderRegisters();
    void captureFrameScroll();
    void invalidateTileCache();
    
    // PPU cycle tracking and catch-up (DJGPP compatible)
    void stepCycle(int scanline, int cycle);
    void catchUp(uint64_t targetCycles);
    uint64_t getCurrentCycles() const { return ppuCycles; }
    void setCycles(uint64_t cycles) { ppuCycles = cycles; }
    void addCycles(uint64_t cycles) { ppuCycles += cycles; }
    
    // Frame state tracking
    bool isInVBlank() const { return inVBlank; }
    int getCurrentScanline() const { return currentScanline; }
    int getCurrentCycle() const { return currentCycle; }
    bool isFrameComplete() const { return frameComplete; }
    void resetFrame() { frameComplete = false; currentRenderScanline = 0; }
    
private:
    SMBEmulator& engine;

    struct ScalingCache {
        uint16_t* scaledBuffer;
        int* sourceToDestX;
        int* sourceToDestY;
        int scaleFactor;
        int destWidth, destHeight;
        int destOffsetX, destOffsetY;
        int screenWidth, screenHeight;
        bool isValid;        
        ScalingCache();
        ~ScalingCache();
        void cleanup();
    };

    uint8_t scanlineScrollX[240];   // Scroll X value for each scanline
    uint8_t scanlineCtrl[240];      // Control register value for each scanline

    // Scanline-based rendering state
    uint16_t frameBuffer[256 * 240];
    int currentRenderScanline;
    bool frameComplete;
    
    // Scanline rendering methods
    void renderScanline(int scanline);
    void renderBackgroundScanline(int scanline);
    void renderSpriteScanline(int scanline);
    void clearScanline(int scanline);
    void checkSprite0HitScanline(int scanline);
    
    
    static ScalingCache g_scalingCache;
    
    // Scaling methods
    void initializeScalingCache(int screenWidth, int screenHeight);
    void updateScalingCache(int screenWidth, int screenHeight);
    bool isScalingCacheValid(int screenWidth, int screenHeight);
    
    // Optimized scaling implementations
    void renderScaled1x1(uint16_t* nesBuffer, uint16_t* screenBuffer, int screenWidth, int screenHeight);
    void renderScaled2x(uint16_t* nesBuffer, uint16_t* screenBuffer, int screenWidth, int screenHeight);
    void renderScaled3x(uint16_t* nesBuffer, uint16_t* screenBuffer, int screenWidth, int screenHeight);
    void renderScaledGeneric(uint16_t* nesBuffer, uint16_t* screenBuffer, int screenWidth, int screenHeight, int scale);
    
    // Color conversion
    void convertNESToScreen32(uint16_t* nesBuffer, uint32_t* screenBuffer, int screenWidth, int screenHeight);

    // PPU registers
    uint8_t ppuCtrl; /**< $2000 */
    uint8_t ppuMask; /**< $2001 */
    uint8_t ppuStatus; /**< 2002 */
    uint8_t oamAddress; /**< $2003 */
    uint8_t ppuScrollX; /**< $2005 */
    uint8_t ppuScrollY; /**< $2005 */

    uint8_t palette[32]; /**< Palette data. */
    uint8_t nametable[2048]; /**< Background table. */
    uint8_t oam[256]; /**< Sprite memory. */
    uint8_t nextFrameScrollX[240]; 
    // PPU Address control
    uint16_t currentAddress; /**< Address that will be accessed on the next PPU read/write. */
    bool writeToggle; /**< Toggles whether the low or high bit of the current address will be set on the next write to PPUADDR. */
    uint8_t vramBuffer; /**< Stores the last read byte from VRAM to delay reads by 1 byte. */

    // PPU cycle tracking
    uint64_t ppuCycles;      // Total PPU cycles executed
    int currentScanline;     // Current scanline (0-261)
    int currentCycle;        // Current cycle within scanline (0-340)
    bool inVBlank;           // VBlank state
    bool frameOdd;           // Odd frame flag (for cycle skipping)
    
    // PPU timing constants (NTSC)
    static const int CYCLES_PER_SCANLINE = 341;
    static const int VISIBLE_SCANLINES = 240;
    static const int VBLANK_START_SCANLINE = 241;
    static const int PRERENDER_SCANLINE = 261;
    static const int TOTAL_SCANLINES = 262;
    
    // Internal cycle stepping methods
    void stepScanline();
    void handleVBlankStart();
    void handleVBlankEnd();
    void handleSpriteEvaluation();
    void handleBackgroundFetch();
    void checkSprite0Hit();
    
    // Internal helper methods
    uint8_t getAttributeTableValue(uint16_t nametableAddress);
    uint16_t getNametableIndex(uint16_t address);
    uint8_t readByte(uint16_t address);
    uint8_t readCHR(int index);
    uint8_t readCHRFromBank(int index, uint8_t chr_bank);  // Add this method
    uint8_t readDataRegister();
    void renderTile(uint32_t* buffer, int index, int xOffset, int yOffset);
    void writeAddressRegister(uint8_t value);
    void writeByte(uint16_t address, uint8_t value);
    void writeDataRegister(uint8_t value);
    void renderTile16(uint16_t* buffer, int index, int xOffset, int yOffset);

    // Legacy static cache (keeping for backwards compatibility)
    static ComprehensiveTileCache g_comprehensiveCache[512 * 8];
    static bool g_comprehensiveCacheInit;
    static std::vector<FlipCacheEntry> g_flipCache;
    static std::unordered_map<uint32_t, size_t> g_flipCacheIndex;
    
    // Tile caching methods - UPDATE SIGNATURES TO INCLUDE CHR BANK
    int getTileCacheIndex(uint16_t tile, uint8_t palette_type, uint8_t attribute);
    void renderCachedTile(uint16_t* buffer, int index, int xOffset, int yOffset, bool flipX, bool flipY);
    void renderCachedSprite(uint16_t* buffer, uint16_t tile, uint8_t palette_idx, int xOffset, int yOffset, bool flipX, bool flipY);
    void renderCachedSpriteWithPriority(uint16_t* buffer, uint16_t tile, uint8_t sprite_palette, int xOffset, int yOffset, bool flipX, bool flipY, bool behindBackground);
    void cacheFlipVariation(uint16_t tile, uint8_t palette_type, uint8_t attribute, bool flipX, bool flipY, uint8_t chr_bank);
    uint32_t getFlipCacheKey(uint16_t tile, uint8_t palette_type, uint8_t attribute, uint8_t flip_flags);
    
    // PPU state variables
    bool sprite0Hit;
    uint8_t cachedScrollX;
    uint8_t cachedScrollY;
    uint8_t cachedCtrl;
    uint8_t renderScrollX;
    uint8_t renderScrollY; 
    uint8_t renderCtrl;
    uint8_t gameAreaScrollX;  // The "real" scroll value for the game area
    bool ignoreNextScrollWrite;
    uint8_t frameScrollX; 
    uint8_t frameCtrl;     // The control register for this entire frame
    uint8_t frameCHRBank;

};

#endif // PPU_HPP
