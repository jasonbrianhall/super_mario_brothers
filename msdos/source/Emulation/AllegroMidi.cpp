// AllegroMIDIAudioSystem.cpp - FM synthesis only implementation
#include "AllegroMidi.hpp"
#include "APU.hpp"
#include <cmath>
#include <cstdio>
#include <ctime>
#include <cstring>

// Forward declare FM instrument structure
#ifndef FM_INSTRUMENT_DEFINED
struct FMInstrument {
    uint8_t modChar1, carChar1;
    uint8_t modChar2, carChar2;
    uint8_t modChar3, carChar3;
    uint8_t modChar4, carChar4;
    uint8_t modChar5, carChar5;
    uint8_t fbConn;
    uint8_t percNote;
};

extern FMInstrument adl[181]; // From instruments.c
extern void initFMInstruments(); // From instruments.c
#define FM_INSTRUMENT_DEFINED
#endif

AllegroMIDIAudioSystem::AllegroMIDIAudioSystem(APU* apu) 
    : originalAPU(apu), useFMMode(false), fmInitialized(false) {
    
    // Initialize channels
    for (int i = 0; i < 4; i++) {
        channels[i] = {};
        
        // Initialize FM oscillators
        fmChannels[i].phase1 = 0.0;
        fmChannels[i].phase2 = 0.0;
        fmChannels[i].frequency = 440.0;
        fmChannels[i].amplitude = 0.0;
        fmChannels[i].instrumentIndex = 80; // Default to Lead 1 (square)
        fmChannels[i].active = false;
    }
    
    // Initialize FM instruments
    initFMInstruments();
    
    printf("Enhanced audio system created (FM synthesis available)\n");
}

AllegroMIDIAudioSystem::~AllegroMIDIAudioSystem() {
    // Turn off any active FM notes
    if (fmInitialized) {
        for (int i = 0; i < 4; i++) {
            fmChannels[i].active = false;
        }
        printf("FM synthesis cleanup completed\n");
        fmInitialized = false;
    }
}

bool AllegroMIDIAudioSystem::initializeFM() {
    if (fmInitialized) {
        printf("FM synthesis already initialized\n");
        return true;
    }
    
    printf("FM synthesis audio system initialized\n");
    fmInitialized = true;
    return true;
}

uint32_t AllegroMIDIAudioSystem::getGameTicks() {
    #ifdef __DJGPP__
    static uint32_t ticks = 0;
    return ++ticks;
    #else
    return (uint32_t)(clock() * 1000 / CLOCKS_PER_SEC);
    #endif
}

double AllegroMIDIAudioSystem::getFrequencyFromTimer(uint16_t timer, bool isTriangle) {
    if (timer == 0) return 0.0;
    
    const double NES_CPU_CLOCK = 1789773.0;
    if (isTriangle) {
        return NES_CPU_CLOCK / (32.0 * (timer + 1));
    } else {
        return NES_CPU_CLOCK / (16.0 * (timer + 1));
    }
}

uint8_t AllegroMIDIAudioSystem::frequencyToMIDI(double freq) {
    if (freq <= 8.0) return 0;
    
    double noteFloat = 69.0 + 12.0 * log2(freq / 440.0);
    int note = (int)round(noteFloat);
    
    return (note < 0) ? 0 : (note > 127) ? 127 : (uint8_t)note;
}

double AllegroMIDIAudioSystem::apuVolumeToAmplitude(uint8_t apuVol) {
    if (apuVol == 0) return 0.0;
    return (apuVol / 15.0) * 0.3; // Scale to 0.3 max to prevent clipping
}

