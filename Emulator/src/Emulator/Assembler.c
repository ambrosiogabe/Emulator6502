#include "Emulator/Assembler.h"
#include "Emulator/Parser.h"

// Internal structures

// Internal Functions

// Public Functions
emu_assembler_program emu_assembler_assembleProgram(const char* filename, size_t _)
{
	emu_TokenList tokenList = emu_parser_parseFile(filename);
	for (size_t i = 0; i < emu_parser_tokenListLength(&tokenList); i++)
	{
		emu_parser_debugPrintToken(&tokenList, i);
	}

	emu_parser_freeTokenList(&tokenList);

	return (emu_assembler_program)
	{
		.program = NULL,
			.size = 0,
	};
}

void emu_assembler_free(emu_assembler_program* program)
{
	if (!program)
	{
		return;
	}

	if (program->program)
	{
		g_memory_free(program->program);
	}
}

// Internal definitions