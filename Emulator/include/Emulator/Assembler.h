#ifndef EMU_ASSEMBLER_H
#define EMU_ASSEMBLER_H
#include <cppUtils/cppUtils.h>

typedef struct emu_assember_program
{
	uint8* program;
	size_t size;
} emu_assembler_program;

emu_assembler_program emu_assembler_assembleProgram(const char* filename, size_t programSize);
void emu_assembler_free(emu_assembler_program* program);

#endif