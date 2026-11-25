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

// NTSC 2A03 CPU clock. Pulse/noise timers tick every other CPU cycle
// (one "APU cycle"); the triangle timer ticks every CPU cycle.
static const double CPU_CLOCK = 1789773.0;

/**
 * Shared envelope unit (pulse and noise).
 * Only a write to the 4th register of the channel ($4003/$4007/$400F)
 * restarts it -- writing the control register ($4000/$4004/$400C) does not.
 */
struct Envelope
{
    bool start = false;
    bool loop = false;
    bool constant = false;
    uint8_t period = 0;       // also the constant volume
    uint8_t divider = 0;
    uint8_t decay = 0;

    void write(uint8_t value)
    {
        loop = (value & 0x20) != 0;
        constant = (value & 0x10) != 0;
        period = value & 0x0f;
    }

    void clock()
    {
        if (start)
        {
            start = false;
            decay = 15;
            divider = period;
        }
        else if (divider == 0)
        {
            divider = period;
            if (decay > 0)
                decay--;
            else if (loop)
                decay = 15;
        }
        else
        {
            divider--;
        }
    }

    uint8_t volume() const { return constant ? period : decay; }
};

/**
 * Pulse waveform generator.
 */
class Pulse
{
    friend class APU;
public:
    explicit Pulse(uint8_t channel) : channel(channel) {}

    void writeControl(uint8_t value)
    {
        dutyMode = (value >> 6) & 3;
        lengthHalt = (value & 0x20) != 0;
        envelope.write(value);
    }

    void writeSweep(uint8_t value)
    {
        sweepEnabled = (value & 0x80) != 0;
        sweepPeriod = (value >> 4) & 7;
        sweepNegate = (value & 0x08) != 0;
        sweepShift = value & 7;
        sweepReload = true;
    }

    void writeTimerLow(uint8_t value)
    {
        timerPeriod = (timerPeriod & 0x0700) | value;
    }

    void writeTimerHigh(uint8_t value)
    {
        if (enabled)
            lengthValue = lengthTable[value >> 3];
        timerPeriod = (timerPeriod & 0x00ff) | ((uint16_t)(value & 7) << 8);
        envelope.start = true;
        dutyValue = 0;
    }

    void stepTimer()
    {
        if (timerValue == 0)
        {
            timerValue = timerPeriod;
            dutyValue = (dutyValue + 1) & 7;
        }
        else
        {
            timerValue--;
        }
    }

    void stepLength()
    {
        if (!lengthHalt && lengthValue > 0)
            lengthValue--;
    }

    uint16_t sweepTarget() const
    {
        uint16_t delta = timerPeriod >> sweepShift;
        if (sweepNegate)
        {
            // Pulse 1 uses one's complement, pulse 2 two's complement
            int t = (int)timerPeriod - delta - (channel == 1 ? 1 : 0);
            return t < 0 ? 0 : (uint16_t)t;
        }
        return timerPeriod + delta;
    }

    // The sweep unit mutes the channel whenever the period is too low or the
    // target period overflows -- even when the sweep itself is disabled.
    bool muted() const
    {
        return timerPeriod < 8 || sweepTarget() > 0x7ff;
    }

    void stepSweep()
    {
        if (sweepDivider == 0 && sweepEnabled && sweepShift != 0 && !muted())
            timerPeriod = sweepTarget();

        if (sweepDivider == 0 || sweepReload)
        {
            sweepDivider = sweepPeriod;
            sweepReload = false;
        }
        else
        {
            sweepDivider--;
        }
    }

    uint8_t output() const
    {
        if (lengthValue == 0 || muted() || dutyTable[dutyMode][dutyValue] == 0)
            return 0;
        return envelope.volume();
    }

private:
    uint8_t channel;
    bool enabled = false;
    bool lengthHalt = false;
    uint8_t lengthValue = 0;
    uint16_t timerPeriod = 0;
    uint16_t timerValue = 0;
    uint8_t dutyMode = 0;
    uint8_t dutyValue = 0;
    bool sweepReload = false;
    bool sweepEnabled = false;
    bool sweepNegate = false;
    uint8_t sweepShift = 0;
    uint8_t sweepPeriod = 0;
    uint8_t sweepDivider = 0;
    Envelope envelope;
};

