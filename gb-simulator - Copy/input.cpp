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
    // Bits 6–7 = 1, bits 0–3 = all 1s (unpressed)
    uint8_t buttons = 0xCF;

    if (direction_switch) {
        buttons = (buttons & 0xF0) |    // preserve upper bits
            ((right ? 0 : 1) << 0) |
            ((left ? 0 : 1) << 1) |
            ((up ? 0 : 1) << 2) |
            ((down ? 0 : 1) << 3);
    }
    else if (button_switch) {
        buttons = (buttons & 0xF0) |    // preserve upper bits
            ((a ? 0 : 1) << 0) |
            ((b ? 0 : 1) << 1) |
            ((select ? 0 : 1) << 2) |
            ((start ? 0 : 1) << 3);
    }

    // Set bits 4–5 according to switch flags
    if (!direction_switch) buttons |= (1 << 4);
    else                   buttons &= ~(1 << 4);

    if (!button_switch) buttons |= (1 << 5);
    else                buttons &= ~(1 << 5);

    return buttons;
}
