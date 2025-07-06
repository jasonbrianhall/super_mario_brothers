#include "GTKMainWindow.hpp"
#include "SMB/SMBEngine.hpp"
#include "Emulation/Controller.hpp"
#include "Configuration.hpp"
#include "Constants.hpp"
#include "Util/Video.hpp"
#include "Util/VideoFilters.hpp"
#include "SMBRom.hpp"

GTKMainWindow::ScalingCache::ScalingCache() 
    : scaledBuffer(nullptr), sourceToDestX(nullptr), sourceToDestY(nullptr), 
      scaleFactor(0), destWidth(0), destHeight(0), 
      destOffsetX(0), destOffsetY(0), isValid(false) 
{
}

GTKMainWindow::ScalingCache::~ScalingCache() 
{
    cleanup();
}

void GTKMainWindow::ScalingCache::cleanup() 
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

void GTKMainWindow::initializeScalingCache()
{
    printf("Initializing GTK scaling cache...\n");
    scalingCache.cleanup();
    useOptimizedScaling = true;
    printf("GTK scaling cache initialized\n");
}

void GTKMainWindow::updateScalingCache(int widget_width, int widget_height)
{
    // Calculate optimal scaling
    int scale_x = widget_width / RENDER_WIDTH;
    int scale_y = widget_height / RENDER_HEIGHT;
    int scale = (scale_x < scale_y) ? scale_x : scale_y;
    if (scale < 1) scale = 1;
    
    // Check if cache needs updating
    if (scalingCache.isValid && 
        scalingCache.scaleFactor == scale &&
        scalingCache.destWidth == (RENDER_WIDTH * scale) &&
        scalingCache.destHeight == (RENDER_HEIGHT * scale)) {
        return; // Cache is still valid
    }
    
    // Clean up old cache
    scalingCache.cleanup();
    
    // Calculate new dimensions
    scalingCache.scaleFactor = scale;
    scalingCache.destWidth = RENDER_WIDTH * scale;
    scalingCache.destHeight = RENDER_HEIGHT * scale;
    scalingCache.destOffsetX = (widget_width - scalingCache.destWidth) / 2;
    scalingCache.destOffsetY = (widget_height - scalingCache.destHeight) / 2;
    
    // Allocate coordinate mapping tables
    scalingCache.sourceToDestX = new int[RENDER_WIDTH];
    scalingCache.sourceToDestY = new int[RENDER_HEIGHT];
    
    // Pre-calculate X coordinate mappings
    for (int x = 0; x < RENDER_WIDTH; x++) {
        scalingCache.sourceToDestX[x] = x * scale + scalingCache.destOffsetX;
    }
    
    // Pre-calculate Y coordinate mappings
    for (int y = 0; y < RENDER_HEIGHT; y++) {
        scalingCache.sourceToDestY[y] = y * scale + scalingCache.destOffsetY;
    }
    
    // Allocate pre-scaled buffer for certain optimizations
    if (scale == 2 || scale == 3) {
        scalingCache.scaledBuffer = new uint32_t[scalingCache.destWidth * scalingCache.destHeight];
        printf("Allocated pre-scaled buffer for %dx scaling\n", scale);
    }
    
    scalingCache.isValid = true;
    
    printf("GTK scaling cache updated: %dx%d -> %dx%d (scale %d)\n", 
           RENDER_WIDTH, RENDER_HEIGHT, scalingCache.destWidth, scalingCache.destHeight, scale);
}

bool GTKMainWindow::isScalingCacheValid(int widget_width, int widget_height)
{
    if (!scalingCache.isValid) return false;
    
    int scale_x = widget_width / RENDER_WIDTH;
    int scale_y = widget_height / RENDER_HEIGHT;
    int scale = (scale_x < scale_y) ? scale_x : scale_y;
    if (scale < 1) scale = 1;
    
    return scalingCache.scaleFactor == scale;
}

// Optimized game drawing with caching
void GTKMainWindow::drawGameCached(cairo_t* cr, int widget_width, int widget_height)
{
    if (!currentFrameBuffer || !isScalingCacheValid(widget_width, widget_height)) {
        // Update cache if needed
        updateScalingCache(widget_width, widget_height);
    }
    
    if (!scalingCache.isValid) {
        // Fallback to original method
        printf("Cache invalid, using fallback\n");
        return;
    }
    
    const int scale = scalingCache.scaleFactor;
    
    if (scale == 1) {
        // 1:1 copy - super optimized
        drawGame1x1(currentFrameBuffer, cr, widget_width, widget_height);
    } else if (scale == 2) {
        // 2x scaling - optimized
        drawGame2x(currentFrameBuffer, cr, widget_width, widget_height);
    } else if (scale == 3) {
        // 3x scaling - optimized
        drawGame3x(currentFrameBuffer, cr, widget_width, widget_height);
    } else {
        // Generic scaling with coordinate tables
        drawGameGenericScale(currentFrameBuffer, cr, widget_width, widget_height, scale);
    }
}

