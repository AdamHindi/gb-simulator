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
        lastM3Length = 172 + calculateDrawPenalties() ;
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

    uint8_t bgY = scy + ly;
    uint8_t pixelRow = bgY & 0x07;  // which of the 8 Y rows

    for (int x = 0; x < 160; ++x) {
        uint8_t bgX = scx + x;
        

        uint16_t tileRow = bgY >> 3;
        uint16_t tileCol = bgX >> 3;

        // correct pixel within tile values:
        uint8_t pixelCol = bgX & 0x07;  // which of the 8 bits in that row

        uint16_t tileMapAddr = mapBase + tileRow * 32 + tileCol;

        uint8_t raw = vram[tileMapAddr - 0x8000];
        int16_t index16 = useUnsigned ? raw : int8_t(raw);
        uint16_t tileAddr = (useUnsigned ? 0x8000 : 0x9000) + index16 * 16;

        // pick the two data bytes for this row
        uint8_t lo = vram[tileAddr + pixelRow * 2 - 0x8000];
        uint8_t hi = vram[tileAddr + pixelRow * 2 + 1 - 0x8000];
		

        // extract the correct bit for this column
        int bit = 7 - pixelCol;
        uint8_t colour = ((hi >> bit) & 1) << 1 | ((lo >> bit) & 1);

        scanline[x] = colour;
    }

}



//

void PPU::scanlineWindow() {
    constexpr int SCREEN_W = 160;
    constexpr int TILE_W = 8;
    constexpr int MAP_TILES = 32;
    constexpr int MAP_MASK = MAP_TILES - 1;

    uint8_t lcdc = memory->read(0xFF40);
    // Window disabled or LCD off
    if (!(lcdc & 0x20) || !(lcdc & 0x01)) return;

    int wy = memory->read(0xFF4A);
    int wx = int(memory->read(0xFF4B)) - 7;    // screen X offset

    // Not yet reached the window vertically, or fully off right
    if (currentLine < wy || wx >= SCREEN_W) return;

    // Start windowLine on first visible line
    if (windowLine == -1) windowLine = 0;

    // Invariants for this scanline
    int tileRow = windowLine / TILE_W;               // which tilerow
    int pixRow = windowLine % TILE_W;               // which Ypixel in that tile
    int rowByte = pixRow * 2;                        // byteoffset in tile data
    bool useUnsigned = (lcdc & 0x10) != 0;             // tiledata signed/unsigned
    uint16_t mapBase = (lcdc & 0x40) ? 0x9C00 : 0x9800;// window map select

    // Compute range in screen X
    int startX = std::max(0, wx);
    int endX = SCREEN_W - 1;

    // First and last tile indices (modulo 32 for wrap)
    int firstTileX = ((startX - wx) / TILE_W) & MAP_MASK;
    int lastTileX = ((endX - wx) / TILE_W) & MAP_MASK;
    int numTiles = lastTileX - firstTileX + 1;
    if (numTiles < 0) numTiles += MAP_TILES;

    // Pixel offset into the first tile
    int firstOffset = (startX - wx) % TILE_W;
    if (firstOffset < 0) firstOffset += TILE_W;

    // Now batch pertile
    for (int t = 0; t < numTiles; ++t) {
        int tileCol = (firstTileX + t) & MAP_MASK;
        // 1) Fetch tile index once
        uint16_t mapAddr = mapBase + tileRow * MAP_TILES + tileCol;
        uint8_t  raw = memory->read(mapAddr);
        int16_t  idx = useUnsigned ? raw : int8_t(raw);

        // 2) Fetch the two data bytes once
        uint16_t tileAddr = (useUnsigned ? 0x8000 : 0x9000) + idx * 16;
        uint8_t  loRow = memory->read(tileAddr + rowByte);
        uint8_t  hiRow = memory->read(tileAddr + rowByte + 1);

        // 3) Unpack the pixels
        int tileScreenX = (t * TILE_W) + wx;  // leftedge in screen coords
        int pxStart = (t == 0 ? firstOffset : 0);
        int pxEnd = (t == numTiles - 1)
            ? ((endX - wx) % TILE_W)
            : (TILE_W - 1);

        for (int px = pxStart; px <= pxEnd; ++px) {
            int screenX = tileScreenX + px;
            if (screenX < 0 || screenX > endX) continue;

            // absolute bit index within tile data
            int bit = 7 - px;
            uint8_t colour = ((hiRow >> bit) & 1) << 1
                | ((loRow >> bit) & 1);
            scanline[screenX] = colour;
        }
    }

    windowLine++;
}


void PPU::scanlineSprite() {
    uint8_t lcdc = memory->read(0xFF40);
    if (!(lcdc & 0x02)) return;  // Sprites off

    const int spriteH = spriteHeight();  // 8 or 16
    struct OAMEntry { int16_t x, y; uint8_t tile, attr; };
    OAMEntry candidates[10];
    int nSprites = 0;

    // 1) Collect up to 10 sprites that overlap this line
    for (int i = 0; i < 40 && nSprites < 10; ++i) {
        int16_t sprY = int16_t(memory->read(0xFE00 + i * 4)) - 16;
        int16_t sprX = int16_t(memory->read(0xFE00 + i * 4 + 1)) - 8;
        if (currentLine >= sprY && currentLine < sprY + spriteH) {
            candidates[nSprites++] = {
                sprX,
                sprY,
                memory->read(0xFE00 + i * 4 + 2),
                memory->read(0xFE00 + i * 4 + 3)
            };
        }
    }

    // 2) Sort by X (lower X first; ties keep OAM order)
    std::sort(candidates, candidates + nSprites,
        [](auto& a, auto& b) { return a.x < b.x; }
    );

    // 3) Render each sprite
    for (int i = 0; i < nSprites; ++i) {
        auto& sp = candidates[i];

        // Skip fully off screen
        if (sp.x <= -8 || sp.x >= 160) continue;

        // Determine which row of the sprite to draw
        int row = currentLine - sp.y;
        bool yFlip = sp.attr & 0x40;
        if (yFlip) row = spriteH - 1 - row;

        // In 8×16 mode, ignore LSB of tile index and pick high/low half
        uint8_t baseTile = sp.tile;
        if (spriteH == 16) baseTile &= 0xFE;
        if (spriteH == 16 && row >= 8) {
            baseTile += 1;
            row -= 8;
        }

        // Fetch that tile's two bytes just once
        uint16_t tileAddr = 0x8000 + baseTile * 16;
        uint8_t lo = memory->read(tileAddr + row * 2);
        uint8_t hi = memory->read(tileAddr + row * 2 + 1);

        bool xFlip = sp.attr & 0x20;
        bool priority = sp.attr & 0x80;      // OBJ to BG priority
        uint8_t palTag = 0x10 | ((sp.attr & 0x10) ? 0x20 : 0x00);

        // Draw 8 pixels
        for (int px = 0; px < 8; ++px) {
            int bit = xFlip ? px : (7 - px);
            uint8_t color = ((hi >> bit) & 1) << 1
                | ((lo >> bit) & 1);
            if (color == 0) continue;  // transparent

            int screenX = sp.x + px;
            if (screenX < 0 || screenX >= 160) continue;

            // If BG priority bit set *and* BG pixel non-zero, skip
            if (priority && (scanline[screenX] & 0x03) != 0)
                continue;

            // Only first sprite may write to this pixel
            if (scanline[screenX] & 0x10)
                continue;

            scanline[screenX] = palTag | color;
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
