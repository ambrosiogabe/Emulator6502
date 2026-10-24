#ifndef EMU_MEMORY_MAP_H
#define EMU_MEMORY_MAP_H
#include "utils/SafeVendor.h"
#include "Emulator/Chips.h"

#define PAGE_SIZE 256
#define STACK_BASE 0x1ff

typedef struct emu_AddressRange
{
	uint16 start;
	uint16 end;
} emu_AddressRange;

typedef struct emu_MemoryMap
{
	uint8* physicalMemory;
	size_t physicalMemorySize;

	emu_AddressRange sram;
	emu_AddressRange ppuRegisterRange;
	emu_AddressRange apuRegisterRange;
	emu_AddressRange cartridgeExpansion;
	emu_AddressRange cartridgeSram;

	emu_AddressRange rom;
	// ROM Vector, for Hardware Vectors
	emu_AddressRange romv;
	emu_AddressRange header;
	emu_AddressRange stack;
} emu_MemoryMap;

typedef struct emu_Ppu_MemoryMap
{
	emu_AddressRange patternTable0;
	emu_AddressRange patternTable1;
	emu_AddressRange nameAttrTable0;
	emu_AddressRange nameAttrTable1;
	emu_AddressRange nameAttrTable2;
	emu_AddressRange nameAttrTable3;
	emu_AddressRange nameAttrTableMirrors;
	emu_AddressRange backgroundAndSpritePalettes;
} emu_Ppu_MemoryMap;

emu_MemoryMap emu_mmap_newNesMap(size_t physicalMemorySize);
emu_Ppu_MemoryMap emu_mmap_newNesPpuMap();
void emu_mmap_free(emu_MemoryMap* mmap);
uint8* emu_mmap_getNesAddress(emu_MemoryMap* mmap, uint16 address);

size_t emu_mmap_getSize(emu_AddressRange range);
bool emu_mmap_inRange(emu_AddressRange const* range, uint16 address);

#endif 