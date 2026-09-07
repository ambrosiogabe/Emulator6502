#include "Emulator/VirtualMachine.h"
#include "Emulator/Assembler.h"

#include <stdio.h>
#include <string.h>

static const uint16 nmiVectorAddress = 0xFFFA;
static const uint16 resetVectorAddress = 0xFFFC;
static const uint16 irqBrkVectorAddress = 0xFFFE;

// --------------- Internal Structures --------------- 
typedef struct VmInstruction
{
	emu_vmInstruction type;
} VmInstruction;

// --------------- Internal Functions ---------------
// Fetch and decode next instruction
static emu_vmInstruction fetchInstruction(emu_virtualMachine* vm);
static void executeInstruction(emu_virtualMachine* vm, emu_vmInstruction instruction);
static uint8 getNext(emu_virtualMachine* vm);
static uint8 getRegisterValue(emu_virtualMachine* vm, emu_vmInstruction instruction);
static void setRegisterValue(emu_virtualMachine* vm, emu_vmInstruction instruction, uint8 value);
static void setAbsRegisterValue(emu_virtualMachine* vm, emu_vmInstruction instruction, uint8* baseAddress);
static void storeRamValue(emu_virtualMachine* vm, emu_vmInstruction instruction, uint8 address);
static void addWithCarry(emu_virtualMachine* vm, emu_vmInstruction, uint8 value);
static void subtractWithCarry(emu_virtualMachine* vm, emu_vmInstruction, uint8 value);
static void compare(emu_virtualMachine* vm, emu_vmInstruction, uint8 value);
static void decrement(emu_virtualMachine* vm, emu_vmInstruction, uint8 address);
static void increment(emu_virtualMachine* vm, emu_vmInstruction, uint8 address);
static void logicalOr(emu_virtualMachine* vm, emu_vmInstruction, uint8 value);
static void logicalAnd(emu_virtualMachine* vm, emu_vmInstruction, uint8 value);
static void logicalXor(emu_virtualMachine* vm, emu_vmInstruction, uint8 value);
static void arithmeticShiftLeft(emu_virtualMachine* vm, emu_vmInstruction, uint8 address);
static void rotateLeft(emu_virtualMachine* vm, emu_vmInstruction, uint8 address);
static void logicalShiftRight(emu_virtualMachine* vm, emu_vmInstruction, uint8 address);
static void rotateRight(emu_virtualMachine* vm, emu_vmInstruction, uint8 address);

static void checkFlagStatuses(emu_virtualMachine* vm, uint8 flagsToCheck, uint8 value);
static void checkOverflowFlag(emu_virtualMachine* vm, int16 value);

#define INSTRUCTION_EXPANSION(caseName, function) \
case caseName:\
{\
  uint8 byte = getNext(vm);\
  function(vm, instruction, byte);\
}\
break

#define INSTRUCTION_EXPANSION_RAM(caseName, function) \
case caseName:\
{\
  uint8 address = getNext(vm);\
  function(vm, instruction, emu_mmap_getNesAddress(&vm->mmap, address)[0]);\
}\
break

#define INSTRUCTION_EXPANSION_LONG_RAM(caseName, function) \
case caseName:\
{\
  uint8 lo = getNext(vm);\
  uint8 hi = getNext(vm);\
  uint16 globalAddress = (hi << 8) | lo;\
  function(vm, instruction, emu_mmap_getNesAddress(&vm->mmap, globalAddress));\
}\
break

const char* emu_vmInstructions[EMU_MAX_INSTRUCTION_OPCODE] = { 0 };