/**
 * Triangle waveform generator.
 */
class Triangle
{
    friend class APU;
public:
    void writeControl(uint8_t value)
    {
        // Bit 7 is both the length-counter halt and the linear-counter
        // control flag. While it is set the linear counter keeps reloading,
        // so the note sustains instead of being cut off.
        control = (value & 0x80) != 0;
        counterPeriod = value & 0x7f;
    }

    void writeTimerLow(uint8_t value)
    {
        timerPeriod = (timerPeriod & 0x0700) | value;
    }

    void writeTimerHigh(uint8_t value)
    {
        if (enabled)
            lengthValue = lengthTable[value >> 3];
        timerPeriod = (timerPeriod & 0x00ff) | ((uint16_t)(value & 7) << 8);
        counterReload = true;
    }

    void stepTimer()
    {
        if (timerValue == 0)
        {
            timerValue = timerPeriod;
            // Very small periods are ultrasonic; real hardware produces a
            // ~7.5 value DC level. Freezing the sequencer avoids aliasing.
            if (lengthValue > 0 && counterValue > 0 && timerPeriod >= 2)
                dutyValue = (dutyValue + 1) & 31;
        }
        else
        {
            timerValue--;
        }
    }

    void stepLength()
    {
        if (!control && lengthValue > 0)
            lengthValue--;
    }

    void stepCounter()
    {
        if (counterReload)
            counterValue = counterPeriod;
        else if (counterValue > 0)
            counterValue--;

        if (!control)
            counterReload = false;
    }

    // The triangle holds its last level when silenced rather than
    // dropping to 0 (which would click).
    uint8_t output() const
    {
        return triangleTable[dutyValue];
    }

private:
    bool enabled = false;
    bool control = false;
    uint8_t lengthValue = 0;
    uint16_t timerPeriod = 0;
    uint16_t timerValue = 0;
    uint8_t dutyValue = 0;
    uint8_t counterPeriod = 0;
    uint8_t counterValue = 0;
    bool counterReload = false;
};

/**
 * Noise generator.
 */
class Noise
{
    friend class APU;
public:
    void writeControl(uint8_t value)
    {
        lengthHalt = (value & 0x20) != 0;
        envelope.write(value);
    }

    void writePeriod(uint8_t value)
    {
        mode = (value & 0x80) != 0;
        timerPeriod = noiseTable[value & 0x0f];
    }

    void writeLength(uint8_t value)
    {
        if (enabled)
            lengthValue = lengthTable[value >> 3];
        envelope.start = true;
    }

    // Clocked every CPU cycle: noiseTable holds periods in CPU cycles.
    void stepTimer()
    {
        if (timerValue == 0)
        {
            timerValue = timerPeriod - 1;
            uint16_t b1 = shiftRegister & 1;
            uint16_t b2 = (shiftRegister >> (mode ? 6 : 1)) & 1;
            shiftRegister >>= 1;
            shiftRegister |= (b1 ^ b2) << 14;
        }
        else
        {
            timerValue--;
        }
    }

    void stepLength()
    {
        if (!lengthHalt && lengthValue > 0)
            lengthValue--;
    }

    uint8_t output() const
    {
        if (lengthValue == 0 || (shiftRegister & 1))
            return 0;
        return envelope.volume();
    }

private:
    bool enabled = false;
    bool mode = false;
    uint16_t shiftRegister = 1;
    bool lengthHalt = false;
    uint8_t lengthValue = 0;
    uint16_t timerPeriod = 4;
    uint16_t timerValue = 0;
    Envelope envelope;
};

