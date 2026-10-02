#pragma once
#include <stdint.h>

enum AudioDriver { AUDIO_NONE, AUDIO_HDA, AUDIO_AC97, AUDIO_SB };

// Detects Intel HD Audio, then AC97, then a Sound Blaster on the ISA bus
// (SB16 in 16-bit, SB Pro / 2.0 in 8-bit). The boot command line can force
// one: audio=hda, audio=ac97, audio=sb or audio=off; sb=220,1,5 gives the
// Sound Blaster's port, 8-bit and 16-bit DMA channels (the defaults;
// BLASTER's A220 D1 H5).
AudioDriver audio_init(const char* cmdline);
uint32_t audio_play_pos();
void audio_submit(const uint8_t* samples, int n);   // the APU's 8-bit mix levels, mono
void audio_silence(int n);                          // n samples of silence (paused): fades, no click
void audio_set_volume(int level);                   // 0 (mute) .. AUDIO_VOLUME_MAX
constexpr int AUDIO_VOLUME_MAX = 10;
const char* audio_name();
uint32_t audio_delay_ms();                          // queued ahead of the speaker right now
