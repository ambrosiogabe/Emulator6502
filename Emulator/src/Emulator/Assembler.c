#include "Emulator/Assembler.h"
#include "Emulator/VirtualMachine.h"
#include "utils/FileHelper.h"

#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <stb/stb_ds.h>

// Internal structures
typedef enum emu_DotKeyword
{
	emu_DotKeyword_Export,
	emu_DotKeyword_Segment,
	emu_DotKeyword_Proc,
	emu_DotKeyword_EndProc,
	emu_DotKeyword_Byte,
	emu_DotKeyword_Length,
	emu_DotKeyword_NULL
} emu_DotKeyword;

typedef enum emu_InstructionKeyword
{
	emu_InstructionKeyword_ldx,
	emu_InstructionKeyword_stx,

	emu_InstructionKeyword_ldy,
	emu_InstructionKeyword_sty,

	emu_InstructionKeyword_lda,
	emu_InstructionKeyword_sta,

	emu_InstructionKeyword_clc,

	emu_InstructionKeyword_rts,
	emu_InstructionKeyword_bcc,

	// Logical/Arithmetic commands
	emu_InstructionKeyword_ora,
	emu_InstructionKeyword_and,
	emu_InstructionKeyword_eor,
	emu_InstructionKeyword_adc,
	emu_InstructionKeyword_sbc,
	emu_InstructionKeyword_cmp,
	emu_InstructionKeyword_cpx,
	emu_InstructionKeyword_cpy,
	emu_InstructionKeyword_dec,
	emu_InstructionKeyword_dex,
	emu_InstructionKeyword_dey,
	emu_InstructionKeyword_inc,
	emu_InstructionKeyword_inx,
	emu_InstructionKeyword_iny,
	emu_InstructionKeyword_asl,
	emu_InstructionKeyword_rol,
	emu_InstructionKeyword_lsr,
	emu_InstructionKeyword_ror,

	emu_InstructionKeyword_Length,
	emu_InstructionKeyword_NULL,
} emu_InstructionKeyword;

const char* emu_InstructionKeywords[] = {
	"ldx",
	"stx",

	"ldy",
	"sty",

	"lda",
	"sta",

	"clc",

	"rts",
	"bcc",

	// Logical/Arithmetic commands
	"ora",
	"and",
	"eor",
	"adc",
	"sbc",
	"cmp",
	"cpx",
	"cpy",
	"dec",
	"dex",
	"dey",
	"inc",
	"inx",
	"iny",
	"asl",
	"rol",
	"lsr",
	"ror",

	"LENGTH",
	"NULL"
};

const char* emu_DotKeywords[] = {
	"export",
	"segment",
	"proc",
	"endproc",
	"byte",
	"LENGTH",
	"NULL"
};

typedef enum emu_TokenType
{
	emu_DotKeyword_Export,
	emu_DotKeyword_Segment,
	emu_DotKeyword_Proc,
	emu_DotKeyword_EndProc,
	emu_DotKeyword_Byte,
	emu_DotKeyword_Length,
	emu_DotKeyword_NULL
} emu_TokenType;

typedef struct emu_Token
{
	uint16 foo;
} emu_Token;

typedef struct emu_PatchLocation
{
	// The index in the program that needs to be patched
	size_t programIndex;
	// The label we need to jump to
	char* label;
	// Debug info
	size_t originalCodeIndex;
	size_t originalCodeLine;
	size_t originalCodeColumn;
} emu_PatchLocation;

typedef struct emu_Label
{
	char* key;
	// The program index where this label is located
	size_t value;
} emu_Label;

typedef struct emu_Parser
{
	emu_file* file;
	size_t current;
	uint8* program;
	size_t programIndex;
	size_t programSize;

	size_t currentLine;
	size_t currentColumn;
	size_t lastSymbolStart;

	emu_InstructionKeyword currentInstruction;
	bool expectingSymbol;
	bool expectingString;
	emu_PatchLocation* patches;
	emu_Label* labels;
} emu_Parser;

typedef struct emu_Symbol
{
	size_t start;
	size_t length;
} emu_Symbol;

typedef struct emu_StringConstant
{
	size_t start;
	size_t length;
} emu_StringConstant;

// Internal functions

