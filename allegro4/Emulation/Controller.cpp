#include "Controller.hpp"
#include <iostream>
#include <cstring>

Controller::Controller(int playerNumber) 
    : playerNumber(playerNumber), joystickAvailable(false), joystickIndex(-1), controllerLatch(0), shiftRegister(0)
{
    // Initialize button states to false (not pressed)
    memset(buttonStates, false, sizeof(buttonStates));
    memset(prevJoystickButtons, false, sizeof(prevJoystickButtons));
}

Controller::~Controller()
{
    // Allegro handles cleanup automatically
}

bool Controller::initJoystick()
{
    // Check if joysticks are available
    if (num_joysticks > 0) {
        // Use first available joystick for player 1, second for player 2 if available
        if (playerNumber == 1) {
            joystickIndex = 0;
        } else if (playerNumber == 2 && num_joysticks > 1) {
            joystickIndex = 1;
        } else if (playerNumber == 2) {
            // Player 2 but only one joystick - could share or skip
            joystickIndex = 0;
        }
        
        if (joystickIndex >= 0 && joystickIndex < num_joysticks) {
            joystickAvailable = true;
            std::cout << "Player " << playerNumber << " joystick initialized: Joystick " << joystickIndex << std::endl;
            std::cout << "  Sticks: " << joy[joystickIndex].num_sticks 
                      << ", Buttons: " << joy[joystickIndex].num_buttons << std::endl;
            return true;
        }
    }
    
    std::cout << "No joystick available for player " << playerNumber << std::endl;
    return false;
}

void Controller::updateJoystickState()
{
    if (!joystickAvailable) return;
    
    // Poll joystick state
    poll_joystick();
}

void Controller::processKeyboardInput()
{
    // Clear button states first
    for (int i = 0; i < NUM_BUTTONS; i++) {
        buttonStates[i] = false;
    }
    
    // Process keyboard input based on configuration
    setButtonState(BUTTON_UP, key[getConfiguredScancode(BUTTON_UP)]);
    setButtonState(BUTTON_DOWN, key[getConfiguredScancode(BUTTON_DOWN)]);
    setButtonState(BUTTON_LEFT, key[getConfiguredScancode(BUTTON_LEFT)]);
    setButtonState(BUTTON_RIGHT, key[getConfiguredScancode(BUTTON_RIGHT)]);
    setButtonState(BUTTON_A, key[getConfiguredScancode(BUTTON_A)]);
    setButtonState(BUTTON_B, key[getConfiguredScancode(BUTTON_B)]);
    setButtonState(BUTTON_SELECT, key[getConfiguredScancode(BUTTON_SELECT)]);
    setButtonState(BUTTON_START, key[getConfiguredScancode(BUTTON_START)]);
}

void Controller::processJoystickInput()
{
    if (!joystickAvailable || !Configuration::getJoystickPollingEnabled()) return;
    
    // Update joystick state first
    updateJoystickState();
    
    int deadzone = Configuration::getJoystickDeadzone();
    
    // Handle directional input from first stick
    if (joy[joystickIndex].num_sticks > 0) {
        // Horizontal axis (left/right)
        if (joy[joystickIndex].stick[0].axis[0].pos < -deadzone) {
            setButtonState(BUTTON_LEFT, true);
        } else if (joy[joystickIndex].stick[0].axis[0].pos > deadzone) {
            setButtonState(BUTTON_RIGHT, true);
        }
        
        // Vertical axis (up/down)
        if (joy[joystickIndex].stick[0].axis[1].pos < -deadzone) {
            setButtonState(BUTTON_UP, true);
        } else if (joy[joystickIndex].stick[0].axis[1].pos > deadzone) {
            setButtonState(BUTTON_DOWN, true);
        }
    }
    
    // Handle button input
    int buttonA = getConfiguredJoystickButton(BUTTON_A);
    int buttonB = getConfiguredJoystickButton(BUTTON_B);
    int buttonStart = getConfiguredJoystickButton(BUTTON_START);
    int buttonSelect = getConfiguredJoystickButton(BUTTON_SELECT);
    
    if (buttonA >= 0 && buttonA < joy[joystickIndex].num_buttons) {
        setButtonState(BUTTON_A, joy[joystickIndex].button[buttonA].b);
    }
    
    if (buttonB >= 0 && buttonB < joy[joystickIndex].num_buttons) {
        setButtonState(BUTTON_B, joy[joystickIndex].button[buttonB].b);
    }
    
    if (buttonStart >= 0 && buttonStart < joy[joystickIndex].num_buttons) {
        setButtonState(BUTTON_START, joy[joystickIndex].button[buttonStart].b);
    }
    
    if (buttonSelect >= 0 && buttonSelect < joy[joystickIndex].num_buttons) {
        setButtonState(BUTTON_SELECT, joy[joystickIndex].button[buttonSelect].b);
    }
}

void Controller::setButtonState(ControllerButton button, bool pressed)
{
    if (button >= 0 && button < NUM_BUTTONS) {
        buttonStates[button] = pressed;
    }
}

bool Controller::getButtonState(ControllerButton button) const
{
    if (button >= 0 && button < NUM_BUTTONS) {
        return buttonStates[button];
    }
    return false;
}

