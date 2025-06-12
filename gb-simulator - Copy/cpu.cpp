#include "cpu.hpp"

static constexpr uint16_t INT_VECTOR[5] = {
    0x40, // V-Blank
    0x48, // LCD-STAT
    0x50, // Timer
    0x58, // Serial
    0x60  // Joypad
};


CPU::CPU(uint8_t* io_regs) : timer(io_regs) { 
    haltbug = false;
    ime = false;  
    stopped = false;  
    halted2 = false;
    halted = false;  
    enableInterruptsNext = false;  
    opcode = 0x00;  
    cbOpcodeTemp = 0x00;  
    cycles = 0; // Initialize cycles to 0  
	conditional = false;
    memory = nullptr;  
    regs.af = 0x01B0;
    regs.bc = 0x0013;
    regs.de = 0x00D8;
    regs.hl = 0x014D;
    sp = 0xFFFE;  
    pc = 0x0100; // Game Boy entry point  
}

uint8_t CPU::inc8(uint8_t val) {
    uint8_t result = val + 1;

    // Zero flag
    if (result == 0) regs.f |= FLAG_Z;
    else regs.f &= ~FLAG_Z;

    // Half-carry flag (set if carry from bit 3)
    if ((val & 0x0F) + 1 > 0x0F) regs.f |= FLAG_H;
    else regs.f &= ~FLAG_H;

    // Clear N (subtract) flag
    regs.f &= ~FLAG_N;

    return result;
}


uint8_t CPU::dec8(uint8_t val) {
    uint8_t result = val - 1;

    // Zero flag
    if (result == 0) regs.f |= FLAG_Z;
    else regs.f &= ~FLAG_Z;

    // Half-carry flag (set if borrow from bit 4)
    if ((val & 0x0F) == 0x00) regs.f |= FLAG_H;
    else regs.f &= ~FLAG_H;

    // Set N (subtract) flag
    regs.f |= FLAG_N;

    return result;
}

uint8_t CPU::read8(uint16_t addr) const {
    return memory->read(addr);
}
void CPU::write8(uint16_t addr, uint8_t value) {

    switch (addr) {
        // --- DIV reset ---
    case 0xFF04:
        timer.resetDIV();
        return;

        // --- TIMA write ---
    case 0xFF05:
        timer.writeTIMA(value);
        return;

        // --- TMA write ---
    case 0xFF06:
        timer.writeTMA(value);
        return;

        // --- TAC write ---
    case 0xFF07:
        timer.writeTAC(value);
        return;
    
	    

    default:
        memory->write(addr, value);
    }
}

uint8_t CPU::add8(uint8_t a, uint8_t b) {
    uint16_t result = a + b;
    regs.f = 0;
    if ((result & 0xFF) == 0) regs.f |= FLAG_Z;
    if ((a & 0xF) + (b & 0xF) > 0xF) regs.f |= FLAG_H;
    if (result > 0xFF) regs.f |= FLAG_C;
    // N is cleared for ADD
    return result & 0xFF;
}

uint8_t CPU::adc8(uint8_t a, uint8_t b) {
    uint8_t carry = regs.getFlag(FLAG_C) ? 1 : 0;
    uint16_t result = a + b + carry;
    regs.f = 0;
    if ((result & 0xFF) == 0) regs.f |= FLAG_Z;
    if (((a & 0xF) + (b & 0xF) + carry) > 0xF) regs.f |= FLAG_H;
    if (result > 0xFF) regs.f |= FLAG_C;
    return result & 0xFF;
}
uint8_t CPU::sub8(uint8_t a, uint8_t b) {
    uint16_t result = a - b;
    regs.f = FLAG_N;
    if ((result & 0xFF) == 0) regs.f |= FLAG_Z;
    if ((b & 0xF) > (a & 0xF)) regs.f |= FLAG_H;
    if (b > a) regs.f |= FLAG_C;
    return result & 0xFF;
}
uint8_t CPU::sbc8(uint8_t a, uint8_t b) {
    uint8_t carry = regs.getFlag(FLAG_C) ? 1 : 0;
    uint16_t result = a - b - carry;
    regs.f = FLAG_N;
    if ((result & 0xFF) == 0) regs.f |= FLAG_Z;
    if (((b & 0xF) + carry) > (a & 0xF)) regs.f |= FLAG_H;
    if (b + carry > a) regs.f |= FLAG_C;
    return result & 0xFF;
}
void CPU::cp8(uint8_t a, uint8_t b) {
    regs.f = FLAG_N;
    if (a == b) regs.f |= FLAG_Z;
    if ((b & 0xF) > (a & 0xF)) regs.f |= FLAG_H;
    if (b > a) regs.f |= FLAG_C;
}
uint8_t CPU::rlc(uint8_t val) {
    uint8_t result = (val << 1) | (val >> 7);
    regs.f = 0;
    if (result == 0) regs.f |= FLAG_Z;
    if (val & 0x80) regs.f |= FLAG_C;
    return result;
}
uint8_t CPU::rrc(uint8_t val) {
    uint8_t result = (val >> 1) | ((val & 0x01) << 7);
    regs.f = 0;
    if (result == 0) regs.f |= FLAG_Z;
    if (val & 0x01) regs.f |= FLAG_C;
    return result;
}
uint8_t CPU::rl(uint8_t val) {
    uint8_t carry_in = regs.getFlag(FLAG_C) ? 1 : 0;
    uint8_t carry_out = (val >> 7) & 0x01;
    uint8_t result = (val << 1) | carry_in;

    regs.f = 0;
    if (result == 0) regs.f |= FLAG_Z;
    if (carry_out) regs.f |= FLAG_C;
    return result;
}
uint8_t CPU::rr(uint8_t val) {
    uint8_t carry_in = regs.getFlag(FLAG_C) ? 0x80 : 0;
    uint8_t carry_out = val & 0x01;
    uint8_t result = (val >> 1) | carry_in;

    regs.f = 0;
    if (result == 0) regs.f |= FLAG_Z;
    if (carry_out) regs.f |= FLAG_C;
    return result;
}
uint8_t CPU::sla(uint8_t val) {
    uint8_t carry_out = val & 0x80;
    uint8_t result = val << 1;

    regs.f = 0;
    if (result == 0) regs.f |= FLAG_Z;
    if (carry_out) regs.f |= FLAG_C;
    return result;
}
uint8_t CPU::sra(uint8_t val) {
    uint8_t carry_out = val & 0x01;
    uint8_t result = (val >> 1) | (val & 0x80); // Preserve MSB

    regs.f = 0;
    if (result == 0) regs.f |= FLAG_Z;
    if (carry_out) regs.f |= FLAG_C;
    return result;
}
uint8_t CPU::swap(uint8_t val) {
    uint8_t result = (val >> 4) | (val << 4);

    regs.f = 0;
    if (result == 0) regs.f |= FLAG_Z;
    return result;
}
uint8_t CPU::srl(uint8_t val) {
    uint8_t carry_out = val & 0x01;
    uint8_t result = val >> 1;

    regs.f = 0;
    if (result == 0) regs.f |= FLAG_Z;
    if (carry_out) regs.f |= FLAG_C;
    return result;
}




bool CPU::interruptPending() const{
    uint8_t ie = read8(0xFFFF); // Interrupt Enable
    uint8_t flags = read8(0xFF0F); // Interrupt Request (IF)
    return (ie & flags & 0x1F) != 0; // At least one interrupt both enabled and requested
}
void CPU::connectMemory(Memory* m) {
    memory = m;
}

void CPU::clearIFBit(int i)   // 0-4
{
    uint8_t iff = read8(0xFF0F);
    iff &= ~(1u << i);
    write8(0xFF0F, iff);
}




void CPU::handleInterrupts() {

    uint8_t ie = read8(0xFFFF);  // IE register
	uint8_t flags = read8(0xFF0F); // IF register
	uint8_t triggered = flags & ie & 0x1F; // Mask to only the first 5 bits (interrupts 0-4)
	if (triggered == 0) return; // No interrupts to handle
	
	// Disable interrupts 
    ime = false;       // Clear IME
    halted = false;    // Always unhalt on interrupt

    // Push PC onto stack
    sp--;
    write8(sp, (pc >> 8) & 0xFF);
    sp--;
    write8(sp, pc & 0xFF);

    // Service the first triggered interrupt (lowest bit wins)
    static const uint16_t vectors[5] = {
        0x40, // V-Blank
        0x48, // LCD STAT
        0x50, // Timer
        0x58, // Serial
        0x60  // Joypad
    };
    cycles += 5;
    timer.update(20, stopped); // Update timer based on cycles executed
    for (int i = 0; i < 5; ++i) {
       
        if (triggered & (1 << i)) {
            flags &= ~(1 << i);
            write8(0xFF0F, flags);
            pc = vectors[i];
			
            break;
        }
    }
}

