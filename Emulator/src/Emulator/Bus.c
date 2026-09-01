#include "Emulator/Bus.h"

void emu_bus_init(emu_bus* const bus)
{
	g_memory_zeroMem(bus->ram, MAX_RAM_SIZE * sizeof(uint8));

	emu_basic6502_connectBus(&bus->cpu, bus);
}

void emu_bus_write(emu_bus* const bus, uint16 addr, uint8 data)
{
	if (addr >= 0x0000 && addr <= 0xFFFF)
	{
		bus->ram[addr] = data;
	}
}

uint8 emu_bus_read(emu_bus* const bus, uint16 addr, emu_readType readType)
{
	if (addr >= 0x0000 && addr <= 0xFFFF)
	{
		return bus->ram[addr];
	}

	return 0x00;
}