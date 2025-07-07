// AllegroMIDIAudioSystem.cpp - True NES-style synthesis
#include "AllegroMidi.hpp"
#include "APU.hpp"
#include <cmath>
#include <cstdio>
#include <ctime>
#include <cstring>

AllegroMIDIAudioSystem::AllegroMIDIAudioSystem(APU* apu) 
    : originalAPU(apu), useFMMode(false), fmInitialized(false) {
    
    // Initialize channels
    for (int i = 0; i < 4; i++) {
        channels[i] = {};
        
        // Initialize NES-style oscillators
        fmChannels[i].phase1 = 0.0;
        fmChannels[i].phase2 = 0.0;
        fmChannels[i].frequency = 440.0;
        fmChannels[i].amplitude = 0.0;
        fmChannels[i].instrumentIndex = 80;
        fmChannels[i].active = false;
        fmChannels[i].dutyFactor = 0.5; // 50% duty cycle by default
        fmChannels[i].noiseShift = 1;   // For noise channel
        
        // Initialize real NES hardware features
        fmChannels[i].sweepEnabled = false;
        fmChannels[i].sweepNegate = false;
        fmChannels[i].sweepShift = 0;
        fmChannels[i].sweepPeriod = 0;
        fmChannels[i].sweepCounter = 0;
        fmChannels[i].sweepReload = false;
        
        fmChannels[i].envelopeEnabled = false;
        fmChannels[i].envelopeVolume = 0;
        fmChannels[i].envelopePeriod = 0;
        fmChannels[i].envelopeCounter = 0;
        fmChannels[i].envelopeStart = false;
        fmChannels[i].envelopeLoop = false;
        fmChannels[i].constantVolume = 0;
        
        fmChannels[i].lengthCounter = 0;
        fmChannels[i].lengthEnabled = true;
        fmChannels[i].timerPeriod = 0;
    }
    
    // Removed debugging output
}

AllegroMIDIAudioSystem::~AllegroMIDIAudioSystem() {
    if (fmInitialized) {
        for (int i = 0; i < 4; i++) {
            fmChannels[i].active = false;
        }
        fmInitialized = false;
    }
}

bool AllegroMIDIAudioSystem::initializeFM() {
    if (fmInitialized) {
        return true;
    }
    
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
    return (apuVol / 15.0) * 0.4; // Moderate volume increase without distortion
}

// Generate NES waveforms with proper hardware emulation
double AllegroMIDIAudioSystem::generateNESWave(int channelIndex, double sampleRate) {
    FMChannel& ch = fmChannels[channelIndex];
    if (!ch.active || ch.lengthCounter == 0) return 0.0;
    
    // Get current volume from envelope or constant volume
    double currentVolume = 0.0;
    if (ch.envelopeEnabled) {
        currentVolume = ch.envelopeVolume / 15.0;
    } else {
        currentVolume = ch.constantVolume / 15.0;
    }
    
    if (currentVolume <= 0.0 || ch.frequency <= 0.0) return 0.0;
    
    double output = 0.0;
    
    switch (channelIndex) {
        case 0: // Pulse 1 - Clean square wave
        case 1: // Pulse 2 - Clean square wave
        {
            // Update phase
            ch.phase1 += ch.frequency / sampleRate;
            while (ch.phase1 >= 1.0) ch.phase1 -= 1.0;
            
            // Generate clean square wave
            output = (ch.phase1 < ch.dutyFactor) ? currentVolume : -currentVolume;
            break;
        }
        
        case 2: // Triangle - Clean triangle wave
        {
            // Update phase
            ch.phase1 += ch.frequency / sampleRate;
            while (ch.phase1 >= 1.0) ch.phase1 -= 1.0;
            
            // Generate clean triangle wave
            if (ch.phase1 < 0.25) {
                output = ch.phase1 * 4.0 * currentVolume; // 0 to +amp
            } else if (ch.phase1 < 0.75) {
                output = (2.0 - ch.phase1 * 4.0) * currentVolume; // +amp to -amp
            } else {
                output = (ch.phase1 * 4.0 - 4.0) * currentVolume; // -amp to 0
            }
            break;
        }
        
        case 3: // Noise
        {
            static double noisePhase = 0.0;
            static double noiseValue = 1.0;
            
            noisePhase += ch.frequency / sampleRate;
            if (noisePhase >= 1.0) {
                noisePhase -= 1.0;
                noiseValue = (fmod(noiseValue * 16807.0, 2147483647.0) > 1073741823.0) ? 1.0 : -1.0;
            }
            
            output = noiseValue * currentVolume;
            break;
        }
    }
    
    return output * 0.4; // Scale to prevent clipping
}

