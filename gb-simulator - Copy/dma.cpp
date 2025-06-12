#include "dma.hpp"

#include "memory.hpp"




void DMA::start(uint8_t highByte, Memory* mem) {
    source = highByte << 8;
    progress = 0;
    active = true;
    memory = mem;
}

// call once per *machine* cycle
void DMA::tick() {
    if (!active) return;
    if (progress < 160) {
        uint8_t data = memory->read(source + progress);
        memory->oamPtr()[progress] = data;
        ++progress;
    }
    if (progress == 160) active = false;
}