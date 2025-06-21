#include "Controller.hpp"
#include <iostream>

// Keyboard mappings
const Controller::KeyboardMapping Controller::player1Keys = {
    SDL_SCANCODE_UP,     // up
    SDL_SCANCODE_DOWN,   // down
    SDL_SCANCODE_LEFT,   // left
    SDL_SCANCODE_RIGHT,  // right
    SDL_SCANCODE_X,      // a
    SDL_SCANCODE_Z,      // b
    SDL_SCANCODE_RSHIFT, // select
    SDL_SCANCODE_RETURN  // start
};

const Controller::KeyboardMapping Controller::player2Keys = {
    SDL_SCANCODE_I,      // up
    SDL_SCANCODE_K,      // down
    SDL_SCANCODE_J,      // left
    SDL_SCANCODE_L,      // right
    SDL_SCANCODE_N,      // a
    SDL_SCANCODE_M,      // b
    SDL_SCANCODE_RCTRL,  // select
    SDL_SCANCODE_SPACE   // start
};

Controller::Controller() : strobe(1), joystickPollingEnabled(true)
{
    // Initialize button states for both players
    for (int player = 0; player < 2; player++)
    {
        for (int button = 0; button < 8; button++)
        {
            buttonStates[player][button] = false;
        }
        buttonIndex[player] = 0;
        joysticks[player] = nullptr;
        gameControllers[player] = nullptr;
        joystickIDs[player] = -1;
        joystickInitialized[player] = false;
    }
    
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
    for (int player = 0; player < 2; player++)
    {
        if (gameControllers[player] != nullptr)
        {
            SDL_GameControllerClose(gameControllers[player]);
        }
        else if (joysticks[player] != nullptr)
        {
            SDL_JoystickClose(joysticks[player]);
        }
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
    
    // Set up controller mappings
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

    // Try to open joysticks for up to 2 players
    int playersInitialized = 0;
    for (int i = 0; i < numJoysticks && playersInitialized < 2; i++)
    {
        Player player = static_cast<Player>(playersInitialized);
        
        // First try to open as game controller (more standardized)
        if (SDL_IsGameController(i))
        {
            gameControllers[player] = SDL_GameControllerOpen(i);
            if (gameControllers[player])
            {
                std::cout << "Player " << (player + 1) << " gamepad: " 
                          << SDL_GameControllerName(gameControllers[player]) << std::endl;
                joysticks[player] = SDL_GameControllerGetJoystick(gameControllers[player]);
                joystickIDs[player] = SDL_JoystickInstanceID(joysticks[player]);
                joystickInitialized[player] = true;
                playersInitialized++;
                continue;
            }
        }

        // If no game controller is available, try as generic joystick
        joysticks[player] = SDL_JoystickOpen(i);
        if (joysticks[player])
        {
            std::cout << "Player " << (player + 1) << " joystick: " 
                      << SDL_JoystickName(joysticks[player]) << std::endl;
            joystickIDs[player] = SDL_JoystickInstanceID(joysticks[player]);
            joystickInitialized[player] = true;
            
            // Debug: Print number of buttons, axes, etc.
            std::cout << "Joystick has " << SDL_JoystickNumButtons(joysticks[player]) 
                      << " buttons and " << SDL_JoystickNumAxes(joysticks[player]) 
                      << " axes." << std::endl;
            
            playersInitialized++;
        }
        else
        {
            std::cerr << "Could not open joystick " << i << ": " << SDL_GetError() << std::endl;
        }
    }

    return playersInitialized > 0;
}

uint8_t Controller::readByte(Player player)
{
    uint8_t value = 1;

    if (buttonIndex[player] < 8)
    {
        value = (buttonStates[player][buttonIndex[player]] ? 0x41 : 0x40);
    }

    if ((strobe & (1 << 0)) == 0)
    {
        buttonIndex[player]++;
    }

    return value;
}

void Controller::setButtonState(Player player, ControllerButton button, bool state)
{
    buttonStates[player][(int)button] = state;
}

bool Controller::getButtonState(Player player, ControllerButton button) const
{
    return buttonStates[player][(int)button];
}

void Controller::writeByte(uint8_t value)
{
    if ((value & (1 << 0)) == 0 && (strobe & (1 << 0)) == 1)
    {
        buttonIndex[PLAYER_1] = 0;
        buttonIndex[PLAYER_2] = 0;
    }
    strobe = value;
}

// Backward compatibility methods
void Controller::setButtonState(ControllerButton button, bool state)
{
    setButtonState(PLAYER_1, button, state);
}

bool Controller::getButtonState(ControllerButton button) const
{
    return getButtonState(PLAYER_1, button);
}

uint8_t Controller::readByte()
{
    return readByte(PLAYER_1);
}

void Controller::processKeyboardEvent(const SDL_Event& event)
{
    bool pressed = (event.type == SDL_KEYDOWN);
    SDL_Scancode key = event.key.keysym.scancode;

    // Handle Player 1 keyboard input
    if (key == player1Keys.up)         setButtonState(PLAYER_1, BUTTON_UP, pressed);
    else if (key == player1Keys.down)  setButtonState(PLAYER_1, BUTTON_DOWN, pressed);
    else if (key == player1Keys.left)  setButtonState(PLAYER_1, BUTTON_LEFT, pressed);
    else if (key == player1Keys.right) setButtonState(PLAYER_1, BUTTON_RIGHT, pressed);
    else if (key == player1Keys.a)     setButtonState(PLAYER_1, BUTTON_A, pressed);
    else if (key == player1Keys.b)     setButtonState(PLAYER_1, BUTTON_B, pressed);
    else if (key == player1Keys.select) setButtonState(PLAYER_1, BUTTON_SELECT, pressed);
    else if (key == player1Keys.start) setButtonState(PLAYER_1, BUTTON_START, pressed);
    
    // Handle Player 2 keyboard input
    else if (key == player2Keys.up)    setButtonState(PLAYER_2, BUTTON_UP, pressed);
    else if (key == player2Keys.down)  setButtonState(PLAYER_2, BUTTON_DOWN, pressed);
    else if (key == player2Keys.left)  setButtonState(PLAYER_2, BUTTON_LEFT, pressed);
    else if (key == player2Keys.right) setButtonState(PLAYER_2, BUTTON_RIGHT, pressed);
    else if (key == player2Keys.a)     setButtonState(PLAYER_2, BUTTON_A, pressed);
    else if (key == player2Keys.b)     setButtonState(PLAYER_2, BUTTON_B, pressed);
    else if (key == player2Keys.select) setButtonState(PLAYER_2, BUTTON_SELECT, pressed);
    else if (key == player2Keys.start) setButtonState(PLAYER_2, BUTTON_START, pressed);
}

void Controller::processJoystickEvent(const SDL_Event& event)
{
    Player player = getPlayerFromJoystickID(event.jaxis.which);
    if (player == PLAYER_1 && !joystickInitialized[PLAYER_1] ||
        player == PLAYER_2 && !joystickInitialized[PLAYER_2])
        return;

    switch (event.type)
    {
        case SDL_JOYAXISMOTION:
            if (player != PLAYER_1 && player != PLAYER_2) return;
            handleJoystickAxis(player, event.jaxis.axis, event.jaxis.value);
            break;

        case SDL_JOYHATMOTION:
            if (player != PLAYER_1 && player != PLAYER_2) return;
            {
                Uint8 hatValue = event.jhat.value;
                setButtonState(player, BUTTON_UP, (hatValue & SDL_HAT_UP) != 0);
                setButtonState(player, BUTTON_DOWN, (hatValue & SDL_HAT_DOWN) != 0);
                setButtonState(player, BUTTON_LEFT, (hatValue & SDL_HAT_LEFT) != 0);
                setButtonState(player, BUTTON_RIGHT, (hatValue & SDL_HAT_RIGHT) != 0);
            }
            break;

        case SDL_JOYBUTTONDOWN:
            if (player != PLAYER_1 && player != PLAYER_2) return;
            handleJoystickButton(player, event.jbutton.button, true);
            break;

        case SDL_JOYBUTTONUP:
            if (player != PLAYER_1 && player != PLAYER_2) return;
            handleJoystickButton(player, event.jbutton.button, false);
            break;

        case SDL_CONTROLLERBUTTONDOWN:
            if (player != PLAYER_1 && player != PLAYER_2) return;
            handleControllerButton(player, (SDL_GameControllerButton)event.cbutton.button, true);
            break;

        case SDL_CONTROLLERBUTTONUP:
            if (player != PLAYER_1 && player != PLAYER_2) return;
            handleControllerButton(player, (SDL_GameControllerButton)event.cbutton.button, false);
            break;

        case SDL_CONTROLLERAXISMOTION:
            if (player != PLAYER_1 && player != PLAYER_2) return;
            handleControllerAxis(player, (SDL_GameControllerAxis)event.caxis.axis, event.caxis.value);
            break;
    }
}

void Controller::updateJoystickState()
{
    // Skip polling if disabled
    if (!joystickPollingEnabled)
        return;
        
    // This method is now mainly for compatibility
    // Most joystick input should be handled through events in processJoystickEvent()
    // Only poll for controllers that don't send proper events
    
    for (int p = 0; p < 2; p++)
    {
        Player player = static_cast<Player>(p);
        
        if (!joystickInitialized[player] || !joysticks[player])
            continue;

        // Only poll if this is NOT a game controller (game controllers use events)
        if (gameControllers[player] != nullptr)
            continue;
            
        // For non-game controllers, poll button states but don't override keyboard
        // Check if any joystick input is active before overriding
        bool hasJoystickInput = false;
        
        if (SDL_JoystickNumButtons(joysticks[player]) >= 10) 
        {
            // Check if any buttons are pressed
            for (int btn = 0; btn < SDL_JoystickNumButtons(joysticks[player]); btn++)
            {
                if (SDL_JoystickGetButton(joysticks[player], btn))
                {
                    hasJoystickInput = true;
                    break;
                }
            }
        }
        
        // Check hat/dpad input
        if (SDL_JoystickNumHats(joysticks[player]) > 0)
        {
            Uint8 hatState = SDL_JoystickGetHat(joysticks[player], 0);
            if (hatState != SDL_HAT_CENTERED)
            {
                hasJoystickInput = true;
            }
        }
        
        // Check axis input
        if (SDL_JoystickNumAxes(joysticks[player]) >= 2)
        {
            Sint16 xAxis = SDL_JoystickGetAxis(joysticks[player], 0);
            Sint16 yAxis = SDL_JoystickGetAxis(joysticks[player], 1);
            
            if (abs(xAxis) > JOYSTICK_DEADZONE || abs(yAxis) > JOYSTICK_DEADZONE)
            {
                hasJoystickInput = true;
            }
        }
        
        // Only update joystick state if there's actual joystick input
        // This prevents overriding keyboard input when joystick is idle
        if (hasJoystickInput)
        {
            // Update button states
            if (SDL_JoystickNumButtons(joysticks[player]) >= 10) 
            {
                setButtonState(player, BUTTON_B, SDL_JoystickGetButton(joysticks[player], 0));
                setButtonState(player, BUTTON_A, SDL_JoystickGetButton(joysticks[player], 1) || 
                                                 SDL_JoystickGetButton(joysticks[player], 4));
                setButtonState(player, BUTTON_SELECT, SDL_JoystickGetButton(joysticks[player], 8));
                setButtonState(player, BUTTON_START, SDL_JoystickGetButton(joysticks[player], 9));
                
                if (!buttonStates[player][BUTTON_B])
                    setButtonState(player, BUTTON_B, SDL_JoystickGetButton(joysticks[player], 5));
            }
            
            // Update directional input
            if (SDL_JoystickNumHats(joysticks[player]) > 0)
            {
                Uint8 hatState = SDL_JoystickGetHat(joysticks[player], 0);
                setButtonState(player, BUTTON_UP, (hatState & SDL_HAT_UP) != 0);
                setButtonState(player, BUTTON_DOWN, (hatState & SDL_HAT_DOWN) != 0);
                setButtonState(player, BUTTON_LEFT, (hatState & SDL_HAT_LEFT) != 0);
                setButtonState(player, BUTTON_RIGHT, (hatState & SDL_HAT_RIGHT) != 0);
            }
            else if (SDL_JoystickNumAxes(joysticks[player]) >= 2)
            {
                handleJoystickAxis(player, 0, SDL_JoystickGetAxis(joysticks[player], 0));
                handleJoystickAxis(player, 1, SDL_JoystickGetAxis(joysticks[player], 1));
            }
        }
    }
}

void Controller::printButtonStates() const
{
    for (int p = 0; p < 2; p++)
    {
        printf("Player %d - A:%d B:%d Select:%d Start:%d Up:%d Down:%d Left:%d Right:%d\n",
               p + 1,
               buttonStates[p][BUTTON_A], buttonStates[p][BUTTON_B], 
               buttonStates[p][BUTTON_SELECT], buttonStates[p][BUTTON_START],
               buttonStates[p][BUTTON_UP], buttonStates[p][BUTTON_DOWN], 
               buttonStates[p][BUTTON_LEFT], buttonStates[p][BUTTON_RIGHT]);
    }
}

bool Controller::isJoystickConnected(Player player) const
{
    return joystickInitialized[player];
}

void Controller::setJoystickPolling(bool enabled)
{
    joystickPollingEnabled = enabled;
}

void Controller::setupRetrolinkMapping()
{
    std::cout << "Setting up controller mappings for up to 2 players..." << std::endl;
}

Player Controller::getPlayerFromJoystickID(int joystickID)
{
    if (joystickIDs[PLAYER_1] == joystickID) return PLAYER_1;
    if (joystickIDs[PLAYER_2] == joystickID) return PLAYER_2;
    return PLAYER_1; // Default fallback
}

void Controller::handleJoystickAxis(Player player, int axis, Sint16 value)
{
    if (axis == 0) // X axis
    {
        if (value < -JOYSTICK_DEADZONE)
        {
            setButtonState(player, BUTTON_LEFT, true);
            setButtonState(player, BUTTON_RIGHT, false);
        }
        else if (value > JOYSTICK_DEADZONE)
        {
            setButtonState(player, BUTTON_RIGHT, true);
            setButtonState(player, BUTTON_LEFT, false);
        }
        else
        {
            setButtonState(player, BUTTON_LEFT, false);
            setButtonState(player, BUTTON_RIGHT, false);
        }
    }
    else if (axis == 1) // Y axis
    {
        if (value < -JOYSTICK_DEADZONE)
        {
            setButtonState(player, BUTTON_UP, true);
            setButtonState(player, BUTTON_DOWN, false);
        }
        else if (value > JOYSTICK_DEADZONE)
        {
            setButtonState(player, BUTTON_DOWN, true);
            setButtonState(player, BUTTON_UP, false);
        }
        else
        {
            setButtonState(player, BUTTON_UP, false);
            setButtonState(player, BUTTON_DOWN, false);
        }
    }
}

void Controller::handleJoystickButton(Player player, int button, bool pressed)
{
    switch (button)
    {
        case 0: setButtonState(player, BUTTON_B, pressed); break;
        case 1: setButtonState(player, BUTTON_A, pressed); break;
        case 8: setButtonState(player, BUTTON_SELECT, pressed); break;
        case 9: setButtonState(player, BUTTON_START, pressed); break;
        case 4: setButtonState(player, BUTTON_A, pressed); break;
        case 5: setButtonState(player, BUTTON_B, pressed); break;
        default: break;
    }
}

void Controller::handleControllerButton(Player player, SDL_GameControllerButton button, bool pressed)
{
    switch (button)
    {
        case SDL_CONTROLLER_BUTTON_A:
            setButtonState(player, BUTTON_A, pressed);
            break;
        case SDL_CONTROLLER_BUTTON_B:
            setButtonState(player, BUTTON_B, pressed);
            break;
        case SDL_CONTROLLER_BUTTON_BACK:
            setButtonState(player, BUTTON_SELECT, pressed);
            break;
        case SDL_CONTROLLER_BUTTON_START:
            setButtonState(player, BUTTON_START, pressed);
            break;
        case SDL_CONTROLLER_BUTTON_DPAD_UP:
            setButtonState(player, BUTTON_UP, pressed);
            break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
            setButtonState(player, BUTTON_DOWN, pressed);
            break;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
            setButtonState(player, BUTTON_LEFT, pressed);
            break;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
            setButtonState(player, BUTTON_RIGHT, pressed);
            break;
    }
}

void Controller::handleControllerAxis(Player player, SDL_GameControllerAxis axis, Sint16 value)
{
    if (axis == SDL_CONTROLLER_AXIS_LEFTX)
    {
        handleJoystickAxis(player, 0, value);
    }
    else if (axis == SDL_CONTROLLER_AXIS_LEFTY)
    {
        handleJoystickAxis(player, 1, value);
    }
}
