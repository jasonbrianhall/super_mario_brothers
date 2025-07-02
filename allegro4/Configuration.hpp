#ifndef CONTROLLER_HPP
#define CONTROLLER_HPP

#include <allegro.h>
#include "Configuration.hpp"

/**
 * Button constants for NES controller
 */
enum ControllerButton {
    BUTTON_A = 0,
    BUTTON_B = 1,
    BUTTON_SELECT = 2,
    BUTTON_START = 3,
    BUTTON_UP = 4,
    BUTTON_DOWN = 5,
    BUTTON_LEFT = 6,
    BUTTON_RIGHT = 7,
    NUM_BUTTONS = 8
};

/**
 * Controller class that handles input from keyboard and joystick
 * using Allegro 4 instead of SDL2
 */
class Controller {
public:
    /**
     * Constructor
     */
    Controller(int playerNumber = 1);

    /**
     * Destructor
     */
    ~Controller();

    /**
     * Initialize joystick support
     * @return true if joystick was successfully initialized
     */
    bool initJoystick();

    /**
     * Update joystick state (poll for current state)
     */
    void updateJoystickState();

    /**
     * Process keyboard input using Allegro 4 key array
     */
    void processKeyboardInput();

    /**
     * Process joystick input using Allegro 4 joystick system
     */
    void processJoystickInput();

    /**
     * Set the state of a specific button
     * @param button The button to set
     * @param pressed Whether the button is pressed
     */
    void setButtonState(ControllerButton button, bool pressed);

    /**
     * Get the state of a specific button
     * @param button The button to check
     * @return true if the button is pressed
     */
    bool getButtonState(ControllerButton button) const;

    /**
     * Get the current button states as a byte (for NES controller emulation)
     * @return 8-bit value representing button states
     */
    uint8_t getButtonStates() const;

    /**
     * Print current button states for debugging
     */
    void printButtonStates() const;

    /**
     * Check if joystick is available and initialized
     */
    bool isJoystickAvailable() const;

    /**
     * Get the joystick index being used
     */
    int getJoystickIndex() const;

private:
    int playerNumber;           // Player 1 or 2
    bool buttonStates[NUM_BUTTONS];  // Current button states
    bool joystickAvailable;     // Whether joystick is available
    int joystickIndex;          // Which joystick to use (0-based)
    
    // Previous joystick button states for edge detection
    bool prevJoystickButtons[32]; // Allegro supports up to 32 buttons
    
    /**
     * Map Allegro scancode to controller button based on configuration
     */
    ControllerButton mapScancodeToButton(int scancode) const;
    
    /**
     * Get the configured scancode for a specific button
     */
    int getConfiguredScancode(ControllerButton button) const;
    
    /**
     * Get the configured joystick button for a specific controller button
     */
    int getConfiguredJoystickButton(ControllerButton button) const;
};

#endif // CONTROLLER_HPP
