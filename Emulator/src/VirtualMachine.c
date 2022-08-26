#include "Emulator/VirtualMachine.h"

// --------------- Internal Structures --------------- 
typedef enum RegisterMode
{
	RegA,
	RegX,
	RegY
} RegisterMode;

typedef enum AddressMode
{
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

emu_vmError emu_vm_tick(emu_virtualMachine* vm, uint8* program, size_t programSize)
{
	VmInstruction instruction = fetchInstruction(vm);
	executeInstruction(vm, &instruction);
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

// --------------- Internal Functions ---------------
// Fetch and decode next instruction
static VmInstruction fetchInstruction(emu_virtualMachine* vm)
{
	uint8 nextInstruction = emu_vmInstruction_BRK;
	if (vm->programCounter < vm->romSize)
	{
		nextInstruction = vm->rom[vm->programCounter];
	}

	VmInstruction res;

	// Figure out the addressing mode
	switch (nextInstruction)
	{
	case emu_vmInstruction_ABS_ADC:
	case emu_vmInstruction_ABS_ADC_X:
	case emu_vmInstruction_ABS_STA:
	case emu_vmInstruction_ABS_STX:
	case emu_vmInstruction_ABS_STY:
		res.addressMode = AddressMode_Absolute;
		break;
	case emu_vmInstruction_IMMEDIATE_ADC:
		res.addressMode = AddressMode_Immediate;
		break;
	case emu_vmInstruction_ZP_ADC:
	case emu_vmInstruction_ZP_ADC_X:
	case emu_vmInstruction_ZP_STX:
	case emu_vmInstruction_ZP_STY:
		res.addressMode = AddressMode_ZeroPage;
		break;
	case emu_vmInstruction_BRK:
		res.baseType = BaseInstruction_Break;
		break;
	}

	return res;
}

static void executeInstruction(emu_virtualMachine* vm, const VmInstruction* instruction)
{

}