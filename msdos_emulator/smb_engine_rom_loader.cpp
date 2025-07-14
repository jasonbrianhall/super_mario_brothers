#include <stdio.h>
#include <stdlib.h>
#include <cstdint>
#include <allegro.h>
#include "source/SMB/SMBEmulator.hpp"
#include "source/Configuration.hpp"

// Load external ROM into same format as embedded ROM
uint8_t* loadNESROM(const std::string& filename, size_t& romSize) {
    FILE* file = fopen(filename.c_str(), "rb");
    if (!file) {
        printf("Failed to open ROM file: %s\n", filename.c_str());
        return nullptr;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    romSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // Allocate and read ROM
    uint8_t* rom = new uint8_t[romSize];
    if (fread(rom, 1, romSize, file) != romSize) {
        printf("Failed to read ROM file\n");
        fclose(file);
        delete[] rom;
        return nullptr;
    }
    
    fclose(file);
    
    // Validate NES header
    if (romSize < 16 || 
        rom[0] != 'N' || rom[1] != 'E' || 
        rom[2] != 'S' || rom[3] != 0x1A) {
        printf("Invalid NES ROM format\n");
        delete[] rom;
        return nullptr;
    }
    
    printf("ROM loaded: %zu bytes\n", romSize);
    printf("PRG ROM: %dKB, CHR ROM: %dKB\n", 
           rom[4] * 16, rom[5] * 8);
    printf("Mapper: %d, Mirroring: %s\n", 
           (rom[6] >> 4) | (rom[7] & 0xF0),
           (rom[6] & 1) ? "Vertical" : "Horizontal");
    
    return rom;
}

// Working emulator using the proven SMBEngine
int main(int argc, char** argv) 
{
    printf("SMBEngine with External ROM Support\n");
    
    // Initialize Allegro
    printf("Initializing Allegro...\n");
    if (allegro_init() != 0) {
        return -1;
    }
    
    if (install_keyboard() < 0) {
        return -1;
    }
    
    set_color_depth(16);
    if (set_gfx_mode(GFX_AUTODETECT_WINDOWED, 640, 480, 0, 0) != 0) {
        return -1;
    }
    
    Configuration::initialize("smbc.cfg");
    
    // Determine which ROM to use
    uint8_t* romData = nullptr;
    size_t romSize = 0;
    bool usingExternalROM = false;
    
    if (argc >= 2) {
        printf("Loading external ROM: %s\n", argv[1]);
        romData = loadNESROM(argv[1], romSize);
        if (romData) {
            usingExternalROM = true;
            printf("Using external ROM\n");
        }  
    }  
    // Create engine with chosen ROM
    printf("Creating SMBEngine...\n");
    SMBEmulator* engine = new SMBEmulator();
    engine->loadROM(argv[1]);
    printf("Resetting engine...\n");
    engine->reset();
    
    // Main game loop
    printf("Starting game...\n");
    
    static uint16_t renderBuffer[256 * 240];
    BITMAP* back_buffer = create_bitmap(640, 480);
    bool running = true;
    int frameCount = 0;
    
    printf("Controls:\n");
    printf("  Arrow keys - D-pad\n");
    printf("  Z/X - A/B buttons\n");
    printf("  Enter - Start\n");
    printf("  Space - Select\n");
    printf("  P - Pause/Resume\n");
    printf("  R - Reset\n");
    printf("  ESC - Quit\n");
    
    bool gamePaused = false;
    
    while (running) {
        frameCount++;
        
        // Input
        poll_keyboard();
        if (key[KEY_ESC]) break;
        
        // Pause toggle
        static bool pPressed = false;
        if (key[KEY_P] && !pPressed) {
            gamePaused = !gamePaused;
            printf("Game %s\n", gamePaused ? "Paused" : "Resumed");
            pPressed = true;
        }
        if (!key[KEY_P]) {
            pPressed = false;
        }
        
        // Reset
        static bool rPressed = false;
        if (key[KEY_R] && !rPressed) {
            engine->reset();
            printf("Game Reset\n");
            rPressed = true;
        }
        if (!key[KEY_R]) {
            rPressed = false;
        }
        
        if (!gamePaused) {
            // Game controls
            /*Controller& ctrl = engine->getController1();
            ctrl.setButtonState(BUTTON_UP, key[KEY_UP]);
            ctrl.setButtonState(BUTTON_DOWN, key[KEY_DOWN]);
            ctrl.setButtonState(BUTTON_LEFT, key[KEY_LEFT]);
            ctrl.setButtonState(BUTTON_RIGHT, key[KEY_RIGHT]);
            ctrl.setButtonState(BUTTON_A, key[KEY_X]);
            ctrl.setButtonState(BUTTON_B, key[KEY_Z]);
            ctrl.setButtonState(BUTTON_START, key[KEY_ENTER]);
            ctrl.setButtonState(BUTTON_SELECT, key[KEY_SPACE]);
            */
            // Update and render
            engine->update();
        }
        
        engine->render16(renderBuffer);
        
        // Draw to screen
        clear_to_color(back_buffer, makecol(0, 0, 0));
        
        /*for (int y = 0; y < 240; y++) {
            for (int x = 0; x < 256; x++) {
                uint16_t pixel = renderBuffer[y * 256 + x];
                int r = ((pixel >> 11) & 0x1F) << 3;
                int g = ((pixel >> 5) & 0x3F) << 2;
                int b = (pixel & 0x1F) << 3;
                
                // 2x scaling, centered
                int sx = 64 + x * 2;
                int sy = y * 2;
                
                int color = makecol(r, g, b);
                putpixel(back_buffer, sx, sy, color);
                putpixel(back_buffer, sx+1, sy, color);
                putpixel(back_buffer, sx, sy+1, color);
                putpixel(back_buffer, sx+1, sy+1, color);
            }
        }*/
        
        // Show pause message
        if (gamePaused) {
            textout_centre(back_buffer, font, "PAUSED - Press P to resume", 
                          320, 240, makecol(255, 255, 255));
        }
        
        blit(back_buffer, screen, 0, 0, 0, 0, 640, 480);
                
        rest(16);
    }
    
    printf("Game ended\n");
    
    destroy_bitmap(back_buffer);
    delete engine;
    
    // Clean up external ROM if allocated
    if (usingExternalROM && romData) {
        delete[] romData;
    }
    
    allegro_exit();
    
    return 0;
}
END_OF_MAIN()
