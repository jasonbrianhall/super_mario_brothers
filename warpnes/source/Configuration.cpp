#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

#include "Configuration.hpp"

// SimpleINI Implementation
bool SimpleINI::loadFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    std::string line;
    std::string currentSection = "";
    
    while (std::getline(file, line)) {
        line = trim(line);
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == ';' || line[0] == '#') {
            continue;
        }
        
        // Check for section header
        if (line[0] == '[' && line.back() == ']') {
            currentSection = line.substr(1, line.length() - 2);
            currentSection = trim(currentSection);
            continue;
        }
        
        // Parse key=value pairs
        size_t equalPos = line.find('=');
        if (equalPos != std::string::npos) {
            std::string key = trim(line.substr(0, equalPos));
            std::string value = trim(line.substr(equalPos + 1));
            
            // Remove quotes from value if present
            if (value.length() >= 2 && value[0] == '"' && value.back() == '"') {
                value = value.substr(1, value.length() - 2);
            }
            
            data[currentSection][key] = value;
        }
    }
    
    return true;
}

bool SimpleINI::saveToFile(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    for (const auto& section : data) {
        if (!section.first.empty()) {
            file << "[" << section.first << "]" << std::endl;
        }
        
        for (const auto& keyValue : section.second) {
            file << keyValue.first << " = " << keyValue.second << std::endl;
        }
        
        file << std::endl;
    }
    
    return true;
}

std::string SimpleINI::getString(const std::string& section, const std::string& key, const std::string& defaultValue) {
    auto sectionIt = data.find(section);
    if (sectionIt != data.end()) {
        auto keyIt = sectionIt->second.find(key);
        if (keyIt != sectionIt->second.end()) {
            return keyIt->second;
        }
    }
    return defaultValue;
}

int SimpleINI::getInt(const std::string& section, const std::string& key, int defaultValue) {
    std::string value = getString(section, key, "");
    if (!value.empty()) {
        try {
            return std::stoi(value);
        } catch (const std::exception&) {
            // Fall through to return default
        }
    }
    return defaultValue;
}

float SimpleINI::getFloat(const std::string& section, const std::string& key, float defaultValue) {
    std::string value = getString(section, key, "");
    if (!value.empty()) {
        try {
            return std::stof(value);
        } catch (const std::exception&) {
            // Fall through to return default
        }
    }
    return defaultValue;
}

bool SimpleINI::getBool(const std::string& section, const std::string& key, bool defaultValue) {
    std::string value = getString(section, key, "");
    if (!value.empty()) {
        std::transform(value.begin(), value.end(), value.begin(), ::tolower);
        if (value == "true" || value == "1" || value == "yes" || value == "on") {
            return true;
        } else if (value == "false" || value == "0" || value == "no" || value == "off") {
            return false;
        }
    }
    return defaultValue;
}

void SimpleINI::setString(const std::string& section, const std::string& key, const std::string& value) {
    data[section][key] = value;
}

void SimpleINI::setInt(const std::string& section, const std::string& key, int value) {
    data[section][key] = std::to_string(value);
}

void SimpleINI::setFloat(const std::string& section, const std::string& key, float value) {
    data[section][key] = std::to_string(value);
}

void SimpleINI::setBool(const std::string& section, const std::string& key, bool value) {
    data[section][key] = value ? "true" : "false";
}

std::string SimpleINI::trim(const std::string& str) {
    size_t first = str.find_first_not_of(' ');
    if (std::string::npos == first) {
        return str;
    }
    size_t last = str.find_last_not_of(' ');
    return str.substr(first, (last - first + 1));
}

void SimpleINI::parsePath(const std::string& path, std::string& section, std::string& key) {
    size_t dotPos = path.find('.');
    if (dotPos != std::string::npos) {
        section = path.substr(0, dotPos);
        key = path.substr(dotPos + 1);
    } else {
        section = "";
        key = path;
    }
}