void emu_vm_initDebug()
{
	for (size_t i = 0; i < EMU_MAX_INSTRUCTION_OPCODE; i++)
	{
		emu_vmInstructions[i] = "NULL";
	}

	emu_vmInstructions[emu_vmInstruction_BRK] = "BRK";
	emu_vmInstructions[emu_vmInstruction_CLC_IMP] = "CLC_IMP";
	emu_vmInstructions[emu_vmInstruction_SEC_IMP] = "CLC_IMP";
	emu_vmInstructions[emu_vmInstruction_RTS_IMP] = "RTS_IMP";
	// -- OR instructions --
	emu_vmInstructions[emu_vmInstruction_ORA_IMM] = "ORA_IMM";
	emu_vmInstructions[emu_vmInstruction_ORA_ZP] = "ORA_ZP";
	emu_vmInstructions[emu_vmInstruction_ORA_ZPX] = "ORA_ZPX";
	emu_vmInstructions[emu_vmInstruction_ORA_IZY] = "ORA_IZY";
	emu_vmInstructions[emu_vmInstruction_ORA_ABS] = "ORA_ABS";
	emu_vmInstructions[emu_vmInstruction_ORA_ABX] = "ORA_ABX";
	emu_vmInstructions[emu_vmInstruction_ORA_ABY] = "ORA_ABY";
	emu_vmInstructions[emu_vmInstruction_ORA_IZX] = "ORA_IZX";
	// -- XOR Instructions --
	emu_vmInstructions[emu_vmInstruction_EOR_IMM] = "EOR_IMM";
	emu_vmInstructions[emu_vmInstruction_EOR_ZP] = "EOR_ZP";
	emu_vmInstructions[emu_vmInstruction_EOR_ZPX] = "EOR_ZPX";
	emu_vmInstructions[emu_vmInstruction_EOR_IZX] = "EOR_IZX";
	emu_vmInstructions[emu_vmInstruction_EOR_IZY] = "EOR_IZY";
	emu_vmInstructions[emu_vmInstruction_EOR_ABS] = "EOR_ABS";
	emu_vmInstructions[emu_vmInstruction_EOR_ABX] = "EOR_ABX";
	emu_vmInstructions[emu_vmInstruction_EOR_ABY] = "EOR_ABY";
	// -- AND instructions --
	emu_vmInstructions[emu_vmInstruction_AND_IMM] = "IMM";
	emu_vmInstructions[emu_vmInstruction_AND_ZP] = "ZP";
	emu_vmInstructions[emu_vmInstruction_AND_ZPX] = "ZPX";
	emu_vmInstructions[emu_vmInstruction_AND_IZX] = "IZX";
	emu_vmInstructions[emu_vmInstruction_AND_IZY] = "IZY";
	emu_vmInstructions[emu_vmInstruction_AND_ABS] = "ABS";
	emu_vmInstructions[emu_vmInstruction_AND_ABX] = "ABX";
	emu_vmInstructions[emu_vmInstruction_AND_ABY] = "ABY";
	// --  ADC instructions --
	emu_vmInstructions[emu_vmInstruction_ADC_IZX] = "ADC_IZX";
	emu_vmInstructions[emu_vmInstruction_ADC_ZP] = "ADC_ZP";
	emu_vmInstructions[emu_vmInstruction_ADC_IMM] = "ADC_IMM";
	emu_vmInstructions[emu_vmInstruction_ADC_ABS] = "ADC_ABS";
	emu_vmInstructions[emu_vmInstruction_ADC_IZY] = "ADC_IZY";
	emu_vmInstructions[emu_vmInstruction_ADC_ZPX] = "ADC_ZPX";
	emu_vmInstructions[emu_vmInstruction_ADC_ABY] = "ADC_ABY";
	emu_vmInstructions[emu_vmInstruction_ADC_ABX] = "ADC_ABX";
	// -- SBC Instructions --
	emu_vmInstructions[emu_vmInstruction_SBC_IMM] = "SBC_IMM";
	emu_vmInstructions[emu_vmInstruction_SBC_ZP] = "SBC_ZP";
	emu_vmInstructions[emu_vmInstruction_SBC_ZPX] = "SBC_ZPX";
	emu_vmInstructions[emu_vmInstruction_SBC_IZX] = "SBC_IZX";
	emu_vmInstructions[emu_vmInstruction_SBC_IZY] = "SBC_IZY";
	emu_vmInstructions[emu_vmInstruction_SBC_ABS] = "SBC_ABS";
	emu_vmInstructions[emu_vmInstruction_SBC_ABX] = "SBC_ABX";
	emu_vmInstructions[emu_vmInstruction_SBC_ABY] = "SBC_ABY";
	// -- Store Instructions --
	emu_vmInstructions[emu_vmInstruction_STA_IZX] = "STA_IZX";
	emu_vmInstructions[emu_vmInstruction_STY_ZP] = "STY_ZP";
	emu_vmInstructions[emu_vmInstruction_STA_ZP] = "STA_ZP";
	emu_vmInstructions[emu_vmInstruction_STX_ZP] = "STX_ZP";
	emu_vmInstructions[emu_vmInstruction_STY_ABS] = "STY_ABS";
	emu_vmInstructions[emu_vmInstruction_STA_ABS] = "STA_ABS";
	emu_vmInstructions[emu_vmInstruction_STX_ABS] = "STX_ABS";
	emu_vmInstructions[emu_vmInstruction_STA_IZY] = "STA_IZY";
	emu_vmInstructions[emu_vmInstruction_STY_ZPX] = "STY_ZPX";
	emu_vmInstructions[emu_vmInstruction_STA_ZPX] = "STA_ZPX";
	emu_vmInstructions[emu_vmInstruction_STX_ZPY] = "STX_ZPY";
	emu_vmInstructions[emu_vmInstruction_STA_ABY] = "STA_ABY";
	emu_vmInstructions[emu_vmInstruction_STA_ABX] = "STA_ABX";
	// -- Load Instructions --
	emu_vmInstructions[emu_vmInstruction_LDY_IMM] = "LDY_IMM";
	emu_vmInstructions[emu_vmInstruction_LDA_IZX] = "LDA_IZX";
	emu_vmInstructions[emu_vmInstruction_LDX_IMM] = "LDX_IMM";
	emu_vmInstructions[emu_vmInstruction_LDY_ZP] = "LDY_ZP";
	emu_vmInstructions[emu_vmInstruction_LDA_ZP] = "LDA_ZP";
	emu_vmInstructions[emu_vmInstruction_LDX_ZP] = "LDX_ZP";
	emu_vmInstructions[emu_vmInstruction_LDA_IMM] = "LDA_IMM";
	emu_vmInstructions[emu_vmInstruction_LDY_ABS] = "LDY_ABS";
	emu_vmInstructions[emu_vmInstruction_LDA_ABS] = "LDA_ABS";
	emu_vmInstructions[emu_vmInstruction_LDX_ABS] = "LDX_ABS";
	emu_vmInstructions[emu_vmInstruction_LDA_IZY] = "LDA_IZY";
	emu_vmInstructions[emu_vmInstruction_LDY_ZPX] = "LDY_ZPX";
	emu_vmInstructions[emu_vmInstruction_LDA_ZPX] = "LDA_ZPX";
	emu_vmInstructions[emu_vmInstruction_LDX_ZPY] = "LDX_ZPY";
	emu_vmInstructions[emu_vmInstruction_LDA_ABY] = "LDA_ABY";
	emu_vmInstructions[emu_vmInstruction_LDY_ABX] = "LDY_ABX";
	emu_vmInstructions[emu_vmInstruction_LDA_ABX] = "LDA_ABX";
	emu_vmInstructions[emu_vmInstruction_LDX_ABY] = "LDX_ABY";
	// -- JMP instructions --
	emu_vmInstructions[emu_vmInstruction_JMP_IND] = "JMP_IND";
	// -- Compare instructions --
	emu_vmInstructions[emu_vmInstruction_CMP_IZX] = "CMP_IZX";
	emu_vmInstructions[emu_vmInstruction_CMP_ZP] = "CMP_ZP";
	emu_vmInstructions[emu_vmInstruction_CMP_IMM] = "CMP_IMM";
	emu_vmInstructions[emu_vmInstruction_CMP_ABS] = "CMP_ABS";
	emu_vmInstructions[emu_vmInstruction_CMP_IZY] = "CMP_IZY";
	emu_vmInstructions[emu_vmInstruction_CMP_ZPX] = "CMP_ZPX";
	emu_vmInstructions[emu_vmInstruction_CMP_ABY] = "CMP_ABY";
	emu_vmInstructions[emu_vmInstruction_CMP_ABX] = "CMP_ABX";
	// -- CPX (Compare X) Instructions --
	emu_vmInstructions[emu_vmInstruction_CPX_IMM] = "CPX_IMM";
	emu_vmInstructions[emu_vmInstruction_CPX_ZP] = "CPX_ZP";
	emu_vmInstructions[emu_vmInstruction_CPX_ABS] = "CPX_ABS";
	// -- CPY (Compare Y) Instructions --
	emu_vmInstructions[emu_vmInstruction_CPY_IMM] = "CPY_IMM";
	emu_vmInstructions[emu_vmInstruction_CPY_ZP] = "CPY_ZP";
	emu_vmInstructions[emu_vmInstruction_CPY_ABS] = "CPY_ABS";
	// -- DEC Instructions --
	emu_vmInstructions[emu_vmInstruction_DEC_ZP] = "DEC_ZP";
	emu_vmInstructions[emu_vmInstruction_DEC_ZPX] = "DEC_ZPX";
	emu_vmInstructions[emu_vmInstruction_DEC_ABS] = "DEC_ABS";
	emu_vmInstructions[emu_vmInstruction_DEC_ABX] = "DEC_ABX";
	// -- DEX/DEY (Decrement X/Y) Instructions --
	emu_vmInstructions[emu_vmInstruction_DEX_IMP] = "DEX_IMP";
	emu_vmInstructions[emu_vmInstruction_DEY_IMP] = "DEY_IMP";
	// -- INC Instructions
	emu_vmInstructions[emu_vmInstruction_INC_ZP] = "INC_ZP";
	emu_vmInstructions[emu_vmInstruction_INC_ZPX] = "INC_ZPX";
	emu_vmInstructions[emu_vmInstruction_INC_ABS] = "INC_ABS";
	emu_vmInstructions[emu_vmInstruction_INC_ABX] = "INC_ABX";
	// -- INX/INY (Increment X/Y) Instructions --
	emu_vmInstructions[emu_vmInstruction_INX_IMP] = "INX_IMP";
	emu_vmInstructions[emu_vmInstruction_INY_IMP] = "INY_IMP";
	// -- ASL (Arithmetic Shift Left) Instructions --
	emu_vmInstructions[emu_vmInstruction_ASL_IMP] = "ASL_IMP";
	emu_vmInstructions[emu_vmInstruction_ASL_ZP] = "ASL_ZP";
	emu_vmInstructions[emu_vmInstruction_ASL_ZPX] = "ASL_ZPX";
	emu_vmInstructions[emu_vmInstruction_ASL_ABS] = "ASL_ABS";
	emu_vmInstructions[emu_vmInstruction_ASL_ABX] = "ASL_ABX";
	// -- ROL (Rotate Left) Instructions --
	emu_vmInstructions[emu_vmInstruction_ROL_IMP] = "ROL_IMP";
	emu_vmInstructions[emu_vmInstruction_ROL_ZP] = "ROL_ZP";
	emu_vmInstructions[emu_vmInstruction_ROL_ZPX] = "ROL_ZPX";
	emu_vmInstructions[emu_vmInstruction_ROL_ABS] = "ROL_ABS";
	emu_vmInstructions[emu_vmInstruction_ROL_ABX] = "ROL_ABX";
	// -- LSR (Logical Shift Right) Instructions --
	emu_vmInstructions[emu_vmInstruction_LSR_IMP] = "LSR_IMP";
	emu_vmInstructions[emu_vmInstruction_LSR_ZP] = "LSR_ZP";
	emu_vmInstructions[emu_vmInstruction_LSR_ZPX] = "LSR_ZPX";
	emu_vmInstructions[emu_vmInstruction_LSR_ABS] = "LSR_ABS";
	emu_vmInstructions[emu_vmInstruction_LSR_ABX] = "LSR_ABX";
	// -- ROR (Rotate Right) Instructions --
	emu_vmInstructions[emu_vmInstruction_ROR_IMP] = "ROR_IMP";
	emu_vmInstructions[emu_vmInstruction_ROR_ZP] = "ROR_ZP";
	emu_vmInstructions[emu_vmInstruction_ROR_ZPX] = "ROR_ZPX";
	emu_vmInstructions[emu_vmInstruction_ROR_ABS] = "ROR_ABS";
	emu_vmInstructions[emu_vmInstruction_ROR_ABX] = "ROR_ABX";
	// -- Branch instructions --
	emu_vmInstructions[emu_vmInstruction_BPL_REL] = "BPL_REL";
	emu_vmInstructions[emu_vmInstruction_BMI_REL] = "BMI_REL";
	emu_vmInstructions[emu_vmInstruction_BVC_REL] = "BVC_REL";
	emu_vmInstructions[emu_vmInstruction_BVS_REL] = "BVS_REL";
	emu_vmInstructions[emu_vmInstruction_BCC_REL] = "BCC_REL";
	emu_vmInstructions[emu_vmInstruction_BCS_REL] = "BCS_REL";
	emu_vmInstructions[emu_vmInstruction_BNE_REL] = "BNE_REL";
	emu_vmInstructions[emu_vmInstruction_BEQ_REL] = "BEQ_REL";

	// NOP that we'll use as a flag
	emu_vmInstructions[emu_vmInstruction_ILLEGAL] = "ILLEGAL OPCODE";
}

