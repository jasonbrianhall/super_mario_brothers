#include <algorithm>
#include <cstring>
#include <iostream>
#include "SDLCacheScaling.hpp"
#include "Constants.hpp"

// ScaleInfo implementation
SDLScalingCache::ScaleInfo::ScaleInfo() 
    : scaledBuffer(nullptr), sourceToDestX(nullptr), sourceToDestY(nullptr), 
      scaleFactor(0), destWidth(0), destHeight(0), 
      destOffsetX(0), destOffsetY(0), isValid(false) 
{
}

SDLScalingCache::ScaleInfo::~ScaleInfo() 
{
    cleanup();
}

void SDLScalingCache::ScaleInfo::cleanup() 
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

// SDLScalingCache implementation
SDLScalingCache::SDLScalingCache(SDL_Renderer* sdl_renderer)
    : renderer(sdl_renderer), optimizedTexture(nullptr), useOptimizedScaling(true),
      currentWindowWidth(0), currentWindowHeight(0)
{
}

SDLScalingCache::~SDLScalingCache()
{
    scaleInfo.cleanup();
    if (optimizedTexture) {
        SDL_DestroyTexture(optimizedTexture);
    }
}

void SDLScalingCache::initialize()
{
    printf("Initializing SDL scaling cache...\n");
    scaleInfo.cleanup();
    useOptimizedScaling = true;
    printf("SDL scaling cache initialized\n");
}

void SDLScalingCache::updateScalingCache(int window_width, int window_height)
{
    // IMPORTANT: Calculate scale based on LOGICAL size, not window size
    // The original code uses SDL_RenderSetLogicalSize(RENDER_WIDTH, RENDER_HEIGHT)
    // So we need to respect that logical coordinate system
    
    // For cache scaling, we'll work within the logical coordinate system
    // The scale factor should be based on how much larger we want to make
    // each logical pixel, but we need to respect SDL's logical scaling
    
    int logical_width = RENDER_WIDTH;
    int logical_height = RENDER_HEIGHT;
    
    // Calculate what scale factor SDL is using internally
    float logical_scale_x = (float)window_width / logical_width;
    float logical_scale_y = (float)window_height / logical_height;
    float logical_scale = std::min(logical_scale_x, logical_scale_y);
    
    // For our cache, we'll use integer scale factors that make sense
    int cache_scale = 1;
    if (logical_scale >= 3.0f) cache_scale = 3;
    else if (logical_scale >= 2.0f) cache_scale = 2;
    else cache_scale = 1;
    
    // Check if cache needs updating
    if (scaleInfo.isValid && 
        scaleInfo.scaleFactor == cache_scale &&
        currentWindowWidth == window_width &&
        currentWindowHeight == window_height) {
        return; // Cache is still valid
    }
    
    // Clean up old cache
    scaleInfo.cleanup();
    if (optimizedTexture) {
        SDL_DestroyTexture(optimizedTexture);
        optimizedTexture = nullptr;
    }
    
    // Calculate new dimensions - KEEP LOGICAL SIZE RESPECTED
    scaleInfo.scaleFactor = cache_scale;
    scaleInfo.destWidth = logical_width;   // Keep logical width
    scaleInfo.destHeight = logical_height; // Keep logical height
    scaleInfo.destOffsetX = 0;             // No offset needed - SDL handles it
    scaleInfo.destOffsetY = 0;             // No offset needed - SDL handles it
    
    // For cache scaling, we only optimize the texture operations
    // We don't try to replace SDL's logical scaling system
    
    // Create optimized texture for scaled rendering (only for 2x/3x)
    if (cache_scale == 2 || cache_scale == 3) {
        // Create a buffer for pre-scaled pixels
        int scaled_width = RENDER_WIDTH * cache_scale;
        int scaled_height = RENDER_HEIGHT * cache_scale;
        scaleInfo.scaledBuffer = new uint16_t[scaled_width * scaled_height];
        
        // Create texture at the scaled size
        optimizedTexture = SDL_CreateTexture(renderer, 
                                            SDL_PIXELFORMAT_ARGB8888, 
                                            SDL_TEXTUREACCESS_STREAMING, 
                                            scaled_width, 
                                            scaled_height);
        
        if (optimizedTexture) {
            printf("Created optimized texture for %dx scaling (%dx%d)\n", 
                   cache_scale, scaled_width, scaled_height);
        }
    }
    
    scaleInfo.isValid = true;
    currentWindowWidth = window_width;
    currentWindowHeight = window_height;
    
    printf("SDL scaling cache updated: logical %dx%d, cache scale %dx\n", 
           logical_width, logical_height, cache_scale);
}

bool SDLScalingCache::isScalingCacheValid(int window_width, int window_height)
{
    return scaleInfo.isValid && 
           currentWindowWidth == window_width && 
           currentWindowHeight == window_height;
}

void SDLScalingCache::renderOptimized(uint16_t* frameBuffer, int window_width, int window_height)
{
    if (!frameBuffer || !useOptimizedScaling) {
        return; // Let original rendering handle it
    }
    
    if (!isScalingCacheValid(window_width, window_height)) {
        updateScalingCache(window_width, window_height);
    }
    
    if (!scaleInfo.isValid) {
        printf("Cache invalid, falling back to standard rendering\n");
        return;
    }
    
    // CRITICAL: Set logical size first - this is what the original code does
    SDL_RenderSetLogicalSize(renderer, RENDER_WIDTH, RENDER_HEIGHT);
    
    const int scale = scaleInfo.scaleFactor;
    
    if (scale == 1) {
        // 1:1 copy - just optimize the texture update
        renderGame1x1(frameBuffer);
    } else if (scale == 2) {
        // 2x scaling - use pre-scaled buffer but render at logical size
        renderGame2x(frameBuffer);
    } else if (scale == 3) {
        // 3x scaling - use pre-scaled buffer but render at logical size
        renderGame3x(frameBuffer);
    } else {
        // Generic scaling - fall back to original method
        renderGameGenericScale(frameBuffer, scale);
    }
}