APU::APU()
{
    readPos = 0;
    audioBufferLength = 0;
    lastSample = 0;
    memset(audioBuffer, 0, sizeof(audioBuffer));

    frameStep = 0;
    fiveStepMode = false;

    cycleRemainder = 0.0;
    sampleClock = 0.0;
    mixAccumulator = 0.0;
    mixCount = 0;
    oddCycle = false;

    hpPrevIn = 0.0f;
    hpPrevOut = 0.0f;
    lpPrevOut = 0.0f;

    // Non-linear mixer lookup tables (nesdev "APU Mixer")
    pulseTable[0] = 0.0f;
    for (int n = 1; n < 31; n++)
        pulseTable[n] = 95.52f / (8128.0f / n + 100.0f);
    tndTable[0] = 0.0f;
    for (int n = 1; n < 203; n++)
        tndTable[n] = 163.67f / (24329.0f / n + 100.0f);

    pulse1 = new Pulse(1);
    pulse2 = new Pulse(2);
    triangle = new Triangle;
    noise = new Noise;
}

APU::~APU()
{
    delete pulse1;
    delete pulse2;
    delete triangle;
    delete noise;
}

float APU::getOutput()
{
    return pulseTable[pulse1->output() + pulse2->output()] +
           tndTable[3 * triangle->output() + 2 * noise->output()];
}

void APU::pushSample(float value)
{
    // First-order high-pass (~90 Hz) removes the DC offset of the NES mixer,
    // then a gentle low-pass (~14 kHz), roughly like the console's output.
    float rate = (float)Configuration::getAudioFrequency();
    float dt = 1.0f / rate;
    float hpRC = 1.0f / (2.0f * 3.14159265f * 90.0f);
    float lpRC = 1.0f / (2.0f * 3.14159265f * 14000.0f);
    float hpA = hpRC / (hpRC + dt);
    float lpA = dt / (lpRC + dt);

    float hp = hpA * (hpPrevOut + value - hpPrevIn);
    hpPrevIn = value;
    hpPrevOut = hp;
    lpPrevOut += lpA * (hp - lpPrevOut);

    int s = (int)lrintf(lpPrevOut * 255.0f);
    if (s > 127) s = 127;
    if (s < -128) s = -128;

    if (audioBufferLength >= AUDIO_BUFFER_LENGTH)
    {
        // Overflow: drop the oldest sample to keep latency bounded
        readPos = (readPos + 1) % AUDIO_BUFFER_LENGTH;
        audioBufferLength--;
    }
    audioBuffer[(readPos + audioBufferLength) % AUDIO_BUFFER_LENGTH] = (int8_t)s;
    audioBufferLength++;
}

void APU::output(uint8_t* buffer, int len)
{
#ifndef __DJGPP__
    std::lock_guard<std::mutex> lock(bufferMutex);
#endif
    int n = (len < audioBufferLength) ? len : audioBufferLength;
    for (int i = 0; i < n; i++)
    {
        lastSample = audioBuffer[readPos];
        buffer[i] = (uint8_t)lastSample;
        readPos = (readPos + 1) % AUDIO_BUFFER_LENGTH;
    }
    audioBufferLength -= n;

    // Underrun: SDL2 does not pre-clear the buffer, so fill the rest.
    // Holding the last value avoids a pop.
    for (int i = n; i < len; i++)
        buffer[i] = (uint8_t)lastSample;
}

void APU::clockQuarterFrame()
{
    pulse1->envelope.clock();
    pulse2->envelope.clock();
    triangle->stepCounter();
    noise->envelope.clock();
}

void APU::clockHalfFrame()
{
    pulse1->stepSweep();
    pulse2->stepSweep();
    pulse1->stepLength();
    pulse2->stepLength();
    triangle->stepLength();
    noise->stepLength();
}

void APU::clockFrameSequencer()
{
    frameStep++;
    if (!fiveStepMode)
    {
        // 4-step: quarter on 1,2,3,4; half on 2,4
        clockQuarterFrame();
        if (frameStep == 2 || frameStep == 4)
            clockHalfFrame();
        if (frameStep >= 4)
            frameStep = 0;
    }
    else
    {
        // 5-step: quarter on 1,2,3,5; half on 2,5; step 4 does nothing
        if (frameStep != 4)
            clockQuarterFrame();
        if (frameStep == 2 || frameStep == 5)
            clockHalfFrame();
        if (frameStep >= 5)
            frameStep = 0;
    }
}

