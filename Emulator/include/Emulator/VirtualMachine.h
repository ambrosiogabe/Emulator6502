#ifndef EMULATOR_VIRTUAL_MACHINE_H
#define EMULATOR_VIRTUAL_MACHINE_H
#include <cppUtils/cppUtils.h>

typedef enum emu_vmType
{
	emu_vmType_None = 0,
	emu_vmType_NES = 1,
	emu_vmType_Commodore64 = 2,
} emu_vmType;

typedef enum emu_vmInstruction
{
	emu_vmInstruction_BRK = 0x0,
	// -- OR instructions --
	emu_vmInstruction_ORA = 0x1,
	// -- ADC instructions --
	emu_vmInstruction_IND_ADC = 0x61,
	emu_vmInstruction_ZP_ADC = 0x65,
	emu_vmInstruction_IMMEDIATE_ADC = 0x69,
	emu_vmInstruction_ABS_ADC = 0x6c,
	emu_vmInstruction_IND_ADC_Y = 0x71,
	emu_vmInstruction_ZP_ADC_X = 0x75,
	emu_vmInstruction_ABS_ADC_X = 0x7D,
	// TODO: Is this immediate mode?
	emu_vmInstruction_IDX_ADC_Y = 0x79,
	// --  Store Instructions --
	emu_vmInstruction_ZP_STY = 0x84,
	emu_vmInstruction_STA = 0x85,
	emu_vmInstruction_ZP_STX = 0x86,
	emu_vmInstruction_ABS_STY = 0x8c,
	emu_vmInstruction_ABS_STA = 0x8D,
	emu_vmInstruction_ABS_STX = 0x8e,
	emu_vmInstruction_IDX_STA_IND_Y = 0x91,
	emu_vmInstruction_IDX_STY_ZP_X = 0x94,
	// -- JMP instructions --
	emu_vmInstruction_JMP_INDIRECT = 0x6C,
} emu_vmInstruction;

typedef enum emu_vmError
{
	emu_vmError_None = 0,
	emu_vmError_NotEnoughROM,
	emu_vmError_NullVm,
	emu_vmError_NullProgram,
	emu_vmError_EmptyProgram,
} emu_vmError;

typedef struct emu_virtualMachine
{
	emu_vmType vmType;
	uint16 programCounter;
	uint8 accumulatorReg;
	uint8 xReg;
	uint8 yReg;
	uint8 statusReg;
	uint8 stackPointer;

	uint32 ramSize;
	uint8* ram;
	// 3 Mirrors, same size as RAM
	uint8* mirrors[3];

	uint32 romSize;
	uint8* rom;
} emu_virtualMachine;

// NES Type
// @romSize: $BFE0 = 49'120 bytes
// @ramSize: $0800 = 2 KiloBytes
// Commodore64 Type
// Unsupported
emu_virtualMachine emu_vm_init(emu_vmType vmType);
emu_virtualMachine emu_vm_sizedInit(uint32 romSize, uint32 ramSize, emu_vmType vmType);

emu_vmError emu_vm_loadProgram(emu_virtualMachine* vm, uint8* program, size_t programSize);

emu_vmError emu_vm_initProgram(emu_virtualMachine* vm);

emu_vmError emu_vm_tick(emu_virtualMachine* vm);

void emu_vm_free(emu_virtualMachine* vm);

#endif