void AllegroMIDIAudioSystem::setNESNote(int channelIndex, double frequency, double amplitude, uint8_t duty) {
    FMChannel& ch = fmChannels[channelIndex];
    
    if (amplitude > 0.0 && frequency > 0.0) {
        ch.frequency = frequency;
        ch.amplitude = amplitude;
        ch.active = true;
        
        // Set duty cycle for pulse channels (matches NES duty settings)
        switch (duty) {
            case 0: ch.dutyFactor = 0.125; break; // 12.5%
            case 1: ch.dutyFactor = 0.25;  break; // 25%
            case 2: ch.dutyFactor = 0.5;   break; // 50%
            case 3: ch.dutyFactor = 0.75;  break; // 75%
            default: ch.dutyFactor = 0.5;  break;
        }
        
        // Reset phases for clean start
        ch.phase1 = 0.0;
        ch.phase2 = 0.0;
        
    } else {
        ch.active = false;
        ch.amplitude = 0.0;
    }
}

void AllegroMIDIAudioSystem::setFMInstrument(int channelIndex, uint8_t instrument) {
    // For NES-style synthesis, instrument just affects timbre slightly
    fmChannels[channelIndex].instrumentIndex = instrument;
    // Removed debugging output
}

void AllegroMIDIAudioSystem::generateNESAudio(uint8_t* buffer, int length) {
    const double sampleRate = 22050.0;
    
    for (int i = 0; i < length; i++) {
        double mixedSample = 0.0;
        
        // Generate and mix each channel cleanly
        for (int ch = 0; ch < 4; ch++) {
            if (fmChannels[ch].active) {
                mixedSample += generateNESWave(ch, sampleRate) * 0.25; // Back to conservative mixing
            }
        }
        
        // Conservative mixing to avoid static
        // Convert to 8-bit unsigned (128 = silence)
        int sample = (int)(mixedSample * 80.0) + 128; // Moderate gain increase
        
        // Clamp to valid range
        if (sample < 0) sample = 0;
        if (sample > 255) sample = 255;
        
        buffer[i] = (uint8_t)sample;
    }
}

void AllegroMIDIAudioSystem::updateNESChannel(int channelIndex) {
    GameChannel& ch = channels[channelIndex];
    
    if (!ch.enabled || !fmInitialized) {
        if (ch.noteActive) {
            setNESNote(channelIndex, 0.0, 0.0, 0);
            ch.noteActive = false;
        }
        return;
    }
    
    double freq = getFrequencyFromTimer(ch.lastTimerPeriod, channelIndex == 2);
    double amplitude = apuVolumeToAmplitude(ch.lastVolume);
    
    if (freq > 0.0 && amplitude > 0.0) {
        setNESNote(channelIndex, freq, amplitude, ch.lastDuty);
        ch.noteActive = true;
    } else {
        setNESNote(channelIndex, 0.0, 0.0, 0);
        ch.noteActive = false;
    }
}

void AllegroMIDIAudioSystem::setupFMInstruments() {
    if (!fmInitialized) return;
    
    // Removed debugging output
    
    // For NES-style, we don't really need different "instruments"
    // But we can set different characteristics per channel
    setFMInstrument(0, 0); // Pulse 1
    setFMInstrument(1, 1); // Pulse 2  
    setFMInstrument(2, 2); // Triangle
    setFMInstrument(3, 3); // Noise
    
    // Removed debugging output
}

