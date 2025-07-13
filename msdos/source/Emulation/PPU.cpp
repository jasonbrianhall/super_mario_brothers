#include "../SMB/SMBEngine.hpp"
#include "../Util/Video.hpp"

#include "PPU.hpp"

ComprehensiveTileCache PPU::g_comprehensiveCache[512 * 8];
bool PPU::g_comprehensiveCacheInit = false;
std::vector<FlipCacheEntry> PPU::g_flipCache;
std::unordered_map<uint32_t, size_t> PPU::g_flipCacheIndex;
PPU::ScalingCache PPU::g_scalingCache;


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

PPU::PPU(SMBEngine& engine) :
    engine(engine)
{
    currentAddress = 0;
    writeToggle = false;
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
        return engine.getCHR()[index];
    }
    else
    {
        return 0;
    }
}

uint8_t PPU::readDataRegister()
{
    uint8_t value = vramBuffer;
    vramBuffer = readByte(currentAddress);

    if (!(ppuCtrl & (1 << 2)))
    {
        currentAddress++;
    }
    else
    {
        currentAddress += 32;
    }

    return value;
}

uint8_t PPU::readRegister(uint16_t address)
{
    static int cycle = 0;
    switch(address)
    {
    // PPUSTATUS
    case 0x2002:
        writeToggle = false;
        return (cycle++ % 2 == 0 ? 0xc0 : 0);
    // OAMDATA
    case 0x2004:
        return oam[oamAddress];
    // PPUDATA
    case 0x2007:
        return readDataRegister();
    default:
        break;
    }

    return 0;
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
    // Create unique index for each tile+palette+attribute combination
    // tile: 0-511, palette_type: 0-7, attribute: 0-15 (but we only use 0-3 for most)
    return (tile * 8) + (palette_type & 0x7);
}

