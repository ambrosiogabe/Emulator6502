#include "Emulator/Assembler.h"
#include "Emulator/Parser.h"
#include "Emulator/VirtualMachine.h"
#include "utils/FileHelper.h"
#include "utils/SafeVendor.h"

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

#define EMIT_RELATIVE_OPCODE(type) \
case emu_Keyword_##type:\
emu_emitOpcode(assembler, emu_vmInstruction_##type##_REL);\
break

#define EMIT_ABSOLUTE_OPCODE(type) \
case emu_Keyword_##type:\
emu_emitOpcode(assembler, emu_vmInstruction_##type##_ABS);\
break

#define EMIT_ABSOLUTE_X_OPCODE(type) \
case emu_Keyword_##type:\
emu_emitOpcode(assembler, emu_vmInstruction_##type##_ABX);\
break

// Internal structures
typedef enum emu_AddressingMode
{
	emu_AddressingMode_ZeroPage,
	emu_AddressingMode_Immediate,
	emu_AddressingMode_Implicit,
	emu_AddressingMode_Absolute,
	emu_AddressingMode_Relative,
	emu_AddressingMode_Unknown,
} emu_AddressingMode;

typedef enum emu_ArgumentType
{
	emu_ArgumentType_Byte,
	emu_ArgumentType_Word,
	emu_ArgumentType_Label,
	emu_ArgumentType_X,
	emu_ArgumentType_Y
} emu_ArgumentType;

typedef struct emu_Argument
{
	emu_Token const* token;
	emu_ArgumentType type;
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
	size_t programRomIndex;
	emu_Label* labels;
	emu_PatchLocation* patches;
} emu_Assembler;

// Internal Functions
static emu_StatementError assembleNextStatement(emu_Assembler* assembler);
static emu_StatementError assembleInstruction(emu_Assembler* assembler, emu_Token const* token);
static emu_StatementError assembleControlCommand(emu_Assembler* assembler, emu_Token const* token);
static emu_StatementError parseLabel(emu_Assembler* assembler, emu_Token const* token);
static emu_ArgList* parseArgList(emu_Assembler* assembler, emu_Token const* token);
static void freeArgList(emu_ArgList* argList);
static void addPatch(emu_Assembler* assembler, emu_Token const* token);

static emu_StatementError emu_parseAndEmitByteList(emu_Assembler* assembler);
static void emu_emitImplicitOpcode(emu_Assembler* assembler, emu_Token const* token);
static void emu_emitImmediateOpcode(emu_Assembler* assembler, emu_Token const* token);
static void emu_emitZeroPageOpcode(emu_Assembler* assembler, emu_Token const* token);
static void emu_emitRelativeOpcode(emu_Assembler* assembler, emu_Token const* token);
static void emu_emitAbsoluteOpcode(emu_Assembler* assembler, emu_Token const* token);
static void emu_emitAbsoluteXOpcode(emu_Assembler* assembler, emu_Token const* token);
static void emu_emitOpcode(emu_Assembler* assembler, emu_vmInstruction opcode);
static void emu_emitRomByte(emu_Assembler* assembler, uint8 opcode);
static void emu_emitByte(emu_Assembler* assembler, uint8 opcode);

static bool isArgStart(emu_Assembler* assembler);

static bool emu_expectImplicitCommand(emu_Assembler* assembler, emu_Token const* token);
static bool emu_expectImmediateCommand(emu_Assembler* assembler, emu_Token const* token);
static bool emu_expectZeroPageCommand(emu_Assembler* assembler, emu_Token const* token);
static bool emu_expectRelativeCommandWithError(emu_Assembler* assembler, emu_Token const* token, bool withError);
static bool emu_expectRelativeCommand(emu_Assembler* assembler, emu_Token const* token);
static bool emu_expectAbsoluteCommandWithError(emu_Assembler* assembler, emu_Token const* token, bool withError);
static bool emu_expectAbsoluteCommand(emu_Assembler* assembler, emu_Token const* token);
static bool emu_expect(emu_Assembler* assembler, emu_TokenType expected);
static emu_Token const* emu_expectOneOf(emu_Assembler* assembler, emu_TokenType* expected, size_t numExpected);
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
	.programRomIndex = programSize / 2,
	.tokenList = &tokenList,
	.labels = NULL,
	.patches = NULL,
	};

	emu_StatementError error = emu_StatementError_None;
	while (!error)
	{
		error = assembleNextStatement(&assembler);
	}

	// Patch all the needed patches
	for (int i = 0; i < stbds_arrlen(assembler.patches); i++)
	{
		emu_PatchLocation* patch = assembler.patches + i;

		if (stbds_shgeti(assembler.labels, patch->label) >= 0)
		{
			size_t value = stbds_shget(assembler.labels, patch->label);
			int16 relativeOffset = (int16)((int64)value - (int64)patch->programIndex);
			assembler.program.data[patch->programIndex] = relativeOffset >> 8;
			assembler.program.data[patch->programIndex + 1] = (uint8)(relativeOffset & 0xFF);
		}
		else
		{
			emu_Token const* token = patch->token;
			emu_logError(&assembler, token, "Label not found'%s'.", patch->label);
		}
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
			.irqBrkVector = 0,
			.nmiVector = 0,
			.resetVector = 0,
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
	case emu_TokenType_Symbol:
		return parseLabel(assembler, token);
	case emu_TokenType_String:
	case emu_TokenType_Comment:
		return emu_StatementError_None;
		// TODO: Add real support for control commands
	case emu_TokenType_ControlCommand:
		return assembleControlCommand(assembler, token);
	}

	return emu_StatementError_Invalid;
}

static emu_StatementError parseLabel(emu_Assembler* assembler, emu_Token const* token)
{
	if (!emu_expect(assembler, emu_TokenType_Colon))
	{
		return emu_StatementError_Invalid;
	}

	// Record the location of this label
	char* symbolString = g_memory_allocate(token->length + 1);
	g_memory_copyMem(symbolString, assembler->tokenList->sourceFile->data + token->start, token->length);
	symbolString[token->length] = '\0';
	stbds_shput(assembler->labels, symbolString, assembler->programIndex);

	return emu_StatementError_None;
}

static emu_StatementError assembleInstruction(emu_Assembler* assembler, emu_Token const* token)
{
	emu_ArgList* argList = parseArgList(assembler, token);

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
	else if (argList->mode == emu_AddressingMode_Relative)
	{
		if (emu_expectRelativeCommand(assembler, token))
		{
			if (argList->arg0.type != emu_ArgumentType_Label)
			{
				emu_logError(assembler, argList->arg0.token, "Command needs a label to jump to.");
			}
			else
			{
				emu_emitRelativeOpcode(assembler, token);
				// Record the location of this patch
				emu_Token const* patchToken = argList->arg0.token;
				char* symbolString = g_memory_allocate(patchToken->length + 1);
				g_memory_copyMem(symbolString, assembler->tokenList->sourceFile->data + patchToken->start, patchToken->length);
				symbolString[patchToken->length] = '\0';
				emu_PatchLocation patch = {
					.label = symbolString,
					.programIndex = assembler->programIndex,
					.token = patchToken
				};
				// Increment 2 bytes to save room for the patched location
				assembler->programIndex += 2;
				stbds_arrput(assembler->patches, patch);
			}
		}
		else
		{
			freeArgList(argList);
			return emu_StatementError_Invalid;
		}
	}
	else if (argList->mode == emu_AddressingMode_Absolute)
	{
		if (emu_expectAbsoluteCommand(assembler, token))
		{
			if (argList->arg0.type == emu_ArgumentType_Label && argList->numArgs == 1)
			{
				emu_emitAbsoluteOpcode(assembler, token);
				// Record the location of this patch
				emu_Token const* patchToken = argList->arg0.token;
				char* symbolString = g_memory_allocate(patchToken->length + 1);
				g_memory_copyMem(symbolString, assembler->tokenList->sourceFile->data + patchToken->start, patchToken->length);
				symbolString[patchToken->length] = '\0';
				emu_PatchLocation patch = {
					.label = symbolString,
					.programIndex = assembler->programIndex,
					.token = patchToken
				};
				// Increment 2 bytes to save room for the patched location
				assembler->programIndex += 2;
				stbds_arrput(assembler->patches, patch);
			}
			else if (argList->arg0.type == emu_ArgumentType_Label && argList->numArgs == 2 && argList->arg1.type == emu_ArgumentType_X)
			{
				emu_emitAbsoluteXOpcode(assembler, token);
				// Record the location of this patch
				emu_Token const* patchToken = argList->arg0.token;
				char* symbolString = g_memory_allocate(patchToken->length + 1);
				g_memory_copyMem(symbolString, assembler->tokenList->sourceFile->data + patchToken->start, patchToken->length);
				symbolString[patchToken->length] = '\0';
				emu_PatchLocation patch = {
					.label = symbolString,
					.programIndex = assembler->programIndex,
					.token = patchToken
				};
				// Increment 2 bytes to save room for the patched location
				assembler->programIndex += 2;
				stbds_arrput(assembler->patches, patch);
			}
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

static emu_StatementError assembleControlCommand(emu_Assembler* assembler, emu_Token const* token)
{
	switch (token->data.controlCommand)
	{
	case emu_ControlCommand_Export:
	case emu_ControlCommand_Proc:
		// TODO: Add proper support here
		emu_expect(assembler, emu_TokenType_Symbol);
		return emu_StatementError_None;
	case emu_ControlCommand_Segment:
		// TODO: Add proper support here
		emu_expect(assembler, emu_TokenType_String);
		return emu_StatementError_None;
	case emu_ControlCommand_Byte:
		return emu_parseAndEmitByteList(assembler);
	}

	return emu_StatementError_Invalid;
}

static emu_ArgList* parseArgList(emu_Assembler* assembler, emu_Token const* token)
{
	emu_ArgList* argList = g_memory_allocate(sizeof(emu_ArgList));
	*argList = (emu_ArgList){ 0 };

	// Assume no arguments will be provided
	argList->mode = emu_AddressingMode_Implicit;
	argList->arg0.type = emu_ArgumentType_Byte;
	argList->arg1.type = emu_ArgumentType_Byte;

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
		argList->arg0.type = emu_ArgumentType_Word;
		argList->mode = emu_AddressingMode_Absolute;
	}
	else if (argList->arg0.token->type == emu_TokenType_Symbol)
	{
		// Handle jumping to a symbol
		if (emu_expectRelativeCommandWithError(assembler, token, false))
		{
			argList->mode = emu_AddressingMode_Relative;
			argList->arg0.type = emu_ArgumentType_Label;
		}
		// Handle using a symbol as an address
		else if (emu_expectAbsoluteCommandWithError(assembler, token, false))
		{
			argList->mode = emu_AddressingMode_Absolute;
			argList->arg0.type = emu_ArgumentType_Label;
		}
		else
		{
			g_logger_error("Unexpected symbol after instruction '%s'. Not sure what to do here.", emu_Keywords[token->data.keyword]);
		}
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

	argList->arg1.token = getNext(assembler);
	if (argList->arg1.token->type == emu_TokenType_ByteConstant)
	{
		argList->arg1.as.byte = argList->arg1.token->data.byteConstant;
	}
	else if (argList->arg1.token->type == emu_TokenType_TwoByteConstant)
	{
		argList->arg1.as.word = argList->arg1.token->data.twoByteConstant;
		argList->arg1.type = emu_ArgumentType_Word;
		argList->mode = emu_AddressingMode_Absolute;
	}
	else if (argList->arg1.token->type == emu_TokenType_Symbol)
	{
		emu_file* file = assembler->tokenList->sourceFile;
		char symbolFirstChar = file->data[argList->arg1.token->start];
		if (symbolFirstChar == 'x' && argList->arg1.token->length == 1)
		{
			argList->arg1.type = emu_ArgumentType_X;
		}
		else if (symbolFirstChar == 'y' && argList->arg1.token->length == 1)
		{
			argList->arg1.type = emu_ArgumentType_Y;
		}
		else
		{
			argList->arg1.type = emu_ArgumentType_Label;
		}
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

static emu_StatementError emu_parseAndEmitByteList(emu_Assembler* assembler)
{
	emu_Token const* token = NULL;
	emu_TokenType expectedTypes[] = { emu_TokenType_ImmediateConstant, emu_TokenType_ByteConstant };
	do
	{
		token = emu_expectOneOf(assembler, expectedTypes, (sizeof(expectedTypes) / sizeof(emu_TokenType)));
		if (!token)
		{
			return emu_StatementError_Invalid;
		}
		emu_emitRomByte(assembler, token->data.byteConstant);

		if (peek(assembler) != emu_TokenType_Comma)
		{
			return emu_StatementError_None;
		}
		token = getNext(assembler);

	} while (token);

	return emu_StatementError_None;
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
		EMIT_IMPLICIT_OPCODE(SEC);
		EMIT_IMPLICIT_OPCODE(CLC);
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

static void emu_emitRelativeOpcode(emu_Assembler* assembler, emu_Token const* token)
{
	switch (token->data.keyword)
	{
		EMIT_RELATIVE_OPCODE(BPL);
		EMIT_RELATIVE_OPCODE(BMI);
		EMIT_RELATIVE_OPCODE(BVC);
		EMIT_RELATIVE_OPCODE(BVS);
		EMIT_RELATIVE_OPCODE(BCC);
		EMIT_RELATIVE_OPCODE(BCS);
		EMIT_RELATIVE_OPCODE(BNE);
		EMIT_RELATIVE_OPCODE(BEQ);
	}
}

static void emu_emitAbsoluteOpcode(emu_Assembler* assembler, emu_Token const* token)
{
	switch (token->data.keyword)
	{
		EMIT_ABSOLUTE_OPCODE(ORA);
		EMIT_ABSOLUTE_OPCODE(AND);
		EMIT_ABSOLUTE_OPCODE(EOR);
		EMIT_ABSOLUTE_OPCODE(ADC);
		EMIT_ABSOLUTE_OPCODE(SBC);
		EMIT_ABSOLUTE_OPCODE(CMP);
		EMIT_ABSOLUTE_OPCODE(CPX);
		EMIT_ABSOLUTE_OPCODE(CPY);
		EMIT_ABSOLUTE_OPCODE(DEC);
		EMIT_ABSOLUTE_OPCODE(INC);
		EMIT_ABSOLUTE_OPCODE(ASL);
		EMIT_ABSOLUTE_OPCODE(ROL);
		EMIT_ABSOLUTE_OPCODE(LSR);
		EMIT_ABSOLUTE_OPCODE(ROR);
		EMIT_ABSOLUTE_OPCODE(LDA);
		EMIT_ABSOLUTE_OPCODE(STA);
		EMIT_ABSOLUTE_OPCODE(LDX);
		EMIT_ABSOLUTE_OPCODE(LDY);
		EMIT_ABSOLUTE_OPCODE(STY);
		EMIT_ABSOLUTE_OPCODE(STX);
	}
}

static void emu_emitAbsoluteXOpcode(emu_Assembler* assembler, emu_Token const* token)
{
	switch (token->data.keyword)
	{
		EMIT_ABSOLUTE_X_OPCODE(ORA);
		EMIT_ABSOLUTE_X_OPCODE(AND);
		EMIT_ABSOLUTE_X_OPCODE(EOR);
		EMIT_ABSOLUTE_X_OPCODE(ADC);
		EMIT_ABSOLUTE_X_OPCODE(SBC);
		EMIT_ABSOLUTE_X_OPCODE(CMP);
		EMIT_ABSOLUTE_X_OPCODE(DEC);
		EMIT_ABSOLUTE_X_OPCODE(INC);
		EMIT_ABSOLUTE_X_OPCODE(ASL);
		EMIT_ABSOLUTE_X_OPCODE(ROL);
		EMIT_ABSOLUTE_X_OPCODE(LSR);
		EMIT_ABSOLUTE_X_OPCODE(ROR);
		EMIT_ABSOLUTE_X_OPCODE(LDA);
		EMIT_ABSOLUTE_X_OPCODE(STA);
		EMIT_ABSOLUTE_X_OPCODE(LDY);
	}
}

static void emu_emitOpcode(emu_Assembler* assembler, emu_vmInstruction opcode)
{
	emu_emitByte(assembler, opcode);
}

static void emu_emitRomByte(emu_Assembler* assembler, uint8 opcode)
{
	if (assembler->programRomIndex >= assembler->program.size)
	{
		return;
	}

	assembler->program.data[assembler->programRomIndex] = opcode;
	assembler->programRomIndex++;
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

static emu_Token const* emu_expectOneOf(emu_Assembler* assembler, emu_TokenType* expected, size_t numExpected)
{
	bool res = false;
	emu_Token const* token = getNext(assembler);

	for (size_t i = 0; i < numExpected; i++)
	{
		if (token->type == expected[i])
		{
			res = true;
			break;
		}
	}

	if (!res)
	{
		char expectedTypesMessage[1'024];
		char* expectedTypesMessageCursor = expectedTypesMessage;
		for (size_t i = 0; i < numExpected; i++)
		{
			expectedTypesMessageCursor += snprintf(expectedTypesMessageCursor, (expectedTypesMessage + sizeof(expectedTypesMessage)) - expectedTypesMessageCursor, "'%s'", emu_TokenTypes[expected[i]]);
			if (expectedTypesMessageCursor > expectedTypesMessage + sizeof(expectedTypesMessage))
			{
				break;
			}

			if (i < numExpected - 1)
			{
				expectedTypesMessageCursor += snprintf(expectedTypesMessageCursor, (expectedTypesMessage + sizeof(expectedTypesMessage)) - expectedTypesMessageCursor, " or ");
				if (expectedTypesMessageCursor > expectedTypesMessage + sizeof(expectedTypesMessage))
				{
					break;
				}
			}
		}
		emu_logError(assembler, token, "Expected token of type %s, instead got type '%s'.", expectedTypesMessage, emu_TokenTypes[token->type]);
	}

	return res ? token : NULL;
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
	case emu_Keyword_SEC:
	case emu_Keyword_CLC:
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

static bool emu_expectRelativeCommandWithError(emu_Assembler* assembler, emu_Token const* token, bool withError)
{
	switch (token->data.keyword)
	{
	case emu_Keyword_BPL:
	case emu_Keyword_BMI:
	case emu_Keyword_BVC:
	case emu_Keyword_BVS:
	case emu_Keyword_BCC:
	case emu_Keyword_BCS:
	case emu_Keyword_BNE:
	case emu_Keyword_BEQ:
		return true;
	}

	if (withError)
	{
		emu_logError(assembler, token, "Instruction '%s' does not support Relative jumping.", emu_Keywords[token->data.keyword]);
	}
	return false;
}

static bool emu_expectRelativeCommand(emu_Assembler* assembler, emu_Token const* token)
{
	return emu_expectRelativeCommandWithError(assembler, token, true);
}

static bool emu_expectAbsoluteCommandWithError(emu_Assembler* assembler, emu_Token const* token, bool withError)
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

	if (withError)
	{
		emu_logError(assembler, token, "Instruction '%s' does not support Relative jumping.", emu_Keywords[token->data.keyword]);
	}
	return false;
}

static bool emu_expectAbsoluteCommand(emu_Assembler* assembler, emu_Token const* token)
{
	return emu_expectAbsoluteCommandWithError(assembler, token, true);
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