void AllegroMIDIAudioSystem::interceptAPURegister(uint16_t address, uint8_t value) {
    if (!useFMMode || !fmInitialized) return;
    
    uint32_t currentTime = getGameTicks();
    
    switch (address) {
        case 0x4000: // Pulse 1 control
            channels[0].lastVolume = value & 0x0F;
            channels[0].lastDuty = (value >> 6) & 3;
            channels[0].lastUpdate = currentTime;
            
            // Set real NES hardware parameters
            fmChannels[0].envelopeEnabled = ((value >> 4) & 1) == 0;
            fmChannels[0].envelopeLoop = ((value >> 5) & 1) == 1;
            fmChannels[0].lengthEnabled = ((value >> 5) & 1) == 0;
            fmChannels[0].envelopePeriod = value & 0x0F;
            fmChannels[0].constantVolume = value & 0x0F;
            fmChannels[0].envelopeStart = true;
            
            updateNESChannel(0);
            break;
            
        case 0x4001: // Pulse 1 sweep
            fmChannels[0].sweepEnabled = ((value >> 7) & 1) == 1;
            fmChannels[0].sweepPeriod = ((value >> 4) & 7) + 1;
            fmChannels[0].sweepNegate = ((value >> 3) & 1) == 1;
            fmChannels[0].sweepShift = value & 7;
            fmChannels[0].sweepReload = true;
            break;
            
        case 0x4002:
            channels[0].lastTimerPeriod = (channels[0].lastTimerPeriod & 0xFF00) | value;
            fmChannels[0].timerPeriod = channels[0].lastTimerPeriod;
            updateNESChannel(0);
            break;
        case 0x4003:
            channels[0].lastTimerPeriod = (channels[0].lastTimerPeriod & 0x00FF) | ((value & 0x07) << 8);
            fmChannels[0].timerPeriod = channels[0].lastTimerPeriod;
            fmChannels[0].lengthCounter = (value >> 3) & 0x1F; // Load length counter
            fmChannels[0].envelopeStart = true;
            updateNESChannel(0);
            break;
            
        case 0x4004: // Pulse 2 control (similar to pulse 1)
            channels[1].lastVolume = value & 0x0F;
            channels[1].lastDuty = (value >> 6) & 3;
            
            fmChannels[1].envelopeEnabled = ((value >> 4) & 1) == 0;
            fmChannels[1].envelopeLoop = ((value >> 5) & 1) == 1;
            fmChannels[1].lengthEnabled = ((value >> 5) & 1) == 0;
            fmChannels[1].envelopePeriod = value & 0x0F;
            fmChannels[1].constantVolume = value & 0x0F;
            fmChannels[1].envelopeStart = true;
            
            updateNESChannel(1);
            break;
            
        case 0x4005: // Pulse 2 sweep
            fmChannels[1].sweepEnabled = ((value >> 7) & 1) == 1;
            fmChannels[1].sweepPeriod = ((value >> 4) & 7) + 1;
            fmChannels[1].sweepNegate = ((value >> 3) & 1) == 1;
            fmChannels[1].sweepShift = value & 7;
            fmChannels[1].sweepReload = true;
            break;
            
        case 0x4006:
            channels[1].lastTimerPeriod = (channels[1].lastTimerPeriod & 0xFF00) | value;
            fmChannels[1].timerPeriod = channels[1].lastTimerPeriod;
            updateNESChannel(1);
            break;
        case 0x4007:
            channels[1].lastTimerPeriod = (channels[1].lastTimerPeriod & 0x00FF) | ((value & 0x07) << 8);
            fmChannels[1].timerPeriod = channels[1].lastTimerPeriod;
            fmChannels[1].lengthCounter = (value >> 3) & 0x1F;
            fmChannels[1].envelopeStart = true;
            updateNESChannel(1);
            break;
            
        case 0x4008: // Triangle control
            channels[2].lastVolume = (value & 0x80) ? 15 : 0;
            fmChannels[2].lengthEnabled = ((value >> 7) & 1) == 0;
            updateNESChannel(2);
            break;
        case 0x400A:
            channels[2].lastTimerPeriod = (channels[2].lastTimerPeriod & 0xFF00) | value;
            updateNESChannel(2);
            break;
        case 0x400B:
            channels[2].lastTimerPeriod = (channels[2].lastTimerPeriod & 0x00FF) | ((value & 0x07) << 8);
            fmChannels[2].lengthCounter = (value >> 3) & 0x1F;
            updateNESChannel(2);
            break;
            
        case 0x400C: // Noise control
            channels[3].lastVolume = value & 0x0F;
            
            fmChannels[3].envelopeEnabled = ((value >> 4) & 1) == 0;
            fmChannels[3].envelopeLoop = ((value >> 5) & 1) == 1;
            fmChannels[3].lengthEnabled = ((value >> 5) & 1) == 0;
            fmChannels[3].envelopePeriod = value & 0x0F;
            fmChannels[3].constantVolume = value & 0x0F;
            fmChannels[3].envelopeStart = true;
            
            updateNESChannel(3);
            break;
        case 0x400E:
            channels[3].lastTimerPeriod = value & 0x0F;
            updateNESChannel(3);
            break;
        case 0x400F:
            fmChannels[3].lengthCounter = (value >> 3) & 0x1F;
            fmChannels[3].envelopeStart = true;
            updateNESChannel(3);
            break;
            
        case 0x4015:
            channels[0].enabled = (value & 0x01) != 0;
            channels[1].enabled = (value & 0x02) != 0;
            channels[2].enabled = (value & 0x04) != 0;
            channels[3].enabled = (value & 0x08) != 0;
            
            // If channel disabled, clear length counter
            if (!channels[0].enabled) fmChannels[0].lengthCounter = 0;
            if (!channels[1].enabled) fmChannels[1].lengthCounter = 0;
            if (!channels[2].enabled) fmChannels[2].lengthCounter = 0;
            if (!channels[3].enabled) fmChannels[3].lengthCounter = 0;
            
            for (int i = 0; i < 4; i++) {
                updateNESChannel(i);
            }
            break;
    }
}

