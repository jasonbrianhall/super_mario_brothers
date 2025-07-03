#include <fstream>

#include "Configuration.hpp"

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
 * Player 1 keyboard mappings (using numeric scancode values)
 */
BasicConfigurationOption<int> Configuration::player1KeyUp(
    "input.player1.key.up", 82  // SDL_SCANCODE_UP
);

BasicConfigurationOption<int> Configuration::player1KeyDown(
    "input.player1.key.down", 81  // SDL_SCANCODE_DOWN
);

BasicConfigurationOption<int> Configuration::player1KeyLeft(
    "input.player1.key.left", 80  // SDL_SCANCODE_LEFT
);

BasicConfigurationOption<int> Configuration::player1KeyRight(
    "input.player1.key.right", 79  // SDL_SCANCODE_RIGHT
);

BasicConfigurationOption<int> Configuration::player1KeyA(
    "input.player1.key.a", 27  // SDL_SCANCODE_X
);

BasicConfigurationOption<int> Configuration::player1KeyB(
    "input.player1.key.b", 29  // SDL_SCANCODE_Z
);

BasicConfigurationOption<int> Configuration::player1KeySelect(
    "input.player1.key.select", 229  // SDL_SCANCODE_RSHIFT
);

BasicConfigurationOption<int> Configuration::player1KeyStart(
    "input.player1.key.start", 40  // SDL_SCANCODE_RETURN
);

/**
 * Player 2 keyboard mappings
 */
BasicConfigurationOption<int> Configuration::player2KeyUp(
    "input.player2.key.up", 12  // SDL_SCANCODE_I
);

BasicConfigurationOption<int> Configuration::player2KeyDown(
    "input.player2.key.down", 14  // SDL_SCANCODE_K
);

BasicConfigurationOption<int> Configuration::player2KeyLeft(
    "input.player2.key.left", 13  // SDL_SCANCODE_J
);

BasicConfigurationOption<int> Configuration::player2KeyRight(
    "input.player2.key.right", 15  // SDL_SCANCODE_L
);

BasicConfigurationOption<int> Configuration::player2KeyA(
    "input.player2.key.a", 17  // SDL_SCANCODE_N
);

BasicConfigurationOption<int> Configuration::player2KeyB(
    "input.player2.key.b", 16  // SDL_SCANCODE_M
);

BasicConfigurationOption<int> Configuration::player2KeySelect(
    "input.player2.key.select", 228  // SDL_SCANCODE_RCTRL
);

BasicConfigurationOption<int> Configuration::player2KeyStart(
    "input.player2.key.start", 44  // SDL_SCANCODE_SPACE
);

/**
 * Joystick settings
 */
BasicConfigurationOption<bool> Configuration::joystickPollingEnabled(
    "input.joystick.polling_enabled", true
);

BasicConfigurationOption<int> Configuration::joystickDeadzone(
    "input.joystick.deadzone", 8000
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
    
    // Check that the configuration file exists.
    // If it does not exist, we will fall back to default values.
    //
    std::ifstream configFile(fileName.c_str());

    if (configFile.good())
    {
        // Load the configuration file into a property tree to parse it
        //
        boost::property_tree::ptree propertyTree;
        boost::property_tree::ini_parser::read_ini(configFile, propertyTree);

        // Try to load the value for all known config options
        //
        for (auto option : configurationOptions)
        {
            option->initializeValue(propertyTree);
        }
    }
}

