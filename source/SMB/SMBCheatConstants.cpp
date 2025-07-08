// Auto-generated cheat constants implementation
// Generated from cheats.ini

#include "SMBCheatConstants.hpp"
#include <iostream>

void printAllConstants() {
    auto constants = SMBCheatConstants::getConstants();
    
    for (const auto& [category, constantList] : constants) {
        std::cout << "\n[" << category << "]\n";
        for (const auto& constant : constantList) {
            std::cout << "  " << constant.name << " = " << std::hex << constant.address 
                      << " ; " << constant.description << "\n";
        }
    }
}

void printConstantsByCategory(const std::string& category) {
    auto constants = SMBCheatConstants::getConstants();
    auto it = constants.find(category);
    
    if (it != constants.end()) {
        std::cout << "[" << category << "]\n";
        for (const auto& constant : it->second) {
            std::cout << "  " << constant.name << " = " << std::hex << constant.address 
                      << " ; " << constant.description << "\n";
        }
    } else {
        std::cout << "Category '" << category << "' not found.\n";
    }
}
