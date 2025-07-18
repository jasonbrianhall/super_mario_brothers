#include <iostream>
#include <cstring>

#include "../Constants.hpp"

#include "Video.hpp"
#include "../SMBRom.hpp"

struct TilePixelCache {
    uint16_t pixels[64];  // 8x8 = 64 pixels, pre-converted to RGB565
    uint32_t palette_used;
    bool is_cached;
};

static TilePixelCache g_tileCache[512];  // Cache for all possible tiles
static bool g_cacheInitialized = false;


void drawBox(uint16_t* buffer, int xOffset, int yOffset, int width, int height, uint32_t palette)
{
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            int tile;
            if (y == 0)
            {
                if (x == 0)
                {
                    tile = TILE_BOX_NW;
                }
                else if (x == width - 1)
                {
                    tile = TILE_BOX_NE;
                }
                else
                {
                    tile = TILE_BOX_N;
                }
            }
            else if (y == height - 1)
            {
                if (x == 0)
                {
                    tile = TILE_BOX_SW;
                }
                else if (x == width - 1)
                {
                    tile = TILE_BOX_SE;
                }
                else
                {
                    tile = TILE_BOX_S;
                }
            }
            else
            {
                if (x == 0)
                {
                    tile = TILE_BOX_W;
                }
                else if (x == width - 1)
                {
                    tile = TILE_BOX_E;
                }
                else
                {
                    tile = TILE_BOX_CENTER;
                }
            }
            drawCHRTile(buffer, xOffset + x * 8, yOffset + y * 8, tile, palette);
        }
    }
}

