// AllegroMIDIAudioSystem.cpp - Implementation
#include "AllegroMidi.hpp"
#include "APU.hpp"
#include <cmath>
#include <cstdio>
#include <ctime>
#include <cstring>

#ifdef __DJGPP__
#include <pc.h>
#include <dos.h>
#endif

AllegroMIDIAudioSystem::AllegroMIDIAudioSystem(APU* apu) 
    : originalAPU(apu), useMIDIMode(false), midiInitialized(false) {
    
    // Initialize channels
    for (int i = 0; i < 4; i++) {
        channels[i] = {};
        channels[i].midiChannel = i;  // Use MIDI channels 0-3
    }
    
    // CRITICAL: Don't auto-enable MIDI mode during construction
    printf("Enhanced audio system created (MIDI mode disabled by default)\n");
}

AllegroMIDIAudioSystem::~AllegroMIDIAudioSystem() {
    // Turn off any active MIDI notes
    if (midiInitialized) {
        for (int i = 0; i < 4; i++) {
            if (channels[i].noteActive) {
                sendMIDINoteOff(channels[i].midiChannel, channels[i].currentMidiNote);
            }
        }
        
        // Both DOS and Linux use the same cleanup approach
        printf("MIDI cleanup completed\n");
        midiInitialized = false;
    }
}

bool AllegroMIDIAudioSystem::initializeMIDI() {
    if (midiInitialized) {
        printf("MIDI already initialized\n");
        return true;
    }
    
    // Both DOS and Linux Allegro versions assume MIDI is available 
    // if sound system is properly initialized
    #ifdef __DJGPP__
    printf("DOS MIDI: Assuming available through sound system\n");
    #else
    printf("Linux MIDI: Assuming available through sound system\n");
    #endif
    
    midiInitialized = true;
    printf("MIDI initialization completed\n");
    return true;
}

uint32_t AllegroMIDIAudioSystem::getGameTicks() {
    #ifdef __DJGPP__
    // DOS - simple tick counter
    static uint32_t ticks = 0;
    return ++ticks;
    #else
    // Linux - use clock
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
    if (freq <= 8.0) return 0;  // Below MIDI range
    
    // MIDI note formula: note = 69 + 12 * log2(freq/440)
    double noteFloat = 69.0 + 12.0 * log2(freq / 440.0);
    int note = (int)round(noteFloat);
    
    return (note < 0) ? 0 : (note > 127) ? 127 : (uint8_t)note;
}

uint8_t AllegroMIDIAudioSystem::apuVolumeToVelocity(uint8_t apuVol) {
    if (apuVol == 0) return 0;
    // Scale 0-15 to 40-127 (avoid very quiet notes)
    return 40 + ((apuVol * 87) / 15);
}

void AllegroMIDIAudioSystem::sendMIDINoteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
    if (!midiInitialized) return;
    
    // Both DOS and Linux use the same buffer-based MIDI API
    unsigned char midiData[3];
    midiData[0] = 0x90 | channel;  // Note On
    midiData[1] = note;
    midiData[2] = velocity;
    midi_out(midiData, 3);
    
    #ifdef __DJGPP__
    printf("DOS MIDI Note On: Ch%d Note%d Vel%d\n", channel, note, velocity);
    #else
    printf("Linux MIDI Note On: Ch%d Note%d Vel%d\n", channel, note, velocity);
    #endif
}

void AllegroMIDIAudioSystem::sendMIDINoteOff(uint8_t channel, uint8_t note) {
    if (!midiInitialized) return;
    
    // Both DOS and Linux use the same buffer-based MIDI API
    unsigned char midiData[3];
    midiData[0] = 0x80 | channel;  // Note Off
    midiData[1] = note;
    midiData[2] = 0;
    midi_out(midiData, 3);
    
    #ifdef __DJGPP__
    printf("DOS MIDI Note Off: Ch%d Note%d\n", channel, note);
    #else
    printf("Linux MIDI Note Off: Ch%d Note%d\n", channel, note);
    #endif
}

