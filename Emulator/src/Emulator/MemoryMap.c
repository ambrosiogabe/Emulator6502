#include "Emulator/MemoryMap.h"

emu_MemoryMap emu_mmap_newNesMap(size_t physicalMemorySize)
{
	g_logger_assert(physicalMemorySize == UINT16_MAX + 1, "Only exactly correct memory right now.");
	uint8* physicalMemory = g_memory_allocate(physicalMemorySize);
	emu_AddressRange zeroPage = { .start = 0x00, .end = 0x00ff };
	emu_AddressRange stack = { .start = 0x0100, .end = 0x01ff };
	emu_AddressRange interruptVector = { .start = 0xfffa, .end = 0xffff };

	emu_AddressRange io = { .start = 0x8000, .end = 0xefff };
	emu_AddressRange ram = { .start = 0x0000, .end = 0x7fff };
	emu_AddressRange rom = { .start = 0xf000, .end = 0xffff };

	emu_MemoryMap res = {
		.physicalMemory = physicalMemory,
		.physicalMemorySize = physicalMemorySize,
		.zeroPage = zeroPage,
		.zeroPagePtr = physicalMemory,
		.stack = stack,
		.stackPtr = physicalMemory + stack.start,
		.interruptVector = interruptVector,
		.interruptVectorPtr = physicalMemory + interruptVector.start,
		.as = (emu_NesMemoryMap) {
			.io = io,
			.ioPtr = physicalMemory + io.start,
			.ram = ram,
			.ramPtr = physicalMemory + ram.start,
			.rom = rom,
			.romPtr = physicalMemory + rom.start
},
	};

	return res;

	//uint8* header = physicalMemory;
	//// NOTE: Zero Page and Header are both stored at the start of the physical memory
	//uint8* zeroPage = physicalMemory;
	//uint8* stackMemory = zeroPage + getSize(ZeroPageRange);
	//uint8* ioMemory = stackMemory + getSize(StackRange);
	//// NOTE: Interrupt vector is included in the address range of ROM
	//uint8* interruptVectorMemory = (physicalMemory + physicalMemorySize) - getSize(InterruptVectorRange);
	//uint8* romPtr = (physicalMemory + physicalMemorySize) - romSize;
	//// NOTE: We include the ZeroPage and stack Range in our RAM
	//uint8* ramPtr = zeroPage;
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
	return range.end - range.start;
}