void emu_vm_printOpcodes(uint8* program, size_t programSize)
{
	for (size_t i = 0; i < programSize; i++)
	{
		i++;
		g_logger_info("Opcode: %s on %X", emu_vm_instructionToString(program[i - 1]), program[i]);
	}
}

void emu_vm_printStatusFlags(emu_virtualMachine* vm)
{
	const char* tableHeader = "| N | V | B | D | 1 | Z | C |  A |  X |  Y |";
	int tableHeaderLength = (int)strlen(tableHeader);
	const char* lines = "=====================================================";
	const char* smallLines = "-----------------------------------------------------";
	printf("%.*s\n", tableHeaderLength, lines);
	printf("|               Status Flags               |\n");
	printf("%.*s\n%s\n%.*s\n", tableHeaderLength, smallLines, tableHeader, tableHeaderLength, smallLines);
	printf("| %d | %d | %d | %d | %d | %d | %d | %02x | %02x | %02x |\n%.*s\n",
		emu_vm_getStatus(vm, emu_vmStatus_Negative),
		emu_vm_getStatus(vm, emu_vmStatus_Overflow),
		emu_vm_getStatus(vm, emu_vmStatus_B),
		emu_vm_getStatus(vm, emu_vmStatus_Decimal),
		emu_vm_getStatus(vm, emu_vmStatus_1),
		emu_vm_getStatus(vm, emu_vmStatus_Zero),
		emu_vm_getStatus(vm, emu_vmStatus_Carry),
		vm->accumulatorReg,
		vm->xReg,
		vm->yReg,
		tableHeaderLength,
		lines
	);
}

