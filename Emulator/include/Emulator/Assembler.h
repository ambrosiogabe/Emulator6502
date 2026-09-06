#ifndef EMU_ASSEMBLER_H
#define EMU_ASSEMBLER_H
#include "utils/SafeVendor.h"

typedef struct emu_assembler_program
{
	uint8* data;
	size_t size;
} emu_assembler_program;

emu_assembler_program emu_assembler_assembleProgram(const char* filename, size_t programSize);
void emu_assembler_free(emu_assembler_program* program);

#endif