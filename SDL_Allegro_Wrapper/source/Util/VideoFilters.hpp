#ifndef VIDEO_FILTERS_HPP
#define VIDEO_FILTERS_HPP

#include <cstdint>

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
 * Initialize the MSAA (Multi-Sample Anti-Aliasing) renderer
 * 
 * @param renderer The SDL renderer to use
 * @return True if MSAA was successfully enabled, false otherwise
 */
bool initMSAA(SDL_Renderer* renderer);

#endif // VIDEO_FILTERS_HPP