void AllegroMIDIAudioSystem::sendMIDIProgramChange(uint8_t channel, uint8_t instrument) {
    if (!midiInitialized) return;
    
    // Both DOS and Linux use the same buffer-based MIDI API
    unsigned char midiData[2];
    midiData[0] = 0xC0 | channel;  // Program Change
    midiData[1] = instrument;
    midi_out(midiData, 2);
    
    #ifdef __DJGPP__
    printf("DOS MIDI Program Change: Ch%d Instrument%d\n", channel, instrument);
    #else
    printf("Linux MIDI Program Change: Ch%d Instrument%d\n", channel, instrument);
    #endif
}

void AllegroMIDIAudioSystem::updateMIDIChannel(int channelIndex) {
    GameChannel& ch = channels[channelIndex];
    
    if (!ch.enabled || !midiInitialized) {
        if (ch.noteActive) {
            sendMIDINoteOff(ch.midiChannel, ch.currentMidiNote);
            ch.noteActive = false;
        }
        return;
    }
    
    double freq = getFrequencyFromTimer(ch.lastTimerPeriod, channelIndex == 2);
    uint8_t midiNote = frequencyToMIDI(freq);
    uint8_t velocity = apuVolumeToVelocity(ch.lastVolume);
    
    // Only update if note changed significantly
    if (midiNote != ch.currentMidiNote && velocity > 0) {
        // Turn off previous note
        if (ch.noteActive) {
            sendMIDINoteOff(ch.midiChannel, ch.currentMidiNote);
        }
        
        // Play new note
        if (midiNote > 0) {
            sendMIDINoteOn(ch.midiChannel, midiNote, velocity);
            ch.noteActive = true;
            ch.currentMidiNote = midiNote;
        } else {
            ch.noteActive = false;
        }
    }
}

void AllegroMIDIAudioSystem::setupGameInstruments() {
    if (!midiInitialized) return;
    
    printf("Setting up MIDI instruments...\n");
    
    // Set up General MIDI instruments for each channel
    sendMIDIProgramChange(0, 80);  // Lead 1 (square) for Pulse 1
    sendMIDIProgramChange(1, 81);  // Lead 2 (sawtooth) for Pulse 2  
    sendMIDIProgramChange(2, 38);  // Synth Bass 1 for Triangle
    sendMIDIProgramChange(3, 125); // Reverse Cymbal for Noise
}

void AllegroMIDIAudioSystem::interceptAPURegister(uint16_t address, uint8_t value) {
    if (!useMIDIMode || !midiInitialized) return;
    
    uint32_t currentTime = getGameTicks();
    
    switch (address) {
        // Pulse 1
        case 0x4000:
            channels[0].lastVolume = value & 0x0F;
            channels[0].lastDuty = (value >> 6) & 3;
            channels[0].lastUpdate = currentTime;
            updateMIDIChannel(0);
            break;
            
        case 0x4002:
            channels[0].lastTimerPeriod = (channels[0].lastTimerPeriod & 0xFF00) | value;
            updateMIDIChannel(0);
            break;
            
        case 0x4003:
            channels[0].lastTimerPeriod = (channels[0].lastTimerPeriod & 0x00FF) | ((value & 0x07) << 8);
            updateMIDIChannel(0);
            break;
        
        // Pulse 2
        case 0x4004:
            channels[1].lastVolume = value & 0x0F;
            channels[1].lastDuty = (value >> 6) & 3;
            updateMIDIChannel(1);
            break;
            
        case 0x4006:
            channels[1].lastTimerPeriod = (channels[1].lastTimerPeriod & 0xFF00) | value;
            updateMIDIChannel(1);
            break;
            
        case 0x4007:
            channels[1].lastTimerPeriod = (channels[1].lastTimerPeriod & 0x00FF) | ((value & 0x07) << 8);
            updateMIDIChannel(1);
            break;
        
        // Triangle
        case 0x4008:
            channels[2].lastVolume = (value & 0x80) ? 15 : 0;
            updateMIDIChannel(2);
            break;
            
        case 0x400A:
            channels[2].lastTimerPeriod = (channels[2].lastTimerPeriod & 0xFF00) | value;
            updateMIDIChannel(2);
            break;
            
        case 0x400B:
            channels[2].lastTimerPeriod = (channels[2].lastTimerPeriod & 0x00FF) | ((value & 0x07) << 8);
            updateMIDIChannel(2);
            break;
        
        // Noise channel
        case 0x400C:
            channels[3].lastVolume = value & 0x0F;
            updateMIDIChannel(3);
            break;
            
        case 0x400E:
            // Map noise periods to different notes/percussion
            channels[3].lastTimerPeriod = value & 0x0F;
            updateMIDIChannel(3);
            break;
        
        // Channel enables
        case 0x4015:
            channels[0].enabled = (value & 0x01) != 0;
            channels[1].enabled = (value & 0x02) != 0;
            channels[2].enabled = (value & 0x04) != 0;
            channels[3].enabled = (value & 0x08) != 0;
            
            for (int i = 0; i < 4; i++) {
                updateMIDIChannel(i);
            }
            break;
    }
}

