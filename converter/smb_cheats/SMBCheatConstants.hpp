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
                {"numberoflives", 0x075a, "Player Stats", "Number of lives (0-99)"},
                {"cointally", 0x075e, "Player Stats", "Coin count (0-99)"},
                {"worldnumber", 0x075f, "Player Stats", "Current world (0-7)"},
                {"levelnumber", 0x075c, "Player Stats", "Current level (0-3)"},
                {"playersize", 0x0754, "Player Stats", "Mario size (0=small, 1=big, 2=fire)"},
                {"numberofplayers", 0x077a, "Player Stats", "Number of players (1-2)"},
            }},
            {"Player Movement", {
                {"playerxposition", 0x86, "Player Movement", "Mario X position"},
                {"playeryposition", 0xce, "Player Movement", "Mario Y position"},
                {"playerxspeed", 0x57, "Player Movement", "Mario horizontal speed"},
                {"playeryspeed", 0x9f, "Player Movement", "Mario vertical speed"},
                {"playerfacingdir", 0x33, "Player Movement", "Direction Mario is facing"},
            }},
            {"Game State", {
                {"gamepausestatus", 0x0776, "Game State", "Game pause state (0=running, 1=paused)"},
                {"opermode", 0x0770, "Game State", "Operating mode"},
                {"timercontrol", 0x0747, "Game State", "Game timer control"},
                {"scrolllock", 0x0723, "Game State", "Screen scroll lock"},
            }},
            {"Cheats", {
                {"starinvincibletimer", 0x079f, "Cheats", "Star power timer (0=off, 255=max)"},
                {"disablecollisiondet", 0x0716, "Cheats", "Disable collision detection (0=off, 1=on)"},
                {"injurytimer", 0x079e, "Cheats", "Injury invincibility timer"},
            }},
            {"Enemies", {
                {"enemyfreezeall", 0x0717, "Enemies", "Freeze all enemies (0=normal, 1=frozen)"},
                {"enemyspawnrate", 0x0712, "Enemies", "Control enemy spawn frequency"},
                {"goombastate", 0x000e, "Enemies", "Individual Goomba states"},
                {"koopastate", 0x0018, "Enemies", "Individual Koopa states"},
            }},
            {"Objects", {
                {"fireballcount", 0x0024, "Objects", "Number of active fireballs"},
                {"blockstates", 0x0500, "Objects", "Block destruction states"},
                {"coinblockhits", 0x0504, "Objects", "Coin block hit counters"},
            }},
            {"Advanced Game State", {
                {"levelcompleteflag", 0x0770, "Advanced Game State", "Force level completion"},
                {"bowserdefeated", 0x0784, "Advanced Game State", "Bowser battle state"},
                {"flagpoletimer", 0x0747, "Advanced Game State", "Flagpole slide timer"},
                {"warpzoneactive", 0x0725, "Advanced Game State", "Warp zone detection"},
            }},
            {"Power-ups", {
                {"firefloweractive", 0x0755, "Power-ups", "Fire flower power state"},
                {"oneupcounter", 0x0758, "Power-ups", "1-UP mushroom spawn control"},
                {"supermushroomspawn", 0x0756, "Power-ups", "Force super mushroom spawns"},
            }},
            {"Debug", {
                {"hitboxdisplay", 0x0720, "Debug", "Show collision boxes (emulator dependent)"},
                {"speedhack", 0x0721, "Debug", "Game speed multiplier"},
                {"noclipmode", 0x0722, "Debug", "Walk through walls"},
                {"infinitetime", 0x0748, "Debug", "Stop countdown timer"},
            }},
            {"Audio-Visual", {
                {"musictrack", 0x0780, "Audio-Visual", "Force specific background music"},
                {"soundeffectmute", 0x0781, "Audio-Visual", "Mute sound effects"},
                {"paletteswap", 0x0782, "Audio-Visual", "Change color palette"},
            }},
            {"Physics", {
                {"gravitymodifier", 0x0730, "Physics", "Adjust gravity strength (0=moon, 255=heavy)"},
                {"jumpheight", 0x0731, "Physics", "Mario's jump power multiplier"},
                {"walljumpenable", 0x0732, "Physics", "Allow wall jumping (0=off, 1=on)"},
                {"waterphysics", 0x0733, "Physics", "Apply swimming physics on land"},
            }},
            {"World Control", {
                {"infinitescrolling", 0x0740, "World Control", "Remove screen boundaries"},
                {"levelwraparound", 0x0741, "World Control", "Loop level horizontally"},
                {"backgroundscroll", 0x0742, "World Control", "Independent background scrolling"},
                {"pipewarpoverride", 0x0743, "World Control", "Custom pipe destinations"},
            }},
            {"Chaos", {
                {"randomizepowerups", 0x0750, "Chaos", "Randomize power-up effects"},
                {"enemybehaviorswap", 0x0751, "Chaos", "Enemies act like different types"},
                {"blockshuffle", 0x0752, "Chaos", "Randomize block contents"},
                {"physicsglitch", 0x0753, "Chaos", "Random physics changes"},
            }},
            {"Player Enhancement", {
                {"doublejumpenable", 0x0760, "Player Enhancement", "Allow double/triple jumping"},
                {"dashattack", 0x0761, "Player Enhancement", "Hold button to dash through enemies"},
                {"sizeoverride", 0x0762, "Player Enhancement", "Force specific Mario size regardless of power-ups"},
                {"invisibilitymode", 0x0763, "Player Enhancement", "Mario becomes invisible to enemies"},
            }},
            {"Score Hacks", {
                {"scoremultiplier", 0x0770, "Score Hacks", "Points multiplier (1-255)"},
                {"automaticoneups", 0x0771, "Score Hacks", "Gain 1-UP every X seconds"},
                {"unlockallworlds", 0x0772, "Score Hacks", "Access any world from start"},
                {"bonusstageaccess", 0x0773, "Score Hacks", "Force bonus room spawns"},
            }},
            {"Experimental", {
                {"multimario", 0x0780, "Experimental", "Control multiple Marios simultaneously"},
                {"timerewind", 0x0781, "Experimental", "Rewind last 5 seconds of gameplay"},
                {"ghostmode", 0x0782, "Experimental", "Record/playback Mario's movements"},
                {"weaponpickup", 0x0783, "Experimental", "Allow Mario to pick up enemy shells as weapons"},
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