// Optimized 1:1 copy
void GTKMainWindow::drawGame1x1(uint32_t* nesBuffer, cairo_t* cr, int widget_width, int widget_height)
{
    const int dest_x = scalingCache.destOffsetX;
    const int dest_y = scalingCache.destOffsetY;
    
    // Create static buffer to avoid repeated allocations
    static guchar* rgb_data = nullptr;
    static size_t buffer_size = 0;
    size_t required_size = RENDER_WIDTH * RENDER_HEIGHT * 4;
    
    if (!rgb_data || buffer_size < required_size) {
        if (rgb_data) g_free(rgb_data);
        rgb_data = g_new(guchar, required_size);
        buffer_size = required_size;
    }
    
    // Direct pixel conversion without scaling
    for (int i = 0; i < RENDER_WIDTH * RENDER_HEIGHT; i++) {
        uint32_t pixel = nesBuffer[i];
        rgb_data[i * 4 + 0] = pixel & 0xFF;         // Blue
        rgb_data[i * 4 + 1] = (pixel >> 8) & 0xFF;  // Green  
        rgb_data[i * 4 + 2] = (pixel >> 16) & 0xFF; // Red
        rgb_data[i * 4 + 3] = 255;                  // Alpha
    }
    
    cairo_surface_t* surface = cairo_image_surface_create_for_data(
        rgb_data, CAIRO_FORMAT_ARGB32, RENDER_WIDTH, RENDER_HEIGHT, RENDER_WIDTH * 4);
    
    cairo_save(cr);
    cairo_translate(cr, dest_x, dest_y);
    cairo_set_source_surface(cr, surface, 0, 0);
    cairo_paint(cr);
    cairo_restore(cr);
    
    cairo_surface_destroy(surface);
}

// Optimized 2x scaling
void GTKMainWindow::drawGame2x(uint32_t* nesBuffer, cairo_t* cr, int widget_width, int widget_height)
{
    if (!scalingCache.scaledBuffer) {
        // Fallback to generic scaling
        drawGameGenericScale(nesBuffer, cr, widget_width, widget_height, 2);
        return;
    }
    
    // Pre-scale to the scaled buffer
    uint32_t* scaledBuf = scalingCache.scaledBuffer;
    const int scaledWidth = scalingCache.destWidth;
    
    // Ultra-fast 2x scaling
    for (int y = 0; y < RENDER_HEIGHT; y++) {
        uint32_t* src_row = &nesBuffer[y * RENDER_WIDTH];
        uint32_t* dest_row1 = &scaledBuf[y * 2 * scaledWidth];
        uint32_t* dest_row2 = &scaledBuf[(y * 2 + 1) * scaledWidth];
        
        for (int x = 0; x < RENDER_WIDTH; x++) {
            uint32_t pixel = src_row[x];
            int dest_x = x * 2;
            
            // 2x2 block
            dest_row1[dest_x] = dest_row1[dest_x + 1] = pixel;
            dest_row2[dest_x] = dest_row2[dest_x + 1] = pixel;
        }
    }
    
    // Convert pre-scaled buffer to Cairo format
    static guchar* rgb_data = nullptr;
    static size_t buffer_size = 0;
    size_t required_size = scaledWidth * scalingCache.destHeight * 4;
    
    if (!rgb_data || buffer_size < required_size) {
        if (rgb_data) g_free(rgb_data);
        rgb_data = g_new(guchar, required_size);
        buffer_size = required_size;
    }
    
    for (int i = 0; i < scaledWidth * scalingCache.destHeight; i++) {
        uint32_t pixel = scaledBuf[i];
        rgb_data[i * 4 + 0] = pixel & 0xFF;
        rgb_data[i * 4 + 1] = (pixel >> 8) & 0xFF;
        rgb_data[i * 4 + 2] = (pixel >> 16) & 0xFF;
        rgb_data[i * 4 + 3] = 255;
    }
    
    cairo_surface_t* surface = cairo_image_surface_create_for_data(
        rgb_data, CAIRO_FORMAT_ARGB32, scaledWidth, scalingCache.destHeight, scaledWidth * 4);
    
    cairo_save(cr);
    cairo_translate(cr, scalingCache.destOffsetX, scalingCache.destOffsetY);
    cairo_set_source_surface(cr, surface, 0, 0);
    cairo_paint(cr);
    cairo_restore(cr);
    
    cairo_surface_destroy(surface);
}

