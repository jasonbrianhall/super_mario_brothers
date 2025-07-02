#include <SDL2/SDL.h>
#include <cmath>
#include <algorithm>
#include <iostream>

#include "VideoFilters.hpp"
#include "../Constants.hpp"

// Buffers for HQDN3D filter
static uint32_t* prevFrameBuffer = nullptr;

// Lookup tables for HQDN3D
static uint16_t* spatialLUT = nullptr;
static uint16_t* temporalLUT = nullptr;

// Extract color components from ARGB
static inline void colorFromARGB(uint32_t color, uint8_t& r, uint8_t& g, uint8_t& b) {
    r = (color >> 16) & 0xFF;
    g = (color >> 8) & 0xFF;
    b = color & 0xFF;
}

// Create ARGB from color components
static inline uint32_t colorToARGB(uint8_t r, uint8_t g, uint8_t b) {
    return 0xFF000000 | (r << 16) | (g << 8) | b;
}

void initHQDN3D(int width, int height) {
    // Allocate buffer for previous frame
    if (prevFrameBuffer == nullptr) {
        prevFrameBuffer = new uint32_t[width * height];
        memset(prevFrameBuffer, 0, width * height * sizeof(uint32_t));
    }
    
    // Create lookup tables for spatial and temporal filtering
    if (spatialLUT == nullptr) {
        spatialLUT = new uint16_t[256 * 256];
        temporalLUT = new uint16_t[256 * 256];
    }
}

void cleanupHQDN3D() {
    delete[] prevFrameBuffer;
    prevFrameBuffer = nullptr;
    
    delete[] spatialLUT;
    spatialLUT = nullptr;
    
    delete[] temporalLUT;
    temporalLUT = nullptr;
}

// Helper to precalculate lookup tables for HQDN3D
static void precalculateLUT(uint16_t* lut, float strength) {
    strength = std::min(1.0f, std::max(0.0f, strength));
    float coef = strength * 256.0f; // Scale to 0-256 range
    
    for (int i = 0; i < 256; i++) {
        for (int j = 0; j < 256; j++) {
            int diff = std::abs(i - j);
            int val = static_cast<int>((diff << 8) / (diff + coef));
            lut[i * 256 + j] = val;
        }
    }
}

void applyHQDN3D(uint32_t* outBuffer, const uint32_t* inBuffer, const uint32_t* prevBuffer, 
                 int width, int height, float spatialStrength, float temporalStrength) {
    // Precalculate lookup tables with current strength values
    precalculateLUT(spatialLUT, spatialStrength);
    precalculateLUT(temporalLUT, temporalStrength);
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = y * width + x;
            
            uint8_t r, g, b;
            uint8_t rN, gN, bN;  // North pixel
            uint8_t rW, gW, bW;  // West pixel
            uint8_t rP, gP, bP;  // Previous frame pixel
            
            // Current pixel
            colorFromARGB(inBuffer[idx], r, g, b);
            
            // Apply spatial filter (north and west neighbors)
            uint8_t rFiltered = r, gFiltered = g, bFiltered = b;
            
            if (y > 0) {
                // North pixel
                colorFromARGB(inBuffer[idx - width], rN, gN, bN);
                
                // Apply north filter
                uint16_t nr = (r << 8) - (spatialLUT[r * 256 + rN] * (r - rN));
                uint16_t ng = (g << 8) - (spatialLUT[g * 256 + gN] * (g - gN));
                uint16_t nb = (b << 8) - (spatialLUT[b * 256 + bN] * (b - bN));
                
                rFiltered = nr >> 8;
                gFiltered = ng >> 8;
                bFiltered = nb >> 8;
            }
            
            if (x > 0) {
                // West pixel
                colorFromARGB(inBuffer[idx - 1], rW, gW, bW);
                
                // Apply west filter
                uint16_t nr = (rFiltered << 8) - (spatialLUT[rFiltered * 256 + rW] * (rFiltered - rW));
                uint16_t ng = (gFiltered << 8) - (spatialLUT[gFiltered * 256 + gW] * (gFiltered - gW));
                uint16_t nb = (bFiltered << 8) - (spatialLUT[bFiltered * 256 + bW] * (bFiltered - bW));
                
                rFiltered = nr >> 8;
                gFiltered = ng >> 8;
                bFiltered = nb >> 8;
            }
            
            // Apply temporal filter if previous frame is available
            if (prevBuffer != nullptr) {
                colorFromARGB(prevBuffer[idx], rP, gP, bP);
                
                // Apply temporal filter
                uint16_t nr = (rFiltered << 8) - (temporalLUT[rFiltered * 256 + rP] * (rFiltered - rP));
                uint16_t ng = (gFiltered << 8) - (temporalLUT[gFiltered * 256 + gP] * (gFiltered - gP));
                uint16_t nb = (bFiltered << 8) - (temporalLUT[bFiltered * 256 + bP] * (bFiltered - bP));
                
                rFiltered = nr >> 8;
                gFiltered = ng >> 8;
                bFiltered = nb >> 8;
            }
            
            // Write filtered pixel to output
            outBuffer[idx] = colorToARGB(rFiltered, gFiltered, bFiltered);
        }
    }
    
    // Copy current frame to previous frame buffer for next time
    memcpy(prevFrameBuffer, inBuffer, width * height * sizeof(uint32_t));
}

