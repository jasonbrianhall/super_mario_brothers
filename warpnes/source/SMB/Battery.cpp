#include "SMBEmulator.hpp"
#include "../Configuration.hpp"
#include "../Emulation/APU.hpp"
#include "../Emulation/Controller.hpp"
#include "../Emulation/PPU.hpp"
#include "../Zapper.hpp"
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

void SMBEmulator::initializeSRAM() {
  cleanupSRAM();  // Clean up any existing SRAM
  
  if (!nesHeader.battery) {
    return;  // No battery save support
  }
  
  // Allocate 8KB SRAM (standard for most games)
  sramSize = 8192;
  sram = new uint8_t[sramSize];
  memset(sram, 0, sramSize);  // Initialize to zero
  sramEnabled = true;
  sramDirty = false;
  
  printf("SRAM: Initialized %d bytes for battery save\n", sramSize);
  
  // Load existing save if it exists
  loadSRAM();
}

void SMBEmulator::loadSRAM() {
  if (!sram || !nesHeader.battery || romBaseName.empty()) {
    return;
  }
  
  // Create battery save filename with .srm extension
  std::string saveFilename = romBaseName + ".srm";
  
  std::ifstream file(saveFilename, std::ios::binary);
  if (!file.is_open()) {
    printf("SRAM: No existing battery save found (%s)\n", saveFilename.c_str());
    return;
  }
  
  // Read SRAM data
  file.read(reinterpret_cast<char*>(sram), sramSize);
  file.close();
  
  printf("SRAM: Loaded battery save from %s\n", saveFilename.c_str());
}

void SMBEmulator::saveSRAM() {
  if (!sram || !nesHeader.battery || !sramDirty || romBaseName.empty()) {
    return;
  }
  
  // Create battery save filename with .srm extension
  std::string saveFilename = romBaseName + ".srm";
  
  std::ofstream file(saveFilename, std::ios::binary);
  if (!file.is_open()) {
    printf("SRAM: Error - Could not save battery data to %s\n", saveFilename.c_str());
    return;
  }
  
  // Write SRAM data
  file.write(reinterpret_cast<const char*>(sram), sramSize);
  file.close();
  
  printf("SRAM: Saved battery data to %s\n", saveFilename.c_str());
  sramDirty = false;
}

void SMBEmulator::cleanupSRAM() {
  if (sram) {
    delete[] sram;
    sram = nullptr;
  }
  sramSize = 0;
  sramEnabled = false;
  sramDirty = false;
}

// Add public method to manually save (good for clean shutdown):
void SMBEmulator::forceSRAMSave() {
    if (sramDirty && nesHeader.battery) {
        saveSRAM();
    }
}
