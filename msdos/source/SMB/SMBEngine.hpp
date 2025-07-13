#ifndef SMBENGINE_HPP
#define SMBENGINE_HPP

#include <cstdint>
#include <cstddef>
#include <fstream>
#include <iostream>

#include "../Emulation/MemoryAccess.hpp"
#include "SMBDataPointers.hpp"


// Save state structure for binary file format
struct SaveState {
    // Header for validation
    char header[8];
    uint32_t version;
    
    // CPU Registers
    uint8_t registerA;
    uint8_t registerX;
    uint8_t registerY;
    uint8_t registerS;
    
    // CPU Flags
    bool c;  // Carry flag
    bool z;  // Zero flag
    bool n;  // Negative flag
    
    // Global flags (declared as extern in your code)
    uint8_t i;  // Interrupt disable
    uint8_t d;  // Decimal mode
    uint8_t b;  // Break command
    uint8_t v;  // Overflow flag
    
    // Call stack state
    int returnIndexStack[100];
    int returnIndexStackTop;
    
    // 2KB RAM data
    uint8_t ram[0x800];
    
    // PPU State - matching your PPU class exactly
    uint8_t nametable[2048];        // Your nametable array
    uint8_t oam[256];               // Your sprite memory
    uint8_t palette[32];            // Your palette data
    
    // PPU Registers - matching your PPU class
    uint8_t ppuCtrl;                // $2000
    uint8_t ppuMask;                // $2001
    uint8_t ppuStatus;              // $2002
    uint8_t oamAddress;             // $2003
    uint8_t ppuScrollX;             // $2005
    uint8_t ppuScrollY;             // $2005
    
    // PPU Internal State - matching your PPU class
    uint16_t currentAddress;        // Your currentAddress
    bool writeToggle;               // Your writeToggle
    uint8_t vramBuffer;             // Your vramBuffer
    
    // Additional state for future expansion
    uint8_t reserved[64];
};

class APU;
class Controller;
class PPU;

/**
 * Engine that runs Super Mario Bros.
 * Handles emulation of various NES subsystems for compatibility and accuracy.
 */
class SMBEngine
{
    friend class MemoryAccess;
    friend class PPU;
public:
    /**
     * Construct a new SMBEngine instance.
     *
     * @param romImage the data from the Super Mario Bros. ROM image.
     */
    SMBEngine(uint8_t* romImage);

    ~SMBEngine();

    /**
     * Callback for handling audio buffering.
     */
    void audioCallback(uint8_t* stream, int length);

    /**
     * Get player 1's controller.
     */
    Controller& getController1();

    /**
     * Get player 2's controller.
     */
    Controller& getController2();

    /**
     * Render the screen to a 32-bit color buffer (legacy method).
     *
     * @param buffer a 256x240 32-bit color buffer for storing the rendering.
     */
    void render(uint32_t* buffer);

    /**
     * Render the screen to a 16-bit color buffer (optimized).
     *
     * @param buffer a 256x240 16-bit color buffer for storing the rendering.
     */
    void render16(uint16_t* buffer);

    /**
     * Render directly to any sized screen buffer with scaling and centering.
     *
     * @param buffer target screen buffer (16-bit color)
     * @param screenWidth width of the target screen buffer
     * @param screenHeight height of the target screen buffer  
     * @param scale desired scaling factor (0 = auto-fit to screen)
     */
    void renderDirect(uint16_t* buffer, int screenWidth, int screenHeight, int scale = 0);

    /**
     * Fast render directly to screen buffer (no scaling, centered).
     * Optimized for cases where screen is at least 256x240.
     *
     * @param buffer target screen buffer (16-bit color)
     * @param screenWidth width of the target screen buffer (must be >= 256)
     * @param screenHeight height of the target screen buffer (must be >= 240)
     */
    void renderDirectFast(uint16_t* buffer, int screenWidth, int screenHeight);

    /**
     * Reset the game engine to power-on state.
     */
    void reset();

    /**
     * Update the game engine by one frame.
     */
    void update();

    /**
     * Toggle between APU and MIDI audio modes.
     */
    void toggleAudioMode();
    
    /**
     * Check if currently using MIDI audio.
     */
    bool isUsingMIDIAudio() const;
    
    /**
     * Print debug information about audio channels.
     */
    void debugAudioChannels();

    void saveState(const std::string& filename);
    bool loadState(const std::string& filename);
    void renderScaled16(uint16_t* buffer, int screenWidth, int screenHeight);  
    void renderScaled32(uint32_t* buffer, int screenWidth, int screenHeight);


private:
    // NES Emulation subsystems:
    APU* apu;
    PPU* ppu;
    Controller* controller1;
    Controller* controller2;

    // Fields for NES CPU emulation:
    bool c;                      /**< Carry flag. */
    bool z;                      /**< Zero flag. */
    bool n;                      /**< Negative flag. */
    uint8_t registerA;           /**< Accumulator register. */
    uint8_t registerX;           /**< X index register. */
    uint8_t registerY;           /**< Y index register. */
    uint8_t registerS;           /**< Stack index register. */
    MemoryAccess a;              /**< Wrapper for A register. */
    MemoryAccess x;              /**< Wrapper for X register. */
    MemoryAccess y;              /**< Wrapper for Y register. */
    MemoryAccess s;              /**< Wrapper for S register. */
    uint8_t dataStorage[0x8000]; /**< 32kb of storage for constant data. */
    uint8_t ram[0x800];          /**< 2kb of RAM. */
    uint8_t* chr;                /**< Pointer to CHR data from the ROM. */
    int returnIndexStack[100];   /**< Stack for managing JSR subroutines. */
    int returnIndexStackTop;     /**< Current index of the top of the call stack. */

    // Pointers to constant data used in the decompiled code
    //
    SMBDataPointers dataPointers;

    /**
     * Run the decompiled code for the game.
     * 
     * See SMB.cpp for implementation.
     *
     * @param mode the mode to run. 0 runs initialization routines, 1 runs the logic for frames.
     */
    void code(int mode);

    /**
     * Logic for CMP, CPY, and CPY instructions.
     */
    void compare(uint8_t value1, uint8_t value2);

    /**
     * BIT instruction.
     */
    void bit(uint8_t value);

    /**
     * Get CHR data from the ROM.
     */
    uint8_t* getCHR();

    /**
     * Get a pointer to a byte in the address space.
     */
    uint8_t* getDataPointer(uint16_t address);

    /**
     * Get a memory access object for a particular address.
     */
    MemoryAccess getMemory(uint16_t address);

    /**
     * Get a word of memory from a zero-page address and the next byte (wrapped around),
     * in little-endian format.
     */
    uint16_t getMemoryWord(uint8_t address);

    /**
     * Load all constant data that was present in the SMB ROM.
     * 
     * See SMBData.cpp for implementation.
     */
    void loadConstantData();

    void php();
    
    void plp();

    /**
     * PHA instruction.
     */
    void pha();

    /**
     * PLA instruction.
     */
    void pla();

    /**
     * Pop an index from the call stack.
     */
    int popReturnIndex();

    /**
     * Push an index to the call stack.
     */
    void pushReturnIndex(int index);

    /**
     * Read data from an address in the NES address space.
     */
    uint8_t readData(uint16_t address);

    /**
     * Set the zero and negative flags based on a result value.
     */
    void setZN(uint8_t value);

    /**
     * Write data to an address in the NES address space.
     */
    void writeData(uint16_t address, uint8_t value);

    /**
     * Map constant data to the address space. The address must be at least 0x8000.
     */
    void writeData(uint16_t address, const uint8_t* data, std::size_t length);

};

#endif // SMBENGINE_HPP
