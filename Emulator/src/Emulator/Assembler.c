#include "Emulator/Assembler.h"
#include "Emulator/Parser.h"
#include "Emulator/VirtualMachine.h"
#include "utils/FileHelper.h"

#include <stb/stb_ds.h>
#include <stdio.h>
#include <stdarg.h>

#define EMIT_IMPLICIT_OPCODE(type) \
case emu_Keyword_##type:\
emu_emitOpcode(assembler, emu_vmInstruction_##type##_IMP);\
break

#define EMIT_IMMEDIATE_OPCODE(type) \
case emu_Keyword_##type:\
emu_emitOpcode(assembler, emu_vmInstruction_##type##_IMM);\
break

#define EMIT_ZERO_PAGE_OPCODE(type) \
case emu_Keyword_##type:\
emu_emitOpcode(assembler, emu_vmInstruction_##type##_ZP);\
break

// Internal structures
typedef enum emu_AddressingMode
{
	emu_AddressingMode_ZeroPage,
	emu_AddressingMode_Immediate,
	emu_AddressingMode_Implicit,
	emu_AddressingMode_Absolute,
	emu_AddressingMode_Jump,
	emu_AddressingMode_Unknown,
} emu_AddressingMode;

typedef struct emu_Argument
{
	emu_Token const* token;
	bool isLabel;
	union
	{
		uint8 byte;
		uint16 word;
	} as;
} emu_Argument;

typedef struct emu_ArgList
{
	emu_AddressingMode mode;
	emu_Argument arg0;
	emu_Argument arg1;
	size_t numArgs;
} emu_ArgList;

typedef struct emu_PatchLocation
{
	// The index in the program that needs to be patched
	size_t programIndex;
	// The label we need to jump to
	char* label;
	// Debug info
	emu_Token const* token;
} emu_PatchLocation;

typedef struct emu_Label
{
	char* key;
	// The program index where this label is located
	size_t value;
} emu_Label;

typedef enum emu_StatementError
{
	emu_StatementError_None = 0,
	emu_StatementError_Invalid,
	emu_StatementError_EOF
} emu_StatementError;

typedef struct emu_Assembler
{
	emu_assembler_program program;
	size_t current;
	emu_TokenList* tokenList;

	size_t programIndex;
	emu_Label* labels;
	emu_PatchLocation* patches;
} emu_Assembler;

// Internal Functions
static emu_StatementError assembleNextStatement(emu_Assembler* assembler);
static emu_StatementError assembleInstruction(emu_Assembler* assembler, emu_Token const* token);
static emu_ArgList* parseArgList(emu_Assembler* assembler);
static void freeArgList(emu_ArgList* argList);
static void addPatch(emu_Assembler* assembler, emu_Token const* token);

static void emu_emitImplicitOpcode(emu_Assembler* assembler, emu_Token const* token);
static void emu_emitImmediateOpcode(emu_Assembler* assembler, emu_Token const* token);
static void emu_emitZeroPageOpcode(emu_Assembler* assembler, emu_Token const* token);
static void emu_emitOpcode(emu_Assembler* assembler, emu_vmInstruction opcode);
static void emu_emitByte(emu_Assembler* assembler, uint8 opcode);

static bool isArgStart(emu_Assembler* assembler);

static bool emu_expectImplicitCommand(emu_Assembler* assembler, emu_Token const* token);
static bool emu_expectImmediateCommand(emu_Assembler* assembler, emu_Token const* token);
static bool emu_expectZeroPageCommand(emu_Assembler* assembler, emu_Token const* token);
static bool emu_expect(emu_Assembler* assembler, emu_TokenType expected);
static void emu_logError(emu_Assembler* assembler, emu_Token const* token, const char* fmtString, ...);
static emu_Token const* getNext(emu_Assembler* assembler);
static emu_TokenType peek(emu_Assembler* assembler);
static emu_TokenType peekMulti(emu_Assembler* assembler, size_t offset);

// Public Functions
emu_assembler_program emu_assembler_assembleProgram(const char* filename, size_t programSize)
{
	emu_TokenList tokenList = emu_parser_parseFile(filename);

	//for (size_t i = 0; i < emu_parser_tokenListLength(&tokenList); i++)
	//{
	//	emu_parser_debugPrintToken(&tokenList, i);
	//}

	emu_Assembler assembler = {
		.program = {.data = g_memory_allocate(programSize), .size = programSize },
	.current = 0,
	.programIndex = 0,
	.tokenList = &tokenList,
	.labels = NULL,
	.patches = NULL,
	};

	emu_StatementError error = emu_StatementError_None;
	while (!error)
	{
		error = assembleNextStatement(&assembler);
	}

	emu_parser_freeTokenList(&tokenList);

	for (int i = 0; i < stbds_arrlen(assembler.patches); i++)
	{
		emu_PatchLocation* patch = assembler.patches + i;
		g_memory_free(patch->label);
	}

	for (int i = 0; i < stbds_shlen(assembler.labels); i++)
	{
		g_memory_free(assembler.labels[i].key);
	}

	stbds_arrfree(assembler.patches);
	stbds_shfree(assembler.labels);

	return (emu_assembler_program)
	{
		.data = assembler.program.data,
			.size = assembler.programIndex,
	};
}

void emu_assembler_free(emu_assembler_program* program)
{
	if (!program)
	{
		return;
	}

	if (program->data)
	{
		g_memory_free(program->data);
	}
}

// Internal definitions
static emu_StatementError assembleNextStatement(emu_Assembler* assembler)
{
	emu_Token const* token = getNext(assembler);
	if (!token)
	{
		return emu_StatementError_EOF;
	}

	switch (token->type)
	{
	case emu_TokenType_Keyword:
		return assembleInstruction(assembler, token);
		// TODO: Add support for control commands
	case emu_TokenType_ControlCommand:
	case emu_TokenType_Symbol:
	case emu_TokenType_String:
	case emu_TokenType_Comment:
		return emu_StatementError_None;
	}

	return emu_StatementError_Invalid;
}

static emu_StatementError assembleInstruction(emu_Assembler* assembler, emu_Token const* token)
{
	emu_ArgList* argList = parseArgList(assembler);

	if (argList->mode == emu_AddressingMode_Implicit)
	{
		if (emu_expectImplicitCommand(assembler, token))
		{ 
			emu_emitImplicitOpcode(assembler, token);
		}
		else
		{
			freeArgList(argList);
			return emu_StatementError_Invalid;
		}
	}
	else if (argList->mode == emu_AddressingMode_Immediate)
	{
		if (emu_expectImmediateCommand(assembler, token))
		{
			emu_emitImmediateOpcode(assembler, token);
			emu_emitByte(assembler, argList->arg0.as.byte);
		}
		else
		{
			freeArgList(argList);
			return emu_StatementError_Invalid;
		}
	}
	else if (argList->mode == emu_AddressingMode_ZeroPage)
	{
		if (emu_expectZeroPageCommand(assembler, token))
		{
			emu_emitZeroPageOpcode(assembler, token);
			emu_emitByte(assembler, argList->arg0.as.byte);
		}
		else
		{
			freeArgList(argList);
			return emu_StatementError_Invalid;
		}
	}

	freeArgList(argList);
	return emu_StatementError_None;
}

static emu_ArgList* parseArgList(emu_Assembler* assembler)
{
	emu_ArgList* argList = g_memory_allocate(sizeof(emu_ArgList));
	*argList = (emu_ArgList){ 0 };

	// Assume no arguments will be provided
	argList->mode = emu_AddressingMode_Implicit;
	argList->arg0.isLabel = false;
	argList->arg1.isLabel = false;

	if (!isArgStart(assembler))
	{
		return argList;
	}

	// For now, just expect either label/byte/word
	argList->arg0.token = getNext(assembler);
	if (argList->arg0.token->type == emu_TokenType_ByteConstant)
	{
		argList->arg0.as.byte = argList->arg0.token->data.byteConstant;
		argList->mode = emu_AddressingMode_ZeroPage;
	}
	else if (argList->arg0.token->type == emu_TokenType_ImmediateConstant)
	{
		argList->arg0.as.byte = argList->arg0.token->data.byteConstant;
		argList->mode = emu_AddressingMode_Immediate;
	}
	else if (argList->arg0.token->type == emu_TokenType_TwoByteConstant)
	{
		argList->arg0.as.word = argList->arg0.token->data.twoByteConstant;
		argList->mode = emu_AddressingMode_Absolute;
	}
	else if (argList->arg0.token->type == emu_TokenType_Symbol)
	{
		argList->mode = emu_AddressingMode_Jump;
		argList->arg0.isLabel = true;
	}
	else
	{
		g_logger_error("Unexpected token type: %s", emu_TokenTypes[argList->arg0.token->type]);
	}

	// If we don't see a comma, then this instruction only has a single argument
	if (peek(assembler) != emu_TokenType_Comma)
	{
		argList->numArgs = 1;
		return argList;
	}

	// Otherwise, expect a comma and then parse one more instruction
	emu_expect(assembler, emu_TokenType_Comma);

	// For now, just expect either label/byte/word

	// TODO: Add support for more complex addressing modes
	argList->mode = emu_AddressingMode_Unknown;

	argList->arg1.token = getNext(assembler);
	if (argList->arg1.token->type == emu_TokenType_ByteConstant)
	{
		argList->arg1.as.byte = argList->arg1.token->data.byteConstant;
	}
	else if (argList->arg1.token->type == emu_TokenType_TwoByteConstant)
	{
		argList->arg1.as.word = argList->arg1.token->data.twoByteConstant;
	}
	else if (argList->arg1.token->type == emu_TokenType_Symbol)
	{
		argList->arg1.isLabel = true;
	}
	else
	{
		g_logger_error("Unexpected token type: %s", emu_TokenTypes[argList->arg1.token->type]);
	}

	argList->numArgs = 2;
	return argList;
}

static void freeArgList(emu_ArgList* argList)
{
	if (argList)
	{
		g_memory_free(argList);
	}
}

static void emu_emitImplicitOpcode(emu_Assembler* assembler, emu_Token const* token)
{
	switch (token->data.keyword)
	{
		EMIT_IMPLICIT_OPCODE(DEX);
		EMIT_IMPLICIT_OPCODE(DEY);
		EMIT_IMPLICIT_OPCODE(INX);
		EMIT_IMPLICIT_OPCODE(INY);
		EMIT_IMPLICIT_OPCODE(ASL);
		EMIT_IMPLICIT_OPCODE(ROL);
		EMIT_IMPLICIT_OPCODE(LSR);
		EMIT_IMPLICIT_OPCODE(ROR);
		EMIT_IMPLICIT_OPCODE(RTS);
	}
}

static void emu_emitImmediateOpcode(emu_Assembler* assembler, emu_Token const* token)
{
	switch (token->data.keyword)
	{
		EMIT_IMMEDIATE_OPCODE(ORA);
		EMIT_IMMEDIATE_OPCODE(AND);
		EMIT_IMMEDIATE_OPCODE(EOR);
		EMIT_IMMEDIATE_OPCODE(ADC);
		EMIT_IMMEDIATE_OPCODE(SBC);
		EMIT_IMMEDIATE_OPCODE(CMP);
		EMIT_IMMEDIATE_OPCODE(CPX);
		EMIT_IMMEDIATE_OPCODE(CPY);
		EMIT_IMMEDIATE_OPCODE(LDA);
		EMIT_IMMEDIATE_OPCODE(LDX);
		EMIT_IMMEDIATE_OPCODE(LDY);
	}
}

static void emu_emitZeroPageOpcode(emu_Assembler* assembler, emu_Token const* token)
{
	switch (token->data.keyword)
	{
		EMIT_ZERO_PAGE_OPCODE(ORA);
		EMIT_ZERO_PAGE_OPCODE(AND);
		EMIT_ZERO_PAGE_OPCODE(EOR);
		EMIT_ZERO_PAGE_OPCODE(ADC);
		EMIT_ZERO_PAGE_OPCODE(SBC);
		EMIT_ZERO_PAGE_OPCODE(CMP);
		EMIT_ZERO_PAGE_OPCODE(CPX);
		EMIT_ZERO_PAGE_OPCODE(CPY);
		EMIT_ZERO_PAGE_OPCODE(DEC);
		EMIT_ZERO_PAGE_OPCODE(INC);
		EMIT_ZERO_PAGE_OPCODE(ASL);
		EMIT_ZERO_PAGE_OPCODE(ROL);
		EMIT_ZERO_PAGE_OPCODE(LSR);
		EMIT_ZERO_PAGE_OPCODE(ROR);
		EMIT_ZERO_PAGE_OPCODE(LDA);
		EMIT_ZERO_PAGE_OPCODE(STA);
		EMIT_ZERO_PAGE_OPCODE(LDX);
		EMIT_ZERO_PAGE_OPCODE(STX);
		EMIT_ZERO_PAGE_OPCODE(LDY);
		EMIT_ZERO_PAGE_OPCODE(STY);
	}
}

static void emu_emitOpcode(emu_Assembler* assembler, emu_vmInstruction opcode)
{
	emu_emitByte(assembler, opcode);
}

static void emu_emitByte(emu_Assembler* assembler, uint8 opcode)
{
	if (assembler->programIndex >= assembler->program.size)
	{
		return;
	}

	assembler->program.data[assembler->programIndex] = opcode;
	assembler->programIndex++;
}

static bool isArgStart(emu_Assembler* assembler)
{
	emu_TokenType type = peek(assembler);
	emu_TokenType nextType = peekMulti(assembler, 1);
	return type == emu_TokenType_ByteConstant || type == emu_TokenType_ImmediateConstant || type == emu_TokenType_TwoByteConstant || (type == emu_TokenType_Symbol && nextType != emu_TokenType_Symbol);
}

static bool emu_expect(emu_Assembler* assembler, emu_TokenType expected)
{
	emu_Token const* token = getNext(assembler);
	if (token->type == expected)
	{
		return true;
	}

	emu_logError(assembler, token, "Expected token of type '%s', instead got type '%s'.", emu_TokenTypes[expected], emu_TokenTypes[token->type]);
	return false;
}

static bool emu_expectImplicitCommand(emu_Assembler* assembler, emu_Token const* token)
{
	switch (token->data.keyword)
	{
	case emu_Keyword_DEX:
	case emu_Keyword_DEY:
	case emu_Keyword_INX:
	case emu_Keyword_INY:
	case emu_Keyword_ASL:
	case emu_Keyword_ROL:
	case emu_Keyword_LSR:
	case emu_Keyword_ROR:
	case emu_Keyword_RTS:
		return true;
	}

	emu_logError(assembler, token, "Instruction '%s' does not support implicit addressing mode. Please supply an argument, ex: '%s #00'", emu_Keywords[token->data.keyword], emu_Keywords[token->data.keyword]);
	return false;
}

static bool emu_expectImmediateCommand(emu_Assembler* assembler, emu_Token const* token)
{
	switch (token->data.keyword)
	{
	case emu_Keyword_ORA:
	case emu_Keyword_AND:
	case emu_Keyword_EOR:
	case emu_Keyword_ADC:
	case emu_Keyword_SBC:
	case emu_Keyword_CMP:
	case emu_Keyword_CPX:
	case emu_Keyword_CPY:
	case emu_Keyword_LDA:
	case emu_Keyword_LDX:
	case emu_Keyword_LDY:
		return true;
	}

	emu_logError(assembler, token, "Instruction '%s' does not support Immediate addressing mode.", emu_Keywords[token->data.keyword]);
	return false;
}

static bool emu_expectZeroPageCommand(emu_Assembler* assembler, emu_Token const* token)
{
	switch (token->data.keyword)
	{
	case emu_Keyword_ORA:
	case emu_Keyword_AND:
	case emu_Keyword_EOR:
	case emu_Keyword_ADC:
	case emu_Keyword_SBC:
	case emu_Keyword_CMP:
	case emu_Keyword_CPX:
	case emu_Keyword_CPY:
	case emu_Keyword_DEC:
	case emu_Keyword_INC:
	case emu_Keyword_ASL:
	case emu_Keyword_ROL:
	case emu_Keyword_LSR:
	case emu_Keyword_ROR:
	case emu_Keyword_LDA:
	case emu_Keyword_STA:
	case emu_Keyword_LDX:
	case emu_Keyword_STX:
	case emu_Keyword_LDY:
	case emu_Keyword_STY:
		return true;
	}

	emu_logError(assembler, token, "Instruction '%s' does not support Zero Page addressing mode.", emu_Keywords[token->data.keyword]);
	return false;
}

static void emu_logError(emu_Assembler* assembler, emu_Token const* token, const char* fmtString, ...)
{
	static char messageBuffer[1'024];
	va_list args;
	va_start(args, fmtString);
	vsnprintf(messageBuffer, sizeof(messageBuffer), fmtString, args);
	va_end(args);

	emu_file* file = assembler->tokenList->sourceFile;
	size_t lineStart = 0;
	size_t lineEnd = file->data_size;
	for (size_t i = token->start; i > 0; i--)
	{
		if (file->data[i] == '\n' && i != token->start)
		{
			lineStart = i + 1;
			break;
		}
	}

	for (size_t i = token->start + token->length; i < file->data_size; i++)
	{
		if (file->data[i] == '\n')
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
		(int)token->line,
		(int)token->column,
		messageBuffer,
		(int)(lineEnd - lineStart),
		file->data + lineStart,
		(int)token->column - 1,
		""
	);

	printf("%s\n", fullErrorBuffer);
}

static emu_Token const* getNext(emu_Assembler* assembler)
{
	size_t tokenListLength = emu_parser_tokenListLength(assembler->tokenList);
	if (assembler->current >= tokenListLength)
	{
		return NULL;
	}

	emu_Token const* token = assembler->tokenList->tokens + assembler->current;
	assembler->current++;
	return token;
}

static emu_TokenType peek(emu_Assembler* assembler)
{
	return peekMulti(assembler, 0);
}

static emu_TokenType peekMulti(emu_Assembler* assembler, size_t offset)
{
	size_t tokenListLength = emu_parser_tokenListLength(assembler->tokenList);
	if (assembler->current + offset >= tokenListLength)
	{
		return emu_TokenType_NULL;
	}

	return assembler->tokenList->tokens[assembler->current + offset].type;
}