void CPU::cycle() {
    cycles = 0;
    uint8_t ie = read8(0xFFFF);
    uint8_t iflag = read8(0xFF0F);
    uint8_t triggered = read8(0xFFFF) & read8(0xFF0F) & 0x1F;

    bool fired = (read8(0xFFFF) & read8(0xFF0F) & 0x1F) != 0;

    if (ime && fired) {
        // Find highest-priority interrupt
        triggered = read8(0xFFFF) & read8(0xFF0F) & 0x1F;

        // Clear IF flag later
        uint8_t vector;
        if (triggered & (1 << 0)) vector = 0x40; // V-Blank
        else if (triggered & (1 << 1)) vector = 0x48;
        else if (triggered & (1 << 2)) vector = 0x50;
        else if (triggered & (1 << 3)) vector = 0x58;
        else if (triggered & (1 << 4)) vector = 0x60;
        halted = false;
        interruptInProgress = true;
        interruptDelay = 5;
        interruptVector = triggered;
        ie = read8(0xFFFF);
        iflag = read8(0xFF0F);
        ime = false;
    }

    if (interruptInProgress) {
        interruptDelay--;
        
        timer.update(4, false); // 1 T-cycle at a time
        
        cycles += 5;
        switch (interruptDelay) {
        
        case 4: // push PC high
        {

            write8(--sp, pc >> 8);
            break;
        }
        case 3: // push PC low
            fired = (read8(0xFFFF) & read8(0xFF0F) & 0x1F) != 0;
            interruptVector = read8(0xFFFF) & read8(0xFF0F) & 0x1F;
            if (!fired) {
                // Interrupt cancelled!
                interruptInProgress = false;
                pc = 0x0000;
            }
            
            write8(--sp, pc & 0xFF);

            break;
        case 0: {
            
            // Still valid
           // Still valid
            static const uint16_t vectors[5] = {
    0x40, // V-Blank
    0x48, // LCD STAT
    0x50, // Timer
    0x58, // Serial
    0x60  // Joypad
            };
            for (int i = 0; i < 5; ++i) {
                if (interruptVector & (1 << i)) {
                    iflag &= ~(1 << i);
                    write8(0xFF0F, iflag);
                    pc = vectors[i];
                    
                    break;
                }
            }
            interruptVector = 0;
            interruptInProgress = false;
            break;
        }
                
            
        }

        // Don't execute other instructions during dispatch
        return;
    }

    if (halted)
    {
        cycles += 1;

        if (fired) {
            halted = false;
        }
        else {
            timer.update(4, stopped); // Update timer while halted
            return;
        }
    }
    if (haltbug) {
        opcode = read8(pc);  // Do not increment PC
        haltbug = false;
    }
    else {
        opcode = read8(pc++);
    }

    executeInstruction();

    if (enableInterruptsNext) {
        ime = true;
        enableInterruptsNext = false;
    }

    // Update timer after executing the instruction
    timer.update(cycles * 4, stopped); // Update timer based on cycles executed
    


}


//void CPU::cycle() {
//    cycles = 0;
//    
//    bool fired = (read8(0xFFFF) & read8(0xFF0F) & 0x1F) !=0;
//    
//    if(ime && fired) {
//        handleInterrupts(); // Handle interrupts if IME is set and an interrupt is pending
//        return;
//	}
//    if (halted)
//    {
//        cycles += 1;
//		
//        if (fired) {
//            halted = false;
//        }
//        else {
//            timer.update(4, stopped); // Update timer while halted
//            return;
//        }
//    }
//    if (haltbug) {
//        opcode = read8(pc);  // Do not increment PC
//        haltbug = false;
//    }
//    else {
//        opcode = read8(pc++);
//    }
//
//    executeInstruction();
//
//    if (enableInterruptsNext) {
//        ime = true;
//        enableInterruptsNext = false;
//    }
//	
//	// Update timer after executing the instruction
//	timer.update(cycles * 4, stopped); // Update timer based on cycles executed
//    for (uint16_t i = 0; i < cycles; ++i) memory->dma.tick();
//     
//
//}

