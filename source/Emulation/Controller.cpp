#include "Controller.hpp"
#include <iostream>

Controller::Controller() : joystick(nullptr), gameController(nullptr), joystickID(-1), joystickInitialized(false)
{
    for (auto& b : buttonStates)
    {
        b = false;
    }
    buttonIndex = 0;
    strobe = 1;
    
    // Load SDL_GameControllerDB database if it exists
    FILE* dbFile = fopen("gamecontrollerdb.txt", "r");
    if (dbFile)
    {
        fclose(dbFile);
        if (SDL_GameControllerAddMappingsFromFile("gamecontrollerdb.txt") > 0)
        {
            std::cout << "Loaded controller mappings from gamecontrollerdb.txt" << std::endl;
        }
    }
}

Controller::~Controller()
{
    if (gameController != nullptr)
    {
        SDL_GameControllerClose(gameController);
    }
    else if (joystick != nullptr)
    {
        SDL_JoystickClose(joystick);
    }
}

bool Controller::initJoystick()
{
    // Make sure SDL has been initialized with joystick support
    if (SDL_WasInit(SDL_INIT_JOYSTICK) == 0)
    {
        if (SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) < 0)
        {
            std::cerr << "SDL joystick subsystem could not be initialized: " << SDL_GetError() << std::endl;
            return false;
        }
    }

    // Enable joystick events
    SDL_JoystickEventState(SDL_ENABLE);
    SDL_GameControllerEventState(SDL_ENABLE);
    
    // Set up Retrolink SNES Controller mapping
    setupRetrolinkMapping();

    // Check for available joysticks
    int numJoysticks = SDL_NumJoysticks();
    if (numJoysticks <= 0)
    {
        std::cout << "No joysticks connected." << std::endl;
        return false;
    }
    
    // List all connected joysticks for debugging
    std::cout << "Found " << numJoysticks << " joystick(s):" << std::endl;
    for (int i = 0; i < numJoysticks; i++)
    {
        std::cout << i << ": " << SDL_JoystickNameForIndex(i);
        if (SDL_IsGameController(i))
        {
            std::cout << " (GameController compatible)";
        }
        std::cout << std::endl;
    }

    // Try to open the first available joystick
    // First try to open as game controller (more standardized)
    for (int i = 0; i < numJoysticks; i++)
    {
        if (SDL_IsGameController(i))
        {
            gameController = SDL_GameControllerOpen(i);
            if (gameController)
            {
                std::cout << "Found gamepad: " << SDL_GameControllerName(gameController) << std::endl;
                joystick = SDL_GameControllerGetJoystick(gameController);
                joystickID = SDL_JoystickInstanceID(joystick);
                joystickInitialized = true;
                
                // Debug: Print controller info
                std::cout << "Connected as game controller with "
                          << SDL_JoystickNumAxes(joystick) << " axes and "
                          << SDL_JoystickNumButtons(joystick) << " buttons." << std::endl;
                
                return true;
            }
        }
    }

    // If no game controller is available, try as generic joystick
    if (!joystickInitialized)
    {
        joystick = SDL_JoystickOpen(0);
        if (joystick)
        {
            std::cout << "Found joystick: " << SDL_JoystickName(joystick) << std::endl;
            joystickID = SDL_JoystickInstanceID(joystick);
            joystickInitialized = true;
            
            // Debug: Print number of buttons, axes, etc.
            std::cout << "Joystick has " << SDL_JoystickNumButtons(joystick) 
                      << " buttons and " << SDL_JoystickNumAxes(joystick) 
                      << " axes." << std::endl;
            
            return true;
        }
        else
        {
            std::cerr << "Could not open joystick: " << SDL_GetError() << std::endl;
            return false;
        }
    }

    return joystickInitialized;
}

uint8_t Controller::readByte()
{
    uint8_t value = 1;

    if (buttonIndex < 8)
    {
        value = (buttonStates[buttonIndex] ? 0x41 : 0x40);
    }

    if ((strobe & (1 << 0)) == 0)
    {
        buttonIndex++;
    }

    return value;
}

