#ifndef VIDEO_FILTERS_HPP
#define VIDEO_FILTERS_HPP

#include <cstdint>
#include <allegro.h>

/**
 * Apply HQDN3D (High Quality De-Noise 3D) filter to a buffer
 * This is a simplified version of the algorithm used in FFmpeg/MPlayer
 * 
 * @param outBuffer The destination buffer
 * @param inBuffer The source buffer
 * @param prevBuffer The previous frame buffer
 * @param width Width of the buffer
 * @param height Height of the buffer
 * @param spatialStrength Strength of spatial filtering (0.0-1.0)
 * @param temporalStrength Strength of temporal filtering (0.0-1.0)
 */
void applyHQDN3D(uint32_t* outBuffer, const uint32_t* inBuffer, const uint32_t* prevBuffer, 
                 int width, int height, float spatialStrength, float temporalStrength);

/**
 * Apply Fast Approximate Anti-Aliasing (FXAA)
 * 
 * @param outBuffer The destination buffer
 * @param inBuffer The source buffer
 * @param width Width of the buffer
 * @param height Height of the buffer
 */
void applyFXAA(uint32_t* outBuffer, const uint32_t* inBuffer, int width, int height);

/**
 * Initialize the HQDN3D filter
 * 
 * @param width Width of the frame
 * @param height Height of the frame
 */
void initHQDN3D(int width, int height);

/**
 * Clean up the HQDN3D filter resources
 */
void cleanupHQDN3D();

/**
 * Initialize enhanced rendering for Allegro 4
 * This replaces the SDL MSAA functionality with Allegro-specific enhancements
 * 
 * @return True if enhanced rendering was successfully enabled, false otherwise
 */
bool initEnhancedRendering();

/**
 * Apply a simple 2x super-sampling anti-aliasing to a bitmap
 * This is a software-based alternative to hardware MSAA for Allegro 4
 * 
 * @param target The target bitmap to render to
 * @param source The source bitmap to scale down from
 * @param sourceWidth Width of the source bitmap
 * @param sourceHeight Height of the source bitmap
 */
void applySuperSampling(BITMAP* target, const uint32_t* source, int sourceWidth, int sourceHeight);

/**
 * Convert 32-bit ARGB color to Allegro RGB format
 */
int convertARGBToAllegro(uint32_t argb_color);

/**
 * Convert Allegro RGB color to 32-bit ARGB format
 */
uint32_t convertAllegroToARGB(int allegro_color);

#endif // VIDEO_FILTERS_HPP
