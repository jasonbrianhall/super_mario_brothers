#ifndef APU_HPP
#define APU_HPP

#include <cstdint>

#define AUDIO_BUFFER_LENGTH 4096

class Pulse;
class Triangle;
class Noise;
class AllegroMIDIAudioSystem; // Forward declaration

/**
 * Audio processing unit emulator.
 */
class APU
{
public:
    APU();
    ~APU();

    /**
     * Step the APU by one frame.
     */
    void stepFrame();

    /**
     * Output audio samples to the provided buffer.
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

    /**
     * Toggle between APU and MIDI audio modes.
     */
    void toggleAudioMode();

    /**
     * Check if currently using MIDI mode.
     */
    bool isUsingMIDI() const;

    /**
     * Debug audio channels.
     */
    void debugAudio();

private:
    uint8_t audioBuffer[AUDIO_BUFFER_LENGTH];
    int audioBufferLength;      /**< Amount of data currently in buffer */

    int frameValue; /**< The value of the frame counter. */

    Pulse* pulse1;
    Pulse* pulse2;
    Triangle* triangle;
    Noise* noise;

    AllegroMIDIAudioSystem* gameAudio;  /**< Enhanced audio system */

    /**
     * Get the current mixed audio output sample.
     * @return 8-bit unsigned audio sample (0-255, 128=silence)
     */
    uint8_t getOutput();
    
    void stepEnvelope();
    void stepSweep();
    void stepLength();
    void writeControl(uint8_t value);
    
    struct MixCache {
        uint8_t pulse1_val;
        uint8_t pulse2_val;
        uint8_t triangle_val;
        uint8_t noise_val;
        uint8_t result;
        bool valid;
    };
    
    static MixCache outputCache[256];  // Cache recent calculations
    static int cacheIndex;
};

#endif // APU_HPP
