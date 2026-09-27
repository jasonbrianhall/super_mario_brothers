// Silent APU for the first bare-metal version: register writes are ignored.
#include "APU.hpp"

APU::APU() : audioBufferLength(0), frameValue(0),
             pulse1(nullptr), pulse2(nullptr), triangle(nullptr), noise(nullptr) {}
APU::~APU() {}
void APU::stepFrame() {}
void APU::output(uint8_t* buffer, int len) { for (int i = 0; i < len; i++) buffer[i] = 128; }
void APU::writeRegister(uint16_t, uint8_t) {}
