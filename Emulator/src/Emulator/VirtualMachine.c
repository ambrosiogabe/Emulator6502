#include "Emulator/VirtualMachine.h"
#include "Emulator/NesBinaryFile.h"

#include <stdio.h>
#include <string.h>

//#define NTSC_VBLANK_CYCLE 29'780.0f
//#define NTSC_NS_PER_CYCLE 558.73
#define PAL_VBLANK_CYCLE 33'247.5f
#define PAL_NS_PER_CYCLE 601.47f

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
static void pushToStack(emu_virtualMachine* vm, uint8 byte);
static uint8 popFromStack(emu_virtualMachine* vm);
static void jumpToNonMaskableInterrupt(emu_virtualMachine* vm);
static void returnFromNonMaskableInterrupt(emu_virtualMachine* vm);

static uint16 getGlobalIndirectAddress(uint8 hi, uint8 lo, uint8 offset);
static uint16 getGlobalAddress(uint8 hi, uint8 lo);
static void writeDataToCpu(emu_virtualMachine* vm, uint16 address, uint8 value);
static uint8 getDataFromCpu(emu_virtualMachine* vm, uint16 address);

static uint8 getRegisterValue(emu_virtualMachine* vm, emu_vmInstruction instruction);
static void setRegisterValue(emu_virtualMachine* vm, emu_vmInstruction instruction, uint8 value);
static void setAbsRegisterValue(emu_virtualMachine* vm, emu_vmInstruction instruction);
static void storeAbsRamValue(emu_virtualMachine* vm, emu_vmInstruction instruction);
static void addWithCarry(emu_virtualMachine* vm, emu_vmInstruction, uint8 value);
static void subtractWithCarry(emu_virtualMachine* vm, emu_vmInstruction, uint8 value);
static void compare(emu_virtualMachine* vm, emu_vmInstruction, uint8 value);
static void decrement(emu_virtualMachine* vm, uint8 address);
static void increment(emu_virtualMachine* vm, uint8 address);
static void logicalOr(emu_virtualMachine* vm, emu_vmInstruction, uint8 value);
static void logicalAnd(emu_virtualMachine* vm, emu_vmInstruction, uint8 value);
static void logicalXor(emu_virtualMachine* vm, emu_vmInstruction, uint8 value);
static void arithmeticShiftLeft(emu_virtualMachine* vm, emu_vmInstruction, uint8 address);
static void rotateLeft(emu_virtualMachine* vm, emu_vmInstruction, uint8 address);
static void logicalShiftRight(emu_virtualMachine* vm, emu_vmInstruction, uint8 address);
static void rotateRight(emu_virtualMachine* vm, emu_vmInstruction, uint8 address);
static void bitComparison(emu_virtualMachine* vm, emu_vmInstruction instruction, uint8 value);

static void checkFlagStatuses(emu_virtualMachine* vm, uint8 flagsToCheck, uint8 value);
static void checkOverflowFlag(emu_virtualMachine* vm, int16 value);

#define INSTRUCTION_EXPANSION_BY_VALUE(caseName, function) \
case caseName:\
{\
  uint8 value = getNext(vm);\
  function(vm, instruction, value);\
}\
break

#define INSTRUCTION_EXPANSION_RAM(caseName, function) \
case caseName:\
{\
  uint8 address = getNext(vm);\
  emu_chip_cpuWriteAddress(vm, address);\
  emu_chip_setReadWrite(vm, emu_RW_Read);\
  emu_chip_cycle(vm);\
  uint8 data = emu_chip_cpuReadDataBus(vm);\
  function(vm, instruction, data);\
}\
break

