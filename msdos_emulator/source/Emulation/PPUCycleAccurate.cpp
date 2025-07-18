#include "PPUCycleAccurate.hpp"
#include "../SMB/SMBEmulator.hpp"
#include "PPU.hpp"

// NES palette (same as main PPU)
static constexpr const uint32_t paletteRGB[64] = {
    0x7c7c7c, 0x0000fc, 0x0000bc, 0x4428bc, 0x940084, 0xa80020, 0xa81000, 0x881400,
    0x503000, 0x007800, 0x006800, 0x005800, 0x004058, 0x000000, 0x000000, 0x000000,
    0xbcbcbc, 0x0078f8, 0x0058f8, 0x6844fc, 0xd800cc, 0xe40058, 0xf83800, 0xe45c10,
    0xac7c00, 0x00b800, 0x00a800, 0x00a844, 0x008888, 0x000000, 0x000000, 0x000000,
    0xf8f8f8, 0x3cbcfc, 0x6888fc, 0x9878f8, 0xf878f8, 0xf85898, 0xf87858, 0xfca044,
    0xf8b800, 0xb8f818, 0x58d854, 0x58f898, 0x00e8d8, 0x787878, 0x000000, 0x000000,
    0xfcfcfc, 0xa4e4fc, 0xb8b8f8, 0xd8b8f8, 0xf8b8f8, 0xf8a4c0, 0xf0d0b0, 0xfce0a8,
    0xf8d878, 0xd8f878, 0xb8f8b8, 0xb8f8d8, 0x00fcfc, 0xf8d8f8, 0x000000, 0x000000
};

PPUCycleAccurate::PPUCycleAccurate(SMBEmulator& engine) : engine(engine)
{
    reset();
}

PPUCycleAccurate::~PPUCycleAccurate()
{
}

void PPUCycleAccurate::reset()
{
    // Clear frame buffer
    clearFrameBuffer();
    
    // Reset registers
    ppuCtrl = 0x00;
    ppuMask = 0x00;
    ppuStatus = 0x80;
    ppuScrollX = 0x00;
    ppuScrollY = 0x00;
    
    // Clear memory
    memset(palette, 0, sizeof(palette));
    memset(nametable, 0, sizeof(nametable));
    memset(oam, 0, sizeof(oam));
    
    // Reset internal state
    memset(&state, 0, sizeof(state));
}

void PPUCycleAccurate::syncWithMainPPU(PPU* mainPPU)
{
    if (!mainPPU) return;
    
    // Sync registers
    ppuCtrl = mainPPU->getControl();
    ppuMask = mainPPU->getMask();
    ppuStatus = mainPPU->getStatus();
    ppuScrollX = mainPPU->getScrollX();
    ppuScrollY = mainPPU->getScrollY();
    
    // Sync memory
    memcpy(palette, mainPPU->getPaletteRAM(), 32);
    memcpy(nametable, mainPPU->getVRAM(), 2048);
    memcpy(oam, mainPPU->getOAM(), 256);
}

void PPUCycleAccurate::clearFrameBuffer()
{
    // Fill with background color
    uint16_t bgColor = convertToRGB565(palette[0]);
    for (int i = 0; i < 256 * 240; i++) {
        frameBuffer[i] = bgColor;
    }
}

void PPUCycleAccurate::stepCycle(int scanline, int cycle)
{
    if (scanline < 240) {
        stepVisibleScanline(scanline, cycle);
    } else if (scanline >= 241 && scanline <= 260) {
        stepVBlankScanline(scanline, cycle);
    } else if (scanline == 261) {
        stepPreRenderScanline(scanline, cycle);
    }
}

void PPUCycleAccurate::stepVisibleScanline(int scanline, int cycle)
{
    bool renderingEnabled = (ppuMask & 0x18) != 0;
    
    if (!renderingEnabled) {
        // If rendering disabled, just output background color
        if (cycle >= 1 && cycle <= 256) {
            int pixelX = cycle - 1;
            frameBuffer[scanline * 256 + pixelX] = convertToRGB565(palette[0]);
        }
        return;
    }
    
    // Background tile fetching (cycles 1-256 and 321-336)
    if ((cycle >= 1 && cycle <= 256) || (cycle >= 321 && cycle <= 336)) {
        stepBackgroundFetch(scanline, cycle);
        shiftBackgroundRegisters();
    }
    
    // Sprite evaluation for next scanline (cycles 65-256)
    if (cycle == 65) {
        evaluateSprites(scanline + 1);
    }
    
    // Pixel output (cycles 1-256)
    if (cycle >= 1 && cycle <= 256) {
        renderPixel(scanline, cycle);
    }
}

