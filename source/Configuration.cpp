#include <fstream>

#include "Configuration.hpp"

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
    &Configuration::antiAliasingMethod
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
