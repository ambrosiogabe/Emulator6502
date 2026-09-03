#include "Emulator/Assembler.h"
#include "Emulator/VirtualMachine.h"
#include "utils/FileHelper.h"

#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <stb/stb_ds.h>

// Internal structures
typedef enum emu_ControlCommand
{
	emu_ControlCommand_Export,
	emu_ControlCommand_Segment,
	emu_ControlCommand_Proc,
	emu_ControlCommand_EndProc,
	emu_ControlCommand_Byte,
	emu_ControlCommand_Length,
	emu_ControlCommand_NULL
} emu_ControlCommand;

const char* emu_ControlCommands[] = {
	"export",
	"segment",
	"proc",
	"endproc",
	"byte",
	"LENGTH",
	"NULL"
};

typedef enum emu_Keyword
{
	emu_Keyword_ldx,
	emu_Keyword_stx,

	emu_Keyword_ldy,
	emu_Keyword_sty,

	emu_Keyword_lda,
	emu_Keyword_sta,

	emu_Keyword_clc,

	emu_Keyword_rts,
	emu_Keyword_bcc,

	// Logical/Arithmetic commands
	emu_Keyword_ora,
	emu_Keyword_and,
	emu_Keyword_eor,
	emu_Keyword_adc,
	emu_Keyword_sbc,
	emu_Keyword_cmp,
	emu_Keyword_cpx,
	emu_Keyword_cpy,
	emu_Keyword_dec,
	emu_Keyword_dex,
	emu_Keyword_dey,
	emu_Keyword_inc,
	emu_Keyword_inx,
	emu_Keyword_iny,
	emu_Keyword_asl,
	emu_Keyword_rol,
	emu_Keyword_lsr,
	emu_Keyword_ror,

	emu_Keyword_Length,
	emu_Keyword_NULL,
} emu_Keyword;

