#include "ppu.hpp"
#include "memory.hpp"
#include <algorithm>



bool PPU::inOAMRestrictedMode() const {
    return mode == 2 || mode == 3;
}


// -----------------------------------------------------------------------------
//  Convert a 2-bit colour index (plus BG/OBJ tags) to 32-bit RGB.
//  Bits 0-1 : colour index 0-3
//  Bit 4    : 0 = BG/Window pixel, 1 = OBJ pixel
//  Bit 5    : (when bit 4 = 1) 0 = OBP0, 1 = OBP1
// -----------------------------------------------------------------------------
uint32_t PPU::mapColor(uint8_t pix) const
{
    // ----- 1.  Select the right palette register -----------------------------
    uint8_t palReg;
    if (pix & 0x10) {                                  // sprite pixel
        palReg = memory->read((pix & 0x20) ? 0xFF49     // OBP1
            : 0xFF48);  // OBP0
    }
    else {                                           // background/window
        palReg = memory->read(0xFF47);                  // BGP
    }

    // ----- 2.  Extract the shade (0-3) from the palette -----------------------
    uint8_t shade = (palReg >> ((pix & 0x03) * 2)) & 0x03;

    // ----- 3.  DMG greens in sRGB (ARGB 0xFFrrggbb) --------------------------
    static constexpr uint32_t dmg[4] = {
        0xFFe0f8d0,   // lightest
        0xFF88c070,
        0xFF346856,
        0xFF081820    // darkest
    };
    return dmg[shade];
}

//------------------------------------------------------------------------------
//  Mode-3 penalty helper (SCX + Window + OBJs) – Pan Docs accurate enough
//------------------------------------------------------------------------------
int PPU::calculateDrawPenalties() const
{
    int penalty = 0;

    //  1)  Fine-scroll:  SCX % 8 
    uint8_t scx = memory->read(0xFF43);
    penalty += scx & 0x07;                                        // 0-7 dots

    // 2)  Window start-up delay: always 6 dots once per line  
    uint8_t lcdc = memory->read(0xFF40);
    if (lcdc & 0x20) {                                            // Window enable
        uint8_t wy = memory->read(0xFF4A);
        uint8_t wxReg = memory->read(0xFF4B);   // 0-255 in the register
        int16_t wx = static_cast<int16_t>(wxReg) - 7; // screen X (-7 … 166)

        if (currentLine >= wy && wx < 160) {
            penalty += 6;                       // fixed start-up delay
        }
    }

    //  3)  Sprite stalls: 6 dots per visible OBJ (good heuristic) 
    if (lcdc & 0x02) {                                            // OBJ enable
        int count = 0;
        int h = spriteHeight();
        for (int i = 0; i < 40 && count < 10; ++i) {
            uint16_t spriteY = memory->read(0xFE00 + i * 4) - 16;
            if (currentLine >= spriteY && currentLine < spriteY + h)
                ++count;
        }
        penalty += count * 6;                                     // 0-60 dots
    }
    return penalty;                                               // 0-73+ dots
}



void PPU::renderScanline()
{
    std::fill(std::begin(scanline), std::end(scanline), 0);
    // 1.  Render BG into a temporary 160-pixel buffer (color index 0-3).
    scanlineBackground();

    // 2.  If the window is visible this line, replace the relevant pixels.
    scanlineWindow();

    // 3.  Overlay up to 10 sprites, handling priority & transparency.
    scanlineSprite();

    // 4.  Convert the 4-level color indices in scanline[]
    //     to 32-bit RGB and copy into the frame buffer.
    for (int x = 0; x < 160; ++x)
        framebuffer[currentLine][x] = mapColor(scanline[x]);
}

