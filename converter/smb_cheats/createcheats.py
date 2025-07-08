#!/usr/bin/env python3
import configparser
import sys
import os

def parse_ini_file(filename):
    """Parse INI file and extract constants with categories"""
    categories = {}
    
    # Read file manually to preserve comments that configparser strips
    with open(filename, 'r') as f:
        lines = f.readlines()
    
    current_section = None
    
    for line in lines:
        line = line.strip()
        
        # Skip empty lines and pure comments
        if not line or line.startswith(';') or line.startswith('#'):
            continue
        
        # Check for section headers
        if line.startswith('[') and line.endswith(']'):
            current_section = line[1:-1]
            categories[current_section] = []
            continue
        
        # Parse key-value pairs with comments
        if current_section and '=' in line:
            # Split on first = only
            key, value_with_comment = line.split('=', 1)
            key = key.strip()
            value_with_comment = value_with_comment.strip()
            
            # Split value and comment on semicolon
            if ';' in value_with_comment:
                address, description = value_with_comment.split(';', 1)
                address = address.strip()
                description = description.strip()
            else:
                address = value_with_comment.strip()
                description = f"Memory address for {key.replace('_', ' ').lower()}"
            
            categories[current_section].append({
                'name': key.lower().replace('_', ''),  # Normalize name for C++
                'address': address,
                'description': description
            })
    
    return categories

def generate_cpp_header(categories, output_file):
    """Generate C++ header file"""
    
    with open(output_file, 'w') as f:
        f.write("""// Auto-generated cheat constants
// Generated from cheats.ini

#ifndef SMB_CHEAT_CONSTANTS_HPP
#define SMB_CHEAT_CONSTANTS_HPP

#include <map>
#include <string>
#include <vector>
#include <cstdint>

struct CheatConstant {
    std::string name;
    uint16_t address;
    std::string category;
    std::string description;
};

class SMBCheatConstants {
public:
    static const std::map<std::string, std::vector<CheatConstant>>& getConstants() {
        static std::map<std::string, std::vector<CheatConstant>> constants = {
""")
        
        # Generate each category
        for category, constants in categories.items():
            f.write(f'            {{"{category}", {{\n')
            
            for const in constants:
                name = const['name']
                address = const['address']
                description = const['description'].replace('"', '\\"')  # Escape quotes
                f.write(f'                {{"{name}", {address}, "{category}", "{description}"}},\n')
            
            f.write('            }},\n')
        
        f.write("""        };
        return constants;
    }
    
    // Get a flat map of name -> address for quick lookups
    static std::map<std::string, uint16_t> getFlatMap() {
        static std::map<std::string, uint16_t> flatMap;
        static bool initialized = false;
        
        if (!initialized) {
            for (const auto& [category, constants] : getConstants()) {
                for (const auto& constant : constants) {
                    flatMap[constant.name] = constant.address;
                }
            }
            initialized = true;
        }
        return flatMap;
    }
    
    // Get address by name
    static uint16_t getAddress(const std::string& name) {
        auto flatMap = getFlatMap();
        auto it = flatMap.find(name);
        return (it != flatMap.end()) ? it->second : 0;
    }
    
    // Get all constants in a category
    static std::vector<CheatConstant> getConstantsByCategory(const std::string& category) {
        auto constants = getConstants();
        auto it = constants.find(category);
        return (it != constants.end()) ? it->second : std::vector<CheatConstant>();
    }
    
    // Get all category names
    static std::vector<std::string> getCategoryNames() {
        std::vector<std::string> categories;
        for (const auto& [category, constants] : getConstants()) {
            categories.push_back(category);
        }
        return categories;
    }
};

#endif // SMB_CHEAT_CONSTANTS_HPP
""")

