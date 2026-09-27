#pragma once
// Bare-metal controller: same NES-side behavior as the original, no SDL.
#include <cstdint>

enum ControllerButton {
    BUTTON_A = 0, BUTTON_B = 1, BUTTON_SELECT = 2, BUTTON_START = 3,
    BUTTON_UP = 4, BUTTON_DOWN = 5, BUTTON_LEFT = 6, BUTTON_RIGHT = 7
};
#ifndef PLAYER_ENUM_DEFINED
#define PLAYER_ENUM_DEFINED
enum Player { PLAYER_1 = 0, PLAYER_2 = 1 };
#endif

class Controller {
public:
    Controller();
    uint8_t readByte(Player player);
    uint8_t readByte() { return readByte(PLAYER_1); }
    void writeByte(uint8_t value);
    void setButtonState(Player player, ControllerButton button, bool state);
    void setButtonState(ControllerButton button, bool state) { setButtonState(PLAYER_1, button, state); }
    bool getButtonState(Player player, ControllerButton button) const;
private:
    bool buttonStates[2][8];
    uint8_t buttonIndex[2];
    uint8_t strobe;
};