void PPU::step(int ticks) {
    modeClock += ticks;

    switch (mode) {
    case OAM_SEARCH:  // Mode 2
        if (modeClock >= 80) enterMode(DRAWING, modeClock - 80);
        break;

    case DRAWING:     // Mode 3
        // Duration is 172 + SCX%8 + sprite/window penalties
        lastM3Length = 172 + calculateDrawPenalties();
        if (modeClock >= lastM3Length) {

            renderScanline(); // draw pixels here
            enterMode(HBLANK, modeClock - lastM3Length);
        }
        break;

    case HBLANK: {      // Mode 0
        int hblankLen = 456 - 80 - lastM3Length;   // Pan Docs
        if (modeClock >= hblankLen) {
            currentLine++;
            memory->write(0xFF44, currentLine);
            
            checkCoincidence();
            if (currentLine == 144)
                enterMode(VBLANK, modeClock - hblankLen);
            else
                enterMode(OAM_SEARCH, modeClock - hblankLen);
        }
        break;
    }

    case VBLANK:
        if (modeClock >= 456) {
            modeClock -= 456;
            currentLine++;

            if (currentLine > 153) {
                currentLine = 0;
				windowLine = -1;  // reset window line
                memory->write(0xFF44, 0);
                checkCoincidence();                 // update bit-2, maybe raise IRQ
                enterMode(OAM_SEARCH, modeClock);
            }
            else {
                memory->write(0xFF44, currentLine);
                checkCoincidence();                 // same for LY = 145-153
            }
        }
        break;
    }
    if (mode == 2 || mode == 3) {
        memory->oamBlocked = true;
    }
    else {
        memory->oamBlocked = false;
    }
}

void PPU::scanlineBackground()
{

    uint8_t lcdc = memory->read(0xFF40);
    bool bgEnabled = lcdc & 0x01;
        
    bool useUnsigned = (lcdc & 0x10) != 0;
    uint16_t mapBase = (lcdc & 0x08) ? 0x9C00 : 0x9800;

    uint8_t scy = memory->read(0xFF42);
    uint8_t scx = memory->read(0xFF43);
    uint8_t ly = currentLine;


    if (!bgEnabled) {
        std::fill(std::begin(scanline), std::end(scanline), 0);  // colour 0
        return;
    }

    for (int x = 0; x < 160; ++x) {
        uint8_t bgX = scx + x;
        uint8_t bgY = scy + ly;

        uint16_t tileMapAddr = mapBase + ((bgY / 8) * 32) + (bgX / 8);
        int16_t  tileIndex = memory->read(tileMapAddr);
        if (!useUnsigned) tileIndex = int8_t(tileIndex);           // signed index

        uint8_t  raw = memory->read(tileMapAddr);
        int16_t  index16 = useUnsigned
             ? raw
             : int8_t(raw);
        uint16_t tileAddr = (useUnsigned ? 0x8000 : 0x9000)
                                    +index16 * 16;
        uint8_t  rowByte = (bgY & 7) * 2;
        uint8_t  lo = memory->read(tileAddr + rowByte);
        uint8_t  hi = memory->read(tileAddr + rowByte + 1);
        int bit = 7 - (bgX & 7);
        uint8_t colour = ((hi >> bit) & 1) << 1 | ((lo >> bit) & 1);
        
        scanline[x] = colour;    // store colour index 0-3
    }

}
void PPU::scanlineWindow()
{
    uint8_t lcdc = memory->read(0xFF40);

    // Window disabled      (bit-5) OR BG/window master disable (bit 0)
    if (!(lcdc & 0x20) || !(lcdc & 0x01))
        return;

    int wy = memory->read(0xFF4A);
    int wx = int(memory->read(0xFF4B)) - 7;          // screen X, -7

    // Window not active on this scan-line
    if (currentLine < wy || wx >= 160)
        return;

    // Initialize window line counter
    if (windowLine == -1)
        windowLine = 0;

    // Compute the window's tile row and pixel row
    int tileRow = windowLine / 8;
    int pixRow = windowLine % 8;

    // Signed vs unsigned tile data (bit-4)
    bool useUnsigned = (lcdc & 0x10) != 0;

    // Window tile map base select (bit-6)
    uint16_t mapBase = (lcdc & 0x40) ? 0x9C00 : 0x9800;

    // Draw window pixels
    for (int x = std::max(0, wx); x < 160; ++x) {
        int winX = x - wx;

        // Fetch tile index from map
        uint16_t tileMapAddr = mapBase + tileRow * 32 + (winX / 8);
        int16_t tileIndex = memory->read(tileMapAddr);
        if (!useUnsigned)
            tileIndex = int8_t(tileIndex);

        // Compute tile data address
        uint16_t tileAddr = (useUnsigned ? 0x8000 : 0x9000) + tileIndex * 16;

        // Read the two bytes for this pixel row
        uint8_t lo = memory->read(tileAddr + pixRow * 2);
        uint8_t hi = memory->read(tileAddr + pixRow * 2 + 1);

        // Extract the color bits
        int bit = 7 - (winX & 7);
        uint8_t colour = ((hi >> bit) & 1) << 1 | ((lo >> bit) & 1);

        // Overwrite background
        scanline[x] = colour;
    }

    // Advance for the next physical scan-line
    windowLine++;
}

