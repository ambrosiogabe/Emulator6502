#include "Emulator/Parser.h"
#include "Emulator/VirtualMachine.h"
#include "utils/FileHelper.h"

#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <stb/stb_ds.h>

// Internal structures
const char* emu_ControlCommands[] = {
	"export",
	"segment",
	"proc",
	"endproc",
	"byte",
	"addr",
	"LENGTH",
	"NULL"
};

const char* emu_Keywords[] = {
	"ldx",
	"stx",

	"ldy",
	"sty",

	"lda",
	"sta",


	"rts",

	// Jump/Flag commands
	"bpl",
	"bmi",
	"bvc",
	"bvs",
	"bcc",
	"bcs",
	"bne",
	"beq",
	"sec",
	"clc",
	"jsr",

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

const char* emu_TokenTypes[] = {
	"NULL",
	"ControlCommand",
	"Keyword",
	"Comment",
	"Symbol",
	"String",
	"Character",
	"Comma",
	"Plus",
	"Minus",
	"RightAngleBracket",
	"LeftAngleBracket",
	"Colon",
	"ImmediateConstant",
	"ByteConstant",
	"TwoByteConstant",
	"Length"
};

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
	emu_Token* tokens;
} emu_Parser;

// Internal functions
static emu_Token emu_parseToken(emu_Parser* parser);
static emu_Token emu_makeToken(emu_TokenType tokenType, size_t start, size_t end, size_t line, size_t column, emu_TokenData data);

/**
* Parses from 'current' to the next whitespace character and stores the result in 'symbolStart' and 'symbolLength'
*/
static emu_Symbol emu_parseSymbol(emu_Parser* parser);
static emu_ControlCommand emu_parseControlCommand(emu_Parser* parser);
static emu_StringConstant emu_parseStringConstant(emu_Parser* parser);
static char emu_parseCharConstant(emu_Parser* parser);
static emu_Keyword emu_isKeyword(emu_Parser* parser, emu_Symbol symbol);
static uint8 emu_parseNumberConstant(emu_Parser* parser);
static uint16 emu_parseAddressConstant(emu_Parser* parser, bool oneByteOnly);
static uint16 emu_parseAddressConstantWithLength(emu_Parser* parser, bool oneByteOnly, bool* isAbsolute);
static uint8 emu_parseBinaryConstant(emu_Parser* parser);
static void emu_skipToEndOfLine(emu_Parser* parser);

static char emu_getChar(emu_Parser* parser);
static void emu_expectChar(emu_Parser* parser, char expected);
static char emu_peek(emu_Parser* parser);
static char emu_peekMulti(emu_Parser* parser, size_t offset);
static void emu_skip(emu_Parser* parser, size_t skipAmount);
static char emu_toUpper(char c);

static bool emu_isWhitespace(char c);
static bool emu_isSymbolStart(char c);
static bool emu_isSymbolChar(char c);

static void emu_logError(emu_Parser* parser, const char* fmtString, ...);
static void emu_logErrorLineColumn(emu_Parser* parser, size_t line, size_t column, const char* fmtString, ...);
static void emu_logErrorLineColumnWithArgs(emu_Parser* parser, size_t line, size_t column, const char* fmtString, va_list args);

static void emu_freeParser(emu_Parser* parser);

// Public functions
void emu_parser_debugPrintToken(emu_TokenList* tokenList, size_t tokenIndex)
{
	emu_Token* token = tokenList->tokens + tokenIndex;
	g_logger_info(
		"Token<%s:%u:%u>: '%.*s'",
		emu_TokenTypes[token->type],
		token->line,
		token->column,
		token->length,
		tokenList->sourceFile->data + token->start
	);
}

size_t emu_parser_tokenListLength(emu_TokenList* tokenList)
{
	return stbds_arrlen(tokenList->tokens);
}

emu_TokenList emu_parser_parseFile(const char* filename)
{
	emu_file* file = g_memory_allocate(sizeof(emu_file));
	g_memory_zeroMem(file, sizeof(emu_file));
	if (emu_file_read(filename, file) != emu_fileResult_Success)
	{
		g_logger_error("Failed to read file '%s'. Cannot assemble program.", filename);
		return (emu_TokenList)
		{
			.sourceFile = NULL,
				.tokens = NULL
		};
	}

	// TODO: Parser isn't in charge of emitting bytecode. Move this to the assembler
	const size_t programSize = 1'024;
	uint8* program = g_memory_allocate(programSize);
	emu_Parser parser = {
		.current = 0,
		.file = file,
		.program = program,
		.programSize = programSize,
		.currentColumn = 1,
		.currentLine = 1,
		.currentInstruction = emu_Keyword_NULL,
		.tokens = NULL,
	};

	for (size_t i = 0; i < file->data_size; i++)
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

	emu_freeParser(&parser);

	// Safe because we don't free these two fields when we free the parser
	return (emu_TokenList)
	{
		.sourceFile = parser.file,
			.tokens = parser.tokens,
	};
}

void emu_parser_freeTokenList(emu_TokenList* tokenList)
{
	stbds_arrfree(tokenList->tokens);
	emu_file_free(tokenList->sourceFile);
	g_memory_free(tokenList->sourceFile);
}

