#include "Emulator/VirtualMachine.h"

#include "Emulator/Assembler.h"

const char* emu_vmInstructions[EMU_MAX_INSTRUCTION_OPCODE] = { 0 };

// --------------- Internal Structures --------------- 
typedef enum RegisterMode
{
	RegA,
	RegX,
	RegY
} RegisterMode;

typedef enum AddressMode
{
	AddressMode_None,
	AddressMode_ZeroPage,
	AddressMode_Absolute,
	AddressMode_Immediate,
	AddressMode_Relative
} AddressMode;

typedef enum BaseInstruction
{
	BaseInstruction_Break = 0,
	BaseInstruction_Store,
	BaseInstruction_Load,
	BaseInstruction_Jump,
	BaseInstruction_Add,
	BaseInstruction_ClearFlag,
	BaseInstruction_Compare,
	BaseInstruction_Branch,
} BaseInstruction;

typedef struct VmInstruction
{
	emu_vmInstruction type;
	BaseInstruction baseType;
	AddressMode addressMode;
	RegisterMode regMode;
	bool isIndexed;
	uint8 instructionLength;
	uint8 arg0;
	uint8 arg1;
} VmInstruction;

// --------------- Internal Functions ---------------
// Fetch and decode next instruction
static VmInstruction fetchInstruction(emu_virtualMachine* vm);
static void executeInstruction(emu_virtualMachine* vm, const VmInstruction* instruction);
static uint8 getNext(emu_virtualMachine* vm);

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
	emu_vmInstructions[emu_vmInstruction_ORA_IZX] = "ORA_IZX";
	// --  ADC instructions --
	emu_vmInstructions[emu_vmInstruction_ADC_IZX] = "ADC_IZX";
	emu_vmInstructions[emu_vmInstruction_ADC_ZP] = "ADC_ZP";
	emu_vmInstructions[emu_vmInstruction_ADC_IMM] = "ADC_IMM";
	emu_vmInstructions[emu_vmInstruction_ADC_ABS] = "ADC_ABS";
	emu_vmInstructions[emu_vmInstruction_ADC_IZY] = "ADC_IZY";
	emu_vmInstructions[emu_vmInstruction_ADC_ZPX] = "ADC_ZPX";
	emu_vmInstructions[emu_vmInstruction_ADC_ABY] = "ADC_ABY";
	emu_vmInstructions[emu_vmInstruction_ADC_ABX] = "ADC_ABX";
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
	VmInstruction instruction = fetchInstruction(vm);
	if (instruction.type == emu_vmInstruction_ILLEGAL)
	{
		return emu_vmError_IllegalOpcode;
	}
	else if (instruction.type == emu_vmInstruction_BRK)
	{
		return emu_vmError_Break;
	}
	executeInstruction(vm, &instruction);

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
// Fetch and decode next instruction
static VmInstruction fetchInstruction(emu_virtualMachine* vm)
{
	emu_vmInstruction nextInstruction = (emu_vmInstruction)getNext(vm);
	VmInstruction res = { 0 };
	res.type = nextInstruction;

	// Figure out the addressing mode
	switch (nextInstruction)
	{
	case emu_vmInstruction_ADC_ABS:
	case emu_vmInstruction_ADC_ABY:
	case emu_vmInstruction_ADC_ABX:
	case emu_vmInstruction_STY_ABS:
	case emu_vmInstruction_STA_ABS:
	case emu_vmInstruction_STX_ABS:
	case emu_vmInstruction_STA_ABY:
	case emu_vmInstruction_STA_ABX:
	case emu_vmInstruction_LDY_ABS:
	case emu_vmInstruction_LDA_ABS:
	case emu_vmInstruction_LDX_ABS:
	case emu_vmInstruction_LDA_ABY:
	case emu_vmInstruction_LDY_ABX:
	case emu_vmInstruction_LDA_ABX:
	case emu_vmInstruction_LDX_ABY:
	case emu_vmInstruction_CMP_ABS:
	case emu_vmInstruction_CMP_ABY:
	case emu_vmInstruction_CMP_ABX:
		res.addressMode = AddressMode_Absolute;
		break;
	case emu_vmInstruction_ADC_IMM:
	case emu_vmInstruction_LDY_IMM:
	case emu_vmInstruction_LDX_IMM:
	case emu_vmInstruction_LDA_IMM:
	case emu_vmInstruction_CMP_IMM:
		res.addressMode = AddressMode_Immediate;
		break;
	case emu_vmInstruction_ADC_ZP:
	case emu_vmInstruction_ADC_ZPX:
	case emu_vmInstruction_STY_ZP:
	case emu_vmInstruction_STA_ZP:
	case emu_vmInstruction_STX_ZP:
	case emu_vmInstruction_STY_ZPX:
	case emu_vmInstruction_STA_ZPX:
	case emu_vmInstruction_STX_ZPY:
	case emu_vmInstruction_LDY_ZP:
	case emu_vmInstruction_LDA_ZP:
	case emu_vmInstruction_LDX_ZP:
	case emu_vmInstruction_LDY_ZPX:
	case emu_vmInstruction_LDA_ZPX:
	case emu_vmInstruction_LDX_ZPY:
	case emu_vmInstruction_CMP_ZP:
	case emu_vmInstruction_CMP_ZPX:
		res.addressMode = AddressMode_ZeroPage;
		break;
	case emu_vmInstruction_BCC_REL:
		res.addressMode = AddressMode_Relative;
		break;
	default:
		res.addressMode = AddressMode_None;
		break;
	}

	switch (res.addressMode)
	{
	case AddressMode_ZeroPage:
	case AddressMode_Immediate:
		res.instructionLength = 1;
		break;
	case AddressMode_Absolute:
	case AddressMode_Relative:
		res.instructionLength = 2;
		break;
	case AddressMode_None:
		res.instructionLength = 0;
		break;
	}

	if (res.instructionLength == 2)
	{
		res.arg0 = getNext(vm);
		res.arg1 = getNext(vm);
	}
	else if (res.instructionLength == 1)
	{
		res.arg0 = getNext(vm);
	}

	// TODO: Implement indexing
	res.isIndexed = false;

	// Figure out the base type
	switch (nextInstruction)
	{
	case emu_vmInstruction_BRK:
		res.baseType = BaseInstruction_Break;
		break;
	case emu_vmInstruction_CLC:
		res.baseType = BaseInstruction_ClearFlag;
		break;
	case emu_vmInstruction_RTS:
	case emu_vmInstruction_JMP_IND:
		res.baseType = BaseInstruction_Jump;
		break;
	case emu_vmInstruction_ORA_IZX:
		break;
	case emu_vmInstruction_ADC_IZX:
	case emu_vmInstruction_ADC_ZP:
	case emu_vmInstruction_ADC_IMM:
	case emu_vmInstruction_ADC_ABS:
	case emu_vmInstruction_ADC_IZY:
	case emu_vmInstruction_ADC_ZPX:
	case emu_vmInstruction_ADC_ABY:
	case emu_vmInstruction_ADC_ABX:
		res.baseType = BaseInstruction_Add;
		break;
	case emu_vmInstruction_STA_IZX:
	case emu_vmInstruction_STY_ZP:
	case emu_vmInstruction_STA_ZP:
	case emu_vmInstruction_STX_ZP:
	case emu_vmInstruction_STY_ABS:
	case emu_vmInstruction_STA_ABS:
	case emu_vmInstruction_STX_ABS:
	case emu_vmInstruction_STA_IZY:
	case emu_vmInstruction_STY_ZPX:
	case emu_vmInstruction_STA_ZPX:
	case emu_vmInstruction_STX_ZPY:
	case emu_vmInstruction_STA_ABY:
	case emu_vmInstruction_STA_ABX:
		res.baseType = BaseInstruction_Store;
		break;
	case emu_vmInstruction_LDY_IMM:
	case emu_vmInstruction_LDA_IZX:
	case emu_vmInstruction_LDX_IMM:
	case emu_vmInstruction_LDY_ZP:
	case emu_vmInstruction_LDA_ZP:
	case emu_vmInstruction_LDX_ZP:
	case emu_vmInstruction_LDA_IMM:
	case emu_vmInstruction_LDY_ABS:
	case emu_vmInstruction_LDA_ABS:
	case emu_vmInstruction_LDX_ABS:
	case emu_vmInstruction_LDA_IZY:
	case emu_vmInstruction_LDY_ZPX:
	case emu_vmInstruction_LDA_ZPX:
	case emu_vmInstruction_LDX_ZPY:
	case emu_vmInstruction_LDA_ABY:
	case emu_vmInstruction_LDY_ABX:
	case emu_vmInstruction_LDA_ABX:
	case emu_vmInstruction_LDX_ABY:
		res.baseType = BaseInstruction_Load;
		break;
	case emu_vmInstruction_CMP_IZX:
	case emu_vmInstruction_CMP_ZP:
	case emu_vmInstruction_CMP_IMM:
	case emu_vmInstruction_CMP_ABS:
	case emu_vmInstruction_CMP_IZY:
	case emu_vmInstruction_CMP_ZPX:
	case emu_vmInstruction_CMP_ABY:
	case emu_vmInstruction_CMP_ABX:
		res.baseType = BaseInstruction_Compare;
		break;
	case emu_vmInstruction_BCC_REL:
		res.baseType = BaseInstruction_Branch;
		break;
	}

	// Figure out reg mode
	switch (nextInstruction)
	{
	case emu_vmInstruction_BRK:
	case emu_vmInstruction_CLC:
	case emu_vmInstruction_RTS:
	case emu_vmInstruction_JMP_IND:
	case emu_vmInstruction_ADC_ZP:
	case emu_vmInstruction_ADC_IMM:
	case emu_vmInstruction_ADC_ABS:
	case emu_vmInstruction_BCC_REL:
		break;
	case emu_vmInstruction_ORA_IZX:
	case emu_vmInstruction_ADC_IZX:
	case emu_vmInstruction_ADC_ZPX:
	case emu_vmInstruction_ADC_ABX:
	case emu_vmInstruction_STX_ZP:
	case emu_vmInstruction_STX_ABS:
	case emu_vmInstruction_STX_ZPY:
	case emu_vmInstruction_LDX_IMM:
	case emu_vmInstruction_LDX_ZP:
	case emu_vmInstruction_LDX_ABS:
	case emu_vmInstruction_LDX_ZPY:
	case emu_vmInstruction_LDX_ABY:
		res.regMode = RegX;
		break;
	case emu_vmInstruction_ADC_IZY:
	case emu_vmInstruction_ADC_ABY:
	case emu_vmInstruction_STY_ZP:
	case emu_vmInstruction_STY_ABS:
	case emu_vmInstruction_STY_ZPX:
	case emu_vmInstruction_LDY_IMM:
	case emu_vmInstruction_LDY_ZP:
	case emu_vmInstruction_LDY_ABS:
	case emu_vmInstruction_LDY_ZPX:
	case emu_vmInstruction_LDY_ABX:
		res.regMode = RegY;
		break;
	case emu_vmInstruction_STA_IZX:
	case emu_vmInstruction_STA_ZP:
	case emu_vmInstruction_STA_ABS:
	case emu_vmInstruction_STA_IZY:
	case emu_vmInstruction_STA_ZPX:
	case emu_vmInstruction_STA_ABY:
	case emu_vmInstruction_STA_ABX:
	case emu_vmInstruction_LDA_IZX:
	case emu_vmInstruction_LDA_ZP:
	case emu_vmInstruction_LDA_IMM:
	case emu_vmInstruction_LDA_ABS:
	case emu_vmInstruction_LDA_IZY:
	case emu_vmInstruction_LDA_ZPX:
	case emu_vmInstruction_LDA_ABY:
	case emu_vmInstruction_LDA_ABX:
	case emu_vmInstruction_CMP_IZX:
	case emu_vmInstruction_CMP_ZP:
	case emu_vmInstruction_CMP_IMM:
	case emu_vmInstruction_CMP_ABS:
	case emu_vmInstruction_CMP_IZY:
	case emu_vmInstruction_CMP_ZPX:
	case emu_vmInstruction_CMP_ABY:
	case emu_vmInstruction_CMP_ABX:
		res.regMode = RegA;
		break;
	}

	return res;
}