//void PPU::scanlineWindow()
//{
//    uint8_t lcdc = memory->read(0xFF40);
//
//    // Window disabled      (bit-5) OR BG/window master disable (bit 0)
//    if (!(lcdc & 0x20) || !(lcdc & 0x01))
//        return;
//
//    int  wy = memory->read(0xFF4A);
//    int  wx = int(memory->read(0xFF4B)) - 7;          // screen X, -7  166
//
//    // Window not active on this scan-line
//    if (currentLine < wy || wx >= 160)
//        return;
//
//    // manage the internal "window line counter" 
//    if (windowLine == -1)        // first line the window becomes visible
//        windowLine = 0;
//
//    int winY = (currentLine - wy) & 7;       // use the running counter for tile row
//
//    // Signed vs unsigned tile set
//    bool useUnsigned = (lcdc & 0x10) != 0;
//    uint16_t mapBase = (lcdc & 0x08) ? 0x9C00 : 0x9800;
//
//
//    for (int x = std::max(0, wx); x < 160; ++x) {
//        int winX = x - wx;
//
//        uint16_t tileMapAddr = mapBase + ((winY / 8) * 32) + (winX / 8);
//        int16_t  tileIndex = memory->read(tileMapAddr);
//        if (!useUnsigned) tileIndex = int8_t(tileIndex);           // signed index
//
//        uint8_t  raw = memory->read(tileMapAddr);
//        int16_t  index16 = useUnsigned
//            ? raw
//            : int8_t(raw);
//        uint16_t tileAddr = (useUnsigned ? 0x8000 : 0x9000)
//            + index16 * 16;
//
//        uint8_t  rowByte = (winY & 7) * 2;
//        uint8_t  lo = memory->read(tileAddr + rowByte);
//        uint8_t  hi = memory->read(tileAddr + rowByte + 1);
//        int bit = 7 - (winX & 7);
//        uint8_t colour = ((hi >> bit) & 1) << 1 | ((lo >> bit) & 1);
//
//        scanline[x] = colour;        // overwrite BG
//    }
//
//    windowLine++;     // advance for next physical scan-line, even if WX < 7
//}
void PPU::scanlineSprite()
{
    uint8_t lcdc = memory->read(0xFF40);
    if (!(lcdc & 0x02)) return;                      // Sprites off

    int h = spriteHeight();
    struct Sprite { uint16_t x, y, tile, attr; };
    Sprite sprites[10]; int vis = 0;

    // Pass 1: find up to 10 sprites that intersect this LY
    for (int i = 0; i < 40 && vis < 10; ++i) {
        uint16_t y = int16_t(memory->read(0xFE00 + i * 4)) - 16;
        uint16_t x = int16_t(memory->read(0xFE00 + i * 4 + 1)) - 8;
        uint8_t tile = memory->read(0xFE00 + i * 4 + 2);
        uint8_t attr = memory->read(0xFE00 + i * 4 + 3);
        if (currentLine >= y && currentLine < y + h) {
            sprites[vis++] = { x, y, tile, attr };
        }
    }

    // Sprites with lower X draw first (ties broken by OAM order already)
    //std::sort(sprites, sprites + vis,
     //   [](const Sprite& a, const Sprite& b) { return a.x < b.x; });

    for (int s = 0; s < vis; ++s) {
        const Sprite& sp = sprites[s];
        if (sp.x >= 160) continue;

        int lineInSprite = currentLine - sp.y;
        if (sp.attr & 0x40) lineInSprite = h - 1 - lineInSprite; // Y-flip

        uint16_t tileAddr = 0x8000 + sp.tile * 16;
        uint16_t baseTile = (h == 16) ? (sp.tile & 0xFE) : sp.tile;

        if (h == 16) tileAddr = 0x8000 + baseTile * 16;  // ignore LSB in 8×16 mode
        int row = lineInSprite;
        if (h == 16 && row >= 8) {        // bottom half of 8×16 sprite
            tileAddr += 16;               // skip to second tile
            row -= 8;
        }

        uint8_t lo = memory->read(tileAddr + row * 2);
        uint8_t hi = memory->read(tileAddr + row * 2 + 1);

        for (int px = 0; px < 8; ++px) {
            int bit = (sp.attr & 0x20) ? px : (7 - px);          // X-flip
            uint8_t colour = ((hi >> bit) & 1) << 1 | ((lo >> bit) & 1);
            if (colour == 0) continue;                           // transparent

            int xPos = sp.x + px;
            if (xPos < 0 || xPos >= 160) continue;

            // OBJ-BG priority: if bit 7 clear OR BG pixel == 0, draw.
            if (!(sp.attr & 0x80) || scanline[xPos] == 0) {
                if ((scanline[xPos] & 0x10) == 0) {  // no previous sprite drawn
                    uint8_t tag = 0x10;
                    if (sp.attr & 0x10) tag |= 0x20;
                    scanline[xPos] = tag | colour;
                }
            }
                
        }
    }
}

 int PPU::spriteHeight()  const {
    uint8_t lcdc = memory->read(0xFF40);   // LCD Control
    return (lcdc & 0x04) ? 16 : 8;        // bit-2 = OBJ_SIZE
}

 void PPU::checkCoincidence()
 {
     bool coinc = memory->read(0xFF44) == memory->read(0xFF45);
     uint8_t stat = memory->read(0xFF41);
     bool before = stat & 0x04;            // previous coincidence state

     stat = (stat & ~0x04) | (coinc ? 0x04 : 0);
     memory->write(0xFF41, stat);           // bit-2 always reflects LY==LYC

     /* Rising edge + enable bit 6 request STAT */
     if (!before && coinc && (stat & 0x40)) {
         if (!(memory->read(0xFF0F) & Memory::INT_STAT))     // edge-trigger
             memory->requestInterrupt(Memory::INT_STAT);
     }
 }