// Template specializations for BasicConfigurationOption
template<>
void BasicConfigurationOption<bool>::initializeValue(const SimpleINI &ini) {
    std::string section, key;
    std::string path = getPath();
    size_t dotPos = path.find('.');
    if (dotPos != std::string::npos) {
        section = path.substr(0, dotPos);
        key = path.substr(dotPos + 1);
    } else {
        section = "";
        key = path;
    }
    
    value = ini.getBool(section, key, defaultValue);
    std::cout << "Configuration option \"" << getPath() << "\" set to \"" << (value ? "true" : "false") << "\"" << std::endl;
}

template<>
void BasicConfigurationOption<int>::initializeValue(const SimpleINI &ini) {
    std::string section, key;
    std::string path = getPath();
    size_t dotPos = path.find('.');
    if (dotPos != std::string::npos) {
        section = path.substr(0, dotPos);
        key = path.substr(dotPos + 1);
    } else {
        section = "";
        key = path;
    }
    
    value = ini.getInt(section, key, defaultValue);
    std::cout << "Configuration option \"" << getPath() << "\" set to \"" << value << "\"" << std::endl;
}

template<>
void BasicConfigurationOption<float>::initializeValue(const SimpleINI &ini) {
    std::string section, key;
    std::string path = getPath();
    size_t dotPos = path.find('.');
    if (dotPos != std::string::npos) {
        section = path.substr(0, dotPos);
        key = path.substr(dotPos + 1);
    } else {
        section = "";
        key = path;
    }
    
    value = ini.getFloat(section, key, defaultValue);
    std::cout << "Configuration option \"" << getPath() << "\" set to \"" << value << "\"" << std::endl;
}

template<>
void BasicConfigurationOption<std::string>::initializeValue(const SimpleINI &ini) {
    std::string section, key;
    std::string path = getPath();
    size_t dotPos = path.find('.');
    if (dotPos != std::string::npos) {
        section = path.substr(0, dotPos);
        key = path.substr(dotPos + 1);
    } else {
        section = "";
        key = path;
    }
    
    value = ini.getString(section, key, defaultValue);
    std::cout << "Configuration option \"" << getPath() << "\" set to \"" << value << "\"" << std::endl;
}

template<>
void BasicConfigurationOption<bool>::saveValue(SimpleINI &ini) {
    std::string section, key;
    std::string path = getPath();
    size_t dotPos = path.find('.');
    if (dotPos != std::string::npos) {
        section = path.substr(0, dotPos);
        key = path.substr(dotPos + 1);
    } else {
        section = "";
        key = path;
    }
    
    ini.setBool(section, key, value);
}

template<>
void BasicConfigurationOption<int>::saveValue(SimpleINI &ini) {
    std::string section, key;
    std::string path = getPath();
    size_t dotPos = path.find('.');
    if (dotPos != std::string::npos) {
        section = path.substr(0, dotPos);
        key = path.substr(dotPos + 1);
    } else {
        section = "";
        key = path;
    }
    
    ini.setInt(section, key, value);
}

template<>
void BasicConfigurationOption<float>::saveValue(SimpleINI &ini) {
    std::string section, key;
    std::string path = getPath();
    size_t dotPos = path.find('.');
    if (dotPos != std::string::npos) {
        section = path.substr(0, dotPos);
        key = path.substr(dotPos + 1);
    } else {
        section = "";
        key = path;
    }
    
    ini.setFloat(section, key, value);
}

template<>
void BasicConfigurationOption<std::string>::saveValue(SimpleINI &ini) {
    std::string section, key;
    std::string path = getPath();
    size_t dotPos = path.find('.');
    if (dotPos != std::string::npos) {
        section = path.substr(0, dotPos);
        key = path.substr(dotPos + 1);
    } else {
        section = "";
        key = path;
    }
    
    ini.setString(section, key, value);
}

// Static member to store config file name
std::string Configuration::configFileName;

/**
 * List of all supported configuration options.
 */
