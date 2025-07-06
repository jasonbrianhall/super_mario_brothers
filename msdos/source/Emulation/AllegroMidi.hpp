// AllegroMIDIAudioSystem.hpp - Real-time APU to MIDI conversion using Allegro
#ifndef ALLEGRO_MIDI_AUDIO_SYSTEM_HPP
#define ALLEGRO_MIDI_AUDIO_SYSTEM_HPP

#include <cstdint>
#include <allegro.h>

// Forward declarations
class APU;

class AllegroMIDIAudioSystem {
private:
    APU* originalAPU;
    bool useMIDIMode;
    bool midiInitialized;
    
    struct GameChannel {
        uint16_t lastTimerPeriod;
        uint8_t lastVolume;
        uint8_t lastDuty;
        bool enabled;
        bool noteActive;
        uint8_t currentMidiNote;
        uint8_t midiChannel;     // MIDI channel (0-15)
        uint32_t lastUpdate;
    };
    
    GameChannel channels[4]; // P1, P2, Triangle, Noise
    
    // Private helper methods
    double getFrequencyFromTimer(uint16_t timer, bool isTriangle = false);
    uint8_t frequencyToMIDI(double freq);
    uint8_t apuVolumeToVelocity(uint8_t apuVol);
    void updateMIDIChannel(int channelIndex);
    void sendMIDINoteOn(uint8_t channel, uint8_t note, uint8_t velocity);
    void sendMIDINoteOff(uint8_t channel, uint8_t note);
    void sendMIDIProgramChange(uint8_t channel, uint8_t instrument);
    uint32_t getGameTicks();

public:
    AllegroMIDIAudioSystem(APU* apu);
    ~AllegroMIDIAudioSystem();
    
    bool initializeMIDI();
    void setupGameInstruments();
    void interceptAPURegister(uint16_t address, uint8_t value);
    void toggleAudioMode();
    bool isMIDIMode() const;
    void generateAudio(uint8_t* buffer, int length);
    void debugPrintChannels();
};

#endif // ALLEGRO_MIDI_AUDIO_SYSTEM_HPP
