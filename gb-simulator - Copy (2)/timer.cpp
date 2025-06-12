#include "timer.hpp"



void Timer::update(uint16_t cycles, bool stopped)
{
    if (stopped) return;

    while (cycles--) {
        prev_divider = divider;
        ++divider;

        // update DIV high byte if needed
        if ((divider & 0xFF00) != (prev_divider & 0xFF00))
            io_registers[0x04] = divider >> 8;

        // delayed reload handling (exactly as you have now) --------------
        if (tima_overflow) {
            if (--overflow_delay == 0) {
                tima_overflow = false;
                tima_blocked = false;
                io_registers[0x05] = io_registers[0x06];
                io_registers[0x0F] |= 1 << 2; // TIMER IRQ
            }
            continue;
        }
        //----------------------------------------------------------------

        uint8_t tac = io_registers[0x07];
        if (!(tac & 0x04)) continue;           // timer disabled

        static constexpr int bit_idx[4] = { 9, 3, 5, 7 };
        int sel = tac & 0x03;
        bool old_bit = (prev_divider >> bit_idx[sel]) & 1;
        bool new_bit = (divider >> bit_idx[sel]) & 1;

        if (old_bit && !new_bit && !tima_blocked) {
            uint8_t& TIMA = io_registers[0x05];
            if (TIMA == 0xFF) {
                TIMA = 0;
                tima_overflow = true;
                tima_blocked = true;
                overflow_delay = 4;
            }
            else {
                ++TIMA;
            }
        }
    }
}


void Timer::resetDIV() {
    uint16_t old_div = divider;

    uint8_t tac = io_registers[0x07];
    if (tac & 0x04) {
        static constexpr int bit_idx[4] = { 9, 3, 5, 7 };
        int sel = tac & 0x03;
        bool old_bit = (old_div >> bit_idx[sel]) & 1;
        bool new_bit = 0; // divider becomes 0

        if (old_bit && !new_bit) {
            uint8_t& TIMA = io_registers[0x05];
            std::cout << "DIV reset: old_div=" << std::hex << old_div
                << " TIMA=" << (int)io_registers[0x05] << "\n";
            if (TIMA == 0xFF) {
                // Immediate overflow behavior (no 4-cycle delay on DIV reset)
                TIMA = io_registers[0x06];
                io_registers[0x0F] |= (1 << 2); // request TIMER interrupt
            }
            else {
                TIMA++;
            }
        }
    }

    divider = 0;
    prev_divider = 0;
    io_registers[0x04] = 0;
}


void Timer::writeTIMA(uint8_t v) {
    if (tima_overflow) {
		if (overflow_delay >= 1) { // if overflow delay is still active, ignore write
            tima_overflow = false;      // cancel reload / IRQ
            tima_blocked = false;      // UNBLOCK so next edge works
            overflow_delay = 0;         // reset delay
            io_registers[0x05] = v;
        }
		return; // ignore write if overflow is pending
    }
	io_registers[0x05] = v;// write TIMA directly

}
void Timer::writeTMA(uint8_t v) { io_registers[0x06] = v; }
void Timer::writeTAC(uint8_t v) {
    io_registers[0x07] = v & 0x07;
}