std::list<ConfigurationOption*> Configuration::configurationOptions = {
    &Configuration::audioEnabled,
    &Configuration::audioFrequency,
    &Configuration::frameRate,
    &Configuration::paletteFileName,
    &Configuration::renderScale,
    &Configuration::romFileName,
    &Configuration::scanlinesEnabled,
    &Configuration::vsyncEnabled,
    &Configuration::hqdn3dEnabled,
    &Configuration::hqdn3dSpatialStrength,
    &Configuration::hqdn3dTemporalStrength,
    &Configuration::antiAliasingEnabled,
    &Configuration::antiAliasingMethod,
    
    // Input configuration options
    &Configuration::player1KeyUp,
    &Configuration::player1KeyDown,
    &Configuration::player1KeyLeft,
    &Configuration::player1KeyRight,
    &Configuration::player1KeyA,
    &Configuration::player1KeyB,
    &Configuration::player1KeySelect,
    &Configuration::player1KeyStart,
    
    &Configuration::player2KeyUp,
    &Configuration::player2KeyDown,
    &Configuration::player2KeyLeft,
    &Configuration::player2KeyRight,
    &Configuration::player2KeyA,
    &Configuration::player2KeyB,
    &Configuration::player2KeySelect,
    &Configuration::player2KeyStart,
    
    &Configuration::joystickPollingEnabled,
    &Configuration::joystickDeadzone,
    
    &Configuration::player1JoystickButtonA,
    &Configuration::player1JoystickButtonB,
    &Configuration::player1JoystickButtonStart,
    &Configuration::player1JoystickButtonSelect,
    
    &Configuration::player2JoystickButtonA,
    &Configuration::player2JoystickButtonB,
    &Configuration::player2JoystickButtonStart,
    &Configuration::player2JoystickButtonSelect
};

/**
 * Whether audio is enabled or not.
 */
BasicConfigurationOption<bool> Configuration::audioEnabled(
    "audio.enabled", true
);

/**
 * Audio frequency, in Hz
 */
BasicConfigurationOption<int> Configuration::audioFrequency(
    "audio.frequency", 48000
);

/**
 * Frame rate (per second).
 */
BasicConfigurationOption<int> Configuration::frameRate(
    "game.frame_rate", 60
);

/**
 * The filename for a custom palette to use for rendering.
 */
BasicConfigurationOption<std::string> Configuration::paletteFileName(
    "video.palette_file", ""
);

/**
 * Scaling factor for rendering.
 */
BasicConfigurationOption<int> Configuration::renderScale(
    "video.scale", 3
);

/**
 * Filename for the SMB ROM image.
 */
BasicConfigurationOption<std::string> Configuration::romFileName(
    "game.rom_file", "Super Mario Bros. (JU) (PRG0) [!].nes"
);

/**
 * Whether scanlines are enabled or not.
 */
BasicConfigurationOption<bool> Configuration::scanlinesEnabled(
    "video.scanlines", false
);

/**
 * Whether vsync is enabled for video.
 */
BasicConfigurationOption<bool> Configuration::vsyncEnabled(
    "video.vsync", true
);

/**
 * Whether hqdn3d is enabled for video.
 */
BasicConfigurationOption<bool> Configuration::hqdn3dEnabled(
    "video.hqdn3d", false
);

/**
 * Spatial strength for hqdn3d filter (0.0 - 1.0)
 */
BasicConfigurationOption<float> Configuration::hqdn3dSpatialStrength(
    "video.hqdn3d_spatial", 0.4f
);

/**
 * Temporal strength for hqdn3d filter (0.0 - 1.0)
 */
BasicConfigurationOption<float> Configuration::hqdn3dTemporalStrength(
    "video.hqdn3d_temporal", 0.6f
);

/**
 * Whether anti-aliasing is enabled for video.
 */
BasicConfigurationOption<bool> Configuration::antiAliasingEnabled(
    "video.antialiasing", false
);

/**
 * Anti-aliasing method to use.
 * 0 = FXAA, 1 = MSAA
 */