void AllegroMIDIAudioSystem::toggleAudioMode() {
    printf("Toggling audio mode...\n");
    
    if (!midiInitialized) {
        printf("Attempting to initialize MIDI...\n");
        if (!initializeMIDI()) {
            printf("MIDI initialization failed - staying in APU mode\n");
            useMIDIMode = false;
            return;
        }
    }
    
    useMIDIMode = !useMIDIMode;
    
    if (useMIDIMode) {
        setupGameInstruments();
        #ifdef __DJGPP__
        printf("Switched to DOS MIDI mode\n");
        #else
        printf("Switched to Linux MIDI mode\n");
        #endif
    } else {
        // Turn off all MIDI notes
        for (int i = 0; i < 4; i++) {
            if (channels[i].noteActive) {
                sendMIDINoteOff(channels[i].midiChannel, channels[i].currentMidiNote);
                channels[i].noteActive = false;
            }
        }
        printf("Switched to classic APU mode\n");
    }
}

bool AllegroMIDIAudioSystem::isMIDIMode() const {
    return useMIDIMode && midiInitialized;
}

void AllegroMIDIAudioSystem::generateAudio(uint8_t* buffer, int length) {
    if (useMIDIMode && midiInitialized) {
        // MIDI audio is handled by the MIDI subsystem
        // Clear the buffer since MIDI goes directly to MIDI devices
        memset(buffer, 128, length); // 128 = silence for unsigned 8-bit
    } else {
        // Use original APU
        if (originalAPU) {
            originalAPU->output(buffer, length);
        } else {
            // Fallback: fill with silence
            memset(buffer, 128, length);
        }
    }
}

void AllegroMIDIAudioSystem::debugPrintChannels() {
    printf("Enhanced Audio System Status:\n");
    printf("Platform: %s\n", 
    #ifdef __DJGPP__
           "DOS"
    #else
           "Linux"
    #endif
    );
    printf("Mode: %s\n", (useMIDIMode && midiInitialized) ? "MIDI" : "APU");
    printf("MIDI Initialized: %s\n", midiInitialized ? "Yes" : "No");
    
    const char* channelNames[] = {"Pulse1", "Pulse2", "Triangle", "Noise"};
    
    for (int i = 0; i < 4; i++) {
        printf("%s: %s Timer=%d Vol=%d Note=%d %s\n", 
               channelNames[i],
               channels[i].enabled ? "ON " : "OFF",
               channels[i].lastTimerPeriod,
               channels[i].lastVolume,
               channels[i].currentMidiNote,
               channels[i].noteActive ? "PLAYING" : "SILENT");
    }
}