void Configuration::save()
{
    if (configFileName.empty()) {
        std::cerr << "Configuration file name not set, cannot save!" << std::endl;
        return;
    }
    
    boost::property_tree::ptree propertyTree;
    
    // Build property tree from all configuration options
    for (auto option : configurationOptions) {
        // This is a bit tricky since we need to know the actual type
        // We'll need to cast each option to its concrete type
        
        // For now, let's handle the common types manually
        std::string path = option->getPath();
        
        if (path == "audio.enabled") {
            propertyTree.put(path, audioEnabled.getValue());
        } else if (path == "audio.frequency") {
            propertyTree.put(path, audioFrequency.getValue());
        } else if (path == "game.frame_rate") {
            propertyTree.put(path, frameRate.getValue());
        } else if (path == "video.palette_file") {
            propertyTree.put(path, paletteFileName.getValue());
        } else if (path == "video.scale") {
            propertyTree.put(path, renderScale.getValue());
        } else if (path == "game.rom_file") {
            propertyTree.put(path, romFileName.getValue());
        } else if (path == "video.scanlines") {
            propertyTree.put(path, scanlinesEnabled.getValue());
        } else if (path == "video.vsync") {
            propertyTree.put(path, vsyncEnabled.getValue());
        } else if (path == "video.hqdn3d") {
            propertyTree.put(path, hqdn3dEnabled.getValue());
        } else if (path == "video.hqdn3d_spatial") {
            propertyTree.put(path, hqdn3dSpatialStrength.getValue());
        } else if (path == "video.hqdn3d_temporal") {
            propertyTree.put(path, hqdn3dTemporalStrength.getValue());
        } else if (path == "video.antialiasing") {
            propertyTree.put(path, antiAliasingEnabled.getValue());
        } else if (path == "video.antialiasing_method") {
            propertyTree.put(path, antiAliasingMethod.getValue());
        }
        // Input settings
        else if (path == "input.player1.key.up") {
            propertyTree.put(path, player1KeyUp.getValue());
        } else if (path == "input.player1.key.down") {
            propertyTree.put(path, player1KeyDown.getValue());
        } else if (path == "input.player1.key.left") {
            propertyTree.put(path, player1KeyLeft.getValue());
        } else if (path == "input.player1.key.right") {
            propertyTree.put(path, player1KeyRight.getValue());
        } else if (path == "input.player1.key.a") {
            propertyTree.put(path, player1KeyA.getValue());
        } else if (path == "input.player1.key.b") {
            propertyTree.put(path, player1KeyB.getValue());
        } else if (path == "input.player1.key.select") {
            propertyTree.put(path, player1KeySelect.getValue());
        } else if (path == "input.player1.key.start") {
            propertyTree.put(path, player1KeyStart.getValue());
        }
        // Player 2 keyboard
        else if (path == "input.player2.key.up") {
            propertyTree.put(path, player2KeyUp.getValue());
        } else if (path == "input.player2.key.down") {
            propertyTree.put(path, player2KeyDown.getValue());
        } else if (path == "input.player2.key.left") {
            propertyTree.put(path, player2KeyLeft.getValue());
        } else if (path == "input.player2.key.right") {
            propertyTree.put(path, player2KeyRight.getValue());
        } else if (path == "input.player2.key.a") {
            propertyTree.put(path, player2KeyA.getValue());
        } else if (path == "input.player2.key.b") {
            propertyTree.put(path, player2KeyB.getValue());
        } else if (path == "input.player2.key.select") {
            propertyTree.put(path, player2KeySelect.getValue());
        } else if (path == "input.player2.key.start") {
            propertyTree.put(path, player2KeyStart.getValue());
        }
        // Joystick settings
        else if (path == "input.joystick.polling_enabled") {
            propertyTree.put(path, joystickPollingEnabled.getValue());
        } else if (path == "input.joystick.deadzone") {
            propertyTree.put(path, joystickDeadzone.getValue());
        }
        // Player 1 joystick buttons
        else if (path == "input.player1.joystick.button_a") {
            propertyTree.put(path, player1JoystickButtonA.getValue());
        } else if (path == "input.player1.joystick.button_b") {
            propertyTree.put(path, player1JoystickButtonB.getValue());
        } else if (path == "input.player1.joystick.button_start") {
            propertyTree.put(path, player1JoystickButtonStart.getValue());
        } else if (path == "input.player1.joystick.button_select") {
            propertyTree.put(path, player1JoystickButtonSelect.getValue());
        }
        // Player 2 joystick buttons
        else if (path == "input.player2.joystick.button_a") {
            propertyTree.put(path, player2JoystickButtonA.getValue());
        } else if (path == "input.player2.joystick.button_b") {
            propertyTree.put(path, player2JoystickButtonB.getValue());
        } else if (path == "input.player2.joystick.button_start") {
            propertyTree.put(path, player2JoystickButtonStart.getValue());
        } else if (path == "input.player2.joystick.button_select") {
            propertyTree.put(path, player2JoystickButtonSelect.getValue());
        }
    }
    
    // Write to file
    try {
        std::ofstream configFile(configFileName);
        boost::property_tree::ini_parser::write_ini(configFile, propertyTree);
        std::cout << "Configuration saved to " << configFileName << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error saving configuration: " << e.what() << std::endl;
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
