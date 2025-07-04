#include <cmath>
#include <cstring>
#include <iostream>

#ifdef __DJGPP__
#include <dos.h>
#include <pc.h>
#include <dpmi.h>
#endif

#include "../Configuration.hpp"

#include "APU.hpp"

static const uint8_t lengthTable[] = {
    10, 254, 20, 2, 40, 4, 80, 6, 160, 8, 60, 10, 14, 12, 26, 14,
    12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
};

static const uint8_t dutyTable[][8] = {
    {0, 1, 0, 0, 0, 0, 0, 0},
    {0, 1, 1, 0, 0, 0, 0, 0},
    {0, 1, 1, 1, 1, 0, 0, 0},
    {1, 0, 0, 1, 1, 1, 1, 1}
};

static const uint8_t triangleTable[] = {
    15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
};

static const uint16_t noiseTable[] = {
    4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068
};

/**
 * Pulse waveform generator.
 */
class Pulse
{
    friend class APU;
public:
    Pulse(uint8_t channel)
    {
        enabled = false;
        this->channel = channel;
        lengthEnabled = false;
        lengthValue = 0;
        timerPeriod = 0;
        timerValue = 0;
        dutyMode = 0;
        dutyValue = 0;
        sweepReload = false;
        sweepEnabled = false;
        sweepNegate = false;
        sweepShift = 0;
        sweepPeriod = 0;
        sweepValue = 0;
        envelopeEnabled = false;
        envelopeLoop = false;
        envelopeStart = false;
        envelopePeriod = 0;
        envelopeValue = 0;
        envelopeVolume = 0;
        constantVolume = 0;
    }

    void writeControl(uint8_t value)
    {
        dutyMode = (value >> 6) & 3;
        lengthEnabled = ((value >> 5) & 1) == 0;
        envelopeLoop = ((value >> 5) & 1) == 1;
        envelopeEnabled = ((value >> 4) & 1) == 0;
        envelopePeriod = value & 15;
        constantVolume = value & 15;
        envelopeStart = true;
    }

    void writeSweep(uint8_t value)
    {
        sweepEnabled = ((value >> 7) & 1) == 1;
        sweepPeriod = ((value >> 4) & 7) + 1;
        sweepNegate = ((value >> 3) & 1) == 1;
        sweepShift = value & 7;
        sweepReload = true;
    }

    void writeTimerLow(uint8_t value)
    {
        timerPeriod = (timerPeriod & 0xff00) | (uint16_t)value;
    }

    void writeTimerHigh(uint8_t value)
    {
        lengthValue = lengthTable[value >> 3];
        timerPeriod = (timerPeriod & 0xff) | ((uint16_t)(value & 7) << 8);
        envelopeStart = true;
        dutyValue = 0;
    }

    void stepTimer()
    {
        if (timerValue == 0)
        {
            timerValue = timerPeriod;
            dutyValue = (dutyValue + 1) % 8;
        }
        else
        {
            timerValue--;
        }
    }

    void stepEnvelope()
    {
        if (envelopeStart)
        {
            envelopeVolume = 15;
            envelopeValue = envelopePeriod;
            envelopeStart = false;
        }
        else if (envelopeValue > 0)
        {
            envelopeValue--;
        }
        else
        {
            if (envelopeVolume > 0)
            {
                envelopeVolume--;
            }
            else if (envelopeLoop)
            {
                envelopeVolume = 15;
            }
            envelopeValue = envelopePeriod;
        }
    }

    void stepSweep()
    {
        if (sweepReload)
        {
            if (sweepEnabled && sweepValue == 0)
            {
                sweep();
            }
            sweepValue = sweepPeriod;
            sweepReload = false;
        }
        else if (sweepValue > 0)
        {
            sweepValue--;
        }
        else
        {
            if (sweepEnabled)
            {
                sweep();
            }
            sweepValue = sweepPeriod;
        }
    }

    void stepLength()
    {
        if (lengthEnabled && lengthValue > 0)
        {
            lengthValue--;
        }
    }

    void sweep()
    {
        uint16_t delta = timerPeriod >> sweepShift;
        if (sweepNegate)
        {
            timerPeriod -= delta;
            if (channel == 1)
            {
                timerPeriod--;
            }
        }
        else
        {
            timerPeriod += delta;
        }
    }

