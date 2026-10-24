#ifndef EMU_ASSEMBLER_H
#define EMU_ASSEMBLER_H
#include "utils/SafeVendor.h"

typedef struct emu_MemoryMap emu_MemoryMap;
typedef struct emu_appData_SourceFile emu_appData_SourceFile;

#define EMU_BIN_HEADER_SIZE 16

typedef struct emu_assembler_program
{
	uint8* prgRom;
	size_t prgRomSize;

	uint8* chrRom;
	size_t chrRomSize;

	uint8 header[EMU_BIN_HEADER_SIZE];
} emu_assembler_program;

emu_assembler_program emu_assembler_assembleAndLinkProgram(
	emu_MemoryMap const* const mmap,
	const emu_appData_SourceFile* sourceFiles,
	size_t prgRomSize,
	size_t chrRomSize
);
void emu_assembler_free(emu_assembler_program* program);

#endif