void PPUCycleAccurate::stepVBlankScanline(int scanline, int cycle)
{
    // Nothing happens during VBlank for rendering
}

void PPUCycleAccurate::stepPreRenderScanline(int scanline, int cycle)
{
    bool renderingEnabled = (ppuMask & 0x18) != 0;
    
    if (!renderingEnabled) return;
    
    // Pre-render scanline does background fetching but no pixel output
    if ((cycle >= 1 && cycle <= 256) || (cycle >= 321 && cycle <= 336)) {
        stepBackgroundFetch(scanline, cycle);
        shiftBackgroundRegisters();
    }
    
    // Sprite evaluation for first visible scanline
    if (cycle == 65) {
        evaluateSprites(0);
    }
}

void PPUCycleAccurate::stepBackgroundFetch(int scanline, int cycle)
{
    // Determine which fetch cycle we're in (1-8 pattern)
    int fetchCycle;
    if (cycle >= 1 && cycle <= 256) {
        fetchCycle = ((cycle - 1) % 8) + 1;
    } else if (cycle >= 321 && cycle <= 336) {
        fetchCycle = ((cycle - 321) % 8) + 1;
    } else {
        return;
    }
    
    switch (fetchCycle) {
        case 1: // Fetch nametable byte
            fetchNametableByte(scanline, cycle);
            break;
            
        case 3: // Fetch attribute byte
            fetchAttributeByte(scanline, cycle);
            break;
            
        case 5: // Fetch pattern low byte
            fetchPatternLow(scanline, cycle);
            break;
            
        case 7: // Fetch pattern high byte
            fetchPatternHigh(scanline, cycle);
            break;
            
        case 0: // Every 8th cycle (8, 16, 24, etc) - load shift registers
            loadShiftRegisters();
            break;
    }
}

void PPUCycleAccurate::fetchNametableByte(int scanline, int cycle)
{
    // Calculate effective scroll position
    state.effectiveScrollX = ppuScrollX + ((ppuCtrl & 0x01) ? 256 : 0);
    state.effectiveScrollY = ppuScrollY + ((ppuCtrl & 0x02) ? 240 : 0);
    
    // Calculate current tile position
    int pixelX = (cycle >= 321) ? (cycle - 321) + 256 : (cycle - 1);
    int scrolledX = (pixelX + state.effectiveScrollX) % 512;
    int scrolledY = (scanline + state.effectiveScrollY) % 480;
    
    state.currentTileX = scrolledX / 8;
    state.currentTileY = scrolledY / 8;
    
    // Fetch nametable byte
    uint16_t nametableAddr = getNametableAddress(state.currentTileX, state.currentTileY);
    
    // Handle status bar area (always from nametable 0, rows 0-3)
    if (scanline < 32) {  // Status bar area
        int statusTileX = pixelX / 8;
        int statusTileY = scanline / 8;
        nametableAddr = 0x2000 + (statusTileY * 32) + statusTileX;
    }
    
    state.bgTileIndex = nametable[applyNametableMirroring(nametableAddr) - 0x2000];
}

void PPUCycleAccurate::fetchAttributeByte(int scanline, int cycle)
{
    // Get attribute for current tile position
    uint16_t attrAddr = getAttributeAddress(state.currentTileX, state.currentTileY);
    
    // Handle status bar area
    if (scanline < 32) {
        int statusTileX = ((cycle >= 321) ? (cycle - 321) + 256 : (cycle - 1)) / 8;
        int statusTileY = scanline / 8;
        attrAddr = 0x23C0 + (statusTileY / 4) * 8 + (statusTileX / 4);
    }
    
    uint8_t attrByte = nametable[applyNametableMirroring(attrAddr) - 0x2000];
    state.bgAttribute = getAttributeBits(state.currentTileX, state.currentTileY, attrByte);
}