    uint8_t output()
    {
        if (!enabled)
        {
            return 0;
        }
        if (lengthValue == 0)
        {
            return 0;
        }
        if (dutyTable[dutyMode][dutyValue] == 0)
        {
            return 0;
        }
        if (timerPeriod < 8 || timerPeriod > 0x7ff)
        {
            return 0;
        }
        if (envelopeEnabled)
        {
            return envelopeVolume;
        }
        else
        {
            return constantVolume;
        }
    }

private:
    bool enabled;
    uint8_t channel;
    bool lengthEnabled;
    uint8_t lengthValue;
    uint16_t timerPeriod;
    uint16_t timerValue;
    uint8_t dutyMode;
    uint8_t dutyValue;
    bool sweepReload;
    bool sweepEnabled;
    bool sweepNegate;
    uint8_t sweepShift;
    uint8_t sweepPeriod;
    uint8_t sweepValue;
    bool envelopeEnabled;
    bool envelopeLoop;
    bool envelopeStart;
    uint8_t envelopePeriod;
    uint8_t envelopeValue;
    uint8_t envelopeVolume;
    uint8_t constantVolume;
};

/**
 * Triangle waveform generator.
 */
class Triangle
{
    friend class APU;
public:
    Triangle()
    {
        enabled = false;
        lengthEnabled = false;
        lengthValue = 0;
        timerPeriod = 0;
        timerValue = 0;  // This was missing!
        dutyValue = 0;
        counterPeriod = 0;
        counterValue = 0;
        counterReload = false;
    }

    void writeControl(uint8_t value)
    {
        lengthEnabled = ((value >> 7) & 1) == 0;
        counterPeriod = value & 0x7f;
    }

    void writeTimerLow(uint8_t value)
    {
        timerPeriod = (timerPeriod & 0xff00) | (uint16_t)value;
    }

    void writeTimerHigh(uint8_t value)
    {
        lengthValue = lengthTable[value >> 3];
        timerPeriod = (timerPeriod & 0x00ff) | ((uint16_t)(value & 7) << 8);
        timerValue = timerPeriod;
        counterReload = true;
    }

    void stepTimer()
    {
        if (timerValue == 0)
        {
            timerValue = timerPeriod;
            if (lengthValue > 0 && counterValue > 0)
            {
                dutyValue = (dutyValue + 1) % 32;
            }
        }
        else
        {
            timerValue--;
        }
    }

    void stepLength()
    {
        if (lengthEnabled && lengthValue > 0)
        {
            lengthValue--;
        }
    }

    void stepCounter()
    {
        if (counterReload)
        {
            counterValue = counterPeriod;
        }
        else if (counterValue > 0)
        {
            counterValue--;
        }
        if (lengthEnabled)
        {
            counterReload = false;
        }
    }

    uint8_t output()
    {
        if (!enabled)
        {
            return 0;
        }
        if (lengthValue == 0)
        {
            return 0;
        }
        if (counterValue == 0)
        {
            return 0;
        }
        return triangleTable[dutyValue];
    }

private:
    bool enabled;
    bool lengthEnabled;
    uint8_t lengthValue;
    uint16_t timerPeriod;
    uint16_t timerValue;
    uint8_t dutyValue;
    uint8_t counterPeriod;
    uint8_t counterValue;
    bool counterReload;
};

class Noise
{
    friend class APU;
public:
    Noise()
    {
        enabled = false;
        mode = false;
        shiftRegister = 1;
        lengthEnabled = false;
        lengthValue = 0;
        timerPeriod = 0;
        timerValue = 0;
        envelopeEnabled = false;
        envelopeLoop = false;
        envelopeStart = false;
        envelopePeriod = 0;
        envelopeValue = 0;
        envelopeVolume = 0;
        constantVolume = 0;
    }

    void writeControl(uint8_t value)
    {
        lengthEnabled = ((value >> 5) & 1) == 0;
        envelopeLoop = ((value >> 5) & 1) == 1;
        envelopeEnabled = ((value >> 4) & 1) == 0;
        envelopePeriod = value & 15;
        constantVolume = value & 15;
        envelopeStart = true;
    }

    void writePeriod(uint8_t value)
    {
        mode = (value & 0x80) == 0x80;
        timerPeriod = noiseTable[value & 0x0f];
    }

    void writeLength(uint8_t value)
    {
        lengthValue = lengthTable[value >> 3];
        envelopeStart = true;
    }

    void stepTimer()
    {
        if (timerValue == 0)
        {
            timerValue = timerPeriod;
            uint8_t shift;
            if (mode)
            {
                shift = 6;
            }
            else
            {
                shift = 1;
            }
            uint16_t b1 = shiftRegister & 1;
            uint16_t b2 = (shiftRegister >> shift) & 1;
            shiftRegister >>= 1;
            shiftRegister |= (b1 ^ b2) << 14;
        }
        else
        {
            timerValue--;
        }
    }

    void stepEnvelope()
    {
        if (envelopeStart)
        {
            envelopeVolume = 15;
            envelopeValue = envelopePeriod;
            envelopeStart = false;
        }
        else if (envelopeValue > 0)
        {
            envelopeValue--;
        }
        else
        {
            if (envelopeVolume > 0)
            {
                envelopeVolume--;
            }
            else if (envelopeLoop)
            {
                envelopeVolume = 15;
            }
            envelopeValue = envelopePeriod;
        }
    }