const char* emu_Keywords[] = {
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

typedef enum emu_TokenType
{
	emu_TokenType_NULL = 0,
	emu_TokenType_ControlCommand,
	emu_TokenType_Keyword,
	emu_TokenType_Comment,
	emu_TokenType_Symbol,
	emu_TokenType_String,
	emu_TokenType_Label,
	emu_TokenType_Comma,
	emu_TokenType_ByteConstant,
	emu_TokenType_TwoByteConstant,
	emu_TokenType_Length
} emu_TokenType;

const char* emu_TokenTypes[] = {
	"NULL",
	"ControlCommand",
	"Keyword",
	"Comment",
	"Symbol",
	"String",
	"Label",
	"Comma",
	"ByteConstant",
	"TwoByteConstant",
	"Length",
};

typedef union emu_TokenData
{
	uint8 byteConstant;
	uint16 twoByteConstant;
	emu_ControlCommand controlCommand;
	emu_Keyword keyword;
} emu_TokenData;

typedef struct emu_Token
{
	emu_TokenType type;
	size_t start;
	size_t length;
	size_t line;
	size_t column;
	emu_TokenData data;
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

	emu_Keyword currentInstruction;
	bool expectingSymbol;
	bool expectingString;
	emu_PatchLocation* patches;
	emu_Label* labels;
	emu_Token* tokens;
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
static emu_Token emu_parseToken(emu_Parser* parser);
static emu_Token emu_makeToken(emu_TokenType tokenType, size_t start, size_t end, size_t line, size_t column, emu_TokenData data);

/**
* Parses from 'current' to the next whitespace character and stores the result in 'symbolStart' and 'symbolLength'
*/
static emu_Symbol emu_parseSymbol(emu_Parser* parser);
static emu_ControlCommand emu_parseControlCommand(emu_Parser* parser);
static emu_StringConstant emu_parseStringConstant(emu_Parser* parser);
static emu_Keyword emu_isKeyword(emu_Parser* parser, emu_Symbol symbol);
static uint8 emu_parseNumberConstant(emu_Parser* parser);
static uint16 emu_parseAddressConstant(emu_Parser* parser, bool oneByteOnly);
static uint8 emu_parseBinaryConstant(emu_Parser* parser);
static void emu_skipToEndOfLine(emu_Parser* parser);

static void emu_emitOpcode_zeroPage(emu_Parser* parser, emu_Keyword keyword);
static void emu_emitOpcode_immediate(emu_Parser* parser, emu_Keyword keyword);
static void emu_emitOpcode(emu_Parser* parser, emu_vmInstruction opcode);
// You can only load a 1 byte constant into the parser
static void emu_emitConstant(emu_Parser* parser, uint8 constant);

static char emu_getChar(emu_Parser* parser);
static void emu_expectChar(emu_Parser* parser, char expected);
static char emu_peek(emu_Parser* parser);
static char emu_peekMulti(emu_Parser* parser, uint8 offset);
static void emu_skip(emu_Parser* parser, size_t skipAmount);
static char emu_toUpper(char c);

static bool emu_isWhitespace(char c);
static bool emu_isSymbolStart(char c);
static bool emu_isSymbolChar(char c);

static void emu_logError(emu_Parser* parser, const char* fmtString, ...);
static void emu_logErrorLineColumn(emu_Parser* parser, size_t line, size_t column, const char* fmtString, ...);
static void emu_logErrorLineColumnWithArgs(emu_Parser* parser, size_t line, size_t column, const char* fmtString, va_list args);

static void emu_debugPrintToken(emu_Parser* parser, emu_Token* token);

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
		.currentInstruction = emu_Keyword_NULL,
		.patches = NULL,
		.labels = NULL,
		.tokens = NULL,
	};

	for (size_t i = 0; i < file.data_size; i++)
	{
		// Skip white space
		if (emu_isWhitespace(emu_peek(&parser)))
		{
			emu_getChar(&parser);
			continue;
		}

		emu_Token token = emu_parseToken(&parser);
		stbds_arrput(parser.tokens, token);
	}

	// Print all tokens
	for (size_t i = 0; i < stbds_arrlen(parser.tokens); i++)
	{
		emu_debugPrintToken(&parser, parser.tokens + i);
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
	stbds_arrfree(parser.tokens);

	emu_file_free(&file);

	return (emu_assembler_program)
	{
		.program = program,
			.size = parser.programIndex
	};
}

// Internal definitions
static emu_Token emu_parseToken(emu_Parser* parser)
{
	size_t start = parser->current;
	size_t line = parser->currentLine;
	size_t column = parser->currentColumn;
	char c = emu_peek(parser);

	// Check expectations
	if (parser->expectingSymbol && !emu_isSymbolStart(c))
	{
		emu_logError(parser, "Expected symbol instead got '%c'", c);
		parser->expectingSymbol = false;
	}
	else if (parser->expectingString && c != '"')
	{
		emu_logError(parser, "Expected string instead got '%c'", c);
		parser->expectingString = false;
	}

	switch (c)
	{
	case ';':
	{
		emu_skipToEndOfLine(parser);
		return emu_makeToken(emu_TokenType_Comment, start, parser->current - 1, line, column, (emu_TokenData) { 0 });
	}
	case '.':
	{
		emu_ControlCommand keyword = emu_parseControlCommand(parser);
		return emu_makeToken(emu_TokenType_ControlCommand, start, parser->current, line, column, (emu_TokenData) { .controlCommand = keyword });
	}
	case '"':
	{
		emu_StringConstant strConstant = emu_parseStringConstant(parser);
		parser->expectingString = false;
		return emu_makeToken(emu_TokenType_String, start, parser->current, line, column, (emu_TokenData) { 0 });
	}
	case '#':
	{
		uint8 numberConstant = emu_parseNumberConstant(parser);
		emu_emitOpcode_immediate(parser, parser->currentInstruction);
		emu_emitConstant(parser, numberConstant);
		return emu_makeToken(emu_TokenType_ByteConstant, start, parser->current, line, column, (emu_TokenData) { .byteConstant = numberConstant });
	}
	case '$':
	{
		uint8 numberConstant = (uint8)emu_parseAddressConstant(parser, false);
		emu_emitOpcode_zeroPage(parser, parser->currentInstruction);
		emu_emitConstant(parser, numberConstant);
		return emu_makeToken(emu_TokenType_ByteConstant, start, parser->current, line, column, (emu_TokenData) { .byteConstant = numberConstant });
	}
	default:
		if (emu_isSymbolStart(c))
		{
			emu_Symbol symbol = emu_parseSymbol(parser);
			emu_Keyword instruction = emu_isKeyword(parser, symbol);
			if (instruction != emu_Keyword_NULL)
			{
				parser->currentInstruction = instruction;
				if (instruction == emu_Keyword_bcc)
				{
					parser->expectingSymbol = true;
				}
				return emu_makeToken(emu_TokenType_Keyword, start, parser->current, line, column, (emu_TokenData) { .keyword = instruction });
			}
			else
			{
				// If we're expecting a symbol, we may need to record the location to patch later
				if (parser->expectingSymbol)
				{
					if (parser->currentInstruction == emu_Keyword_bcc)
					{
						emu_emitOpcode(parser, emu_vmInstruction_BCC_REL);

						// Record the location of this patch
						char* symbolString = g_memory_allocate(symbol.length + 1);
						g_memory_copyMem(symbolString, parser->file->data + symbol.start, symbol.length);
						symbolString[symbol.length] = '\0';
						emu_PatchLocation patch = {
							.label = symbolString,
							.programIndex = parser->programIndex,
							.originalCodeIndex = symbol.start,
							.originalCodeColumn = parser->currentColumn,
							.originalCodeLine = parser->currentLine,
						};
						// Increment 2 bytes to save room for the patched location
						parser->programIndex += 2;
						stbds_arrput(parser->patches, patch);
					}
				}
				// Otherwise we're declaring a new symbol and need to follow it with a ':'
				else
				{
					emu_expectChar(parser, ':');
					// Record the location of this label
					char* symbolString = g_memory_allocate(symbol.length + 1);
					g_memory_copyMem(symbolString, parser->file->data + symbol.start, symbol.length);
					symbolString[symbol.length] = '\0';
					stbds_shput(parser->labels, symbolString, parser->programIndex);
				}
			}

			// Make sure to reset expectations if needed
			parser->expectingSymbol = false;
			return emu_makeToken(emu_TokenType_Symbol, start, parser->current, line, column, (emu_TokenData) { 0 });
		}
		else
		{
			emu_getChar(parser);
			g_logger_warning("Parser does not know what to do with symbol: '%c'", c);
			return emu_makeToken(emu_TokenType_NULL, start, parser->current, line, column, (emu_TokenData) { 0 });
		}
	}
}

static emu_Token emu_makeToken(emu_TokenType tokenType, size_t start, size_t end, size_t line, size_t column, emu_TokenData data)
{
	return (emu_Token)
	{
		.type = tokenType,
			.start = start,
			.length = end - start,
			.line = line,
			.column = column,
			.data = data,
	};
}

static emu_Symbol emu_parseSymbol(emu_Parser* parser)
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

static emu_ControlCommand emu_parseControlCommand(emu_Parser* parser)
{
	// Parse the '.'
	emu_getChar(parser);

	for (size_t i = 0; i < emu_ControlCommand_Length; i++)
	{
		size_t keywordLength = strlen(emu_ControlCommands[i]);
		bool isKeyword = true;
		for (size_t offset = 0; offset < keywordLength; offset++)
		{
			if (emu_ControlCommands[i][offset] != emu_peekMulti(parser, offset))
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
			case emu_ControlCommand_Export:
			case emu_ControlCommand_Proc:
				parser->expectingSymbol = true;
				break;
			case emu_ControlCommand_Segment:
				parser->expectingString = true;
				break;
			}

			return (emu_ControlCommand)i;
		}
	}

	return emu_ControlCommand_NULL;
}