void PPUCycleAccurate::fetchPatternLow(int scanline, int cycle)
{
    uint16_t tileIndex = state.bgTileIndex;
    uint8_t fineY = (scanline + state.effectiveScrollY) % 8;
    
    // Handle status bar
    if (scanline < 32) {
        fineY = scanline % 8;
    }
    
    // Add pattern table base
    if (ppuCtrl & 0x10) {
        tileIndex += 256;
    }
    
    // CRITICAL: Read CHR data through engine (handles mapper banking)
    uint16_t patternAddr = tileIndex * 16 + fineY;
    state.bgPatternLow = engine.readCHRData(patternAddr);
}

void PPUCycleAccurate::fetchPatternHigh(int scanline, int cycle)
{
    uint16_t tileIndex = state.bgTileIndex;
    uint8_t fineY = (scanline + state.effectiveScrollY) % 8;
    
    // Handle status bar
    if (scanline < 32) {
        fineY = scanline % 8;
    }
    
    // Add pattern table base
    if (ppuCtrl & 0x10) {
        tileIndex += 256;
    }
    
    // CRITICAL: Read CHR data through engine (handles mapper banking)
    uint16_t patternAddr = tileIndex * 16 + fineY + 8;
    state.bgPatternHigh = engine.readCHRData(patternAddr);
}

void PPUCycleAccurate::loadShiftRegisters()
{
    // Load new tile data into shift registers
    state.bgPatternShiftLow = (state.bgPatternShiftLow & 0xFF00) | state.bgPatternLow;
    state.bgPatternShiftHigh = (state.bgPatternShiftHigh & 0xFF00) | state.bgPatternHigh;
    
    // Load attribute data
    state.bgAttributeLatch0 = state.bgAttribute;
}

void PPUCycleAccurate::shiftBackgroundRegisters()
{
    // Shift pattern data
    state.bgPatternShiftLow <<= 1;
    state.bgPatternShiftHigh <<= 1;
}

void PPUCycleAccurate::evaluateSprites(int scanline)
{
    if (scanline >= 240) return;
    
    state.spriteCount = 0;
    state.sprite0Present = false;
    
    // Check all 64 sprites
    for (int i = 0; i < 64 && state.spriteCount < 8; i++) {
        uint8_t spriteY = oam[i * 4];
        uint8_t spriteIndex = oam[i * 4 + 1];
        uint8_t spriteAttr = oam[i * 4 + 2];
        uint8_t spriteX = oam[i * 4 + 3];
        
        // Check if sprite is on this scanline
        spriteY++; // Sprite Y is delayed by 1
        if (scanline >= spriteY && scanline < spriteY + 8) {
            // Add sprite to render list
            int spriteSlot = state.spriteCount++;
            state.sprites[spriteSlot].x = spriteX;
            state.sprites[spriteSlot].attribute = spriteAttr;
            state.sprites[spriteSlot].index = i;
            
            if (i == 0) {
                state.sprite0Present = true;
            }
            
            // Fetch sprite pattern data
            uint8_t fineY = scanline - spriteY;
            if (spriteAttr & 0x80) { // Vertical flip
                fineY = 7 - fineY;
            }
            
            uint16_t tileAddr = spriteIndex;
            if (ppuCtrl & 0x08) { // Sprite pattern table
                tileAddr += 256;
            }
            
            // Fetch pattern data through engine
            state.sprites[spriteSlot].patternLow = engine.readCHRData(tileAddr * 16 + fineY);
            state.sprites[spriteSlot].patternHigh = engine.readCHRData(tileAddr * 16 + fineY + 8);
        }
    }
}

void PPUCycleAccurate::renderPixel(int scanline, int cycle)
{
    int pixelX = cycle - 1;
    if (pixelX >= 256 || scanline >= 240) return;
    
    uint16_t finalPixel = convertToRGB565(palette[0]); // Default background
    
    // Get background pixel
    if (ppuMask & 0x08) { // Background enabled
        uint8_t fineX = (pixelX + state.effectiveScrollX) % 8;
        finalPixel = getBackgroundPixel(fineX);
    }
    
    // Get sprite pixel
    if (ppuMask & 0x10) { // Sprites enabled
        bool sprite0Hit = false;
        bool spritePriority = false;
        uint16_t spritePixel = getSpritePixel(pixelX, sprite0Hit, spritePriority);
        
        if (spritePixel != 0) { // Non-transparent sprite
            if (!spritePriority || finalPixel == convertToRGB565(palette[0])) {
                finalPixel = spritePixel;
            }
            
            // Sprite 0 hit detection
            if (sprite0Hit && finalPixel != convertToRGB565(palette[0])) {
                // Set sprite 0 hit flag in main PPU through engine
                // This is a simplified approach
            }
        }
    }
    
    frameBuffer[scanline * 256 + pixelX] = finalPixel;
}

