#ifdef _WIN32

#include "WindowsAudio.hpp"
#include "SMB/SMBEngine.hpp"
#include <iostream>

WindowsAudio::WindowsAudio()
    : hWaveOut(nullptr), currentBuffer(0), isInitialized(false), 
      isPlaying(false), smbEngine(nullptr), sampleRate(44100)
{
    for (int i = 0; i < BUFFER_COUNT; i++) {
        audioBuffers[i] = nullptr;
        memset(&waveHeaders[i], 0, sizeof(WAVEHDR));
    }
}

WindowsAudio::~WindowsAudio()
{
    shutdown();
}

bool WindowsAudio::initialize(int frequency, SMBEngine* engine)
{
    if (isInitialized) {
        shutdown();
    }
    
    smbEngine = engine;
    sampleRate = frequency;
    
    // Set up wave format
    WAVEFORMATEX waveFormat;
    waveFormat.wFormatTag = WAVE_FORMAT_PCM;
    waveFormat.nChannels = 1; // Mono
    waveFormat.nSamplesPerSec = frequency;
    waveFormat.wBitsPerSample = 16;
    waveFormat.nBlockAlign = waveFormat.nChannels * waveFormat.wBitsPerSample / 8;
    waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
    waveFormat.cbSize = 0;
    
    // Open wave output device
    MMRESULT result = waveOutOpen(&hWaveOut, WAVE_MAPPER, &waveFormat, 
                                  (DWORD_PTR)waveOutProc, (DWORD_PTR)this, 
                                  CALLBACK_FUNCTION);
    
    if (result != MMSYSERR_NOERROR) {
        std::cout << "Failed to open wave output device. Error: " << result << std::endl;
        return false;
    }
    
    // Allocate and prepare buffers
    for (int i = 0; i < BUFFER_COUNT; i++) {
        audioBuffers[i] = new int16_t[BUFFER_SIZE];
        memset(audioBuffers[i], 0, BUFFER_SIZE * sizeof(int16_t));
        
        waveHeaders[i].lpData = (LPSTR)audioBuffers[i];
        waveHeaders[i].dwBufferLength = BUFFER_SIZE * sizeof(int16_t);
        waveHeaders[i].dwFlags = 0;
        waveHeaders[i].dwLoops = 0;
        
        result = waveOutPrepareHeader(hWaveOut, &waveHeaders[i], sizeof(WAVEHDR));
        if (result != MMSYSERR_NOERROR) {
            std::cout << "Failed to prepare wave header " << i << ". Error: " << result << std::endl;
            shutdown();
            return false;
        }
    }
    
    isInitialized = true;
    std::cout << "WinMM audio initialized successfully" << std::endl;
    std::cout << "  Format: 16-bit mono" << std::endl;
    std::cout << "  Frequency: " << frequency << " Hz" << std::endl;
    std::cout << "  Buffer size: " << BUFFER_SIZE << " samples" << std::endl;
    
    return true;
}

void WindowsAudio::shutdown()
{
    if (!isInitialized) return;
    
    stop();
    
    if (hWaveOut) {
        waveOutReset(hWaveOut);
        
        // Unprepare and free buffers
        for (int i = 0; i < BUFFER_COUNT; i++) {
            if (waveHeaders[i].dwFlags & WHDR_PREPARED) {
                waveOutUnprepareHeader(hWaveOut, &waveHeaders[i], sizeof(WAVEHDR));
            }
            delete[] audioBuffers[i];
            audioBuffers[i] = nullptr;
        }
        
        waveOutClose(hWaveOut);
        hWaveOut = nullptr;
    }
    
    isInitialized = false;
}

void WindowsAudio::start()
{
    if (!isInitialized || isPlaying) return;
    
    // Queue all buffers initially
    for (int i = 0; i < BUFFER_COUNT; i++) {
        fillBuffer(i);
        waveOutWrite(hWaveOut, &waveHeaders[i], sizeof(WAVEHDR));
    }
    
    isPlaying = true;
}

void WindowsAudio::stop()
{
    if (!isPlaying) return;
    
    isPlaying = false;
    
    if (hWaveOut) {
        waveOutReset(hWaveOut);
    }
}

void CALLBACK WindowsAudio::waveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance,
                                        DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    if (uMsg == WOM_DONE) {
        WindowsAudio* audio = (WindowsAudio*)dwInstance;
        WAVEHDR* waveHeader = (WAVEHDR*)dwParam1;
        
        if (audio->isPlaying) {
            // Find which buffer completed
            for (int i = 0; i < BUFFER_COUNT; i++) {
                if (&audio->waveHeaders[i] == waveHeader) {
                    audio->fillBuffer(i);
                    waveOutWrite(audio->hWaveOut, &audio->waveHeaders[i], sizeof(WAVEHDR));
                    break;
                }
            }
        }
    }
}

void WindowsAudio::fillBuffer(int bufferIndex)
{
    if (!smbEngine) return;
    
    // Get audio data from the APU in 16-bit format
    uint8_t* byteBuffer = (uint8_t*)audioBuffers[bufferIndex];
    int byteLength = BUFFER_SIZE * sizeof(int16_t);
    
    // Call the engine's audio callback
    smbEngine->audioCallback(byteBuffer, byteLength);
}

#endif // _WIN32
