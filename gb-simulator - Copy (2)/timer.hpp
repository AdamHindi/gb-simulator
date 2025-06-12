#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <iostream>


class Timer {
public:
    Timer(uint8_t* io_registers) : io_registers(io_registers), divider(0) { };
    
    void update(uint16_t cycles, bool stopped);

    void resetDIV();
    void writeTIMA(uint8_t v);
    void writeTMA(uint8_t v);
	void writeTAC(uint8_t v);

    

private:

    uint8_t* io_registers;
    bool tima_overflow = false;
    int overflow_delay = 0;
    uint16_t  divider;   // full 16-bit divider
    uint16_t prev_divider = 0;

	bool tima_blocked = false; // Prevents TIMA from incrementing on next falling edge
};