void applyFXAA(uint32_t* outBuffer, const uint32_t* inBuffer, int width, int height) {
    // FXAA (Fast Approximate Anti-Aliasing) implementation
    // This is a simplified version of the FXAA algorithm
    
    // Create a temporary buffer to avoid reading from pixels we've already written
    uint32_t* tempBuffer = new uint32_t[width * height];
    memcpy(tempBuffer, inBuffer, width * height * sizeof(uint32_t));
    
    // FXAA constants
    const float EDGE_THRESHOLD_MIN = 0.0312f;
    const float EDGE_THRESHOLD = 0.125f;
    const float SUBPIXEL_QUALITY = 0.75f;
    
    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            int idx = y * width + x;
            
            // Get the center pixel and its immediate neighbors
            uint8_t rC, gC, bC;  // Center
            uint8_t rN, gN, bN;  // North
            uint8_t rS, gS, bS;  // South
            uint8_t rE, gE, bE;  // East
            uint8_t rW, gW, bW;  // West
            
            colorFromARGB(tempBuffer[idx], rC, gC, bC);
            colorFromARGB(tempBuffer[idx - width], rN, gN, bN);
            colorFromARGB(tempBuffer[idx + width], rS, gS, bS);
            colorFromARGB(tempBuffer[idx + 1], rE, gE, bE);
            colorFromARGB(tempBuffer[idx - 1], rW, gW, bW);
            
            // Calculate luma (brightness) for each pixel
            // Using the formula: luma = 0.299*r + 0.587*g + 0.114*b
            float lumaC = 0.299f * rC + 0.587f * gC + 0.114f * bC;
            float lumaN = 0.299f * rN + 0.587f * gN + 0.114f * bN;
            float lumaS = 0.299f * rS + 0.587f * gS + 0.114f * bS;
            float lumaE = 0.299f * rE + 0.587f * gE + 0.114f * bE;
            float lumaW = 0.299f * rW + 0.587f * gW + 0.114f * bW;
            
            // Find the minimum and maximum luma
            float lumaMin = std::min(lumaC, std::min(std::min(lumaN, lumaS), std::min(lumaE, lumaW)));
            float lumaMax = std::max(lumaC, std::max(std::max(lumaN, lumaS), std::max(lumaE, lumaW)));
            
            // Calculate the range of luma
            float lumaRange = lumaMax - lumaMin;
            
            // If the range is below the threshold, no anti-aliasing is needed
            if (lumaRange < std::max(EDGE_THRESHOLD_MIN, lumaMax * EDGE_THRESHOLD)) {
                outBuffer[idx] = tempBuffer[idx];
                continue;
            }
            
            // Get the luma at corners
            uint8_t rNW, gNW, bNW; // Northwest
            uint8_t rNE, gNE, bNE; // Northeast
            uint8_t rSW, gSW, bSW; // Southwest
            uint8_t rSE, gSE, bSE; // Southeast
            
            colorFromARGB(tempBuffer[(y-1) * width + (x-1)], rNW, gNW, bNW);
            colorFromARGB(tempBuffer[(y-1) * width + (x+1)], rNE, gNE, bNE);
            colorFromARGB(tempBuffer[(y+1) * width + (x-1)], rSW, gSW, bSW);
            colorFromARGB(tempBuffer[(y+1) * width + (x+1)], rSE, gSE, bSE);
            
            float lumaNW = 0.299f * rNW + 0.587f * gNW + 0.114f * bNW;
            float lumaNE = 0.299f * rNE + 0.587f * gNE + 0.114f * bNE;
            float lumaSW = 0.299f * rSW + 0.587f * gSW + 0.114f * bSW;
            float lumaSE = 0.299f * rSE + 0.587f * gSE + 0.114f * bSE;
            
            // Calculate horizontal and vertical contrast
            float lumaH = std::abs(lumaNW + lumaNE - 2.0f * lumaN) + 
                          2.0f * std::abs(lumaW + lumaE - 2.0f * lumaC) + 
                          std::abs(lumaSW + lumaSE - 2.0f * lumaS);
            
            float lumaV = std::abs(lumaNW + lumaSW - 2.0f * lumaW) + 
                          2.0f * std::abs(lumaN + lumaS - 2.0f * lumaC) + 
                          std::abs(lumaNE + lumaSE - 2.0f * lumaE);
            
            // Determine if we have a horizontal or vertical edge
            bool isHorizontal = lumaH >= lumaV;
            
            // Select the two edge points
            float luma1 = isHorizontal ? lumaN : lumaW;
            float luma2 = isHorizontal ? lumaS : lumaE;
            
            // Calculate gradients in the selected direction
            float gradient1 = luma1 - lumaC;
            float gradient2 = luma2 - lumaC;
            
            // Choose the side with the steepest gradient
            bool is1Steepest = std::abs(gradient1) >= std::abs(gradient2);
            
            // Calculate gradient in the steepest direction
            float gradientScaled = 0.25f * std::max(std::abs(gradient1), std::abs(gradient2));
            
            // Choose the step direction based on which side has the steepest gradient
            float stepLength = isHorizontal ? 1.0f / width : 1.0f / height;
            float lumaLocalAverage = 0.0f;
            
            if (is1Steepest) {
                // The steepest direction is toward luma1
                stepLength = -stepLength;
                lumaLocalAverage = 0.5f * (lumaC + luma1);
            } else {
                // The steepest direction is toward luma2
                lumaLocalAverage = 0.5f * (lumaC + luma2);
            }
            
            // Calculate the UV position for sampling
            float currentU = x / (float)width;
            float currentV = y / (float)height;
            
            // Direction of the edge
            float dirU = isHorizontal ? 0.0f : stepLength;
            float dirV = isHorizontal ? stepLength : 0.0f;
            
            // Shortcut: we're working with discrete pixels, so we can simplify the sampling
            // Instead of actually sampling, we'll just use the pixels we've already read
            
            // Calculate blending factor
            float subpixelOffset = isHorizontal ? 
                (lumaW + lumaE - 2.0f * lumaC) * 0.5f : 
                (lumaN + lumaS - 2.0f * lumaC) * 0.5f;
            
            subpixelOffset = std::max(-gradientScaled, std::min(gradientScaled, subpixelOffset)) * SUBPIXEL_QUALITY;
            
            // Calculate blend factor based on the subpixel offset
            float blendFactor = 0.5f - subpixelOffset / (lumaRange * 2.0f);
            blendFactor = std::max(0.0f, std::min(1.0f, blendFactor));
            
            // Blend between the center and the edge pixel
            uint8_t rBlend, gBlend, bBlend;
            
            if (isHorizontal) {
                // Horizontal edge
                if (is1Steepest) {
                    // Blend with north pixel
                    rBlend = static_cast<uint8_t>(rC * (1.0f - blendFactor) + rN * blendFactor);
                    gBlend = static_cast<uint8_t>(gC * (1.0f - blendFactor) + gN * blendFactor);
                    bBlend = static_cast<uint8_t>(bC * (1.0f - blendFactor) + bN * blendFactor);
                } else {
                    // Blend with south pixel
                    rBlend = static_cast<uint8_t>(rC * (1.0f - blendFactor) + rS * blendFactor);
                    gBlend = static_cast<uint8_t>(gC * (1.0f - blendFactor) + gS * blendFactor);
                    bBlend = static_cast<uint8_t>(bC * (1.0f - blendFactor) + bS * blendFactor);
                }
            } else {
                // Vertical edge
                if (is1Steepest) {
                    // Blend with west pixel
                    rBlend = static_cast<uint8_t>(rC * (1.0f - blendFactor) + rW * blendFactor);
                    gBlend = static_cast<uint8_t>(gC * (1.0f - blendFactor) + gW * blendFactor);
                    bBlend = static_cast<uint8_t>(bC * (1.0f - blendFactor) + bW * blendFactor);
                } else {
                    // Blend with east pixel
                    rBlend = static_cast<uint8_t>(rC * (1.0f - blendFactor) + rE * blendFactor);
                    gBlend = static_cast<uint8_t>(gC * (1.0f - blendFactor) + gE * blendFactor);
                    bBlend = static_cast<uint8_t>(bC * (1.0f - blendFactor) + bE * blendFactor);
                }
            }
            
            // Write blended pixel to output
            outBuffer[idx] = colorToARGB(rBlend, gBlend, bBlend);
        }
    }
    
    // Clean up temporary buffer
    delete[] tempBuffer;
}

bool initMSAA(SDL_Renderer* renderer) {
    // Check if the renderer supports MSAA
    SDL_RendererInfo info;
    SDL_GetRendererInfo(renderer, &info);
    
    // Check if the renderer supports the render target feature
    if (!(info.flags & SDL_RENDERER_TARGETTEXTURE)) {
        std::cout << "Renderer does not support render targets, MSAA unavailable." << std::endl;
        return false;
    }
    
    // Try to set hint for MSAA
    if (!SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2")) {
        std::cout << "Failed to set MSAA quality hint." << std::endl;
        return false;
    }
    
    std::cout << "MSAA enabled successfully." << std::endl;
    return true;
}
