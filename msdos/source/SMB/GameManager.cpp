#include "GameManager.hpp"
#include "SMBEngine.hpp"
#include "SMBEmulator.hpp"
#include "SMBRom.hpp"
#include "../Emulation/Controller.hpp"
#include <iostream>

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