#define INSTRUCTION_EXPANSION_LONG_RAM_VALUE(caseName, function) \
case caseName:\
{\
  uint8 lo = getNext(vm);\
  uint8 hi = getNext(vm);\
  uint16 globalAddress = (hi << 8) | lo;\
  emu_chip_cpuWriteAddress(vm, globalAddress);\
  emu_chip_setReadWrite(vm, emu_RW_Read);\
  emu_chip_cycle(vm);\
  uint8 data = emu_chip_cpuReadDataBus(vm);\
  function(vm, instruction, data);\
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

#define BRANCH_ON_STATUS_SET_BASE(caseName, status, op) \
case caseName:\
{\
  int8 relativeAddress = getNext(vm);\
\
  if (op emu_vm_getStatus(vm, emu_vmStatus_##status))\
  {\
	  vm->programCounter += relativeAddress;\
  }\
}\
break

#define BRANCH_ON_STATUS_SET(caseName, status) BRANCH_ON_STATUS_SET_BASE(caseName, status, !!)
#define BRANCH_ON_NOT_STATUS_SET(caseName, status) BRANCH_ON_STATUS_SET_BASE(caseName, status, !)

const char* emu_vmInstructions[EMU_MAX_INSTRUCTION_OPCODE + 1] = { 0 };
uint8 emu_instructionCycleCount[EMU_MAX_INSTRUCTION_OPCODE + 1] = { 0 };

int64 emu_vm_getFrameTimeNs()
{
	return (int64)(PAL_VBLANK_CYCLE * PAL_NS_PER_CYCLE);
}

void emu_vm_initMeta()
{
	// Initialize debug text
	for (size_t i = 0; i < EMU_MAX_INSTRUCTION_OPCODE; i++)
	{
		emu_vmInstructions[i] = "NULL";
	}

	emu_vmInstructions[emu_vmInstruction_BRK] = "BRK";
	emu_vmInstructions[emu_vmInstruction_CLC_IMP] = "CLC_IMP";
	emu_vmInstructions[emu_vmInstruction_CLI_IMP] = "CLI_IMP";
	emu_vmInstructions[emu_vmInstruction_CLD_IMP] = "CLD_IMP";
	emu_vmInstructions[emu_vmInstruction_CLV_IMP] = "CLV_IMP";
	emu_vmInstructions[emu_vmInstruction_SEC_IMP] = "SEC_IMP";
	emu_vmInstructions[emu_vmInstruction_SEI_IMP] = "SEI_IMP";
	emu_vmInstructions[emu_vmInstruction_SED_IMP] = "SED_IMP";
	emu_vmInstructions[emu_vmInstruction_RTS_IMP] = "RTS_IMP";
	emu_vmInstructions[emu_vmInstruction_RTI_IMP] = "RTI_IMP";
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
	emu_vmInstructions[emu_vmInstruction_AND_IMM] = "AND_IMM";
	emu_vmInstructions[emu_vmInstruction_AND_ZP] = "AND_ZP";
	emu_vmInstructions[emu_vmInstruction_AND_ZPX] = "AND_ZPX";
	emu_vmInstructions[emu_vmInstruction_AND_IZX] = "AND_IZX";
	emu_vmInstructions[emu_vmInstruction_AND_IZY] = "AND_IZY";
	emu_vmInstructions[emu_vmInstruction_AND_ABS] = "AND_ABS";
	emu_vmInstructions[emu_vmInstruction_AND_ABX] = "AND_ABX";
	emu_vmInstructions[emu_vmInstruction_AND_ABY] = "AND_ABY";
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
	emu_vmInstructions[emu_vmInstruction_JSR_ABS] = "JSR_ABS";
	emu_vmInstructions[emu_vmInstruction_JMP_ABS] = "JMP_ABS";
	emu_vmInstructions[emu_vmInstruction_JMP_IND] = "JMP_IND";
	// -- BIT instructions --
	emu_vmInstructions[emu_vmInstruction_BIT_ZP] = "BIT_ZP";
	emu_vmInstructions[emu_vmInstruction_BIT_ABS] = "BIT_ABS";
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
	// -- Transfer Instructions --
	emu_vmInstructions[emu_vmInstruction_TAX_IMP] = "TAX_IMP";
	emu_vmInstructions[emu_vmInstruction_TXA_IMP] = "TXA_IMP";
	emu_vmInstructions[emu_vmInstruction_TAY_IMP] = "TAY_IMP";
	emu_vmInstructions[emu_vmInstruction_TYA_IMP] = "TYA_IMP";
	emu_vmInstructions[emu_vmInstruction_TSX_IMP] = "TSX_IMP";
	emu_vmInstructions[emu_vmInstruction_TXS_IMP] = "TXS_IMP";

	// NOP that we'll use as a flag
	emu_vmInstructions[emu_vmInstruction_NOP] = "NOP_IMP";

	// Initialize cycle count for all instructions
	emu_instructionCycleCount[emu_vmInstruction_BRK] = 7;
	emu_instructionCycleCount[emu_vmInstruction_CLC_IMP] = 2;
	emu_instructionCycleCount[emu_vmInstruction_CLI_IMP] = 2;
	emu_instructionCycleCount[emu_vmInstruction_CLD_IMP] = 2;
	emu_instructionCycleCount[emu_vmInstruction_CLV_IMP] = 2;
	emu_instructionCycleCount[emu_vmInstruction_SEC_IMP] = 2;
	emu_instructionCycleCount[emu_vmInstruction_SEI_IMP] = 2;
	emu_instructionCycleCount[emu_vmInstruction_SED_IMP] = 2;
	emu_instructionCycleCount[emu_vmInstruction_RTS_IMP] = 6;
	emu_instructionCycleCount[emu_vmInstruction_RTI_IMP] = 6;
	// -- OR instructions --
	emu_instructionCycleCount[emu_vmInstruction_ORA_IMM] = 2;
	emu_instructionCycleCount[emu_vmInstruction_ORA_ZP] = 3;
	emu_instructionCycleCount[emu_vmInstruction_ORA_ZPX] = 4;
	emu_instructionCycleCount[emu_vmInstruction_ORA_IZY] = 5; // TODO: Add one cycle if page boundary is crossed
	emu_instructionCycleCount[emu_vmInstruction_ORA_ABS] = 4;
	emu_instructionCycleCount[emu_vmInstruction_ORA_ABX] = 4; // TODO: Add one cycle if page boundary is crossed
	emu_instructionCycleCount[emu_vmInstruction_ORA_ABY] = 4; // TODO: Add one cycle if page boundary is crossed
	emu_instructionCycleCount[emu_vmInstruction_ORA_IZX] = 6;
	// -- AND instructions --
	emu_instructionCycleCount[emu_vmInstruction_AND_IMM] = 2;
	emu_instructionCycleCount[emu_vmInstruction_AND_ZP] = 3;
	emu_instructionCycleCount[emu_vmInstruction_AND_ZPX] = 4;
	emu_instructionCycleCount[emu_vmInstruction_AND_IZX] = 6;
	emu_instructionCycleCount[emu_vmInstruction_AND_IZY] = 5; // TODO: Add one cycle if page boundary is crossed
	emu_instructionCycleCount[emu_vmInstruction_AND_ABS] = 4;
	emu_instructionCycleCount[emu_vmInstruction_AND_ABX] = 4; // TODO: Add one cycle if page boundary is crossed
	emu_instructionCycleCount[emu_vmInstruction_AND_ABY] = 4; // TODO: Add one cycle if page boundary is crossed
	// -- XOR instructions --
	emu_instructionCycleCount[emu_vmInstruction_EOR_IMM] = 2;
	emu_instructionCycleCount[emu_vmInstruction_EOR_ZP] = 3;
	emu_instructionCycleCount[emu_vmInstruction_EOR_ZPX] = 4;
	emu_instructionCycleCount[emu_vmInstruction_EOR_IZX] = 6;
	emu_instructionCycleCount[emu_vmInstruction_EOR_IZY] = 5; // TODO: Add one cycle if page boundary is crossed
	emu_instructionCycleCount[emu_vmInstruction_EOR_ABS] = 4;
	emu_instructionCycleCount[emu_vmInstruction_EOR_ABX] = 4; // TODO: Add one cycle if page boundary is crossed
	emu_instructionCycleCount[emu_vmInstruction_EOR_ABY] = 4; // TODO: Add one cycle if page boundary is crossed
	// -- ADC instructions --
	emu_instructionCycleCount[emu_vmInstruction_ADC_IZX] = 6;
	emu_instructionCycleCount[emu_vmInstruction_ADC_ZP] = 3;
	emu_instructionCycleCount[emu_vmInstruction_ADC_IMM] = 2;
	emu_instructionCycleCount[emu_vmInstruction_ADC_ABS] = 4;
	emu_instructionCycleCount[emu_vmInstruction_ADC_IZY] = 5; // TODO: Add one cycle if page boundary is crossed
	emu_instructionCycleCount[emu_vmInstruction_ADC_ZPX] = 4;
	emu_instructionCycleCount[emu_vmInstruction_ADC_ABY] = 4; // TODO: Add one cycle if page boundary is crossed
	emu_instructionCycleCount[emu_vmInstruction_ADC_ABX] = 4; // TODO: Add one cycle if page boundary is crossed
	// -- SBC Instructions --
	emu_instructionCycleCount[emu_vmInstruction_SBC_IMM] = 2;
	emu_instructionCycleCount[emu_vmInstruction_SBC_ZP] = 3;
	emu_instructionCycleCount[emu_vmInstruction_SBC_ZPX] = 4;
	emu_instructionCycleCount[emu_vmInstruction_SBC_IZX] = 6;
	emu_instructionCycleCount[emu_vmInstruction_SBC_IZY] = 5; // TODO: Add one cycle if page boundary is crossed
	emu_instructionCycleCount[emu_vmInstruction_SBC_ABS] = 4;
	emu_instructionCycleCount[emu_vmInstruction_SBC_ABX] = 4; // TODO: Add one cycle if page boundary is crossed
	emu_instructionCycleCount[emu_vmInstruction_SBC_ABY] = 4; // TODO: Add one cycle if page boundary is crossed
	// -- Store Instructions --
	emu_instructionCycleCount[emu_vmInstruction_STA_IZX] = 6;
	emu_instructionCycleCount[emu_vmInstruction_STA_ZP] = 3;
	emu_instructionCycleCount[emu_vmInstruction_STA_ABS] = 4;
	emu_instructionCycleCount[emu_vmInstruction_STA_IZY] = 6;
	emu_instructionCycleCount[emu_vmInstruction_STA_ZPX] = 4;
	emu_instructionCycleCount[emu_vmInstruction_STA_ABY] = 5;
	emu_instructionCycleCount[emu_vmInstruction_STA_ABX] = 5;
	emu_instructionCycleCount[emu_vmInstruction_STY_ZP] = 3;
	emu_instructionCycleCount[emu_vmInstruction_STY_ABS] = 4;
	emu_instructionCycleCount[emu_vmInstruction_STY_ZPX] = 4;
	emu_instructionCycleCount[emu_vmInstruction_STX_ZP] = 3;
	emu_instructionCycleCount[emu_vmInstruction_STX_ABS] = 4;
	emu_instructionCycleCount[emu_vmInstruction_STX_ZPY] = 4;
	// -- Load Instructions --
	emu_instructionCycleCount[emu_vmInstruction_LDA_IZX] = 6;
	emu_instructionCycleCount[emu_vmInstruction_LDA_ZP] = 3;
	emu_instructionCycleCount[emu_vmInstruction_LDA_IMM] = 2;
	emu_instructionCycleCount[emu_vmInstruction_LDA_ABS] = 4;
	emu_instructionCycleCount[emu_vmInstruction_LDA_IZY] = 5; // TODO: Add one cycle if page boundary is crossed
	emu_instructionCycleCount[emu_vmInstruction_LDA_ZPX] = 4;
	emu_instructionCycleCount[emu_vmInstruction_LDA_ABY] = 4; // TODO: Add one cycle if page boundary is crossed
	emu_instructionCycleCount[emu_vmInstruction_LDA_ABX] = 4; // TODO: Add one cycle if page boundary is crossed
	emu_instructionCycleCount[emu_vmInstruction_LDX_IMM] = 2;
	emu_instructionCycleCount[emu_vmInstruction_LDX_ZP] = 3;
	emu_instructionCycleCount[emu_vmInstruction_LDX_ABS] = 4;
	emu_instructionCycleCount[emu_vmInstruction_LDX_ZPY] = 4;
	emu_instructionCycleCount[emu_vmInstruction_LDX_ABY] = 4; // TODO: Add one cycle if page boundary is crossed
	emu_instructionCycleCount[emu_vmInstruction_LDY_IMM] = 2;
	emu_instructionCycleCount[emu_vmInstruction_LDY_ZP] = 3;
	emu_instructionCycleCount[emu_vmInstruction_LDY_ABS] = 4;
	emu_instructionCycleCount[emu_vmInstruction_LDY_ZPX] = 4;
	emu_instructionCycleCount[emu_vmInstruction_LDY_ABX] = 4; // TODO: Add one cycle if page boundary is crossed
	// -- JMP instructions --
	emu_instructionCycleCount[emu_vmInstruction_JSR_ABS] = 6;
	emu_instructionCycleCount[emu_vmInstruction_JMP_ABS] = 3;
	emu_instructionCycleCount[emu_vmInstruction_JMP_IND] = 5; // TODO: For CMOS add 1 to cycle if address is at page boundary
	// -- BIT instructions --
	emu_instructionCycleCount[emu_vmInstruction_BIT_ZP] = 3;
	emu_instructionCycleCount[emu_vmInstruction_BIT_ABS] = 4;
	// -- Compare instructions --
	emu_instructionCycleCount[emu_vmInstruction_CMP_IZX] = 6;
	emu_instructionCycleCount[emu_vmInstruction_CMP_ZP] = 3;
	emu_instructionCycleCount[emu_vmInstruction_CMP_IMM] = 2;
	emu_instructionCycleCount[emu_vmInstruction_CMP_ABS] = 4;
	emu_instructionCycleCount[emu_vmInstruction_CMP_IZY] = 5; // TODO: Add one cycle if page boundary is crossed
	emu_instructionCycleCount[emu_vmInstruction_CMP_ZPX] = 4;
	emu_instructionCycleCount[emu_vmInstruction_CMP_ABY] = 4; // TODO: Add one cycle if page boundary is crossed
	emu_instructionCycleCount[emu_vmInstruction_CMP_ABX] = 4; // TODO: Add one cycle if page boundary is crossed
	// -- CPX (Compare X) Instructions --
	emu_instructionCycleCount[emu_vmInstruction_CPX_IMM] = 2;
	emu_instructionCycleCount[emu_vmInstruction_CPX_ZP] = 3;
	emu_instructionCycleCount[emu_vmInstruction_CPX_ABS] = 4;
	// -- CPY (Compare Y) Instructions --
	emu_instructionCycleCount[emu_vmInstruction_CPY_IMM] = 2;
	emu_instructionCycleCount[emu_vmInstruction_CPY_ZP] = 3;
	emu_instructionCycleCount[emu_vmInstruction_CPY_ABS] = 4;
	// -- DEC Instructions --
	emu_instructionCycleCount[emu_vmInstruction_DEC_ZP] = 5;
	emu_instructionCycleCount[emu_vmInstruction_DEC_ZPX] = 6;
	emu_instructionCycleCount[emu_vmInstruction_DEC_ABS] = 6;
	emu_instructionCycleCount[emu_vmInstruction_DEC_ABX] = 7;
	// -- DEX/DEY (Decrement X/Y) Instructions --
	emu_instructionCycleCount[emu_vmInstruction_DEX_IMP] = 2;
	emu_instructionCycleCount[emu_vmInstruction_DEY_IMP] = 2;
	// -- INC Instructions
	emu_instructionCycleCount[emu_vmInstruction_INC_ZP] = 5;
	emu_instructionCycleCount[emu_vmInstruction_INC_ZPX] = 6;
	emu_instructionCycleCount[emu_vmInstruction_INC_ABS] = 6;
	emu_instructionCycleCount[emu_vmInstruction_INC_ABX] = 7;
	// -- INX/INY (Increment X/Y) Instructions --
	emu_instructionCycleCount[emu_vmInstruction_INX_IMP] = 2;
	emu_instructionCycleCount[emu_vmInstruction_INY_IMP] = 2;
	// -- ASL (Arithmetic Shift Left) Instructions --
	emu_instructionCycleCount[emu_vmInstruction_ASL_IMP] = 2;
	emu_instructionCycleCount[emu_vmInstruction_ASL_ZP] = 5;
	emu_instructionCycleCount[emu_vmInstruction_ASL_ZPX] = 6;
	emu_instructionCycleCount[emu_vmInstruction_ASL_ABS] = 6;
	emu_instructionCycleCount[emu_vmInstruction_ASL_ABX] = 7;
	// -- ROL (Rotate Left) Instructions --
	emu_instructionCycleCount[emu_vmInstruction_ROL_IMP] = 2;
	emu_instructionCycleCount[emu_vmInstruction_ROL_ZP] = 5;
	emu_instructionCycleCount[emu_vmInstruction_ROL_ZPX] = 6;
	emu_instructionCycleCount[emu_vmInstruction_ROL_ABS] = 6;
	emu_instructionCycleCount[emu_vmInstruction_ROL_ABX] = 7;
	// -- LSR (Logical Shift Right) Instructions --
	emu_instructionCycleCount[emu_vmInstruction_LSR_IMP] = 2;
	emu_instructionCycleCount[emu_vmInstruction_LSR_ZP] = 5;
	emu_instructionCycleCount[emu_vmInstruction_LSR_ZPX] = 6;
	emu_instructionCycleCount[emu_vmInstruction_LSR_ABS] = 6;
	emu_instructionCycleCount[emu_vmInstruction_LSR_ABX] = 7;
	// -- ROR (Rotate Right) Instructions --
	emu_instructionCycleCount[emu_vmInstruction_ROR_IMP] = 2;
	emu_instructionCycleCount[emu_vmInstruction_ROR_ZP] = 5;
	emu_instructionCycleCount[emu_vmInstruction_ROR_ZPX] = 6;
	emu_instructionCycleCount[emu_vmInstruction_ROR_ABS] = 6;
	emu_instructionCycleCount[emu_vmInstruction_ROR_ABX] = 7;
	// -- Branch instructions --
	emu_instructionCycleCount[emu_vmInstruction_BPL_REL] = 2; // Add 1 to cycle if branch is on same page. Add 2 if branch is to different page.
	emu_instructionCycleCount[emu_vmInstruction_BMI_REL] = 2; // Add 1 to cycle if branch is on same page. Add 2 if branch is to different page.
	emu_instructionCycleCount[emu_vmInstruction_BVC_REL] = 2; // Add 1 to cycle if branch is on same page. Add 2 if branch is to different page.
	emu_instructionCycleCount[emu_vmInstruction_BVS_REL] = 2; // Add 1 to cycle if branch is on same page. Add 2 if branch is to different page.
	emu_instructionCycleCount[emu_vmInstruction_BCC_REL] = 2; // Add 1 to cycle if branch is on same page. Add 2 if branch is to different page.
	emu_instructionCycleCount[emu_vmInstruction_BCS_REL] = 2; // Add 1 to cycle if branch is on same page. Add 2 if branch is to different page.
	emu_instructionCycleCount[emu_vmInstruction_BNE_REL] = 2; // Add 1 to cycle if branch is on same page. Add 2 if branch is to different page.
	emu_instructionCycleCount[emu_vmInstruction_BEQ_REL] = 2; // Add 1 to cycle if branch is on same page. Add 2 if branch is to different page.
	// -- Transfer Instructions --
	emu_instructionCycleCount[emu_vmInstruction_TAX_IMP] = 2;
	emu_instructionCycleCount[emu_vmInstruction_TXA_IMP] = 2;
	emu_instructionCycleCount[emu_vmInstruction_TAY_IMP] = 2;
	emu_instructionCycleCount[emu_vmInstruction_TYA_IMP] = 2;
	emu_instructionCycleCount[emu_vmInstruction_TSX_IMP] = 2;
	emu_instructionCycleCount[emu_vmInstruction_TXS_IMP] = 2;

	// NOP that we'll use as a flag
	emu_instructionCycleCount[emu_vmInstruction_NOP] = 2;
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
	emu_Ppu_MemoryMap ppuMmap = emu_mmap_newNesPpuMap();

	emu_virtualMachine vm = {
		.vmType = vmType,
		.programCounter = 0,
		.accumulatorReg = 0,
		.xReg = 0,
		.yReg = 0,
		.statusReg = 0,
		.stackPointer = 0,
		.cycleCount = 0,

		.mmap = memoryMap,
		.ppuMmap = ppuMmap,
		.cpuChip = { 0 },
		.ppuChip = { 0 },
		.staticRam = { 0 },
		.cartridge = { 0 },
		.chipEnable = 0,
	};
	return vm;
}

emu_vmError emu_vm_loadProgram(emu_virtualMachine* vm, emu_NesBinaryFile* program)
{
	// Check assertions
	if (vm == NULL)
	{
		return emu_vmError_NullVm;
	}
	
	// TODO: Move this to some sort of freeing thing for cartridges and stuff...
	// If a program is already loaded, unload it to load the new one
	if (vm->cartridge.prgRom)
	{
		g_memory_free(vm->cartridge.prgRom);
		vm->cartridge.numPrgRomPages = 0;
	}

	if (vm->cartridge.chrRom)
	{
		g_memory_free(vm->cartridge.chrRom);
		vm->cartridge.numChrRomPages = 0;
	}

	// TODO: We're only working with mapper chip 0, NROM
	//       so we're hard coding how to load that ROM. In the future
	//       we need to properly emulate how to read/mirror PRG ROM
	uint8* romPtr = vm->mmap.physicalMemory + vm->mmap.rom.start;
	g_logger_assert(program->header.numPrgRomPages <= 2, "Cannot handle more than 2 ROM pages right now.");
	g_memory_copyMem(romPtr, program->prgRom, KB(16) * program->header.numPrgRomPages);
	// Mirror ROM if needed
	if (program->header.numPrgRomPages == 1)
	{
		g_memory_copyMem(KB(16) + romPtr, program->prgRom, KB(16));
	}

	// TODO: Make sure all ROM reads (including Program Counter) go through this chip
	size_t prgRomSize = program->header.numPrgRomPages * KB(16);
	vm->cartridge.numPrgRomPages = program->header.numPrgRomPages;
	vm->cartridge.prgRom = g_memory_allocate(prgRomSize);
	g_memory_copyMem(vm->cartridge.prgRom, program->prgRom, prgRomSize);
	
	size_t chrRomSize = program->header.numChrRomPages * KB(8);
	vm->cartridge.numChrRomPages = program->header.numChrRomPages;
	vm->cartridge.chrRom = g_memory_allocate(chrRomSize);
	g_memory_copyMem(vm->cartridge.chrRom, program->chrRom, chrRomSize);

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
	vm->stackPointer = 0xFF;
	vm->statusReg = 0;

	// NOTE: We don't do anything with ROM here because on reset, only ram should be cleared.
	g_memory_zeroMem(vm->staticRam.ram, sizeof(vm->staticRam.ram));

	// Read the reset vector into our program counter so we know where to start executing code.
	uint8 resetLo = getDataFromCpu(vm, vm->mmap.romv.start + 2);
	uint8 resetHi = getDataFromCpu(vm, vm->mmap.romv.start + 3);
	vm->programCounter = resetLo;
	vm->programCounter |= (resetHi << 8);

	return emu_vmError_None;
}

emu_vmError emu_vm_tick(emu_virtualMachine* vm)
{
	emu_vmInstruction instruction = fetchInstruction(vm);
	if (instruction == emu_vmInstruction_NOP)
	{
		return emu_vmError_Nop;
	}
	else if (instruction == emu_vmInstruction_BRK)
	{
		return emu_vmError_Break;
	}
	executeInstruction(vm, instruction);

	// TODO: Add special case handling for page boundary crossing
	vm->cycleCount += emu_instructionCycleCount[instruction];

	// TODO: Make PAL vs NTSC timings configurable via editor
	if (vm->cycleCount >= PAL_VBLANK_CYCLE)
	{
		// Trigger NMI, right now we don't actually display anything to the screen, but we will eventually
		// When we trigger an NMI, we need to set bit 7 of $2002 to 1
		vm->ppuChip.status = (uint8)0b10000000;
		jumpToNonMaskableInterrupt(vm);
		vm->cycleCount = 0;
		return emu_vmError_Vblank;
	}

	return emu_vmError_None;
}

void emu_vm_free(emu_virtualMachine* vm)
{
	if (vm)
	{
		emu_mmap_free(&vm->mmap);

		if (vm->cartridge.prgRom)
		{
			g_memory_free(vm->cartridge.prgRom);
		}

		if (vm->cartridge.chrRom)
		{
			g_memory_free(vm->cartridge.chrRom);
		}
	}

	g_memory_zeroMem(vm, sizeof(emu_virtualMachine));
}

const char* emu_vm_instructionToString(emu_vmInstruction instruction)
{
	return emu_vmInstructions[instruction];
}

const char* emu_vm_disassembleInstruction(emu_vmInstruction instruction)
{
	static char instructionTxt[4];
	const char* strToCopy = emu_vmInstructions[instruction];
	g_memory_copyMem(instructionTxt, (char*)strToCopy, 3);
	instructionTxt[3] = '\0';
	return instructionTxt;
}

uint8 emu_vm_instructionNumArgs(emu_vmInstruction instruction)
{
	if (instruction >= EMU_MAX_INSTRUCTION_OPCODE)
	{
		return 0;
	}

	char instructionType[5];
	const char* fullInstructionTxt = emu_vmInstructions[instruction];
	// Copy last 3 chars of instruction into instructionType
	size_t fullInstructionTxtLen = strlen(fullInstructionTxt);
	// Get pointer to _ in instruction
	const char* underscorePtr = fullInstructionTxt + fullInstructionTxtLen;
	for (int i = (int)fullInstructionTxtLen; i >= 0; i--)
	{
		if (fullInstructionTxt[i] == '_')
		{
			break;
		}
		underscorePtr = fullInstructionTxt + i;
	}

	int suffixLength = (int)(fullInstructionTxt + fullInstructionTxtLen - underscorePtr);
	if (suffixLength >= 4)
	{
		suffixLength = 3;
	}
	g_logger_assert(suffixLength < 4, "Invalid");
	g_memory_copyMem(instructionType, (char*)underscorePtr, suffixLength);
	instructionType[suffixLength] = '\0';

	// Now instruction type should be ZP, ABS, ABX, ...
	if (strcmp(instructionType, "ZP") == 0)
	{
		return 1;
	}

	if (strcmp(instructionType, "ZPX") == 0)
	{
		return 2;
	}

	if (strcmp(instructionType, "IMP") == 0)
	{
		return 0;
	}

	if (strcmp(instructionType, "ABS") == 0)
	{
		return 2;
	}

	if (strcmp(instructionType, "ABX") == 0)
	{
		return 2;
	}

	if (strcmp(instructionType, "ABY") == 0)
	{
		return 2;
	}

	if (strcmp(instructionType, "IMM") == 0)
	{
		return 1;
	}

	if (strcmp(instructionType, "REL") == 0)
	{
		return 1;
	}

	return 0;
}

uint8 emu_vm_getStatus(emu_virtualMachine* vm, emu_vmStatus status)
{
	return (uint8)(vm->statusReg & status) / (uint8)status;
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
	case emu_vmInstruction_STA_ZP:
	case emu_vmInstruction_STX_ZP:
	case emu_vmInstruction_STY_ZP:
	{
		uint8 address = getNext(vm);
		writeDataToCpu(vm, address, getRegisterValue(vm, instruction));
	}
	break;
	// Store ZP indirect
	case emu_vmInstruction_STA_ZPX:
	{
		uint8 address = getNext(vm);
		uint16 globalAddress = getGlobalIndirectAddress(0, address, vm->xReg);
		writeDataToCpu(vm, globalAddress, getRegisterValue(vm, instruction));
	}
	break;
	// Decrement
	case emu_vmInstruction_DEC_ZP:
	{
		uint8 address = getNext(vm);
		decrement(vm, address);
	}
	break;
	// Increment
	case emu_vmInstruction_INC_ZP:
	{
		uint8 address = getNext(vm);
		increment(vm, address);
	}
	break;
	// Store absolute
	case emu_vmInstruction_STA_ABX:
	case emu_vmInstruction_STA_ABS:
	case emu_vmInstruction_STX_ABS:
		storeAbsRamValue(vm, instruction);
		break;
		// Load absolute
	case emu_vmInstruction_LDA_ABX:
	case emu_vmInstruction_LDA_ABS:
		setAbsRegisterValue(vm, instruction);
		break;
		// Load
		INSTRUCTION_EXPANSION_RAM(emu_vmInstruction_LDA_ZP, setRegisterValue);
		INSTRUCTION_EXPANSION_RAM(emu_vmInstruction_LDX_ZP, setRegisterValue);
		INSTRUCTION_EXPANSION_RAM(emu_vmInstruction_LDY_ZP, setRegisterValue);
		INSTRUCTION_EXPANSION_BY_VALUE(emu_vmInstruction_LDA_IMM, setRegisterValue);
		INSTRUCTION_EXPANSION_BY_VALUE(emu_vmInstruction_LDX_IMM, setRegisterValue);
		INSTRUCTION_EXPANSION_BY_VALUE(emu_vmInstruction_LDY_IMM, setRegisterValue);
		// Add with carry
		INSTRUCTION_EXPANSION_RAM(emu_vmInstruction_ADC_ZP, addWithCarry);
		INSTRUCTION_EXPANSION_BY_VALUE(emu_vmInstruction_ADC_IMM, addWithCarry);
		// Sub with carry
		INSTRUCTION_EXPANSION_RAM(emu_vmInstruction_SBC_ZP, subtractWithCarry);
		INSTRUCTION_EXPANSION_BY_VALUE(emu_vmInstruction_SBC_IMM, subtractWithCarry);
		// Compare
		INSTRUCTION_EXPANSION_RAM(emu_vmInstruction_CMP_ZP, compare);
		INSTRUCTION_EXPANSION_BY_VALUE(emu_vmInstruction_CMP_IMM, compare);
		INSTRUCTION_EXPANSION_RAM(emu_vmInstruction_CPX_ZP, compare);
		INSTRUCTION_EXPANSION_BY_VALUE(emu_vmInstruction_CPX_IMM, compare);
		INSTRUCTION_EXPANSION_RAM(emu_vmInstruction_CPY_ZP, compare);
		INSTRUCTION_EXPANSION_BY_VALUE(emu_vmInstruction_CPY_IMM, compare);
		// Logical OR
		INSTRUCTION_EXPANSION_RAM(emu_vmInstruction_ORA_ZP, logicalOr);
		INSTRUCTION_EXPANSION_BY_VALUE(emu_vmInstruction_ORA_IMM, logicalOr);
		// Logical AND
		INSTRUCTION_EXPANSION_RAM(emu_vmInstruction_AND_ZP, logicalAnd);
		INSTRUCTION_EXPANSION_BY_VALUE(emu_vmInstruction_AND_IMM, logicalAnd);
		// Logical XOR
		INSTRUCTION_EXPANSION_RAM(emu_vmInstruction_EOR_ZP, logicalXor);
		INSTRUCTION_EXPANSION_BY_VALUE(emu_vmInstruction_EOR_IMM, logicalXor);
		// Arithmetic Shift Left
		INSTRUCTION_EXPANSION_BY_VALUE(emu_vmInstruction_ASL_ZP, arithmeticShiftLeft);
		// Rotate Left
		INSTRUCTION_EXPANSION_BY_VALUE(emu_vmInstruction_ROL_ZP, rotateLeft);
		// Logical Shift Right
		INSTRUCTION_EXPANSION_BY_VALUE(emu_vmInstruction_LSR_ZP, logicalShiftRight);
		// Rotate Right
		INSTRUCTION_EXPANSION_BY_VALUE(emu_vmInstruction_ROR_ZP, rotateRight);
		// Comparison instructions
		BRANCH_ON_STATUS_SET(emu_vmInstruction_BMI_REL, Negative);
		BRANCH_ON_STATUS_SET(emu_vmInstruction_BVS_REL, Overflow);
		BRANCH_ON_STATUS_SET(emu_vmInstruction_BCS_REL, Carry);
		BRANCH_ON_STATUS_SET(emu_vmInstruction_BEQ_REL, Zero);
		BRANCH_ON_NOT_STATUS_SET(emu_vmInstruction_BPL_REL, Negative);
		BRANCH_ON_NOT_STATUS_SET(emu_vmInstruction_BVC_REL, Overflow);
		BRANCH_ON_NOT_STATUS_SET(emu_vmInstruction_BCC_REL, Carry);
		BRANCH_ON_NOT_STATUS_SET(emu_vmInstruction_BNE_REL, Zero);
		// Bit instructions
		INSTRUCTION_EXPANSION_RAM(emu_vmInstruction_BIT_ZP, bitComparison);
		INSTRUCTION_EXPANSION_LONG_RAM_VALUE(emu_vmInstruction_BIT_ABS, bitComparison);

		// Rotate instructions
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

		// Transfer operations
	case emu_vmInstruction_TAX_IMP:
		vm->xReg = vm->accumulatorReg;
		checkFlagStatuses(vm, emu_vmStatus_Negative | emu_vmStatus_Zero, vm->xReg);
		break;
	case emu_vmInstruction_TXA_IMP:
		vm->accumulatorReg = vm->xReg;
		checkFlagStatuses(vm, emu_vmStatus_Negative | emu_vmStatus_Zero, vm->accumulatorReg);
		break;
	case emu_vmInstruction_TAY_IMP:
		vm->yReg = vm->accumulatorReg;
		checkFlagStatuses(vm, emu_vmStatus_Negative | emu_vmStatus_Zero, vm->yReg);
		break;
	case emu_vmInstruction_TYA_IMP:
		vm->accumulatorReg = vm->yReg;
		checkFlagStatuses(vm, emu_vmStatus_Negative | emu_vmStatus_Zero, vm->accumulatorReg);
		break;
	case emu_vmInstruction_TSX_IMP:
		vm->xReg = vm->stackPointer;
		checkFlagStatuses(vm, emu_vmStatus_Negative | emu_vmStatus_Zero, vm->xReg);
		break;
	case emu_vmInstruction_TXS_IMP:
		vm->stackPointer = vm->xReg;
		break;

		// Increment/decrement operations
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

		// Jump instructions
	case emu_vmInstruction_JSR_ABS:
	{
		uint8 address0 = getNext(vm);
		uint8 address1 = getNext(vm);

		uint16 globalAddress = ((uint16)address1 << 8) | address0;

		// Store current program counter to our stack
		uint8 programCounter0 = vm->programCounter & 0xFF;
		uint8 programCounter1 = (vm->programCounter >> 8) & 0xFF;
		pushToStack(vm, programCounter0);
		pushToStack(vm, programCounter1);

		vm->programCounter = globalAddress;
	}
	break;
	case emu_vmInstruction_JMP_ABS:
	{
		uint8 address0 = getNext(vm);
		uint8 address1 = getNext(vm);

		uint16 globalAddress = ((uint16)address1 << 8) | address0;
		vm->programCounter = globalAddress;
	}
	break;
	case emu_vmInstruction_RTS_IMP:
	{
		// Get saved address from stack
		uint8 address1 = popFromStack(vm);
		uint8 address0 = popFromStack(vm);

		uint16 globalAddress = ((uint16)address1 << 8) | address0;
		vm->programCounter = globalAddress;
	}
	break;
	case emu_vmInstruction_RTI_IMP:
		returnFromNonMaskableInterrupt(vm);
		break;

		// Set/Clear status flags
	case emu_vmInstruction_CLC_IMP:
		emu_vm_clearStatus(vm, emu_vmStatus_Carry);
		break;
	case emu_vmInstruction_CLI_IMP:
		emu_vm_clearStatus(vm, emu_vmStatus_InterruptDisable);
		break;
	case emu_vmInstruction_CLD_IMP:
		emu_vm_clearStatus(vm, emu_vmStatus_Decimal);
		break;
	case emu_vmInstruction_CLV_IMP:
		emu_vm_clearStatus(vm, emu_vmStatus_Overflow);
		break;
	case emu_vmInstruction_SEC_IMP:
		emu_vm_setStatus(vm, emu_vmStatus_Carry);
		break;
	case emu_vmInstruction_SEI_IMP:
		emu_vm_setStatus(vm, emu_vmStatus_InterruptDisable);
		break;
	case emu_vmInstruction_SED_IMP:
		emu_vm_setStatus(vm, emu_vmStatus_Decimal);
		break;
	default:
		g_logger_error("Cannot execute instruction: '%s'", emu_vmInstructions[instruction]);
		break;
	}
}

static uint8 getNext(emu_virtualMachine* vm)
{
	uint8 nextInstruction = getDataFromCpu(vm, vm->programCounter);
	vm->programCounter++;
	return nextInstruction;
}

static void pushToStack(emu_virtualMachine* vm, uint8 byte)
{
	if (vm->stackPointer == 0)
	{
		g_logger_error("Stack overflow.");
	}

	emu_SRam* sram = &vm->staticRam;
	uint8* stack = sram->ram + STACK_BASE;
	stack[vm->stackPointer] = byte;
	vm->stackPointer--;
}

static uint8 popFromStack(emu_virtualMachine* vm)
{
	if (vm->stackPointer == 0xff)
	{
		g_logger_error("Stack underflow.");
		return 0;
	}

	emu_SRam* sram = &vm->staticRam;
	uint8* stack = sram->ram + STACK_BASE;
	uint8 res = stack[vm->stackPointer + 1];
	vm->stackPointer++;
	return res;
}

static void jumpToNonMaskableInterrupt(emu_virtualMachine* vm)
{
	// First fetch the nmi address from our hardware vectors
	uint8 address0 = getDataFromCpu(vm, vm->mmap.romv.start);
	uint8 address1 = getDataFromCpu(vm, vm->mmap.romv.start + 1);

	uint16 globalNmiAddress = ((uint16)address1 << 8) | address0;

	// Store current program counter to our stack
	uint8 programCounter0 = vm->programCounter & 0xFF;
	uint8 programCounter1 = (vm->programCounter >> 8) & 0xFF;
	pushToStack(vm, programCounter0);
	pushToStack(vm, programCounter1);

	// Push the status register to the stack
	pushToStack(vm, vm->statusReg);

	// Set interrupt flag
	emu_vm_setStatus(vm, emu_vmStatus_InterruptDisable);

	// Jump to the nmi location
	vm->programCounter = globalNmiAddress;
}

static void returnFromNonMaskableInterrupt(emu_virtualMachine* vm)
{
	// Get saved status register from stack
	uint8 statusReg = popFromStack(vm);
	vm->statusReg = statusReg;

	// Get saved address from stack
	uint8 address1 = popFromStack(vm);
	uint8 address0 = popFromStack(vm);

	uint16 globalAddress = ((uint16)address1 << 8) | address0;
	vm->programCounter = globalAddress;

	emu_vm_clearStatus(vm, emu_vmStatus_InterruptDisable);
}

static void writeDataToCpu(emu_virtualMachine* vm, uint16 address, uint8 value)
{
	emu_chip_cpuWriteAddress(vm, address);
	emu_chip_cpuWriteToDataBus(vm, value);
	emu_chip_setReadWrite(vm, emu_RW_Write);
	emu_chip_cycle(vm);
}

static uint8 getDataFromCpu(emu_virtualMachine* vm, uint16 address)
{
	emu_chip_cpuWriteAddress(vm, address);
	emu_chip_setReadWrite(vm, emu_RW_Read);
	emu_chip_cycle(vm);
	return emu_chip_cpuReadDataBus(vm);
}

static uint16 getGlobalAddress(uint8 hi, uint8 lo)
{
	return (uint16)((hi << 8) | lo);
}

static uint16 getGlobalIndirectAddress(uint8 hi, uint8 lo, uint8 offset)
{
	return getGlobalAddress(hi, lo) + offset;
}

static uint8 getRegisterValue(emu_virtualMachine* vm, emu_vmInstruction instruction)
{
	switch (instruction)
	{
	case emu_vmInstruction_STA_ZP:
	case emu_vmInstruction_CMP_ZP:
	case emu_vmInstruction_CMP_IMM:
	case emu_vmInstruction_STA_ZPX:
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
	case emu_vmInstruction_LDA_ABX:
	case emu_vmInstruction_LDA_ABS:
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

static void setAbsRegisterValue(emu_virtualMachine* vm, emu_vmInstruction instruction)
{
	uint8 lo = getNext(vm);
	uint8 hi = getNext(vm);

	uint8 offset = 0;
	if (instruction == emu_vmInstruction_LDA_ABX)
	{
		offset = vm->xReg;
	}

	uint16 globalAddress = getGlobalIndirectAddress(hi, lo, offset);
	setRegisterValue(vm, instruction, getDataFromCpu(vm, globalAddress));
}

static void storeAbsRamValue(emu_virtualMachine* vm, emu_vmInstruction instruction)
{
	uint8 lo = getNext(vm);
	uint8 hi = getNext(vm);

	switch (instruction)
	{
	case emu_vmInstruction_STA_ABX:
	{
		uint16 globalAddress = getGlobalIndirectAddress(hi, lo, vm->xReg);
		writeDataToCpu(vm, globalAddress, vm->accumulatorReg);
	}
	break;
	case emu_vmInstruction_STA_ABS:
	{
		uint16 globalAddress = getGlobalAddress(hi, lo);
		writeDataToCpu(vm, globalAddress, vm->accumulatorReg);
	}
	break;
	case emu_vmInstruction_STX_ABS:
	{
		uint16 globalAddress = getGlobalAddress(hi, lo);
		writeDataToCpu(vm, globalAddress, vm->xReg);
	}
	break;
	default:
		g_logger_error("Cannot set register value for instruction '%s'", emu_vmInstructions[instruction]);
		break;
	}
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

	int8 res = (int8)registerValue - (int8)value;
	checkFlagStatuses(vm, emu_vmStatus_Zero | emu_vmStatus_Negative, res);
}

static void decrement(emu_virtualMachine* vm, uint8 address)
{
	uint8 value = getDataFromCpu(vm, address);
	value--;
	writeDataToCpu(vm, address, value);
	checkFlagStatuses(vm, emu_vmStatus_Zero | emu_vmStatus_Negative, value);
}

static void increment(emu_virtualMachine* vm, uint8 address)
{
	uint8 value = getDataFromCpu(vm, address);
	value++;
	writeDataToCpu(vm, address, value);
	checkFlagStatuses(vm, emu_vmStatus_Zero | emu_vmStatus_Negative, value);
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

static void bitComparison(emu_virtualMachine* vm, emu_vmInstruction instruction, uint8 value)
{
	if (value & 0b10000000)
	{
		emu_vm_setStatus(vm, emu_vmStatus_Negative);
	}
	else
	{
		emu_vm_clearStatus(vm, emu_vmStatus_Negative);
	}

	if (value & 0b01000000)
	{
		emu_vm_setStatus(vm, emu_vmStatus_Overflow);
	}
	else
	{
		emu_vm_clearStatus(vm, emu_vmStatus_Overflow);
	}

	if ((value & vm->accumulatorReg) == 0)
	{
		emu_vm_setStatus(vm, emu_vmStatus_Zero);
	}
	else
	{
		emu_vm_clearStatus(vm, emu_vmStatus_Zero);
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