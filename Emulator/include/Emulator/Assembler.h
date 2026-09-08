#ifndef EMU_ASSEMBLER_H
#define EMU_ASSEMBLER_H
#include "utils/SafeVendor.h"

typedef struct emu_MemoryMap emu_MemoryMap;

typedef struct emu_assembler_program
{
	uint8* data;
	size_t dataSize;

	uint8* header;
	size_t headerSize;
	uint8* rom;
	size_t romSize;
	uint8* romv;
	size_t romvSize;
} emu_assembler_program;

emu_assembler_program emu_assembler_assembleProgram(emu_MemoryMap const* const mmap, const char* filename, size_t programSize);
void emu_assembler_free(emu_assembler_program* program);

#endif