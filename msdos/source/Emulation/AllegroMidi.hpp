// AllegroMIDIAudioSystem.hpp - True NES-style synthesis
#ifndef ALLEGRO_MIDI_AUDIO_SYSTEM_HPP
#define ALLEGRO_MIDI_AUDIO_SYSTEM_HPP

#include <cstdint>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Forward declarations
class APU;

class AllegroMIDIAudioSystem {
private:
    APU* originalAPU;
    bool useFMMode;
    bool fmInitialized;
    
    struct GameChannel {
        uint16_t lastTimerPeriod;
        uint8_t lastVolume;
        uint8_t lastDuty;
        bool enabled;
        bool noteActive;
        uint32_t lastUpdate;
    };
    
    // NES-style synthesis channel structure
    struct FMChannel {
        double phase1;           // Primary oscillator phase
        double phase2;           // Secondary phase (unused for NES)
        double frequency;        // Current frequency in Hz
        double amplitude;        // Current amplitude (0.0-1.0)
        uint8_t instrumentIndex; // Timbre index
        bool active;            // Whether this channel is currently playing
        double dutyFactor;      // Pulse wave duty cycle (0.125, 0.25, 0.5, 0.75)
        uint32_t noiseShift;    // Noise generator shift register
    };
    
    GameChannel channels[4]; // P1, P2, Triangle, Noise
    FMChannel fmChannels[4]; // NES-style synthesis channels
    
    // Private helper methods
    double getFrequencyFromTimer(uint16_t timer, bool isTriangle = false);
    uint8_t frequencyToMIDI(double freq);
    double apuVolumeToAmplitude(uint8_t apuVol);
    uint32_t getGameTicks();
    
    // NES-style synthesis methods
    double generateNESWave(int channelIndex, double sampleRate);
    void setNESNote(int channelIndex, double frequency, double amplitude, uint8_t duty);
    void generateNESAudio(uint8_t* buffer, int length);
    void updateNESChannel(int channelIndex);
    
    // Legacy compatibility methods (for compilation)
    double generateFMSample(int channelIndex, double sampleRate);
    void setFMNote(int channelIndex, double frequency, double amplitude);
    void setFMInstrument(int channelIndex, uint8_t instrument);
    void generateFMAudio(uint8_t* buffer, int length);
    void updateFMChannel(int channelIndex);

public:
    AllegroMIDIAudioSystem(APU* apu);
    ~AllegroMIDIAudioSystem();
    
    bool initializeFM();
    void setupFMInstruments();
    void interceptAPURegister(uint16_t address, uint8_t value);
    void toggleAudioMode();
    bool isFMMode() const;
    void generateAudio(uint8_t* buffer, int length);
    void debugPrintChannels();
};

#endif // ALLEGRO_MIDI_AUDIO_SYSTEM_HPP
