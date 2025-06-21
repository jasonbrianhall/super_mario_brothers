#ifndef CONTROLLER_HPP
#define CONTROLLER_HPP

#include <cstdint>
#include <SDL2/SDL.h>

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
 * Emulates an NES game controller device.
 * Supports keyboard input and SDL joystick/gamepad input.
 */
class Controller
{
public:
    Controller();
    ~Controller();

    /**
     * Initialize SDL joystick subsystem.
     * Returns true if successful, false otherwise.
     */
    bool initJoystick();

    /**
     * Read from the controller register.
     */
    uint8_t readByte();

    /**
     * Set the state of a button on the controller.
     */
    void setButtonState(ControllerButton button, bool state);

    /**
     * Get the state of a button on the controller.
     */
    bool getButtonState(ControllerButton button) const;

    /**
     * Write a byte to the controller register.
     */
    void writeByte(uint8_t value);

    /**
     * Process SDL joystick events.
     * This should be called in your main event loop.
     */
    void processJoystickEvent(const SDL_Event& event);

    /**
     * Update the controller state from the joystick.
     * This should be called once per frame.
     */
    void updateJoystickState();

    // Debug function to print the current state of the controller
    void printButtonStates() const;
    


private:
    bool    buttonStates[8];
    uint8_t buttonIndex;
    uint8_t strobe;

    // SDL joystick handling
    SDL_Joystick* joystick;
    SDL_GameController* gameController;
    int joystickID;
    bool joystickInitialized;

    // Joystick deadzone
    static const int JOYSTICK_DEADZONE = 8000;

    // Map SDL joystick/gamepad buttons to NES controller buttons
    void mapJoystickButtonToController(int button, ControllerButton nesButton);
    
    // Creates a custom mapping for the Retrolink SNES Controller
    void setupRetrolinkMapping();
};

#endif // CONTROLLER_HPP