def generate_cpp_source(categories, output_file):
    """Generate C++ source file with helper functions"""
    
    with open(output_file, 'w') as f:
        f.write("""// Auto-generated cheat constants implementation
// Generated from cheats.ini

#include "SMBCheatConstants.hpp"
#include <iostream>
#include <iomanip>

void printAllConstants() {
    auto constants = SMBCheatConstants::getConstants();
    
    std::cout << "\\n=== Super Mario Bros Cheat Constants ===\\n";
    
    for (const auto& [category, constantList] : constants) {
        std::cout << "\\n[" << category << "]\\n";
        for (const auto& constant : constantList) {
            std::cout << "  " << std::setw(20) << std::left << constant.name 
                      << " = 0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << constant.address 
                      << std::dec << " ; " << constant.description << "\\n";
        }
    }
    
    std::cout << "\\nTotal: " << std::dec << getTotalConstantCount() << " constants in " 
              << constants.size() << " categories\\n";
}

void printConstantsByCategory(const std::string& category) {
    auto constants = SMBCheatConstants::getConstantsByCategory(category);
    
    if (!constants.empty()) {
        std::cout << "\\n[" << category << "]\\n";
        for (const auto& constant : constants) {
            std::cout << "  " << std::setw(20) << std::left << constant.name 
                      << " = 0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << constant.address 
                      << std::dec << " ; " << constant.description << "\\n";
        }
    } else {
        std::cout << "Category '" << category << "' not found.\\n";
        std::cout << "Available categories: ";
        auto categories = SMBCheatConstants::getCategoryNames();
        for (size_t i = 0; i < categories.size(); ++i) {
            std::cout << categories[i];
            if (i < categories.size() - 1) std::cout << ", ";
        }
        std::cout << "\\n";
    }
}

int getTotalConstantCount() {
    int total = 0;
    auto constants = SMBCheatConstants::getConstants();
    for (const auto& [category, constantList] : constants) {
        total += constantList.size();
    }
    return total;
}

void searchConstants(const std::string& searchTerm) {
    auto constants = SMBCheatConstants::getConstants();
    bool found = false;
    
    std::cout << "\\nSearching for: " << searchTerm << "\\n";
    
    for (const auto& [category, constantList] : constants) {
        for (const auto& constant : constantList) {
            if (constant.name.find(searchTerm) != std::string::npos ||
                constant.description.find(searchTerm) != std::string::npos) {
                if (!found) {
                    std::cout << "Found matches:\\n";
                    found = true;
                }
                std::cout << "  [" << category << "] " << constant.name 
                          << " = 0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << constant.address 
                          << std::dec << " ; " << constant.description << "\\n";
            }
        }
    }
    
    if (!found) {
        std::cout << "No matches found.\\n";
    }
}
""")

def main():
    if len(sys.argv) != 2:
        print("Usage: python createcheats.py cheats.ini")
        sys.exit(1)
    
    input_file = sys.argv[1]
    
    if not os.path.exists(input_file):
        print(f"Error: {input_file} not found")
        sys.exit(1)
    
    output_header = "SMBCheatConstants.hpp"
    output_source = "SMBCheatConstants.cpp"
    
    print(f"Parsing {input_file}...")
    categories = parse_ini_file(input_file)
    
    total_constants = sum(len(constants) for constants in categories.values())
    print(f"Found {total_constants} constants in {len(categories)} categories:")
    
    for category, const_list in categories.items():
        print(f"  {category}: {len(const_list)} constants")
    
    print(f"\nGenerating {output_header}...")
    generate_cpp_header(categories, output_header)
    
    print(f"Generating {output_source}...")
    generate_cpp_source(categories, output_source)
    
    print("\nDone! Include SMBCheatConstants.hpp in your project.")
    print("\nExample usage in C++:")
    print("  auto constants = SMBCheatConstants::getConstants();")
    print("  uint16_t lives_addr = SMBCheatConstants::getAddress(\"numberoflives\");")
    print("  printAllConstants();")
    print("  printConstantsByCategory(\"Player Stats\");")
    print("  searchConstants(\"mario\");")

if __name__ == "__main__":
    main()
