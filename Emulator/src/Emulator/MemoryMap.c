#include "Emulator/MemoryMap.h"

emu_MemoryMap emu_mmap_newNesMap(size_t physicalMemorySize)
{
	g_logger_assert(physicalMemorySize == UINT16_MAX + 1, "Only exactly correct memory right now.");
	uint8* physicalMemory = g_memory_allocate(physicalMemorySize);

	emu_AddressRange sram = { .start = 0x0, .end = 0x1fff };
	emu_AddressRange ppuRegisterRange = { .start = 0x2000, .end = 0x3fff };
	emu_AddressRange apuRegisterRange = { .start = 0x4000, .end = 0x4017 };
	emu_AddressRange cartridgeExpansion = { .start = 0x4018, .end = 0x5fff };
	emu_AddressRange cartridgeSram = { .start = 0x6000, .end = 0x7fff };
	// TODO: Rom should go to 0xffff
	emu_AddressRange rom = { .start = 0x8000, .end = 0xfff9 };

	emu_AddressRange romv = { .start = 0xfffa, .end = 0xffff };
	emu_AddressRange header = { .start = 0x0, .end = 0xf };
	emu_AddressRange stack = { .start = 0x0100, .end = 0x1ff };

	emu_MemoryMap res = {
		.physicalMemory = physicalMemory,
		.physicalMemorySize = physicalMemorySize,
		.sram = sram,
		.ppuRegisterRange = ppuRegisterRange,
		.apuRegisterRange = apuRegisterRange,
		.cartridgeExpansion = cartridgeExpansion,
		.cartridgeSram = cartridgeSram,
		.rom = rom,
		.romv = romv,

		.header = header,
		.stack = stack,
	};

	g_memory_zeroMem(res.physicalMemory, res.physicalMemorySize);

	return res;
}

emu_Ppu_MemoryMap emu_mmap_newNesPpuMap()
{
	emu_AddressRange patternTable0 = {.start = 0x0, .end = 0xfff };
	emu_AddressRange patternTable1 = { .start = 0x1000, .end = 0x1fff };
	emu_AddressRange nameAttrTable0 = { .start = 0x2000, .end = 0x23ff};
	emu_AddressRange nameAttrTable1 = { .start = 0x2400, .end = 0x27ff };
	emu_AddressRange nameAttrTable2 = { .start = 0x2800, .end = 0x2bff };
	emu_AddressRange nameAttrTable3 = { .start = 0x2c00, .end = 0x2fff };
	emu_AddressRange nameAttrTableMirrors = { .start = 0x3000, .end = 0x3fff };
	emu_AddressRange backgroundAndSpritePalettes = { .start = 0x3f00, .end = 0x3f1f };

	emu_Ppu_MemoryMap res = {
		.patternTable0 = patternTable0,
		.patternTable1 = patternTable1,
		.nameAttrTable0 = nameAttrTable0,
		.nameAttrTable1 = nameAttrTable1,
		.nameAttrTable2 = nameAttrTable2,
		.nameAttrTable3 = nameAttrTable3,
		.nameAttrTableMirrors = nameAttrTableMirrors,
		.backgroundAndSpritePalettes = backgroundAndSpritePalettes
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

bool emu_mmap_inRange(emu_AddressRange const* range, uint16 address)
{
	return address >= range->start && address <= range->end;
}