void emu_vm_printRam(emu_virtualMachine* vm, uint16 address, uint16 numBytes)
{
	uint8* ramPtr = emu_vm_getAddress(vm, address);
	for (size_t i = 0; i < numBytes; i++)
	{
		printf("0x%04X: ", (uint16)(address + i));
		printf("0x%02X ", ramPtr[i]);
		printf("\n");
	}
}

uint8* emu_vm_getAddress(emu_virtualMachine* vm, uint16 address)
{
	switch (vm->vmType)
	{
	case emu_vmType_NES:
		return emu_mmap_getNesAddress(&vm->mmap, address);
	case emu_vmType_Commodore64:
		g_logger_error("No Commodore 64 support yet.");
	}

	g_logger_error("Cannot map memory for vm of type: %d", vm->vmType);
	return NULL;
}

emu_virtualMachine emu_vm_init(emu_vmType vmType)
{
	if (vmType == emu_vmType_NES)
	{
		emu_virtualMachine res = emu_vm_sizedInit(UINT16_MAX + 1, vmType);
		g_logger_info("Initialized NES Virtual Machine.");
		return res;
	}
	else if (vmType == emu_vmType_Commodore64)
	{
		g_logger_error("No support for Commodore 64.");
	}
	else
	{
		g_logger_error("No support for vmType::None.");
	}

	emu_virtualMachine dummy = { 0 };
	return dummy;
}

emu_virtualMachine emu_vm_sizedInit(size_t physicalMemorySize, emu_vmType vmType)
{
	if (physicalMemorySize > (UINT16_MAX + 1))
	{
		g_logger_error("You can only initialize a 6502 with a maximum of (UINT16_MAX + 1) bytes of memory.");
		return (emu_virtualMachine) { 0 };
	}

	emu_MemoryMap memoryMap = emu_mmap_newNesMap(physicalMemorySize);

	emu_virtualMachine vm = {
		.vmType = vmType,
		.programCounter = 0,
		.accumulatorReg = 0,
		.xReg = 0,
		.yReg = 0,
		.statusReg = 0,
		.stackPointer = 0,

		.mmap = memoryMap,
	};
	return vm;
}

emu_vmError emu_vm_loadProgram(emu_virtualMachine* vm, emu_assembler_program* program)
{
	// Check assertions
	if (vm == NULL)
	{
		return emu_vmError_NullVm;
	}

	if (emu_mmap_getSize(vm->mmap.as.nes.rom) < program->size)
	{
		g_logger_error("Not enough room in program for rom: %u > %u", emu_mmap_getSize(vm->mmap.as.nes.rom), program->size);
		return emu_vmError_NotEnoughROM;
	}

	// Load the program into ROM
	g_memory_copyMem(vm->mmap.as.nes.romPtr, program->data, program->size);

	// Set all instructions after end of program to illegal opcodes
	for (size_t i = program->size; i < emu_mmap_getSize(vm->mmap.as.nes.rom); i++)
	{
		vm->mmap.as.nes.romPtr[i] = emu_vmInstruction_ILLEGAL;
	}

	// Set the special vectors at the end of rom
	uint8* nmiVector = emu_mmap_getNesAddress(&vm->mmap, nmiVectorAddress);
	uint8* resetVector = emu_mmap_getNesAddress(&vm->mmap, resetVectorAddress);
	uint8* irqBrkVector = emu_mmap_getNesAddress(&vm->mmap, irqBrkVectorAddress);

	nmiVector[0] = (program->nmiVector & 0xFF);
	nmiVector[1] = ((program->nmiVector >> 8) & 0xFF);

	resetVector[0] = (program->resetVector & 0xFF);
	resetVector[1] = ((program->resetVector >> 8) & 0xFF);

	irqBrkVector[0] = (program->irqBrkVector & 0xFF);
	irqBrkVector[1] = ((program->irqBrkVector >> 8) & 0xFF);

	return emu_vmError_None;
}

