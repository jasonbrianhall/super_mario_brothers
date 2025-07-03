#ifndef CONTROLLER_HPP
#define CONTROLLER_HPP

#include <cstdint>
#include "../SDL.h"
#include <array>
#include <iostream>

#ifndef CONTROLLER_ENUMS_INCLUDED
#define CONTROLLER_ENUMS_INCLUDED

/**
 * Buttons found on a standard controller.
 */
enum ControllerButton
{
    BUTTON_A      = 0,
    BUTTON_B      = 1,
    BUTTON_SELECT = 2,
    BUTTON_START  = 3,
    BUTTON_UP     = 4,
    BUTTON_DOWN   = 5,
    BUTTON_LEFT   = 6,
    BUTTON_RIGHT  = 7
};

/**
 * Player identifiers
 */
#ifndef PLAYER_ENUM_DEFINED
#define PLAYER_ENUM_DEFINED
enum Player
{
    PLAYER_1 = 0,
    PLAYER_2 = 1
};
#endif // PLAYER_ENUM_DEFINED
#endif // CONTROLLER_ENUMS_INCLUDED

/**
 * Emulates NES game controller devices for two players.
 * Supports keyboard input and SDL joystick/gamepad input.
 */
class Controller
{
public:
    Controller();
    ~Controller();

    void shutdownJoystick();
    /**
     * Initialize SDL joystick subsystem.
     * Returns true if successful, false otherwise.
     */
    bool initJoystick();

    /**
     * Read from the controller register for a specific player.
     */
    uint8_t readByte(Player player);

    /**
     * Set the state of a button on the controller for a specific player.
     */
    void setButtonState(Player player, ControllerButton button, bool state);

    /**
     * Get the state of a button on the controller for a specific player.
     */
    bool getButtonState(Player player, ControllerButton button) const;

    /**
     * Write a byte to the controller register (affects both players).
     */
    void writeByte(uint8_t value);

    // Backward compatibility methods for existing code
    /**
     * Set button state for Player 1 (backward compatibility)
     */
    void setButtonState(ControllerButton button, bool state);

    /**
     * Get button state for Player 1 (backward compatibility)
     */
    bool getButtonState(ControllerButton button) const;

    /**
     * Read from Player 1 controller (backward compatibility)
     */
    uint8_t readByte();

    /**
     * Process SDL keyboard events.
     * This should be called in your main event loop.
     */
    void processKeyboardEvent(const SDL_Event& event);

    /**
     * Process SDL joystick events.
     * This should be called in your main event loop.
     */
    void processJoystickEvent(const SDL_Event& event);

    /**
     * Update the controller state from joysticks.
     * This should be called once per frame.
     */
    void updateJoystickState();

    /**
     * Debug function to print the current state of both controllers
     */
    void printButtonStates() const;

    /**
     * Check if a joystick is connected for a specific player
     */
    bool isJoystickConnected(Player player) const;

    /**
     * Enable/disable joystick polling (default: from configuration)
     */
    void setJoystickPolling(bool enabled);

    /**
     * Load controller configuration from the config system
     */
    void loadConfiguration();

private:
    // Controller state for each player
    std::array<std::array<bool, 8>, 2> buttonStates;
    std::array<uint8_t, 2> buttonIndex;
    uint8_t strobe;

    // SDL joystick handling for up to 2 joysticks
    std::array<SDL_Joystick*, 2> joysticks;
    std::array<SDL_GameController*, 2> gameControllers;
    std::array<int, 2> joystickIDs;
    std::array<bool, 2> joystickInitialized;

    // Joystick settings - now loaded from configuration
    int joystickDeadzone;
    bool joystickPollingEnabled;

    // Keyboard mappings for both players - now configurable
    struct KeyboardMapping
    {
        SDL_Scancode up, down, left, right;
        SDL_Scancode a, b, select, start;
    };

    KeyboardMapping player1Keys;
    KeyboardMapping player2Keys;

    // Joystick button mappings - now configurable
    struct JoystickMapping
    {
        int buttonA, buttonB, buttonSelect, buttonStart;
    };

    JoystickMapping player1JoystickButtons;
    JoystickMapping player2JoystickButtons;

    // Helper methods
    void setupRetrolinkMapping();
    Player getPlayerFromJoystickID(int joystickID);
    void handleJoystickAxis(Player player, int axis, Sint16 value);
    void handleJoystickButton(Player player, int button, bool pressed);
    void handleControllerButton(Player player, SDL_GameControllerButton button, bool pressed);
    void handleControllerAxis(Player player, SDL_GameControllerAxis axis, Sint16 value);
};

#endif // CONTROLLER_HPP
