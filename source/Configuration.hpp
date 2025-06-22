#ifndef CONFIGURATION_HPP
#define CONFIGURATION_HPP

#include <iostream>
#include <list>
#include <string>

#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/ptree.hpp>

/**
 * Base class for configuration options.
 */
class ConfigurationOption {
public:
  /**
   * Construct a configuration option.
   *
   * @param path the path to the option in the conf file,
   * in the format "section.key" where section is the
   * INI section (e.g. [example]) and key is the option
   * name in the section.
   */
  ConfigurationOption(const std::string &path);

  /**
   * Get the path of the configuration option within the INI file.
   */
  const std::string &getPath() const;

  /**
   * Initialize the configuration option from the parsed property tree.
   */
  virtual void
  initializeValue(const boost::property_tree::ptree &propertyTree) = 0;

private:
  std::string path;
};

/**
 * Basic configuration option template for values that have simple types and
 * only a default value.
 */
template <typename T>
class BasicConfigurationOption : public ConfigurationOption {
public:
  /**
   * Constructor.
   */
  BasicConfigurationOption(const std::string &path, const T &defaultValue)
      : ConfigurationOption(path), value(defaultValue) {}

  /**
   * Get the value of the configuration option.
   */
  const T &getValue() const { return value; }

  /**
   * Set the value of the configuration option.
   */
  void setValue(const T &newValue) { value = newValue; }

  /**
   * Initialize the configuration option.
   */
  void
  initializeValue(const boost::property_tree::ptree &propertyTree) override {
    value = propertyTree.get<T>(getPath(), value);
    std::cout << "Configuration option \"" << getPath() << "\" set to \""
              << value << "\"" << std::endl;
  }

private:
  T value;
};

/**
 * Singleton class that reads the configuration file that provides global
 * program options from the user.
 */
class Configuration {
public:
  /**
   * Initialize the global configuration from the given file.
   */
  static void initialize(const std::string &fileName);

  /**
   * Save the configuration to the file.
   */
  static void save();

  /**
   * Get if audio is enabled or not.
   */
  static bool getAudioEnabled();

  /**
   * Get the desired audio frequency, in Hz.
   */
  static int getAudioFrequency();

  /**
   * Get the desired frame rate (per second).
   */
  static int getFrameRate();

  /**
   * Get the filename for a custom palette to use for rendering.
   */
  static const std::string &getPaletteFileName();

  /**
   * Get the desired ROM file name.
   */
  static const std::string &getRomFileName();

  /**
   * Get the desired render scale.
   */
  static int getRenderScale();

  /**
   * Get whether scanlines are enabled or not.
   */
  static bool getScanlinesEnabled();

  /**
   * Get whether vsync is enabled or not.
   */
  static bool getVsyncEnabled();

  /**
   * Get whether hqdn3d denoising is enabled or not.
   */
  static bool getHqdn3dEnabled();

  /**
   * Get the hqdn3d spatial strength.
   */
  static float getHqdn3dSpatialStrength();

  /**
   * Get the hqdn3d temporal strength.
   */
  static float getHqdn3dTemporalStrength();

  /**
   * Get whether anti-aliasing is enabled or not.
   */
  static bool getAntiAliasingEnabled();

  /**
   * Get the anti-aliasing method to use.
   * 0 = FXAA, 1 = MSAA (if supported by hardware)
   */
  static int getAntiAliasingMethod();

  /**
   * Get Player 1 keyboard mapping for UP button
   */
  static int getPlayer1KeyUp();
  static void setPlayer1KeyUp(int value);

  /**
   * Get Player 1 keyboard mapping for DOWN button
   */
  static int getPlayer1KeyDown();
  static void setPlayer1KeyDown(int value);

  /**
   * Get Player 1 keyboard mapping for LEFT button
   */
  static int getPlayer1KeyLeft();
  static void setPlayer1KeyLeft(int value);

  /**
   * Get Player 1 keyboard mapping for RIGHT button
   */
  static int getPlayer1KeyRight();
  static void setPlayer1KeyRight(int value);

  /**
   * Get Player 1 keyboard mapping for A button
   */
  static int getPlayer1KeyA();
  static void setPlayer1KeyA(int value);

  /**
   * Get Player 1 keyboard mapping for B button
   */
  static int getPlayer1KeyB();
  static void setPlayer1KeyB(int value);

  /**
   * Get Player 1 keyboard mapping for SELECT button
   */
  static int getPlayer1KeySelect();
  static void setPlayer1KeySelect(int value);

  /**
   * Get Player 1 keyboard mapping for START button
   */
  static int getPlayer1KeyStart();
  static void setPlayer1KeyStart(int value);

