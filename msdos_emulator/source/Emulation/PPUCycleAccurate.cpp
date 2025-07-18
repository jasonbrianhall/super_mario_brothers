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
        if (cycle >= 1 && cycle <= 256) {
            int pixelX = cycle - 1;
            if (pixelX < 256 && scanline < 240) {
                uint32_t bgColor32 = paletteRGB[palette[0]];
                uint16_t bgColor16 = ((bgColor32 & 0xF80000) >> 8) | 
                                   ((bgColor32 & 0x00FC00) >> 5) | 
                                   ((bgColor32 & 0x0000F8) >> 3);
                frameBuffer[scanline * 256 + pixelX] = bgColor16;
            }
        }
        return;
    }
    
    // Background tile fetching
    if ((cycle >= 1 && cycle <= 256) || (cycle >= 321 && cycle <= 336)) {
        stepBackgroundFetch(scanline, cycle);
    }
    
    // Debug cycles where we should be loading
    if (cycle == 8 || cycle == 16 || cycle == 24 || cycle == 32) {
        printf("On cycle %d - should have just loaded shift registers\n", cycle);
    }
    
    // Shift and render
    if (cycle >= 1 && cycle <= 256) {
        shiftBackgroundRegisters();
        renderPixel(scanline, cycle);
    }
    
    // Sprite evaluation
    if (cycle == 65) {
        evaluateSprites(scanline + 1);
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
    int fetchCycle;
    if (cycle >= 1 && cycle <= 256) {
        fetchCycle = ((cycle - 1) % 8) + 1;
    } else if (cycle >= 321 && cycle <= 336) {
        fetchCycle = ((cycle - 321) % 8) + 1;
    } else {
        return;
    }
    
    switch (fetchCycle) {
        case 1: fetchNametableByte(scanline, cycle); break;
        case 3: fetchAttributeByte(scanline, cycle); break;
        case 5: fetchPatternLow(scanline, cycle); break;
        case 7: fetchPatternHigh(scanline, cycle); break;
        case 8: loadShiftRegisters(); break;
    }
}

void PPUCycleAccurate::fetchNametableByte(int scanline, int cycle)
{
    int pixelX = cycle - 1;
    
    // Simple approach: fetch tile for current position
    int scrolledX = (pixelX + ppuScrollX) / 8;
    int scrolledY = (scanline + ppuScrollY) / 8;
    
    int nametableX = scrolledX % 64;
    int nametableY = scrolledY % 60;
    
    if (nametableX < 0) nametableX += 64;
    if (nametableY < 0) nametableY += 60;
    
    uint16_t baseAddr = 0x2000;
    if (nametableX >= 32) {
        baseAddr = 0x2400;
        nametableX -= 32;
    }
    if (nametableY >= 30) {
        baseAddr += 0x800;
        nametableY -= 30;
    }
    
    uint16_t addr = baseAddr + (nametableY * 32) + nametableX;
    addr = applyNametableMirroring(addr);
    
    if ((addr - 0x2000) < 2048 && (addr - 0x2000) >= 0) {
        state.bgTileIndex = nametable[addr - 0x2000];
    } else {
        state.bgTileIndex = 0x20;
    }
    
    state.currentTileX = nametableX;
    state.currentTileY = nametableY;
}

void PPUCycleAccurate::fetchAttributeByte(int scanline, int cycle)
{
    // Use the tile position from fetchNametableByte
    int tileX = state.currentTileX;
    int tileY = state.currentTileY;
    
    // Each attribute byte covers a 4x4 tile area (32x32 pixels)
    int attrX = (tileX % 32) / 4;
    int attrY = (tileY % 30) / 4;
    
    // Determine which nametable's attribute table
    int nametableIndex = 0;
    if (tileX >= 32) nametableIndex |= 1;
    if (tileY >= 30) nametableIndex |= 2;
    
    uint16_t attrAddr = 0x23C0 + (nametableIndex * 0x400) + (attrY * 8) + attrX;
    attrAddr = applyNametableMirroring(attrAddr);
    
    uint8_t attrByte = 0;
    if ((attrAddr - 0x2000) < 2048) {
        attrByte = nametable[attrAddr - 0x2000];
    }
    
    // Extract the 2-bit palette for this specific tile's quadrant
    int quadX = ((tileX % 32) / 2) % 2;
    int quadY = ((tileY % 30) / 2) % 2;
    int shift = (quadY * 4) + (quadX * 2);
    
    state.bgAttribute = (attrByte >> shift) & 0x03;
}


void PPUCycleAccurate::fetchPatternLow(int scanline, int cycle)
{
    uint16_t tileIndex = state.bgTileIndex;
    uint8_t fineY = (scanline + ppuScrollY) % 8;
    
    // Make sure fineY is positive
    if (fineY < 0) fineY += 8;
    
    // Pattern table selection
    uint16_t patternTableBase = (ppuCtrl & 0x10) ? 0x1000 : 0x0000;
    
    // Calculate pattern address with bounds checking
    uint16_t patternAddr = patternTableBase + (tileIndex * 16) + fineY;
    
    // Ensure we're reading from valid CHR range
    if (patternAddr < 0x2000) {
        state.bgPatternLow = engine.readCHRData(patternAddr);
    } else {
        state.bgPatternLow = 0x00; // Default to empty pattern
    }
}

