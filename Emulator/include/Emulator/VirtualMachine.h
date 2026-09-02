#ifndef EMULATOR_VIRTUAL_MACHINE_H
#define EMULATOR_VIRTUAL_MACHINE_H
#include <cppUtils/cppUtils.h>

typedef enum emu_vmType
{
	emu_vmType_None = 0,
	emu_vmType_NES = 1,
	emu_vmType_Commodore64 = 2,
} emu_vmType;

// From https://www.oxyron.de/html/opcodes02.html
// imm = #$00    immediate
// zp  = $00     zero page
// zpx = $00,X   zero page + X
// zpy = $00,Y   zero page + y
// izx = ($00,X) 

// Ones I care about for now
// imm
// zp
// 

#define EMU_MAX_INSTRUCTION_OPCODE UINT8_MAX
extern const char* emu_vmInstructions[];

/**
* ZP - Zero Page
* IMP - Implicit
* ZPX -
* ZPY -
* IMM - Immediate
* ABS - Absolute
* ABX -
* ABY -
* IZX -
* IZY -
*/
typedef enum emu_vmInstruction
{
	emu_vmInstruction_BRK = 0x00,
	emu_vmInstruction_CLC = 0x18,
	emu_vmInstruction_RTS = 0x60,
	// -- OR instructions --
	emu_vmInstruction_ORA_IMM = 0x09,
	emu_vmInstruction_ORA_ZP = 0x05,
	emu_vmInstruction_ORA_ZPX = 0x15,
	emu_vmInstruction_ORA_IZY = 0x11,
	emu_vmInstruction_ORA_ABS = 0x0D,
	emu_vmInstruction_ORA_ABX = 0x1D,
	emu_vmInstruction_ORA_ABY = 0x19,
	emu_vmInstruction_ORA_IZX = 0x01,
	// -- AND instructions --
	emu_vmInstruction_AND_IMM = 0x29,
	emu_vmInstruction_AND_ZP = 0x25,
	emu_vmInstruction_AND_ZPX = 0x15,
	emu_vmInstruction_AND_IZX = 0x21,
	emu_vmInstruction_AND_IZY = 0x31,
	emu_vmInstruction_AND_ABS = 0x2D,
	emu_vmInstruction_AND_ABX = 0x3D,
	emu_vmInstruction_AND_ABY = 0x39,
	// -- XOR instructions --
	emu_vmInstruction_EOR_IMM = 0x49,
	emu_vmInstruction_EOR_ZP = 0x45,
	emu_vmInstruction_EOR_ZPX = 0x55,
	emu_vmInstruction_EOR_IZX = 0x41,
	emu_vmInstruction_EOR_IZY = 0x51,
	emu_vmInstruction_EOR_ABS = 0x4D,
	emu_vmInstruction_EOR_ABX = 0x5D,
	emu_vmInstruction_EOR_ABY = 0x59,
	// -- ADC instructions --
	emu_vmInstruction_ADC_IZX = 0x61,
	emu_vmInstruction_ADC_ZP = 0x65,
	emu_vmInstruction_ADC_IMM = 0x69,
	emu_vmInstruction_ADC_ABS = 0x6D,
	emu_vmInstruction_ADC_IZY = 0x71,
	emu_vmInstruction_ADC_ZPX = 0x75,
	emu_vmInstruction_ADC_ABY = 0x79,
	emu_vmInstruction_ADC_ABX = 0x7D,
	// -- SBC Instructions --
	emu_vmInstruction_SBC_IMM = 0xE9,
	emu_vmInstruction_SBC_ZP = 0xE5,
	emu_vmInstruction_SBC_ZPX = 0xF5,
	emu_vmInstruction_SBC_IZX = 0xE1,
	emu_vmInstruction_SBC_IZY = 0xF1,
	emu_vmInstruction_SBC_ABS = 0xED,
	emu_vmInstruction_SBC_ABX = 0xFD,
	emu_vmInstruction_SBC_ABY = 0xF9,
	// -- Store Instructions --
	emu_vmInstruction_STA_IZX = 0x81,
	emu_vmInstruction_STY_ZP = 0x84,
	emu_vmInstruction_STA_ZP = 0x85,
	emu_vmInstruction_STX_ZP = 0x86,
	emu_vmInstruction_STY_ABS = 0x8c,
	emu_vmInstruction_STA_ABS = 0x8D,
	emu_vmInstruction_STX_ABS = 0x8e,
	emu_vmInstruction_STA_IZY = 0x91,
	emu_vmInstruction_STY_ZPX = 0x94,
	emu_vmInstruction_STA_ZPX = 0x95,
	emu_vmInstruction_STX_ZPY = 0x96,
	emu_vmInstruction_STA_ABY = 0x99,
	emu_vmInstruction_STA_ABX = 0x9D,
	// -- Load Instructions --
	emu_vmInstruction_LDY_IMM = 0xA0,
	emu_vmInstruction_LDA_IZX = 0xA1,
	emu_vmInstruction_LDX_IMM = 0xA2,
	emu_vmInstruction_LDY_ZP = 0xA4,
	emu_vmInstruction_LDA_ZP = 0xA5,
	emu_vmInstruction_LDX_ZP = 0xA6,
	emu_vmInstruction_LDA_IMM = 0xA9,
	emu_vmInstruction_LDY_ABS = 0xAC,
	emu_vmInstruction_LDA_ABS = 0xAD,
	emu_vmInstruction_LDX_ABS = 0xAE,
	emu_vmInstruction_LDA_IZY = 0xB1,
	emu_vmInstruction_LDY_ZPX = 0xB4,
	emu_vmInstruction_LDA_ZPX = 0xB5,
	emu_vmInstruction_LDX_ZPY = 0xB6,
	emu_vmInstruction_LDA_ABY = 0xB9,
	emu_vmInstruction_LDY_ABX = 0xBC,
	emu_vmInstruction_LDA_ABX = 0xBD,
	emu_vmInstruction_LDX_ABY = 0xBE,
	// -- JMP instructions --
	emu_vmInstruction_JMP_IND = 0x6C,
	// -- Compare instructions --
	emu_vmInstruction_CMP_IZX = 0xC1,
	emu_vmInstruction_CMP_ZP = 0xC5,
	emu_vmInstruction_CMP_IMM = 0xC9,
	emu_vmInstruction_CMP_ABS = 0xCD,
	emu_vmInstruction_CMP_IZY = 0xD1,
	emu_vmInstruction_CMP_ZPX = 0xD5,
	emu_vmInstruction_CMP_ABY = 0xD9,
	emu_vmInstruction_CMP_ABX = 0xDD,
	// -- CPX (Compare X) Instructions --
	emu_vmInstruction_CPX_IMM = 0xE0,
	emu_vmInstruction_CPX_ZP = 0xE4,
	emu_vmInstruction_CPX_ABS = 0xEC,
	// -- CPY (Compare Y) Instructions --
	emu_vmInstruction_CPY_IMM = 0xC0,
	emu_vmInstruction_CPY_ZP = 0xC4,
	emu_vmInstruction_CPY_ABS = 0xCC,
	// -- DEC Instructions --
	emu_vmInstruction_DEC_ZP = 0xC6,
	emu_vmInstruction_DEC_ZPX = 0xD6,
	emu_vmInstruction_DEC_ABS = 0xCE,
	emu_vmInstruction_DEC_ABX = 0xDE,
	// -- DEX/DEY (Decrement X/Y) Instructions --
	emu_vmInstruction_DEX_IMP = 0xCA,
	emu_vmInstruction_DEY_IMP = 0x88,
	// -- INC Instructions
	emu_vmInstruction_INC_ZP = 0xE6,
	emu_vmInstruction_INC_ZPX = 0xF6,
	emu_vmInstruction_INC_ABS = 0xEE,
	emu_vmInstruction_INC_ABX = 0xFE,
	// -- INX/INY (Increment X/Y) Instructions --
	emu_vmInstruction_INX_IMP = 0xE8,
	emu_vmInstruction_INY_IMP = 0xC8,
	// -- ASL (Arithmetic Shift Left) Instructions --
	emu_vmInstruction_ASL_IMP = 0x0A,
	emu_vmInstruction_ASL_ZP = 0x06,
	emu_vmInstruction_ASL_ZPX = 0x16,
	emu_vmInstruction_ASL_ABS = 0x0E,
	emu_vmInstruction_ASL_ABX = 0x1E,
	// -- Branch instructions --
	emu_vmInstruction_BCC_REL = 0x90,

	// NOP that we'll use as a flag
	emu_vmInstruction_ILLEGAL = 0xFA,
} emu_vmInstruction;