  // Player 2 keyboard mappings
  static int getPlayer2KeyUp();
  static void setPlayer2KeyUp(int value);
  static int getPlayer2KeyDown();
  static void setPlayer2KeyDown(int value);
  static int getPlayer2KeyLeft();
  static void setPlayer2KeyLeft(int value);
  static int getPlayer2KeyRight();
  static void setPlayer2KeyRight(int value);
  static int getPlayer2KeyA();
  static void setPlayer2KeyA(int value);
  static int getPlayer2KeyB();
  static void setPlayer2KeyB(int value);
  static int getPlayer2KeySelect();
  static void setPlayer2KeySelect(int value);
  static int getPlayer2KeyStart();
  static void setPlayer2KeyStart(int value);

  /**
   * Get whether joystick polling is enabled
   */
  static bool getJoystickPollingEnabled();
  static void setJoystickPollingEnabled(bool value);

  /**
   * Get joystick deadzone value
   */
  static int getJoystickDeadzone();
  static void setJoystickDeadzone(int value);

  /**
   * Get joystick button mapping for Player 1 A button
   */
  static int getPlayer1JoystickButtonA();
  static void setPlayer1JoystickButtonA(int value);

  /**
   * Get joystick button mapping for Player 1 B button
   */
  static int getPlayer1JoystickButtonB();
  static void setPlayer1JoystickButtonB(int value);

  /**
   * Get joystick button mapping for Player 1 START button
   */
  static int getPlayer1JoystickButtonStart();
  static void setPlayer1JoystickButtonStart(int value);

  /**
   * Get joystick button mapping for Player 1 SELECT button
   */
  static int getPlayer1JoystickButtonSelect();
  static void setPlayer1JoystickButtonSelect(int value);

  // Player 2 joystick button mappings
  static int getPlayer2JoystickButtonA();
  static void setPlayer2JoystickButtonA(int value);
  static int getPlayer2JoystickButtonB();
  static void setPlayer2JoystickButtonB(int value);
  static int getPlayer2JoystickButtonStart();
  static void setPlayer2JoystickButtonStart(int value);
  static int getPlayer2JoystickButtonSelect();
  static void setPlayer2JoystickButtonSelect(int value);

private:
  static BasicConfigurationOption<bool> audioEnabled;
  static BasicConfigurationOption<int> audioFrequency;
  static BasicConfigurationOption<int> frameRate;
  static BasicConfigurationOption<std::string> paletteFileName;
  static BasicConfigurationOption<int> renderScale;
  static BasicConfigurationOption<std::string> romFileName;
  static BasicConfigurationOption<bool> scanlinesEnabled;
  static BasicConfigurationOption<bool> vsyncEnabled;
  static BasicConfigurationOption<bool> hqdn3dEnabled;
  static BasicConfigurationOption<float> hqdn3dSpatialStrength;
  static BasicConfigurationOption<float> hqdn3dTemporalStrength;
  static BasicConfigurationOption<bool> antiAliasingEnabled;
  static BasicConfigurationOption<int> antiAliasingMethod;

  // Player 1 keyboard mappings (SDL_Scancode values stored as int)
  static BasicConfigurationOption<int> player1KeyUp;
  static BasicConfigurationOption<int> player1KeyDown;
  static BasicConfigurationOption<int> player1KeyLeft;
  static BasicConfigurationOption<int> player1KeyRight;
  static BasicConfigurationOption<int> player1KeyA;
  static BasicConfigurationOption<int> player1KeyB;
  static BasicConfigurationOption<int> player1KeySelect;
  static BasicConfigurationOption<int> player1KeyStart;

  // Player 2 keyboard mappings
  static BasicConfigurationOption<int> player2KeyUp;
  static BasicConfigurationOption<int> player2KeyDown;
  static BasicConfigurationOption<int> player2KeyLeft;
  static BasicConfigurationOption<int> player2KeyRight;
  static BasicConfigurationOption<int> player2KeyA;
  static BasicConfigurationOption<int> player2KeyB;
  static BasicConfigurationOption<int> player2KeySelect;
  static BasicConfigurationOption<int> player2KeyStart;

  // Joystick settings
  static BasicConfigurationOption<bool> joystickPollingEnabled;
  static BasicConfigurationOption<int> joystickDeadzone;

  // Player 1 joystick button mappings
  static BasicConfigurationOption<int> player1JoystickButtonA;
  static BasicConfigurationOption<int> player1JoystickButtonB;
  static BasicConfigurationOption<int> player1JoystickButtonStart;
  static BasicConfigurationOption<int> player1JoystickButtonSelect;

  // Player 2 joystick button mappings
  static BasicConfigurationOption<int> player2JoystickButtonA;
  static BasicConfigurationOption<int> player2JoystickButtonB;
  static BasicConfigurationOption<int> player2JoystickButtonStart;
  static BasicConfigurationOption<int> player2JoystickButtonSelect;

  static std::list<ConfigurationOption *> configurationOptions;
  static std::string configFileName;
};

#endif // CONFIGURATION_HPP
