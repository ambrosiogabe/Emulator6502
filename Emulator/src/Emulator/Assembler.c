#include "Emulator/Assembler.h"
#include "Emulator/Parser.h"
#include "Emulator/VirtualMachine.h"
#include "Emulator/MemoryMap.h"
#include "utils/FileHelper.h"
#include "utils/SafeVendor.h"

#include <stb/stb_ds.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>

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
	emu_ArgumentType_AnonymousLabel,
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
		int16 anonymousLabelJumpAmount;
	} as;
} emu_Argument;

typedef struct emu_ArgList
{
	emu_AddressingMode mode;
	emu_Argument arg0;
	emu_Argument arg1;
	size_t numArgs;
} emu_ArgList;

typedef enum emu_PatchType
{
	emu_PatchType_RelativeJump,
	emu_PatchType_AnonymousJump,
	emu_PatchType_GlobalAddress,
} emu_PatchType;

typedef struct emu_PatchLocation
{
	emu_PatchType type;
	// The memory we're writing into for this patch
	uint8* writePtr;
	// The absolute address in memory of where this patch is needed
	uint16 romAddress;
	union as
	{
		// The label we need to jump to
		char* label;
		int16 labelIndexToJumpTo;
	} as;
	// Debug info
	emu_Token const* token;
} emu_PatchLocation;

typedef struct emu_LabelData
{
	// The absolute address of this label in memory
	uint16 address;
	// The index of this label which lets us know where it falls in the order of labels
	// This is used for unnamed labels and relative branching
	uint16 index;
} emu_LabelData;

typedef struct emu_Label
{
	char* key;
	emu_LabelData value;
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

	uint8* writeIndexStart;
	uint8* writeIndex;
	size_t writeIndexSize;

	emu_Label* labels;
	emu_PatchLocation* patches;
	emu_MemoryMap const* const mmap;
} emu_Assembler;

// Internal Functions
static emu_StatementError assembleNextStatement(emu_Assembler* assembler);
static emu_StatementError assembleInstruction(emu_Assembler* assembler, emu_Token const* token);
static emu_StatementError assembleControlCommand(emu_Assembler* assembler, emu_Token const* token);
static emu_StatementError parseLabel(emu_Assembler* assembler, emu_Token const* token);
static emu_StatementError parseAnonymousLabel(emu_Assembler* assembler, emu_Token const* token);
static emu_ArgList* parseArgList(emu_Assembler* assembler, emu_Token const* token);
static void freeArgList(emu_ArgList* argList);

