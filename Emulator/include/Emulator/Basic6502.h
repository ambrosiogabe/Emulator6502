#ifndef EMULATOR_BASIC_6502_H
#define EMULATOR_BASIC_6502_H
#include "utils/SafeVendor.h"

typedef enum emu_basic6502_statusFlag
{
	CarryBit           = (1 << 0),
	Zero               = (1 << 1),
	DisableInterrupts  = (1 << 2),
	DecimalMode        = (1 << 3),
	Break              = (1 << 4),
	Unused             = (1 << 5),
	Overflow           = (1 << 6),
	Negative           = (1 << 7),
} emu_basic6502_statusFlag;

typedef struct emu_bus emu_bus;

typedef struct emu_basic6502
{
	emu_bus* bus;

	uint8 accumulator;
	uint8 x;
	uint8 y;
	uint8 stackPointer;
	uint16 programCounter;
	emu_basic6502_statusFlag status;

	uint8 fetched;
	uint16 absAddress;
	uint16 relAddress;
	uint8 opcode;
	uint8 cycles;
} emu_basic6502;

void emu_basic6502_init(emu_basic6502* cpu);

inline void emu_basic6502_connectBus(emu_basic6502* cpu, emu_bus* bus) { cpu->bus = bus; }

void emu_basic6502_clock(emu_basic6502* cpu);
void emu_basic6502_reset(emu_basic6502* cpu);
void emu_basic6502_irq(emu_basic6502* cpu);
void emu_basic6502_nmi(emu_basic6502* cpu);

uint8 emu_basic6502_read(emu_basic6502* const cpu, uint16 addr);
void emu_basic6502_write(emu_basic6502* const cpu, uint16 addr, uint8 data);

#endif