    void stepLength()
    {
        if (lengthEnabled && lengthValue > 0)
        {
            lengthValue--;
        }
    }

    uint8_t output()
    {
        if (!enabled)
        {
            return 0;
        }
        if (lengthValue == 0)
        {
            return 0;
        }
        if ((shiftRegister & 1) == 1)
        {
            return 0;
        }
        if (envelopeEnabled)
        {
            return envelopeVolume;
        }
        else
        {
            return constantVolume;
        }
    }

private:
    bool enabled;
    bool mode;
    uint16_t shiftRegister;
    bool lengthEnabled;
    uint8_t lengthValue;
    uint16_t timerPeriod;
    uint16_t timerValue;
    bool envelopeEnabled;
    bool envelopeLoop;
    bool envelopeStart;
    uint8_t envelopePeriod;
    uint8_t envelopeValue;
    uint8_t envelopeVolume;
    uint8_t constantVolume;
};

APU::APU()
{
    frameValue = 0;
    audioBufferLength = 0;

    // Initialize pointers to null first for safety
    pulse1 = nullptr;
    pulse2 = nullptr;
    triangle = nullptr;
    noise = nullptr;

    // Clear audio buffer
    memset(audioBuffer, 0, AUDIO_BUFFER_LENGTH);

    try {
        pulse1 = new Pulse(1);
        pulse2 = new Pulse(2);
        triangle = new Triangle;
        noise = new Noise;
        
        #ifdef __DJGPP__
        printf("APU initialized for DOS - all objects created successfully\n");
        #else
        printf("APU initialized for Linux\n");
        #endif
    } catch (...) {
        printf("ERROR: Failed to create APU objects\n");
        // Clean up any partially created objects
        if (pulse1) { delete pulse1; pulse1 = nullptr; }
        if (pulse2) { delete pulse2; pulse2 = nullptr; }
        if (triangle) { delete triangle; triangle = nullptr; }
        if (noise) { delete noise; noise = nullptr; }
    }
}

APU::~APU()
{
    if (pulse1) {
        delete pulse1;
        pulse1 = nullptr;
    }
    if (pulse2) {
        delete pulse2;
        pulse2 = nullptr;
    }
    if (triangle) {
        delete triangle;
        triangle = nullptr;
    }
    if (noise) {
        delete noise;
        noise = nullptr;
    }
}

uint8_t APU::getOutput()
{
    // Get raw channel outputs (0-15 range)
    uint8_t pulse1Out = pulse1->output();
    uint8_t pulse2Out = pulse2->output();
    uint8_t triangleOut = triangle->output();
    uint8_t noiseOut = noise->output();
    
    // Simpler, more accurate NES mixer
    // Based on Blargg's NES sound quality doc
    double pulse_out = 0;
    if (pulse1Out || pulse2Out) {
        pulse_out = 95.88 / (8128.0 / (pulse1Out + pulse2Out) + 100.0);
    }
    
    double tnd_out = 0;
    if (triangleOut || noiseOut) {
        tnd_out = 159.79 / (1.0 / (triangleOut / 8227.0 + noiseOut / 12241.0) + 100.0);
    }
    
    // Combine and scale to 8-bit unsigned
    double output = pulse_out + tnd_out;
    
    // Scale to 8-bit unsigned (0-255, 128 = silence)
    int sample = (int)(output * 255.0 + 128.5);
    if (sample > 255) sample = 255;
    if (sample < 0) sample = 0;
    
    return (uint8_t)sample;
}

void APU::output(uint8_t* buffer, int len)
{
    // Use the exact same buffer management as the original
    len = (len > audioBufferLength) ? audioBufferLength : len;
    if (len > audioBufferLength)
    {
        memcpy(buffer, audioBuffer, audioBufferLength);
        audioBufferLength = 0;
    }
    else
    {
        memcpy(buffer, audioBuffer, len);
        audioBufferLength -= len;
        memmove(audioBuffer, audioBuffer + len, audioBufferLength);
    }
}