/**
* Parses from 'current' to the next whitespace character and stores the result in 'symbolStart' and 'symbolLength'
*/
emu_Symbol emu_parseSymbol(emu_Parser* parser);
emu_DotKeyword emu_parseDotKeyword(emu_Parser* parser);
emu_StringConstant emu_parseStringConstant(emu_Parser* parser);
emu_InstructionKeyword emu_isInstructionKeyword(emu_Parser* parser, emu_Symbol symbol);
uint8 emu_parseNumberConstant(emu_Parser* parser);
uint16 emu_parseAddressConstant(emu_Parser* parser, bool oneByteOnly);
uint8 emu_parseBinaryConstant(emu_Parser* parser);
void emu_skipToEndOfLine(emu_Parser* parser);

void emu_emitOpcode_zeroPage(emu_Parser* parser, emu_InstructionKeyword keyword);
void emu_emitOpcode_immediate(emu_Parser* parser, emu_InstructionKeyword keyword);
void emu_emitOpcode(emu_Parser* parser, emu_vmInstruction opcode);
// You can only load a 1 byte constant into the parser
void emu_emitConstant(emu_Parser* parser, uint8 constant);

char emu_getChar(emu_Parser* parser);
void emu_expectChar(emu_Parser* parser, char expected);
char emu_peek(emu_Parser* parser);
char emu_peekMulti(emu_Parser* parser, uint8 offset);
void emu_skip(emu_Parser* parser, size_t skipAmount);
char emu_toUpper(char c);

bool emu_isWhitespace(char c);
bool emu_isSymbolStart(char c);
bool emu_isSymbolChar(char c);

void emu_logError(emu_Parser* parser, const char* fmtString, ...);
void emu_logErrorLineColumn(emu_Parser* parser, size_t line, size_t column, const char* fmtString, ...);
void emu_logErrorLineColumnWithArgs(emu_Parser* parser, size_t line, size_t column, const char* fmtString, va_list args);