emu_vmError emu_vm_resetMachine(emu_virtualMachine* vm)
{
	// Check assertions
	if (vm == NULL)
	{
		return emu_vmError_NullVm;
	}

	g_logger_assert(vm->mmap.physicalMemory != NULL, "Null VirtualMachine Memory.");

	vm->programCounter = 0;
	vm->accumulatorReg = 0;
	vm->xReg = 0;
	vm->yReg = 0;
	vm->stackPointer = 0;
	vm->statusReg = 0;

	// NOTE: We don't do anything with ROM here because on reset, only ram should be cleared.
	g_memory_zeroMem(vm->mmap.as.nes.ramPtr, emu_mmap_getSize(vm->mmap.as.nes.ram));

	// Read the reset vector into our program counter so we know where to start executing code.
	uint8* resetVector = emu_mmap_getNesAddress(&vm->mmap, resetVectorAddress);
	vm->programCounter = resetVector[0];
	vm->programCounter |= (resetVector[1] << 8);

	return emu_vmError_None;
}

emu_vmError emu_vm_tick(emu_virtualMachine* vm)
{
	emu_vmInstruction instruction = fetchInstruction(vm);
	if (instruction == emu_vmInstruction_ILLEGAL)
	{
		return emu_vmError_IllegalOpcode;
	}
	else if (instruction == emu_vmInstruction_BRK)
	{
		return emu_vmError_Break;
	}
	executeInstruction(vm, instruction);

	return emu_vmError_None;
}

void emu_vm_free(emu_virtualMachine* vm)
{
	if (vm)
	{
		emu_mmap_free(&vm->mmap);
	}

	g_memory_zeroMem(vm, sizeof(emu_virtualMachine));
}

const char* emu_vm_instructionToString(emu_vmInstruction instruction)
{
	return emu_vmInstructions[instruction];
}

uint8 emu_vm_getStatus(emu_virtualMachine* vm, emu_vmStatus status)
{
	return (vm->statusReg & status) & 0xFF;
}

void emu_vm_setStatus(emu_virtualMachine* vm, emu_vmStatus status)
{
	vm->statusReg = vm->statusReg | (uint8)status;
}

void emu_vm_clearStatus(emu_virtualMachine* vm, emu_vmStatus status)
{
	vm->statusReg = vm->statusReg & ~status;
}

// --------------- Internal Functions ---------------
static emu_vmInstruction fetchInstruction(emu_virtualMachine* vm)
{
	return (emu_vmInstruction)getNext(vm);
}

