#include "CycleAccuratePPU.hpp"
#include "../SMB/SMBEmulator.hpp"
#include <cstring>

static const uint8_t nametableMirrorLookup[][4] = {
    {0, 0, 1, 1}, // Vertical
    {0, 1, 0, 1}  // Horizontal
};

/**
 * Default hardcoded NES palette in RGB format.
 */
static constexpr const uint32_t defaultPaletteRGB[64] = {
    0x7c7c7c, 0x0000fc, 0x0000bc, 0x4428bc, 0x940084, 0xa80020, 0xa81000, 0x881400,
    0x503000, 0x007800, 0x006800, 0x005800, 0x004058, 0x000000, 0x000000, 0x000000,
    0xbcbcbc, 0x0078f8, 0x0058f8, 0x6844fc, 0xd800cc, 0xe40058, 0xf83800, 0xe45c10,
    0xac7c00, 0x00b800, 0x00a800, 0x00a844, 0x008888, 0x000000, 0x000000, 0x000000,
    0xf8f8f8, 0x3cbcfc, 0x6888fc, 0x9878f8, 0xf878f8, 0xf85898, 0xf87858, 0xfca044,
    0xf8b800, 0xb8f818, 0x58d854, 0x58f898, 0x00e8d8, 0x787878, 0x000000, 0x000000,
    0xfcfcfc, 0xa4e4fc, 0xb8b8f8, 0xd8b8f8, 0xf8b8f8, 0xf8a4c0, 0xf0d0b0, 0xfce0a8,
    0xf8d878, 0xd8f878, 0xb8f8b8, 0xb8f8d8, 0x00fcfc, 0xf8d8f8, 0x000000, 0x000000
};

CycleAccuratePPU::CycleAccuratePPU(SMBEmulator& engine) : engine(engine)
{
    // Initialize timing state
    currentScanline = 0;
    currentCycle = 0;
    frameComplete = false;
    frameBuffer = nullptr;
    
    // Initialize PPU registers to proper reset state
    ppuCtrl = 0x00;
    ppuMask = 0x00;
    ppuStatus = 0x80;  // VBlank flag set initially
    oamAddress = 0x00;
    ppuScrollX = 0x00;
    ppuScrollY = 0x00;
    currentAddress = 0;
    writeToggle = false;
    vramBuffer = 0x00;
    
    // Clear all memory
    memset(palette, 0, sizeof(palette));
    memset(nametable, 0, sizeof(nametable));
    memset(oam, 0, sizeof(oam));
    memset(chrBankHistory, 0, sizeof(chrBankHistory));
    
    // Set default background color (usually black)
    palette[0] = 0x0F;  // Black
    
    // Initialize frame capture state
    frameScrollX = 0;
    frameCtrl = 0;
    sprite0Hit = false;
}

bool CycleAccuratePPU::executeCycle(uint16_t* buffer)
{
    frameBuffer = buffer;
    frameComplete = false;
    
    // NES PPU timing: 341 cycles per scanline, 262 scanlines per frame
    // Scanlines 0-239: visible area
    // Scanline 240: post-render 
    // Scanlines 241-260: VBlank
    // Scanline 261: pre-render
    
    if (currentScanline >= 0 && currentScanline <= 239) {
        // Visible scanlines
        executeVisibleScanline();
    } else if (currentScanline == 240) {
        // Post-render scanline - do nothing
    } else if (currentScanline == 241 && currentCycle == 1) {
        // Start of VBlank
        ppuStatus |= 0x80;  // Set VBlank flag
        frameScrollX = ppuScrollX;  // Capture scroll for this frame
        frameCtrl = ppuCtrl;        // Capture control for this frame
    } else if (currentScanline == 261) {
        // Pre-render scanline
        if (currentCycle == 1) {
            ppuStatus &= 0x7F;  // Clear VBlank flag
            sprite0Hit = false; // Clear sprite 0 hit flag
        }
    }
    
    // Advance timing
    currentCycle++;
    if (currentCycle >= 341) {
        currentCycle = 0;
        currentScanline++;
        
        if (currentScanline >= 262) {
            currentScanline = 0;
            frameComplete = true;
        }
    }
    
    return frameComplete;
}