void CPU::executeCB(uint8_t cbOpcode) {
	//
    // timer.update(TstatesCB[cbOpcode] * 4, stopped); // Update timer before executing CB opcode
    //cbOpcodeTemp = cbOpcode;
	cycles += TstatesCB[cbOpcode]; // Update cycles based on CB opcode timing
	//timer.update(TstatesCB[cbOpcode] * 4, stopped); // Update timer based on cycles executed
    switch (cbOpcode) {
    case 0x00: regs.b = rlc(regs.b); break;
    case 0x01: regs.c = rlc(regs.c); break;
    case 0x02: regs.d = rlc(regs.d); break;
    case 0x03: regs.e = rlc(regs.e); break;
    case 0x04: regs.h = rlc(regs.h); break;
    case 0x05: regs.l = rlc(regs.l); break;
    case 0x06: { uint8_t v = rlc(read8(regs.hl)); write8(regs.hl, v); break; }
    case 0x07: regs.a = rlc(regs.a); break;
    case 0x08: regs.b = rrc(regs.b); break;
    case 0x09: regs.c = rrc(regs.c); break;
    case 0x0A: regs.d = rrc(regs.d); break;
    case 0x0B: regs.e = rrc(regs.e); break;
    case 0x0C: regs.h = rrc(regs.h); break;
    case 0x0D: regs.l = rrc(regs.l); break;
    case 0x0E: { uint8_t v = rrc(read8(regs.hl)); write8(regs.hl, v); break; }
    case 0x0F: regs.a = rrc(regs.a); break;
    case 0x10: regs.b = rl(regs.b); break;
    case 0x11: regs.c = rl(regs.c); break;
    case 0x12: regs.d = rl(regs.d); break;
    case 0x13: regs.e = rl(regs.e); break;
    case 0x14: regs.h = rl(regs.h); break;
    case 0x15: regs.l = rl(regs.l); break;
    case 0x16: { uint8_t v = rl(read8(regs.hl)); write8(regs.hl, v); break; }
    case 0x17: regs.a = rl(regs.a); break;
    case 0x18: regs.b = rr(regs.b); break;
    case 0x19: regs.c = rr(regs.c); break;
    case 0x1A: regs.d = rr(regs.d); break;
    case 0x1B: regs.e = rr(regs.e); break;
    case 0x1C: regs.h = rr(regs.h); break;
    case 0x1D: regs.l = rr(regs.l); break;
    case 0x1E: { uint8_t v = rr(read8(regs.hl)); write8(regs.hl, v); break; }
    case 0x1F: regs.a = rr(regs.a); break;
    case 0x20: regs.b = sla(regs.b); break;
    case 0x21: regs.c = sla(regs.c); break;
    case 0x22: regs.d = sla(regs.d); break;
    case 0x23: regs.e = sla(regs.e); break;
    case 0x24: regs.h = sla(regs.h); break;
    case 0x25: regs.l = sla(regs.l); break;
    case 0x26: { uint8_t v = sla(read8(regs.hl)); write8(regs.hl, v); break; }
    case 0x27: regs.a = sla(regs.a); break;
    case 0x28: regs.b = sra(regs.b); break;
    case 0x29: regs.c = sra(regs.c); break;
    case 0x2A: regs.d = sra(regs.d); break;
    case 0x2B: regs.e = sra(regs.e); break;
    case 0x2C: regs.h = sra(regs.h); break;
    case 0x2D: regs.l = sra(regs.l); break;
    case 0x2E: { uint8_t v = sra(read8(regs.hl)); write8(regs.hl, v); break; }
    case 0x2F: regs.a = sra(regs.a); break;
    case 0x30: regs.b = swap(regs.b); break;
    case 0x31: regs.c = swap(regs.c); break;
    case 0x32: regs.d = swap(regs.d); break;
    case 0x33: regs.e = swap(regs.e); break;
    case 0x34: regs.h = swap(regs.h); break;
    case 0x35: regs.l = swap(regs.l); break;
    case 0x36: { uint8_t v = swap(read8(regs.hl)); write8(regs.hl, v); break; }
    case 0x37: regs.a = swap(regs.a); break;
    case 0x38: regs.b = srl(regs.b); break;
    case 0x39: regs.c = srl(regs.c); break;
    case 0x3A: regs.d = srl(regs.d); break;
    case 0x3B: regs.e = srl(regs.e); break;
    case 0x3C: regs.h = srl(regs.h); break;
    case 0x3D: regs.l = srl(regs.l); break;
    case 0x3E: { uint8_t v = srl(read8(regs.hl)); write8(regs.hl, v); break; }
    case 0x3F: regs.a = srl(regs.a); break;


    
    case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45:case 0x46:
    case 0x47: { // BIT 0, r
        uint8_t val = (cbOpcode == 0x46) ? read8(regs.hl) :
            ((cbOpcode == 0x40) ? regs.b :
                (cbOpcode == 0x41) ? regs.c :
                (cbOpcode == 0x42) ? regs.d :
                (cbOpcode == 0x43) ? regs.e :
                (cbOpcode == 0x44) ? regs.h :
                (cbOpcode == 0x45) ? regs.l :
                regs.a);
        regs.f = (regs.f & FLAG_C) | FLAG_H | ((val & (1 << 0)) ? 0 : FLAG_Z);
        break;
    }

    case 0x48: case 0x49: case 0x4A: case 0x4B: case 0x4C: case 0x4D:case 0x4E:
    case 0x4F: { // BIT 1, r
        uint8_t val = (cbOpcode == 0x4E) ? read8(regs.hl) :
            ((cbOpcode == 0x48) ? regs.b :
                (cbOpcode == 0x49) ? regs.c :
                (cbOpcode == 0x4A) ? regs.d :
                (cbOpcode == 0x4B) ? regs.e :
                (cbOpcode == 0x4C) ? regs.h :
                (cbOpcode == 0x4D) ? regs.l :
                regs.a);
        regs.f = (regs.f & FLAG_C) | FLAG_H | ((val & (1 << 1)) ? 0 : FLAG_Z);
        break;
    }
    case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55:case 0x56:
    case 0x57: { // BIT 2, r
        uint8_t val = (cbOpcode == 0x56) ? read8(regs.hl) :
            ((cbOpcode == 0x50) ? regs.b :
                (cbOpcode == 0x51) ? regs.c :
                (cbOpcode == 0x52) ? regs.d :
                (cbOpcode == 0x53) ? regs.e :
                (cbOpcode == 0x54) ? regs.h :
                (cbOpcode == 0x55) ? regs.l :
                regs.a);
        regs.f = (regs.f & FLAG_C) | FLAG_H | ((val & (1 << 2)) ? 0 : FLAG_Z);
        break;
    }
    case 0x58: case 0x59: case 0x5A: case 0x5B: case 0x5C: case 0x5D:case 0x5E:
    case 0x5F: { // BIT 3, r
        uint8_t val = (cbOpcode == 0x5E) ? read8(regs.hl) :
            ((cbOpcode == 0x58) ? regs.b :
                (cbOpcode == 0x59) ? regs.c :
                (cbOpcode == 0x5A) ? regs.d :
                (cbOpcode == 0x5B) ? regs.e :
                (cbOpcode == 0x5C) ? regs.h :
                (cbOpcode == 0x5D) ? regs.l :
                regs.a);
        regs.f = (regs.f & FLAG_C) | FLAG_H | ((val & (1 << 3)) ? 0 : FLAG_Z);
        break;
    }
	case 0x60: case 0x61: case 0x62: case 0x63: case 0x64: case 0x65: case 0x66:
    case 0x67: { // BIT 4, r
        uint8_t val = (cbOpcode == 0x66) ? read8(regs.hl) :
            ((cbOpcode == 0x60) ? regs.b :
                (cbOpcode == 0x61) ? regs.c :
                (cbOpcode == 0x62) ? regs.d :
                (cbOpcode == 0x63) ? regs.e :
                (cbOpcode == 0x64) ? regs.h :
                (cbOpcode == 0x65) ? regs.l :
                regs.a);
        regs.f = (regs.f & FLAG_C) | FLAG_H | ((val & (1 << 4)) ? 0 : FLAG_Z);
        break;
    }
	case 0x68: case 0x69: case 0x6A: case 0x6B: case 0x6C: case 0x6D: case 0x6E:
    case 0x6F: { // BIT 5, r
        uint8_t val = (cbOpcode == 0x6E) ? read8(regs.hl) :
            ((cbOpcode == 0x68) ? regs.b :
                (cbOpcode == 0x69) ? regs.c :
                (cbOpcode == 0x6A) ? regs.d :
                (cbOpcode == 0x6B) ? regs.e :
                (cbOpcode == 0x6C) ? regs.h :
                (cbOpcode == 0x6D) ? regs.l :
                regs.a);
        regs.f = (regs.f & FLAG_C) | FLAG_H | ((val & (1 << 5)) ? 0 : FLAG_Z);
        break;
    }
	case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76:
    case 0x77: { // BIT 6, r
        uint8_t val = (cbOpcode == 0x76) ? read8(regs.hl) :
            ((cbOpcode == 0x70) ? regs.b :
                (cbOpcode == 0x71) ? regs.c :
                (cbOpcode == 0x72) ? regs.d :
                (cbOpcode == 0x73) ? regs.e :
                (cbOpcode == 0x74) ? regs.h :
                (cbOpcode == 0x75) ? regs.l :
                regs.a);
        regs.f = (regs.f & FLAG_C) | FLAG_H | ((val & (1 << 6)) ? 0 : FLAG_Z);
        break;
    }
	case 0x78: case 0x79: case 0x7A: case 0x7B: case 0x7C: case 0x7D: case 0x7E:
    case 0x7F: { // BIT 7, r
        uint8_t val = (cbOpcode == 0x7E) ? read8(regs.hl) :
            ((cbOpcode == 0x78) ? regs.b :
                (cbOpcode == 0x79) ? regs.c :
                (cbOpcode == 0x7A) ? regs.d :
                (cbOpcode == 0x7B) ? regs.e :
                (cbOpcode == 0x7C) ? regs.h :
                (cbOpcode == 0x7D) ? regs.l :
                regs.a);
        regs.f = (regs.f & FLAG_C) | FLAG_H | ((val & (1 << 7)) ? 0 : FLAG_Z);
        break;
    }
    case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85: case 0x86:
    case 0x87: { // RES 0, r
        uint8_t mask = ~(1 << 0);
        if (cbOpcode == 0x86) {
            uint8_t val = read8(regs.hl);
            write8(regs.hl, val & mask);
        }
        else {
            uint8_t& r = (cbOpcode == 0x80) ? regs.b :
                (cbOpcode == 0x81) ? regs.c :
                (cbOpcode == 0x82) ? regs.d :
                (cbOpcode == 0x83) ? regs.e :
                (cbOpcode == 0x84) ? regs.h :
                (cbOpcode == 0x85) ? regs.l :
                regs.a;
            r &= mask;
        }
        break;
    }
    case 0x88: case 0x89: case 0x8A: case 0x8B: case 0x8C: case 0x8D: case 0x8E:
    case 0x8F: { // RES 1, r
        uint8_t mask = ~(1 << 1);
        if (cbOpcode == 0x8E) {
            uint8_t val = read8(regs.hl);
            write8(regs.hl, val & mask);
        }
        else {
            uint8_t& r = (cbOpcode == 0x88) ? regs.b :
                (cbOpcode == 0x89) ? regs.c :
                (cbOpcode == 0x8A) ? regs.d :
                (cbOpcode == 0x8B) ? regs.e :
                (cbOpcode == 0x8C) ? regs.h :
                (cbOpcode == 0x8D) ? regs.l :
                regs.a;
            r &= mask;
        }
        break;
    }
    case 0x90: case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96:
    case 0x97: { // RES 2, r
        uint8_t mask = ~(1 << 2);
        if (cbOpcode == 0x96) {
            uint8_t val = read8(regs.hl);
            write8(regs.hl, val & mask);
        }
        else {
            uint8_t& r = (cbOpcode == 0x90) ? regs.b :
                (cbOpcode == 0x91) ? regs.c :
                (cbOpcode == 0x92) ? regs.d :
                (cbOpcode == 0x93) ? regs.e :
                (cbOpcode == 0x94) ? regs.h :
                (cbOpcode == 0x95) ? regs.l :
                regs.a;
            r &= mask;
        }
        break;
    }
    case 0x98: case 0x99: case 0x9A: case 0x9B: case 0x9C: case 0x9D: case 0x9E:
    case 0x9F: { // RES 3, r
        uint8_t mask = ~(1 << 3);
        if (cbOpcode == 0x9E) {
            uint8_t val = read8(regs.hl);
            write8(regs.hl, val & mask);
        }
        else {
            uint8_t& r = (cbOpcode == 0x98) ? regs.b :
                (cbOpcode == 0x99) ? regs.c :
                (cbOpcode == 0x9A) ? regs.d :
                (cbOpcode == 0x9B) ? regs.e :
                (cbOpcode == 0x9C) ? regs.h :
                (cbOpcode == 0x9D) ? regs.l :
                regs.a;
            r &= mask;
        }
        break;
    }
    case 0xA0: case 0xA1: case 0xA2: case 0xA3: case 0xA4: case 0xA5: case 0xA6:
    case 0xA7: { // RES 4, r
        uint8_t mask = ~(1 << 4);
        if (cbOpcode == 0xA6) {
            uint8_t val = read8(regs.hl);
            write8(regs.hl, val & mask);
        }
        else {
            uint8_t& r = (cbOpcode == 0xA0) ? regs.b :
                (cbOpcode == 0xA1) ? regs.c :
                (cbOpcode == 0xA2) ? regs.d :
                (cbOpcode == 0xA3) ? regs.e :
                (cbOpcode == 0xA4) ? regs.h :
                (cbOpcode == 0xA5) ? regs.l :
                regs.a;
            r &= mask;
        }
        break;
    }

    case 0xA8: case 0xA9: case 0xAA: case 0xAB: case 0xAC: case 0xAD: case 0xAE:
    case 0xAF: { // RES 5, r
        uint8_t mask = ~(1 << 5);
        if (cbOpcode == 0xAE) {
            uint8_t val = read8(regs.hl);
            write8(regs.hl, val & mask);
        }
        else {
            uint8_t& r = (cbOpcode == 0xA8) ? regs.b :
                (cbOpcode == 0xA9) ? regs.c :
                (cbOpcode == 0xAA) ? regs.d :
                (cbOpcode == 0xAB) ? regs.e :
                (cbOpcode == 0xAC) ? regs.h :
                (cbOpcode == 0xAD) ? regs.l :
                regs.a;
            r &= mask;
        }
        break;
    }
    case 0xB0: case 0xB1: case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6:
    case 0xB7: { // RES 6, r
        uint8_t mask = ~(1 << 6);
        if (cbOpcode == 0xB6) {
            uint8_t val = read8(regs.hl);
            write8(regs.hl, val & mask);
        }
        else {
            uint8_t& r = (cbOpcode == 0xB0) ? regs.b :
                (cbOpcode == 0xB1) ? regs.c :
                (cbOpcode == 0xB2) ? regs.d :
                (cbOpcode == 0xB3) ? regs.e :
                (cbOpcode == 0xB4) ? regs.h :
                (cbOpcode == 0xB5) ? regs.l :
                regs.a;
            r &= mask;
        }
        break;
    }

    case 0xB8: case 0xB9: case 0xBA: case 0xBB: case 0xBC: case 0xBD: case 0xBE:
    case 0xBF: { // RES 7, r
        uint8_t mask = ~(1 << 7);
        if (cbOpcode == 0xBE) {
            uint8_t val = read8(regs.hl);
            write8(regs.hl, val & mask);
        }
        else {
            uint8_t& r = (cbOpcode == 0xB8) ? regs.b :
                (cbOpcode == 0xB9) ? regs.c :
                (cbOpcode == 0xBA) ? regs.d :
                (cbOpcode == 0xBB) ? regs.e :
                (cbOpcode == 0xBC) ? regs.h :
                (cbOpcode == 0xBD) ? regs.l :
                regs.a;
            r &= mask;
        }
        break;
    }




        // SET 0, r
    case 0xC0: regs.b |= (1 << 0); break;
    case 0xC1: regs.c |= (1 << 0); break;
    case 0xC2: regs.d |= (1 << 0); break;
    case 0xC3: regs.e |= (1 << 0); break;
    case 0xC4: regs.h |= (1 << 0); break;
    case 0xC5: regs.l |= (1 << 0); break;
    case 0xC6: {
        uint8_t value = read8(regs.hl);
        write8(regs.hl, value | (1 << 0));
        break;
    }
    case 0xC7: regs.a |= (1 << 0); break;

        // SET 1, r
    case 0xC8: regs.b |= (1 << 1); break;
    case 0xC9: regs.c |= (1 << 1); break;
    case 0xCA: regs.d |= (1 << 1); break;
    case 0xCB: regs.e |= (1 << 1); break;
    case 0xCC: regs.h |= (1 << 1); break;
    case 0xCD: regs.l |= (1 << 1); break;
    case 0xCE: {
        uint8_t value = read8(regs.hl);
        write8(regs.hl, value | (1 << 1));
        break;
    }
    case 0xCF: regs.a |= (1 << 1); break;


        // SET 2, r
    case 0xD0: regs.b |= (1 << 2); break;
    case 0xD1: regs.c |= (1 << 2); break;
    case 0xD2: regs.d |= (1 << 2); break;
    case 0xD3: regs.e |= (1 << 2); break;
    case 0xD4: regs.h |= (1 << 2); break;
    case 0xD5: regs.l |= (1 << 2); break;
    case 0xD6: {
        uint8_t value = read8(regs.hl);
        write8(regs.hl, value | (1 << 2));
        break;
    }
    case 0xD7: regs.a |= (1 << 2); break;

        // SET 3, r
    case 0xD8: regs.b |= (1 << 3); break;
    case 0xD9: regs.c |= (1 << 3); break;
    case 0xDA: regs.d |= (1 << 3); break;
    case 0xDB: regs.e |= (1 << 3); break;
    case 0xDC: regs.h |= (1 << 3); break;
    case 0xDD: regs.l |= (1 << 3); break;
    case 0xDE: {
        uint8_t value = read8(regs.hl);
        write8(regs.hl, value | (1 << 3));
        break;
    }
    case 0xDF: regs.a |= (1 << 3); break;

        // SET 4, r
    case 0xE0: regs.b |= (1 << 4); break;
    case 0xE1: regs.c |= (1 << 4); break;
    case 0xE2: regs.d |= (1 << 4); break;
    case 0xE3: regs.e |= (1 << 4); break;
    case 0xE4: regs.h |= (1 << 4); break;
    case 0xE5: regs.l |= (1 << 4); break;
    case 0xE6: {
        uint8_t value = read8(regs.hl);
        write8(regs.hl, value | (1 << 4));
        break;
    }
    case 0xE7: regs.a |= (1 << 4); break;

        // SET 5, r
    case 0xE8: regs.b |= (1 << 5); break;
    case 0xE9: regs.c |= (1 << 5); break;
    case 0xEA: regs.d |= (1 << 5); break;
    case 0xEB: regs.e |= (1 << 5); break;
    case 0xEC: regs.h |= (1 << 5); break;
    case 0xED: regs.l |= (1 << 5); break;
    case 0xEE: {
        uint8_t value = read8(regs.hl);
        write8(regs.hl, value | (1 << 5));
        break;
    }
    case 0xEF: regs.a |= (1 << 5); break;

        // SET 6, r
    case 0xF0: regs.b |= (1 << 6); break;
    case 0xF1: regs.c |= (1 << 6); break;
    case 0xF2: regs.d |= (1 << 6); break;
    case 0xF3: regs.e |= (1 << 6); break;
    case 0xF4: regs.h |= (1 << 6); break;
    case 0xF5: regs.l |= (1 << 6); break;
    case 0xF6: {
        uint8_t value = read8(regs.hl);
        write8(regs.hl, value | (1 << 6));
        break;
    }
    case 0xF7: regs.a |= (1 << 6); break;

        // SET 7, r
    case 0xF8: regs.b |= (1 << 7); break;
    case 0xF9: regs.c |= (1 << 7); break;
    case 0xFA: regs.d |= (1 << 7); break;
    case 0xFB: regs.e |= (1 << 7); break;
    case 0xFC: regs.h |= (1 << 7); break;
    case 0xFD: regs.l |= (1 << 7); break;
    case 0xFE: {
        uint8_t value = read8(regs.hl);
        write8(regs.hl, value | (1 << 7));
        break;
    }
    case 0xFF: regs.a |= (1 << 7); break;


    default:
        // Handle unimplemented CB instructions
        // You can throw an error or log it here
		printf("Unimplemented CB opcode: 0x%02X\n", cbOpcode);
        break;

    }
    


}

