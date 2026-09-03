#ifndef EMU_PARSER_H
#define EMU_PARSER_H
#include <stdbool.h>

// Forward Declarations
typedef struct emu_Token emu_Token;
typedef struct emu_file emu_file;

// Public Structures/Constants
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
