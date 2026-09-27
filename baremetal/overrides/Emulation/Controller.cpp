#include "Controller.hpp"

Controller::Controller() : buttonStates{}, buttonIndex{}, strobe(0) {}

uint8_t Controller::readByte(Player player)
{
    uint8_t value = 1;
    if (buttonIndex[player] < 8)
        value = buttonStates[player][buttonIndex[player]] ? 0x41 : 0x40;
    if ((strobe & 1) == 0)
        buttonIndex[player]++;
    return value;
}

void Controller::writeByte(uint8_t value)
{
    if ((value & 1) == 0 && (strobe & 1) == 1) {
        buttonIndex[PLAYER_1] = 0;
        buttonIndex[PLAYER_2] = 0;
    }
    strobe = value;
}

void Controller::setButtonState(Player player, ControllerButton button, bool state)
{
    buttonStates[player][(int)button] = state;
}

bool Controller::getButtonState(Player player, ControllerButton button) const
{
    return buttonStates[player][(int)button];
}
