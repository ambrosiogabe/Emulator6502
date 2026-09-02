#include "Emulator/VirtualMachine.h"

#include "Emulator/Assembler.h"

const char* emu_vmInstructions[EMU_MAX_INSTRUCTION_OPCODE] = { 0 };

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
  function(vm, instruction, vm->ram[address]);\
}\
break

void emu_vm_initDebug()
{
	for (size_t i = 0; i < EMU_MAX_INSTRUCTION_OPCODE; i++)
	{
		emu_vmInstructions[i] = "NULL";
	}

	emu_vmInstructions[emu_vmInstruction_BRK] = "BRK";
	emu_vmInstructions[emu_vmInstruction_CLC] = "CLC";
	emu_vmInstructions[emu_vmInstruction_RTS] = "RTS";
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
	emu_vmInstructions[emu_vmInstruction_BCC_REL] = "BCC_REL";

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

// romSize = $BFE0 = 49'120 bytes
// ramSize = $0800 = 2 KiloBytes
emu_virtualMachine emu_vm_init(emu_vmType vmType)
{
	if (vmType == emu_vmType_NES)
	{
		emu_virtualMachine res = emu_vm_sizedInit(49'120, 2'048, vmType);
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

emu_virtualMachine emu_vm_sizedInit(uint32 romSize, uint32 ramSize, emu_vmType vmType)
{
	uint8* ramPtr = (uint8*)g_memory_allocate(sizeof(uint8) * ramSize);
	uint8* mirrorPtrs[3];
	mirrorPtrs[0] = g_memory_allocate(sizeof(uint8) * ramSize);
	mirrorPtrs[1] = g_memory_allocate(sizeof(uint8) * ramSize);
	mirrorPtrs[2] = g_memory_allocate(sizeof(uint8) * ramSize);

	uint8* romPtr = (uint8*)g_memory_allocate(sizeof(uint8) * romSize);

	emu_virtualMachine vm = {
		.vmType = vmType,
		.programCounter = 0,
		.accumulatorReg = 0,
		.xReg = 0,
		.yReg = 0,
		.statusReg = 0,
		.stackPointer = 0,

		.ramSize = ramSize,
		.ram = ramPtr,
		.mirrors = {mirrorPtrs[0], mirrorPtrs[1], mirrorPtrs[2]},

		.romSize = romSize,
		.rom = romPtr
	};
	return vm;
}

emu_vmError emu_vm_loadProgram(emu_virtualMachine* vm, uint8* program, size_t programSize)
{
	// Check assertions
	if (vm == NULL)
	{
		return emu_vmError_NullVm;
	}

	// TODO: Should I allow null programs to reset ROM
	if (program == NULL)
	{
		return emu_vmError_NullProgram;
	}

	if (programSize == 0)
	{
		return emu_vmError_EmptyProgram;
	}

	if (vm->romSize < programSize)
	{
		return emu_vmError_NotEnoughROM;
	}

	// Load the program into ROM
	g_memory_copyMem(vm->rom, program, programSize);

	// Set all instructions after end of program to illegal opcodes
	for (size_t i = programSize; i < vm->romSize; i++)
	{
		vm->rom[i] = emu_vmInstruction_ILLEGAL;
	}

	return emu_vmError_None;
}

emu_vmError emu_vm_initProgram(emu_virtualMachine* vm)
{
	// Check assertions
	if (vm == NULL)
	{
		return emu_vmError_NullVm;
	}

	g_logger_assert(vm->ram != NULL, "Null VirtualMachine RAM.");
	g_logger_assert(vm->mirrors[0] != NULL, "Null VirtualMachine Mirror[0].");
	g_logger_assert(vm->mirrors[1] != NULL, "Null VirtualMachine Mirror[1].");
	g_logger_assert(vm->mirrors[2] != NULL, "Null VirtualMachine Mirror[2].");

	vm->programCounter = 0;
	vm->accumulatorReg = 0;
	vm->xReg = 0;
	vm->yReg = 0;
	vm->stackPointer = 0;
	vm->statusReg = 0;

	g_memory_zeroMem(vm->ram, vm->ramSize);
	g_memory_zeroMem(vm->mirrors[0], vm->ramSize);
	g_memory_zeroMem(vm->mirrors[1], vm->ramSize);
	g_memory_zeroMem(vm->mirrors[2], vm->ramSize);
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
		if (vm->ram)
		{
			g_memory_free(vm->ram);
			vm->ram = NULL;
			vm->ramSize = 0;
		}

		if (vm->mirrors[0])
		{
			g_memory_free(vm->mirrors[0]);
			vm->mirrors[0] = NULL;
		}

		if (vm->mirrors[1])
		{
			g_memory_free(vm->mirrors[1]);
			vm->mirrors[1] = NULL;
		}

		if (vm->mirrors[2])
		{
			g_memory_free(vm->mirrors[2]);
			vm->mirrors[2] = NULL;
		}

		if (vm->rom)
		{
			g_memory_free(vm->rom);
			vm->rom = NULL;
			vm->romSize = 0;
		}
	}

	g_memory_zeroMem(vm, sizeof(emu_virtualMachine));
}

const char* emu_vm_instructionToString(emu_vmInstruction instruction)
{
	return emu_vmInstructions[instruction];
}

uint8 emu_vm_getStatus(emu_virtualMachine* vm, emu_vmStatus status)
{
	return (vm->statusReg >> status) & 1;
}

void emu_vm_setStatus(emu_virtualMachine* vm, emu_vmStatus status)
{
	vm->statusReg = (vm->statusReg & ~(1 << status)) | (1 << status);
}

void emu_vm_clearStatus(emu_virtualMachine* vm, emu_vmStatus status)
{
	vm->statusReg = (vm->statusReg & ~(1 << status)) | (0 << status);
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
		break;
	case emu_vmInstruction_DEY_IMP:
		vm->yReg--;
		break;
	case emu_vmInstruction_INX_IMP:
		vm->xReg++;
		break;
	case emu_vmInstruction_INY_IMP:
		vm->yReg++;
		break;
	case emu_vmInstruction_BCC_REL:
	{
		uint8 address0 = getNext(vm);
		uint8 address1 = getNext(vm);

		// If carry flag is set, jump
		if (emu_vm_getStatus(vm, emu_vmStatus_Carry))
		{
			int16 relativeAddress = ((uint16)address0 << 8) | address1;
			// We need to subtract the 2 bytes that our program counter has already incremented
			vm->programCounter += (relativeAddress - 2);
		}
	}
	break;

	// Special
	case emu_vmInstruction_CLC:
		emu_vm_clearStatus(vm, emu_vmStatus_Carry);
		break;
	default:
		g_logger_error("Cannot execute instruction: '%s'", emu_vmInstructions[instruction]);
		break;
	}
}

static uint8 getNext(emu_virtualMachine* vm)
{
	uint8 nextInstruction = emu_vmInstruction_ILLEGAL;
	if (vm->programCounter < vm->romSize)
	{
		nextInstruction = vm->rom[vm->programCounter];
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

static void storeRamValue(emu_virtualMachine* vm, emu_vmInstruction instruction, uint8 address)
{
	vm->ram[address] = getRegisterValue(vm, instruction);
}

static void addWithCarry(emu_virtualMachine* vm, emu_vmInstruction _, uint8 value)
{
	value += emu_vm_getStatus(vm, emu_vmStatus_Carry);
	if ((UINT8_MAX - vm->accumulatorReg) < value)
	{
		emu_vm_setStatus(vm, emu_vmStatus_Carry);
	}
	vm->accumulatorReg += value;
}

static void subtractWithCarry(emu_virtualMachine* vm, emu_vmInstruction _, uint8 value)
{
	// TODO: Double check that ones' complement works correctly here
	value = ~value;
	value += emu_vm_getStatus(vm, emu_vmStatus_Carry);
	if ((UINT8_MAX - vm->accumulatorReg) < value)
	{
		emu_vm_setStatus(vm, emu_vmStatus_Carry);
	}
	vm->accumulatorReg += value;
}

static void compare(emu_virtualMachine* vm, emu_vmInstruction instruction, uint8 value)
{
	uint8 registerValue = getRegisterValue(vm, instruction);
	// Perform unsigned subtraction 
	if (registerValue > value)
	{
		emu_vm_setStatus(vm, emu_vmStatus_Carry);
	}
	setRegisterValue(vm, instruction, registerValue - value);
}

static void decrement(emu_virtualMachine* vm, emu_vmInstruction _, uint8 address)
{
	vm->ram[address] = vm->ram[address] - 1;
}

static void increment(emu_virtualMachine* vm, emu_vmInstruction _, uint8 address)
{
	vm->ram[address] = vm->ram[address] + 1;
}

static void logicalOr(emu_virtualMachine* vm, emu_vmInstruction _, uint8 value)
{
	vm->accumulatorReg |= value;
}

static void logicalAnd(emu_virtualMachine* vm, emu_vmInstruction _, uint8 value)
{
	vm->accumulatorReg &= value;
}

static void logicalXor(emu_virtualMachine* vm, emu_vmInstruction _, uint8 value)
{
	vm->accumulatorReg ^= value;
}

static void arithmeticShiftLeft(emu_virtualMachine* vm, emu_vmInstruction instruction, uint8 address)
{
	// Implicit instructions shift the A register left
	uint8 value = vm->ram[address];
	if (instruction == emu_vmInstruction_ASL_IMP)
	{
		value = vm->accumulatorReg;
	}

	// Set carry flag if highest bit is set
	if ((value & (1 << 7)) > 0)
	{
		emu_vm_setStatus(vm, emu_vmStatus_Carry);
	}

	if (instruction == emu_vmInstruction_ASL_IMP)
	{
		vm->accumulatorReg = vm->accumulatorReg << 1;
	}
	else
	{
		vm->ram[address] = vm->ram[address] << 1;
	}
}

static void rotateLeft(emu_virtualMachine* vm, emu_vmInstruction instruction, uint8 address)
{
	// Implicit instructions rotate the A register left
	uint8 value = vm->ram[address];
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

	if (instruction == emu_vmInstruction_ROL_IMP)
	{
		vm->accumulatorReg = vm->accumulatorReg << 1;
		vm->accumulatorReg |= oldCarry;
	}
	else
	{
		vm->ram[address] = vm->ram[address] << 1;
		vm->ram[address] |= oldCarry;
	}
}

static void logicalShiftRight(emu_virtualMachine* vm, emu_vmInstruction instruction, uint8 address)
{
	// Implicit instructions shift the A register right
	uint8 value = vm->ram[address];
	if (instruction == emu_vmInstruction_LSR_IMP)
	{
		value = vm->accumulatorReg;
	}

	// Set carry flag if lowest bit is set
	if ((value & 1) > 0)
	{
		emu_vm_setStatus(vm, emu_vmStatus_Carry);
	}

	if (instruction == emu_vmInstruction_LSR_IMP)
	{
		vm->accumulatorReg = vm->accumulatorReg >> 1;
	}
	else
	{
		vm->ram[address] = vm->ram[address] >> 1;
	}
}

static void rotateRight(emu_virtualMachine* vm, emu_vmInstruction instruction, uint8 address)
{
	// Implicit instructions rotate the A register right
	uint8 value = vm->ram[address];
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

	if (instruction == emu_vmInstruction_ROR_IMP)
	{
		vm->accumulatorReg = vm->accumulatorReg >> 1;
		vm->accumulatorReg |= (oldCarry << 7);
	}
	else
	{
		vm->ram[address] = vm->ram[address] >> 1;
		vm->ram[address] |= (oldCarry << 7);
	}
}