// FM Synthesis Functions
double AllegroMIDIAudioSystem::generateFMSample(int channelIndex, double sampleRate) {
    FMChannel& ch = fmChannels[channelIndex];
    if (!ch.active) return 0.0;
    
    FMInstrument& inst = adl[ch.instrumentIndex];
    
    // Convert FM instrument parameters to usable values
    double modRatio = ((inst.modChar2 & 0x0F) + 1) * 0.5; // Frequency ratio
    double carRatio = ((inst.carChar2 & 0x0F) + 1) * 0.5;
    double modIndex = (inst.modChar1 & 0x3F) / 16.0; // Modulation depth
    double feedback = ((inst.fbConn >> 1) & 0x07) / 8.0; // Feedback amount
    
    // Generate modulator
    double modFreq = ch.frequency * modRatio;
    double modOutput = sin(ch.phase1) * modIndex;
    ch.phase1 += (2.0 * M_PI * modFreq) / sampleRate;
    if (ch.phase1 > 2.0 * M_PI) ch.phase1 -= 2.0 * M_PI;
    
    // Generate carrier with modulation
    double carFreq = ch.frequency * carRatio;
    double carOutput = sin(ch.phase2 + modOutput) * ch.amplitude;
    ch.phase2 += (2.0 * M_PI * carFreq) / sampleRate;
    if (ch.phase2 > 2.0 * M_PI) ch.phase2 -= 2.0 * M_PI;
    
    return carOutput;
}

void AllegroMIDIAudioSystem::setFMNote(int channelIndex, double frequency, double amplitude) {
    FMChannel& ch = fmChannels[channelIndex];
    
    if (amplitude > 0.0 && frequency > 0.0) {
        ch.frequency = frequency;
        ch.amplitude = amplitude;
        ch.active = true;
        
        uint8_t midiNote = frequencyToMIDI(frequency);
        printf("FM Ch%d: Note %d (%.1fHz) Amp %.2f\n", channelIndex, midiNote, frequency, amplitude);
    } else {
        ch.active = false;
        ch.amplitude = 0.0;
        printf("FM Ch%d: Note Off\n", channelIndex);
    }
}

void AllegroMIDIAudioSystem::setFMInstrument(int channelIndex, uint8_t instrument) {
    if (instrument < 128) {
        fmChannels[channelIndex].instrumentIndex = instrument;
        printf("FM Ch%d: Instrument %d\n", channelIndex, instrument);
    }
}

void AllegroMIDIAudioSystem::generateFMAudio(uint8_t* buffer, int length) {
    const double sampleRate = 22050.0; // Assume 22kHz sample rate
    
    for (int i = 0; i < length; i++) {
        double mixedSample = 0.0;
        
        // Mix all active FM channels
        for (int ch = 0; ch < 4; ch++) {
            mixedSample += generateFMSample(ch, sampleRate);
        }
        
        // Convert to 8-bit unsigned (128 = silence)
        int sample = (int)(mixedSample * 127.0) + 128;
        if (sample < 0) sample = 0;
        if (sample > 255) sample = 255;
        
        buffer[i] = (uint8_t)sample;
    }
}

void AllegroMIDIAudioSystem::updateFMChannel(int channelIndex) {
    GameChannel& ch = channels[channelIndex];
    
    if (!ch.enabled || !fmInitialized) {
        if (ch.noteActive) {
            setFMNote(channelIndex, 0.0, 0.0);
            ch.noteActive = false;
        }
        return;
    }
    
    double freq = getFrequencyFromTimer(ch.lastTimerPeriod, channelIndex == 2);
    double amplitude = apuVolumeToAmplitude(ch.lastVolume);
    
    if (freq > 0.0 && amplitude > 0.0) {
        setFMNote(channelIndex, freq, amplitude);
        ch.noteActive = true;
    } else {
        setFMNote(channelIndex, 0.0, 0.0);
        ch.noteActive = false;
    }
}

void AllegroMIDIAudioSystem::setupFMInstruments() {
    if (!fmInitialized) return;
    
    printf("Setting up FM instruments...\n");
    
    setFMInstrument(0, 26);
    setFMInstrument(1, 26);
    setFMInstrument(2, 26);
    setFMInstrument(9, 0);
    
    printf("FM instruments configured\n");
}