// Public functions
emu_assembler_program emu_assembler_assembleProgram(const char* filename, size_t programSize)
{
	emu_file file = { 0 };
	if (emu_file_read(filename, &file) != emu_fileResult_Success)
	{
		g_logger_error("Failed to read file '%s'. Cannot assemble program.", filename);
		return;
	}

	uint8* program = g_memory_allocate(programSize);
	emu_Parser parser = {
		.current = 0,
		.file = &file,
		.program = program,
		.programSize = programSize,
		.currentColumn = 1,
		.currentLine = 1,
		.currentInstruction = emu_InstructionKeyword_NULL,
		.patches = NULL,
		.labels = NULL,
	};

	for (size_t i = 0; i < file.data_size; i++)
	{
		// Skip white space
		if (emu_isWhitespace(emu_peek(&parser)))
		{
			emu_getChar(&parser);
			continue;
		}

		char c = emu_peek(&parser);

		// Check expectations
		if (parser.expectingSymbol && !emu_isSymbolStart(c))
		{
			emu_logError(&parser, "Expected symbol instead got '%c'", c);
			parser.expectingSymbol = false;
		}
		else if (parser.expectingString && c != '"')
		{
			emu_logError(&parser, "Expected string instead got '%c'", c);
			parser.expectingString = false;
		}

		switch (c)
		{
		case ';':
			emu_skipToEndOfLine(&parser);
			break;
		case '.':
		{
			emu_DotKeyword keyword = emu_parseDotKeyword(&parser);
		}
		break;
		case '"':
		{
			emu_StringConstant strConstant = emu_parseStringConstant(&parser);
			parser.expectingString = false;
		}
		break;
		case '#':
			// Parse number constant
		{
			uint8 numberConstant = emu_parseNumberConstant(&parser);
			emu_emitOpcode_immediate(&parser, parser.currentInstruction);
			emu_emitConstant(&parser, numberConstant);
		}
		break;
		case '$':
			// Parse address constant
		{
			uint8 numberConstant = (uint8)emu_parseAddressConstant(&parser, false);
			emu_emitOpcode_zeroPage(&parser, parser.currentInstruction);
			emu_emitConstant(&parser, numberConstant);
		}
		break;
		default:
			if (emu_isSymbolStart(c))
			{
				emu_Symbol symbol = emu_parseSymbol(&parser);
				emu_InstructionKeyword instruction = emu_isInstructionKeyword(&parser, symbol);
				if (instruction != emu_InstructionKeyword_NULL)
				{
					parser.currentInstruction = instruction;
				}
				else
				{
					// If we're expecting a symbol, we may need to record the location to patch later
					if (parser.expectingSymbol)
					{
						if (parser.currentInstruction == emu_InstructionKeyword_bcc)
						{
							emu_emitOpcode(&parser, emu_vmInstruction_BCC_REL);

							// Record the location of this patch
							char* symbolString = g_memory_allocate(symbol.length + 1);
							g_memory_copyMem(symbolString, parser.file->data + symbol.start, symbol.length);
							symbolString[symbol.length] = '\0';
							emu_PatchLocation patch = {
								.label = symbolString,
								.programIndex = parser.programIndex,
								.originalCodeIndex = symbol.start,
								.originalCodeColumn = parser.currentColumn,
								.originalCodeLine = parser.currentLine,
							};
							// Increment 2 bytes to save room for the patched location
							parser.programIndex += 2;
							stbds_arrput(parser.patches, patch);
						}
					}
					// Otherwise we're declaring a new symbol and need to follow it with a ':'
					else
					{
						emu_expectChar(&parser, ':');
						// Record the location of this label
						char* symbolString = g_memory_allocate(symbol.length + 1);
						g_memory_copyMem(symbolString, parser.file->data + symbol.start, symbol.length);
						symbolString[symbol.length] = '\0';
						stbds_shput(parser.labels, symbolString, parser.programIndex);
					}
				}

				// Make sure to reset expectations if needed
				switch (instruction)
				{
				case emu_InstructionKeyword_bcc:
					parser.expectingSymbol = true;
					break;
				default:
					parser.expectingSymbol = false;
					break;
				}
			}
			else
			{
				emu_getChar(&parser);
				g_logger_warning("Parser does not know what to do with symbol: '%c'", c);
			}
			break;
		}
	}

	// Patch all the needed patches
	for (size_t i = 0; i < stbds_arrlen(parser.patches); i++)
	{
		emu_PatchLocation* patch = parser.patches + i;

		if (stbds_shgeti(parser.labels, patch->label) >= 0)
		{
			size_t value = stbds_shget(parser.labels, patch->label);
			int16 relativeOffset = (int16)((int64)value - (int64)patch->programIndex);
			parser.program[patch->programIndex] = relativeOffset >> 8;
			parser.program[patch->programIndex + 1] = (uint8)(relativeOffset & 0xFF);
		}
		else
		{
			parser.current = patch->originalCodeIndex;
			emu_logErrorLineColumn(&parser, patch->originalCodeLine, patch->originalCodeColumn, "Label not found '%s'.", patch->label);
		}

		// And free memory after we're done with it
		g_memory_free(patch->label);
	}

	for (size_t i = 0; i < stbds_shlen(parser.labels); i++)
	{
		g_memory_free(parser.labels[i].key);
	}

	stbds_arrfree(parser.patches);
	stbds_shfree(parser.labels);

	emu_file_free(&file);

	return (emu_assembler_program)
	{
		.program = program,
			.size = parser.programIndex
	};
}

// Internal definitions
emu_Symbol emu_parseSymbol(emu_Parser* parser)
{
	emu_Symbol symbol = {
		.start = parser->current,
		.length = 0
	};
	parser->lastSymbolStart = parser->current;
	while (emu_isSymbolChar(emu_peek(parser)))
	{
		emu_getChar(parser);
	}

	symbol.length = parser->current - symbol.start;
	return symbol;
}

emu_DotKeyword emu_parseDotKeyword(emu_Parser* parser)
{
	// Parse the '.'
	emu_getChar(parser);

	for (size_t i = 0; i < emu_DotKeyword_Length; i++)
	{
		size_t keywordLength = strlen(emu_DotKeywords[i]);
		bool isKeyword = true;
		for (size_t offset = 0; offset < keywordLength; offset++)
		{
			if (emu_DotKeywords[i][offset] != emu_peekMulti(parser, offset))
			{
				isKeyword = false;
				break;
			}
		}

		if (isKeyword)
		{
			emu_skip(parser, keywordLength);
			switch (i)
			{
			case emu_DotKeyword_Export:
			case emu_DotKeyword_Proc:
				parser->expectingSymbol = true;
				break;
			case emu_DotKeyword_Segment:
				parser->expectingString = true;
				break;
			}

			return (emu_DotKeyword)i;
		}
	}

	return emu_DotKeyword_NULL;
}