static void executeInstruction(emu_virtualMachine* vm, emu_vmInstruction instruction)
{
	switch (instruction)
	{
		// Store
		INSTRUCTION_EXPANSION(emu_vmInstruction_STA_ZP, storeRamValue);
		INSTRUCTION_EXPANSION(emu_vmInstruction_STX_ZP, storeRamValue);
		INSTRUCTION_EXPANSION(emu_vmInstruction_STY_ZP, storeRamValue);
		// Load
		INSTRUCTION_EXPANSION_RAM(emu_vmInstruction_LDA_ZP, setRegisterValue);
		INSTRUCTION_EXPANSION_RAM(emu_vmInstruction_LDX_ZP, setRegisterValue);
		INSTRUCTION_EXPANSION_RAM(emu_vmInstruction_LDY_ZP, setRegisterValue);
		INSTRUCTION_EXPANSION(emu_vmInstruction_LDA_IMM, setRegisterValue);
		INSTRUCTION_EXPANSION(emu_vmInstruction_LDX_IMM, setRegisterValue);
		INSTRUCTION_EXPANSION(emu_vmInstruction_LDY_IMM, setRegisterValue);
		// Load absolute
		INSTRUCTION_EXPANSION_LONG_RAM(emu_vmInstruction_LDA_ABX, setAbsRegisterValue);
		// Add with carry
		INSTRUCTION_EXPANSION_RAM(emu_vmInstruction_ADC_ZP, addWithCarry);
		INSTRUCTION_EXPANSION(emu_vmInstruction_ADC_IMM, addWithCarry);
		// Sub with carry
		INSTRUCTION_EXPANSION_RAM(emu_vmInstruction_SBC_ZP, subtractWithCarry);
		INSTRUCTION_EXPANSION(emu_vmInstruction_SBC_IMM, subtractWithCarry);
		// Compare
		INSTRUCTION_EXPANSION_RAM(emu_vmInstruction_CMP_ZP, compare);
		INSTRUCTION_EXPANSION(emu_vmInstruction_CMP_IMM, compare);
		INSTRUCTION_EXPANSION_RAM(emu_vmInstruction_CPX_ZP, compare);
		INSTRUCTION_EXPANSION(emu_vmInstruction_CPX_IMM, compare);
		INSTRUCTION_EXPANSION_RAM(emu_vmInstruction_CPY_ZP, compare);
		INSTRUCTION_EXPANSION(emu_vmInstruction_CPY_IMM, compare);
		// Decrement
		INSTRUCTION_EXPANSION(emu_vmInstruction_DEC_ZP, decrement);
		// Increment
		INSTRUCTION_EXPANSION(emu_vmInstruction_INC_ZP, increment);
		// Logical OR
		INSTRUCTION_EXPANSION_RAM(emu_vmInstruction_ORA_ZP, logicalOr);
		INSTRUCTION_EXPANSION(emu_vmInstruction_ORA_IMM, logicalOr);
		// Logical AND
		INSTRUCTION_EXPANSION_RAM(emu_vmInstruction_AND_ZP, logicalAnd);
		INSTRUCTION_EXPANSION(emu_vmInstruction_AND_IMM, logicalAnd);
		// Logical XOR
		INSTRUCTION_EXPANSION_RAM(emu_vmInstruction_EOR_ZP, logicalXor);
		INSTRUCTION_EXPANSION(emu_vmInstruction_EOR_IMM, logicalXor);
		// Arithmetic Shift Left
		INSTRUCTION_EXPANSION(emu_vmInstruction_ASL_ZP, arithmeticShiftLeft);
		// Rotate Left
		INSTRUCTION_EXPANSION(emu_vmInstruction_ROL_ZP, rotateLeft);
		// Logical Shift Right
		INSTRUCTION_EXPANSION(emu_vmInstruction_LSR_ZP, logicalShiftRight);
		// Rotate Right
		INSTRUCTION_EXPANSION(emu_vmInstruction_ROR_ZP, rotateRight);
	case emu_vmInstruction_ROR_IMP:
		rotateRight(vm, instruction, UINT8_MAX);
		break;
	case emu_vmInstruction_LSR_IMP:
		logicalShiftRight(vm, instruction, UINT8_MAX);
		break;
	case emu_vmInstruction_ROL_IMP:
		rotateLeft(vm, instruction, UINT8_MAX);
		break;
	case emu_vmInstruction_ASL_IMP:
		arithmeticShiftLeft(vm, instruction, UINT8_MAX);
		break;
	case emu_vmInstruction_DEX_IMP:
		vm->xReg--;
		checkFlagStatuses(vm, emu_vmStatus_Zero | emu_vmStatus_Negative, vm->xReg);
		break;
	case emu_vmInstruction_DEY_IMP:
		vm->yReg--;
		checkFlagStatuses(vm, emu_vmStatus_Zero | emu_vmStatus_Negative, vm->yReg);
		break;
	case emu_vmInstruction_INX_IMP:
		vm->xReg++;
		checkFlagStatuses(vm, emu_vmStatus_Zero | emu_vmStatus_Negative, vm->xReg);
		break;
	case emu_vmInstruction_INY_IMP:
		vm->yReg++;
		checkFlagStatuses(vm, emu_vmStatus_Zero | emu_vmStatus_Negative, vm->yReg);
		break;
	case emu_vmInstruction_BCC_REL:
	{
		uint8 address0 = getNext(vm);
		uint8 address1 = getNext(vm);

		// If carry flag is set, jump
		if (emu_vm_getStatus(vm, emu_vmStatus_Carry))
		{
			int16 relativeAddress = ((uint16)address1 << 8) | address0;
			// We need to subtract the 2 bytes that our program counter has already incremented
			vm->programCounter += (relativeAddress - 2);
		}
	}
	break;

	// Special
	case emu_vmInstruction_CLC_IMP:
		emu_vm_clearStatus(vm, emu_vmStatus_Carry);
		break;
	case emu_vmInstruction_SEC_IMP:
		emu_vm_setStatus(vm, emu_vmStatus_Carry);
		break;
	case emu_vmInstruction_RTS_IMP:
		g_logger_warning("Add proper support for RTS");
		break;
	default:
		g_logger_error("Cannot execute instruction: '%s'", emu_vmInstructions[instruction]);
		break;
	}
}

static uint8 getNext(emu_virtualMachine* vm)
{
	uint8 nextInstruction = emu_vmInstruction_ILLEGAL;
	if (vm->programCounter < vm->mmap.physicalMemorySize)
	{
		nextInstruction = emu_mmap_getNesAddress(&vm->mmap, vm->programCounter)[0];
	}

	vm->programCounter++;
	return nextInstruction;
}

static uint8 getRegisterValue(emu_virtualMachine* vm, emu_vmInstruction instruction)
{
	switch (instruction)
	{
	case emu_vmInstruction_STA_ZP:
	case emu_vmInstruction_CMP_ZP:
	case emu_vmInstruction_CMP_IMM:
		return vm->accumulatorReg;
	case emu_vmInstruction_STX_ZP:
	case emu_vmInstruction_CPX_ZP:
	case emu_vmInstruction_CPX_IMM:
		return vm->xReg;
	case emu_vmInstruction_STY_ZP:
	case emu_vmInstruction_CPY_ZP:
	case emu_vmInstruction_CPY_IMM:
		return vm->yReg;
	}

	g_logger_error("Cannot get register value for instruction '%s'", emu_vmInstructions[instruction]);
	return 0;
}

