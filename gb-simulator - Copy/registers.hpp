#pragma once

#include <cstdint>

// Flags bit definitions
#define FLAG_Z 0x80 // Zero flag
#define FLAG_N 0x40 // Subtract flag
#define FLAG_H 0x20 // Half carry flag
#define FLAG_C 0x10 // Carry flag


// Register structure
struct Registers {

	union {
		struct {
			uint8_t f; // Flags
			uint8_t a; // Accumulator
		};
		uint16_t af; // Combined AF register
	};
	union {
		struct {
			uint8_t c; // Register C
			uint8_t b; // Register B
		};
		uint16_t bc; // Combined BC register
	};
	union {
		struct {
			uint8_t e; // Register E
			uint8_t d; // Register D
		};
		uint16_t de; // Combined DE register
	};
	union {
		struct {
			uint8_t l; // Register L
			uint8_t h; // Register H
		};
		uint16_t hl; // Combined HL register
	};
	
	void setFlags(uint8_t flag, uint8_t value);
	bool getFlag(uint8_t flagMask) const; 
};

//Function to set the flags based on a value
inline void Registers::setFlags(uint8_t flag, uint8_t value) {
	if (value == 0) {
		f |= FLAG_Z; // Set Zero flag
	} else {
		f &= ~FLAG_Z; // Clear Zero flag
	}
	f = (f & ~(FLAG_N | FLAG_H | FLAG_C)) | (flag & (FLAG_N | FLAG_H | FLAG_C)); // Set or clear other flags
}

// Function to get the value of a specific flag
inline bool Registers::getFlag(uint8_t flagMask) const {
	return f & flagMask;
}