void PPU::cacheTileAllVariations(uint16_t tile, uint8_t palette_type, uint8_t attribute)
{
    int cacheIndex = getTileCacheIndex(tile, palette_type, attribute);
    if (cacheIndex >= 512 * 8) return;
    
    ComprehensiveTileCache& cache = g_comprehensiveCache[cacheIndex];
    
    // Check if already cached
    if (cache.is_valid && cache.tile_id == tile && 
        cache.palette_type == palette_type && cache.attribute == attribute) {
        return;  // Already cached
    }
    
    // Cache ONLY normal orientation (no flipping)
    for (int row = 0; row < 8; row++) {
        uint8_t plane1 = readCHR(tile * 16 + row);
        uint8_t plane2 = readCHR(tile * 16 + row + 8);

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
            
            // Store in normal orientation (7-column for NES bit order)
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
    // Initialize cache
    if (!g_comprehensiveCacheInit) {
        memset(g_comprehensiveCache, 0, sizeof(g_comprehensiveCache));
        g_comprehensiveCacheInit = true;
        printf("Comprehensive PPU cache initialized (%d KB)\n", 
               (int)(sizeof(g_comprehensiveCache) / 1024));
    }
    
    uint16_t tile = readByte(index) + (ppuCtrl & (1 << 4) ? 256 : 0);
    uint8_t attribute = getAttributeTableValue(index);
    
    uint16_t* pixels;
    
    if (!flipX && !flipY) {
        // Use normal cache
        cacheTileAllVariations(tile, 0, attribute);
        int cacheIndex = getTileCacheIndex(tile, 0, attribute);
        if (cacheIndex >= 512 * 8) return;
        pixels = g_comprehensiveCache[cacheIndex].pixels;
    } else {
        // Use flip cache - create if needed
        cacheFlipVariation(tile, 0, attribute, flipX, flipY);
        
        uint8_t flip_flags = (flipY ? 2 : 0) | (flipX ? 1 : 0);
        uint32_t key = getFlipCacheKey(tile, 0, attribute, flip_flags);
        
        auto it = g_flipCacheIndex.find(key);
        if (it == g_flipCacheIndex.end()) return; // Should not happen
        
        pixels = g_flipCache[it->second].pixels;
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
    uint16_t* pixels;
    
    if (!flipX && !flipY) {
        // Use normal cache
        cacheTileAllVariations(tile, palette_type, 0);
        int cacheIndex = getTileCacheIndex(tile, palette_type, 0);
        if (cacheIndex >= 512 * 8) return;
        pixels = g_comprehensiveCache[cacheIndex].pixels;
    } else {
        // Use flip cache - create if needed
        cacheFlipVariation(tile, palette_type, 0, flipX, flipY);
        
        uint8_t flip_flags = (flipY ? 2 : 0) | (flipX ? 1 : 0);
        uint32_t key = getFlipCacheKey(tile, palette_type, 0, flip_flags);
        
        auto it = g_flipCacheIndex.find(key);
        if (it == g_flipCacheIndex.end()) return;
        
        pixels = g_flipCache[it->second].pixels;
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
    // Clear the buffer with the background color
    uint32_t bgColor32 = paletteRGB[palette[0]];
    uint16_t bgColor16 = ((bgColor32 & 0xF80000) >> 8) | ((bgColor32 & 0x00FC00) >> 5) | ((bgColor32 & 0x0000F8) >> 3);
    
    for (int index = 0; index < 256 * 240; index++)
    {
        buffer[index] = bgColor16;
    }

    // First, render ALL background tiles
    if (ppuMask & (1 << 3))
    {
        int scrollX = (int)ppuScrollX + ((ppuCtrl & (1 << 0)) ? 256 : 0);
        int xMin = scrollX / 8;
        int xMax = ((int)scrollX + 256) / 8;
        
        // Status bar tiles - cached
        for (int x = 0; x < 32; x++)
        {
            for (int y = 0; y < 4; y++)
            {
                renderCachedTile(buffer, 0x2000 + 32 * y + x, x * 8, y * 8, false, false);
            }
        }
        
        // Background tiles - cached
        for (int x = xMin; x <= xMax; x++)
        {
            for (int y = 4; y < 30; y++)
            {
                int index;
                if (x < 32) {
                    index = 0x2000 + 32 * y + x;
                } else if (x < 64) {
                    index = 0x2400 + 32 * y + (x - 32);
                } else {
                    index = 0x2800 + 32 * y + (x - 64);
                }
                renderCachedTile(buffer, index, (x * 8) - (int)scrollX, (y * 8), false, false);
            }
        }
    }

    // Then render ALL sprites with proper priority handling
    if (ppuMask & (1 << 4))
    {
        // Render in reverse order (highest priority first)
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

            // Render sprite with priority-aware pixel drawing
            renderCachedSpriteWithPriority(buffer, tile, sprite_palette, x, y, flipX, flipY, behindBackground);
        }
    }
}

void PPU::renderCachedSpriteWithPriority(uint16_t* buffer, uint16_t tile, uint8_t sprite_palette, int xOffset, int yOffset, bool flipX, bool flipY, bool behindBackground)
{
    // Cache the sprite tile
    uint8_t palette_type = (sprite_palette & 0x03) + 1;
    uint16_t* pixels;
    
    if (!flipX && !flipY) {
        // Use normal cache
        cacheTileAllVariations(tile, palette_type, 0);
        int cacheIndex = getTileCacheIndex(tile, palette_type, 0);
        if (cacheIndex >= 512 * 8) return;
        pixels = g_comprehensiveCache[cacheIndex].pixels;
    } else {
        // Use flip cache - create if needed
        cacheFlipVariation(tile, palette_type, 0, flipX, flipY);
        
        uint8_t flip_flags = (flipY ? 2 : 0) | (flipX ? 1 : 0);
        uint32_t key = getFlipCacheKey(tile, palette_type, 0, flip_flags);
        
        auto it = g_flipCacheIndex.find(key);
        if (it == g_flipCacheIndex.end()) return;
        
        pixels = g_flipCache[it->second].pixels;
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
        // CHR (no-op)
    }
    else if (address < 0x3f00)
    {
        nametable[getNametableIndex(address)] = value;
    }
    else if (address < 0x3f20)
    {
        // Palette data
        palette[address - 0x3f00] = value;

        // INVALIDATE THE ENTIRE CACHE when palette changes
        if (g_comprehensiveCacheInit) {
            memset(g_comprehensiveCache, 0, sizeof(g_comprehensiveCache));
            for (int i = 0; i < 512 * 8; i++) {
                g_comprehensiveCache[i].is_valid = false;
            }
            
            // ALSO CLEAR THE FLIP CACHE
            g_flipCache.clear();
            g_flipCacheIndex.clear();
        }

        // Mirroring
        if (address == 0x3f10 || address == 0x3f14 || address == 0x3f18 || address == 0x3f1c)
        {
            palette[address - 0x3f10] = value;
        }
    }
}

void PPU::writeDataRegister(uint8_t value)
{
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
    switch(address)
    {
    // PPUCTRL
    case 0x2000:
        ppuCtrl = value;
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
            ppuScrollX = value;
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
    return (tile << 16) | (palette_type << 8) | (attribute << 4) | flip_flags;
}

void PPU::cacheFlipVariation(uint16_t tile, uint8_t palette_type, uint8_t attribute, bool flipX, bool flipY)
{
    uint8_t flip_flags = (flipY ? 2 : 0) | (flipX ? 1 : 0);
    if (flip_flags == 0) return; // Normal orientation already cached
    
    uint32_t key = getFlipCacheKey(tile, palette_type, attribute, flip_flags);
    
    // Check if this flip variation is already cached
    if (g_flipCacheIndex.find(key) != g_flipCacheIndex.end()) {
        return; // Already cached
    }
    
    // First, make sure we have the normal version cached
    cacheTileAllVariations(tile, palette_type, attribute);
    
    // Get the normal cached version
    int normalCacheIndex = getTileCacheIndex(tile, palette_type, attribute);
    if (normalCacheIndex >= 512 * 8) return;
    
    ComprehensiveTileCache& normalCache = g_comprehensiveCache[normalCacheIndex];
    if (!normalCache.is_valid) return;
    
    // Create new flip cache entry
    FlipCacheEntry flipEntry;
    flipEntry.tile_id = tile;
    flipEntry.palette_type = palette_type;
    flipEntry.attribute = attribute;
    flipEntry.flip_flags = flip_flags;
    
    // Copy pixels from normal cache and apply flipping
    for (int row = 0; row < 8; row++) {
        for (int column = 0; column < 8; column++) {
            int srcIndex = row * 8 + column;
            
            int dstRow = flipY ? (7 - row) : row;
            int dstColumn = flipX ? (7 - column) : column;
            int dstIndex = dstRow * 8 + dstColumn;
            
            flipEntry.pixels[dstIndex] = normalCache.pixels[srcIndex];
        }
    }
    
    // Add to dynamic cache
    size_t newIndex = g_flipCache.size();
    g_flipCache.push_back(flipEntry);
    g_flipCacheIndex[key] = newIndex;
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

