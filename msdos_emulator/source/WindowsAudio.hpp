// WindowsAudio.hpp - WinMM-based audio for Windows
#ifndef WINDOWS_AUDIO_HPP
#define WINDOWS_AUDIO_HPP

#ifdef _WIN32

#include <windows.h>
#include <mmsystem.h>
#include <cstdint>

#pragma comment(lib, "winmm.lib")

class SMBEngine; // Forward declaration

class WindowsAudio
{
public:
    WindowsAudio();
    ~WindowsAudio();
    
    bool initialize(int frequency, SMBEngine* engine);
    void shutdown();
    void start();
    void stop();
    
private:
    static const int BUFFER_COUNT = 4;
    static const int BUFFER_SIZE = 2048; // samples per buffer
    
    HWAVEOUT hWaveOut;
    WAVEHDR waveHeaders[BUFFER_COUNT];
    int16_t* audioBuffers[BUFFER_COUNT];
    int currentBuffer;
    bool isInitialized;
    bool isPlaying;
    SMBEngine* smbEngine;
    int sampleRate;
    
    static void CALLBACK waveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, 
                                     DWORD_PTR dwParam1, DWORD_PTR dwParam2);
    void fillBuffer(int bufferIndex);
};

#endif // _WIN32

#endif // WINDOWS_AUDIO_HPP