BasicConfigurationOption<int> Configuration::antiAliasingMethod(
    "video.antialiasing_method", 0
);

/**
 * Player 1 keyboard mappings (using Allegro key constants)
 * Note: These default values should be updated to use Allegro KEY_* constants
 */
BasicConfigurationOption<int> Configuration::player1KeyUp(
    "input.player1.key.up", 84  // KEY_UP in Allegro
);

BasicConfigurationOption<int> Configuration::player1KeyDown(
    "input.player1.key.down", 85  // KEY_DOWN in Allegro
);

BasicConfigurationOption<int> Configuration::player1KeyLeft(
    "input.player1.key.left", 82  // KEY_LEFT in Allegro
);

BasicConfigurationOption<int> Configuration::player1KeyRight(
    "input.player1.key.right", 83  // KEY_RIGHT in Allegro
);

BasicConfigurationOption<int> Configuration::player1KeyA(
    "input.player1.key.a", 120  // KEY_X in Allegro
);

BasicConfigurationOption<int> Configuration::player1KeyB(
    "input.player1.key.b", 122  // KEY_Z in Allegro
);

BasicConfigurationOption<int> Configuration::player1KeySelect(
    "input.player1.key.select", 26  // [
);

BasicConfigurationOption<int> Configuration::player1KeyStart(
    "input.player1.key.start", 27  // ]
);

/**
 * Player 2 keyboard mappings
 */
BasicConfigurationOption<int> Configuration::player2KeyUp(
    "input.player2.key.up", 105  // KEY_I in Allegro
);

BasicConfigurationOption<int> Configuration::player2KeyDown(
    "input.player2.key.down", 107  // KEY_K in Allegro
);

BasicConfigurationOption<int> Configuration::player2KeyLeft(
    "input.player2.key.left", 106  // KEY_J in Allegro
);

BasicConfigurationOption<int> Configuration::player2KeyRight(
    "input.player2.key.right", 108  // KEY_L in Allegro
);

BasicConfigurationOption<int> Configuration::player2KeyA(
    "input.player2.key.a", 110  // KEY_N in Allegro
);

BasicConfigurationOption<int> Configuration::player2KeyB(
    "input.player2.key.b", 109  // KEY_M in Allegro
);

BasicConfigurationOption<int> Configuration::player2KeySelect(
    "input.player2.key.select", 97  // KEY_RCONTROL in Allegro
);

BasicConfigurationOption<int> Configuration::player2KeyStart(
    "input.player2.key.start", 57  // KEY_SPACE in Allegro
);

/**
 * Joystick settings
 */
BasicConfigurationOption<bool> Configuration::joystickPollingEnabled(
    "input.joystick.polling_enabled", true
);

BasicConfigurationOption<int> Configuration::joystickDeadzone(
    "input.joystick.deadzone", 64  // Allegro uses different scale (0-255)
);

/**
 * Player 1 joystick button mappings
 */
BasicConfigurationOption<int> Configuration::player1JoystickButtonA(
    "input.player1.joystick.button_a", 1
);

BasicConfigurationOption<int> Configuration::player1JoystickButtonB(
    "input.player1.joystick.button_b", 0
);

BasicConfigurationOption<int> Configuration::player1JoystickButtonStart(
    "input.player1.joystick.button_start", 9
);

BasicConfigurationOption<int> Configuration::player1JoystickButtonSelect(
    "input.player1.joystick.button_select", 8
);

/**
 * Player 2 joystick button mappings
 */
BasicConfigurationOption<int> Configuration::player2JoystickButtonA(
    "input.player2.joystick.button_a", 1
);

BasicConfigurationOption<int> Configuration::player2JoystickButtonB(
    "input.player2.joystick.button_b", 0
);

BasicConfigurationOption<int> Configuration::player2JoystickButtonStart(
    "input.player2.joystick.button_start", 9
);

BasicConfigurationOption<int> Configuration::player2JoystickButtonSelect(
    "input.player2.joystick.button_select", 8
);

