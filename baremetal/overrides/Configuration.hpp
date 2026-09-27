#pragma once
// Bare-metal stand-in for the boost/INI-backed Configuration class.
class Configuration {
public:
    static bool getAudioEnabled() { return false; }
};