emu_StringConstant emu_parseStringConstant(emu_Parser* parser)
{
	size_t strColumnStart = parser->currentColumn;

	// Parse beginning '"'
	emu_getChar(parser);

	emu_StringConstant strConstant = {
		.start = parser->current,
		.length = 0
	};
	char c = '\0';
	do
	{
		c = emu_getChar(parser);

	} while (c != '"' && c != '\n' && c != '\0');

	if (c == '\n' || c == '\0')
	{
		// Do some gross hacks to get right string
		parser->current--;
		emu_logErrorLineColumn(parser, parser->currentLine - 1, strColumnStart, "Malformed string. No end at line: %u:%u", parser->currentLine - 1, strColumnStart);
		parser->current++;
		strConstant.length = parser->current - strConstant.start - 2;
		return strConstant;
	}

	strConstant.length = parser->current - strConstant.start - 1;
	return strConstant;
}

uint8 emu_parseNumberConstant(emu_Parser* parser)
{
	// Skip the '#' character
	emu_getChar(parser);

	if (emu_peek(parser) != '%')
	{
		return emu_parseAddressConstant(parser, true);
	}

	return emu_parseBinaryConstant(parser);
}

uint16 emu_parseAddressConstant(emu_Parser* parser, bool oneByteOnly)
{
	size_t columnStart = parser->currentColumn;

	char start = emu_getChar(parser);
	bool isHexadecimal = start == '$';
	size_t digitStart = isHexadecimal ? parser->current : parser->current - 1;
	bool isInvalid = false;
	while (!emu_isWhitespace(emu_peek(parser)))
	{
		char digit = emu_peek(parser);
		if (isHexadecimal)
		{
			if (!((digit >= 'a' && digit <= 'f') || (digit >= 'A' && digit <= 'F') || (digit >= '0' && digit <= '9')))
			{
				emu_logError(parser, "Invalid digit encountered '%c'. Hexadecimal constant must contain only 0-9 or A-F.", digit);
				isInvalid = true;
			}
		}
		else
		{
			if (!(digit >= '0' && digit <= '9'))
			{
				emu_logError(parser, "Invalid digit encountered '%c'. Decimal constant must contain only 0-9.", digit);
				isInvalid = true;
			}
		}

		emu_getChar(parser);
	}

	if (isInvalid)
	{
		return 0;
	}

	size_t digitCharLength = parser->current - digitStart;
	uint16 result = 0;
	for (size_t i = 0; i < digitCharLength; i++)
	{
		char digitChar = parser->file->data[parser->current - i - 1];
		if (isHexadecimal)
		{
			uint16 base = (uint16)pow(16, i);
			uint16 digit =
				digitChar >= 'a' && digitChar <= 'f'
				? digitChar - 'a' + 10
				: digitChar >= 'A' && digitChar <= 'F'
				? digitChar - 'A' + 10
				: digitChar - '0';
			result += base * digit;
		}
		else
		{
			uint16 base = (uint16)pow(10, i);
			uint16 digit = digitChar - '0';
			result += base * digit;
		}
	}

	if (result > UINT8_MAX && oneByteOnly)
	{
		// Do some gross hacks to display error correctly
		size_t oldCurrent = parser->current;
		parser->current = digitStart;
		emu_logErrorLineColumn(parser, parser->currentLine, columnStart, "Number is larger than one byte. Numeric constants can only be 1 byte, or a value of 255 maximum.");
		parser->current = oldCurrent;
		return 0;
	}

	return result;
}

uint8 emu_parseBinaryConstant(emu_Parser* parser)
{
	size_t columnStart = parser->currentColumn;

	char start = emu_getChar(parser);
	if (start != '%')
	{
		emu_logError(parser, "Invalid binary constant. Expected to start with '%' and instead started with '%c'.", start);
		return 0;
	}

	size_t digitStart = parser->current;
	bool isInvalid = false;
	while (!emu_isWhitespace(emu_peek(parser)))
	{
		char digit = emu_peek(parser);
		if (digit != '0' && digit != '1')
		{
			emu_logError(parser, "Invalid digit encountered '%c'. Binary constant must contain only 0's and 1's.", digit);
			isInvalid = true;
		}

		emu_getChar(parser);
	}

	if (isInvalid)
	{
		return 0;
	}

	size_t digitCharLength = parser->current - digitStart;
	uint16 result = 0;
	for (size_t i = 0; i < digitCharLength; i++)
	{
		char digitChar = parser->file->data[parser->current - i - 1];

		uint16 base = (uint16)pow(2, i);
		uint16 digit = digitChar == '1' ? 1 : 0;
		result += base * digit;
	}

	if (result > UINT8_MAX)
	{
		// Do some gross hacks to display error correctly
		size_t oldCurrent = parser->current;
		parser->current = digitStart;
		emu_logErrorLineColumn(parser, parser->currentLine, columnStart, "Number is larger than one byte. Numeric constants can only be 1 byte, or a value of 255 maximum.");
		parser->current = oldCurrent;
		return 0;
	}

	return result;
}

