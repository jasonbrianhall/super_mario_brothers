// Auto-generated cheat constants implementation
// Generated from cheats.ini

#include "SMBCheatConstants.hpp"
#include <iostream>
#include <iomanip>

// Forward declarations
int getTotalConstantCount();
void printAllConstants();
void printConstantsByCategory(const std::string& category);
void searchConstants(const std::string& searchTerm);

int getTotalConstantCount() {
    int total = 0;
    auto constants = SMBCheatConstants::getConstants();
    for (const auto& [category, constantList] : constants) {
        total += constantList.size();
    }
    return total;
}

void printAllConstants() {
    auto constants = SMBCheatConstants::getConstants();
    
    std::cout << "\n=== Super Mario Bros Cheat Constants ===\n";
    
    for (const auto& [category, constantList] : constants) {
        std::cout << "\n[" << category << "]\n";
        for (const auto& constant : constantList) {
            std::cout << "  " << std::setw(20) << std::left << constant.name 
                      << " = 0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << constant.address 
                      << std::dec << " ; " << constant.description << "\n";
        }
    }
    
    std::cout << "\nTotal: " << std::dec << getTotalConstantCount() << " constants in " 
              << constants.size() << " categories\n";
}

void printConstantsByCategory(const std::string& category) {
    auto constants = SMBCheatConstants::getConstantsByCategory(category);
    
    if (!constants.empty()) {
        std::cout << "\n[" << category << "]\n";
        for (const auto& constant : constants) {
            std::cout << "  " << std::setw(20) << std::left << constant.name 
                      << " = 0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << constant.address 
                      << std::dec << " ; " << constant.description << "\n";
        }
    } else {
        std::cout << "Category '" << category << "' not found.\n";
        std::cout << "Available categories: ";
        auto categories = SMBCheatConstants::getCategoryNames();
        for (size_t i = 0; i < categories.size(); ++i) {
            std::cout << categories[i];
            if (i < categories.size() - 1) std::cout << ", ";
        }
        std::cout << "\n";
    }
}

void searchConstants(const std::string& searchTerm) {
    auto constants = SMBCheatConstants::getConstants();
    bool found = false;
    
    std::cout << "\nSearching for: " << searchTerm << "\n";
    
    for (const auto& [category, constantList] : constants) {
        for (const auto& constant : constantList) {
            if (constant.name.find(searchTerm) != std::string::npos ||
                constant.description.find(searchTerm) != std::string::npos) {
                if (!found) {
                    std::cout << "Found matches:\n";
                    found = true;
                }
                std::cout << "  [" << category << "] " << constant.name 
                          << " = 0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << constant.address 
                          << std::dec << " ; " << constant.description << "\n";
            }
        }
    }
    
    if (!found) {
        std::cout << "No matches found.\n";
    }
}
