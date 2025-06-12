// Cartridge.cpp
#include "Cartridge.hpp"
#include <fstream>
#include <iostream>
#include <cassert>

// -------- MBC1 Implementation --------

MBC1::MBC1() {}

void MBC1::loadROM(const std::vector<uint8_t>& romData) {
    rom = romData;
}

void MBC1::loadRAM(std::vector<uint8_t>& ramData) {
    ram = ramData;
    if (ram.empty()) ram.resize(0x8000); // Support up to 4 banks
}

uint8_t MBC1::readROM(uint16_t addr) {
    if (addr < 0x4000) {
        return rom[addr];
    }
    else {
        uint32_t bank = currentROMBank();
        uint32_t offset = (bank * 0x4000) + (addr - 0x4000);
        return rom[offset % rom.size()];
    }
}

void MBC1::writeROM(uint16_t addr, uint8_t value) {
    if (addr < 0x2000) {
        ramEnabled = (value & 0x0F) == 0x0A;
    }
    else if (addr < 0x4000) {
        romBankLow = value & 0x1F;
        if (romBankLow == 0) romBankLow = 1;
    }
    else if (addr < 0x6000) {
        romBankHigh = value & 0x03;
    }
    else if (addr < 0x8000) {
        bankingMode = value & 0x01;
    }
}

uint8_t MBC1::readRAM(uint16_t addr) {
    if (!ramEnabled || ram.empty()) return 0xFF;
    uint32_t bank = currentRAMBank();
    uint32_t offset = bank * 0x2000 + (addr - 0xA000);
    return ram[offset % ram.size()];
}

void MBC1::writeRAM(uint16_t addr, uint8_t value) {
    if (!ramEnabled || ram.empty()) return;
    uint32_t bank = currentRAMBank();
    uint32_t offset = bank * 0x2000 + (addr - 0xA000);
    ram[offset % ram.size()] = value;
}

uint8_t MBC1::currentROMBank() const {
    return bankingMode ? ((romBankHigh << 5) | romBankLow) : (romBankLow);
}

uint8_t MBC1::currentRAMBank() const {
    return bankingMode ? romBankHigh : 0;
}

// -------- Cartridge Loader --------

bool Cartridge::loadFromFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        printf("not found");
        return false;
    }

    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0);

    rom.resize(size);
    file.read(reinterpret_cast<char*>(rom.data()), size);

    uint8_t mbcType = rom[0x147];
    switch (mbcType) {
    case 0x00:
        std::cout << "ROM uses No MBC\n";
        mbc = std::make_shared<NoMBC>();
        break;
    case 0x01: case 0x02: case 0x03:
        mbc = std::make_shared<MBC1>();
        break;
    default:
        std::cerr << "Unsupported MBC type: " << std::hex << (int)mbcType << "\n";
        return false;
    }

    mbc->loadROM(rom);
    mbc->loadRAM(ram);
    return true;
}

std::shared_ptr<MBC> Cartridge::getMBC() const {
    return mbc;
}