emu_InstructionKeyword emu_isInstructionKeyword(emu_Parser* parser, emu_Symbol symbol)
{
	if (symbol.length != 3)
	{
		return emu_InstructionKeyword_NULL;
	}

	for (size_t i = 0; i < emu_InstructionKeyword_Length; i++)
	{
		bool isKeyword = true;
		for (size_t offset = 0; offset < 3; offset++)
		{
			char c = parser->file->data[symbol.start + offset];
			if (emu_toUpper(emu_InstructionKeywords[i][offset]) != emu_toUpper(c))
			{
				isKeyword = false;
				break;
			}
		}

		if (isKeyword)
		{
			return (emu_InstructionKeyword)i;
		}
	}

	return emu_InstructionKeyword_NULL;
}

void emu_skipToEndOfLine(emu_Parser* parser)
{
	while (emu_peek(parser) != '\n')
	{
		emu_getChar(parser);
	}

	// Make sure to consume end line character as well
	emu_getChar(parser);
}

void emu_emitOpcode_zeroPage(emu_Parser* parser, emu_InstructionKeyword keyword)
{
	switch (keyword)
	{
	case emu_InstructionKeyword_ldx:
		emu_emitOpcode(parser, emu_vmInstruction_LDX_ZP);
		break;
	case emu_InstructionKeyword_stx:
		emu_emitOpcode(parser, emu_vmInstruction_STX_ZP);
		break;
	case emu_InstructionKeyword_ldy:
		emu_emitOpcode(parser, emu_vmInstruction_LDY_ZP);
		break;
	case emu_InstructionKeyword_sty:
		emu_emitOpcode(parser, emu_vmInstruction_STY_ZP);
		break;
	case emu_InstructionKeyword_lda:
		emu_emitOpcode(parser, emu_vmInstruction_LDA_ZP);
		break;
	case emu_InstructionKeyword_sta:
		emu_emitOpcode(parser, emu_vmInstruction_STA_ZP);
		break;
	case emu_InstructionKeyword_clc:
		emu_emitOpcode(parser, emu_vmInstruction_CLC);
		break;
	case emu_InstructionKeyword_adc:
		emu_emitOpcode(parser, emu_vmInstruction_ADC_ZP);
		break;
	case emu_InstructionKeyword_rts:
		emu_emitOpcode(parser, emu_vmInstruction_RTS);
		break;
	case emu_InstructionKeyword_cmp:
		emu_emitOpcode(parser, emu_vmInstruction_CMP_ZP);
		break;
	default:
		g_logger_warning("Cannot emit invalid instruction.");
	}
}

void emu_emitOpcode_immediate(emu_Parser* parser, emu_InstructionKeyword keyword)
{
	switch (keyword)
	{
	case emu_InstructionKeyword_ldx:
		emu_emitOpcode(parser, emu_vmInstruction_LDX_IMM);
		break;
	case emu_InstructionKeyword_ldy:
		emu_emitOpcode(parser, emu_vmInstruction_LDY_IMM);
		break;
	case emu_InstructionKeyword_lda:
		emu_emitOpcode(parser, emu_vmInstruction_LDA_IMM);
		break;
	case emu_InstructionKeyword_clc:
		emu_emitOpcode(parser, emu_vmInstruction_CLC);
		break;
	case emu_InstructionKeyword_adc:
		emu_emitOpcode(parser, emu_vmInstruction_ADC_IMM);
		break;
	case emu_InstructionKeyword_rts:
		emu_emitOpcode(parser, emu_vmInstruction_RTS);
		break;
	case emu_InstructionKeyword_cmp:
		emu_emitOpcode(parser, emu_vmInstruction_CMP_IMM);
		break;
	case emu_InstructionKeyword_stx:
	case emu_InstructionKeyword_sty:
	case emu_InstructionKeyword_sta:
	default:
		g_logger_warning("Cannot emit invalid instruction.");
	}
}

void emu_emitOpcode(emu_Parser* parser, emu_vmInstruction opcode)
{
	if (parser->programIndex >= parser->programSize)
	{
		g_logger_warning("Ran out of program memory. Cannot emit anymore instructions.");
		return;
	}

	parser->program[parser->programIndex] = (uint8)opcode;
	parser->programIndex++;
}