void Controller::setButtonState(ControllerButton button, bool state)
{
    buttonStates[(int)button] = state;
}

bool Controller::getButtonState(ControllerButton button) const
{
    return buttonStates[(int)button];
}

void Controller::writeByte(uint8_t value)
{
    if ((value & (1 << 0)) == 0 && (strobe & (1 << 0)) == 1)
    {
        buttonIndex = 0;
    }
    strobe = value;
}

void Controller::processJoystickEvent(const SDL_Event& event)
{
    if (!joystickInitialized)
        return;

    // Handle various joystick events
    switch (event.type)
    {
        case SDL_JOYAXISMOTION:
            if (event.jaxis.which == joystickID)
            {
                // Handle X axis (left/right)
                if (event.jaxis.axis == 0)
                {
                    if (event.jaxis.value < -JOYSTICK_DEADZONE)
                    {
                        setButtonState(BUTTON_LEFT, true);
                        setButtonState(BUTTON_RIGHT, false);
                    }
                    else if (event.jaxis.value > JOYSTICK_DEADZONE)
                    {
                        setButtonState(BUTTON_RIGHT, true);
                        setButtonState(BUTTON_LEFT, false);
                    }
                    else
                    {
                        setButtonState(BUTTON_LEFT, false);
                        setButtonState(BUTTON_RIGHT, false);
                    }
                }
                // Handle Y axis (up/down)
                else if (event.jaxis.axis == 1)
                {
                    if (event.jaxis.value < -JOYSTICK_DEADZONE)
                    {
                        setButtonState(BUTTON_UP, true);
                        setButtonState(BUTTON_DOWN, false);
                    }
                    else if (event.jaxis.value > JOYSTICK_DEADZONE)
                    {
                        setButtonState(BUTTON_DOWN, true);
                        setButtonState(BUTTON_UP, false);
                    }
                    else
                    {
                        setButtonState(BUTTON_UP, false);
                        setButtonState(BUTTON_DOWN, false);
                    }
                }
            }
            break;

        case SDL_JOYHATMOTION:
            if (event.jhat.which == joystickID)
            {
                // Handle hat movement (D-pad on many controllers)
                Uint8 hatValue = event.jhat.value;
                
                setButtonState(BUTTON_UP, (hatValue & SDL_HAT_UP) != 0);
                setButtonState(BUTTON_DOWN, (hatValue & SDL_HAT_DOWN) != 0);
                setButtonState(BUTTON_LEFT, (hatValue & SDL_HAT_LEFT) != 0);
                setButtonState(BUTTON_RIGHT, (hatValue & SDL_HAT_RIGHT) != 0);
            }
            break;

        case SDL_JOYBUTTONDOWN:
            if (event.jbutton.which == joystickID)
            {
                // Debug output
                std::cout << "Button pressed: " << (int)event.jbutton.button << std::endl;
                
                // Call our mapping function
                mapJoystickButtonToController(event.jbutton.button, BUTTON_A);
            }
            break;

        case SDL_JOYBUTTONUP:
            if (event.jbutton.which == joystickID)
            {
                // Log button release for debugging
                std::cout << "Button released: " << (int)event.jbutton.button << std::endl;
                
                // Map based on Retrolink SNES layout - correct mapping for your controller
                switch (event.jbutton.button)
                {
                    case 0: setButtonState(BUTTON_B, false); break;      // SNES B -> NES B
                    case 1: setButtonState(BUTTON_A, false); break;      // SNES Y -> NES A 
                    case 8: setButtonState(BUTTON_SELECT, false); break; // SNES Select -> NES Select
                    case 9: setButtonState(BUTTON_START, false); break;  // SNES Start -> NES Start
                    case 4: setButtonState(BUTTON_A, false); break;      // SNES A -> NES A (alternate)
                    case 5: setButtonState(BUTTON_B, false); break;      // SNES X -> NES B (alternate)
                    default: break;
                }
            }
            break;

        case SDL_CONTROLLERBUTTONDOWN:
            if (event.cbutton.which == joystickID)
            {
                // GameController mapping (standardized across different controllers)
                switch (event.cbutton.button)
                {
                    case SDL_CONTROLLER_BUTTON_A:
                        setButtonState(BUTTON_A, true);
                        break;
                    case SDL_CONTROLLER_BUTTON_B:
                        setButtonState(BUTTON_B, true);
                        break;
                    case SDL_CONTROLLER_BUTTON_BACK:
                        setButtonState(BUTTON_SELECT, true);
                        break;
                    case SDL_CONTROLLER_BUTTON_START:
                        setButtonState(BUTTON_START, true);
                        break;
                    case SDL_CONTROLLER_BUTTON_DPAD_UP:
                        setButtonState(BUTTON_UP, true);
                        break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                        setButtonState(BUTTON_DOWN, true);
                        break;
                    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                        setButtonState(BUTTON_LEFT, true);
                        break;
                    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                        setButtonState(BUTTON_RIGHT, true);
                        break;
                }
            }
            break;

        case SDL_CONTROLLERBUTTONUP:
            if (event.cbutton.which == joystickID)
            {
                // GameController mapping
                switch (event.cbutton.button)
                {
                    case SDL_CONTROLLER_BUTTON_A:
                        setButtonState(BUTTON_A, false);
                        break;
                    case SDL_CONTROLLER_BUTTON_B:
                        setButtonState(BUTTON_B, false);
                        break;
                    case SDL_CONTROLLER_BUTTON_BACK:
                        setButtonState(BUTTON_SELECT, false);
                        break;
                    case SDL_CONTROLLER_BUTTON_START:
                        setButtonState(BUTTON_START, false);
                        break;
                    case SDL_CONTROLLER_BUTTON_DPAD_UP:
                        setButtonState(BUTTON_UP, false);
                        break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                        setButtonState(BUTTON_DOWN, false);
                        break;
                    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                        setButtonState(BUTTON_LEFT, false);
                        break;
                    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                        setButtonState(BUTTON_RIGHT, false);
                        break;
                }
            }
            break;

        case SDL_CONTROLLERAXISMOTION:
            if (event.caxis.which == joystickID)
            {
                // Handle left thumbstick X axis
                if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX)
                {
                    if (event.caxis.value < -JOYSTICK_DEADZONE)
                    {
                        setButtonState(BUTTON_LEFT, true);
                        setButtonState(BUTTON_RIGHT, false);
                    }
                    else if (event.caxis.value > JOYSTICK_DEADZONE)
                    {
                        setButtonState(BUTTON_RIGHT, true);
                        setButtonState(BUTTON_LEFT, false);
                    }
                    else
                    {
                        setButtonState(BUTTON_LEFT, false);
                        setButtonState(BUTTON_RIGHT, false);
                    }
                }
                // Handle left thumbstick Y axis
                else if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY)
                {
                    if (event.caxis.value < -JOYSTICK_DEADZONE)
                    {
                        setButtonState(BUTTON_UP, true);
                        setButtonState(BUTTON_DOWN, false);
                    }
                    else if (event.caxis.value > JOYSTICK_DEADZONE)
                    {
                        setButtonState(BUTTON_DOWN, true);
                        setButtonState(BUTTON_UP, false);
                    }
                    else
                    {
                        setButtonState(BUTTON_UP, false);
                        setButtonState(BUTTON_DOWN, false);
                    }
                }
            }
            break;
    }
}