static void executeInstruction(emu_virtualMachine* vm, const VmInstruction* instruction)
{
	switch (instruction->baseType)
	{
	case BaseInstruction_Store:
	{
		uint8 address = instruction->arg0;

		switch (instruction->regMode)
		{
		case RegX:
			vm->ram[address] = vm->xReg;
			break;
		case RegY:
			vm->ram[address] = vm->yReg;
			break;
		case RegA:
			vm->ram[address] = vm->accumulatorReg;
			break;
		}
	}
	break;
	case BaseInstruction_Load:
	{
		uint8 value = instruction->arg0;
		if (instruction->addressMode == AddressMode_ZeroPage)
		{
			uint8 address = instruction->arg0;
			value = vm->ram[address];
		}

		switch (instruction->regMode)
		{
		case RegX:
			vm->xReg = value;
			break;
		case RegY:
			vm->yReg = value;
			break;
		case RegA:
			vm->accumulatorReg = value;
			break;
		}
	}
	break;
	case BaseInstruction_Add:
	{
		uint8 value = instruction->arg0;
		if (instruction->addressMode == AddressMode_ZeroPage)
		{
			uint8 address = instruction->arg0;
			value = vm->ram[address];
		}

		value += emu_vm_getStatus(vm, emu_vmStatus_Carry);
		if ((UINT8_MAX - vm->accumulatorReg) < value)
		{
			emu_vm_setStatus(vm, emu_vmStatus_Carry);
		}
		vm->accumulatorReg += value;
	}
	break;
	case BaseInstruction_ClearFlag:
	{
		if (instruction->type == emu_vmInstruction_CLC)
		{
			emu_vm_clearStatus(vm, emu_vmStatus_Carry);
		}
	}
	break;
	case BaseInstruction_Compare:
	{
		uint8 value = instruction->arg0;
		if (instruction->addressMode == AddressMode_ZeroPage)
		{
			uint8 address = instruction->arg0;
			value = vm->ram[address];
		}

		// Perform unsigned subtraction 
		if (vm->accumulatorReg > value)
		{
			emu_vm_setStatus(vm, emu_vmStatus_Carry);
		}
		vm->accumulatorReg -= value;
	}
	break;
	case BaseInstruction_Branch:
	{
		// If carry flag is set, jump
		if (emu_vm_getStatus(vm, emu_vmStatus_Carry))
		{
			int16 relativeAddress = ((uint16)instruction->arg0 << 8) | instruction->arg1;
			// We need to subtract the 2 bytes that our program counter has already incremented
			vm->programCounter += (relativeAddress - instruction->instructionLength);
		}
	}
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