static emu_StringConstant emu_parseStringConstant(emu_Parser* parser)
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

static uint8 emu_parseNumberConstant(emu_Parser* parser)
{
	// Skip the '#' character
	emu_getChar(parser);

	if (emu_peek(parser) != '%')
	{
		return emu_parseAddressConstant(parser, true);
	}

	return emu_parseBinaryConstant(parser);
}

static uint16 emu_parseAddressConstant(emu_Parser* parser, bool oneByteOnly)
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

static uint8 emu_parseBinaryConstant(emu_Parser* parser)
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

static emu_Keyword emu_isKeyword(emu_Parser* parser, emu_Symbol symbol)
{
	if (symbol.length != 3)
	{
		return emu_Keyword_NULL;
	}

	for (size_t i = 0; i < emu_Keyword_Length; i++)
	{
		bool isKeyword = true;
		for (size_t offset = 0; offset < 3; offset++)
		{
			char c = parser->file->data[symbol.start + offset];
			if (emu_toUpper(emu_Keywords[i][offset]) != emu_toUpper(c))
			{
				isKeyword = false;
				break;
			}
		}

		if (isKeyword)
		{
			return (emu_Keyword)i;
		}
	}

	return emu_Keyword_NULL;
}

static void emu_skipToEndOfLine(emu_Parser* parser)
{
	while (emu_peek(parser) != '\n')
	{
		emu_getChar(parser);
	}

	// Make sure to consume end line character as well
	emu_getChar(parser);
}

static void emu_emitOpcode_zeroPage(emu_Parser* parser, emu_Keyword keyword)
{
	switch (keyword)
	{
	case emu_Keyword_ldx:
		emu_emitOpcode(parser, emu_vmInstruction_LDX_ZP);
		break;
	case emu_Keyword_stx:
		emu_emitOpcode(parser, emu_vmInstruction_STX_ZP);
		break;
	case emu_Keyword_ldy:
		emu_emitOpcode(parser, emu_vmInstruction_LDY_ZP);
		break;
	case emu_Keyword_sty:
		emu_emitOpcode(parser, emu_vmInstruction_STY_ZP);
		break;
	case emu_Keyword_lda:
		emu_emitOpcode(parser, emu_vmInstruction_LDA_ZP);
		break;
	case emu_Keyword_sta:
		emu_emitOpcode(parser, emu_vmInstruction_STA_ZP);
		break;
	case emu_Keyword_clc:
		emu_emitOpcode(parser, emu_vmInstruction_CLC);
		break;
	case emu_Keyword_adc:
		emu_emitOpcode(parser, emu_vmInstruction_ADC_ZP);
		break;
	case emu_Keyword_rts:
		emu_emitOpcode(parser, emu_vmInstruction_RTS);
		break;
	case emu_Keyword_cmp:
		emu_emitOpcode(parser, emu_vmInstruction_CMP_ZP);
		break;
	default:
		g_logger_warning("Cannot emit invalid instruction.");
	}
}