void APU::stepFrame()
{
    // Comprehensive safety checks
    if (!pulse1 || !pulse2 || !triangle || !noise) {
        // Don't spam error messages
        static int errorCount = 0;
        if (errorCount++ < 3) {
            printf("ERROR: APU objects are null\n");
        }
        return;
    }

    // Reset buffer if it's getting too full
    if (audioBufferLength >= AUDIO_BUFFER_LENGTH - 200) {
        audioBufferLength = 0;  // Reset buffer instead of failing
    }

    int frequency = Configuration::getAudioFrequency();
    int frameRate = Configuration::getFrameRate();
    
    // Validate configuration
    if (frequency <= 0 || frequency > 48000 || frameRate <= 0 || frameRate > 120) {
        static int configErrorCount = 0;
        if (configErrorCount++ < 3) {
            printf("ERROR: Invalid audio config: freq=%d, rate=%d\n", frequency, frameRate);
        }
        return;
    }

    // Simplified frame stepping - less likely to crash
    for (int i = 0; i < 4; i++)
    {
        frameValue = (frameValue + 1) % 5;
        
        // Step audio components
        if (frameValue == 1 || frameValue == 3) {
            stepEnvelope();
        } else if (frameValue == 0 || frameValue == 2) {
            stepEnvelope();
            stepSweep();
            stepLength();
        }

        // Calculate samples needed for this quarter frame
        int samplesToWrite = frequency / (frameRate * 4);
        if (i == 3) {
            // Handle remainder on last iteration
            samplesToWrite = (frequency / frameRate) - (3 * frequency / (frameRate * 4));
        }
        
        // Clamp to reasonable values
        if (samplesToWrite < 1) samplesToWrite = 1;
        if (samplesToWrite > 500) samplesToWrite = 500;
        
        // Generate audio samples with simplified timing
        int samplesGenerated = 0;
        for (int step = 0; step < 1000 && samplesGenerated < samplesToWrite; step++) {
            // Step the audio hardware every iteration
            if (pulse1) pulse1->stepTimer();
            if (pulse2) pulse2->stepTimer();
            if (noise) noise->stepTimer();
            if (triangle) {
                triangle->stepTimer();
                triangle->stepTimer();  // Triangle channel runs at 2x
            }
            
            // Generate a sample every few steps
            if (step % (1000 / samplesToWrite) == 0 && samplesGenerated < samplesToWrite) {
                if (audioBufferLength < AUDIO_BUFFER_LENGTH - 1) {
                    audioBuffer[audioBufferLength] = getOutput();
                    audioBufferLength++;
                    samplesGenerated++;
                } else {
                    break;  // Buffer full
                }
            }
        }
    }
}

void APU::stepEnvelope()
{
    if (pulse1) pulse1->stepEnvelope();
    if (pulse2) pulse2->stepEnvelope();
    if (triangle) triangle->stepCounter();
    if (noise) noise->stepEnvelope();
}

void APU::stepSweep()
{
    if (pulse1) pulse1->stepSweep();
    if (pulse2) pulse2->stepSweep();
}

void APU::stepLength()
{
    if (pulse1) pulse1->stepLength();
    if (pulse2) pulse2->stepLength();
    if (triangle) triangle->stepLength();
    if (noise) noise->stepLength();
}

void APU::writeControl(uint8_t value)
{
    if (pulse1) pulse1->enabled = (value & 1) == 1;
    if (pulse2) pulse2->enabled = (value & 2) == 2;
    if (triangle) triangle->enabled = (value & 4) == 4;
    if (noise) noise->enabled = (value & 8) == 8;
    
    if (pulse1 && !pulse1->enabled) {
        pulse1->lengthValue = 0;
    }
    if (pulse2 && !pulse2->enabled) {
        pulse2->lengthValue = 0;
    }
    if (triangle && !triangle->enabled) {
        triangle->lengthValue = 0;
    }
    if (noise && !noise->enabled) {
        noise->lengthValue = 0;
    }
}

void APU::writeRegister(uint16_t address, uint8_t value)
{
    switch (address)
    {
    case 0x4000:
        if (pulse1) pulse1->writeControl(value);
        break;
    case 0x4001:
        if (pulse1) pulse1->writeSweep(value);
        break;
    case 0x4002:
        if (pulse1) pulse1->writeTimerLow(value);
        break;
    case 0x4003:
        if (pulse1) pulse1->writeTimerHigh(value);
        break;
    case 0x4004:
        if (pulse2) pulse2->writeControl(value);
        break;
    case 0x4005:
        if (pulse2) pulse2->writeSweep(value);
        break;
    case 0x4006:
        if (pulse2) pulse2->writeTimerLow(value);
        break;
    case 0x4007:
        if (pulse2) pulse2->writeTimerHigh(value);
        break;
    case 0x4008:
        if (triangle) triangle->writeControl(value);
        break;
    case 0x400a:
        if (triangle) triangle->writeTimerLow(value);
        break;
    case 0x400b:
        if (triangle) triangle->writeTimerHigh(value);
        break;
    case 0x400c:
        if (noise) noise->writeControl(value);
        break;
    case 0x400d:
    case 0x400e:
        if (noise) noise->writePeriod(value);
        break;
    case 0x400f:
        if (noise) noise->writeLength(value);
        break;
    case 0x4015:
        writeControl(value);
        break;
    case 0x4017:
        stepEnvelope();
        stepSweep();
        stepLength();
        break;
    default:
        break;
    }
}