void CycleAccuratePPU::executeVisibleScanline()
{
    // Only render during active drawing cycles (1-256)
    if (currentCycle >= 1 && currentCycle <= 256) {
        int x = currentCycle - 1;  // Convert to 0-255 pixel position
        int y = currentScanline;
        
        renderPixel(x, y);
    }
}

void CycleAccuratePPU::renderPixel(int x, int y)
{
    if (!frameBuffer || x < 0 || x >= 256 || y < 0 || y >= 240) {
        return;
    }
    
    uint16_t backgroundColor = getBackgroundColor16();
    uint16_t finalColor = backgroundColor;
    
    // Render background pixel if enabled
    if (ppuMask & 0x08) {  // Background enabled
        uint16_t bgPixel = renderBackgroundPixel(x, y);
        if (bgPixel != 0) {
            finalColor = bgPixel;
        }
    }
    
    // Render sprite pixel if enabled
    if (ppuMask & 0x10) {  // Sprites enabled
        uint16_t spritePixel = renderSpritePixel(x, y);
        if (spritePixel != 0) {
            // Check sprite 0 hit
            if (isSpriteZeroAtPixel(x, y) && (finalColor != backgroundColor)) {
                sprite0Hit = true;
            }
            
            // Apply sprite priority (this is simplified - should check individual sprite priority)
            finalColor = spritePixel;
        }
    }
    
    frameBuffer[y * 256 + x] = finalColor;
}

uint16_t CycleAccuratePPU::renderBackgroundPixel(int x, int y)
{
    // Use captured frame values for consistent rendering
    int scrollX = frameScrollX + ((frameCtrl & 0x01) ? 256 : 0);
    
    // Status bar (rows 0-31) never scrolls
    if (y < 32) {
        int tileX = x / 8;
        int tileY = y / 8;
        uint16_t nametableAddr = 0x2000 + (tileY * 32) + tileX;
        return renderTilePixel(nametableAddr, x % 8, y % 8, currentScanline);
    }
    
    // Game area scrolls
    int scrolledX = x + scrollX;
    int tileX = scrolledX / 8;
    int tileY = y / 8;
    
    // Determine which nametable to use
    uint16_t baseAddr = (frameCtrl & 0x01) ? 0x2400 : 0x2000;
    
    // Handle horizontal wrapping
    if (tileX >= 32) {
        baseAddr = (frameCtrl & 0x01) ? 0x2000 : 0x2400;
        tileX -= 32;
    }
    
    uint16_t nametableAddr = baseAddr + (tileY * 32) + (tileX % 32);
    int pixelX = scrolledX % 8;
    int pixelY = y % 8;
    
    return renderTilePixel(nametableAddr, pixelX, pixelY, currentScanline);
}

uint16_t CycleAccuratePPU::renderSpritePixel(int x, int y)
{
    // Scan through OAM for sprites at this pixel (in reverse order for priority)
    for (int i = 63; i >= 0; i--) {
        uint8_t spriteY = oam[i * 4];
        uint8_t tileIndex = oam[i * 4 + 1];
        uint8_t attributes = oam[i * 4 + 2];
        uint8_t spriteX = oam[i * 4 + 3];
        
        // Check if sprite is visible
        if (spriteY >= 0xEF || spriteX >= 0xF9) {
            continue;
        }
        
        // Increment Y by one (sprite data is delayed by one scanline)
        spriteY++;
        
        // Check if pixel is within this sprite
        if (x < spriteX || x >= spriteX + 8 || y < spriteY || y >= spriteY + 8) {
            continue;
        }
        
        // Calculate pixel position within sprite
        int pixelX = x - spriteX;
        int pixelY = y - spriteY;
        
        // Apply flipping
        bool flipX = (attributes & 0x40) != 0;
        bool flipY = (attributes & 0x80) != 0;
        
        if (flipX) pixelX = 7 - pixelX;
        if (flipY) pixelY = 7 - pixelY;
        
        // Get tile pattern
        uint16_t tile = tileIndex + ((frameCtrl & 0x08) ? 256 : 0);  // Use captured control
        
        uint8_t plane1 = readByte(tile * 16 + pixelY);
        uint8_t plane2 = readByte(tile * 16 + pixelY + 8);
        
        // Extract pixel color
        uint8_t paletteIndex = (((plane1 >> (7 - pixelX)) & 1) | 
                               (((plane2 >> (7 - pixelX)) & 1) << 1));
        
        if (paletteIndex == 0) {
            continue;  // Transparent pixel
        }
        
        // Get sprite palette
        uint8_t spritePalette = attributes & 0x03;
        uint8_t colorIndex = palette[0x10 + spritePalette * 4 + paletteIndex];
        
        return convertColorTo16Bit(colorIndex);
    }
    
    return 0;  // No sprite pixel
}

