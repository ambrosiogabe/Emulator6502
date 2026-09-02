#include "Emulator/Assembler.h"
#include "Emulator/VirtualMachine.h"
#include "utils/FileHelper.h"

#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <stb/stb_ds.h>

// Internal structures
typedef enum emu_dot_keyword
{
	emu_dot_keyword_Export,
	emu_dot_keyword_Segment,
	emu_dot_keyword_Proc,
	emu_dot_keyword_EndProc,
	emu_dot_keyword_Length,
	emu_dot_keyword_NULL
} emu_dot_keyword;

typedef enum emu_instruction_keyword
{
	emu_instruction_keyword_ldx,
	emu_instruction_keyword_stx,

	emu_instruction_keyword_ldy,
	emu_instruction_keyword_sty,

	emu_instruction_keyword_lda,
	emu_instruction_keyword_sta,

	emu_instruction_keyword_clc,

	emu_instruction_keyword_rts,
	emu_instruction_keyword_bcc,

	// Logical/Arithmetic commands
	emu_instruction_keyword_ora,
	emu_instruction_keyword_and,
	emu_instruction_keyword_eor,
	emu_instruction_keyword_adc,
	emu_instruction_keyword_sbc,
	emu_instruction_keyword_cmp,
	emu_instruction_keyword_cpx,
	emu_instruction_keyword_cpy,
	emu_instruction_keyword_dec,
	emu_instruction_keyword_dex,
	emu_instruction_keyword_dey,
	emu_instruction_keyword_inc,
	emu_instruction_keyword_inx,
	emu_instruction_keyword_iny,
	emu_instruction_keyword_asl,
	emu_instruction_keyword_rol,
	emu_instruction_keyword_lsr,
	emu_instruction_keyword_ror,

	emu_instruction_keyword_Length,
	emu_instruction_keyword_NULL,
} emu_instruction_keyword;