static emu_StatementError emu_parseAndEmitByteList(emu_Assembler* assembler);
static emu_StatementError emu_parseAndSetSegment(emu_Assembler* assembler);
static emu_StatementError emu_parseControlAddr(emu_Assembler* assembler);
static emu_StatementError emu_parseProc(emu_Assembler* assembler);
static void emu_emitImplicitOpcode(emu_Assembler* assembler, emu_Token const* token);
static void emu_emitImmediateOpcode(emu_Assembler* assembler, emu_Token const* token);
static void emu_emitZeroPageOpcode(emu_Assembler* assembler, emu_Token const* token);
static void emu_emitRelativeOpcode(emu_Assembler* assembler, emu_Token const* token);
static void emu_emitAbsoluteOpcode(emu_Assembler* assembler, emu_Token const* token);
static void emu_emitAbsoluteXOpcode(emu_Assembler* assembler, emu_Token const* token);
static void emu_emitOpcode(emu_Assembler* assembler, emu_vmInstruction opcode);
static void emu_emitWord(emu_Assembler* assembler, uint16 word);
static void emu_emitByte(emu_Assembler* assembler, uint8 byte);
static void emu_setWriteIndex(emu_Assembler* assembler, uint8* indexStart, uint8* index, size_t indexSize);
static emu_StatementError emu_addLabel(emu_Assembler* assembler, emu_Token const* token);
static emu_StatementError emu_recordPatchLocation(emu_Assembler* assembler, emu_Token const* token, emu_PatchType type, bool canBeRelative);
static emu_StatementError emu_recordAnonymousPatchLocation(emu_Assembler* assembler, int16 numberOfLabelsToJump, emu_Token const* debugToken);

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
emu_assembler_program emu_assembler_assembleProgram(emu_MemoryMap const* const mmap, const char* filename, size_t programSize)
{
	emu_TokenList tokenList = emu_parser_parseFile(filename);

	//for (size_t i = 0; i < emu_parser_tokenListLength(&tokenList); i++)
	//{
	//	emu_parser_debugPrintToken(&tokenList, i);
	//}

	emu_Assembler assembler = {
		.program = {.data = g_memory_allocate(programSize), .dataSize = programSize },
	.current = 0,
	.writeIndex = NULL,
	.writeIndexStart = NULL,
	.writeIndexSize = 0,
	.tokenList = &tokenList,
	.labels = NULL,
	.patches = NULL,
	.mmap = mmap,
	};

	// Initialize to BRK instructions
	for (size_t i = 0; i < programSize; i++)
	{
		assembler.program.data[i] = emu_vmInstruction_BRK;
	}

	// Start off with writing to Code segment
	assembler.writeIndex = assembler.program.data;
	assembler.writeIndexStart = assembler.writeIndex;
	assembler.writeIndexSize = programSize;

	emu_StatementError error = emu_StatementError_None;
	while (!error)
	{
		error = assembleNextStatement(&assembler);
	}

	// Patch all the needed patches
	for (int i = 0; i < stbds_arrlen(assembler.patches); i++)
	{
		emu_PatchLocation* patch = assembler.patches + i;

		if (patch->type == emu_PatchType_RelativeJump || patch->type == emu_PatchType_GlobalAddress)
		{
			if (stbds_shgeti(assembler.labels, patch->as.label) >= 0)
			{
				if (patch->type == emu_PatchType_RelativeJump)
				{
					emu_LabelData label = stbds_shget(assembler.labels, patch->as.label);
					uint16 labelAddress = label.address;
					int16 relativeOffset = (int16)((int32)labelAddress - (int32)patch->romAddress);
					patch->writePtr[0] = (uint8)(relativeOffset & 0xFF);
					patch->writePtr[1] = (uint8)(relativeOffset >> 8);
				}
				else if (patch->type == emu_PatchType_GlobalAddress)
				{
					emu_LabelData label = stbds_shget(assembler.labels, patch->as.label);
					uint16 labelAddress = label.address;
					patch->writePtr[0] = (uint8)(labelAddress & 0xFF);
					patch->writePtr[1] = (uint8)(labelAddress >> 8);
				}
			}
			else
			{
				emu_Token const* token = patch->token;
				emu_logError(&assembler, token, "Label not found '%s'.", patch->as.label);
			}
		}
		else if (patch->type == emu_PatchType_AnonymousJump)
		{
			if (patch->as.labelIndexToJumpTo >= (int16)stbds_shlen(assembler.labels))
			{
				emu_Token const* token = patch->token;
				emu_logError(&assembler, token, "Anonymous label not found.");
			}
			else
			{
				emu_LabelData label = (assembler.labels + patch->as.labelIndexToJumpTo)->value;
				g_logger_assert(label.index == patch->as.labelIndexToJumpTo, "Somehow recorded incorrect label index to jump to.");
				uint16 labelAddress = label.address;
				int16 relativeOffset = (int16)((int32)labelAddress - (int32)patch->romAddress);
				patch->writePtr[0] = (uint8)(relativeOffset & 0xFF);
				patch->writePtr[1] = (uint8)(relativeOffset >> 8);
			}
		}
		else
		{
			g_logger_error("Unable to handle patching this type of label %d", patch->type);
		}
	}

	emu_parser_freeTokenList(&tokenList);

	for (int i = 0; i < stbds_arrlen(assembler.patches); i++)
	{
		emu_PatchLocation* patch = assembler.patches + i;
		if (patch->type == emu_PatchType_RelativeJump || patch->type == emu_PatchType_GlobalAddress)
		{
			g_memory_free(patch->as.label);
		}

	}

	for (int i = 0; i < stbds_shlen(assembler.labels); i++)
	{
		g_memory_free(assembler.labels[i].key);
	}

	stbds_arrfree(assembler.patches);
	stbds_shfree(assembler.labels);

	size_t romvSize = emu_mmap_getSize(assembler.mmap->as.nes.romv);
	size_t headerSize = emu_mmap_getSize(assembler.mmap->as.nes.header);
	size_t romSize = emu_mmap_getSize(assembler.mmap->as.nes.rom);

	return (emu_assembler_program)
	{
		.data = assembler.program.data,
			.dataSize = headerSize + romvSize + romSize,
			.header = assembler.program.data,
			.headerSize = headerSize,
			.rom = assembler.program.data + headerSize,
			.romSize = romSize,
			.romv = assembler.program.data + headerSize + romSize,
			.romvSize = romvSize,
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
	case emu_TokenType_Colon:
		return parseAnonymousLabel(assembler, token);
	case emu_TokenType_String:
	case emu_TokenType_Comment:
		return emu_StatementError_None;
	case emu_TokenType_ControlCommand:
		return assembleControlCommand(assembler, token);
	default:
		g_logger_error("No support for token type: '%s'", emu_TokenTypes[token->type]);
	}

	return emu_StatementError_Invalid;
}