void CPU::executeInstruction() {
	conditional = false; // Reset conditional flag for each instruction
    //cycles += Tstates[opcode];  // Update cycles based on opcode timing
    //timer.update(Tstates[opcode] * 4, stopped); // Update timer before executing CB opcode

    switch (opcode) {
    case 0xCB: {
        uint8_t cbOpcode = read8(pc++);
        executeCB(cbOpcode);
        break;
    }
        // --- NOP ---
    case 0x00:
        break;

        // LD B, r
    case 0x40: regs.b = regs.b; break;
    case 0x41: regs.b = regs.c; break;
    case 0x42: regs.b = regs.d; break;
    case 0x43: regs.b = regs.e; break;
    case 0x44: regs.b = regs.h; break;
    case 0x45: regs.b = regs.l; break;
    case 0x46: regs.b = read8(regs.hl); break; // LD B, (HL)
    case 0x47: regs.b = regs.a; break;
        // LD C, r
    case 0x48: regs.c = regs.b; break;
    case 0x49: regs.c = regs.c; break;
    case 0x4A: regs.c = regs.d; break;
    case 0x4B: regs.c = regs.e; break;
    case 0x4C: regs.c = regs.h; break;
    case 0x4D: regs.c = regs.l; break;
    case 0x4E: regs.c = read8(regs.hl); break;
    case 0x4F: regs.c = regs.a; break;
        // LD D, r
    case 0x50: regs.d = regs.b; break;
    case 0x51: regs.d = regs.c; break;
    case 0x52: regs.d = regs.d; break;
    case 0x53: regs.d = regs.e; break;
    case 0x54: regs.d = regs.h; break;
    case 0x55: regs.d = regs.l; break;
    case 0x56: regs.d = read8(regs.hl); break;
    case 0x57: regs.d = regs.a; break;

        // LD E, r
    case 0x58: regs.e = regs.b; break;
    case 0x59: regs.e = regs.c; break;
    case 0x5A: regs.e = regs.d; break;
    case 0x5B: regs.e = regs.e; break;
    case 0x5C: regs.e = regs.h; break;
    case 0x5D: regs.e = regs.l; break;
    case 0x5E: regs.e = read8(regs.hl); break;
    case 0x5F: regs.e = regs.a; break;
        // LD H, r
    case 0x60: regs.h = regs.b; break;
    case 0x61: regs.h = regs.c; break;
    case 0x62: regs.h = regs.d; break;
    case 0x63: regs.h = regs.e; break;
    case 0x64: regs.h = regs.h; break;
    case 0x65: regs.h = regs.l; break;
    case 0x66: regs.h = read8(regs.hl); break;
    case 0x67: regs.h = regs.a; break;

        // LD L, r
    case 0x68: regs.l = regs.b; break;
    case 0x69: regs.l = regs.c; break;
    case 0x6A: regs.l = regs.d; break;
    case 0x6B: regs.l = regs.e; break;
    case 0x6C: regs.l = regs.h; break;
    case 0x6D: regs.l = regs.l; break;
    case 0x6E: regs.l = read8(regs.hl); break;
    case 0x6F: regs.l = regs.a; break;
        // LD (HL), r
    case 0x70: write8(regs.hl, regs.b); break;
    case 0x71: write8(regs.hl, regs.c); break;
    case 0x72: write8(regs.hl, regs.d); break;
    case 0x73: write8(regs.hl, regs.e); break;
    case 0x74: write8(regs.hl, regs.h); break;
    case 0x75: write8(regs.hl, regs.l); break;

    case 0x76: {// HALT
        
        uint8_t ie = read8(0xFFFF);
        uint8_t iff = read8(0xFF0F);
        bool x = (ie & iff & 0x1F) != 0;
        if (!x) {
                        // If no interrupts are enabled, halt the CPU
            halted = true;
        }
        else {
			haltbug = true; // Set a flag to indicate that the CPU is halted
        }
        
        break;
        

    }


    case 0x77: write8(regs.hl, regs.a); break;

        // LD A, r
    case 0x78: regs.a = regs.b; break;
    case 0x79: regs.a = regs.c; break;
    case 0x7A: regs.a = regs.d; break;
    case 0x7B: regs.a = regs.e; break;
    case 0x7C: regs.a = regs.h; break;
    case 0x7D: regs.a = regs.l; break;
    case 0x7E: regs.a = read8(regs.hl); break;
    case 0x7F: regs.a = regs.a; break;
        //---------------//
        // --- LD r, n ---
    case 0x06: regs.b = read8(pc++); break; // LD B, n
    case 0x0E: regs.c = read8(pc++); break; // LD C, n
    case 0x16: regs.d = read8(pc++); break; // LD D, n
    case 0x1E: regs.e = read8(pc++); break; // LD E, n
    case 0x26: regs.h = read8(pc++); break; // LD H, n
    case 0x2E: regs.l = read8(pc++); break; // LD L, n
    case 0x3E: regs.a = read8(pc++); break; // LD A, n

        //--------------//

        //--------------------//
        // --- ALU: ADD A, r ---
    case 0x80: regs.a = add8(regs.a, regs.b); break; // ADD A, B
    case 0x81: regs.a = add8(regs.a, regs.c); break; // ADD A, C
    case 0x82: regs.a = add8(regs.a, regs.d); break; // ADD A, D
    case 0x83: regs.a = add8(regs.a, regs.e); break; // ADD A, E
    case 0x84: regs.a = add8(regs.a, regs.h); break; // ADD A, H
    case 0x85: regs.a = add8(regs.a, regs.l); break; // ADD A, L
    case 0x86: regs.a = add8(regs.a, read8(regs.hl)); break; // ADD A, (HL)
    case 0x87: regs.a = add8(regs.a, regs.a); break; // ADD A, A

    case 0x88: regs.a = adc8(regs.a, regs.b); break;
    case 0x89: regs.a = adc8(regs.a, regs.c); break;
    case 0x8A: regs.a = adc8(regs.a, regs.d); break;
    case 0x8B: regs.a = adc8(regs.a, regs.e); break;
    case 0x8C: regs.a = adc8(regs.a, regs.h); break;
    case 0x8D: regs.a = adc8(regs.a, regs.l); break;
    case 0x8E: regs.a = adc8(regs.a, read8(regs.hl)); break;
    case 0x8F: regs.a = adc8(regs.a, regs.a); break;


		// --- ALU: SUB A, r ---
    case 0x90: regs.a = sub8(regs.a, regs.b); break;
    case 0x91: regs.a = sub8(regs.a, regs.c); break;
    case 0x92: regs.a = sub8(regs.a, regs.d); break;
    case 0x93: regs.a = sub8(regs.a, regs.e); break;
    case 0x94: regs.a = sub8(regs.a, regs.h); break;
    case 0x95: regs.a = sub8(regs.a, regs.l); break;
    case 0x96: regs.a = sub8(regs.a, read8(regs.hl)); break;
    case 0x97: regs.a = sub8(regs.a, regs.a); break;

    case 0x98: regs.a = sbc8(regs.a, regs.b); break;
    case 0x99: regs.a = sbc8(regs.a, regs.c); break;
    case 0x9A: regs.a = sbc8(regs.a, regs.d); break;
    case 0x9B: regs.a = sbc8(regs.a, regs.e); break;
    case 0x9C: regs.a = sbc8(regs.a, regs.h); break;
    case 0x9D: regs.a = sbc8(regs.a, regs.l); break;
    case 0x9E: regs.a = sbc8(regs.a, read8(regs.hl)); break;
    case 0x9F: regs.a = sbc8(regs.a, regs.a); break;
		//--------------//
		// --- ALU: AND A, r ---
    case 0xA0: regs.a &= regs.b; regs.f = (regs.a == 0 ? FLAG_Z : 0) | FLAG_H; break;
    case 0xA1: regs.a &= regs.c; regs.f = (regs.a == 0 ? FLAG_Z : 0) | FLAG_H; break;
    case 0xA2: regs.a &= regs.d; regs.f = (regs.a == 0 ? FLAG_Z : 0) | FLAG_H; break;
    case 0xA3: regs.a &= regs.e; regs.f = (regs.a == 0 ? FLAG_Z : 0) | FLAG_H; break;
    case 0xA4: regs.a &= regs.h; regs.f = (regs.a == 0 ? FLAG_Z : 0) | FLAG_H; break;
    case 0xA5: regs.a &= regs.l; regs.f = (regs.a == 0 ? FLAG_Z : 0) | FLAG_H; break;
    case 0xA6: regs.a &= read8(regs.hl); regs.f = (regs.a == 0 ? FLAG_Z : 0) | FLAG_H; break;
    case 0xA7: regs.a &= regs.a; regs.f = (regs.a == 0 ? FLAG_Z : 0) | FLAG_H; break;

    case 0xA8: regs.a ^= regs.b; regs.setFlags(0, regs.a); break;
    case 0xA9: regs.a ^= regs.c; regs.setFlags(0, regs.a); break;
    case 0xAA: regs.a ^= regs.d; regs.setFlags(0, regs.a); break;
    case 0xAB: regs.a ^= regs.e; regs.setFlags(0, regs.a); break;
    case 0xAC: regs.a ^= regs.h; regs.setFlags(0, regs.a); break;
    case 0xAD: regs.a ^= regs.l; regs.setFlags(0, regs.a); break;
    case 0xAE: regs.a ^= read8(regs.hl); regs.setFlags(0, regs.a); break;
    case 0xAF: regs.a ^= regs.a; regs.setFlags(0, regs.a); break;

    case 0xB0: regs.a |= regs.b; regs.f = (regs.a == 0 ? FLAG_Z : 0); break;
    case 0xB1: regs.a |= regs.c; regs.f = (regs.a == 0 ? FLAG_Z : 0); break;
    case 0xB2: regs.a |= regs.d; regs.f = (regs.a == 0 ? FLAG_Z : 0); break;
    case 0xB3: regs.a |= regs.e; regs.f = (regs.a == 0 ? FLAG_Z : 0); break;
    case 0xB4: regs.a |= regs.h; regs.f = (regs.a == 0 ? FLAG_Z : 0); break;
    case 0xB5: regs.a |= regs.l; regs.f = (regs.a == 0 ? FLAG_Z : 0); break;
    case 0xB6: regs.a |= read8(regs.hl); regs.f = (regs.a == 0 ? FLAG_Z : 0); break;
    case 0xB7: regs.a |= regs.a; regs.f = (regs.a == 0 ? FLAG_Z : 0); break;

    case 0xB8: cp8(regs.a, regs.b); break;
    case 0xB9: cp8(regs.a, regs.c); break;
    case 0xBA: cp8(regs.a, regs.d); break;
    case 0xBB: cp8(regs.a, regs.e); break;
    case 0xBC: cp8(regs.a, regs.h); break;
    case 0xBD: cp8(regs.a, regs.l); break;
    case 0xBE: cp8(regs.a, read8(regs.hl)); break;
    case 0xBF: cp8(regs.a, regs.a); break;

             // --- INC r ---
    case 0x04: regs.b = inc8(regs.b); break;
    case 0x0C: regs.c = inc8(regs.c); break;
    case 0x14: regs.d = inc8(regs.d); break;
    case 0x1C: regs.e = inc8(regs.e); break;
    case 0x24: regs.h = inc8(regs.h); break;
    case 0x2C: regs.l = inc8(regs.l); break;
    case 0x3C: regs.a = inc8(regs.a); break;

        // --- DEC r ---
    case 0x05: regs.b = dec8(regs.b); break;
    case 0x0D: regs.c = dec8(regs.c); break;
    case 0x15: regs.d = dec8(regs.d); break;
    case 0x1D: regs.e = dec8(regs.e); break;
    case 0x25: regs.h = dec8(regs.h); break;
    case 0x2D: regs.l = dec8(regs.l); break;
    case 0x3D: regs.a = dec8(regs.a); break;

        // --- JP addr ---
    case 0xC3: {
        uint16_t addr = read8(pc++);
        addr |= read8(pc++) << 8;
        pc = addr;
        break;
    }
             // --- Conditional JP ---
        // JP NZ, nn
    case 0xC2: {
        uint16_t addr = read8(pc++);
        addr |= read8(pc++) << 8;
        if (!regs.getFlag(FLAG_Z)) {
            pc = addr;
			conditional = true; // Set conditional flag
        }

        break;
    }
             // JP Z, nn
    case 0xCA: {
        uint16_t addr = read8(pc++);
        addr |= read8(pc++) << 8;
        if (regs.getFlag(FLAG_Z)) {
            pc = addr;
			conditional = true; // Set conditional flag
        }
        break;
    }
             // JP NC, nn
    case 0xD2: {
        uint16_t addr = read8(pc++);
        addr |= read8(pc++) << 8;
        if (!regs.getFlag(FLAG_C)) { 
            pc = addr; 
			conditional = true; // Set conditional flag
        }
        break;
    }
             // JP C, nn
    case 0xDA: {
        uint16_t addr = read8(pc++);
        addr |= read8(pc++) << 8;
        if (regs.getFlag(FLAG_C)) {
            pc = addr;
			conditional = true; // Set conditional flag
        }
        break;
    }


             // --- CALL addr ---
    case 0xCD: {
        uint16_t addr = read8(pc++);
        addr |= read8(pc++) << 8;
        sp--;
        write8(sp, (pc >> 8) & 0xFF);  // Push high byte
        sp--;
        write8(sp, pc & 0xFF);         // Push low byte

        pc = addr;
        break;
    }
             // CALL NZ, nn
    case 0xC4: {
        uint16_t addr = read8(pc++);
        addr |= read8(pc++) << 8;
        if (!regs.getFlag(FLAG_Z)) {
			conditional = true; // Set conditional flag
            sp--;
            write8(sp, (pc >> 8) & 0xFF);
            sp--;
            write8(sp, pc & 0xFF);
            pc = addr;
        }
        break;
    }
             // CALL Z, nn
    case 0xCC: {
        uint16_t addr = read8(pc++);
        addr |= read8(pc++) << 8;
        if (regs.getFlag(FLAG_Z)) {
			conditional = true; // Set conditional flag
            sp--;
            write8(sp, (pc >> 8) & 0xFF);
            sp--;
            write8(sp, pc & 0xFF);
            pc = addr;
        }
        break;
    }
             // CALL NC, nn
    case 0xD4: {
        uint16_t addr = read8(pc++);
        addr |= read8(pc++) << 8;
        if (!regs.getFlag(FLAG_C)) {
			conditional = true; // Set conditional flag
            sp--;
            write8(sp, (pc >> 8) & 0xFF);
            sp--;
            write8(sp, pc & 0xFF);
            pc = addr;
        }
        break;
    }
             // CALL C, nn
    case 0xDC: {
        uint16_t addr = read8(pc++);
        addr |= read8(pc++) << 8;
        if (regs.getFlag(FLAG_C)) {
			conditional = true; // Set conditional flag
            sp--;
            write8(sp, (pc >> 8) & 0xFF);
            sp--;
            write8(sp, pc & 0xFF);
            pc = addr;
        }
        break;
    }


             // --- RET ---
    case 0xC9: {
        uint16_t lo = read8(sp++);
        uint16_t hi = read8(sp++);
        pc = (hi << 8) | lo;
        break;
    }
             // RET NZ
    case 0xC0:
        if (!regs.getFlag(FLAG_Z)) {
			conditional = true; // Set conditional flag
            uint16_t lo = read8(sp++);
            uint16_t hi = read8(sp++);
            pc = (hi << 8) | lo;
        }
        break;
        // RET Z
    case 0xC8:
        if (regs.getFlag(FLAG_Z)) {
			conditional = true; // Set conditional flag
            uint16_t lo = read8(sp++);
            uint16_t hi = read8(sp++);
            pc = (hi << 8) | lo;
        }
        break;
        // RET NC
    case 0xD0:
        if (!regs.getFlag(FLAG_C)) {
			conditional = true; // Set conditional flag
            uint16_t lo = read8(sp++);
            uint16_t hi = read8(sp++);
            pc = (hi << 8) | lo;
        }
        break;
        // RET C
    case 0xD8:
        if (regs.getFlag(FLAG_C)) {
			conditional = true; // Set conditional flag
            uint16_t lo = read8(sp++);
            uint16_t hi = read8(sp++);
            pc = (hi << 8) | lo;
        }
        break;
        // --- PUSH rr ---
    case 0xF5: sp--; write8(sp, regs.a); sp--; write8(sp, regs.f); break; // PUSH AF
    case 0xC5: sp--; write8(sp, regs.b); sp--; write8(sp, regs.c); break; // PUSH BC
    case 0xD5: sp--; write8(sp, regs.d); sp--; write8(sp, regs.e); break; // PUSH DE
    case 0xE5: sp--; write8(sp, regs.h); sp--; write8(sp, regs.l); break; // PUSH HL

        // --- POP rr ---
    
    case 0xF1: {
        regs.f = read8(sp++) & 0xF0; // Pop F (flags), mask lower 4 bits
        regs.a = read8(sp++);        // Pop A (accumulator)
        break; // POP AF
    }
    case 0xC1: regs.c = read8(sp++); regs.b = read8(sp++); break; // POP BC
    case 0xD1: regs.e = read8(sp++); regs.d = read8(sp++); break; // POP DE
    case 0xE1: regs.l = read8(sp++); regs.h = read8(sp++); break; // POP HL

        // --- Miscellaneous ops ---
    case 0x3F: regs.f ^= FLAG_C; regs.f &= ~(FLAG_H | FLAG_N); break; // CCF
    case 0x37: regs.f &= ~(FLAG_N | FLAG_H); regs.f |= FLAG_C; break; // SCF
    case 0x2F: regs.a ^= 0xFF; regs.f |= FLAG_N | FLAG_H; break;      // CPL

        // --- LD A, (nn) and LD (nn), A ---
    case 0xFA: {
        uint16_t addr = read8(pc++);
        addr |= read8(pc++) << 8;
        regs.a = read8(addr);
        break;
    }
    case 0xEA: {
        uint16_t addr = read8(pc++);
        addr |= read8(pc++) << 8;
        write8(addr, regs.a);
        break;
    }

             // --- JP (HL) ---
    case 0xE9: pc = regs.hl; break;

        // --- LD SP, HL ---
    case 0xF9: sp = regs.hl; break;

        // --- LD (nn), SP ---
    case 0x08: {
        uint16_t addr = read8(pc++);
        addr |= read8(pc++) << 8;
        write8(addr, sp & 0xFF);
        write8(addr + 1, (sp >> 8) & 0xFF);
        break;
    }

             // --- 16-bit LD instructions ---
    case 0x01: regs.bc = read8(pc++); regs.bc |= read8(pc++) << 8; break; // LD BC, nn
    case 0x11: regs.de = read8(pc++); regs.de |= read8(pc++) << 8; break; // LD DE, nn
    case 0x21: regs.hl = read8(pc++); regs.hl |= read8(pc++) << 8; break; // LD HL, nn
    case 0x31: sp = read8(pc++); sp |= read8(pc++) << 8; break; // LD SP, nn

    case 0x02: write8(regs.bc, regs.a); break; // LD (BC), A
    case 0x12: write8(regs.de, regs.a); break; // LD (DE), A
    case 0x0A: regs.a = read8(regs.bc); break; // LD A, (BC)
    case 0x1A: regs.a = read8(regs.de); break; // LD A, (DE)

    case 0x22: write8(regs.hl, regs.a); regs.hl++; break; // LD (HL+), A
    case 0x2A: regs.a = read8(regs.hl); regs.hl++; break; // LD A, (HL+)
    case 0x32: write8(regs.hl, regs.a); regs.hl--; break; // LD (HL-), A
    case 0x3A: regs.a = read8(regs.hl); regs.hl--; break; // LD A, (HL-)
		// --- ADD HL, rr ---
    case 0x09: { // ADD HL, BC
        uint32_t result = regs.hl + regs.bc;
        regs.f &= ~FLAG_N;
        if (((regs.hl & 0xFFF) + (regs.bc & 0xFFF)) > 0xFFF) regs.f |= FLAG_H; else regs.f &= ~FLAG_H;
        if (result > 0xFFFF) regs.f |= FLAG_C; else regs.f &= ~FLAG_C;
        regs.hl = result & 0xFFFF;
        break;
    }
    case 0x19: { // ADD HL, DE
        uint32_t result = regs.hl + regs.de;
        regs.f &= ~FLAG_N;
        if (((regs.hl & 0xFFF) + (regs.de & 0xFFF)) > 0xFFF) regs.f |= FLAG_H; else regs.f &= ~FLAG_H;
        if (result > 0xFFFF) regs.f |= FLAG_C; else regs.f &= ~FLAG_C;
        regs.hl = result & 0xFFFF;
        break;
    }
    case 0x29: { // ADD HL, HL
        uint32_t result = regs.hl + regs.hl;
        regs.f &= ~FLAG_N;
        if (((regs.hl & 0xFFF) + (regs.hl & 0xFFF)) > 0xFFF) regs.f |= FLAG_H; else regs.f &= ~FLAG_H;
        if (result > 0xFFFF) regs.f |= FLAG_C; else regs.f &= ~FLAG_C;
        regs.hl = result & 0xFFFF;
        break;
    }
    case 0x39: { // ADD HL, SP
        uint32_t result = regs.hl + sp;
        regs.f &= ~FLAG_N;
        if (((regs.hl & 0xFFF) + (sp & 0xFFF)) > 0xFFF) regs.f |= FLAG_H; else regs.f &= ~FLAG_H;
        if (result > 0xFFFF) regs.f |= FLAG_C; else regs.f &= ~FLAG_C;
        regs.hl = result & 0xFFFF;
        break;
    }

			 // --- INC rr ---
    case 0x03: regs.bc++; break;
    case 0x13: regs.de++; break;
    case 0x23: regs.hl++; break;
    case 0x33: sp++; break;
		// --- DEC rr ---
    case 0x0B: regs.bc--; break;
    case 0x1B: regs.de--; break;
    case 0x2B: regs.hl--; break;
    case 0x3B: sp--; break;
        // --- ADD SP, r8 ---
    case 0xE8: {
        int8_t value = static_cast<int8_t>(read8(pc++));
        uint16_t sp_before = sp;
        uint16_t result = sp_before + value;

        regs.f = 0; // Clear Z and N

        if (((sp_before ^ value ^ result) & 0x10) != 0) regs.f |= FLAG_H;
        if (((sp_before ^ value ^ result) & 0x100) != 0) regs.f |= FLAG_C;

        sp = result;
        break;
    }

             //--- LD HL, SP + r8 ---
    case 0xF8: {
        int8_t value = static_cast<int8_t>(read8(pc++));
        regs.f = 0; // Clear Z and N
        if (((sp & 0xF) + (value & 0xF)) > 0xF) regs.f |= FLAG_H;
        if (((sp & 0xFF) + (value & 0xFF)) > 0xFF) regs.f |= FLAG_C;
        regs.hl = static_cast<uint16_t>(sp + value);
        break;
    }
            //LD (FF00 + n), A 
    case 0xE0: {
        uint8_t offset = read8(pc++);
        write8(0xFF00 + offset, regs.a);
        break;
    }
			 // LD A, (FF00 + n) 
    case 0xF0: {
        uint8_t offset = read8(pc++);
        regs.a = read8(0xFF00 + offset);
        break;
    }
			 // LD (FF00 + C), A
    case 0xE2:
        write8(0xFF00 + regs.c, regs.a);
        break;
		// // LD A, (FF00 + C)
    case 0xF2:
        regs.a = read8(0xFF00 + regs.c);
        break;
		// --- DI and EI ---
    case 0xF3:
        ime = false;
        break;
	case 0xFB:
        enableInterruptsNext = true ;
		break;
    case 0x10:
		read8(pc++); // STOP instruction, ignored in this implementation
        stopped = true;
        break;
        // --- RETI ---
    case 0xD9: {
        uint16_t lo = read8(sp++);
        uint16_t hi = read8(sp++);
        pc = (hi << 8) | lo;
        ime = true; // Enable interrupts after RETI
        break;
    }
    case 0x27: {  // DAA
        uint8_t a = regs.a;
        uint8_t f = regs.f;

        bool n = f & FLAG_N;
        bool h = f & FLAG_H;
        bool c = f & FLAG_C;

        uint8_t corr = 0;

        if (!n) {                       // after ADD / ADC
            if (h || (a & 0x0F) > 9)     corr |= 0x06;
            if (c || a > 0x99) {
                corr |= 0x60;
                c = true;               // will set carry
            }
            a += corr;
        }
        else {                        // after SUB / SBC
            if (h)                      corr |= 0x06;
            if (c)                      corr |= 0x60;
            a -= corr;
            // carry flag unchanged in SUB unless we subtracted 0x60
            c = c || (corr >= 0x60);
        }

        // set A
        regs.a = a;

        // rebuild F: Z N H C
        regs.f = 0;
        if (a == 0) regs.f |= FLAG_Z;
        if (n)      regs.f |= FLAG_N;
        if (c)      regs.f |= FLAG_C;
        // H always 0

        break;
    }



    


			 // --- JR instructions ---
    case 0x18: { // JR r8 (unconditional)
        int8_t offset = static_cast<int8_t>(read8(pc++));
        pc += offset;
        break;
    }
    case 0x20: { // JR NZ, r8
        int8_t offset = static_cast<int8_t>(read8(pc++));
        if (!regs.getFlag(FLAG_Z)) {
            pc += offset;
			conditional = true; // Set conditional flag
        }
        break;
    }
    case 0x28: { // JR Z, r8
        int8_t offset = static_cast<int8_t>(read8(pc++));
        if (regs.getFlag(FLAG_Z)) {
            pc += offset;
			conditional = true; // Set conditional flag
        }
        break;
    }
    case 0x30: { // JR NC, r8
        int8_t offset = static_cast<int8_t>(read8(pc++));
        if (!regs.getFlag(FLAG_C)) {
            pc += offset;
			conditional = true; // Set conditional flag
        }
        break;
    }
    case 0x38: { // JR C, r8
        int8_t offset = static_cast<int8_t>(read8(pc++));
        if (regs.getFlag(FLAG_C)) {
            pc += offset;
			conditional = true; // Set conditional flag
        }

        break;
    }
			 // --- LD A, (n) and LD (n), A ---
    case 0xFE: {
        uint8_t value = read8(pc++);
        cp8(regs.a, value);
        break;
    }
			 // AND n 
    case 0xE6: {
        uint8_t value = read8(pc++);
        regs.a &= value;
        regs.f = (regs.a == 0 ? FLAG_Z : 0) | FLAG_H;
        break;
    }
    case 0xC6: {
        uint8_t value = read8(pc++);
        regs.a = add8(regs.a, value); 
        break;
    }
    case 0xD6: {
        uint8_t value = read8(pc++);
        regs.a = sub8(regs.a, value); // Ensure sub8 sets Z, N, H, C flags
        break;
    }
    case 0xEE: {
        uint8_t value = read8(pc++);
        regs.a ^= value;
        regs.setFlags(0, regs.a);
        break;
    }
    case 0xF6: {
        uint8_t value = read8(pc++);
        regs.a |= value;
        regs.f = (regs.a == 0 ? FLAG_Z : 0);
        break;
    }
    case 0x1F: { // RRA
        bool carry = regs.a & 0x01;
        regs.a = (regs.a >> 1) | (regs.getFlag(FLAG_C) ? 0x80 : 0x00);
        regs.f = 0;
        if (carry) regs.f |= FLAG_C;
        break;
    }
             //ADC A, n
    case 0xCE: {
        uint8_t value = read8(pc++);
        regs.a = adc8(regs.a, value);
        break;
    }
			 //DEC (HL)
    case 0x35: {
        uint8_t val = read8(regs.hl);
        val = dec8(val); // Your dec8() should handle flags correctly
        write8(regs.hl, val);
        break;
    }
    case 0x07: { // RLCA
        uint8_t carry = (regs.a & 0x80) >> 7;
        regs.a = (regs.a << 1) | carry;

        regs.f = 0;
        if (carry)
            regs.f |= FLAG_C;
        break;
    }
    case 0x17: { // RLA
        uint8_t oldCarry = (regs.getFlag(FLAG_C) ? 1 : 0);
        uint8_t newCarry = (regs.a & 0x80) >> 7;

        regs.a = (regs.a << 1) | oldCarry;

        regs.f = 0;
        if (newCarry)
            regs.f |= FLAG_C;
        break;
    }
    case 0x0F: { // RRCA
        uint8_t carry = regs.a & 0x01;
        regs.a = (regs.a >> 1) | (carry << 7);

        regs.f = 0;
        if (carry)
            regs.f |= FLAG_C;
        break;
    }
	case 0x36: { // LD (HL), n
        uint8_t value = read8(pc++);
        write8(regs.hl, value);
        break;
    }
	case 0xDE: { // SBC A, n
        uint8_t value = read8(pc++);
        regs.a = sbc8(regs.a, value);
        break;
    }
    case 0x34: { // INC (HL)
    
             uint8_t val = read8(regs.hl);
             uint8_t result = val + 1;
             // Set flags
             regs.f &= ~FLAG_N;                     // Reset N
             regs.f = (result == 0) ? (regs.f | FLAG_Z) : (regs.f & ~FLAG_Z); // Z
             regs.f = ((val & 0x0F) + 1 > 0x0F) ? (regs.f | FLAG_H) : (regs.f & ~FLAG_H); // H

             write8(regs.hl, result);
             break;
    }
             // RST 00H
    case 0xC7: {
        sp--;
        write8(sp, (pc >> 8) & 0xFF);  // Push high byte
        sp--;
        write8(sp, pc & 0xFF);         // Push low byte
        pc = 0x0000;                   // Jump to address 0x0000
        break;

    }
    case 0xCF: { // RST 08H
        sp--;
        write8(sp, (pc >> 8) & 0xFF);  // Push high byte
        sp--;
        write8(sp, pc & 0xFF);         // Push low byte
        pc = 0x0008;                   // Jump to address 0x0008
		break;
    }
    case 0xD7: { // RST 10H
        sp--;
        write8(sp, (pc >> 8) & 0xFF);  // Push high byte
        sp--;
        write8(sp, pc & 0xFF);         // Push low byte
		pc = 0x0010;                   // Jump to address 0x0010
        break;
    }
    case 0xDF: { // RST 18H
        sp--;
        write8(sp, (pc >> 8) & 0xFF);  // Push high byte
        sp--;
        write8(sp, pc & 0xFF);         // Push low byte
        pc = 0x0018;                   // Jump to address 0x0018
		break;
    }
    case 0xE7: { // RST 20H
        sp--;
        write8(sp, (pc >> 8) & 0xFF);  // Push high byte
        sp--;
        write8(sp, pc & 0xFF);         // Push low byte
		pc = 0x0020;                   // Jump to address 0x0020
        break;
    }
    case 0xEF: { // RST 28H
        sp--;
        write8(sp, (pc >> 8) & 0xFF);  // Push high byte
        sp--;
		write8(sp, pc & 0xFF);         // Push low byte
        pc = 0x0028;                   // Jump to address 0x0028
		break;
    }
             // RST 30h
    case 0xF7: {
        sp--;
        write8(sp, (pc >> 8) & 0xFF);
        sp--;
        write8(sp, pc & 0xFF);
        pc = 0x0030;
        break;
    }

             // RST 38h
    case 0xFF: {
        sp--;
        write8(sp, (pc >> 8) & 0xFF);
        sp--;
        write8(sp, pc & 0xFF);
        pc = 0x0038;
        break;
    }


             // --- Unknown ---
    default:
        printf("Unknown opcode: 0x%02X at PC: 0x%04X\n", opcode, pc - 1);
		halted = true; // Halt the CPU on unknown opcode
        break;
    }
    if (conditional) {
		//printf("Conditional instruction executed: 0x%02X at PC: 0x%04X\n", opcode, pc - 1);
		cycles += TstatesConditional[opcode]; // Add cycles for conditional instructions
    }
    else {
        // Non-conditional instructions just take their normal cycle count
        cycles += Tstates[opcode];
    }

}