void APU::stepFrame()
{
    double frameRate = (double)Configuration::getFrameRate();
    double audioRate = (double)Configuration::getAudioFrequency();
    if (frameRate <= 0 || audioRate <= 0)
        return;

    double cyclesPerSample = CPU_CLOCK / audioRate;

    double exactCycles = CPU_CLOCK / frameRate + cycleRemainder;
    int cycles = (int)exactCycles;
    cycleRemainder = exactCycles - cycles;

#ifndef __DJGPP__
    std::lock_guard<std::mutex> lock(bufferMutex);
#endif

    // The frame sequencer is clocked roughly every quarter of a frame
    // (~7457 CPU cycles, i.e. 240 Hz).
    int nextQuarter = 1;
    int quarterLength = cycles / 4;

    for (int c = 0; c < cycles; c++)
    {
        triangle->stepTimer();
        noise->stepTimer();

        oddCycle = !oddCycle;
        if (oddCycle)
        {
            pulse1->stepTimer();
            pulse2->stepTimer();

            // Box-filter the mixer output over each output sample period
            // (instead of point sampling) to cut down on aliasing.
            mixAccumulator += getOutput();
            mixCount++;
        }

        sampleClock += 1.0;
        if (sampleClock >= cyclesPerSample)
        {
            sampleClock -= cyclesPerSample;
            float v = mixCount ? (float)(mixAccumulator / mixCount) : getOutput();
            mixAccumulator = 0.0;
            mixCount = 0;
            pushSample(v);
        }

        if (nextQuarter <= 4 && c + 1 == quarterLength * nextQuarter)
        {
            clockFrameSequencer();
            nextQuarter++;
        }
    }
}

void APU::writeControl(uint8_t value)
{
    pulse1->enabled = (value & 1) != 0;
    pulse2->enabled = (value & 2) != 0;
    triangle->enabled = (value & 4) != 0;
    noise->enabled = (value & 8) != 0;

    if (!pulse1->enabled) pulse1->lengthValue = 0;
    if (!pulse2->enabled) pulse2->lengthValue = 0;
    if (!triangle->enabled) triangle->lengthValue = 0;
    if (!noise->enabled) noise->lengthValue = 0;
}

void APU::writeFrameCounter(uint8_t value)
{
    // A write resets the frame sequencer. In 5-step mode (bit 7 set) it also
    // immediately clocks the quarter- and half-frame units. SMB writes $FF
    // here at the start of every sound-engine update.
    fiveStepMode = (value & 0x80) != 0;
    frameStep = 0;
    if (fiveStepMode)
    {
        clockQuarterFrame();
        clockHalfFrame();
    }
}

void APU::writeRegister(uint16_t address, uint8_t value)
{
    switch (address)
    {
    case 0x4000: pulse1->writeControl(value); break;
    case 0x4001: pulse1->writeSweep(value); break;
    case 0x4002: pulse1->writeTimerLow(value); break;
    case 0x4003: pulse1->writeTimerHigh(value); break;
    case 0x4004: pulse2->writeControl(value); break;
    case 0x4005: pulse2->writeSweep(value); break;
    case 0x4006: pulse2->writeTimerLow(value); break;
    case 0x4007: pulse2->writeTimerHigh(value); break;
    case 0x4008: triangle->writeControl(value); break;
    case 0x400a: triangle->writeTimerLow(value); break;
    case 0x400b: triangle->writeTimerHigh(value); break;
    case 0x400c: noise->writeControl(value); break;
    case 0x400e: noise->writePeriod(value); break;
    case 0x400f: noise->writeLength(value); break;
    case 0x4015: writeControl(value); break;
    case 0x4017: writeFrameCounter(value); break;
    default: break;
    }
}
