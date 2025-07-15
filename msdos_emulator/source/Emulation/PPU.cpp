#include "../SMB/SMBEmulator.hpp"

#include "PPU.hpp"

ComprehensiveTileCache PPU::g_comprehensiveCache[512 * 8];
bool PPU::g_comprehensiveCacheInit = false;
std::vector<FlipCacheEntry> PPU::g_flipCache;
std::unordered_map<uint32_t, size_t> PPU::g_flipCacheIndex;
PPU::ScalingCache PPU::g_scalingCache;
std::unordered_map<uint8_t, BankTileCache*> PPU::g_bankCaches;
bool PPU::g_bankCacheInit = false;

static const uint8_t nametableMirrorLookup[][4] = {
    {0, 0, 1, 1}, // Vertical
    {0, 1, 0, 1}  // Horizontal
};

/**
 * Default hardcoded palette.
 */
static constexpr const uint32_t defaultPaletteRGB[64] = {
    0x7c7c7c,
    0x0000fc,
    0x0000bc,
    0x4428bc,
    0x940084,
    0xa80020,
    0xa81000,
    0x881400,
    0x503000,
    0x007800,
    0x006800,
    0x005800,
    0x004058,
    0x000000,
    0x000000,
    0x000000,
    0xbcbcbc,
    0x0078f8,
    0x0058f8,
    0x6844fc,
    0xd800cc,
    0xe40058,
    0xf83800,
    0xe45c10,
    0xac7c00,
    0x00b800,
    0x00a800,
    0x00a844,
    0x008888,
    0x000000,
    0x000000,
    0x000000,
    0xf8f8f8,
    0x3cbcfc,
    0x6888fc,
    0x9878f8,
    0xf878f8,
    0xf85898,
    0xf87858,
    0xfca044,
    0xf8b800,
    0xb8f818,
    0x58d854,
    0x58f898,
    0x00e8d8,
    0x787878,
    0x000000,
    0x000000,
    0xfcfcfc,
    0xa4e4fc,
    0xb8b8f8,
    0xd8b8f8,
    0xf8b8f8,
    0xf8a4c0,
    0xf0d0b0,
    0xfce0a8,
    0xf8d878,
    0xd8f878,
    0xb8f8b8,
    0xb8f8d8,
    0x00fcfc,
    0xf8d8f8,
    0x000000,
    0x000000
};

/**
 * RGB representation of the NES palette.
 */
const uint32_t* paletteRGB = defaultPaletteRGB;

PPU::PPU(SMBEmulator& engine) :
    engine(engine)
{
    currentAddress = 0;
    writeToggle = false;
    
    // Initialize PPU registers to proper reset state for SMB
    ppuCtrl = 0x00;
    ppuMask = 0x00;
    ppuStatus = 0x80;  // VBlank flag set initially (IMPORTANT FOR SMB)
    oamAddress = 0x00;
    ppuScrollX = 0x00;
    ppuScrollY = 0x00;
    vramBuffer = 0x00;
    
    // Clear all memory
    memset(palette, 0, sizeof(palette));
    memset(nametable, 0, sizeof(nametable));
    memset(oam, 0, sizeof(oam));
    sprite0Hit = false;
    // Set default background color (usually black)
    palette[0] = 0x0F;  // Black
    cachedScrollX = 0;
    cachedScrollY = 0;
    cachedCtrl = 0;
    renderScrollX = 0;
    renderScrollY = 0;
    renderCtrl = 0;
    gameAreaScrollX = 0;
    ignoreNextScrollWrite = false;
    frameScrollX = 0;
    frameCtrl = 0;
}

uint8_t PPU::getAttributeTableValue(uint16_t nametableAddress)
{
    nametableAddress = getNametableIndex(nametableAddress);

    // Get the tile position within the nametable (32x30 tiles)
    int tileX = nametableAddress & 0x1f;  // 0-31
    int tileY = (nametableAddress >> 5) & 0x1f;  // 0-29 (but we only use 0-29)
    
    // Convert tile position to attribute table position (16x16 pixel groups = 2x2 tiles)
    int attrX = tileX / 4;  // 0-7 (8 groups horizontally)
    int attrY = tileY / 4;  // 0-7 (8 groups vertically)
    
    // Calculate which quadrant within the 4x4 tile group (32x32 pixels)
    int quadX = (tileX / 2) & 1;  // 0 or 1
    int quadY = (tileY / 2) & 1;  // 0 or 1
    
    // Calculate the shift amount for this quadrant
    int shift = (quadY * 4) + (quadX * 2);
    
    // Get the nametable base (which 1KB nametable we're in)
    int nametableBase = (nametableAddress >= 0x400) ? 0x400 : 0x000;
    
    // Attribute table starts at +0x3C0 from nametable base
    int attrOffset = nametableBase + 0x3C0 + (attrY * 8) + attrX;
    
    // Extract the 2-bit palette value and return it
    return (nametable[attrOffset] >> shift) & 0x03;
}

uint16_t PPU::getNametableIndex(uint16_t address)
{
    address = (address - 0x2000) % 0x1000;
    int table = address / 0x400;
    int offset = address % 0x400;
    int mode = 1; // Mirroring mode for Super Mario Bros.
    return (nametableMirrorLookup[mode][table] * 0x400 + offset) % 2048;
}

uint8_t PPU::readByte(uint16_t address)
{
    // Mirror all addresses above $3fff
    address &= 0x3fff;

    if (address < 0x2000)
    {
        // CHR
        return engine.getCHR()[address];
    }
    else if (address < 0x3f00)
    {
        // Nametable
        return nametable[getNametableIndex(address)];
    }

    return 0;
}

uint8_t PPU::readCHR(int index)
{
    if (index < 0x2000)
    {
        return engine.readCHRData(index);
    }
    else
    {
        return 0;
    }
}

uint8_t PPU::readDataRegister()
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

void PPU::setSprite0Hit(bool hit) {
    sprite0Hit = hit;
}