// Fast 16-bit color conversion function
inline uint16_t rgb32_to_rgb16(uint32_t rgb32)
{
    // Extract RGB components from 32-bit color
    uint8_t r = (rgb32 >> 16) & 0xFF;
    uint8_t g = (rgb32 >> 8) & 0xFF;
    uint8_t b = rgb32 & 0xFF;
    
    // Convert to 16-bit RGB565 format (5 bits red, 6 bits green, 5 bits blue)
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

void drawCHRTile(uint16_t* buffer, int xOffset, int yOffset, int tile, uint32_t palette)
{
    // Initialize cache on first use
    if (!g_cacheInitialized) {
        memset(g_tileCache, 0, sizeof(g_tileCache));
        g_cacheInitialized = true;
    }
    
    // Bounds check for the entire tile
    if (xOffset >= 256 || yOffset >= 240 || xOffset + 8 <= 0 || yOffset + 8 <= 0) {
        return;
    }
    
    // Check if tile is cached for this palette
    TilePixelCache& cache = g_tileCache[tile];
    if (!cache.is_cached || cache.palette_used != palette) {
        // Cache miss - convert tile once
        const uint8_t* plane1_base = &smbRomData[16 + 2 * 16384 + tile * 16];
        const uint8_t* plane2_base = plane1_base + 8;
        
        int pixelIndex = 0;
        for (int row = 0; row < 8; row++) {
            uint8_t plane1 = plane1_base[row];
            uint8_t plane2 = plane2_base[row];
            
            for (int column = 0; column < 8; column++) {
                uint8_t paletteIndex = (((plane1 & (1 << column)) ? 1 : 0) + 
                                       ((plane2 & (1 << column)) ? 2 : 0));
                
                if (paletteIndex == 0) {
                    cache.pixels[pixelIndex] = 0;  // Transparent marker
                } else if (palette == 0) {
                    uint8_t gray = paletteIndex * 85;
                    cache.pixels[pixelIndex] = rgb32_to_rgb16((gray << 16) | (gray << 8) | gray);
                } else {
                    uint32_t colorIndex = (palette >> (8 * (3 - paletteIndex))) & 0xff;
                    uint32_t rgb32 = paletteRGB[colorIndex];
                    cache.pixels[pixelIndex] = rgb32_to_rgb16(rgb32);
                }
                pixelIndex++;
            }
        }
        
        cache.is_cached = true;
        cache.palette_used = palette;
    }
    
    // Fast path: copy cached pixels
    int pixelIndex = 0;
    for (int row = 0; row < 8; row++) {
        int y = yOffset + row;
        if (y < 0 || y >= 240) {
            pixelIndex += 8;
            continue;
        }
        
        uint16_t* row_buffer = &buffer[y * 256];
        
        for (int column = 0; column < 8; column++) {
            int x = xOffset + (7 - column);
            if (x >= 0 && x < 256) {
                uint16_t pixel = cache.pixels[pixelIndex];
                if (pixel != 0) {  // Skip transparent
                    row_buffer[x] = pixel;
                }
            }
            pixelIndex++;
        }
    }
}

// Optimized version that can draw multiple tiles at once
void drawCHRTileStrip(uint16_t* buffer, int xOffset, int yOffset, const int* tiles, int tileCount, uint32_t palette)
{
    for (int tileIndex = 0; tileIndex < tileCount; tileIndex++)
    {
        drawCHRTile(buffer, xOffset + tileIndex * 8, yOffset, tiles[tileIndex], palette);
    }
}

void drawText(uint16_t* buffer, int xOffset, int yOffset, const std::string& text, uint32_t palette)
{
    for (size_t i = 0; i < text.length(); i++)
    {
        int tile = 256 + 36; // SPACE
        char c = text[i];
        if (c >= '0' && c <= '9')
        {
            tile = 256 + (int)(c - '0');
        }
        else if (c >= 'a' && c <= 'z')
        {
            tile = 256 + 10 + (int)(c - 'a');
        }
        else if (c >= 'A' && c <= 'Z')
        {
            tile = 256 + 10 + (int)(c - 'A');
        }
        else if (c == '-')
        {
            tile = 256 + 40;
        }
        else if (c == '!')
        {
            tile = 256 + 43;
        }
        else if (c == '*')
        {
            tile = 256 + 41;
        }
        else if (c == '$')
        {
            tile = 256 + 46;
        }
        drawCHRTile(buffer, xOffset + i * 8, yOffset, tile, palette);
    }
}

// Fast screen clear function
void clearScreen(uint16_t* buffer, uint16_t color)
{
    const size_t pixels = 256 * 240;
    
    if (color == 0) {
        // Fast memset for black
        memset(buffer, 0, pixels * sizeof(uint16_t));
        return;
    }
    
    // Optimized loop with unrolling for non-zero colors
    uint16_t* end = buffer + pixels;
    
    // Process 8 pixels at a time
    while (buffer + 8 <= end) {
        buffer[0] = color;
        buffer[1] = color;
        buffer[2] = color;
        buffer[3] = color;
        buffer[4] = color;
        buffer[5] = color;
        buffer[6] = color;
        buffer[7] = color;
        buffer += 8;
    }
    
    // Handle remaining pixels
    while (buffer < end) {
        *buffer++ = color;
    }
}

// Fast horizontal line drawing
void drawHLine(uint16_t* buffer, int x, int y, int width, uint16_t color)
{
    if (y < 0 || y >= 240) return;
    
    int startX = (x < 0) ? 0 : x;
    int endX = (x + width > 256) ? 256 : x + width;
    
    if (startX >= endX) return;
    
    uint16_t* row_buffer = &buffer[y * 256 + startX];
    int pixels = endX - startX;
    
    if (color == 0) {
        memset(row_buffer, 0, pixels * sizeof(uint16_t));
        return;
    }
    
    // Unrolled loop for better performance
    uint16_t* end = row_buffer + pixels;
    while (row_buffer + 4 <= end) {
        row_buffer[0] = color;
        row_buffer[1] = color;
        row_buffer[2] = color;
        row_buffer[3] = color;
        row_buffer += 4;
    }
    while (row_buffer < end) {
        *row_buffer++ = color;
    }
}

// Fast vertical line drawing
void drawVLine(uint16_t* buffer, int x, int y, int height, uint16_t color)
{
    if (x < 0 || x >= 256) return;
    
    int startY = (y < 0) ? 0 : y;
    int endY = (y + height > 240) ? 240 : y + height;
    
    if (startY >= endY) return;
    
    for (int row = startY; row < endY; row++)
    {
        buffer[row * 256 + x] = color;
    }
}

// Fast rectangle fill
void fillRect(uint16_t* buffer, int x, int y, int width, int height, uint16_t color)
{
    for (int row = 0; row < height; row++)
    {
        drawHLine(buffer, x, y + row, width, color);
    }
}

void applyScanlines(BITMAP* target, int scale)
{
    if (!target || scale < 2) return;
    
    // Apply scanlines by darkening every other line
    for (int y = 1; y < target->h; y += 2) {
        for (int x = 0; x < target->w; x++) {
            int color = getpixel(target, x, y);
            
            // Extract RGB components and darken them
            int r = getr(color);
            int g = getg(color);
            int b = getb(color);
            
            // Darken the scanline by reducing brightness
            r = (r * 3) / 4;  // Reduce to 75% brightness
            g = (g * 3) / 4;
            b = (b * 3) / 4;
            
            putpixel(target, x, y, makecol(r, g, b));
        }
    }
}

BITMAP* createScanlineBitmap(int width, int height, int scale)
{
    int scaledWidth = width * scale;
    int scaledHeight = height * scale;
    
    BITMAP* scanlineBitmap = create_bitmap(scaledWidth, scaledHeight);
    if (!scanlineBitmap) {
        return nullptr;
    }
    
    // Create a scanline pattern with CRT-like RGB phosphor simulation
    for (int y = 0; y < scaledHeight; y++) {
        for (int x = 0; x < scaledWidth; x++) {
            int color;
            
            // Create RGB phosphor pattern
            int pixelX = x % scale;
            int pixelY = y % scale;
            
            if (pixelY == 1) {
                // Scanline - make it darker
                color = makecol(32, 32, 32);
            } else {
                // Regular line with RGB phosphor simulation
                switch (pixelX % 3) {
                    case 0:
                        color = makecol(255, 180, 150);  // Reddish phosphor
                        break;
                    case 1:
                        color = makecol(150, 255, 180);  // Greenish phosphor
                        break;
                    case 2:
                        color = makecol(150, 180, 255);  // Bluish phosphor
                        break;
                    default:
                        color = makecol(200, 200, 200);  // Default
                        break;
                }
            }
            
            putpixel(scanlineBitmap, x, y, color);
        }
    }
    
    return scanlineBitmap;
}

const uint32_t* loadPalette(const std::string& fileName)
{
    uint32_t* palette = nullptr;

    FILE* file = fopen(fileName.c_str(), "r");

    if (file != nullptr)
    {
        // Find the size of the file
        //
        fseek(file, 0L, SEEK_END);
        size_t fileSize = ftell(file);
        fseek(file, 0L, SEEK_SET);

        // Read the entire file into a buffer
        //
        uint8_t* fileBuffer = new uint8_t[fileSize];
        fread(fileBuffer, sizeof(uint8_t), fileSize, file);

        // Size determines the type of palette file
        //
        if (fileSize == 192 ||
            fileSize == 1536)
        {
            palette = new uint32_t[64];
            
            for (int entry = 0; entry < 64; entry++)
            {
                palette[entry] = 
                    (static_cast<uint32_t>(fileBuffer[entry * 3]) << 16) |
                    (static_cast<uint32_t>(fileBuffer[entry * 3 + 1]) << 8) |
                    (static_cast<uint32_t>(fileBuffer[entry * 3 + 2]));
            }
        }
        else
        {
            delete [] fileBuffer;
            std::cout << "Unsupported palette file \"" << fileName << "\"" << std::endl;
        }

        fclose(file);
    }
    else
    {
        std::cout << "Unable to open palette file \"" << fileName << "\"" << std::endl;
    }

    return palette;
}

