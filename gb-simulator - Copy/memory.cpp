#include "memory.hpp"

#include <fstream>

#include <iostream>

void Memory::reset()
{

    /*  Joypad  Serial */
    io_registers[0x00] = 0x0F;   // JOYP
    io_registers[0x01] = 0x00;   // SB
    io_registers[0x02] = 0x7E;   // SC
    io_registers[0x03] = 0xFF;   // (unused)

    /* Timer */
    io_registers[0x04] = 0xAB;   // DIV  (random hi-byte)
    io_registers[0x05] = 0x00;   // TIMA
    io_registers[0x06] = 0x00;   // TMA
    io_registers[0x07] = 0xF8;   // TAC (timer off, upper bits 1)

    /*  Sound (power-on DMG values) */
    io_registers[0x10] = 0x80;   // NR10
    io_registers[0x11] = 0xBF;   // NR11
    io_registers[0x12] = 0xF3;   // NR12
    io_registers[0x13] = 0xFF;   // NR13
    io_registers[0x14] = 0xBF;   // NR14

    io_registers[0x16] = 0x3F;   // NR21
    io_registers[0x17] = 0x00;   // NR22
    io_registers[0x18] = 0xFF;   // NR23
    io_registers[0x19] = 0xBF;   // NR24

    io_registers[0x1A] = 0x7F;   // NR30
    io_registers[0x1B] = 0xFF;   // NR31
    io_registers[0x1C] = 0x9F;   // NR32
    io_registers[0x1D] = 0xFF;   // NR33
    io_registers[0x1E] = 0xBF;   // NR34

    io_registers[0x20] = 0xFF;   // NR41
    io_registers[0x21] = 0x00;   // NR42
    io_registers[0x22] = 0x00;   // NR43
    io_registers[0x23] = 0xBF;   // NR44

    io_registers[0x24] = 0x77;   // NR50
    io_registers[0x25] = 0xF3;   // NR51
    io_registers[0x26] = 0xF1;   // NR52  (DMG: sound on, all channels off)

    /* LCD / PPU */
    io_registers[0x40] = 0x91;   // LCDC
    io_registers[0x41] = 0x81;   // STAT (mode-1, LY=LYC int off)
    io_registers[0x42] = 0x00;   // SCY
    io_registers[0x43] = 0x00;   // SCX
    io_registers[0x44] = 0x91;   // LY
    io_registers[0x45] = 0x00;   // LYC
    io_registers[0x46] = 0xFF;   // DMA
    io_registers[0x47] = 0xFC;   // BGP
    io_registers[0x48] = 0xFF;   // OBP0
    io_registers[0x49] = 0xFF;   // OBP1
    io_registers[0x4A] = 0x00;   // WY
    io_registers[0x4B] = 0x00;   // WX

    
    interrupt_enable = 0x00;    // 
}



void Memory::write(uint16_t address,uint8_t value) {
    
    if ((dma.isActive()) && address >= 0xFE00 && address <= 0xFE9F)
        return ;
    if (address < 0x8000) {
        // Normally this would trigger an MBC write (ROM banking)
        // For now, ignore writes to ROM
        return;
    }
    else if (address < 0xA000) {
        vram[address - 0x8000] = value;
    }
    else if (address < 0xC000) {
        ram[address - 0xA000] = value;
    }
    else if (address < 0xE000) {
        wram[address - 0xC000] = value;
    }
    else if (address < 0xFE00) {
        wram[address - 0xE000] = value;  // Echo RAM
    }
    else if (address < 0xFF00) {
        
        oam[address - 0xFE00] = value;
    }
    else if (address < 0xFF80) {
        if (address == 0xFF00) {
            input.write(value);
            return;
        }
        if (address == 0xFF46) { // DMA transfer
            dma.start(value, this);
            io_registers[0x46] = value;
            return;
        }
        uint16_t offset = address - 0xFF00;

        // Serial output triggered
        if (address == 0xFF02 && value == 0x81) {
            char c = static_cast<char>(io_registers[0x01]);  // offset 0x01 = SB (0xFF01)
            std::cout << c << std::flush;
        }

        io_registers[offset] = value;
    }
    else if (address < 0xFFFF) {
        hram[address - 0xFF80] = value;
    }
    else { // 0xFFFF
        interrupt_enable = value;
    }
}

uint8_t Memory::read(uint16_t address) const {
    if ((dma.isActive()) && address >= 0xFE00 && address <= 0xFE9F)
        return 0xFF;
    if (address < 0x8000) {
        
        return rom[address];
    }
    else if (address < 0xA000) {
        return vram[address - 0x8000];
    }
    else if (address < 0xC000) {
        return ram[address - 0xA000];
    }
    else if (address < 0xE000) {
        return wram[address - 0xC000];
    }
    else if (address < 0xFE00) {
        return wram[address - 0xE000];  // Echo RAM (mirror of WRAM)
    }
    else if (address < 0xFF00) {
        
        return oam[address - 0xFE00];
    }
    else if (address < 0xFF80) {
        if (address == 0xFF00) {
            return input.get_input();
        }
        return io_registers[address - 0xFF00];
    }
    else if (address < 0xFFFF) {
        return hram[address - 0xFF80];  // High RAM (HRAM)
    }
    else { // 0xFFFF
        return interrupt_enable;
    }
}


    void Memory::loadROM(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
			std::cout << "Error: Could not open ROM file at " <<std::endl;
        }

        // Load file contents into vector
        rom = std::vector<uint8_t>(
            std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>()
        );

        // Optionally pad to 0x8000 if needed (for no-MBC cartridges)
        if (rom.size() < 0x8000) {
            printf("weird shit");
            rom.resize(0x8000);
        }
    }