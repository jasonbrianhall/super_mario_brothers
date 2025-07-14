#include <iostream>
#include <cstdint>

#include "GameManager.hpp"
#include "SMBEngine.hpp"
#include "SMBEmulator.hpp"
#include "../SMBRom.hpp"
#include "../Emulation/Controller.hpp"

GameManager::GameManager() 
    : staticEngine(nullptr), dynamicEngine(nullptr), currentEngine(ENGINE_STATIC)
{
    // Initialize both engines
    initializeStaticEngine();
    initializeDynamicEngine();
}

GameManager::~GameManager()
{
    delete staticEngine;
    delete dynamicEngine;
}

bool GameManager::initializeStaticEngine()
{
    if (staticEngine) {
        delete staticEngine;
    }
    
    try {
        // Use embedded ROM data
        staticEngine = new SMBEngine(const_cast<unsigned char*>(smbRomData));
        staticEngine->reset();
        std::cout << "Static engine (SMBEngine) initialized successfully" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize static engine: " << e.what() << std::endl;
        staticEngine = nullptr;
        return false;
    }
}

bool GameManager::initializeDynamicEngine()
{
    if (dynamicEngine) {
        delete dynamicEngine;
    }
    
    try {
        dynamicEngine = new SMBEmulator();
        std::cout << "Dynamic engine (SMBEmulator) initialized successfully" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize dynamic engine: " << e.what() << std::endl;
        dynamicEngine = nullptr;
        return false;
    }
}

bool GameManager::loadROM(const std::string& filename)
{
    if (!dynamicEngine) {
        std::cerr << "Dynamic engine not initialized" << std::endl;
        return false;
    }
    
    if (dynamicEngine->loadROM(filename)) {
        currentROMPath = filename;
        std::cout << "ROM loaded successfully: " << filename << std::endl;
        return true;
    } else {
        std::cerr << "Failed to load ROM: " << filename << std::endl;
        return false;
    }
}

void GameManager::switchEngine(EngineType type)
{
    if (type == currentEngine) {
        return; // Already using this engine
    }
    
    if (type == ENGINE_STATIC && !isStaticEngineReady()) {
        std::cerr << "Cannot switch to static engine - not ready" << std::endl;
        return;
    }
    
    if (type == ENGINE_DYNAMIC && !isDynamicEngineReady()) {
        std::cerr << "Cannot switch to dynamic engine - not ready" << std::endl;
        return;
    }
    
    currentEngine = type;
    
    const char* engineName = (type == ENGINE_STATIC) ? "Static (SMBEngine)" : "Dynamic (SMBEmulator)";
    std::cout << "Switched to " << engineName << " engine" << std::endl;
}

bool GameManager::isEngineReady() const
{
    switch (currentEngine) {
        case ENGINE_STATIC:
            return isStaticEngineReady();
        case ENGINE_DYNAMIC:
            return isDynamicEngineReady();
        default:
            return false;
    }
}

bool GameManager::isStaticEngineReady() const
{
    return staticEngine != nullptr;
}

bool GameManager::isDynamicEngineReady() const
{
    return dynamicEngine != nullptr && dynamicEngine->isROMLoaded();
}

bool GameManager::isDynamicEngineLoaded() const
{
    return dynamicEngine != nullptr && dynamicEngine->isROMLoaded();
}

// Game control methods
void GameManager::reset()
{
    switch (currentEngine) {
        case ENGINE_STATIC:
            if (staticEngine) staticEngine->reset();
            break;
        case ENGINE_DYNAMIC:
            if (dynamicEngine) dynamicEngine->reset();
            break;
    }
}

void GameManager::update()
{
    switch (currentEngine) {
        case ENGINE_STATIC:
            if (staticEngine) staticEngine->update();
            break;
        case ENGINE_DYNAMIC:
            if (dynamicEngine) dynamicEngine->update();
            break;
    }
}

// Rendering methods
void GameManager::render(uint32_t* buffer)
{
    switch (currentEngine) {
        case ENGINE_STATIC:
            if (staticEngine) staticEngine->render(buffer);
            break;
        case ENGINE_DYNAMIC:
            if (dynamicEngine) dynamicEngine->render(buffer);
            break;
    }
}

void GameManager::render16(uint16_t* buffer)
{
    switch (currentEngine) {
        case ENGINE_STATIC:
            if (staticEngine) staticEngine->render16(buffer);
            break;
        case ENGINE_DYNAMIC:
            if (dynamicEngine) dynamicEngine->render16(buffer);
            break;
    }
}

void GameManager::renderScaled16(uint16_t* buffer, int screenWidth, int screenHeight)
{
    switch (currentEngine) {
        case ENGINE_STATIC:
            if (staticEngine) staticEngine->renderScaled16(buffer, screenWidth, screenHeight);
            break;
        case ENGINE_DYNAMIC:
            if (dynamicEngine) dynamicEngine->renderScaled16(buffer, screenWidth, screenHeight);
            break;
    }
}

