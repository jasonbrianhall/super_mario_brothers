#pragma once
#include <stdint.h>

enum AudioDriver { AUDIO_NONE, AUDIO_HDA, AUDIO_AC97 };

// Detects Intel HD Audio, then AC97. The boot command line can force one:
// audio=hda, audio=ac97 or audio=off.
AudioDriver audio_init(const char* cmdline);
uint32_t audio_play_pos();
void audio_submit(const uint8_t* samples, int n);   // the APU's 8-bit mix levels, mono
const char* audio_name();
