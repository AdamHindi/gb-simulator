#pragma once
#include <cstdint>

class Memory;

class DMA {
public:
	void start(uint8_t highByte, Memory* mem);
	void tick();
	bool isActive() const { return active; }

private:
	Memory* memory = nullptr;
	uint16_t source = 0;
	uint16_t progress = 0;      // bytes copied so far (0-159)
	bool active = false;
};
