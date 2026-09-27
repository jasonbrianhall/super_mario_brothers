#pragma once
#include <stdint.h>

enum AudioDriver { AUDIO_NONE, AUDIO_SB16, AUDIO_AC97 };

// Detects AC97, then SB16. The boot command line can force one:
// audio=ac97, audio=sb16 or audio=off.
AudioDriver audio_init(const char* cmdline);
uint32_t audio_play_pos();
void audio_submit(const uint8_t* samples, int n);   // the APU's 8-bit mix levels, mono
const char* audio_name();