void emu_emitConstant(emu_Parser* parser, uint8 constant)
{
	if (parser->programIndex >= parser->programSize)
	{
		g_logger_warning("Ran out of program memory. Cannot emit anymore instructions.");
		return;
	}

	parser->program[parser->programIndex] = constant;
	parser->programIndex++;
}

char emu_getChar(emu_Parser* parser)
{
	char result = emu_peek(parser);
	parser->current++;
	parser->currentColumn++;

	if (result == '\n')
	{
		parser->currentLine++;
		parser->currentColumn = 1;
	}

	return result;
}

void emu_expectChar(emu_Parser* parser, char expected)
{
	char actual = emu_peek(parser);
	if (actual != expected)
	{
		char safeActual[3] = { 0 };
		char safeExpected[3] = { 0 };
		safeActual[0] = actual;
		safeExpected[0] = expected;
		if (expected == '\n' || expected == '\r' || expected == '\0' || expected == '\t')
		{
			safeExpected[0] = '\\';
			safeExpected[1] = expected == '\n' ? 'n' : expected == '\r' ? 'r' : expected == '\0' ? '0' : 't';
		}

		if (actual == '\n' || actual == '\r' || actual == '\0' || actual == '\t')
		{
			safeActual[0] = '\\';
			safeActual[1] = actual == '\n' ? 'n' : actual == '\r' ? 'r' : actual == '\0' ? '0' : 't';
		}
		emu_logError(parser, "Expected '%s' and got '%s'", safeExpected, safeActual);
	}

	emu_getChar(parser);
}

char emu_peek(emu_Parser* parser)
{
	return emu_peekMulti(parser, 0);
}

char emu_peekMulti(emu_Parser* parser, uint8 offset)
{
	if (parser->current + offset >= parser->file->data_size)
	{
		return '\0';
	}

	return parser->file->data[parser->current + offset];
}

void emu_skip(emu_Parser* parser, size_t skipAmount)
{
	for (size_t i = 0; i < skipAmount; i++)
	{
		emu_getChar(parser);
	}
}

char emu_toUpper(char c)
{
	if (c >= 'a' && c <= 'z')
	{
		return (c - 'a') + 'A';
	}

	return c;
}

bool emu_isWhitespace(char c)
{
	return c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == '\0';
}

bool emu_isSymbolStart(char c)
{
	return c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c == '_' || c == '.';
}

bool emu_isSymbolChar(char c)
{
	return c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c >= '0' && c <= '9' || c == '_' || c == '.';
}

void emu_logError(emu_Parser* parser, const char* fmtString, ...)
{
	va_list args;
	va_start(args, fmtString);
	emu_logErrorLineColumnWithArgs(parser, parser->currentLine, parser->currentColumn, fmtString, args);
	va_end(args);
}

void emu_logErrorLineColumn(emu_Parser* parser, size_t line, size_t column, const char* fmtString, ...)
{
	static char messageBuffer[1'024];
	va_list args;
	va_start(args, fmtString);
	emu_logErrorLineColumnWithArgs(parser, line, column, fmtString, args);
	va_end(args);
}

void emu_logErrorLineColumnWithArgs(emu_Parser* parser, size_t line, size_t column, const char* fmtString, va_list args)
{
	static char messageBuffer[1'024];
	int numChars = vsnprintf(messageBuffer, sizeof(messageBuffer), fmtString, args);

	size_t lineStart = 0;
	size_t lineEnd = parser->file->data_size;
	for (size_t i = parser->current; i > 0; i--)
	{
		if (parser->file->data[i] == '\n' && i != parser->current)
		{
			lineStart = i + 1;
			break;
		}
	}

	for (size_t i = parser->current; i < parser->file->data_size; i++)
	{
		if (parser->file->data[i] == '\n')
		{
			lineEnd = i;
			break;
		}
	}

	static char fullErrorBuffer[2'048];
	snprintf(
		fullErrorBuffer,
		sizeof(fullErrorBuffer),
		"Error line %d:%d: %s\n\t%.*s\n\t%*s|-- here",
		(int)line,
		(int)column,
		messageBuffer,
		(int)(lineEnd - lineStart),
		parser->file->data + lineStart,
		(int)column - 1,
		""
	);

	printf("%s\n", fullErrorBuffer);
}