ConfigurationOption::ConfigurationOption(
    const std::string& path) :
    path(path)
{
}

const std::string& ConfigurationOption::getPath() const
{
    return path;
}

void Configuration::initialize(const std::string& fileName)
{
    configFileName = fileName;  // Store filename for saving
    
    // Create a SimpleINI instance to parse the file
    SimpleINI ini;
    
    // Try to load the configuration file
    // If it doesn't exist, we'll use default values
    if (ini.loadFromFile(fileName)) {
        std::cout << "Configuration file loaded: " << fileName << std::endl;
    } else {
        std::cout << "Configuration file not found or could not be loaded: " << fileName << std::endl;
        std::cout << "Using default values." << std::endl;
    }

    // Initialize all configuration options
    for (auto option : configurationOptions) {
        option->initializeValue(ini);
    }
}

void Configuration::save()
{
    if (configFileName.empty()) {
        std::cerr << "Configuration file name not set, cannot save!" << std::endl;
        return;
    }
    
    SimpleINI ini;
    
    // Save all configuration options to the INI
    for (auto option : configurationOptions) {
        option->saveValue(ini);
    }
    
    // Write to file
    if (ini.saveToFile(configFileName)) {
        std::cout << "Configuration saved to " << configFileName << std::endl;
    } else {
        std::cerr << "Error saving configuration to " << configFileName << std::endl;
    }
}

bool Configuration::getAudioEnabled()
{
    return audioEnabled.getValue();
}

int Configuration::getAudioFrequency()
{
    return audioFrequency.getValue();
}

int Configuration::getFrameRate()
{
    return frameRate.getValue();
}

const std::string& Configuration::getPaletteFileName()
{
    return paletteFileName.getValue();
}

int Configuration::getRenderScale()
{
    return renderScale.getValue();
}

const std::string& Configuration::getRomFileName()
{
    return romFileName.getValue();
}

bool Configuration::getScanlinesEnabled()
{
    return scanlinesEnabled.getValue();
}

bool Configuration::getVsyncEnabled()
{
    return vsyncEnabled.getValue();
}

bool Configuration::getHqdn3dEnabled()
{
    return hqdn3dEnabled.getValue();
}

float Configuration::getHqdn3dSpatialStrength()
{
    return hqdn3dSpatialStrength.getValue();
}

float Configuration::getHqdn3dTemporalStrength()
{
    return hqdn3dTemporalStrength.getValue();
}

bool Configuration::getAntiAliasingEnabled()
{
    return antiAliasingEnabled.getValue();
}

int Configuration::getAntiAliasingMethod()
{
    return antiAliasingMethod.getValue();
}

// Player 1 keyboard getters and setters
int Configuration::getPlayer1KeyUp() { return player1KeyUp.getValue(); }
void Configuration::setPlayer1KeyUp(int value) { player1KeyUp.setValue(value); }

int Configuration::getPlayer1KeyDown() { return player1KeyDown.getValue(); }
void Configuration::setPlayer1KeyDown(int value) { player1KeyDown.setValue(value); }

int Configuration::getPlayer1KeyLeft() { return player1KeyLeft.getValue(); }
void Configuration::setPlayer1KeyLeft(int value) { player1KeyLeft.setValue(value); }

int Configuration::getPlayer1KeyRight() { return player1KeyRight.getValue(); }
void Configuration::setPlayer1KeyRight(int value) { player1KeyRight.setValue(value); }

int Configuration::getPlayer1KeyA() { return player1KeyA.getValue(); }
void Configuration::setPlayer1KeyA(int value) { player1KeyA.setValue(value); }

int Configuration::getPlayer1KeyB() { return player1KeyB.getValue(); }
void Configuration::setPlayer1KeyB(int value) { player1KeyB.setValue(value); }

int Configuration::getPlayer1KeySelect() { return player1KeySelect.getValue(); }
void Configuration::setPlayer1KeySelect(int value) { player1KeySelect.setValue(value); }