void PPUCycleAccurate::fetchPatternHigh(int scanline, int cycle)
{
    uint16_t tileIndex = state.bgTileIndex;
    uint8_t fineY = (scanline + ppuScrollY) % 8;
    
    // Make sure fineY is positive
    if (fineY < 0) fineY += 8;
    
    // Pattern table selection
    uint16_t patternTableBase = (ppuCtrl & 0x10) ? 0x1000 : 0x0000;
    
    // Calculate pattern address with bounds checking
    uint16_t patternAddr = patternTableBase + (tileIndex * 16) + fineY + 8;
    
    // Ensure we're reading from valid CHR range
    if (patternAddr < 0x2000) {
        state.bgPatternHigh = engine.readCHRData(patternAddr);
    } else {
        state.bgPatternHigh = 0x00; // Default to empty pattern
    }
}



void PPUCycleAccurate::loadShiftRegisters()
{
    // Load new tile data into the LOW 8 bits
    state.bgPatternShiftLow = (state.bgPatternShiftLow & 0xFF00) | state.bgPatternLow;
    state.bgPatternShiftHigh = (state.bgPatternShiftHigh & 0xFF00) | state.bgPatternHigh;
    
    state.bgAttributeLatch0 = state.bgAttribute;
}

void PPUCycleAccurate::shiftBackgroundRegisters()
{
    // Shift left by 1 bit every cycle
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
    
    uint16_t finalPixel = convertToRGB565(palette[0]);
    
    // Get background pixel
    if (ppuMask & 0x08) {
        int fineX = ppuScrollX & 0x07;
        finalPixel = getBackgroundPixel(fineX);
        
        // Debug the boundary area where 0's appear
        if (scanline < 10 && pixelX < 32) {
            static int boundaryDebugCount = 0;
            if (boundaryDebugCount < 20) {
                printf("Boundary: (%d,%d), scroll=%d, fineX=%d, "
                       "shifts=(0x%04X,0x%04X), pixel=0x%04X\n",
                       pixelX, scanline, ppuScrollX, fineX,
                       state.bgPatternShiftLow, state.bgPatternShiftHigh, finalPixel);
                boundaryDebugCount++;
            }
        }
    }
    
    // Get sprite pixel
    if (ppuMask & 0x10) {
        bool sprite0Hit = false;
        bool spritePriority = false;
        uint16_t spritePixel = getSpritePixel(pixelX, sprite0Hit, spritePriority);
        
        if (spritePixel != 0) {
            if (!spritePriority || finalPixel == convertToRGB565(palette[0])) {
                finalPixel = spritePixel;
            }
        }
    }
    
    frameBuffer[scanline * 256 + pixelX] = finalPixel;
}

uint16_t PPUCycleAccurate::getBackgroundPixel(int fineX)
{
    // Read from the shift register based on fine X scroll
    // The shift registers are shifted left every cycle
    // Bit 15 is the leftmost pixel of the current tile, bit 8 is the rightmost
    
    int bitPosition = 15 - fineX;
    
    // Clamp bit position to valid range
    if (bitPosition < 8) bitPosition = 8;
    if (bitPosition > 15) bitPosition = 15;
    
    uint8_t pixel = 0;
    if (state.bgPatternShiftLow & (1 << bitPosition)) pixel |= 1;
    if (state.bgPatternShiftHigh & (1 << bitPosition)) pixel |= 2;
    
    // Debug to see what's happening at the boundary
    static int debugCount = 0;
    if (debugCount < 10) {
        printf("GetPixel[%d]: fineX=%d, bitPos=%d, shifts=(0x%04X,0x%04X), pixel=%d\n",
               debugCount, fineX, bitPosition, 
               state.bgPatternShiftLow, state.bgPatternShiftHigh, pixel);
        debugCount++;
    }
    
    if (pixel == 0) {
        return convertToRGB565(palette[0]);
    }
    
    uint8_t paletteIndex = (state.bgAttributeLatch0 * 4) + pixel;
    if (paletteIndex >= 32) paletteIndex = 0;
    
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
    // DuckTales uses vertical mirroring (mapper 2 = UxROM)
    // Vertical mirroring: $2000=$2800, $2400=$2C00
    
    if (address >= 0x3000) {
        // Mirror $3000-$3EFF to $2000-$2EFF
        address -= 0x1000;
    }
    
    if (address >= 0x2800 && address < 0x2C00) {
        // $2800-$2BFF mirrors to $2000-$23FF  
        address -= 0x800;
    } else if (address >= 0x2C00 && address < 0x3000) {
        // $2C00-$2FFF mirrors to $2400-$27FF
        address -= 0x800;
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