static emu_StatementError parseLabel(emu_Assembler* assembler, emu_Token const* token)
{
	if (!emu_expect(assembler, emu_TokenType_Colon))
	{
		return emu_StatementError_Invalid;
	}

	return emu_addLabel(assembler, token);
}

static emu_StatementError parseAnonymousLabel(emu_Assembler* assembler, emu_Token const* token)
{
	if (token->type != emu_TokenType_Colon)
	{
		emu_logError(assembler, token, "Expected ':'. Instead got '%s'", emu_TokenTypes[token->type]);
		return emu_StatementError_Invalid;
	}

	// Create fake name for label. We'll use the size of labels to make sure it's unique
	char scratch[1'024];
	int length = snprintf(scratch, sizeof(scratch), "##Anonymous##%d", (int16)stbds_shlen(assembler->labels));
	g_logger_assert(length < 1'024 - 1, "Could not write buffer");
	scratch[length] = '\0';

	// Make sure to allocate memory specifically for this label to own
	char* label = g_memory_allocate(length + 1);
	g_memory_copyMem(label, scratch, length + 1);

	// Record location of label
	uint16 prgAddress = (uint16)(assembler->writeIndex - assembler->writeIndexStart);
	uint16 labelIndex = (uint16)(stbds_shlen(assembler->labels));
	stbds_shput(assembler->labels, label, ((emu_LabelData){
		.address = prgAddress + assembler->mmap->as.nes.rom.start,
			.index = labelIndex
	}));

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
			if (argList->arg0.type == emu_ArgumentType_Label)
			{
				emu_emitRelativeOpcode(assembler, token);
				emu_recordPatchLocation(assembler, argList->arg0.token, emu_PatchType_RelativeJump, true);
			}
			else if (argList->arg0.type == emu_ArgumentType_AnonymousLabel)
			{
				emu_emitRelativeOpcode(assembler, token);
				emu_recordAnonymousPatchLocation(assembler, argList->arg0.as.anonymousLabelJumpAmount, argList->arg0.token);
			}
			else
			{
				emu_logError(assembler, argList->arg0.token, "Command needs a label to jump to.");
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
				emu_recordPatchLocation(assembler, argList->arg0.token, emu_PatchType_GlobalAddress, true);
			}
			else if (argList->arg0.type == emu_ArgumentType_Word && argList->numArgs == 1)
			{
				emu_emitAbsoluteOpcode(assembler, token);
				emu_emitWord(assembler, argList->arg0.as.word);
			}
			else if (argList->arg0.type == emu_ArgumentType_Label && argList->numArgs == 2 && argList->arg1.type == emu_ArgumentType_X)
			{
				emu_emitAbsoluteXOpcode(assembler, token);
				emu_recordPatchLocation(assembler, argList->arg0.token, emu_PatchType_GlobalAddress, true);
			}
			else if (argList->arg0.type == emu_ArgumentType_Word && argList->numArgs == 2 && argList->arg1.type == emu_ArgumentType_X)
			{
				emu_emitAbsoluteXOpcode(assembler, token);
				emu_emitWord(assembler, argList->arg0.as.word);
			}
			else
			{
				g_logger_error("Unable to handle absolute instruction: '%d'", argList->arg0.type);
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
		// TODO: Properly support this
		emu_expect(assembler, emu_TokenType_Symbol);
		return emu_StatementError_None;
	case emu_ControlCommand_EndProc:
		// TODO: Properly support this
		return emu_StatementError_None;
	case emu_ControlCommand_Proc:
		// TODO: Properly support this
		return emu_parseProc(assembler);
	case emu_ControlCommand_Segment:
		return emu_parseAndSetSegment(assembler);
	case emu_ControlCommand_Byte:
		return emu_parseAndEmitByteList(assembler);
	case emu_ControlCommand_Addr:
		return emu_parseControlAddr(assembler);
	default:
		g_logger_error("No support for control command: '%s'", emu_ControlCommands[token->data.controlCommand]);
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
	argList->numArgs = 0;

	if (!isArgStart(assembler))
	{
		return argList;
	}

	argList->arg0.token = getNext(assembler);
	switch (argList->arg0.token->type)
	{
	case emu_TokenType_ByteConstant:
	{
		argList->arg0.as.byte = argList->arg0.token->data.byteConstant;
		argList->mode = emu_AddressingMode_ZeroPage;
	}
	break;
	case emu_TokenType_ImmediateConstant:
	{
		argList->arg0.as.byte = argList->arg0.token->data.byteConstant;
		argList->mode = emu_AddressingMode_Immediate;
	}
	break;
	case emu_TokenType_TwoByteConstant:
	{
		argList->arg0.as.word = argList->arg0.token->data.twoByteConstant;
		argList->arg0.type = emu_ArgumentType_Word;
		argList->mode = emu_AddressingMode_Absolute;
	}
	break;
	case emu_TokenType_Symbol:
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
	break;
	case emu_TokenType_Colon:
	{
		// Parse the indicators telling us which relative label to jump to. You could use an indicator like the following:
		//   :+ ; Jump one label forward
		//   :- ; Jump one label back
		//   :< ; Jump one label back
		//   :> ; Jump one label forward
		// And you can stack any of those indicators to indicate a jump of more than one label like:
		//   :++ ; Jump two labels ahead
		emu_Token const* indicator = getNext(assembler);
		int16 numberOfLabelsToJump = 1;
		while (peek(assembler) == indicator->type)
		{
			getNext(assembler);
			numberOfLabelsToJump++;
		}

		if (indicator->type == emu_TokenType_LeftAngleBracket || indicator->type == emu_TokenType_Minus)
		{
			numberOfLabelsToJump *= -1;
			// If we go backwards we need to add one to the number of labels to jump. That's because technically
			// the first label behind us would be the last label index we've seen. In other words, we want to jump
			// (currentLabelIndex + 0) instead of going -1.
			numberOfLabelsToJump++;
		}

		// Handle jumping to a symbol
		if (emu_expectRelativeCommandWithError(assembler, token, true))
		{
			argList->mode = emu_AddressingMode_Relative;
			argList->arg0.type = emu_ArgumentType_AnonymousLabel;
			argList->arg0.as.anonymousLabelJumpAmount = numberOfLabelsToJump;
		}
	}
	break;
	default:
		g_logger_error("Unexpected token type: %s", emu_TokenTypes[argList->arg0.token->type]);
		break;
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
	switch (argList->arg1.token->type)
	{
	case emu_TokenType_ByteConstant:
	{
		argList->arg1.as.byte = argList->arg1.token->data.byteConstant;
	}
	break;
	case emu_TokenType_TwoByteConstant:
	{
		argList->arg1.as.word = argList->arg1.token->data.twoByteConstant;
		argList->arg1.type = emu_ArgumentType_Word;
		argList->mode = emu_AddressingMode_Absolute;
	}
	break;
	case emu_TokenType_Symbol:
	{
		emu_file* file = assembler->tokenList->sourceFile;
		char symbolFirstChar = file->data[argList->arg1.token->start];
		if (toupper(symbolFirstChar) == 'X' && argList->arg1.token->length == 1)
		{
			argList->arg1.type = emu_ArgumentType_X;
		}
		else if (toupper(symbolFirstChar) == 'Y' && argList->arg1.token->length == 1)
		{
			argList->arg1.type = emu_ArgumentType_Y;
		}
		else
		{
			argList->arg1.type = emu_ArgumentType_Label;
		}
	}
	break;
	default:
		g_logger_error("Unexpected token type: %s", emu_TokenTypes[argList->arg1.token->type]);
		break;
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

static emu_StatementError emu_parseAndSetSegment(emu_Assembler* assembler)
{
	emu_Token const* segmentName = getNext(assembler);
	if (segmentName->type != emu_TokenType_String)
	{
		emu_logError(assembler, segmentName, "Expected string to define segment. Instead got '%s'", emu_TokenTypes[segmentName->type]);
		return emu_StatementError_Invalid;
	}

	char* segmentNameStr = g_memory_allocate(segmentName->length + 1);
	g_memory_copyMem(segmentNameStr, assembler->tokenList->sourceFile->data + segmentName->start, segmentName->length);
	segmentNameStr[segmentName->length] = '\0';

	if (strcmp("\"HEADER\"", segmentNameStr) == 0)
	{
		// Set our write index to header segment
		emu_AddressRange headerRange = assembler->mmap->as.nes.header;
		uint16 headerOffset = headerRange.start;
		g_logger_assert(headerOffset + emu_mmap_getSize(headerRange) <= assembler->program.dataSize, "Overflow");
		emu_setWriteIndex(
			assembler,
			assembler->program.data + headerOffset,
			assembler->program.data + headerOffset,
			emu_mmap_getSize(headerRange)
		);
	}
	else if (strcmp("\"VECTORS\"", segmentNameStr) == 0)
	{
		// Set our write index to vector segment
		// Vector segment starts after Code + Header segments
		emu_AddressRange headerRange = assembler->mmap->as.nes.header;
		emu_AddressRange romRange = assembler->mmap->as.nes.rom;
		emu_AddressRange vectorRange = assembler->mmap->as.nes.romv;
		uint16 vectorOffset = headerRange.start + (uint16)(emu_mmap_getSize(headerRange) + emu_mmap_getSize(romRange));
		g_logger_assert(vectorOffset + emu_mmap_getSize(vectorRange) <= assembler->program.dataSize, "Overflow");
		emu_setWriteIndex(
			assembler,
			assembler->program.data + vectorOffset,
			assembler->program.data + vectorOffset,
			emu_mmap_getSize(vectorRange)
		);
	}
	else if (strcmp("\"CODE\"", segmentNameStr) == 0)
	{
		// Set our write index to code segment
		// Code segment starts right after Header segment
		emu_AddressRange headerRange = assembler->mmap->as.nes.header;
		emu_AddressRange romRange = assembler->mmap->as.nes.rom;
		uint16 romOffset = headerRange.start + (uint16)emu_mmap_getSize(headerRange);
		emu_setWriteIndex(
			assembler,
			assembler->program.data + romOffset,
			assembler->program.data + romOffset,
			emu_mmap_getSize(romRange)
		);
	}
	else
	{
		emu_logError(assembler, segmentName, "Unknown segment '%s'.", segmentNameStr);
		g_memory_free(segmentNameStr);
		return emu_StatementError_Invalid;
	}

	g_memory_free(segmentNameStr);
	return emu_StatementError_None;
}

static emu_StatementError emu_parseControlAddr(emu_Assembler* assembler)
{
	emu_Token const* addrToken = getNext(assembler);
	if (addrToken->type == emu_TokenType_ImmediateConstant)
	{
		// TODO: Make sure we can parse two byte constants and emit that here instead of a single byte
		emu_emitByte(assembler, addrToken->data.byteConstant);
		emu_emitByte(assembler, 0);
		return emu_StatementError_None;
	}

	if (addrToken->type != emu_TokenType_Symbol)
	{
		emu_logError(assembler, addrToken, "Expected symbol or immediate constant after .addr command. Instead got '%s'", emu_TokenTypes[addrToken->type]);
		return emu_StatementError_Invalid;
	}

	return emu_recordPatchLocation(assembler, addrToken, emu_PatchType_GlobalAddress, false);
}

static emu_StatementError emu_parseProc(emu_Assembler* assembler)
{
	emu_Token const* addrToken = getNext(assembler);
	if (addrToken->type != emu_TokenType_Symbol)
	{
		emu_logError(assembler, addrToken, "Expected symbol constant after .proc command. Instead got '%s'", emu_TokenTypes[addrToken->type]);
		return emu_StatementError_Invalid;
	}

	return emu_addLabel(assembler, addrToken);
}

static emu_StatementError emu_parseAndEmitByteList(emu_Assembler* assembler)
{
	emu_Token const* token = NULL;
	emu_TokenType expectedTypes[] = { emu_TokenType_ImmediateConstant, emu_TokenType_ByteConstant, emu_TokenType_Character };
	do
	{
		token = emu_expectOneOf(assembler, expectedTypes, (sizeof(expectedTypes) / sizeof(emu_TokenType)));
		if (!token)
		{
			return emu_StatementError_Invalid;
		}

		emu_emitByte(assembler, token->data.byteConstant);

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
	default:
		g_logger_error("Cannot emit implicit opcode for instruction: '%s'", emu_Keywords[token->data.keyword]);
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
	default:
		g_logger_error("Cannot emit immediate opcode for instruction: '%s'", emu_Keywords[token->data.keyword]);
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
	default:
		g_logger_error("Cannot emit zero page opcode for instruction: '%s'", emu_Keywords[token->data.keyword]);
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
	default:
		g_logger_error("Cannot emit relative opcode for instruction: '%s'", emu_Keywords[token->data.keyword]);
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
		EMIT_ABSOLUTE_OPCODE(JSR);
	default:
		g_logger_error("Cannot emit absolute opcode for instruction: '%s'", emu_Keywords[token->data.keyword]);
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
	default:
		g_logger_error("Cannot emit absoluteX opcode for instruction: '%s'", emu_Keywords[token->data.keyword]);
	}
}

static void emu_emitOpcode(emu_Assembler* assembler, emu_vmInstruction opcode)
{
	emu_emitByte(assembler, opcode);
}

static void emu_emitWord(emu_Assembler* assembler, uint16 word)
{
	emu_emitByte(assembler, word & 0xFF);
	emu_emitByte(assembler, (word >> 8) & 0xFF);
}

static void emu_emitByte(emu_Assembler* assembler, uint8 opcode)
{
	if ((size_t)(assembler->writeIndex - assembler->writeIndexStart) > assembler->writeIndexSize)
	{
		g_logger_error("Cannot write any more bytes. Ran out of memory.");
		return;
	}

	*assembler->writeIndex = opcode;
	assembler->writeIndex++;
}

static void emu_setWriteIndex(emu_Assembler* assembler, uint8* indexStart, uint8* index, size_t indexSize)
{
	g_logger_assert(indexStart <= index, "Invalid index start.");
	assembler->writeIndex = index;
	assembler->writeIndexSize = indexSize;
	assembler->writeIndexStart = indexStart;
}

static emu_StatementError emu_addLabel(emu_Assembler* assembler, emu_Token const* token)
{
	if (token->type != emu_TokenType_Symbol)
	{
		emu_logError(assembler, token, "Expected symbol. Instead got '%s'", emu_TokenTypes[token->type]);
		return emu_StatementError_Invalid;
	}

	char* label = g_memory_allocate(token->length + 1);
	g_memory_copyMem(label, assembler->tokenList->sourceFile->data + token->start, token->length);
	label[token->length] = '\0';

	// Record location of label
	uint16 prgAddress = (uint16)(assembler->writeIndex - assembler->writeIndexStart);
	uint16 labelIndex = (uint16)(stbds_shlen(assembler->labels));
	stbds_shput(assembler->labels, label, ((emu_LabelData){
		.address = prgAddress + assembler->mmap->as.nes.rom.start,
			.index = labelIndex
	}));

	return emu_StatementError_None;
}

static emu_StatementError emu_recordPatchLocation(emu_Assembler* assembler, emu_Token const* token, emu_PatchType type, bool canBeRelative)
{
	if (token->type != emu_TokenType_Symbol)
	{
		emu_logError(assembler, token, "Expected symbol. Instead got '%s'", emu_TokenTypes[token->type]);
		return emu_StatementError_Invalid;
	}

	char* label = g_memory_allocate(token->length + 1);
	g_memory_copyMem(label, assembler->tokenList->sourceFile->data + token->start, token->length);
	label[token->length] = '\0';

	uint16 prgAddress = (uint16)(assembler->writeIndex - assembler->writeIndexStart);
	emu_PatchLocation patch = {
		.as = {
			.label = label,
		},
		.writePtr = assembler->writeIndex,
		.romAddress = prgAddress + assembler->mmap->as.nes.rom.start,
		.token = token,
		.type = type,
	};

	// Increment 2 bytes to save room for the patched location
	assembler->writeIndex += 2;
	stbds_arrput(assembler->patches, patch);

	return emu_StatementError_None;
}

static emu_StatementError emu_recordAnonymousPatchLocation(emu_Assembler* assembler, int16 numberOfLabelsToJump, emu_Token const* debugToken)
{
	if (debugToken->type != emu_TokenType_Colon)
	{
		emu_logError(assembler, debugToken, "Expected a colon followed by a series of one of: '+', '-', '>', or '<'. Instead got '%s'", emu_TokenTypes[debugToken->type]);
		return emu_StatementError_Invalid;
	}

	int16 currentLabelIndex = (int16)(stbds_shlen(assembler->labels) - 1);
	int16 labelIndexToJumpTo = currentLabelIndex + numberOfLabelsToJump;
	uint16 prgAddress = (uint16)(assembler->writeIndex - assembler->writeIndexStart);
	emu_PatchLocation patch = {
		.as = {
			.labelIndexToJumpTo = labelIndexToJumpTo,
		},
		.writePtr = assembler->writeIndex,
		.romAddress = prgAddress + assembler->mmap->as.nes.rom.start,
		.token = debugToken,
		.type = emu_PatchType_AnonymousJump,
	};

	// Increment 2 bytes to save room for the patched location
	assembler->writeIndex += 2;
	stbds_arrput(assembler->patches, patch);

	return emu_StatementError_None;
}

static bool isArgStart(emu_Assembler* assembler)
{
	emu_TokenType type = peek(assembler);
	emu_TokenType nextType = peekMulti(assembler, 1);
	return type == emu_TokenType_ByteConstant
		|| type == emu_TokenType_ImmediateConstant
		|| type == emu_TokenType_TwoByteConstant
		|| (type == emu_TokenType_Symbol && nextType != emu_TokenType_Symbol)
		|| (type == emu_TokenType_Colon &&
			(nextType == emu_TokenType_Plus
				|| nextType == emu_TokenType_Minus
				|| nextType == emu_TokenType_LeftAngleBracket
				|| nextType == emu_TokenType_RightAngleBracket)
			);
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
	case emu_Keyword_JSR:
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