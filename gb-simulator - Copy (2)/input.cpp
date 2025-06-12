#include "input.hpp"



void Input::button_pressed(GbButton button) {
    set_button(button, true);
}

void Input::button_released(GbButton button) {
    set_button(button, false);
}

void Input::set_button(GbButton button, bool set) {
    if (button == GbButton::Up) { up = set; }
    if (button == GbButton::Down) { down = set; }
    if (button == GbButton::Left) { left = set; }
    if (button == GbButton::Right) { right = set; }
    if (button == GbButton::A) { a = set; }
    if (button == GbButton::B) { b = set; }
    if (button == GbButton::Select) { select = set; }
    if (button == GbButton::Start) { start = set; }
}

void Input::write(uint8_t set) {
    direction_switch = (set & (1 << 4)) == 0;
    button_switch = (set & (1 << 5)) == 0;
}

uint8_t Input::get_input() const {
    // Start with all buttons released (bits 0–3 = 1), and bits 6–7 = 1
    uint8_t buttons = 0xCF;

    // If direction line selected (P14 = 0)
    if (direction_switch) {
        if (right) buttons &= ~(1 << 0); else buttons |= (1 << 0);
        if (left)  buttons &= ~(1 << 1); else buttons |= (1 << 1);
        if (up)    buttons &= ~(1 << 2); else buttons |= (1 << 2);
        if (down)  buttons &= ~(1 << 3); else buttons |= (1 << 3);
    }

    // If button line selected (P15 = 0)
    if (button_switch) {
        if (a)      buttons &= ~(1 << 0); else buttons |= (1 << 0);
        if (b)      buttons &= ~(1 << 1); else buttons |= (1 << 1);
        if (select) buttons &= ~(1 << 2); else buttons |= (1 << 2);
        if (start)  buttons &= ~(1 << 3); else buttons |= (1 << 3);
    }

    // Bit 4 = ~direction_switch
    if (!direction_switch) buttons |= (1 << 4);
    else                   buttons &= ~(1 << 4);

    // Bit 5 = ~button_switch
    if (!button_switch)    buttons |= (1 << 5);
    else                   buttons &= ~(1 << 5);

    return buttons;
}
