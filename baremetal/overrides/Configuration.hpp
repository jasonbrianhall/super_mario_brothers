#pragma once
// Bare-metal stand-in for the boost/INI-backed Configuration class.
// The audio driver fills these in at boot.
class Configuration {
public:
    static bool audioEnabled;
    static int audioFrequency;
    static bool getAudioEnabled() { return audioEnabled; }
    static int getAudioFrequency() { return audioFrequency; }
    static int getFrameRate() { return 60; }
};