static void setRegisterValue(emu_virtualMachine* vm, emu_vmInstruction instruction, uint8 value)
{
	switch (instruction)
	{
	case emu_vmInstruction_LDA_ZP:
	case emu_vmInstruction_LDA_IMM:
	case emu_vmInstruction_CMP_ZP:
	case emu_vmInstruction_CMP_IMM:
		vm->accumulatorReg = value;
		break;
	case emu_vmInstruction_LDX_IMM:
	case emu_vmInstruction_LDX_ZP:
	case emu_vmInstruction_CPX_ZP:
	case emu_vmInstruction_CPX_IMM:
		vm->xReg = value;
		break;
	case emu_vmInstruction_LDY_IMM:
	case emu_vmInstruction_LDY_ZP:
	case emu_vmInstruction_CPY_ZP:
	case emu_vmInstruction_CPY_IMM:
		vm->yReg = value;
		break;
	default:
		g_logger_error("Cannot set register value for instruction '%s'", emu_vmInstructions[instruction]);
		break;
	}
}

static void setAbsRegisterValue(emu_virtualMachine* vm, emu_vmInstruction instruction, uint8* baseAddress)
{
	switch (instruction)
	{
	case emu_vmInstruction_LDA_ABX:
		vm->accumulatorReg = baseAddress[vm->xReg];
		break;
	default:
		g_logger_error("Cannot set register value for instruction '%s'", emu_vmInstructions[instruction]);
		break;
	}
}

static void storeRamValue(emu_virtualMachine* vm, emu_vmInstruction instruction, uint8 address)
{
	uint8* ramPtr = emu_mmap_getNesAddress(&vm->mmap, address);
	*ramPtr = getRegisterValue(vm, instruction);
}

static void addWithCarry(emu_virtualMachine* vm, emu_vmInstruction _, uint8 value)
{
	value += emu_vm_getStatus(vm, emu_vmStatus_Carry);
	if ((UINT8_MAX - vm->accumulatorReg) < value)
	{
		emu_vm_setStatus(vm, emu_vmStatus_Carry);
	}
	else
	{
		emu_vm_clearStatus(vm, emu_vmStatus_Carry);
	}

	int16 trueValue = (int16)((int8)value + (int8)vm->accumulatorReg);
	vm->accumulatorReg += value;
	checkFlagStatuses(vm, emu_vmStatus_Zero | emu_vmStatus_Negative, vm->accumulatorReg);
	checkOverflowFlag(vm, trueValue);
}

static void subtractWithCarry(emu_virtualMachine* vm, emu_vmInstruction _, uint8 value)
{
	int8 signedValue = -1 * (int8)value;
	signedValue += emu_vm_getStatus(vm, emu_vmStatus_Carry);
	if (vm->accumulatorReg < value)
	{
		emu_vm_setStatus(vm, emu_vmStatus_Carry);
	}
	else
	{
		emu_vm_clearStatus(vm, emu_vmStatus_Carry);
	}

	int16 trueValue = (int16)((int8)vm->accumulatorReg + (int8)signedValue);
	vm->accumulatorReg = (uint8)trueValue;
	checkFlagStatuses(vm, emu_vmStatus_Zero | emu_vmStatus_Negative, vm->accumulatorReg);
	checkOverflowFlag(vm, trueValue);
}

static void compare(emu_virtualMachine* vm, emu_vmInstruction instruction, uint8 value)
{
	uint8 registerValue = getRegisterValue(vm, instruction);
	// Perform unsigned subtraction 
	if (registerValue > value)
	{
		emu_vm_setStatus(vm, emu_vmStatus_Carry);
	}
	else
	{
		emu_vm_clearStatus(vm, emu_vmStatus_Carry);
	}

	setRegisterValue(vm, instruction, registerValue - value);
	checkFlagStatuses(vm, emu_vmStatus_Zero | emu_vmStatus_Negative, getRegisterValue(vm, instruction));
}

static void decrement(emu_virtualMachine* vm, emu_vmInstruction _, uint8 address)
{
	uint8* ramPtr = emu_mmap_getNesAddress(&vm->mmap, address);
	*ramPtr = *ramPtr - 1;
	checkFlagStatuses(vm, emu_vmStatus_Zero | emu_vmStatus_Negative, *ramPtr);
}

static void increment(emu_virtualMachine* vm, emu_vmInstruction _, uint8 address)
{
	uint8* ramPtr = emu_mmap_getNesAddress(&vm->mmap, address);
	*ramPtr = *ramPtr + 1;
	checkFlagStatuses(vm, emu_vmStatus_Zero | emu_vmStatus_Negative, *ramPtr);
}

static void logicalOr(emu_virtualMachine* vm, emu_vmInstruction _, uint8 value)
{
	vm->accumulatorReg |= value;
	checkFlagStatuses(vm, emu_vmStatus_Negative | emu_vmStatus_Zero, vm->accumulatorReg);
}

static void logicalAnd(emu_virtualMachine* vm, emu_vmInstruction _, uint8 value)
{
	vm->accumulatorReg &= value;
	checkFlagStatuses(vm, emu_vmStatus_Negative | emu_vmStatus_Zero, vm->accumulatorReg);
}

static void logicalXor(emu_virtualMachine* vm, emu_vmInstruction _, uint8 value)
{
	vm->accumulatorReg ^= value;
	checkFlagStatuses(vm, emu_vmStatus_Negative | emu_vmStatus_Zero, vm->accumulatorReg);
}

