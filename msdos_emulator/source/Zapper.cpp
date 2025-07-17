#include "Zapper.hpp"
#include <algorithm>
#include <cmath>

Zapper::Zapper() 
    : mouseX(0), mouseY(0), triggerPressed(false), lightDetected(false)
{
}

Zapper::~Zapper() 
{
}

void Zapper::setMousePosition(int x, int y) 
{
    mouseX = x;
    mouseY = y;
}

void Zapper::setTriggerPressed(bool pressed) 
{
    triggerPressed = pressed;
}

void Zapper::setLightDetected(bool detected) 
{
    lightDetected = detected;
}

uint8_t Zapper::readByte() 
{
    // NES Zapper register format (read from $4017):
    // Bit 4: Light sense (0 = light detected, 1 = no light)
    // Bit 3: Trigger (0 = pressed, 1 = not pressed)
    
    uint8_t result = 0x00;
    
    // Trigger state (inverted - 0 when pressed)
    if (!triggerPressed) {
        result |= 0x08;  // Bit 3
    }
    
    // Light detection (inverted - 0 when light detected)
    if (!lightDetected) {
        result |= 0x10;  // Bit 4
    }
    
    return result;
}

void Zapper::writeByte(uint8_t value) 
{
    // Zapper doesn't respond to writes, but we can use this
    // for debugging or future enhancements
}

bool Zapper::detectLight(uint16_t* frameBuffer, int screenWidth, int screenHeight, int mouseX, int mouseY) 
{
    if (!frameBuffer || mouseX < 0 || mouseY < 0 || mouseX >= screenWidth || mouseY >= screenHeight) {
        return false;
    }
    
    // Check pixels in a small radius around the mouse cursor
    for (int dy = -DETECTION_RADIUS; dy <= DETECTION_RADIUS; dy++) {
        for (int dx = -DETECTION_RADIUS; dx <= DETECTION_RADIUS; dx++) {
            int checkX = mouseX + dx;
            int checkY = mouseY + dy;
            
            // Bounds check
            if (checkX >= 0 && checkX < screenWidth && checkY >= 0 && checkY < screenHeight) {
                uint16_t pixel = frameBuffer[checkY * screenWidth + checkX];
                
                // Convert RGB565 to brightness
                int r = (pixel >> 11) & 0x1F;
                int g = (pixel >> 5) & 0x3F;
                int b = pixel & 0x1F;
                
                // Scale to 8-bit and calculate brightness
                r = (r << 3) | (r >> 2);
                g = (g << 2) | (g >> 4);
                b = (b << 3) | (b >> 2);
                
                // Simple brightness calculation (weighted average)
                int brightness = (r * 299 + g * 587 + b * 114) / 1000;
                
                // Check if this pixel is bright enough
                if (brightness > (LIGHT_THRESHOLD >> 8)) {
                    return true;
                }
            }
        }
    }
    
    return false;
}

void Zapper::drawCrosshair(uint16_t* buffer, int screenWidth, int screenHeight, int x, int y) 
{
    if (!buffer || x < 0 || y < 0 || x >= screenWidth || y >= screenHeight) {
        return;
    }
    
    const int crosshairSize = 8;
    const int crosshairThickness = 1;
    
    // Draw horizontal line
    for (int dx = -crosshairSize; dx <= crosshairSize; dx++) {
        int drawX = x + dx;
        if (drawX >= 0 && drawX < screenWidth) {
            for (int dy = -crosshairThickness; dy <= crosshairThickness; dy++) {
                int drawY = y + dy;
                if (drawY >= 0 && drawY < screenHeight) {
                    buffer[drawY * screenWidth + drawX] = CROSSHAIR_COLOR_16;
                }
            }
        }
    }
    
    // Draw vertical line
    for (int dy = -crosshairSize; dy <= crosshairSize; dy++) {
        int drawY = y + dy;
        if (drawY >= 0 && drawY < screenHeight) {
            for (int dx = -crosshairThickness; dx <= crosshairThickness; dx++) {
                int drawX = x + dx;
                if (drawX >= 0 && drawX < screenWidth) {
                    buffer[drawY * screenWidth + drawX] = CROSSHAIR_COLOR_16;
                }
            }
        }
    }
    
    // Draw center dot
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int drawX = x + dx;
            int drawY = y + dy;
            if (drawX >= 0 && drawX < screenWidth && drawY >= 0 && drawY < screenHeight) {
                buffer[drawY * screenWidth + drawX] = CROSSHAIR_COLOR_16;
            }
        }
    }
}

void Zapper::drawCrosshair32(uint32_t* buffer, int screenWidth, int screenHeight, int x, int y) 
{
    if (!buffer || x < 0 || y < 0 || x >= screenWidth || y >= screenHeight) {
        return;
    }
    
    const int crosshairSize = 8;
    const int crosshairThickness = 1;
    
    // Draw horizontal line
    for (int dx = -crosshairSize; dx <= crosshairSize; dx++) {
        int drawX = x + dx;
        if (drawX >= 0 && drawX < screenWidth) {
            for (int dy = -crosshairThickness; dy <= crosshairThickness; dy++) {
                int drawY = y + dy;
                if (drawY >= 0 && drawY < screenHeight) {
                    buffer[drawY * screenWidth + drawX] = CROSSHAIR_COLOR_32;
                }
            }
        }
    }
    
    // Draw vertical line
    for (int dy = -crosshairSize; dy <= crosshairSize; dy++) {
        int drawY = y + dy;
        if (drawY >= 0 && drawY < screenHeight) {
            for (int dx = -crosshairThickness; dx <= crosshairThickness; dx++) {
                int drawX = x + dx;
                if (drawX >= 0 && drawX < screenWidth) {
                    buffer[drawY * screenWidth + drawX] = CROSSHAIR_COLOR_32;
                }
            }
        }
    }
    
    // Draw center dot
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int drawX = x + dx;
            int drawY = y + dy;
            if (drawX >= 0 && drawX < screenWidth && drawY >= 0 && drawY < screenHeight) {
                buffer[drawY * screenWidth + drawX] = CROSSHAIR_COLOR_32;
            }
        }
    }
}