void Controller::updateJoystickState()
{
    if (!joystickInitialized)
        return;

    // Poll joystick state directly - this ensures buttons remain pressed when held down
    if (joystick)
    {
        // For Retrolink SNES Controller, specifically correct the Select and Start buttons
        if (SDL_JoystickNumButtons(joystick) >= 10) 
        {
            // Directly poll button states to properly handle held buttons
            setButtonState(BUTTON_B, SDL_JoystickGetButton(joystick, 0));
            setButtonState(BUTTON_A, SDL_JoystickGetButton(joystick, 1) || SDL_JoystickGetButton(joystick, 4));
            setButtonState(BUTTON_SELECT, SDL_JoystickGetButton(joystick, 8));  // Changed from 2 to 8
            setButtonState(BUTTON_START, SDL_JoystickGetButton(joystick, 9));   // Changed from 3 to 9
            
            // These are alternatives that might be useful for some SNES controllers
            if (!buttonStates[BUTTON_B]) // Don't override if already set
                setButtonState(BUTTON_B, SDL_JoystickGetButton(joystick, 5));
        }
        
        // Handle D-pad via hat (common for SNES controllers)
        if (SDL_JoystickNumHats(joystick) > 0)
        {
            Uint8 hatState = SDL_JoystickGetHat(joystick, 0);
            
            // Always update D-pad buttons based on current hat state
            setButtonState(BUTTON_UP, (hatState & SDL_HAT_UP) != 0);
            setButtonState(BUTTON_DOWN, (hatState & SDL_HAT_DOWN) != 0);
            setButtonState(BUTTON_LEFT, (hatState & SDL_HAT_LEFT) != 0);
            setButtonState(BUTTON_RIGHT, (hatState & SDL_HAT_RIGHT) != 0);
        }
        // As fallback, also try axes for controllers without hat
        else if (SDL_JoystickNumAxes(joystick) >= 2)
        {
            // X-axis
            Sint16 xAxis = SDL_JoystickGetAxis(joystick, 0);
            if (xAxis < -JOYSTICK_DEADZONE) {
                setButtonState(BUTTON_LEFT, true);
                setButtonState(BUTTON_RIGHT, false);
            }
            else if (xAxis > JOYSTICK_DEADZONE) {
                setButtonState(BUTTON_RIGHT, true);
                setButtonState(BUTTON_LEFT, false);
            }
            else {
                setButtonState(BUTTON_LEFT, false);
                setButtonState(BUTTON_RIGHT, false);
            }
            
            // Y-axis
            Sint16 yAxis = SDL_JoystickGetAxis(joystick, 1);
            if (yAxis < -JOYSTICK_DEADZONE) {
                setButtonState(BUTTON_UP, true);
                setButtonState(BUTTON_DOWN, false);
            }
            else if (yAxis > JOYSTICK_DEADZONE) {
                setButtonState(BUTTON_DOWN, true);
                setButtonState(BUTTON_UP, false);
            }
            else {
                setButtonState(BUTTON_UP, false);
                setButtonState(BUTTON_DOWN, false);
            }
        }
    }
}