int Configuration::getPlayer1KeyStart() { return player1KeyStart.getValue(); }
void Configuration::setPlayer1KeyStart(int value) { player1KeyStart.setValue(value); }

// Player 2 keyboard getters and setters
int Configuration::getPlayer2KeyUp() { return player2KeyUp.getValue(); }
void Configuration::setPlayer2KeyUp(int value) { player2KeyUp.setValue(value); }

int Configuration::getPlayer2KeyDown() { return player2KeyDown.getValue(); }
void Configuration::setPlayer2KeyDown(int value) { player2KeyDown.setValue(value); }

int Configuration::getPlayer2KeyLeft() { return player2KeyLeft.getValue(); }
void Configuration::setPlayer2KeyLeft(int value) { player2KeyLeft.setValue(value); }

int Configuration::getPlayer2KeyRight() { return player2KeyRight.getValue(); }
void Configuration::setPlayer2KeyRight(int value) { player2KeyRight.setValue(value); }

int Configuration::getPlayer2KeyA() { return player2KeyA.getValue(); }
void Configuration::setPlayer2KeyA(int value) { player2KeyA.setValue(value); }

int Configuration::getPlayer2KeyB() { return player2KeyB.getValue(); }
void Configuration::setPlayer2KeyB(int value) { player2KeyB.setValue(value); }

int Configuration::getPlayer2KeySelect() { return player2KeySelect.getValue(); }
void Configuration::setPlayer2KeySelect(int value) { player2KeySelect.setValue(value); }

int Configuration::getPlayer2KeyStart() { return player2KeyStart.getValue(); }
void Configuration::setPlayer2KeyStart(int value) { player2KeyStart.setValue(value); }

// Joystick settings getters and setters
bool Configuration::getJoystickPollingEnabled() { return joystickPollingEnabled.getValue(); }
void Configuration::setJoystickPollingEnabled(bool value) { joystickPollingEnabled.setValue(value); }

int Configuration::getJoystickDeadzone() { return joystickDeadzone.getValue(); }
void Configuration::setJoystickDeadzone(int value) { joystickDeadzone.setValue(value); }

// Player 1 joystick button getters and setters
int Configuration::getPlayer1JoystickButtonA() { return player1JoystickButtonA.getValue(); }
void Configuration::setPlayer1JoystickButtonA(int value) { player1JoystickButtonA.setValue(value); }

int Configuration::getPlayer1JoystickButtonB() { return player1JoystickButtonB.getValue(); }
void Configuration::setPlayer1JoystickButtonB(int value) { player1JoystickButtonB.setValue(value); }

int Configuration::getPlayer1JoystickButtonStart() { return player1JoystickButtonStart.getValue(); }
void Configuration::setPlayer1JoystickButtonStart(int value) { player1JoystickButtonStart.setValue(value); }

int Configuration::getPlayer1JoystickButtonSelect() { return player1JoystickButtonSelect.getValue(); }
void Configuration::setPlayer1JoystickButtonSelect(int value) { player1JoystickButtonSelect.setValue(value); }

// Player 2 joystick button getters and setters
int Configuration::getPlayer2JoystickButtonA() { return player2JoystickButtonA.getValue(); }
void Configuration::setPlayer2JoystickButtonA(int value) { player2JoystickButtonA.setValue(value); }

int Configuration::getPlayer2JoystickButtonB() { return player2JoystickButtonB.getValue(); }
void Configuration::setPlayer2JoystickButtonB(int value) { player2JoystickButtonB.setValue(value); }

int Configuration::getPlayer2JoystickButtonStart() { return player2JoystickButtonStart.getValue(); }
void Configuration::setPlayer2JoystickButtonStart(int value) { player2JoystickButtonStart.setValue(value); }

int Configuration::getPlayer2JoystickButtonSelect() { return player2JoystickButtonSelect.getValue(); }
void Configuration::setPlayer2JoystickButtonSelect(int value) { player2JoystickButtonSelect.setValue(value); }