uint16_t CycleAccuratePPU::renderTilePixel(uint16_t nametableAddr, int pixelX, int pixelY, int scanline)
{
    // Get tile index from nametable
    uint8_t tileIndex = readByte(nametableAddr);
    
    // DEBUGGING: Print tile info for first few tiles
    static int debugCount = 0;
    if (debugCount < 10) {
        printf("Tile render: addr=$%04X, tile=$%02X, x=%d, y=%d\n", 
               nametableAddr, tileIndex, pixelX, pixelY);
        debugCount++;
    }
    
    // Get pattern table base - FIXED LOGIC
    uint16_t patternTableBase;
    if (frameCtrl & 0x10) {
        patternTableBase = 0x1000;  // Pattern table 1 ($1000-$1FFF)
    } else {
        patternTableBase = 0x0000;  // Pattern table 0 ($0000-$0FFF)
    }
    
    uint16_t tileAddress = patternTableBase + (tileIndex * 16) + pixelY;
    
    // Read pattern data using mapper-aware reading
    uint8_t plane1 = readByte(tileAddress);
    uint8_t plane2 = readByte(tileAddress + 8);
    
    // DEBUGGING: Print pattern data for first few reads
    if (debugCount < 10) {
        printf("Pattern data: addr=$%04X, plane1=$%02X, plane2=$%02X\n", 
               tileAddress, plane1, plane2);
    }
    
    // Extract 2-bit color index
    uint8_t paletteIndex = (((plane1 >> (7 - pixelX)) & 1) | 
                           (((plane2 >> (7 - pixelX)) & 1) << 1));
    
    if (paletteIndex == 0) {
        return getBackgroundColor16();  // Use universal background color
    }
    
    // Get attribute table value for this tile
    uint8_t attribute = getAttributeTableValue(nametableAddr);
    uint8_t colorIndex = palette[attribute * 4 + paletteIndex];
    
    return convertColorTo16Bit(colorIndex);
}

uint8_t CycleAccuratePPU::readByte(uint16_t address)
{
    // Mirror all addresses above $3fff
    address &= 0x3fff;

    if (address < 0x2000) {
        // CHR data - USE MAPPER-AWARE READING
        return engine.readCHRData(address);  // Changed from engine.getCHR()[address]
    } else if (address < 0x3f00) {
        // Nametable
        return nametable[getNametableIndex(address)];
    }

    return 0;
}

void CycleAccuratePPU::writeByte(uint16_t address, uint8_t value)
{
    // Mirror all addresses above $3fff
    address &= 0x3fff;

    if (address < 0x2000) {
        // CHR-RAM write (if supported)
        engine.writeCHRData(address, value);
    } else if (address < 0x3f00) {
        // Nametable write
        nametable[getNametableIndex(address)] = value;
    } else if (address < 0x3f20) {
        // Palette data
        uint8_t paletteIndex = address - 0x3f00;
        palette[paletteIndex] = value;

        // Handle mirroring
        if (address == 0x3f10 || address == 0x3f14 || address == 0x3f18 || address == 0x3f1c) {
            palette[address - 0x3f10] = value;
        }
    }
}