uint16_t PPUCycleAccurate::getBackgroundPixel(int fineX)
{
    // Extract pixel from shift registers
    int shiftAmount = 15 - fineX;
    
    uint8_t pixel = 0;
    if (state.bgPatternShiftLow & (1 << shiftAmount)) pixel |= 1;
    if (state.bgPatternShiftHigh & (1 << shiftAmount)) pixel |= 2;
    
    if (pixel == 0) {
        return convertToRGB565(palette[0]); // Transparent background
    }
    
    // Get palette from attribute
    uint8_t paletteIndex = state.bgAttributeLatch0 * 4 + pixel;
    return convertToRGB565(palette[paletteIndex]);
}

uint16_t PPUCycleAccurate::getSpritePixel(int pixelX, bool& sprite0Hit, bool& spritePriority)
{
    sprite0Hit = false;
    spritePriority = false;
    
    // Check sprites in priority order (lowest index first)
    for (int i = 0; i < state.spriteCount; i++) {
        auto& sprite = state.sprites[i];
        
        if (pixelX >= sprite.x && pixelX < sprite.x + 8) {
            // Get pixel from sprite
            int spritePixelX = pixelX - sprite.x;
            if (sprite.attribute & 0x40) { // Horizontal flip
                spritePixelX = 7 - spritePixelX;
            }
            
            uint8_t pixel = 0;
            if (sprite.patternLow & (1 << (7 - spritePixelX))) pixel |= 1;
            if (sprite.patternHigh & (1 << (7 - spritePixelX))) pixel |= 2;
            
            if (pixel != 0) { // Non-transparent
                if (sprite.index == 0) {
                    sprite0Hit = true;
                }
                
                spritePriority = (sprite.attribute & 0x20) != 0;
                
                // Get color from sprite palette
                uint8_t spritePalette = sprite.attribute & 0x03;
                uint8_t colorIndex = palette[0x10 + spritePalette * 4 + pixel];
                return convertToRGB565(colorIndex);
            }
        }
    }
    
    return 0; // No sprite pixel
}

void PPUCycleAccurate::getFrameBuffer(uint16_t* buffer)
{
    memcpy(buffer, frameBuffer, 256 * 240 * sizeof(uint16_t));
}

// Utility methods
uint16_t PPUCycleAccurate::getNametableAddress(int tileX, int tileY)
{
    // Simplified nametable addressing
    int nametable = 0;
    if (tileX >= 32) {
        nametable = 1;
        tileX -= 32;
    }
    
    return 0x2000 + (nametable * 0x400) + (tileY * 32) + tileX;
}

uint16_t PPUCycleAccurate::getAttributeAddress(int tileX, int tileY)
{
    int nametable = 0;
    if (tileX >= 32) {
        nametable = 1;
        tileX -= 32;
    }
    
    return 0x23C0 + (nametable * 0x400) + (tileY / 4) * 8 + (tileX / 4);
}

uint8_t PPUCycleAccurate::getAttributeBits(int tileX, int tileY, uint8_t attrByte)
{
    int quadX = (tileX / 2) % 2;
    int quadY = (tileY / 2) % 2;
    int shift = (quadY * 4) + (quadX * 2);
    
    return (attrByte >> shift) & 0x03;
}

uint16_t PPUCycleAccurate::applyNametableMirroring(uint16_t address)
{
    // Simplified mirroring (horizontal for SMB)
    if (address >= 0x2400 && address < 0x2800) {
        return address - 0x400; // Mirror to first nametable
    }
    return address;
}

uint16_t PPUCycleAccurate::convertToRGB565(uint8_t colorIndex)
{
    if (colorIndex >= 64) colorIndex = 0;
    
    uint32_t color32 = paletteRGB[colorIndex];
    
    int r = (color32 >> 16) & 0xFF;
    int g = (color32 >> 8) & 0xFF;
    int b = color32 & 0xFF;
    
    // Convert to RGB565
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}