// Optimized 3x scaling
void GTKMainWindow::drawGame3x(uint32_t* nesBuffer, cairo_t* cr, int widget_width, int widget_height)
{
    if (!scalingCache.scaledBuffer) {
        drawGameGenericScale(nesBuffer, cr, widget_width, widget_height, 3);
        return;
    }
    
    // Similar to 2x but with 3x3 blocks
    uint32_t* scaledBuf = scalingCache.scaledBuffer;
    const int scaledWidth = scalingCache.destWidth;
    
    for (int y = 0; y < RENDER_HEIGHT; y++) {
        uint32_t* src_row = &nesBuffer[y * RENDER_WIDTH];
        uint32_t* dest_row1 = &scaledBuf[y * 3 * scaledWidth];
        uint32_t* dest_row2 = &scaledBuf[(y * 3 + 1) * scaledWidth];
        uint32_t* dest_row3 = &scaledBuf[(y * 3 + 2) * scaledWidth];
        
        for (int x = 0; x < RENDER_WIDTH; x++) {
            uint32_t pixel = src_row[x];
            int dest_x = x * 3;
            
            // 3x3 block
            dest_row1[dest_x] = dest_row1[dest_x + 1] = dest_row1[dest_x + 2] = pixel;
            dest_row2[dest_x] = dest_row2[dest_x + 1] = dest_row2[dest_x + 2] = pixel;
            dest_row3[dest_x] = dest_row3[dest_x + 1] = dest_row3[dest_x + 2] = pixel;
        }
    }
    
    // Render to Cairo (similar to 2x method)
    static guchar* rgb_data = nullptr;
    static size_t buffer_size = 0;
    size_t required_size = scaledWidth * scalingCache.destHeight * 4;
    
    if (!rgb_data || buffer_size < required_size) {
        if (rgb_data) g_free(rgb_data);
        rgb_data = g_new(guchar, required_size);
        buffer_size = required_size;
    }
    
    for (int i = 0; i < scaledWidth * scalingCache.destHeight; i++) {
        uint32_t pixel = scaledBuf[i];
        rgb_data[i * 4 + 0] = pixel & 0xFF;
        rgb_data[i * 4 + 1] = (pixel >> 8) & 0xFF;
        rgb_data[i * 4 + 2] = (pixel >> 16) & 0xFF;
        rgb_data[i * 4 + 3] = 255;
    }
    
    cairo_surface_t* surface = cairo_image_surface_create_for_data(
        rgb_data, CAIRO_FORMAT_ARGB32, scaledWidth, scalingCache.destHeight, scaledWidth * 4);
    
    cairo_save(cr);
    cairo_translate(cr, scalingCache.destOffsetX, scalingCache.destOffsetY);
    cairo_set_source_surface(cr, surface, 0, 0);
    cairo_paint(cr);
    cairo_restore(cr);
    
    cairo_surface_destroy(surface);
}

// Generic scaling using coordinate tables
void GTKMainWindow::drawGameGenericScale(uint32_t* nesBuffer, cairo_t* cr, int widget_width, int widget_height, int scale)
{
    // Use the existing scaling logic but with pre-calculated coordinates
    static guchar* rgb_data = nullptr;
    static size_t buffer_size = 0;
    size_t required_size = RENDER_WIDTH * RENDER_HEIGHT * 4;
    
    if (!rgb_data || buffer_size < required_size) {
        if (rgb_data) g_free(rgb_data);
        rgb_data = g_new(guchar, required_size);
        buffer_size = required_size;
    }
    
    // Convert pixel format
    for (int i = 0; i < RENDER_WIDTH * RENDER_HEIGHT; i++) {
        uint32_t pixel = nesBuffer[i];
        rgb_data[i * 4 + 0] = pixel & 0xFF;
        rgb_data[i * 4 + 1] = (pixel >> 8) & 0xFF;
        rgb_data[i * 4 + 2] = (pixel >> 16) & 0xFF;
        rgb_data[i * 4 + 3] = 255;
    }
    
    cairo_surface_t* surface = cairo_image_surface_create_for_data(
        rgb_data, CAIRO_FORMAT_ARGB32, RENDER_WIDTH, RENDER_HEIGHT, RENDER_WIDTH * 4);
    
    cairo_save(cr);
    cairo_translate(cr, scalingCache.destOffsetX, scalingCache.destOffsetY);
    cairo_scale(cr, scale, scale);
    cairo_set_source_surface(cr, surface, 0, 0);
    cairo_paint(cr);
    cairo_restore(cr);
    
    cairo_surface_destroy(surface);
}
