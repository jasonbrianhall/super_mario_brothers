#ifndef APU_HPP
#define APU_HPP

#include <cstdint>

#define AUDIO_BUFFER_LENGTH 4096

class Pulse;
class Triangle;
class Noise;
class AllegroMIDIAudioSystem; // Forward declaration

/**
 * Improved Audio Processing Unit emulator based on FCE Ultra.
 */
class APU
{
public:
    APU();
    ~APU();

    /**
     * Step the APU by one frame with improved timing.
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
     * Toggle between APU and FM audio modes.
     */
    void toggleAudioMode();

    /**
     * Check if currently using MIDI/FM mode.
     */
    bool isUsingMIDI() const;

    /**
     * Debug audio channels.
     */
    void debugAudio();

private:
    uint8_t audioBuffer[AUDIO_BUFFER_LENGTH];
    int audioBufferLength;      /**< Amount of data currently in buffer */

    int frameValue;             /**< The value of the frame counter */
    int frameCounter;           /**< Current frame sequencer position */
    int frameMode;              /**< Frame sequencer mode (4 or 5 step) */
    bool frameIRQ;              /**< Frame IRQ enabled flag */
    int frameSequencerMode;     /**< 0 = 4-step, 1 = 5-step */

    Pulse* pulse1;
    Pulse* pulse2;
    Triangle* triangle;
    Noise* noise;

    AllegroMIDIAudioSystem* gameAudio;  /**< Enhanced audio system */

    /**
     * Get the current mixed audio output sample using improved mixing.
     * @return 8-bit unsigned audio sample (0-255)
     */
    uint8_t getOutput();
    
    /**
     * Step envelope generators for all channels.
     */
    void stepEnvelope();
    
    /**
     * Step sweep units for pulse channels.
     */
    void stepSweep();
    
    /**
     * Step length counters for all channels.
     */
    void stepLength();
    
    /**
     * Write to the control register (0x4015).
     */
    void writeControl(uint8_t value);
    
    /**
     * Improved mixing cache structure.
     */
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