void AllegroMIDIAudioSystem::interceptAPURegister(uint16_t address, uint8_t value) {
    if (!useFMMode || !fmInitialized) return;
    
    uint32_t currentTime = getGameTicks();
    
    switch (address) {
        case 0x4000:
            channels[0].lastVolume = value & 0x0F;
            channels[0].lastDuty = (value >> 6) & 3;
            channels[0].lastUpdate = currentTime;
            updateFMChannel(0);
            break;
        case 0x4002:
            channels[0].lastTimerPeriod = (channels[0].lastTimerPeriod & 0xFF00) | value;
            updateFMChannel(0);
            break;
        case 0x4003:
            channels[0].lastTimerPeriod = (channels[0].lastTimerPeriod & 0x00FF) | ((value & 0x07) << 8);
            updateFMChannel(0);
            break;
        case 0x4004:
            channels[1].lastVolume = value & 0x0F;
            channels[1].lastDuty = (value >> 6) & 3;
            updateFMChannel(1);
            break;
        case 0x4006:
            channels[1].lastTimerPeriod = (channels[1].lastTimerPeriod & 0xFF00) | value;
            updateFMChannel(1);
            break;
        case 0x4007:
            channels[1].lastTimerPeriod = (channels[1].lastTimerPeriod & 0x00FF) | ((value & 0x07) << 8);
            updateFMChannel(1);
            break;
        case 0x4008:
            channels[2].lastVolume = (value & 0x80) ? 15 : 0;
            updateFMChannel(2);
            break;
        case 0x400A:
            channels[2].lastTimerPeriod = (channels[2].lastTimerPeriod & 0xFF00) | value;
            updateFMChannel(2);
            break;
        case 0x400B:
            channels[2].lastTimerPeriod = (channels[2].lastTimerPeriod & 0x00FF) | ((value & 0x07) << 8);
            updateFMChannel(2);
            break;
        case 0x400C:
            channels[3].lastVolume = value & 0x0F;
            updateFMChannel(3);
            break;
        case 0x400E:
            channels[3].lastTimerPeriod = value & 0x0F;
            updateFMChannel(3);
            break;
        case 0x4015:
            channels[0].enabled = (value & 0x01) != 0;
            channels[1].enabled = (value & 0x02) != 0;
            channels[2].enabled = (value & 0x04) != 0;
            channels[3].enabled = (value & 0x08) != 0;
            for (int i = 0; i < 4; i++) {
                updateFMChannel(i);
            }
            break;
    }
}

void AllegroMIDIAudioSystem::toggleAudioMode() {
    printf("Toggling audio mode...\n");
    
    if (!fmInitialized) {
        printf("Attempting to initialize FM synthesis...\n");
        if (!initializeFM()) {
            printf("FM synthesis initialization failed - staying in APU mode\n");
            useFMMode = false;
            return;
        }
    }
    
    useFMMode = !useFMMode;
    
    if (useFMMode) {
        setupFMInstruments();
        printf("Switched to FM synthesis mode\n");
    } else {
        for (int i = 0; i < 4; i++) {
            if (channels[i].noteActive) {
                setFMNote(i, 0.0, 0.0);
                channels[i].noteActive = false;
            }
        }
        printf("Switched to classic APU mode\n");
    }
}

bool AllegroMIDIAudioSystem::isFMMode() const {
    return useFMMode && fmInitialized;
}

void AllegroMIDIAudioSystem::generateAudio(uint8_t* buffer, int length) {
    if (useFMMode && fmInitialized) {
        generateFMAudio(buffer, length);
    } else {
        if (originalAPU) {
            originalAPU->output(buffer, length);
        } else {
            memset(buffer, 128, length);
        }
    }
}

void AllegroMIDIAudioSystem::debugPrintChannels() {
    printf("=== Enhanced Audio System Debug ===\n");
    printf("Platform: %s\n", 
    #ifdef __DJGPP__
           "DOS (DJGPP)"
    #else
           "Linux"
    #endif
    );
    printf("Mode: %s\n", (useFMMode && fmInitialized) ? "FM Synthesis" : "APU");
    printf("FM Initialized: %s\n", fmInitialized ? "Yes" : "No");
    
    const char* channelNames[] = {"Pulse1", "Pulse2", "Triangle", "Noise"};
    
    for (int i = 0; i < 4; i++) {
        printf("%s: %s Timer=%d Vol=%d %s", 
               channelNames[i],
               channels[i].enabled ? "ON " : "OFF",
               channels[i].lastTimerPeriod,
               channels[i].lastVolume,
               channels[i].noteActive ? "PLAYING" : "SILENT");
               
        if (useFMMode && fmChannels[i].active) {
            printf(" FM=%.1fHz Amp=%.2f Inst=%d", 
                   fmChannels[i].frequency, 
                   fmChannels[i].amplitude,
                   fmChannels[i].instrumentIndex);
        }
        printf("\n");
    }
    printf("=====================================\n");
}