static void arithmeticShiftLeft(emu_virtualMachine* vm, emu_vmInstruction instruction, uint8 address)
{
	// Implicit instructions shift the A register left
	uint8* ramPtr = emu_mmap_getNesAddress(&vm->mmap, address);
	uint8 value = *ramPtr;
	if (instruction == emu_vmInstruction_ASL_IMP)
	{
		value = vm->accumulatorReg;
	}

	// Set carry flag if highest bit is set
	if ((value & (1 << 7)) > 0)
	{
		emu_vm_setStatus(vm, emu_vmStatus_Carry);
	}
	else
	{
		emu_vm_clearStatus(vm, emu_vmStatus_Carry);
	}

	if (instruction == emu_vmInstruction_ASL_IMP)
	{
		vm->accumulatorReg = vm->accumulatorReg << 1;
		checkFlagStatuses(vm, emu_vmStatus_Negative | emu_vmStatus_Zero, vm->accumulatorReg);
	}
	else
	{
		*ramPtr = *ramPtr << 1;
		checkFlagStatuses(vm, emu_vmStatus_Negative | emu_vmStatus_Zero, *ramPtr);
	}
}

static void rotateLeft(emu_virtualMachine* vm, emu_vmInstruction instruction, uint8 address)
{
	// Implicit instructions rotate the A register left
	uint8* ramPtr = emu_mmap_getNesAddress(&vm->mmap, address);
	uint8 value = *ramPtr;
	if (instruction == emu_vmInstruction_ROL_IMP)
	{
		value = vm->accumulatorReg;
	}

	// Set carry flag if highest bit is set
	uint8 oldCarry = emu_vm_getStatus(vm, emu_vmStatus_Carry);
	if ((value & (1 << 7)) > 0)
	{
		emu_vm_setStatus(vm, emu_vmStatus_Carry);
	}
	else
	{
		emu_vm_clearStatus(vm, emu_vmStatus_Carry);
	}

	if (instruction == emu_vmInstruction_ROL_IMP)
	{
		vm->accumulatorReg = vm->accumulatorReg << 1;
		vm->accumulatorReg |= oldCarry;
		checkFlagStatuses(vm, emu_vmStatus_Negative | emu_vmStatus_Zero, vm->accumulatorReg);
	}
	else
	{
		*ramPtr = *ramPtr << 1;
		*ramPtr |= oldCarry;
		checkFlagStatuses(vm, emu_vmStatus_Negative | emu_vmStatus_Zero, *ramPtr);
	}
}

static void logicalShiftRight(emu_virtualMachine* vm, emu_vmInstruction instruction, uint8 address)
{
	// Implicit instructions shift the A register right
	uint8* ramPtr = emu_mmap_getNesAddress(&vm->mmap, address);
	uint8 value = *ramPtr;
	if (instruction == emu_vmInstruction_LSR_IMP)
	{
		value = vm->accumulatorReg;
	}

	// Set carry flag if lowest bit is set
	if ((value & 1) > 0)
	{
		emu_vm_setStatus(vm, emu_vmStatus_Carry);
	}
	else
	{
		emu_vm_clearStatus(vm, emu_vmStatus_Carry);
	}

	if (instruction == emu_vmInstruction_LSR_IMP)
	{
		vm->accumulatorReg = vm->accumulatorReg >> 1;
		checkFlagStatuses(vm, emu_vmStatus_Negative | emu_vmStatus_Zero, vm->accumulatorReg);
	}
	else
	{
		*ramPtr = *ramPtr >> 1;
		checkFlagStatuses(vm, emu_vmStatus_Negative | emu_vmStatus_Zero, *ramPtr);
	}
}

static void rotateRight(emu_virtualMachine* vm, emu_vmInstruction instruction, uint8 address)
{
	// Implicit instructions rotate the A register right
	uint8* ramPtr = emu_mmap_getNesAddress(&vm->mmap, address);
	uint8 value = *ramPtr;
	if (instruction == emu_vmInstruction_ROR_IMP)
	{
		value = vm->accumulatorReg;
	}

	// Set carry flag if lowest bit is set
	uint8 oldCarry = emu_vm_getStatus(vm, emu_vmStatus_Carry);
	if ((value & 1) > 0)
	{
		emu_vm_setStatus(vm, emu_vmStatus_Carry);
	}
	else
	{
		emu_vm_clearStatus(vm, emu_vmStatus_Carry);
	}

	if (instruction == emu_vmInstruction_ROR_IMP)
	{
		vm->accumulatorReg = vm->accumulatorReg >> 1;
		vm->accumulatorReg |= (oldCarry << 7);
		checkFlagStatuses(vm, emu_vmStatus_Negative | emu_vmStatus_Zero, vm->accumulatorReg);
	}
	else
	{
		*ramPtr = *ramPtr >> 1;
		*ramPtr |= (oldCarry << 7);
		checkFlagStatuses(vm, emu_vmStatus_Negative | emu_vmStatus_Zero, *ramPtr);
	}
}

static void checkFlagStatuses(emu_virtualMachine* vm, uint8 flagsToCheck, uint8 value)
{
	if (flagsToCheck & emu_vmStatus_Zero)
	{
		if (value == 0)
		{
			emu_vm_setStatus(vm, emu_vmStatus_Zero);
		}
		else
		{
			emu_vm_clearStatus(vm, emu_vmStatus_Zero);
		}
	}

	if (flagsToCheck & emu_vmStatus_Negative)
	{
		if ((int8)value < 0)
		{
			emu_vm_setStatus(vm, emu_vmStatus_Negative);
		}
		else
		{
			emu_vm_clearStatus(vm, emu_vmStatus_Negative);
		}
	}
}

static void checkOverflowFlag(emu_virtualMachine* vm, int16 trueValue)
{
	if (trueValue > 127 || trueValue < -128)
	{
		emu_vm_setStatus(vm, emu_vmStatus_Overflow);
	}
	else
	{
		emu_vm_clearStatus(vm, emu_vmStatus_Overflow);
	}
}