uint8_t PPU::readRegister(uint16_t address)
{
   switch(address)
   {
   case 0x2002: // PPUSTATUS
   {
       uint8_t status = ppuStatus;
       
       // Add sprite 0 hit flag (bit 6)
       if (sprite0Hit) {
           status |= 0x40;
       }
       
       writeToggle = false;
       
       // Clear VBlank flag AND sprite 0 hit flag after reading
       if (ppuStatus & 0x80) {
           ppuStatus &= 0x7F;
       }
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

void PPU::setVBlankFlag(bool flag)
{
    if (flag) {
        ppuStatus |= 0x80;  // Set VBlank flag
    } else {
        ppuStatus &= 0x7F;  // Clear VBlank flag
    }
}



void PPU::renderTile(uint32_t* buffer, int index, int xOffset, int yOffset)
{
    // Lookup the pattern table entry
    uint16_t tile = readByte(index) + (ppuCtrl & (1 << 4) ? 256 : 0);
    uint8_t attribute = getAttributeTableValue(index);

    // Read the pixels of the tile
    for( int row = 0; row < 8; row++ )
    {
        uint8_t plane1 = readCHR(tile * 16 + row);
        uint8_t plane2 = readCHR(tile * 16 + row + 8);

        for( int column = 0; column < 8; column++ )
        {
            uint8_t paletteIndex = (((plane1 & (1 << column)) ? 1 : 0) + ((plane2 & (1 << column)) ? 2 : 0));
            uint8_t colorIndex = palette[attribute * 4 + paletteIndex];
            if( paletteIndex == 0 )
            {
                // skip transparent pixels
                //colorIndex = palette[0];
                continue;
            }
            uint32_t pixel = 0xff000000 | paletteRGB[colorIndex];

            int x = (xOffset + (7 - column));
            int y = (yOffset + row);
            if (x < 0 || x >= 256 || y < 0 || y >= 240)
            {
                continue;
            }
            buffer[y * 256 + x] = pixel;
        }
    }

}

void PPU::render(uint32_t* buffer)
{
    // Clear the buffer with the background color
    for (int index = 0; index < 256 * 240; index++)
    {
        buffer[index] = paletteRGB[palette[0]];
    }

    // Draw sprites behind the backround
    if (ppuMask & (1 << 4)) // Are sprites enabled?
    {
        // Sprites with the lowest index in OAM take priority.
        // Therefore, render the array of sprites in reverse order.
        //
        for (int i = 63; i >= 0; i--)
        {
            // Read OAM for the sprite
            uint8_t y          = oam[i * 4];
            uint8_t index      = oam[i * 4 + 1];
            uint8_t attributes = oam[i * 4 + 2];
            uint8_t x          = oam[i * 4 + 3];

            // Check if the sprite has the correct priority
            if (!(attributes & (1 << 5)))
            {
                continue;
            }

            // Check if the sprite is visible
            if( y >= 0xef || x >= 0xf9 )
            {
                continue;
            }

            // Increment y by one since sprite data is delayed by one scanline
            //
            y++;

            // Determine the tile to use
            uint16_t tile = index + (ppuCtrl & (1 << 3) ? 256 : 0);
            bool flipX = attributes & (1 << 6);
            bool flipY = attributes & (1 << 7);

            // Copy pixels to the framebuffer
            for( int row = 0; row < 8; row++ )
            {
                uint8_t plane1 = readCHR(tile * 16 + row);
                uint8_t plane2 = readCHR(tile * 16 + row + 8);

                for( int column = 0; column < 8; column++ )
                {
                    uint8_t paletteIndex = (((plane1 & (1 << column)) ? 1 : 0) + ((plane2 & (1 << column)) ? 2 : 0));
                    uint8_t colorIndex = palette[0x10 + (attributes & 0x03) * 4 + paletteIndex];
                    if( paletteIndex == 0 )
                    {
                        // Skip transparent pixels
                        continue;
                    }
                    uint32_t pixel = 0xff000000 | paletteRGB[colorIndex];

                    int xOffset = 7 - column;
                    if( flipX )
                    {
                        xOffset = column;
                    }
                    int yOffset = row;
                    if( flipY )
                    {
                        yOffset = 7 - row;
                    }

                    int xPixel = (int)x + xOffset;
                    int yPixel = (int)y + yOffset;
                    if (xPixel < 0 || xPixel >= 256 || yPixel < 0 || yPixel >= 240)
                    {
                        continue;
                    }

                    buffer[yPixel * 256 + xPixel] = pixel;
                }
            }
        }
    }

    // Draw the background (nametable)
    if (ppuMask & (1 << 3)) // Is the background enabled?
    {
        int scrollX = (int)ppuScrollX + ((ppuCtrl & (1 << 0)) ? 256 : 0);
        int xMin = scrollX / 8;
        int xMax = ((int)scrollX + 256) / 8;
        for (int x = 0; x < 32; x++)
        {
            for (int y = 0; y < 4; y++)
            {
                // Render the status bar in the same position (it doesn't scroll)
                renderTile(buffer, 0x2000 + 32 * y + x, x * 8, y * 8);
            }
        }
        for (int x = xMin; x <= xMax; x++)
        {
            for (int y = 4; y < 30; y++)
            {
                // Determine the index of the tile to render
                int index;
                if (x < 32)
                {
                    index = 0x2000 + 32 * y + x;
                }
                else if (x < 64)
                {
                    index = 0x2400 + 32 * y + (x - 32);
                }
                else
                {
                    index = 0x2800 + 32 * y + (x - 64);
                }

                // Render the tile
                renderTile(buffer, index, (x * 8) - (int)scrollX, (y * 8));
            }
        }
    }

    // Draw sprites in front of the background
    if (ppuMask & (1 << 4))
    {
        // Sprites with the lowest index in OAM take priority.
        // Therefore, render the array of sprites in reverse order.
        //
        // We render sprite 0 first as a special case (coin indicator).
        //
        for (int j = 64; j > 0; j--)
        {
            // Start at 0, then 63, 62, 61, ..., 1
            //
            int i = j % 64;

            // Read OAM for the sprite
            uint8_t y          = oam[i * 4];
            uint8_t index      = oam[i * 4 + 1];
            uint8_t attributes = oam[i * 4 + 2];
            uint8_t x          = oam[i * 4 + 3];

            // Check if the sprite has the correct priority
            //
            // Special case for sprite 0, tile 0xff in Super Mario Bros.
            // (part of the pixels for the coin indicator)
            //
            if (attributes & (1 << 5) && !(i == 0 && index == 0xff))
            {
                continue;
            }

            // Check if the sprite is visible
            if( y >= 0xef || x >= 0xf9 )
            {
                continue;
            }

            // Increment y by one since sprite data is delayed by one scanline
            //
            y++;

            // Determine the tile to use
            uint16_t tile = index + (ppuCtrl & (1 << 3) ? 256 : 0);
            bool flipX = attributes & (1 << 6);
            bool flipY = attributes & (1 << 7);

            // Copy pixels to the framebuffer
            for( int row = 0; row < 8; row++ )
            {
                uint8_t plane1 = readCHR(tile * 16 + row);
                uint8_t plane2 = readCHR(tile * 16 + row + 8);

                for( int column = 0; column < 8; column++ )
                {
                    uint8_t paletteIndex = (((plane1 & (1 << column)) ? 1 : 0) + ((plane2 & (1 << column)) ? 2 : 0));
                    uint8_t colorIndex = palette[0x10 + (attributes & 0x03) * 4 + paletteIndex];
                    if( paletteIndex == 0 )
                    {
                        // Skip transparent pixels
                        continue;
                    }
                    uint32_t pixel = 0xff000000 | paletteRGB[colorIndex];

                    int xOffset = 7 - column;
                    if( flipX )
                    {
                        xOffset = column;
                    }
                    int yOffset = row;
                    if( flipY )
                    {
                        yOffset = 7 - row;
                    }

                    int xPixel = (int)x + xOffset;
                    int yPixel = (int)y + yOffset;
                    if (xPixel < 0 || xPixel >= 256 || yPixel < 0 || yPixel >= 240)
                    {
                        continue;
                    }

                    // Special case for sprite 0, tile 0xff in Super Mario Bros.
                    // (part of the pixels for the coin indicator)
                    //
                    if (i == 0 && index == 0xff && row == 5 && column > 3 && column < 6)
                    {
                        continue;
                    }

                    buffer[yPixel * 256 + xPixel] = pixel;
                }
            }
        }
    }
}

int PPU::getTileCacheIndex(uint16_t tile, uint8_t palette_type, uint8_t attribute)
{
    // Back to original indexing since each bank has its own cache array
    return (tile * 8) + (palette_type & 0x7);
}

void PPU::cacheTileAllVariations(uint16_t tile, uint8_t palette_type, uint8_t attribute, uint8_t chr_bank)
{
    ensureBankCacheExists(chr_bank);
    BankTileCache* bankCache = getBankCache(chr_bank);
    if (!bankCache || !bankCache->is_allocated) return;
    
    int cacheIndex = getTileCacheIndex(tile, palette_type, attribute);
    if (cacheIndex >= 512 * 8) return;
    
    ComprehensiveTileCache& cache = bankCache->tiles[cacheIndex];
    
    // Check if already cached
    if (cache.is_valid && cache.tile_id == tile && 
        cache.palette_type == palette_type && cache.attribute == attribute) {
        return;  // Already cached
    }
    
    // Cache the tile data from this specific bank
    for (int row = 0; row < 8; row++) {
        uint8_t plane1 = readCHRFromBank(tile * 16 + row, chr_bank);
        uint8_t plane2 = readCHRFromBank(tile * 16 + row + 8, chr_bank);

        for (int column = 0; column < 8; column++) {
            uint8_t paletteIndex = (((plane1 & (1 << column)) ? 1 : 0) + 
                                   ((plane2 & (1 << column)) ? 2 : 0));
            
            uint8_t colorIndex;
            uint16_t pixel16;
            
            if (palette_type == 0) {
                if (paletteIndex == 0) {
                    colorIndex = palette[0];
                } else {
                    colorIndex = palette[(attribute & 0x03) * 4 + paletteIndex];
                }
                uint32_t pixel32 = paletteRGB[colorIndex];
                pixel16 = ((pixel32 & 0xF80000) >> 8) | ((pixel32 & 0x00FC00) >> 5) | ((pixel32 & 0x0000F8) >> 3);
            } else {
                if (paletteIndex == 0) {
                    pixel16 = 0;
                } else {
                    colorIndex = palette[0x10 + ((palette_type - 1) & 0x03) * 4 + paletteIndex];
                    uint32_t pixel32 = paletteRGB[colorIndex];
                    pixel16 = ((pixel32 & 0xF80000) >> 8) | ((pixel32 & 0x00FC00) >> 5) | ((pixel32 & 0x0000F8) >> 3);
                    if (pixel16 == 0) pixel16 = 1;
                }
            }
            
            int pixelIndex = row * 8 + (7 - column);
            cache.pixels[pixelIndex] = pixel16;
        }
    }
    
    cache.tile_id = tile;
    cache.palette_type = palette_type;
    cache.attribute = attribute;
    cache.is_valid = true;
}

void PPU::renderCachedTile(uint16_t* buffer, int index, int xOffset, int yOffset, bool flipX, bool flipY)
{
    uint16_t tile = readByte(index) + (ppuCtrl & (1 << 4) ? 256 : 0);
    uint8_t attribute = getAttributeTableValue(index);
    uint8_t chr_bank = frameCHRBank;
    
    ensureBankCacheExists(chr_bank);
    BankTileCache* bankCache = getBankCache(chr_bank);
    if (!bankCache || !bankCache->is_allocated) return;
    
    uint16_t* pixels;
    
    if (!flipX && !flipY) {
        // Use normal cache
        cacheTileAllVariations(tile, 0, attribute, chr_bank);
        int cacheIndex = getTileCacheIndex(tile, 0, attribute);
        if (cacheIndex >= 512 * 8) return;
        pixels = bankCache->tiles[cacheIndex].pixels;
    } else {
        // Use flip cache
        cacheFlipVariation(tile, 0, attribute, flipX, flipY, chr_bank);
        
        uint8_t flip_flags = (flipY ? 2 : 0) | (flipX ? 1 : 0);
        uint32_t key = getFlipCacheKey(tile, 0, attribute, flip_flags);
        
        auto it = bankCache->flipCacheIndex.find(key);
        if (it == bankCache->flipCacheIndex.end()) return;
        
        pixels = bankCache->flipCache[it->second].pixels;
    }
    
    // Draw pixels
    int pixelIndex = 0;
    for (int row = 0; row < 8; row++) {
        int y = yOffset + row;
        if (y < 0 || y >= 240) {
            pixelIndex += 8;
            continue;
        }
        
        for (int column = 0; column < 8; column++) {
            int x = xOffset + column;
            if (x >= 0 && x < 256) {
                buffer[y * 256 + x] = pixels[pixelIndex];
            }
            pixelIndex++;
        }
    }
}

void PPU::renderCachedSprite(uint16_t* buffer, uint16_t tile, uint8_t sprite_palette, int xOffset, int yOffset, bool flipX, bool flipY)
{
    uint8_t palette_type = (sprite_palette & 0x03) + 1;
    uint8_t chr_bank = frameCHRBank;  // Use current frame's CHR bank
    uint16_t* pixels;
    
    ensureBankCacheExists(chr_bank);
    BankTileCache* bankCache = getBankCache(chr_bank);
    if (!bankCache || !bankCache->is_allocated) return;
    
    if (!flipX && !flipY) {
        // Use normal cache
        cacheTileAllVariations(tile, palette_type, 0, chr_bank);
        int cacheIndex = getTileCacheIndex(tile, palette_type, 0);
        if (cacheIndex >= 512 * 8) return;
        pixels = bankCache->tiles[cacheIndex].pixels;
    } else {
        // Use flip cache
        cacheFlipVariation(tile, palette_type, 0, flipX, flipY, chr_bank);
        
        uint8_t flip_flags = (flipY ? 2 : 0) | (flipX ? 1 : 0);
        uint32_t key = getFlipCacheKey(tile, palette_type, 0, flip_flags);
        
        auto it = bankCache->flipCacheIndex.find(key);
        if (it == bankCache->flipCacheIndex.end()) return;
        
        pixels = bankCache->flipCache[it->second].pixels;
    }
    
    // Draw sprites with transparency
    int pixelIndex = 0;
    for (int row = 0; row < 8; row++) {
        int y = yOffset + row;
        if (y < 0 || y >= 240) {
            pixelIndex += 8;
            continue;
        }
        
        for (int column = 0; column < 8; column++) {
            int x = xOffset + column;
            if (x >= 0 && x < 256) {
                uint16_t pixel = pixels[pixelIndex];
                if (pixel != 0) {
                    buffer[y * 256 + x] = pixel;
                }
            }
            pixelIndex++;
        }
    }
}



void PPU::render16(uint16_t* buffer)
{
    // Clear the buffer
    uint32_t bgColor32 = paletteRGB[palette[0]];
    uint16_t bgColor16 = ((bgColor32 & 0xF80000) >> 8) | ((bgColor32 & 0x00FC00) >> 5) | ((bgColor32 & 0x0000F8) >> 3);
    
    for (int index = 0; index < 256 * 240; index++)
    {
        buffer[index] = bgColor16;
    }

    // Use the captured frame scroll AND control values
    int scrollOffset = frameScrollX;
    uint8_t baseNametable = frameCtrl & 0x01;  // Use captured nametable selection
    
    static uint8_t lastFrameScroll = 255;
    static uint8_t lastFrameCtrl = 255;
    if (frameScrollX != lastFrameScroll || frameCtrl != lastFrameCtrl) {
        lastFrameScroll = frameScrollX;
        lastFrameCtrl = frameCtrl;
    }

    if (ppuMask & (1 << 3))
    {
        // Status bar (rows 0-3) - ALWAYS from nametable 0, never scrolled
        for (int x = 0; x < 32; x++) {
            for (int y = 0; y < 4; y++) {
                renderCachedTile(buffer, 0x2000 + 32 * y + x, x * 8, y * 8, false, false);
            }
        }
        
        // Game area (rows 4-29) - use captured scroll and nametable
        int leftmostTile = scrollOffset / 8;
        int rightmostTile = (scrollOffset + 256) / 8 + 1;
        
        for (int tileX = leftmostTile; tileX <= rightmostTile; tileX++) {
            for (int tileY = 4; tileY < 30; tileY++) {
                int screenX = (tileX * 8) - scrollOffset;
                int screenY = tileY * 8;
                
                if (screenX < -8 || screenX >= 256) continue;
                
                // Use the base nametable from captured control register
                uint16_t baseAddr = baseNametable ? 0x2400 : 0x2000;
                
                // Handle wrapping within the 32x32 tile nametable
                int localTileX = tileX % 32;
                if (localTileX < 0) localTileX += 32;
                
                // For SMB horizontal scrolling, when we go past 32 tiles, 
                // we need to switch to the other nametable
                if (tileX >= 32) {
                    baseAddr = baseNametable ? 0x2000 : 0x2400;  // Switch to other nametable
                    localTileX = tileX - 32;
                }
                
                uint16_t nametableAddr = baseAddr + (tileY * 32) + localTileX;
                renderCachedTile(buffer, nametableAddr, screenX, screenY, false, false);
            }
        }
    }

    // Sprites (unchanged)
    if (ppuMask & (1 << 4))
    {
        for (int i = 63; i >= 0; i--)
        {
            uint8_t y          = oam[i * 4];
            uint8_t index      = oam[i * 4 + 1];
            uint8_t attributes = oam[i * 4 + 2];
            uint8_t x          = oam[i * 4 + 3];

            if (y >= 0xef || x >= 0xf9) continue;

            y++;
            uint16_t tile = index + (ppuCtrl & (1 << 3) ? 256 : 0);
            bool flipX = attributes & (1 << 6);
            bool flipY = attributes & (1 << 7);
            uint8_t sprite_palette = attributes & 0x03;
            bool behindBackground = (attributes & (1 << 5)) != 0;

            renderCachedSpriteWithPriority(buffer, tile, sprite_palette, x, y, flipX, flipY, behindBackground);
        }
    }
}

void PPU::updateRenderRegisters()
{
    uint8_t oldRenderScrollX = renderScrollX;
    
    // Use the game area scroll value instead of the alternating one
    renderScrollX = gameAreaScrollX;
    renderScrollY = ppuScrollY;
    renderCtrl = ppuCtrl;
    
}


void PPU::renderCachedSpriteWithPriority(uint16_t* buffer, uint16_t tile, uint8_t sprite_palette, int xOffset, int yOffset, bool flipX, bool flipY, bool behindBackground)
{
    // Cache the sprite tile
    uint8_t palette_type = (sprite_palette & 0x03) + 1;
    uint8_t chr_bank = frameCHRBank;  // Use current frame's CHR bank
    uint16_t* pixels;
    
    ensureBankCacheExists(chr_bank);
    BankTileCache* bankCache = getBankCache(chr_bank);
    if (!bankCache || !bankCache->is_allocated) return;
    
    if (!flipX && !flipY) {
        // Use normal cache
        cacheTileAllVariations(tile, palette_type, 0, chr_bank);
        int cacheIndex = getTileCacheIndex(tile, palette_type, 0);
        if (cacheIndex >= 512 * 8) return;
        pixels = bankCache->tiles[cacheIndex].pixels;
    } else {
        // Use flip cache
        cacheFlipVariation(tile, palette_type, 0, flipX, flipY, chr_bank);
        
        uint8_t flip_flags = (flipY ? 2 : 0) | (flipX ? 1 : 0);
        uint32_t key = getFlipCacheKey(tile, palette_type, 0, flip_flags);
        
        auto it = bankCache->flipCacheIndex.find(key);
        if (it == bankCache->flipCacheIndex.end()) return;
        
        pixels = bankCache->flipCache[it->second].pixels;
    }
    
    // Draw sprites with background priority checking
    int pixelIndex = 0;
    for (int row = 0; row < 8; row++) {
        int y = yOffset + row;
        if (y < 0 || y >= 240) {
            pixelIndex += 8;
            continue;
        }
        
        for (int column = 0; column < 8; column++) {
            int x = xOffset + column;
            if (x >= 0 && x < 256) {
                uint16_t spritePixel = pixels[pixelIndex];
                if (spritePixel != 0) {  // Non-transparent sprite pixel
                    uint16_t backgroundPixel = buffer[y * 256 + x];
                    uint32_t bgColor32 = paletteRGB[palette[0]];
                    uint16_t bgColor16 = ((bgColor32 & 0xF80000) >> 8) | ((bgColor32 & 0x00FC00) >> 5) | ((bgColor32 & 0x0000F8) >> 3);
                    
                    // Check if background pixel is non-transparent (not the universal background color)
                    bool backgroundVisible = (backgroundPixel != bgColor16);
                    
                    // Priority logic:
                    // - If sprite is behind background AND background is visible, don't draw sprite
                    // - Otherwise, draw sprite
                    if (!behindBackground || !backgroundVisible) {
                        buffer[y * 256 + x] = spritePixel;
                    }
                }
            }
            pixelIndex++;
        }
    }
}

void PPU::renderTile16(uint16_t* buffer, int index, int xOffset, int yOffset)
{
    uint16_t tile = readByte(index) + (ppuCtrl & (1 << 4) ? 256 : 0);
    uint8_t attribute = getAttributeTableValue(index);

    for (int row = 0; row < 8; row++)
    {
        uint8_t plane1 = readCHR(tile * 16 + row);
        uint8_t plane2 = readCHR(tile * 16 + row + 8);

        for (int column = 0; column < 8; column++)
        {
            uint8_t paletteIndex = (((plane1 & (1 << column)) ? 1 : 0) + ((plane2 & (1 << column)) ? 2 : 0));
            
            uint8_t colorIndex;
            if (paletteIndex == 0) {
                colorIndex = palette[0];  // Universal background color
            } else {
                colorIndex = palette[(attribute & 0x03) * 4 + paletteIndex];  // Mask attribute to 2 bits
            }
            
            uint32_t pixel32 = paletteRGB[colorIndex];
            uint16_t pixel16 = ((pixel32 & 0xF80000) >> 8) | ((pixel32 & 0x00FC00) >> 5) | ((pixel32 & 0x0000F8) >> 3);

            int x = (xOffset + (7 - column));
            int y = (yOffset + row);
            if (x >= 0 && x < 256 && y >= 0 && y < 240)
            {
                buffer[y * 256 + x] = pixel16;
            }
        }
    }
}

void PPU::writeAddressRegister(uint8_t value)
{
    if (!writeToggle)
    {
        // Upper byte
        currentAddress = (currentAddress & 0xff) | (((uint16_t)value << 8) & 0xff00);
    }
    else
    {
        // Lower byte
        currentAddress = (currentAddress & 0xff00) | (uint16_t)value;
    }
    writeToggle = !writeToggle;
}

void PPU::writeByte(uint16_t address, uint8_t value)
{
    // Mirror all addresses above $3fff
    address &= 0x3fff;

    if (address < 0x2000)
    {
        // CHR-RAM write - CRITICAL FOR UxROM!
        engine.writeCHRData(address, value);
        
        // Also invalidate tile caches when CHR data changes
        clearAllBankCaches();
    }
    else if (address < 0x3f00)
    {
        nametable[getNametableIndex(address)] = value;
    }
    else if (address < 0x3f20)
    {
        // Palette data
        palette[address - 0x3f00] = value;

        // INVALIDATE ALL CACHES when palette changes
        if (g_comprehensiveCacheInit) {
            memset(g_comprehensiveCache, 0, sizeof(g_comprehensiveCache));
            for (int i = 0; i < 512 * 8; i++) {
                g_comprehensiveCache[i].is_valid = false;
            }
            
            // Clear old flip cache
            g_flipCache.clear();
            g_flipCacheIndex.clear();
        }
        
        // Clear ALL bank caches
        clearAllBankCaches();

        // Mirroring
        if (address == 0x3f10 || address == 0x3f14 || address == 0x3f18 || address == 0x3f1c)
        {
            palette[address - 0x3f10] = value;
        }
    }
}

void PPU::writeDataRegister(uint8_t value)
{
    static bool debugPalette = true;
    
    writeByte(currentAddress, value);
    if (!(ppuCtrl & (1 << 2)))
    {
        currentAddress++;
    }
    else
    {
        currentAddress += 32;
    }
}

void PPU::writeDMA(uint8_t page)
{
    uint16_t address = (uint16_t)page << 8;
    for (int i = 0; i < 256; i++)
    {
        oam[oamAddress] = engine.readData(address);
        address++;
        oamAddress++;
    }
}

void PPU::writeRegister(uint16_t address, uint8_t value)
{
    static bool debugPPU = true; // Set to false to disable
    
    switch(address)
    {
    // PPUCTRL
    case 0x2000:
        ppuCtrl = value;
        // Cache for next frame's rendering
        cachedCtrl = value;
        break;
        
    // PPUMASK
    case 0x2001:
        ppuMask = value;
        break;
        
    // OAMADDR
    case 0x2003:
        oamAddress = value;
        break;
        
    // OAMDATA
    case 0x2004:
        oam[oamAddress] = value;
        oamAddress++;
        break;
        
    // PPUSCROLL
case 0x2005:
    if (!writeToggle)
    {
        // SMB alternates between $00 (status bar) and $C6 (game area)
        // We want to keep the non-zero value as the "real" scroll
        if (value != 0) {
            // This is likely the game area scroll value
            if (gameAreaScrollX != value) {
                gameAreaScrollX = value;
            }
        }
        ppuScrollX = value;  // Still update the register for compatibility
    }
    else
    {
        ppuScrollY = value;
    }
    writeToggle = !writeToggle;
    break;
        
    // PPUADDR
    case 0x2006:
        writeAddressRegister(value);
        break;
        
    // PPUDATA
    case 0x2007:
        writeDataRegister(value);
        break;
        
    default:
        break;
    }
}

uint32_t PPU::getFlipCacheKey(uint16_t tile, uint8_t palette_type, uint8_t attribute, uint8_t flip_flags)
{
    // No need to include bank in key since each bank has its own flip cache
    return (tile << 16) | (palette_type << 8) | (attribute << 4) | flip_flags;
}

void PPU::cacheFlipVariation(uint16_t tile, uint8_t palette_type, uint8_t attribute, bool flipX, bool flipY, uint8_t chr_bank)
{
    uint8_t flip_flags = (flipY ? 2 : 0) | (flipX ? 1 : 0);
    if (flip_flags == 0) return;
    
    ensureBankCacheExists(chr_bank);
    BankTileCache* bankCache = getBankCache(chr_bank);
    if (!bankCache || !bankCache->is_allocated) return;
    
    uint32_t key = getFlipCacheKey(tile, palette_type, attribute, flip_flags);
    
    // Check if this flip variation is already cached in this bank
    if (bankCache->flipCacheIndex.find(key) != bankCache->flipCacheIndex.end()) {
        return;
    }
    
    // Make sure we have the normal version cached
    cacheTileAllVariations(tile, palette_type, attribute, chr_bank);
    
    int normalCacheIndex = getTileCacheIndex(tile, palette_type, attribute);
    if (normalCacheIndex >= 512 * 8) return;
    
    ComprehensiveTileCache& normalCache = bankCache->tiles[normalCacheIndex];
    if (!normalCache.is_valid) return;
    
    // Create flip cache entry
    FlipCacheEntry flipEntry;
    flipEntry.tile_id = tile;
    flipEntry.palette_type = palette_type;
    flipEntry.attribute = attribute;
    flipEntry.flip_flags = flip_flags;
    
    // Apply flipping
    for (int row = 0; row < 8; row++) {
        for (int column = 0; column < 8; column++) {
            int srcIndex = row * 8 + column;
            
            int dstRow = flipY ? (7 - row) : row;
            int dstColumn = flipX ? (7 - column) : column;
            int dstIndex = dstRow * 8 + dstColumn;
            
            flipEntry.pixels[dstIndex] = normalCache.pixels[srcIndex];
        }
    }
    
    // Add to this bank's flip cache
    size_t newIndex = bankCache->flipCache.size();
    bankCache->flipCache.push_back(flipEntry);
    bankCache->flipCacheIndex[key] = newIndex;
}

PPU::ScalingCache::ScalingCache() 
    : scaledBuffer(nullptr), sourceToDestX(nullptr), sourceToDestY(nullptr),
      scaleFactor(0), destWidth(0), destHeight(0), destOffsetX(0), destOffsetY(0),
      screenWidth(0), screenHeight(0), isValid(false) 
{
}

PPU::ScalingCache::~ScalingCache() 
{
    cleanup();
}

void PPU::ScalingCache::cleanup() 
{
    if (scaledBuffer) { 
        delete[] scaledBuffer; 
        scaledBuffer = nullptr; 
    }
    if (sourceToDestX) { 
        delete[] sourceToDestX; 
        sourceToDestX = nullptr; 
    }
    if (sourceToDestY) { 
        delete[] sourceToDestY; 
        sourceToDestY = nullptr; 
    }
    isValid = false;
}

void PPU::renderScaled(uint16_t* buffer, int screenWidth, int screenHeight)
{
    // Clear the screen buffer
    uint32_t bgColor32 = paletteRGB[palette[0]];
    //uint16_t bgColor16 = ((bgColor32 & 0xF80000) >> 8) | ((bgColor32 & 0x00FC00) >> 5) | ((bgColor32 & 0x0000F8) >> 3);
    uint16_t bgColor16 = 0x0000; // Pure black in RGB565

    for (int i = 0; i < screenWidth * screenHeight; i++) {
        buffer[i] = bgColor16;
    }
    
    // Update scaling cache if needed
    if (!isScalingCacheValid(screenWidth, screenHeight)) {
        updateScalingCache(screenWidth, screenHeight);
    }
    
    // Render NES frame to temporary buffer
    static uint16_t nesBuffer[256 * 240];
    render16(nesBuffer);
    
    // Apply scaling based on cache
    const int scale = g_scalingCache.scaleFactor;

    if (scale == 1) {
        renderScaled1x1(nesBuffer, buffer, screenWidth, screenHeight);
    } else if (scale == 2) {
        renderScaled2x(nesBuffer, buffer, screenWidth, screenHeight);
    } else if (scale == 3) {
        renderScaled3x(nesBuffer, buffer, screenWidth, screenHeight);
    } else {
        renderScaledGeneric(nesBuffer, buffer, screenWidth, screenHeight, scale);
    }
}

void PPU::renderScaled32(uint32_t* buffer, int screenWidth, int screenHeight)
{
    // For 32-bit rendering, first render to 16-bit then convert
    static uint16_t tempBuffer[1024 * 768]; // Large enough for most screens
    
    if (screenWidth * screenHeight <= 1024 * 768) {
        renderScaled(tempBuffer, screenWidth, screenHeight);
        convertNESToScreen32(tempBuffer, buffer, screenWidth, screenHeight);
    } else {
        // Fallback for very large screens
        render(buffer); // Use original 32-bit render
    }
}

void PPU::updateScalingCache(int screenWidth, int screenHeight)
{
    // Calculate optimal scaling
    int scale_x = screenWidth / 256;
    int scale_y = screenHeight / 240;
    int scale = (scale_x < scale_y) ? scale_x : scale_y;
    if (scale < 1) scale = 1;
    
    // Check if cache needs updating
    if (g_scalingCache.isValid && 
        g_scalingCache.scaleFactor == scale &&
        g_scalingCache.screenWidth == screenWidth &&
        g_scalingCache.screenHeight == screenHeight) {
        return; // Cache is still valid
    }
    
    // Clean up old cache
    g_scalingCache.cleanup();
    
    // Calculate new dimensions
    g_scalingCache.scaleFactor = scale;
    g_scalingCache.destWidth = 256 * scale;
    g_scalingCache.destHeight = 240 * scale;
    g_scalingCache.destOffsetX = (screenWidth - g_scalingCache.destWidth) / 2;
    g_scalingCache.destOffsetY = (screenHeight - g_scalingCache.destHeight) / 2;
    g_scalingCache.screenWidth = screenWidth;
    g_scalingCache.screenHeight = screenHeight;
    
    // Allocate coordinate mapping tables
    g_scalingCache.sourceToDestX = new int[256];
    g_scalingCache.sourceToDestY = new int[240];
    
    // Pre-calculate coordinate mappings
    for (int x = 0; x < 256; x++) {
        g_scalingCache.sourceToDestX[x] = x * scale + g_scalingCache.destOffsetX;
    }
    
    for (int y = 0; y < 240; y++) {
        g_scalingCache.sourceToDestY[y] = y * scale + g_scalingCache.destOffsetY;
    }
    
    g_scalingCache.isValid = true;
}

bool PPU::isScalingCacheValid(int screenWidth, int screenHeight)
{
    return g_scalingCache.isValid && 
           g_scalingCache.screenWidth == screenWidth &&
           g_scalingCache.screenHeight == screenHeight;
}

void PPU::renderScaled1x1(uint16_t* nesBuffer, uint16_t* screenBuffer, int screenWidth, int screenHeight)
{
    const int dest_x = g_scalingCache.destOffsetX;
    const int dest_y = g_scalingCache.destOffsetY;

    // Direct 1:1 copy
    for (int y = 0; y < 240; y++) {
        int screen_y = y + dest_y;
        if (screen_y < 0 || screen_y >= screenHeight) continue;
        
        uint16_t* src_row = &nesBuffer[y * 256];
        uint16_t* dest_row = &screenBuffer[screen_y * screenWidth + dest_x];
        
        int copy_width = 256;
        if (dest_x + copy_width > screenWidth) {
            copy_width = screenWidth - dest_x;
        }
        if (dest_x < 0) {
            src_row -= dest_x;
            dest_row -= dest_x;
            copy_width += dest_x;
        }
        
        if (copy_width > 0) {
            memcpy(dest_row, src_row, copy_width * sizeof(uint16_t));
        }
    }
}

void PPU::renderScaled2x(uint16_t* nesBuffer, uint16_t* screenBuffer, int screenWidth, int screenHeight)
{
    const int dest_x = g_scalingCache.destOffsetX;
    const int dest_y = g_scalingCache.destOffsetY;
    
    // Optimized 2x scaling
    for (int y = 0; y < 240; y++) {
        int dest_y1 = y * 2 + dest_y;
        int dest_y2 = dest_y1 + 1;
        
        if (dest_y2 >= screenHeight) break;
        if (dest_y1 < 0) continue;
        
        uint16_t* src_row = &nesBuffer[y * 256];
        uint16_t* dest_row1 = &screenBuffer[dest_y1 * screenWidth + dest_x];
        uint16_t* dest_row2 = &screenBuffer[dest_y2 * screenWidth + dest_x];
        
        // Process pixels in groups for better cache utilization
        for (int x = 0; x < 256; x += 4) {
            if ((x * 2 + dest_x + 8) > screenWidth) break;
            
            uint16_t p1 = src_row[x];
            uint16_t p2 = src_row[x + 1];
            uint16_t p3 = src_row[x + 2];
            uint16_t p4 = src_row[x + 3];
            
            int dest_base = x * 2;
            
            // First row
            dest_row1[dest_base]     = p1; dest_row1[dest_base + 1] = p1;
            dest_row1[dest_base + 2] = p2; dest_row1[dest_base + 3] = p2;
            dest_row1[dest_base + 4] = p3; dest_row1[dest_base + 5] = p3;
            dest_row1[dest_base + 6] = p4; dest_row1[dest_base + 7] = p4;
            
            // Second row (duplicate)
            dest_row2[dest_base]     = p1; dest_row2[dest_base + 1] = p1;
            dest_row2[dest_base + 2] = p2; dest_row2[dest_base + 3] = p2;
            dest_row2[dest_base + 4] = p3; dest_row2[dest_base + 5] = p3;
            dest_row2[dest_base + 6] = p4; dest_row2[dest_base + 7] = p4;
        }
    }
}

void PPU::renderScaled3x(uint16_t* nesBuffer, uint16_t* screenBuffer, int screenWidth, int screenHeight)
{
    const int dest_x = g_scalingCache.destOffsetX;
    const int dest_y = g_scalingCache.destOffsetY;
    
    for (int y = 0; y < 240; y++) {
        int dest_y1 = y * 3 + dest_y;
        int dest_y2 = dest_y1 + 1;
        int dest_y3 = dest_y1 + 2;
        
        if (dest_y3 >= screenHeight) break;
        if (dest_y1 < 0) continue;
        
        uint16_t* src_row = &nesBuffer[y * 256];
        uint16_t* dest_row1 = &screenBuffer[dest_y1 * screenWidth + dest_x];
        uint16_t* dest_row2 = &screenBuffer[dest_y2 * screenWidth + dest_x];
        uint16_t* dest_row3 = &screenBuffer[dest_y3 * screenWidth + dest_x];
        
        for (int x = 0; x < 256; x++) {
            if ((x * 3 + dest_x + 3) > screenWidth) break;
            
            uint16_t pixel = src_row[x];
            int dest_base = x * 3;
            
            // Triple each pixel
            dest_row1[dest_base] = dest_row1[dest_base + 1] = dest_row1[dest_base + 2] = pixel;
            dest_row2[dest_base] = dest_row2[dest_base + 1] = dest_row2[dest_base + 2] = pixel;
            dest_row3[dest_base] = dest_row3[dest_base + 1] = dest_row3[dest_base + 2] = pixel;
        }
    }
}

void PPU::renderScaledGeneric(uint16_t* nesBuffer, uint16_t* screenBuffer, int screenWidth, int screenHeight, int scale)
{
    // Generic scaling using pre-calculated coordinate tables
    for (int y = 0; y < 240; y++) {
        uint16_t* src_row = &nesBuffer[y * 256];
        int dest_y_start = g_scalingCache.sourceToDestY[y];
        
        for (int scale_y = 0; scale_y < scale; scale_y++) {
            int dest_y = dest_y_start + scale_y;
            if (dest_y < 0 || dest_y >= screenHeight) continue;
            
            uint16_t* dest_row = &screenBuffer[dest_y * screenWidth];
            
            for (int x = 0; x < 256; x++) {
                uint16_t pixel = src_row[x];
                int dest_x_start = g_scalingCache.sourceToDestX[x];
                
                for (int scale_x = 0; scale_x < scale; scale_x++) {
                    int dest_x = dest_x_start + scale_x;
                    if (dest_x >= 0 && dest_x < screenWidth) {
                        dest_row[dest_x] = pixel;
                    }
                }
            }
        }
    }
}

void PPU::convertNESToScreen32(uint16_t* nesBuffer, uint32_t* screenBuffer, int screenWidth, int screenHeight)
{
    // Convert 16-bit RGB565 to 32-bit RGBA
    for (int i = 0; i < screenWidth * screenHeight; i++) {
        uint16_t pixel16 = nesBuffer[i];
        
        // Extract RGB565 components
        int r = (pixel16 >> 11) & 0x1F;
        int g = (pixel16 >> 5) & 0x3F;
        int b = pixel16 & 0x1F;
        
        // Scale to 8-bit
        r = (r << 3) | (r >> 2);
        g = (g << 2) | (g >> 4);
        b = (b << 3) | (b >> 2);
        
        // Pack into 32-bit with alpha
        screenBuffer[i] = 0xFF000000 | (r << 16) | (g << 8) | b;
    }
}

void PPU::captureFrameScroll() {
    // Capture the current scroll value AND control register at the start of VBlank
    frameScrollX = ppuScrollX;
    frameCtrl = ppuCtrl;
    
    // ALSO CAPTURE CHR BANK STATE
    if (engine.getMapper() == 66) {
        frameCHRBank = engine.getCurrentCHRBank();
    }
}    

void PPU::invalidateTileCache()
{
    clearAllBankCaches();
}

BankTileCache* PPU::getBankCache(uint8_t bank)
{
    if (!g_bankCacheInit) {
        g_bankCacheInit = true;
    }
    
    auto it = g_bankCaches.find(bank);
    if (it != g_bankCaches.end()) {
        return it->second;
    }
    
    return nullptr;
}

void PPU::ensureBankCacheExists(uint8_t bank)
{
    if (g_bankCaches.find(bank) == g_bankCaches.end()) {
        BankTileCache* cache = new BankTileCache();
        cache->allocate(bank);
        g_bankCaches[bank] = cache;
    }
}

void PPU::releaseBankCache(uint8_t bank)
{
    auto it = g_bankCaches.find(bank);
    if (it != g_bankCaches.end()) {
        delete it->second;
        g_bankCaches.erase(it);
    }
}

void PPU::clearAllBankCaches()
{
    for (auto& pair : g_bankCaches) {
        delete pair.second;
    }
    g_bankCaches.clear();
}

void PPU::releaseBankCaches(const std::vector<uint8_t>& banksToKeep)
{
    std::unordered_set<uint8_t> keepSet(banksToKeep.begin(), banksToKeep.end());
    
    auto it = g_bankCaches.begin();
    while (it != g_bankCaches.end()) {
        if (keepSet.find(it->first) == keepSet.end()) {
            delete it->second;
            it = g_bankCaches.erase(it);
        } else {
            ++it;
        }
    }
}

size_t PPU::getBankCacheMemoryUsage()
{
    size_t total = 0;
    for (const auto& pair : g_bankCaches) {
        total += sizeof(ComprehensiveTileCache) * 512 * 8;  // Main cache
        total += pair.second->flipCache.size() * sizeof(FlipCacheEntry);  // Flip cache
        total += pair.second->flipCacheIndex.size() * (sizeof(uint32_t) + sizeof(size_t));  // Index map
    }
    return total;
}

void PPU::optimizeCacheMemory()
{
    // Keep only the current and recently used banks
    std::vector<uint8_t> banksToKeep;
    banksToKeep.push_back(frameCHRBank);  // Current bank
    // Add any other banks you want to keep cached
    
    releaseBankCaches(banksToKeep);
}

uint8_t PPU::readCHRFromBank(int index, uint8_t chr_bank)
{
    if (index < 0x2000)
    {
        // If mapper supports banking, get data from specific bank
        if (engine.getMapper() == 66) {
            return engine.readCHRDataFromBank(index, chr_bank);
        } else {
            // Fallback to normal CHR read
            return engine.readCHRData(index);
        }
    }
    return 0;
}

