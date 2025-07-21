#include "Zapper.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

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
        printf("Crosshair draw failed: x=%d, y=%d, screen=%dx%d, buffer=%p\n", 
               x, y, screenWidth, screenHeight, buffer);
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

bool Zapper::detectLightScaled(uint16_t* frameBuffer, int screenWidth, int screenHeight, 
                              int screenX, int screenY, int scale) 
{
    if (!frameBuffer) {
        return false;
    }
    
    if (screenX < 0 || screenY < 0 || screenX >= screenWidth || screenY >= screenHeight) {
        return false;
    }
    
    // Debug: Always detect light when trigger is pressed for testing
    if (triggerPressed) {
        return true;  // Force detection for testing
    }
    
    // If not pressed, do normal detection but with debug info
    int radius = std::max(12, 6 * scale);
    int brightPixelCount = 0;
    int totalPixels = 0;
    int maxBrightness = 0;
    int whitePixelCount = 0;
    
    // Sample just the center pixel for quick debug
    uint16_t centerPixel = frameBuffer[screenY * screenWidth + screenX];
    int r = ((centerPixel >> 11) & 0x1F) << 3;
    int g = ((centerPixel >> 5) & 0x3F) << 2;
    int b = (centerPixel & 0x1F) << 3;
    int centerBrightness = (r * 299 + g * 587 + b * 114) / 1000;
        
    // Sample area around cursor
    for (int dy = -radius; dy <= radius; dy += 4) {  // Sample every 4th pixel for speed
        for (int dx = -radius; dx <= radius; dx += 4) {
            int checkX = screenX + dx;
            int checkY = screenY + dy;
            
            if (checkX >= 0 && checkX < screenWidth && checkY >= 0 && checkY < screenHeight) {
                uint16_t pixel = frameBuffer[checkY * screenWidth + checkX];
                totalPixels++;
                
                int pr = ((pixel >> 11) & 0x1F) << 3;
                int pg = ((pixel >> 5) & 0x3F) << 2;
                int pb = (pixel & 0x1F) << 3;
                int brightness = (pr * 299 + pg * 587 + pb * 114) / 1000;
                
                maxBrightness = std::max(maxBrightness, brightness);
                
                if (brightness > 40) {  // Very low threshold
                    brightPixelCount++;
                }
                
                if (pr > 100 || pg > 100 || pb > 100) {
                    whitePixelCount++;
                }
            }
        }
    }
    
    bool detected = (brightPixelCount >= 1) || (maxBrightness > 60) || (whitePixelCount >= 1);
    
    
    return detected;
}

/*bool Zapper::detectLightScaled(uint16_t* frameBuffer, int screenWidth, int screenHeight, 
                              int screenX, int screenY, int scale) 
{
    if (!frameBuffer || screenX < 0 || screenY < 0 || screenX >= screenWidth || screenY >= screenHeight) {
        printf("SCALED DETECT: Invalid parameters\n");
        return false;
    }
    
    // Larger detection radius for scaled display
    int radius = DETECTION_RADIUS * scale;
    if (radius < 8) radius = 8;  // Minimum radius
    
    int brightPixelCount = 0;
    int totalPixels = 0;
    
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            int checkX = screenX + dx;
            int checkY = screenY + dy;
            
            if (checkX >= 0 && checkX < screenWidth && checkY >= 0 && checkY < screenHeight) {
                uint16_t pixel = frameBuffer[checkY * screenWidth + checkX];
                totalPixels++;
                
                // Convert RGB565 to brightness
                int r = (pixel >> 11) & 0x1F;
                int g = (pixel >> 5) & 0x3F;
                int b = pixel & 0x1F;
                
                // Scale to 8-bit
                r = (r << 3) | (r >> 2);
                g = (g << 2) | (g >> 4);
                b = (b << 3) | (b >> 2);
                
                // Calculate brightness
                int brightness = (r * 299 + g * 587 + b * 114) / 1000;
                
                // More sensitive detection for Duck Hunt
                if (brightness > 100 || r > 180 || g > 180 || b > 180) {
                    brightPixelCount++;
                }
            }
        }
    }
    
    bool detected = (brightPixelCount >= 5);
    
    return detected;
}*/

void Zapper::drawCrosshairScaled(uint16_t* buffer, int screenWidth, int screenHeight, 
                                int screenX, int screenY, int scale) 
{
    if (!buffer) {
        printf("SCALED CROSSHAIR: Buffer is NULL!\n");
        return;
    }
        
    // Bounds check
    if (screenX < 0 || screenX >= screenWidth || screenY < 0 || screenY >= screenHeight) {
        printf("SCALED CROSSHAIR: Position out of bounds!\n");
        return;
    }
    
    // Scale crosshair size based on display scale
    int crosshairSize = 8 * scale;
    if (crosshairSize < 8) crosshairSize = 8;
    if (crosshairSize > 32) crosshairSize = 32;
    
    int thickness = scale;
    if (thickness < 1) thickness = 1;
    if (thickness > 3) thickness = 3;
    
    // Choose colors based on state
    uint16_t crosshairColor = triggerPressed ? 0xFFE0 : 0xF800;  // Yellow when firing, red when idle
    uint16_t centerColor = lightDetected ? 0x07E0 : 0xFFFF;      // Green when light detected, white otherwise
    
    // Draw horizontal line
    for (int dx = -crosshairSize; dx <= crosshairSize; dx++) {
        int drawX = screenX + dx;
        if (drawX >= 0 && drawX < screenWidth) {
            for (int dy = -thickness; dy <= thickness; dy++) {
                int drawY = screenY + dy;
                if (drawY >= 0 && drawY < screenHeight) {
                    buffer[drawY * screenWidth + drawX] = crosshairColor;
                }
            }
        }
    }
    
    // Draw vertical line
    for (int dy = -crosshairSize; dy <= crosshairSize; dy++) {
        int drawY = screenY + dy;
        if (drawY >= 0 && drawY < screenHeight) {
            for (int dx = -thickness; dx <= thickness; dx++) {
                int drawX = screenX + dx;
                if (drawX >= 0 && drawX < screenWidth) {
                    buffer[drawY * screenWidth + drawX] = crosshairColor;
                }
            }
        }
    }
    
    // Draw center circle
    int centerRadius = thickness + 1;
    for (int dy = -centerRadius; dy <= centerRadius; dy++) {
        for (int dx = -centerRadius; dx <= centerRadius; dx++) {
            if (dx*dx + dy*dy <= centerRadius*centerRadius) {
                int drawX = screenX + dx;
                int drawY = screenY + dy;
                if (drawX >= 0 && drawX < screenWidth && drawY >= 0 && drawY < screenHeight) {
                    buffer[drawY * screenWidth + drawX] = centerColor;
                }
            }
        }
    }
    
}
