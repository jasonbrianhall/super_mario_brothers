/**
 * @file
 * @brief defines video utility functions.
 */
#ifndef VIDEO_HPP
#define VIDEO_HPP

#include <cstdint>
#include <string>

#include <allegro.h>

/**
 * Constants for specific tiles in CHR.
 */
enum Tile
{
    TILE_BOX_NW = 324,
    TILE_BOX_N = 328,
    TILE_BOX_NE = 329,
    TILE_BOX_W = 326,
    TILE_BOX_CENTER = 294,
    TILE_BOX_E = 330,
    TILE_BOX_SW = 351,
    TILE_BOX_S = 376,
    TILE_BOX_SE = 378
};

/**
 * Fast 16-bit color conversion from 32-bit RGB
 */
inline uint16_t rgb32_to_rgb16(uint32_t rgb32);

/**
 * Draw a box using 16-bit color buffer.
 */
void drawBox(uint16_t* buffer, int xOffset, int yOffset, int width, int height, uint32_t palette = 0);

/**
 * Draw a tile from CHR memory using 16-bit color buffer.
 */
void drawCHRTile(uint16_t* buffer, int xOffset, int yOffset, int tile, uint32_t palette = 0);

/**
 * Draw multiple tiles in a horizontal strip (optimized for backgrounds).
 */
void drawCHRTileStrip(uint16_t* buffer, int xOffset, int yOffset, const int* tiles, int tileCount, uint32_t palette = 0);

/**
 * Draw a string using characters from CHR using 16-bit color buffer.
 */
void drawText(uint16_t* buffer, int xOffset, int yOffset, const std::string& text, uint32_t palette = 0);

/**
 * Fast screen clear function.
 */
void clearScreen(uint16_t* buffer, uint16_t color = 0);

/**
 * Fast horizontal line drawing.
 */
void drawHLine(uint16_t* buffer, int x, int y, int width, uint16_t color);

/**
 * Fast vertical line drawing.
 */
void drawVLine(uint16_t* buffer, int x, int y, int height, uint16_t color);

/**
 * Fast rectangle fill.
 */
void fillRect(uint16_t* buffer, int x, int y, int width, int height, uint16_t color);

/**
 * Generate scanline pattern for CRT effect (Allegro 4 version)
 * Instead of creating an SDL texture, this creates scanline data in memory
 */
void applyScanlines(BITMAP* target, int scale = 3);

/**
 * Create a scanline pattern bitmap for overlay effects
 */
BITMAP* createScanlineBitmap(int width, int height, int scale = 3);

/**
 * Load a palette from file.
 */
const uint32_t* loadPalette(const std::string& fileName);

extern const uint32_t* paletteRGB;
// Change from pointer to array declaration
extern const uint8_t smbRomData[]; // Use array declaration without size

#endif // VIDEO_HPP
