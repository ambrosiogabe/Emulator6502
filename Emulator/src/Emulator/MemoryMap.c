#include "Emulator/MemoryMap.h"

emu_MemoryMap emu_mmap_newNesMap(size_t physicalMemorySize)
{
	g_logger_assert(physicalMemorySize == UINT16_MAX + 1, "Only exactly correct memory right now.");
	uint8* physicalMemory = g_memory_allocate(physicalMemorySize);

	emu_AddressRange ram = { .start = 0x0000, .end = 0x7fff };
	emu_AddressRange rom = { .start = 0x8000, .end = 0xfff9 };
	emu_AddressRange romv = { .start = 0xfffa, .end = 0xffff };
	emu_AddressRange header = { .start = 0x0, .end = 0xf };
	emu_AddressRange stack = { .start = 0x0100, .end = 0x1ff };

	emu_MemoryMap res = {
		.physicalMemory = physicalMemory,
		.physicalMemorySize = physicalMemorySize,
		.as = (emu_NesMemoryMap) {
			.ram = ram,
			.rom = rom,
			.romv = romv,
			.header = header,
			.stack = stack,
},
	};

	return res;
}

void emu_mmap_free(emu_MemoryMap* mmap)
{
	if (mmap->physicalMemory)
	{
		g_memory_free(mmap->physicalMemory);
	}
}

uint8* emu_mmap_getNesAddress(emu_MemoryMap* mmap, uint16 address)
{
	return mmap->physicalMemory + address;
}

size_t emu_mmap_getSize(emu_AddressRange range)
{
	return range.end - range.start + 1;
}