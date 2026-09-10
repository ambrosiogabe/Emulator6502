#ifndef EMU_MEMORY_MAP_H
#define EMU_MEMORY_MAP_H
#include "utils/SafeVendor.h"

#define PAGE_SIZE 256

typedef struct emu_AddressRange
{
	uint16 start;
	uint16 end;
} emu_AddressRange;

typedef struct emu_NesMemoryMap
{
	// TODO: Figure out how to properly emulate this stuff
	emu_AddressRange ram;
	emu_AddressRange rom;
	// ROM Vector, for Hardware Vectors
	emu_AddressRange romv;
	emu_AddressRange header;
	emu_AddressRange stack;
} emu_NesMemoryMap;

typedef struct emu_CommodoreMemoryMap
{
	emu_AddressRange header;
} emu_CommodoreMemoryMap;

typedef struct emu_MemoryMap
{
	uint8* physicalMemory;
	size_t physicalMemorySize;

	union
	{
		emu_NesMemoryMap nes;
		emu_CommodoreMemoryMap commodore;
	} as;
} emu_MemoryMap;

emu_MemoryMap emu_mmap_newNesMap(size_t physicalMemorySize);
void emu_mmap_free(emu_MemoryMap* mmap);
uint8* emu_mmap_getNesAddress(emu_MemoryMap* mmap, uint16 address);

size_t emu_mmap_getSize(emu_AddressRange range);

#endif 