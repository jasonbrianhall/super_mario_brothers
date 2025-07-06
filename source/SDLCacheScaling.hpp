#ifndef SDL_CACHE_SCALING_HPP
#define SDL_CACHE_SCALING_HPP

#include <SDL2/SDL.h>
#include <cstdint>

class SDLScalingCache {
public:
    struct ScaleInfo {
        uint32_t* scaledBuffer;           // Pre-scaled buffer
        int* sourceToDestX;               // X coordinate mapping table
        int* sourceToDestY;               // Y coordinate mapping table
        int scaleFactor;                  // Current scale factor
        int destWidth, destHeight;        // Destination dimensions
        int destOffsetX, destOffsetY;     // Centering offsets
        bool isValid;                     // Cache validity flag
        
        ScaleInfo();
        ~ScaleInfo();
        void cleanup();
    };

private:
    ScaleInfo scaleInfo;
    SDL_Renderer* renderer;
    SDL_Texture* optimizedTexture;
    bool useOptimizedScaling;
    int currentWindowWidth, currentWindowHeight;
    
    // Optimized rendering methods
    void renderGame1x1(uint32_t* frameBuffer);
    void renderGame2x(uint32_t* frameBuffer);
    void renderGame3x(uint32_t* frameBuffer);
    void renderGameGenericScale(uint32_t* frameBuffer, int scale);
    
    // Cache management
    void updateScalingCache(int window_width, int window_height);
    bool isScalingCacheValid(int window_width, int window_height);

public:
    SDLScalingCache(SDL_Renderer* sdl_renderer);
    ~SDLScalingCache();
    
    void initialize();
    void renderOptimized(uint32_t* frameBuffer, int window_width, int window_height);
    void setOptimizedScaling(bool enabled) { useOptimizedScaling = enabled; }
    bool isOptimizedScaling() const { return useOptimizedScaling; }
};

#endif // SDL_CACHE_SCALING_HPP
