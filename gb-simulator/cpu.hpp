#pragma once

#include <cstdint>
#include <chrono>
#include <random>

#include "timer.hpp"
#include "registers.hpp"
#include "memory.hpp"


const unsigned int START_ADDRESS = 0x0100; // Starting address for Chip-8 programs
// in cpu.hpp

// Base opcodes (0x00–0xFF).  
// For conditional instructions, this is the *not-taken* length.
// When the condition is met, use the longer length shown below.
static constexpr uint8_t Tstates[256] = {
	1, 3, 2, 2, 1, 1, 2, 1, 5, 2, 2, 2, 1, 1, 2, 1,
	1, 3, 2, 2, 1, 1, 2, 1, 3, 2, 2, 2, 1, 1, 2, 1,
	2, 3, 2, 2, 1, 1, 2, 1, 2, 2, 2, 2, 1, 1, 2, 1,
	2, 3, 2, 2, 3, 3, 3, 1, 2, 2, 2, 2, 1, 1, 2, 1,
	1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1,
	1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1,
	1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1,
	2, 2, 2, 2, 2, 2, 1, 2, 1, 1, 1, 1, 1, 1, 2, 1,
	1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1,
	1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1,
	1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1,
	1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1,
	2, 3, 3, 4, 3, 4, 2, 4, 2, 4, 3, 0, 3, 6, 2, 4,
	2, 3, 3, 0, 3, 4, 2, 4, 2, 4, 3, 0, 3, 0, 2, 4,
	3, 3, 2, 0, 0, 4, 2, 4, 4, 1, 4, 0, 0, 0, 2, 4,
	3, 3, 2, 1, 0, 4, 2, 4, 3, 2, 4, 1, 0, 0, 2, 4
};

static constexpr uint8_t TstatesConditional[256] = {
	1, 3, 2, 2, 1, 1, 2, 1, 5, 2, 2, 2, 1, 1, 2, 1,
	1, 3, 2, 2, 1, 1, 2, 1, 3, 2, 2, 2, 1, 1, 2, 1,
	3, 3, 2, 2, 1, 1, 2, 1, 3, 2, 2, 2, 1, 1, 2, 1,
	3, 3, 2, 2, 3, 3, 3, 1, 3, 2, 2, 2, 1, 1, 2, 1,
	1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1,
	1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1,
	1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1,
	2, 2, 2, 2, 2, 2, 1, 2, 1, 1, 1, 1, 1, 1, 2, 1,
	1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1,
	1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1,
	1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1,
	1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1,
	5, 3, 4, 4, 6, 4, 2, 4, 5, 4, 4, 0, 6, 6, 2, 4,
	5, 3, 4, 0, 6, 4, 2, 4, 5, 4, 4, 0, 6, 0, 2, 4,
	3, 3, 2, 0, 0, 4, 2, 4, 4, 1, 4, 0, 0, 0, 2, 4,
	3, 3, 2, 1, 0, 4, 2, 4, 3, 2, 4, 1, 0, 0, 2, 4
};

// CB-prefix opcodes (0x00–0xFF under the 0xCB escape):  
// Every register-only op takes 8 cycles; every (HL)-variant (i & 0x07 == 6) takes 16.
static constexpr uint8_t TstatesCB[256] = {
	2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
	2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
	2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
	2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
	2,2,2,2,2,2,3,2,2,2,2,2,2,2,3,2,
	2,2,2,2,2,2,3,2,2,2,2,2,2,2,3,2,
	2,2,2,2,2,2,3,2,2,2,2,2,2,2,3,2,
	2,2,2,2,2,2,3,2,2,2,2,2,2,2,3,2,
	2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
	2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
	2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
	2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
	2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
	2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
	2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
	2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2
};

#undef CB



// Define the CPU class for Game Boy simulator
class CPU {
public:

	CPU(uint8_t* io_regs);
	void cycle();
	void reset();
	void connectMemory(Memory* m);
	bool isStopped() const { return stopped; }
	uint16_t getSP() const { return sp; }
	uint16_t getPC() const { return pc; }
	uint8_t getflags() const { return regs.f; } // Get the flags register
	uint8_t getA() const { return regs.a; } // Get the accumulator register
	uint8_t getB() const { return regs.b; } // Get the B register
	uint8_t getC() const { return regs.c; } // Get the C register
	uint8_t getD() const { return regs.d; } // Get the D register
	uint8_t getE() const { return regs.e; } // Get the E register
	uint8_t getH() const { return regs.h; } // Get the H register
	uint8_t getL() const { return regs.l; } // Get the L register
	void stepTimer(uint16_t c) { timer.update(c, stopped); } // Step the timer with the given cycles
	uint16_t getCycles() const { return cycles; } // Get the number of cycles executed

private:


	bool ime; // Interrupt Master Enable flag
	bool enableInterruptsNext; // Flag to enable interrupts
	bool halted;
	bool stopped; // Flag to indicate if the CPU is stopped
	bool haltbug; // Flag for halt bug

	bool conditional; // Flag to indicate if the last instruction was conditional

	bool interruptInProgress = false;
	int interruptDelay = 0;
	uint16_t interruptVector = 0;

	uint16_t cycles;
	Registers regs;
	uint16_t opcode;          // Current opcode
	uint16_t cbOpcodeTemp;       // Current CB-prefixed opcode
	uint16_t pc;             // Program counter
	uint16_t sp;             // Stack pointer
	Memory* memory; // Pointer to memory
	Timer timer; // Timer for handling timing-related operations



	uint8_t read8(uint16_t addr) const;
	void write8(uint16_t addr, uint8_t value);
	uint8_t inc8(uint8_t val);
	uint8_t dec8(uint8_t val);
	uint8_t add8(uint8_t a, uint8_t b);
	uint8_t sub8(uint8_t a, uint8_t b);
	uint8_t adc8(uint8_t a, uint8_t b);
	uint8_t sbc8(uint8_t a, uint8_t b);
	void cp8(uint8_t a, uint8_t b);
	// Bit manipulation functions

	uint8_t rrc(uint8_t val); // Rotate right with carry
	uint8_t rl(uint8_t val); // Rotate left
	uint8_t rr(uint8_t val); // Rotate right
	uint8_t rlc(uint8_t val); // Rotate left  circular
	uint8_t sla(uint8_t val); // Shift left arithmetic
	uint8_t sra(uint8_t val); // Shift right arithmetic
	uint8_t srl(uint8_t val); // Shift right logical
	uint8_t swap(uint8_t val); // Swap nibbles




	void executeInstruction(); // Execute the current instruction
	void executeCB(uint8_t cbOpcode); // Execute the opcode
	bool interruptPending() const;
	void handleInterrupts(); // Handle interrupts if any are pending
	void clearIFBit(int i); // Clear a specific interrupt flag bit
};