typedef enum emu_vmError
{
	emu_vmError_None = 0,
	emu_vmError_NotEnoughROM,
	emu_vmError_NullVm,
	emu_vmError_NullProgram,
	emu_vmError_EmptyProgram,
	emu_vmError_IllegalOpcode,
	emu_vmError_Break,
} emu_vmError;

typedef enum emu_vmStatus
{
	emu_vmStatus_Carry            = 0x1 << 0,
	emu_vmStatus_Zero             = 0x1 << 1,
	emu_vmStatus_InterruptDisable = 0x1 << 2,
	emu_vmStatus_Decimal          = 0x1 << 3,
	emu_vmStatus_B                = 0x1 << 4,
	emu_vmStatus_1                = 0x1 << 5,
	emu_vmStatus_Overflow         = 0x1 << 6,
	emu_vmStatus_Negative         = 0x1 << 7
} emu_vmStatus;

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

void emu_vm_initDebug();
void emu_vm_printOpcodes(uint8* program, size_t programSize);

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

const char* emu_vm_instructionToString(emu_vmInstruction instruction);

uint8 emu_vm_getStatus(emu_virtualMachine* vm, emu_vmStatus status);
void emu_vm_setStatus(emu_virtualMachine* vm, emu_vmStatus status);
void emu_vm_clearStatus(emu_virtualMachine* vm, emu_vmStatus status);

#endif
