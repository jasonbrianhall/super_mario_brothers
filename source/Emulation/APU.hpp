#ifndef APU_HPP
#define APU_HPP

#include <cstdint>

#ifndef __DJGPP__
#include <mutex>
#endif

#define AUDIO_BUFFER_LENGTH 4096

class Pulse;
class Triangle;
class Noise;

/**
 * Audio processing unit emulator.
 *
 * Output format is signed 8-bit mono (matches the AUDIO_S8 SDL device).
 */
class APU
{
public:
    APU();
    ~APU();

    /**
     * Step the APU by one video frame (1/frameRate seconds of NES time).
     */
    void stepFrame();

    /**
     * Output audio samples to the provided buffer. Always fills all
     * len bytes (pads with the last sample on underrun).
     * @param buffer Output buffer for audio samples
     * @param len Length of the buffer in bytes
     */
    void output(uint8_t* buffer, int len);

    /**
     * Write to an APU register.
     * @param address Register address
     * @param value Value to write
     */
    void writeRegister(uint16_t address, uint8_t value);

private:
    // Ring buffer of signed 8-bit samples
    int8_t audioBuffer[AUDIO_BUFFER_LENGTH];
    int readPos;
    int audioBufferLength;      /**< Amount of data currently in buffer */
    int8_t lastSample;

#ifndef __DJGPP__
    std::mutex bufferMutex;
#endif

    // Frame sequencer
    int frameStep;              /**< Current step of the frame sequencer */
    bool fiveStepMode;          /**< $4017 bit 7 */

    // Timing / resampling
    double cycleRemainder;      /**< Fractional CPU cycles carried between frames */
    double sampleClock;         /**< CPU cycles still needed for the next sample */
    uint32_t pulse1Sum;         /**< Channel outputs summed over the current sample */
    uint32_t pulse2Sum;
    uint32_t triangleSum;
    uint32_t noiseSum;
    uint32_t sumCycles;         /**< CPU cycles covered by the sums */

    // Output filters (approximation of the NES's analog output stage)
    int filterRate;
    float hpCoeff;
    float lpCoeff;
    float hpPrevIn;
    float hpPrevOut;
    float lpPrevOut;

    Pulse* pulse1;
    Pulse* pulse2;
    Triangle* triangle;
    Noise* noise;

    float pulseTable[32];
    float tndTable[204];

    float mixAverage();
    void pushSample(float value);

    void clockQuarterFrame();
    void clockHalfFrame();
    void clockFrameSequencer();
    void writeControl(uint8_t value);
    void writeFrameCounter(uint8_t value);
};

#endif // APU_HPP