// newMode : 0 = HBlank, 1 = VBlank, 2 = OAM-Search, 3 = Drawing
// carry    : dots that overflowed the previous mode (step() passes this)
void PPU::enterMode(uint8_t newMode, int carry)
{
    mode = newMode;
    modeClock = carry;        // start the next mode with any leftover dots

    /* ---------- 1.  Update STAT register ---------- */
    uint8_t stat = memory->read(0xFF41);    // LCD-STAT
    stat = (stat & 0xFC) | (mode & 0x03);      // bits 1-0 = current mode

    /* LY==LYC coincidence flag (bit 2) */
    const uint8_t ly = memory->read(0xFF44);
    const uint8_t lyc = memory->read(0xFF45);
    if (ly == lyc)
        stat |= 0x04;
    else
        stat &= ~0x04;

    memory->write(0xFF41, stat);

    /* ---------- 2.  Interrupt helper ---------- */
    auto requestSTAT = [&]()
        {
            // IF bit-1 is STAT; we expose it via Memory::requestInterrupt(INT_STAT)
            if (!(memory->read(0xFF0F) & Memory::INT_STAT))
                memory->requestInterrupt(Memory::INT_STAT);
        };

    /* ---------- 3.  Mode-specific side effects ---------- */
    switch (mode)
    {
    case OAM_SEARCH:       // Mode 2
        if (stat & 0x20)   // STAT bit 5 = "Mode-2 interrupt enable"
            requestSTAT();
        break;

    case DRAWING: {        // Mode 3  –– never generates STAT interrupts
        
        break;
    }

    case HBLANK:           // Mode 0
        if (stat & 0x08)   // STAT bit 3 = "Mode-0 interrupt enable"
            requestSTAT();
        break;

    case VBLANK:           // Mode 1
        
        memory->requestInterrupt(Memory::INT_VBLANK);  // IF bit-0
        if (newMode == VBLANK && currentLine == 144)
            frameReady = true;
        if (stat & 0x10)   // STAT bit 4 = "Mode-1 interrupt enable"
            requestSTAT();
        break;
    }
}