uint8_t Controller::getButtonStates() const
{
    uint8_t states = 0;
    
    // Pack button states into a byte (NES controller format)
    if (buttonStates[BUTTON_A]) states |= 0x01;
    if (buttonStates[BUTTON_B]) states |= 0x02;
    if (buttonStates[BUTTON_SELECT]) states |= 0x04;
    if (buttonStates[BUTTON_START]) states |= 0x08;
    if (buttonStates[BUTTON_UP]) states |= 0x10;
    if (buttonStates[BUTTON_DOWN]) states |= 0x20;
    if (buttonStates[BUTTON_LEFT]) states |= 0x40;
    if (buttonStates[BUTTON_RIGHT]) states |= 0x80;
    
    return states;
}

// NES controller read implementation
uint8_t Controller::readByte(int player)
{
    if (player == 1 && playerNumber == 1) {
        // Return the current bit from the shift register
        uint8_t result = (shiftRegister & 0x01) ? 0x01 : 0x00;
        
        // Shift the register for the next read
        shiftRegister >>= 1;
        
        return result | 0x40; // Set bit 6 as per NES controller behavior
    }
    else if (player == 2 && playerNumber == 2) {
        // Similar implementation for player 2
        uint8_t result = (shiftRegister & 0x01) ? 0x01 : 0x00;
        shiftRegister >>= 1;
        return result | 0x40;
    }
    
    return 0x40; // Default return for invalid reads
}

// NES controller write implementation (latch)
void Controller::writeByte(uint8_t value)
{
    // When bit 0 goes from 1 to 0, latch the current button states
    if ((controllerLatch & 0x01) && !(value & 0x01)) {
        // Latch occurred - load current button states into shift register
        shiftRegister = getButtonStates();
    }
    
    controllerLatch = value;
}

void Controller::printButtonStates() const
{
    std::cout << "Player " << playerNumber << " Controller State: ";
    std::cout << "A:" << (buttonStates[BUTTON_A] ? "1" : "0") << " ";
    std::cout << "B:" << (buttonStates[BUTTON_B] ? "1" : "0") << " ";
    std::cout << "Sel:" << (buttonStates[BUTTON_SELECT] ? "1" : "0") << " ";
    std::cout << "Start:" << (buttonStates[BUTTON_START] ? "1" : "0") << " ";
    std::cout << "U:" << (buttonStates[BUTTON_UP] ? "1" : "0") << " ";
    std::cout << "D:" << (buttonStates[BUTTON_DOWN] ? "1" : "0") << " ";
    std::cout << "L:" << (buttonStates[BUTTON_LEFT] ? "1" : "0") << " ";
    std::cout << "R:" << (buttonStates[BUTTON_RIGHT] ? "1" : "0");
    std::cout << std::endl;
}

bool Controller::isJoystickAvailable() const
{
    return joystickAvailable;
}

int Controller::getJoystickIndex() const
{
    return joystickIndex;
}

int Controller::getConfiguredScancode(ControllerButton button) const
{
    // Map controller buttons to configuration scancodes based on player
    if (playerNumber == 1) {
        switch (button) {
            case BUTTON_UP: return Configuration::getPlayer1KeyUp();
            case BUTTON_DOWN: return Configuration::getPlayer1KeyDown();
            case BUTTON_LEFT: return Configuration::getPlayer1KeyLeft();
            case BUTTON_RIGHT: return Configuration::getPlayer1KeyRight();
            case BUTTON_A: return Configuration::getPlayer1KeyA();
            case BUTTON_B: return Configuration::getPlayer1KeyB();
            case BUTTON_SELECT: return Configuration::getPlayer1KeySelect();
            case BUTTON_START: return Configuration::getPlayer1KeyStart();
            default: return -1;
        }
    } else if (playerNumber == 2) {
        switch (button) {
            case BUTTON_UP: return Configuration::getPlayer2KeyUp();
            case BUTTON_DOWN: return Configuration::getPlayer2KeyDown();
            case BUTTON_LEFT: return Configuration::getPlayer2KeyLeft();
            case BUTTON_RIGHT: return Configuration::getPlayer2KeyRight();
            case BUTTON_A: return Configuration::getPlayer2KeyA();
            case BUTTON_B: return Configuration::getPlayer2KeyB();
            case BUTTON_SELECT: return Configuration::getPlayer2KeySelect();
            case BUTTON_START: return Configuration::getPlayer2KeyStart();
            default: return -1;
        }
    }
    return -1;
}

int Controller::getConfiguredJoystickButton(ControllerButton button) const
{
    // Map controller buttons to configuration joystick buttons based on player
    if (playerNumber == 1) {
        switch (button) {
            case BUTTON_A: return Configuration::getPlayer1JoystickButtonA();
            case BUTTON_B: return Configuration::getPlayer1JoystickButtonB();
            case BUTTON_SELECT: return Configuration::getPlayer1JoystickButtonSelect();
            case BUTTON_START: return Configuration::getPlayer1JoystickButtonStart();
            default: return -1;
        }
    } else if (playerNumber == 2) {
        switch (button) {
            case BUTTON_A: return Configuration::getPlayer2JoystickButtonA();
            case BUTTON_B: return Configuration::getPlayer2JoystickButtonB();
            case BUTTON_SELECT: return Configuration::getPlayer2JoystickButtonSelect();
            case BUTTON_START: return Configuration::getPlayer2JoystickButtonStart();
            default: return -1;
        }
    }
    return -1;
}