void AllegroMIDIAudioSystem::toggleAudioMode() {
    useFMMode = !useFMMode;
    
    if (!fmInitialized && useFMMode) {
        initializeFM();
    }
    
    if (useFMMode) {
        setupFMInstruments();
    } else {
        for (int i = 0; i < 4; i++) {
            if (channels[i].noteActive) {
                setNESNote(i, 0.0, 0.0, 0);
                channels[i].noteActive = false;
            }
        }
    }
}

bool AllegroMIDIAudioSystem::isFMMode() const {
    return useFMMode && fmInitialized;
}

void AllegroMIDIAudioSystem::generateAudio(uint8_t* buffer, int length) {
    if (useFMMode && fmInitialized) {
        generateNESAudio(buffer, length);
    } else {
        if (originalAPU) {
            originalAPU->output(buffer, length);
        } else {
            memset(buffer, 128, length);
        }
    }
}

void AllegroMIDIAudioSystem::debugPrintChannels() {
    printf("=== Enhanced NES-Style Audio System Debug ===\n");
    printf("Platform: %s\n", 
    #ifdef __DJGPP__
           "DOS (DJGPP)"
    #else
           "Linux"
    #endif
    );
    printf("Mode: %s\n", (useFMMode && fmInitialized) ? "NES-Style Synthesis" : "APU");
    printf("NES Synthesis Initialized: %s\n", fmInitialized ? "Yes" : "No");
    
    const char* channelNames[] = {"Pulse1", "Pulse2", "Triangle", "Noise"};
    const char* waveTypes[] = {"Square", "Square", "Triangle", "Noise"};
    
    for (int i = 0; i < 4; i++) {
        printf("%s (%s): %s Timer=%d Vol=%d %s", 
               channelNames[i], waveTypes[i],
               channels[i].enabled ? "ON " : "OFF",
               channels[i].lastTimerPeriod,
               channels[i].lastVolume,
               channels[i].noteActive ? "PLAYING" : "SILENT");
               
        if (useFMMode && fmChannels[i].active) {
            if (i < 2) {
                printf(" Duty=%.0f%% %.1fHz Amp=%.2f", 
                       fmChannels[i].dutyFactor * 100,
                       fmChannels[i].frequency, 
                       fmChannels[i].amplitude);
            } else {
                printf(" %.1fHz Amp=%.2f", 
                       fmChannels[i].frequency, 
                       fmChannels[i].amplitude);
            }
        }
        printf("\n");
    }
    printf("============================================\n");
}

// Legacy compatibility methods (kept for compilation)
double AllegroMIDIAudioSystem::generateFMSample(int channelIndex, double sampleRate) {
    return generateNESWave(channelIndex, sampleRate);
}

void AllegroMIDIAudioSystem::setFMNote(int channelIndex, double frequency, double amplitude) {
    setNESNote(channelIndex, frequency, amplitude, 2); // Default 50% duty
}

void AllegroMIDIAudioSystem::generateFMAudio(uint8_t* buffer, int length) {
    generateNESAudio(buffer, length);
}

void AllegroMIDIAudioSystem::updateFMChannel(int channelIndex) {
    updateNESChannel(channelIndex);
}
