#ifndef EMULATOR_BUS_H
#define EMULATOR_BUS_H
#include <cppUtils/cppUtils.h>
#include "Basic6502.h"
#include "Types.h"

typedef enum emu_readType
{
	emu_readType_readOnly = 0,
	emu_readType_readWrite = 1,
} emu_readType;

#define MAX_RAM_SIZE KB(64)

typedef struct emu_bus
{
	emu_basic6502 cpu;
	uint8 ram[MAX_RAM_SIZE];
} emu_bus;

void emu_bus_init(emu_bus* const bus);
void emu_bus_write(emu_bus* const bus, uint16 addr, uint8 data);
uint8 emu_bus_read(emu_bus* const bus, uint16 addr, emu_readType readType);

#endif