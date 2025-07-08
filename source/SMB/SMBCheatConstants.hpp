// Auto-generated cheat constants
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
            {"Player Stats", {
                {"numberoflives", 0x075a, "Player Stats", "Memory address numberoflives"},
                {"cointally", 0x075e, "Player Stats", "Memory address cointally"},
                {"worldnumber", 0x075f, "Player Stats", "Memory address worldnumber"},
                {"levelnumber", 0x075c, "Player Stats", "Memory address levelnumber"},
                {"playersize", 0x0754, "Player Stats", "Memory address playersize"},
                {"numberofplayers", 0x077a, "Player Stats", "Memory address numberofplayers"},
            }},
            {"Player Movement", {
                {"player_x_position", 0x86, "Player Movement", "Memory address player_x_position"},
                {"player_y_position", 0xce, "Player Movement", "Memory address player_y_position"},
                {"player_x_speed", 0x57, "Player Movement", "Memory address player_x_speed"},
                {"player_y_speed", 0x9f, "Player Movement", "Memory address player_y_speed"},
                {"playerfacingdir", 0x33, "Player Movement", "Memory address playerfacingdir"},
            }},
            {"Game State", {
                {"gamepausestatus", 0x0776, "Game State", "Memory address gamepausestatus"},
                {"opermode", 0x0770, "Game State", "Memory address opermode"},
                {"timercontrol", 0x0747, "Game State", "Memory address timercontrol"},
                {"scrolllock", 0x0723, "Game State", "Memory address scrolllock"},
            }},
            {"Cheats", {
                {"starinvincibletimer", 0x079f, "Cheats", "Memory address starinvincibletimer"},
                {"disablecollisiondet", 0x0716, "Cheats", "Memory address disablecollisiondet"},
                {"injurytimer", 0x079e, "Cheats", "Memory address injurytimer"},
            }},
        };
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
