#ifndef ZAPPER_HPP
#define ZAPPER_HPP

#include <cstdint>

/**
 * NES Zapper (Light Gun) Controller Emulation
 * 
 * The NES Zapper works by detecting light from CRT scanlines.
 * We simulate this by checking if the mouse cursor is over
 * a bright pixel when the trigger is pulled.
 */
class Zapper {
public:
    Zapper();
    ~Zapper();
    
    // Mouse input methods
    void setMousePosition(int x, int y);
    void setTriggerPressed(bool pressed);
    void setLightDetected(bool detected);
    
    // Get current state
    int getMouseX() const { return mouseX; }
    int getMouseY() const { return mouseY; }
    bool isTriggerPressed() const { return triggerPressed; }
    bool isLightDetected() const { return lightDetected; }
    
    // NES controller interface (reads $4017)
    uint8_t readByte();
    void writeByte(uint8_t value);
    
    // Light detection logic
    bool detectLight(uint16_t* frameBuffer, int screenWidth, int screenHeight, int mouseX, int mouseY);
    bool detectLightScaled(uint16_t* frameBuffer, int screenWidth, int screenHeight, int screenX, int screenY, int scale);
    
    // Visual feedback
    void drawCrosshair(uint16_t* buffer, int screenWidth, int screenHeight, int x, int y);
    void drawCrosshair32(uint32_t* buffer, int screenWidth, int screenHeight, int x, int y);
    void drawCrosshairScaled(uint16_t* buffer, int screenWidth, int screenHeight, int screenX, int screenY, int scale);
private:
    int mouseX, mouseY;
    bool triggerPressed;
    bool lightDetected;
    
    // Light detection parameters
    static const int LIGHT_THRESHOLD = 0x8000;  // Brightness threshold for light detection
    static const int DETECTION_RADIUS = 3;     // Pixel radius for light detection
    
    // Crosshair colors
    static const uint16_t CROSSHAIR_COLOR_16 = 0xF800;  // Red in RGB565
    static const uint32_t CROSSHAIR_COLOR_32 = 0xFFFF0000;  // Red in RGB888
};

#endif // ZAPPER_HPP