// FIXED: 1:1 rendering - respects logical size
void SDLScalingCache::renderGame1x1(uint16_t* frameBuffer)
{
    // For 1x scaling, create a texture at exactly the render size
    static SDL_Texture* directTexture = nullptr;
    
    if (!directTexture) {
        directTexture = SDL_CreateTexture(renderer, 
                                        SDL_PIXELFORMAT_ARGB8888, 
                                        SDL_TEXTUREACCESS_STREAMING, 
                                        RENDER_WIDTH, 
                                        RENDER_HEIGHT);
    }
    
    if (directTexture) {
        // Update texture with frame data
        SDL_UpdateTexture(directTexture, nullptr, frameBuffer, sizeof(uint16_t) * RENDER_WIDTH);
        
        // Render to fill the logical size (SDL handles scaling to window)
        SDL_RenderCopy(renderer, directTexture, nullptr, nullptr);
    }
}

// FIXED: 2x scaling - create high-res texture but render at logical size
void SDLScalingCache::renderGame2x(uint16_t* frameBuffer)
{
    if (!scaleInfo.scaledBuffer || !optimizedTexture) {
        renderGameGenericScale(frameBuffer, 2);
        return;
    }
    
    uint16_t* scaledBuf = scaleInfo.scaledBuffer;
    const int scaledWidth = RENDER_WIDTH * 2;
    const int scaledHeight = RENDER_HEIGHT * 2;
    
    // Ultra-fast 2x scaling
    for (int y = 0; y < RENDER_HEIGHT; y++) {
        uint16_t* src_row = &frameBuffer[y * RENDER_WIDTH];
        uint16_t* dest_row1 = &scaledBuf[y * 2 * scaledWidth];
        uint16_t* dest_row2 = &scaledBuf[(y * 2 + 1) * scaledWidth];
        
        for (int x = 0; x < RENDER_WIDTH; x++) {
            uint16_t pixel = src_row[x];
            int dest_x = x * 2;
            
            // 2x2 block
            dest_row1[dest_x] = dest_row1[dest_x + 1] = pixel;
            dest_row2[dest_x] = dest_row2[dest_x + 1] = pixel;
        }
    }
    
    // Update the high-res texture
    SDL_UpdateTexture(optimizedTexture, nullptr, scaledBuf, sizeof(uint16_t) * scaledWidth);
    
    // Render the high-res texture to logical size - SDL will scale it properly
    SDL_RenderCopy(renderer, optimizedTexture, nullptr, nullptr);
}

// FIXED: 3x scaling - create high-res texture but render at logical size
void SDLScalingCache::renderGame3x(uint16_t* frameBuffer)
{
    if (!scaleInfo.scaledBuffer || !optimizedTexture) {
        renderGameGenericScale(frameBuffer, 3);
        return;
    }
    
    uint16_t* scaledBuf = scaleInfo.scaledBuffer;
    const int scaledWidth = RENDER_WIDTH * 3;
    const int scaledHeight = RENDER_HEIGHT * 3;
    
    // 3x scaling with 3x3 blocks
    for (int y = 0; y < RENDER_HEIGHT; y++) {
        uint16_t* src_row = &frameBuffer[y * RENDER_WIDTH];
        uint16_t* dest_row1 = &scaledBuf[y * 3 * scaledWidth];
        uint16_t* dest_row2 = &scaledBuf[(y * 3 + 1) * scaledWidth];
        uint16_t* dest_row3 = &scaledBuf[(y * 3 + 2) * scaledWidth];
        
        for (int x = 0; x < RENDER_WIDTH; x++) {
            uint16_t pixel = src_row[x];
            int dest_x = x * 3;
            
            // 3x3 block
            dest_row1[dest_x] = dest_row1[dest_x + 1] = dest_row1[dest_x + 2] = pixel;
            dest_row2[dest_x] = dest_row2[dest_x + 1] = dest_row2[dest_x + 2] = pixel;
            dest_row3[dest_x] = dest_row3[dest_x + 1] = dest_row3[dest_x + 2] = pixel;
        }
    }
    
    // Update the high-res texture
    SDL_UpdateTexture(optimizedTexture, nullptr, scaledBuf, sizeof(uint16_t) * scaledWidth);
    
    // Render the high-res texture to logical size - SDL will scale it properly
    SDL_RenderCopy(renderer, optimizedTexture, nullptr, nullptr);
}

// FIXED: Generic scaling - just use original method
void SDLScalingCache::renderGameGenericScale(uint16_t* frameBuffer, int scale)
{
    // For generic scaling, just create a normal texture and let SDL handle scaling
    static SDL_Texture* genericTexture = nullptr;
    
    if (!genericTexture) {
        genericTexture = SDL_CreateTexture(renderer, 
                                         SDL_PIXELFORMAT_ARGB8888, 
                                         SDL_TEXTUREACCESS_STREAMING, 
                                         RENDER_WIDTH, 
                                         RENDER_HEIGHT);
    }
    
    if (genericTexture) {
        // Update texture with original frame data
        SDL_UpdateTexture(genericTexture, nullptr, frameBuffer, sizeof(uint16_t) * RENDER_WIDTH);
        
        // Render to logical size - SDL handles the scaling
        SDL_RenderCopy(renderer, genericTexture, nullptr, nullptr);
    }
}

