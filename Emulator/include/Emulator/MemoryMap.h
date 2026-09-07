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
	//emu_AddressRange header;
	//emu_AddressRange sram;
	//emu_AddressRange ram;
	//emu_AddressRange rom0;
	//emu_AddressRange romv;
	//emu_AddressRange rom2;

	//uint8* headerPtr;
	//uint8* sramPtr;
	//uint8* ramPtr;
	//uint8* rom0Ptr;
	//uint8* romvPtr;
	//uint8* rom2Ptr;
	emu_AddressRange ram;
	emu_AddressRange io;
	emu_AddressRange rom;

	uint8* ramPtr;
	uint8* ioPtr;
	uint8* romPtr;
} emu_NesMemoryMap;

typedef struct emu_CommodoreMemoryMap
{
	emu_AddressRange header;
} emu_CommodoreMemoryMap;

typedef struct emu_MemoryMap
{
	uint8* physicalMemory;
	size_t physicalMemorySize;

	emu_AddressRange zeroPage;
	uint8* zeroPagePtr;
	emu_AddressRange stack;
	uint8* stackPtr;
	emu_AddressRange interruptVector;
	uint8* interruptVectorPtr;

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