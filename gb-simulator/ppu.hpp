#pragma once 

#include <cstdint>
#include <functional>

class Memory; // Forward declaration of Memory class


class PPU {
public:

    PPU() {
        for (int i = 0; i < 144; ++i) {
            for (int j = 0; j < 160; ++j) {
                framebuffer[i][j] = 0xFFFFFFFF; // Initialize all pixels to white
            }
		}
    };

    void connectVRAM(uint8_t* vramPtr) { vram = vramPtr; }
    void connectOAM(uint8_t* oamPtr) { oam = oamPtr; }
    void connectIO(uint8_t* ioRegsPtr) { io = ioRegsPtr; }

	void connectMemory(Memory* memoryPtr) { memory = memoryPtr; }
    void reset();
    void step(int ticks);



    const uint32_t* framebufferData() const {
        return  &framebuffer[0][0];         // decays to &framebuffer[0]
    }
    bool takeFrameReady() { bool f = frameReady; frameReady = false; return f; }

    uint32_t framebuffer[144][160];
private:
    enum IOReg : uint8_t {
        LCDC = 0x40,
        STAT = 0x41,
        SCY = 0x42,
        SCX = 0x43,
        LY = 0x44,
        LYC = 0x45,
        DMA = 0x46,
        BGP = 0x47,
        OBP0 = 0x48,
        OBP1 = 0x49
    };
    int windowLine = -1;   // -1 means window not started yet this frame

    bool frameReady = false;
     // Framebuffer for 144 lines of 160 pixels each

    uint8_t scanline[160] {};
	void checkCoincidence(); // Check LY == LYC and update STAT register
    int lastM3Length = 172;

    // --- Pointers into main memory regions (no full Memory*) ---
    uint8_t* vram = nullptr;  // 0x8000–0x9FFF
    uint8_t* oam = nullptr;  // 0xFE00–0xFE9F
    uint8_t* io = nullptr;  // base of io_registers[0] == FF00
    uint8_t read8(uint16_t addr) const; 

	Memory* memory = nullptr; // Pointer to the main memory (not used directly in this class)

    int modeClock = 0;
    int currentLine = 0;


    enum PPU_MODE {
        OAM_SEARCH = 2,
        DRAWING = 3,
        HBLANK = 0,
        VBLANK = 1
    };

    uint8_t mode = OAM_SEARCH;



    int calculateDrawPenalties() const;
	void renderScanline();
	void scanlineBackground();
	void scanlineSprite();
	void scanlineWindow();
    int spriteHeight() const;
    bool inOAMRestrictedMode() const;


    uint32_t mapColor(uint8_t pix) const;
    void enterMode(uint8_t newMode, int carry);
};


