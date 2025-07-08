#!/usr/bin/env python3
import configparser
import sys
import os

def parse_ini_file(filename):
    """Parse INI file and extract constants with categories"""
    config = configparser.ConfigParser(inline_comment_prefixes=(';', '#'))
    config.read(filename)
    
    categories = {}
    
    for section_name in config.sections():
        constants = []
        
        for key, value in config.items(section_name):
            # Parse value and optional comment
            parts = value.split(';', 1)
            address = parts[0].strip()
            description = parts[1].strip() if len(parts) > 1 else f"Memory address {key}"
            
            constants.append({
                'name': key,
                'address': address,
                'description': description
            })
        
        categories[section_name] = constants
    
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

void printAllConstants() {
    auto constants = SMBCheatConstants::getConstants();
    
    for (const auto& [category, constantList] : constants) {
        std::cout << "\\n[" << category << "]\\n";
        for (const auto& constant : constantList) {
            std::cout << "  " << constant.name << " = " << std::hex << constant.address 
                      << " ; " << constant.description << "\\n";
        }
    }
}

void printConstantsByCategory(const std::string& category) {
    auto constants = SMBCheatConstants::getConstants();
    auto it = constants.find(category);
    
    if (it != constants.end()) {
        std::cout << "[" << category << "]\\n";
        for (const auto& constant : it->second) {
            std::cout << "  " << constant.name << " = " << std::hex << constant.address 
                      << " ; " << constant.description << "\\n";
        }
    } else {
        std::cout << "Category '" << category << "' not found.\\n";
    }
}
""")

def main():
    if len(sys.argv) != 2:
        print("Usage: python generate_cheats.py cheats.ini")
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
    
    print(f"Generating {output_header}...")
    generate_cpp_header(categories, output_header)
    
    print(f"Generating {output_source}...")
    generate_cpp_source(categories, output_source)
    
    print("Done! Include SMBCheatConstants.hpp in your project.")
    print("\nExample usage in C++:")
    print("  auto constants = SMBCheatConstants::getConstants();")
    print("  uint16_t lives_addr = SMBCheatConstants::getAddress(\"NumberofLives\");")

if __name__ == "__main__":
    main()
