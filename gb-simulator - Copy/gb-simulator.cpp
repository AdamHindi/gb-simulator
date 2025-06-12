// gb-simulator.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include "cpu.hpp"
#include "ppu.hpp"
#include "video.hpp"


#include <iostream>
#include <bitset>

int main(int argc, char** argv)
{
	Memory mem;

	mem.loadROM("dmario.gb");
	
	CPU cpu(mem.io_registers);
	PPU ppu;
	ppu.connectMemory(&mem);

	cpu.connectMemory(&mem);
	Video vid;
	bool running = true;


	while (!cpu.isStopped() || 1==1)
	{
		//print flags for debugging:
		//printf("F: 0x%02X \n", cpu.getflags());
		//print registers for debugging:
		if ((cpu.getB() == 0x03 && cpu.getC() == 0x05) || (cpu.getB()==0x42)) {

			//printf("A: 0x%02X B: 0x%02X C: 0x%02X D: 0x%02X E: 0x%02X H: 0x%02X L: 0x%02X\n", cpu.getA(), cpu.getB(), cpu.getC(), cpu.getD(), cpu.getE(), cpu.getH(), cpu.getL());
		}
		

		cpu.cycle();
		//printf("Cycling");
		for (uint16_t i = 0; i < cpu.getCycles(); ++i) mem.dma.tick();
		ppu.step(cpu.getCycles());
		if (ppu.takeFrameReady())
			vid.present(ppu.framebuffer);
		running = vid.ProcessInput(mem.input);
	}
	std::cout << "CPU halted. Test ROM finished.\n";
	// Optionally dump serial output (0xFF01)
	std::cout << "Serial output:\n";
	for (char c : mem.serialBuffer) {
		std::cout << c;
	}
	std::cout << "\n";
	// pause for user to see output
	std::cout << "Press Enter to exit...\n";
	std::cin.get();

    std::cout << "Hello World!\n";
	return 0;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