static void emu_emitOpcode_immediate(emu_Parser* parser, emu_Keyword keyword)
{
	switch (keyword)
	{
	case emu_Keyword_ldx:
		emu_emitOpcode(parser, emu_vmInstruction_LDX_IMM);
		break;
	case emu_Keyword_ldy:
		emu_emitOpcode(parser, emu_vmInstruction_LDY_IMM);
		break;
	case emu_Keyword_lda:
		emu_emitOpcode(parser, emu_vmInstruction_LDA_IMM);
		break;
	case emu_Keyword_clc:
		emu_emitOpcode(parser, emu_vmInstruction_CLC);
		break;
	case emu_Keyword_adc:
		emu_emitOpcode(parser, emu_vmInstruction_ADC_IMM);
		break;
	case emu_Keyword_rts:
		emu_emitOpcode(parser, emu_vmInstruction_RTS);
		break;
	case emu_Keyword_cmp:
		emu_emitOpcode(parser, emu_vmInstruction_CMP_IMM);
		break;
	case emu_Keyword_stx:
	case emu_Keyword_sty:
	case emu_Keyword_sta:
	default:
		g_logger_warning("Cannot emit invalid instruction.");
	}
}

static void emu_emitOpcode(emu_Parser* parser, emu_vmInstruction opcode)
{
	if (parser->programIndex >= parser->programSize)
	{
		g_logger_warning("Ran out of program memory. Cannot emit anymore instructions.");
		return;
	}

	parser->program[parser->programIndex] = (uint8)opcode;
	parser->programIndex++;
}

static void emu_emitConstant(emu_Parser* parser, uint8 constant)
{
	if (parser->programIndex >= parser->programSize)
	{
		g_logger_warning("Ran out of program memory. Cannot emit anymore instructions.");
		return;
	}

	parser->program[parser->programIndex] = constant;
	parser->programIndex++;
}

static char emu_getChar(emu_Parser* parser)
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

static void emu_expectChar(emu_Parser* parser, char expected)
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

static char emu_peek(emu_Parser* parser)
{
	return emu_peekMulti(parser, 0);
}

static char emu_peekMulti(emu_Parser* parser, uint8 offset)
{
	if (parser->current + offset >= parser->file->data_size)
	{
		return '\0';
	}

	return parser->file->data[parser->current + offset];
}

static void emu_skip(emu_Parser* parser, size_t skipAmount)
{
	for (size_t i = 0; i < skipAmount; i++)
	{
		emu_getChar(parser);
	}
}

static char emu_toUpper(char c)
{
	if (c >= 'a' && c <= 'z')
	{
		return (c - 'a') + 'A';
	}

	return c;
}

static bool emu_isWhitespace(char c)
{
	return c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == '\0';
}

static bool emu_isSymbolStart(char c)
{
	return c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c == '_' || c == '.';
}

static bool emu_isSymbolChar(char c)
{
	return c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c >= '0' && c <= '9' || c == '_' || c == '.';
}

static void emu_logError(emu_Parser* parser, const char* fmtString, ...)
{
	va_list args;
	va_start(args, fmtString);
	emu_logErrorLineColumnWithArgs(parser, parser->currentLine, parser->currentColumn, fmtString, args);
	va_end(args);
}

static void emu_logErrorLineColumn(emu_Parser* parser, size_t line, size_t column, const char* fmtString, ...)
{
	static char messageBuffer[1'024];
	va_list args;
	va_start(args, fmtString);
	emu_logErrorLineColumnWithArgs(parser, line, column, fmtString, args);
	va_end(args);
}

static void emu_logErrorLineColumnWithArgs(emu_Parser* parser, size_t line, size_t column, const char* fmtString, va_list args)
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

static void emu_debugPrintToken(emu_Parser* parser, emu_Token* token)
{
	g_logger_info(
		"Token<%s:%d:%d>: '%.*s'",
		emu_TokenTypes[token->type],
		token->line,
		token->column,
		token->length,
		parser->file->data + token->start
	);
}