const char* emu_instruction_keywords[] = {
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

const char* emu_dot_keywords[] = {
	"export",
	"segment",
	"proc",
	"endproc",
	"LENGTH",
	"NULL"
};

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

typedef struct emu_parser
{
	emu_file* file;
	size_t current;
	uint8* program;
	size_t programIndex;
	size_t programSize;

	size_t currentLine;
	size_t currentColumn;
	size_t lastSymbolStart;

	emu_instruction_keyword currentInstruction;
	bool expectingSymbol;
	bool expectingString;
	emu_PatchLocation* patches;
	emu_Label* labels;
} emu_parser;

typedef struct emu_symbol
{
	size_t start;
	size_t length;
} emu_symbol;

typedef struct emu_stringConstant
{
	size_t start;
	size_t length;
} emu_stringConstant;

// Internal functions

/**
* Parses from 'current' to the next whitespace character and stores the result in 'symbolStart' and 'symbolLength'
*/
emu_symbol i_emu_parseSymbol(emu_parser* parser);
emu_dot_keyword i_emu_parseDotKeyword(emu_parser* parser);
emu_stringConstant i_emu_parseStringConstant(emu_parser* parser);
emu_instruction_keyword i_emu_isInstructionKeyword(emu_parser* parser, emu_symbol symbol);
uint8 i_emu_parseNumberConstant(emu_parser* parser);
uint16 i_emu_parseAddressConstant(emu_parser* parser, bool oneByteOnly);
uint8 i_emu_parseBinaryConstant(emu_parser* parser);
void i_emu_skipToEndOfLine(emu_parser* parser);

void i_emu_emitOpcode_zeroPage(emu_parser* parser, emu_instruction_keyword keyword);
void i_emu_emitOpcode_immediate(emu_parser* parser, emu_instruction_keyword keyword);
void i_emu_emitOpcode(emu_parser* parser, emu_vmInstruction opcode);
// You can only load a 1 byte constant into the parser
void i_emu_emitConstant(emu_parser* parser, uint8 constant);

char i_emu_getChar(emu_parser* parser);
void i_emu_expectChar(emu_parser* parser, char expected);
char i_emu_peek(emu_parser* parser);
char i_emu_peekMulti(emu_parser* parser, uint8 offset);
void i_emu_skip(emu_parser* parser, size_t skipAmount);
char i_emu_toUpper(char c);

bool i_emu_isWhitespace(char c);
bool i_emu_isSymbolStart(char c);
bool i_emu_isSymbolChar(char c);

void i_emu_logError(emu_parser* parser, const char* fmtString, ...);
void i_emu_logErrorLineColumn(emu_parser* parser, size_t line, size_t column, const char* fmtString, ...);
void i_emu_logErrorLineColumnWithArgs(emu_parser* parser, size_t line, size_t column, const char* fmtString, va_list args);

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
	emu_parser parser = {
		.current = 0,
		.file = &file,
		.program = program,
		.programSize = programSize,
		.currentColumn = 1,
		.currentLine = 1,
		.currentInstruction = emu_instruction_keyword_NULL,
		.patches = NULL,
		.labels = NULL,
	};

	for (size_t i = 0; i < file.data_size; i++)
	{
		// Skip white space
		if (i_emu_isWhitespace(i_emu_peek(&parser)))
		{
			i_emu_getChar(&parser);
			continue;
		}

		char c = i_emu_peek(&parser);

		// Check expectations
		if (parser.expectingSymbol && !i_emu_isSymbolStart(c))
		{
			i_emu_logError(&parser, "Expected symbol instead got '%c'", c);
			parser.expectingSymbol = false;
		}
		else if (parser.expectingString && c != '"')
		{
			i_emu_logError(&parser, "Expected string instead got '%c'", c);
			parser.expectingString = false;
		}

		switch (c)
		{
		case ';':
			i_emu_skipToEndOfLine(&parser);
			break;
		case '.':
		{
			emu_dot_keyword keyword = i_emu_parseDotKeyword(&parser);
		}
		break;
		case '"':
		{
			emu_stringConstant strConstant = i_emu_parseStringConstant(&parser);
			parser.expectingString = false;
		}
		break;
		case '#':
			// Parse number constant
		{
			uint8 numberConstant = i_emu_parseNumberConstant(&parser);
			i_emu_emitOpcode_immediate(&parser, parser.currentInstruction);
			i_emu_emitConstant(&parser, numberConstant);
		}
		break;
		case '$':
			// Parse address constant
		{
			uint8 numberConstant = (uint8)i_emu_parseAddressConstant(&parser, false);
			i_emu_emitOpcode_zeroPage(&parser, parser.currentInstruction);
			i_emu_emitConstant(&parser, numberConstant);
		}
		break;
		default:
			if (i_emu_isSymbolStart(c))
			{
				emu_symbol symbol = i_emu_parseSymbol(&parser);
				emu_instruction_keyword instruction = i_emu_isInstructionKeyword(&parser, symbol);
				if (instruction != emu_instruction_keyword_NULL)
				{
					parser.currentInstruction = instruction;
				}
				else
				{
					// If we're expecting a symbol, we may need to record the location to patch later
					if (parser.expectingSymbol)
					{
						if (parser.currentInstruction == emu_instruction_keyword_bcc)
						{
							i_emu_emitOpcode(&parser, emu_vmInstruction_BCC_REL);

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
						i_emu_expectChar(&parser, ':');
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
				case emu_instruction_keyword_bcc:
					parser.expectingSymbol = true;
					break;
				default:
					parser.expectingSymbol = false;
					break;
				}
			}
			else
			{
				i_emu_getChar(&parser);
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
			i_emu_logErrorLineColumn(&parser, patch->originalCodeLine, patch->originalCodeColumn, "Label not found '%s'.", patch->label);
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
emu_symbol i_emu_parseSymbol(emu_parser* parser)
{
	emu_symbol symbol = {
		.start = parser->current,
		.length = 0
	};
	parser->lastSymbolStart = parser->current;
	while (i_emu_isSymbolChar(i_emu_peek(parser)))
	{
		i_emu_getChar(parser);
	}

	symbol.length = parser->current - symbol.start;
	return symbol;
}

emu_dot_keyword i_emu_parseDotKeyword(emu_parser* parser)
{
	// Parse the '.'
	i_emu_getChar(parser);

	for (size_t i = 0; i < emu_dot_keyword_Length; i++)
	{
		size_t keywordLength = strlen(emu_dot_keywords[i]);
		bool isKeyword = true;
		for (size_t offset = 0; offset < keywordLength; offset++)
		{
			if (emu_dot_keywords[i][offset] != i_emu_peekMulti(parser, offset))
			{
				isKeyword = false;
				break;
			}
		}

		if (isKeyword)
		{
			i_emu_skip(parser, keywordLength);
			switch (i)
			{
			case emu_dot_keyword_Export:
			case emu_dot_keyword_Proc:
				parser->expectingSymbol = true;
				break;
			case emu_dot_keyword_Segment:
				parser->expectingString = true;
				break;
			}

			return (emu_dot_keyword)i;
		}
	}

	return emu_dot_keyword_NULL;
}

emu_stringConstant i_emu_parseStringConstant(emu_parser* parser)
{
	size_t strColumnStart = parser->currentColumn;

	// Parse beginning '"'
	i_emu_getChar(parser);

	emu_stringConstant strConstant = {
		.start = parser->current,
		.length = 0
	};
	char c = '\0';
	do
	{
		c = i_emu_getChar(parser);

	} while (c != '"' && c != '\n' && c != '\0');

	if (c == '\n' || c == '\0')
	{
		// Do some gross hacks to get right string
		parser->current--;
		i_emu_logErrorLineColumn(parser, parser->currentLine - 1, strColumnStart, "Malformed string. No end at line: %u:%u", parser->currentLine - 1, strColumnStart);
		parser->current++;
		strConstant.length = parser->current - strConstant.start - 2;
		return strConstant;
	}

	strConstant.length = parser->current - strConstant.start - 1;
	return strConstant;
}

uint8 i_emu_parseNumberConstant(emu_parser* parser)
{
	// Skip the '#' character
	i_emu_getChar(parser);

	if (i_emu_peek(parser) != '%')
	{
		return i_emu_parseAddressConstant(parser, true);
	}

	return i_emu_parseBinaryConstant(parser);
}

uint16 i_emu_parseAddressConstant(emu_parser* parser, bool oneByteOnly)
{
	size_t columnStart = parser->currentColumn;

	char start = i_emu_getChar(parser);
	bool isHexadecimal = start == '$';
	size_t digitStart = isHexadecimal ? parser->current : parser->current - 1;
	bool isInvalid = false;
	while (!i_emu_isWhitespace(i_emu_peek(parser)))
	{
		char digit = i_emu_peek(parser);
		if (isHexadecimal)
		{
			if (!((digit >= 'a' && digit <= 'f') || (digit >= 'A' && digit <= 'F') || (digit >= '0' && digit <= '9')))
			{
				i_emu_logError(parser, "Invalid digit encountered '%c'. Hexadecimal constant must contain only 0-9 or A-F.", digit);
				isInvalid = true;
			}
		}
		else
		{
			if (!(digit >= '0' && digit <= '9'))
			{
				i_emu_logError(parser, "Invalid digit encountered '%c'. Decimal constant must contain only 0-9.", digit);
				isInvalid = true;
			}
		}

		i_emu_getChar(parser);
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
		i_emu_logErrorLineColumn(parser, parser->currentLine, columnStart, "Number is larger than one byte. Numeric constants can only be 1 byte, or a value of 255 maximum.");
		parser->current = oldCurrent;
		return 0;
	}

	return result;
}

uint8 i_emu_parseBinaryConstant(emu_parser* parser)
{
	size_t columnStart = parser->currentColumn;

	char start = i_emu_getChar(parser);
	if (start != '%')
	{
		i_emu_logError(parser, "Invalid binary constant. Expected to start with '%' and instead started with '%c'.", start);
		return 0;
	}

	size_t digitStart = parser->current;
	bool isInvalid = false;
	while (!i_emu_isWhitespace(i_emu_peek(parser)))
	{
		char digit = i_emu_peek(parser);
		if (digit != '0' && digit != '1')
		{
			i_emu_logError(parser, "Invalid digit encountered '%c'. Binary constant must contain only 0's and 1's.", digit);
			isInvalid = true;
		}

		i_emu_getChar(parser);
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
		i_emu_logErrorLineColumn(parser, parser->currentLine, columnStart, "Number is larger than one byte. Numeric constants can only be 1 byte, or a value of 255 maximum.");
		parser->current = oldCurrent;
		return 0;
	}

	return result;
}

emu_instruction_keyword i_emu_isInstructionKeyword(emu_parser* parser, emu_symbol symbol)
{
	if (symbol.length != 3)
	{
		return emu_instruction_keyword_NULL;
	}

	for (size_t i = 0; i < emu_instruction_keyword_Length; i++)
	{
		bool isKeyword = true;
		for (size_t offset = 0; offset < 3; offset++)
		{
			char c = parser->file->data[symbol.start + offset];
			if (i_emu_toUpper(emu_instruction_keywords[i][offset]) != i_emu_toUpper(c))
			{
				isKeyword = false;
				break;
			}
		}

		if (isKeyword)
		{
			return (emu_instruction_keyword)i;
		}
	}

	return emu_instruction_keyword_NULL;
}

void i_emu_skipToEndOfLine(emu_parser* parser)
{
	while (i_emu_peek(parser) != '\n')
	{
		i_emu_getChar(parser);
	}

	// Make sure to consume end line character as well
	i_emu_getChar(parser);
}

void i_emu_emitOpcode_zeroPage(emu_parser* parser, emu_instruction_keyword keyword)
{
	switch (keyword)
	{
	case emu_instruction_keyword_ldx:
		i_emu_emitOpcode(parser, emu_vmInstruction_LDX_ZP);
		break;
	case emu_instruction_keyword_stx:
		i_emu_emitOpcode(parser, emu_vmInstruction_STX_ZP);
		break;
	case emu_instruction_keyword_ldy:
		i_emu_emitOpcode(parser, emu_vmInstruction_LDY_ZP);
		break;
	case emu_instruction_keyword_sty:
		i_emu_emitOpcode(parser, emu_vmInstruction_STY_ZP);
		break;
	case emu_instruction_keyword_lda:
		i_emu_emitOpcode(parser, emu_vmInstruction_LDA_ZP);
		break;
	case emu_instruction_keyword_sta:
		i_emu_emitOpcode(parser, emu_vmInstruction_STA_ZP);
		break;
	case emu_instruction_keyword_clc:
		i_emu_emitOpcode(parser, emu_vmInstruction_CLC);
		break;
	case emu_instruction_keyword_adc:
		i_emu_emitOpcode(parser, emu_vmInstruction_ADC_ZP);
		break;
	case emu_instruction_keyword_rts:
		i_emu_emitOpcode(parser, emu_vmInstruction_RTS);
		break;
	case emu_instruction_keyword_cmp:
		i_emu_emitOpcode(parser, emu_vmInstruction_CMP_ZP);
		break;
	default:
		g_logger_warning("Cannot emit invalid instruction.");
	}
}

void i_emu_emitOpcode_immediate(emu_parser* parser, emu_instruction_keyword keyword)
{
	switch (keyword)
	{
	case emu_instruction_keyword_ldx:
		i_emu_emitOpcode(parser, emu_vmInstruction_LDX_IMM);
		break;
	case emu_instruction_keyword_ldy:
		i_emu_emitOpcode(parser, emu_vmInstruction_LDY_IMM);
		break;
	case emu_instruction_keyword_lda:
		i_emu_emitOpcode(parser, emu_vmInstruction_LDA_IMM);
		break;
	case emu_instruction_keyword_clc:
		i_emu_emitOpcode(parser, emu_vmInstruction_CLC);
		break;
	case emu_instruction_keyword_adc:
		i_emu_emitOpcode(parser, emu_vmInstruction_ADC_IMM);
		break;
	case emu_instruction_keyword_rts:
		i_emu_emitOpcode(parser, emu_vmInstruction_RTS);
		break;
	case emu_instruction_keyword_cmp:
		i_emu_emitOpcode(parser, emu_vmInstruction_CMP_IMM);
		break;
	case emu_instruction_keyword_stx:
	case emu_instruction_keyword_sty:
	case emu_instruction_keyword_sta:
	default:
		g_logger_warning("Cannot emit invalid instruction.");
	}
}

void i_emu_emitOpcode(emu_parser* parser, emu_vmInstruction opcode)
{
	if (parser->programIndex >= parser->programSize)
	{
		g_logger_warning("Ran out of program memory. Cannot emit anymore instructions.");
		return;
	}

	parser->program[parser->programIndex] = (uint8)opcode;
	parser->programIndex++;
}

void i_emu_emitConstant(emu_parser* parser, uint8 constant)
{
	if (parser->programIndex >= parser->programSize)
	{
		g_logger_warning("Ran out of program memory. Cannot emit anymore instructions.");
		return;
	}

	parser->program[parser->programIndex] = constant;
	parser->programIndex++;
}

char i_emu_getChar(emu_parser* parser)
{
	char result = i_emu_peek(parser);
	parser->current++;
	parser->currentColumn++;

	if (result == '\n')
	{
		parser->currentLine++;
		parser->currentColumn = 1;
	}

	return result;
}

void i_emu_expectChar(emu_parser* parser, char expected)
{
	char actual = i_emu_peek(parser);
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
		i_emu_logError(parser, "Expected '%s' and got '%s'", safeExpected, safeActual);
	}

	i_emu_getChar(parser);
}

char i_emu_peek(emu_parser* parser)
{
	return i_emu_peekMulti(parser, 0);
}

char i_emu_peekMulti(emu_parser* parser, uint8 offset)
{
	if (parser->current + offset >= parser->file->data_size)
	{
		return '\0';
	}

	return parser->file->data[parser->current + offset];
}

void i_emu_skip(emu_parser* parser, size_t skipAmount)
{
	for (size_t i = 0; i < skipAmount; i++)
	{
		i_emu_getChar(parser);
	}
}

char i_emu_toUpper(char c)
{
	if (c >= 'a' && c <= 'z')
	{
		return (c - 'a') + 'A';
	}

	return c;
}

bool i_emu_isWhitespace(char c)
{
	return c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == '\0';
}

bool i_emu_isSymbolStart(char c)
{
	return c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c == '_' || c == '.';
}

bool i_emu_isSymbolChar(char c)
{
	return c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c >= '0' && c <= '9' || c == '_' || c == '.';
}

void i_emu_logError(emu_parser* parser, const char* fmtString, ...)
{
	va_list args;
	va_start(args, fmtString);
	i_emu_logErrorLineColumnWithArgs(parser, parser->currentLine, parser->currentColumn, fmtString, args);
	va_end(args);
}

void i_emu_logErrorLineColumn(emu_parser* parser, size_t line, size_t column, const char* fmtString, ...)
{
	static char messageBuffer[1'024];
	va_list args;
	va_start(args, fmtString);
	i_emu_logErrorLineColumnWithArgs(parser, line, column, fmtString, args);
	va_end(args);
}

void i_emu_logErrorLineColumnWithArgs(emu_parser* parser, size_t line, size_t column, const char* fmtString, va_list args)
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