uint16_t CycleAccuratePPU::getNametableIndex(uint16_t address)
{
    address = (address - 0x2000) % 0x1000;
    int table = address / 0x400;
    int offset = address % 0x400;
    
    // GET MIRRORING MODE FROM ENGINE/ROM HEADER
    int mode = engine.getMirroringMode();  // You'll need to add this method
    
    return (nametableMirrorLookup[mode][table] * 0x400 + offset) % 2048;
}

uint8_t CycleAccuratePPU::getAttributeTableValue(uint16_t nametableAddress)
{
    nametableAddress = getNametableIndex(nametableAddress);

    // Get the tile position within the nametable (32x30 tiles)
    int tileX = nametableAddress & 0x1f;  // 0-31
    int tileY = (nametableAddress >> 5) & 0x1f;  // 0-29
    
    // Convert tile position to attribute table position (16x16 pixel groups = 2x2 tiles)
    int attrX = tileX / 4;  // 0-7
    int attrY = tileY / 4;  // 0-7
    
    // Calculate which quadrant within the 4x4 tile group (32x32 pixels)
    int quadX = (tileX / 2) & 1;  // 0 or 1
    int quadY = (tileY / 2) & 1;  // 0 or 1
    
    // Calculate the shift amount for this quadrant
    int shift = (quadY * 4) + (quadX * 2);
    
    // Get the nametable base
    int nametableBase = (nametableAddress >= 0x400) ? 0x400 : 0x000;
    
    // Attribute table starts at +0x3C0 from nametable base
    int attrOffset = nametableBase + 0x3C0 + (attrY * 8) + attrX;
    
    // Extract the 2-bit palette value and return it
    return (nametable[attrOffset] >> shift) & 0x03;
}

uint16_t CycleAccuratePPU::convertColorTo16Bit(uint8_t colorIndex)
{
    if (colorIndex >= 64) return 0;
    
    uint32_t rgb32 = defaultPaletteRGB[colorIndex];
    
    // Convert RGB888 to RGB565
    uint16_t r = (rgb32 >> 16) & 0xFF;
    uint16_t g = (rgb32 >> 8) & 0xFF;
    uint16_t b = rgb32 & 0xFF;
    
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3);
}

uint16_t CycleAccuratePPU::getBackgroundColor16()
{
    return convertColorTo16Bit(palette[0]);
}

bool CycleAccuratePPU::isSpriteZeroAtPixel(int x, int y)
{
    // Check if sprite 0 is at this pixel position
    uint8_t spriteY = oam[0];
    uint8_t spriteX = oam[3];
    
    if (spriteY >= 0xEF || spriteX >= 0xF9) {
        return false;
    }
    
    spriteY++;  // Sprite data delayed by one scanline
    
    return (x >= spriteX && x < spriteX + 8 && y >= spriteY && y < spriteY + 8);
}

uint8_t CycleAccuratePPU::readDataRegister()
{
    uint8_t value;
    
    if (currentAddress < 0x3F00) {
        // Normal VRAM - return buffered value
        value = vramBuffer;
        vramBuffer = readByte(currentAddress);
    } else {
        // Palette RAM - return immediately but also update buffer
        value = readByte(currentAddress);
        vramBuffer = readByte(currentAddress - 0x1000); // Mirror to nametable
    }

    // Increment address
    if (ppuCtrl & 0x04) {
        currentAddress += 32;  // Vertical increment
    } else {
        currentAddress += 1;   // Horizontal increment
    }
    
    return value;
}

void CycleAccuratePPU::writeAddressRegister(uint8_t value)
{
    if (!writeToggle) {
        // Upper byte
        currentAddress = (currentAddress & 0xff) | (((uint16_t)value << 8) & 0xff00);
    } else {
        // Lower byte
        currentAddress = (currentAddress & 0xff00) | (uint16_t)value;
    }
    writeToggle = !writeToggle;
}

