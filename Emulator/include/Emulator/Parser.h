#ifndef EMU_PARSER_H
#define EMU_PARSER_H
#include <stdbool.h>
#include <stdint.h>
#include "utils/SafeVendor.h"

// Forward Declarations
typedef struct emu_file emu_file;

// Public Structures/Constants
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

extern const char* emu_ControlCommands[];

typedef enum emu_Keyword
{
	emu_Keyword_LDX,
	emu_Keyword_STX,

	emu_Keyword_LDY,
	emu_Keyword_STY,

	emu_Keyword_LDA,
	emu_Keyword_STA,

	emu_Keyword_CLC,

	emu_Keyword_RTS,
	emu_Keyword_BCC,

	// Logical/Arithmetic commands
	emu_Keyword_ORA,
	emu_Keyword_AND,
	emu_Keyword_EOR,
	emu_Keyword_ADC,
	emu_Keyword_SBC,
	emu_Keyword_CMP,
	emu_Keyword_CPX,
	emu_Keyword_CPY,
	emu_Keyword_DEC,
	emu_Keyword_DEX,
	emu_Keyword_DEY,
	emu_Keyword_INC,
	emu_Keyword_INX,
	emu_Keyword_INY,
	emu_Keyword_ASL,
	emu_Keyword_ROL,
	emu_Keyword_LSR,
	emu_Keyword_ROR,

	emu_Keyword_Length,
	emu_Keyword_NULL,
} emu_Keyword;

extern const char* emu_Keywords[];

typedef enum emu_TokenType
{
	emu_TokenType_NULL = 0,
	emu_TokenType_ControlCommand,
	emu_TokenType_Keyword,
	emu_TokenType_Comment,
	emu_TokenType_Symbol,
	emu_TokenType_String,
	emu_TokenType_Comma,
	emu_TokenType_Colon,
	emu_TokenType_ImmediateConstant,
	emu_TokenType_ByteConstant,
	emu_TokenType_TwoByteConstant,
	emu_TokenType_Length
} emu_TokenType;

extern const char* emu_TokenTypes[];

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

typedef struct emu_TokenList
{
	emu_Token* tokens;
	emu_file* sourceFile;
} emu_TokenList;

// Public functions
void emu_parser_debugPrintToken(emu_TokenList* tokenList, size_t tokenIndex);
size_t emu_parser_tokenListLength(emu_TokenList* tokenList);
emu_TokenList emu_parser_parseFile(const char* filename);
void emu_parser_freeTokenList(emu_TokenList* tokenList);

#endif