// Internal definitions
static emu_Token emu_parseToken(emu_Parser* parser)
{
	size_t start = parser->current;
	size_t line = parser->currentLine;
	size_t column = parser->currentColumn;
	char c = emu_peek(parser);

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
		emu_parseStringConstant(parser);
		return emu_makeToken(emu_TokenType_String, start, parser->current, line, column, (emu_TokenData) { 0 });
	}
	case '\'':
	{
		char charConstant = emu_parseCharConstant(parser);
		return emu_makeToken(emu_TokenType_Character, start, parser->current, line, column, (emu_TokenData) { .byteConstant = charConstant });
	}
	case '%':
	{
		uint8 binaryConstant = emu_parseBinaryConstant(parser);
		return emu_makeToken(emu_TokenType_ImmediateConstant, start, parser->current, line, column, (emu_TokenData) { .byteConstant = binaryConstant });
	}
	case '#':
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	{
		uint8 numberConstant = emu_parseNumberConstant(parser);
		return emu_makeToken(emu_TokenType_ImmediateConstant, start, parser->current, line, column, (emu_TokenData) { .byteConstant = numberConstant });
	}
	case '$':
	{
		bool isAbsolute;
		uint16 numberConstant = emu_parseAddressConstantWithLength(parser, false, &isAbsolute);
		emu_TokenType type = isAbsolute ? emu_TokenType_TwoByteConstant : emu_TokenType_ByteConstant;
		emu_TokenData data = { 0 };
		if (isAbsolute)
		{
			data.twoByteConstant = numberConstant;
		}
		else
		{
			data.byteConstant = (uint8)numberConstant;
		}
		return emu_makeToken(type, start, parser->current, line, column, data);
	}
	case ',':
		emu_getChar(parser);
		return emu_makeToken(emu_TokenType_Comma, start, parser->current, line, column, (emu_TokenData) { 0 });
	case ':':
		emu_getChar(parser);
		return emu_makeToken(emu_TokenType_Colon, start, parser->current, line, column, (emu_TokenData) { 0 });
	case '+':
		emu_getChar(parser);
		return emu_makeToken(emu_TokenType_Plus, start, parser->current, line, column, (emu_TokenData) { 0 });
	case '-':
		emu_getChar(parser);
		return emu_makeToken(emu_TokenType_Minus, start, parser->current, line, column, (emu_TokenData) { 0 });
	case '>':
		emu_getChar(parser);
		return emu_makeToken(emu_TokenType_RightAngleBracket, start, parser->current, line, column, (emu_TokenData) { 0 });
	case '<':
		emu_getChar(parser);
		return emu_makeToken(emu_TokenType_LeftAngleBracket, start, parser->current, line, column, (emu_TokenData) { 0 });
	default:
		if (emu_isSymbolStart(c))
		{
			emu_Symbol symbol = emu_parseSymbol(parser);
			emu_Keyword instruction = emu_isKeyword(parser, symbol);
			if (instruction != emu_Keyword_NULL)
			{
				parser->currentInstruction = instruction;
				return emu_makeToken(emu_TokenType_Keyword, start, parser->current, line, column, (emu_TokenData) { .keyword = instruction });
			}

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

static char emu_parseCharConstant(emu_Parser* parser)
{
	// Parse beginning '\''
	emu_expectChar(parser, '\'');
	char result = emu_getChar(parser);
	// Parse end '\''
	emu_expectChar(parser, '\'');

	return result;
}

static uint8 emu_parseNumberConstant(emu_Parser* parser)
{
	// Skip the '#' character
	if (emu_peek(parser) == '#')
	{
		emu_getChar(parser);
	}

	if (emu_peek(parser) != '%')
	{
		return (uint8)emu_parseAddressConstant(parser, true);
	}

	return emu_parseBinaryConstant(parser);
}

static uint16 emu_parseAddressConstant(emu_Parser* parser, bool oneByteOnly)
{
	bool isAbsolute;
	return emu_parseAddressConstantWithLength(parser, oneByteOnly, &isAbsolute);
}

static uint16 emu_parseAddressConstantWithLength(emu_Parser* parser, bool oneByteOnly, bool* isAbsolute)
{
	size_t columnStart = parser->currentColumn;

	char start = emu_getChar(parser);
	bool isHexadecimal = start == '$';
	size_t digitStart = isHexadecimal ? parser->current : parser->current - 1;
	bool isInvalid = false;
	while (!emu_isWhitespace(emu_peek(parser)))
	{
		char digit = emu_peek(parser);
		if (digit == ',')
		{
			break;
		}
		else if (isHexadecimal)
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
			uint16 base = (uint16)pow(16, (double)i);
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
			uint16 base = (uint16)pow(10, (double)i);
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

	*isAbsolute = digitCharLength > 2;
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
		if (digit == ',')
		{
			break;
		}
		else if (digit != '0' && digit != '1')
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

		uint16 base = (uint16)pow(2, (double)i);
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

	return (uint8)result;
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

static char emu_peekMulti(emu_Parser* parser, size_t offset)
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
	vsnprintf(messageBuffer, sizeof(messageBuffer), fmtString, args);

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

static void emu_freeParser(emu_Parser* parser)
{
	g_memory_free(parser->program);

	// NOTE: We explicitly don't free the file or tokens since we return those in the parsed result.
	//       It is expected that the user of this interface will eventually call emu_parser_freeTokenList
	//       which will clean up that memory.
}