void CycleAccuratePPU::writeDataRegister(uint8_t value)
{
    writeByte(currentAddress, value);
    
    if (ppuCtrl & 0x04) {
        currentAddress += 32;  // Vertical increment
    } else {
        currentAddress += 1;   // Horizontal increment
    }
}

uint8_t CycleAccuratePPU::readRegister(uint16_t address)
{
    switch(address) {
    case 0x2002: // PPUSTATUS
    {
        uint8_t status = ppuStatus;
        
        // Add sprite 0 hit flag (bit 6)
        if (sprite0Hit) {
            status |= 0x40;
        }
        
        writeToggle = false;
        
        // Clear VBlank flag after reading
        ppuStatus &= 0x7F;
        sprite0Hit = false;  // Clear sprite 0 hit
        
        return status;
    }

    case 0x2004: // OAMDATA
        return oam[oamAddress];
        
    case 0x2007: // PPUDATA
        return readDataRegister();
        
    default:
        break;
    }

    return 0;
}

void CycleAccuratePPU::writeRegister(uint16_t address, uint8_t value)
{
    switch(address) {
    case 0x2000: // PPUCTRL
        ppuCtrl = value;
        break;
        
    case 0x2001: // PPUMASK
        ppuMask = value;
        break;
        
    case 0x2003: // OAMADDR
        oamAddress = value;
        break;
        
    case 0x2004: // OAMDATA
        oam[oamAddress] = value;
        oamAddress++;
        break;
        
    case 0x2005: // PPUSCROLL
        if (!writeToggle) {
            ppuScrollX = value;
        } else {
            ppuScrollY = value;
        }
        writeToggle = !writeToggle;
        break;
        
    case 0x2006: // PPUADDR
        writeAddressRegister(value);
        break;
        
    case 0x2007: // PPUDATA
        writeDataRegister(value);
        break;
        
    default:
        break;
    }
}

void CycleAccuratePPU::writeDMA(uint8_t page)
{
    uint16_t address = (uint16_t)page << 8;
    for (int i = 0; i < 256; i++) {
        oam[oamAddress] = engine.readData(address);
        address++;
        oamAddress++;
    }
}

void CycleAccuratePPU::render(uint32_t* buffer) {
    if (!buffer) return;
    
    // Render the full frame
    for (int y = 0; y < 240; y++) {
        for (int x = 0; x < 256; x++) {
            uint16_t backgroundColor = getBackgroundColor16();
            uint16_t finalColor = backgroundColor;
            
            // Render background pixel if enabled
            if (ppuMask & 0x08) {
                uint16_t bgPixel = renderBackgroundPixel(x, y);
                if (bgPixel != 0) {
                    finalColor = bgPixel;
                }
            }
            
            // Render sprite pixel if enabled
            if (ppuMask & 0x10) {
                uint16_t spritePixel = renderSpritePixel(x, y);
                if (spritePixel != 0) {
                    finalColor = spritePixel;
                }
            }
            
            // Convert 16-bit to 32-bit color
            uint32_t color32 = convert16BitTo32Bit(finalColor);
            buffer[y * 256 + x] = 0xFF000000 | color32;
        }
    }
}

void CycleAccuratePPU::renderScaled(uint16_t* buffer, int screenWidth, int screenHeight) {
    if (!buffer) return;
    
    // First render to temporary NES buffer
    static uint16_t nesBuffer[256 * 240];
    
    for (int y = 0; y < 240; y++) {
        for (int x = 0; x < 256; x++) {
            uint16_t backgroundColor = getBackgroundColor16();
            uint16_t finalColor = backgroundColor;
            
            // Render background pixel if enabled
            if (ppuMask & 0x08) {
                uint16_t bgPixel = renderBackgroundPixel(x, y);
                if (bgPixel != 0) {
                    finalColor = bgPixel;
                }
            }
            
            // Render sprite pixel if enabled
            if (ppuMask & 0x10) {
                uint16_t spritePixel = renderSpritePixel(x, y);
                if (spritePixel != 0) {
                    finalColor = spritePixel;
                }
            }
            
            nesBuffer[y * 256 + x] = finalColor;
        }
    }
    
    // Scale to output buffer
    scaleBuffer16(nesBuffer, buffer, screenWidth, screenHeight);
}