void Controller::printButtonStates() const
{
    printf("Button States: A:%d B:%d Select:%d Start:%d Up:%d Down:%d Left:%d Right:%d\n",
           buttonStates[BUTTON_A], buttonStates[BUTTON_B], 
           buttonStates[BUTTON_SELECT], buttonStates[BUTTON_START],
           buttonStates[BUTTON_UP], buttonStates[BUTTON_DOWN], 
           buttonStates[BUTTON_LEFT], buttonStates[BUTTON_RIGHT]);
}

void Controller::setupRetrolinkMapping()
{
    // Simple debug message about controller mapping
    std::cout << "Setting up Retrolink SNES Controller mapping..." << std::endl;
    
    // We'll handle mappings in the event handlers and updateJoystickState instead
    // of using SDL's mapping system, for better compatibility
}

void Controller::mapJoystickButtonToController(int button, ControllerButton nesButton)
{
    // Log button presses for debugging
    std::cout << "Button pressed: " << button << std::endl;
    
    // Map based on Retrolink SNES layout specifically for your controller
    switch (button)
    {
        case 0: setButtonState(BUTTON_B, true); break;      // SNES B -> NES B
        case 1: setButtonState(BUTTON_A, true); break;      // SNES Y -> NES A
        case 8: setButtonState(BUTTON_SELECT, true); break; // SNES Select -> NES Select
        case 9: setButtonState(BUTTON_START, true); break;  // SNES Start -> NES Start
        case 4: setButtonState(BUTTON_A, true); break;      // SNES A -> NES A (alternate)
        case 5: setButtonState(BUTTON_B, true); break;      // SNES X -> NES B (alternate)
        default: break;
    }
}