void GameManager::renderScaled32(uint32_t* buffer, int screenWidth, int screenHeight)
{
    switch (currentEngine) {
        case ENGINE_STATIC:
            if (staticEngine) staticEngine->renderScaled32(buffer, screenWidth, screenHeight);
            break;
        case ENGINE_DYNAMIC:
            if (dynamicEngine) dynamicEngine->renderScaled32(buffer, screenWidth, screenHeight);
            break;
    }
}

// Audio methods
void GameManager::audioCallback(uint8_t* stream, int length)
{
    switch (currentEngine) {
        case ENGINE_STATIC:
            if (staticEngine) staticEngine->audioCallback(stream, length);
            break;
        case ENGINE_DYNAMIC:
            if (dynamicEngine) dynamicEngine->audioCallback(stream, length);
            break;
    }
}

void GameManager::toggleAudioMode()
{
    switch (currentEngine) {
        case ENGINE_STATIC:
            if (staticEngine) staticEngine->toggleAudioMode();
            break;
        case ENGINE_DYNAMIC:
            if (dynamicEngine) dynamicEngine->toggleAudioMode();
            break;
    }
}

bool GameManager::isUsingMIDIAudio() const
{
    switch (currentEngine) {
        case ENGINE_STATIC:
            return staticEngine ? staticEngine->isUsingMIDIAudio() : false;
        case ENGINE_DYNAMIC:
            return dynamicEngine ? dynamicEngine->isUsingMIDIAudio() : false;
        default:
            return false;
    }
}

void GameManager::debugAudioChannels()
{
    switch (currentEngine) {
        case ENGINE_STATIC:
            if (staticEngine) staticEngine->debugAudioChannels();
            break;
        case ENGINE_DYNAMIC:
            if (dynamicEngine) dynamicEngine->debugAudioChannels();
            break;
    }
}

// Controller methods
Controller& GameManager::getController1()
{
    switch (currentEngine) {
        case ENGINE_STATIC:
            if (staticEngine) return staticEngine->getController1();
            break;
        case ENGINE_DYNAMIC:
            if (dynamicEngine) return dynamicEngine->getController1();
            break;
    }
    
    // Fallback - this should never happen if engines are properly initialized
    static Controller fallback(1);
    return fallback;
}

Controller& GameManager::getController2()
{
    switch (currentEngine) {
        case ENGINE_STATIC:
            if (staticEngine) return staticEngine->getController2();
            break;
        case ENGINE_DYNAMIC:
            if (dynamicEngine) return dynamicEngine->getController2();
            break;
    }
    
    // Fallback - this should never happen if engines are properly initialized
    static Controller fallback(2);
    return fallback;
}

// Save state methods
void GameManager::saveState(const std::string& filename)
{
    switch (currentEngine) {
        case ENGINE_STATIC:
            if (staticEngine) staticEngine->saveState(filename);
            break;
        case ENGINE_DYNAMIC:
            if (dynamicEngine) dynamicEngine->saveState(filename);
            break;
    }
}

bool GameManager::loadState(const std::string& filename)
{
    switch (currentEngine) {
        case ENGINE_STATIC:
            return staticEngine ? staticEngine->loadState(filename) : false;
        case ENGINE_DYNAMIC:
            return dynamicEngine ? dynamicEngine->loadState(filename) : false;
        default:
            return false;
    }
}

// Debug methods (dynamic engine only)
GameManager::CPUDebugInfo GameManager::getCPUDebugInfo() const
{
    CPUDebugInfo info;
    info.available = false;
    
    if (currentEngine == ENGINE_DYNAMIC && dynamicEngine) {
        auto state = dynamicEngine->getCPUState();
        info.A = state.A;
        info.X = state.X;
        info.Y = state.Y;
        info.SP = state.SP;
        info.P = state.P;
        info.PC = state.PC;
        info.cycles = state.cycles;
        info.available = true;
    }
    
    return info;
}

uint8_t GameManager::readMemory(uint16_t address) const
{
    switch (currentEngine) {
        case ENGINE_STATIC:
            // Static engine doesn't expose memory reading directly
            return 0;
        case ENGINE_DYNAMIC:
            return dynamicEngine ? dynamicEngine->readMemory(address) : 0;
        default:
            return 0;
    }
}

void GameManager::writeMemory(uint16_t address, uint8_t value)
{
    switch (currentEngine) {
        case ENGINE_STATIC:
            // Static engine doesn't expose memory writing directly
            break;
        case ENGINE_DYNAMIC:
            if (dynamicEngine) dynamicEngine->writeMemory(address, value);
            break;
    }
}