void CycleAccuratePPU::renderScaled32(uint32_t* buffer, int screenWidth, int screenHeight) {
    if (!buffer) return;
    
    // First render to temporary 32-bit buffer
    static uint32_t nesBuffer[256 * 240];
    render(nesBuffer);
    
    // Scale to output buffer
    scaleBuffer32(nesBuffer, buffer, screenWidth, screenHeight);
}

uint32_t CycleAccuratePPU::convert16BitTo32Bit(uint16_t color16) {
    // Extract RGB565 components
    int r = (color16 >> 11) & 0x1F;
    int g = (color16 >> 5) & 0x3F;
    int b = color16 & 0x1F;
    
    // Scale to 8-bit
    r = (r << 3) | (r >> 2);
    g = (g << 2) | (g >> 4);
    b = (b << 3) | (b >> 2);
    
    return (r << 16) | (g << 8) | b;
}

void CycleAccuratePPU::scaleBuffer16(uint16_t* nesBuffer, uint16_t* outputBuffer, int screenWidth, int screenHeight) {
    // Clear output buffer
    for (int i = 0; i < screenWidth * screenHeight; i++) {
        outputBuffer[i] = 0x0000;
    }
    
    // Calculate scaling
    int scale_x = screenWidth / 256;
    int scale_y = screenHeight / 240;
    int scale = (scale_x < scale_y) ? scale_x : scale_y;
    if (scale < 1) scale = 1;
    
    int dest_w = 256 * scale;
    int dest_h = 240 * scale;
    int dest_x = (screenWidth - dest_w) / 2;
    int dest_y = (screenHeight - dest_h) / 2;
    
    // Scale pixels
    for (int y = 0; y < 240; y++) {
        for (int x = 0; x < 256; x++) {
            uint16_t pixel = nesBuffer[y * 256 + x];
            
            for (int sy = 0; sy < scale; sy++) {
                for (int sx = 0; sx < scale; sx++) {
                    int out_x = dest_x + x * scale + sx;
                    int out_y = dest_y + y * scale + sy;
                    
                    if (out_x >= 0 && out_x < screenWidth && out_y >= 0 && out_y < screenHeight) {
                        outputBuffer[out_y * screenWidth + out_x] = pixel;
                    }
                }
            }
        }
    }
}

void CycleAccuratePPU::scaleBuffer32(uint32_t* nesBuffer, uint32_t* outputBuffer, int screenWidth, int screenHeight) {
    // Clear output buffer
    for (int i = 0; i < screenWidth * screenHeight; i++) {
        outputBuffer[i] = 0xFF000000;
    }
    
    // Calculate scaling
    int scale_x = screenWidth / 256;
    int scale_y = screenHeight / 240;
    int scale = (scale_x < scale_y) ? scale_x : scale_y;
    if (scale < 1) scale = 1;
    
    int dest_w = 256 * scale;
    int dest_h = 240 * scale;
    int dest_x = (screenWidth - dest_w) / 2;
    int dest_y = (screenHeight - dest_h) / 2;
    
    // Scale pixels
    for (int y = 0; y < 240; y++) {
        for (int x = 0; x < 256; x++) {
            uint32_t pixel = nesBuffer[y * 256 + x];
            
            for (int sy = 0; sy < scale; sy++) {
                for (int sx = 0; sx < scale; sx++) {
                    int out_x = dest_x + x * scale + sx;
                    int out_y = dest_y + y * scale + sy;
                    
                    if (out_x >= 0 && out_x < screenWidth && out_y >= 0 && out_y < screenHeight) {
                        outputBuffer[out_y * screenWidth + out_x] = pixel;
                    }
                }
            }
        }
    }
}
