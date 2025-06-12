#pragma once

#include <vector>
#include <cstdint>
#include <memory>
#include <string>

// ---------------- MBC Interface ----------------

class MBC {
public:
    virtual uint8_t readROM(uint16_t addr) = 0;
    virtual void    writeROM(uint16_t addr, uint8_t value) = 0;

    virtual uint8_t readRAM(uint16_t addr) = 0;
    virtual void    writeRAM(uint16_t addr, uint8_t value) = 0;

    virtual void    loadROM(const std::vector<uint8_t>& romData) = 0;
    virtual void    loadRAM(std::vector<uint8_t>& ramData) = 0;

    virtual ~MBC() = default;
};
// ------------------- NO MBC ------------------------------//
class NoMBC : public MBC {
public:
    void loadROM(const std::vector<uint8_t>& romData) override {
        rom = romData;
    }

    void loadRAM(std::vector<uint8_t>&) override {}

    uint8_t readROM(uint16_t addr) override {
        return rom[addr];
    }

    void writeROM(uint16_t, uint8_t) override {}

    uint8_t readRAM(uint16_t) override { return 0xFF; }
    void writeRAM(uint16_t, uint8_t) override {}

private:
    std::vector<uint8_t> rom;
};


// ---------------- MBC1 ----------------

class MBC1 : public MBC {
public:
    MBC1();
    void loadROM(const std::vector<uint8_t>& romData) override;
    void loadRAM(std::vector<uint8_t>& ramData) override;

    uint8_t readROM(uint16_t addr) override;
    void    writeROM(uint16_t addr, uint8_t value) override;

    uint8_t readRAM(uint16_t addr) override;
    void    writeRAM(uint16_t addr, uint8_t value) override;

private:
    std::vector<uint8_t> rom;
    std::vector<uint8_t> ram;

    bool ramEnabled = false;
    uint8_t romBankLow = 1;
    uint8_t romBankHigh = 0;
    bool bankingMode = false; // false = ROM mode, true = RAM mode

    uint8_t currentROMBank() const;
    uint8_t currentRAMBank() const;
};

// ---------------- Cartridge Wrapper ----------------

class Cartridge {
public:
    bool loadFromFile(const std::string& path);
    std::shared_ptr<MBC> getMBC() const;

private:
    std::shared_ptr<MBC> mbc;
    std::vector<uint8_t> rom;
    std::vector<uint8_t> ram;
};
