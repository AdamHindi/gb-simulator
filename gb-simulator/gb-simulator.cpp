// gb-simulator.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include "cpu.hpp"
#include "ppu.hpp"
#include "video.hpp"


#include <iostream>
#include <bitset>
#define SDL_MAIN_HANDLE
int main(int argc, char** argv)
{
	Memory mem;

	if (!mem.loadROM("lz.gb")) {

		return 0;
	}

	CPU cpu(mem.io_registers);
	PPU ppu;
	ppu.connectMemory(&mem);
	ppu.connectVRAM(mem.vramPtr());

	cpu.connectMemory(&mem);
	Video vid;
	bool running = true;


	while (running)
	{
		////print flags for debugging:
		////printf("F: 0x%02X \n", cpu.getflags());
		////print registers for debugging:
		//if ((cpu.getB() == 0x03 && cpu.getC() == 0x05) || (cpu.getB() == 0x42)) {

		//	//printf("A: 0x%02X B: 0x%02X C: 0x%02X D: 0x%02X E: 0x%02X H: 0x%02X L: 0x%02X\n", cpu.getA(), cpu.getB(), cpu.getC(), cpu.getD(), cpu.getE(), cpu.getH(), cpu.getL());
		//}


		cpu.cycle();
		//printf("Cycling");
		for (uint16_t i = 0; i < cpu.getCycles(); ++i) mem.dma.tick();
		ppu.step(cpu.getCycles() * 2);
		running = vid.ProcessInput(mem.input);
		if (ppu.takeFrameReady())
			vid.present(ppu.framebuffer);

	}
	std::cout << "CPU halted. Test ROM finished.\n";
	// Optionally dump serial output (0xFF01)
	std::cout << "Serial output:\n";
	for (char c : mem.serialBuffer) {
		std::cout << c;
	}
	
	return 0;
}
