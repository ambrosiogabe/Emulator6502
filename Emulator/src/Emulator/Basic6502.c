#include "Emulator/Basic6502.h"
#include "Emulator/Bus.h"

// -------------- Internal functions/types -------------- 
typedef enum emu_opCode
{
	emu_opCode_None = 0, emu_opCode_XXX /* Unknown opcode */,
	emu_opCode_ADC, emu_opCode_AND, emu_opCode_ASL, emu_opCode_BCC,
	emu_opCode_BCS, emu_opCode_BEQ, emu_opCode_BIT, emu_opCode_BMI,
	emu_opCode_BNE, emu_opCode_BPL, emu_opCode_BRK, emu_opCode_BVC,
	emu_opCode_BVS, emu_opCode_CLC, emu_opCode_CLD, emu_opCode_CLI,
	emu_opCode_CLV, emu_opCode_CMP, emu_opCode_CPX, emu_opCode_CPY,
	emu_opCode_DEC, emu_opCode_DEX, emu_opCode_DEY, emu_opCode_EOR,
	emu_opCode_INC, emu_opCode_INX, emu_opCode_INY, emu_opCode_JMP,
	emu_opCode_JSR, emu_opCode_LDA, emu_opCode_LDX, emu_opCode_LDY,
	emu_opCode_LSR, emu_opCode_NOP, emu_opCode_ORA, emu_opCode_PHA,
	emu_opCode_PHP, emu_opCode_PLA, emu_opCode_PLP, emu_opCode_ROL,
	emu_opCode_ROR, emu_opCode_RTI, emu_opCode_RTS, emu_opCode_SBC,
	emu_opCode_SEC, emu_opCode_SED, emu_opCode_SEI, emu_opCode_STA,
	emu_opCode_STX, emu_opCode_STY, emu_opCode_TAX, emu_opCode_TAY,
	emu_opCode_TSX, emu_opCode_TXA, emu_opCode_TXS, emu_opCode_TYA,
	emu_opCodeLength
} emu_opCode;

typedef enum emu_addrMode
{
	emu_addrMode_None = 0,
	emu_addrMode_IMP, emu_addrMode_IMM,
	emu_addrMode_ZP0, emu_addrMode_ZPX,
	emu_addrMode_ZPY, emu_addrMode_REL,
	emu_addrMode_ABS, emu_addrMode_ABX,
	emu_addrMode_ABY, emu_addrMode_IND,
	emu_addrMode_IZX, emu_addrMode_IZY,
	emu_addrMode_Length
} emu_addrMode;

uint8 setAddrMode(emu_basic6502* cpu, emu_addrMode addrMode);
uint8 executeOpCode(emu_basic6502* cpu, emu_opCode opCode);

uint8 emu_basic6502_getFlag(emu_basic6502_statusFlag flag);
void emu_basic6502_setFlag(emu_basic6502_statusFlag flag, bool value);

typedef struct cpu_instruction
{
	const char* name;
	emu_opCode opCode;
	emu_addrMode addrMode;
	uint8 numCycles;

} cpu_instruction;

#define OP(op) emu_opCode_##op
#define ADDR(mode) emu_addrMode_##mode

// NOTE: "Stolen" and modified from https://github.com/OneLoneCoder/olcNES/blob/master/Part%232%20-%20CPU/olc6502.cpp
//       Thank you OLC
cpu_instruction lookup[] =
{
	{ "BRK", OP(BRK), ADDR(IMM), 7 },{ "ORA", OP(ORA), ADDR(IZX), 6 },{ "???", OP(XXX), ADDR(IMP), 2 },{ "???", OP(XXX), ADDR(IMP), 8 },{ "???", OP(NOP), ADDR(IMP), 3 },{ "ORA", OP(ORA), ADDR(ZP0), 3 },{ "ASL", OP(ASL), ADDR(ZP0), 5 },{ "???", OP(XXX), ADDR(IMP), 5 },{ "PHP", OP(PHP), ADDR(IMP), 3 },{ "ORA", OP(ORA), ADDR(IMM), 2 },{ "ASL", OP(ASL), ADDR(IMP), 2 },{ "???", OP(XXX), ADDR(IMP), 2 },{ "???", OP(NOP), ADDR(IMP), 4 },{ "ORA", OP(ORA), ADDR(ABS), 4 },{ "ASL", OP(ASL), ADDR(ABS), 6 },{ "???", OP(XXX), ADDR(IMP), 6 },
	{ "BPL", OP(BPL), ADDR(REL), 2 },{ "ORA", OP(ORA), ADDR(IZY), 5 },{ "???", OP(XXX), ADDR(IMP), 2 },{ "???", OP(XXX), ADDR(IMP), 8 },{ "???", OP(NOP), ADDR(IMP), 4 },{ "ORA", OP(ORA), ADDR(ZPX), 4 },{ "ASL", OP(ASL), ADDR(ZPX), 6 },{ "???", OP(XXX), ADDR(IMP), 6 },{ "CLC", OP(CLC), ADDR(IMP), 2 },{ "ORA", OP(ORA), ADDR(ABY), 4 },{ "???", OP(NOP), ADDR(IMP), 2 },{ "???", OP(XXX), ADDR(IMP), 7 },{ "???", OP(NOP), ADDR(IMP), 4 },{ "ORA", OP(ORA), ADDR(ABX), 4 },{ "ASL", OP(ASL), ADDR(ABX), 7 },{ "???", OP(XXX), ADDR(IMP), 7 },
	{ "JSR", OP(JSR), ADDR(ABS), 6 },{ "AND", OP(AND), ADDR(IZX), 6 },{ "???", OP(XXX), ADDR(IMP), 2 },{ "???", OP(XXX), ADDR(IMP), 8 },{ "BIT", OP(BIT), ADDR(ZP0), 3 },{ "AND", OP(AND), ADDR(ZP0), 3 },{ "ROL", OP(ROL), ADDR(ZP0), 5 },{ "???", OP(XXX), ADDR(IMP), 5 },{ "PLP", OP(PLP), ADDR(IMP), 4 },{ "AND", OP(AND), ADDR(IMM), 2 },{ "ROL", OP(ROL), ADDR(IMP), 2 },{ "???", OP(XXX), ADDR(IMP), 2 },{ "BIT", OP(BIT), ADDR(ABS), 4 },{ "AND", OP(AND), ADDR(ABS), 4 },{ "ROL", OP(ROL), ADDR(ABS), 6 },{ "???", OP(XXX), ADDR(IMP), 6 },
	{ "BMI", OP(BMI), ADDR(REL), 2 },{ "AND", OP(AND), ADDR(IZY), 5 },{ "???", OP(XXX), ADDR(IMP), 2 },{ "???", OP(XXX), ADDR(IMP), 8 },{ "???", OP(NOP), ADDR(IMP), 4 },{ "AND", OP(AND), ADDR(ZPX), 4 },{ "ROL", OP(ROL), ADDR(ZPX), 6 },{ "???", OP(XXX), ADDR(IMP), 6 },{ "SEC", OP(SEC), ADDR(IMP), 2 },{ "AND", OP(AND), ADDR(ABY), 4 },{ "???", OP(NOP), ADDR(IMP), 2 },{ "???", OP(XXX), ADDR(IMP), 7 },{ "???", OP(NOP), ADDR(IMP), 4 },{ "AND", OP(AND), ADDR(ABX), 4 },{ "ROL", OP(ROL), ADDR(ABX), 7 },{ "???", OP(XXX), ADDR(IMP), 7 },
	{ "RTI", OP(RTI), ADDR(IMP), 6 },{ "EOR", OP(EOR), ADDR(IZX), 6 },{ "???", OP(XXX), ADDR(IMP), 2 },{ "???", OP(XXX), ADDR(IMP), 8 },{ "???", OP(NOP), ADDR(IMP), 3 },{ "EOR", OP(EOR), ADDR(ZP0), 3 },{ "LSR", OP(LSR), ADDR(ZP0), 5 },{ "???", OP(XXX), ADDR(IMP), 5 },{ "PHA", OP(PHA), ADDR(IMP), 3 },{ "EOR", OP(EOR), ADDR(IMM), 2 },{ "LSR", OP(LSR), ADDR(IMP), 2 },{ "???", OP(XXX), ADDR(IMP), 2 },{ "JMP", OP(JMP), ADDR(ABS), 3 },{ "EOR", OP(EOR), ADDR(ABS), 4 },{ "LSR", OP(LSR), ADDR(ABS), 6 },{ "???", OP(XXX), ADDR(IMP), 6 },
	{ "BVC", OP(BVC), ADDR(REL), 2 },{ "EOR", OP(EOR), ADDR(IZY), 5 },{ "???", OP(XXX), ADDR(IMP), 2 },{ "???", OP(XXX), ADDR(IMP), 8 },{ "???", OP(NOP), ADDR(IMP), 4 },{ "EOR", OP(EOR), ADDR(ZPX), 4 },{ "LSR", OP(LSR), ADDR(ZPX), 6 },{ "???", OP(XXX), ADDR(IMP), 6 },{ "CLI", OP(CLI), ADDR(IMP), 2 },{ "EOR", OP(EOR), ADDR(ABY), 4 },{ "???", OP(NOP), ADDR(IMP), 2 },{ "???", OP(XXX), ADDR(IMP), 7 },{ "???", OP(NOP), ADDR(IMP), 4 },{ "EOR", OP(EOR), ADDR(ABX), 4 },{ "LSR", OP(LSR), ADDR(ABX), 7 },{ "???", OP(XXX), ADDR(IMP), 7 },
	{ "RTS", OP(RTS), ADDR(IMP), 6 },{ "ADC", OP(ADC), ADDR(IZX), 6 },{ "???", OP(XXX), ADDR(IMP), 2 },{ "???", OP(XXX), ADDR(IMP), 8 },{ "???", OP(NOP), ADDR(IMP), 3 },{ "ADC", OP(ADC), ADDR(ZP0), 3 },{ "ROR", OP(ROR), ADDR(ZP0), 5 },{ "???", OP(XXX), ADDR(IMP), 5 },{ "PLA", OP(PLA), ADDR(IMP), 4 },{ "ADC", OP(ADC), ADDR(IMM), 2 },{ "ROR", OP(ROR), ADDR(IMP), 2 },{ "???", OP(XXX), ADDR(IMP), 2 },{ "JMP", OP(JMP), ADDR(IND), 5 },{ "ADC", OP(ADC), ADDR(ABS), 4 },{ "ROR", OP(ROR), ADDR(ABS), 6 },{ "???", OP(XXX), ADDR(IMP), 6 },
	{ "BVS", OP(BVS), ADDR(REL), 2 },{ "ADC", OP(ADC), ADDR(IZY), 5 },{ "???", OP(XXX), ADDR(IMP), 2 },{ "???", OP(XXX), ADDR(IMP), 8 },{ "???", OP(NOP), ADDR(IMP), 4 },{ "ADC", OP(ADC), ADDR(ZPX), 4 },{ "ROR", OP(ROR), ADDR(ZPX), 6 },{ "???", OP(XXX), ADDR(IMP), 6 },{ "SEI", OP(SEI), ADDR(IMP), 2 },{ "ADC", OP(ADC), ADDR(ABY), 4 },{ "???", OP(NOP), ADDR(IMP), 2 },{ "???", OP(XXX), ADDR(IMP), 7 },{ "???", OP(NOP), ADDR(IMP), 4 },{ "ADC", OP(ADC), ADDR(ABX), 4 },{ "ROR", OP(ROR), ADDR(ABX), 7 },{ "???", OP(XXX), ADDR(IMP), 7 },
	{ "???", OP(NOP), ADDR(IMP), 2 },{ "STA", OP(STA), ADDR(IZX), 6 },{ "???", OP(NOP), ADDR(IMP), 2 },{ "???", OP(XXX), ADDR(IMP), 6 },{ "STY", OP(STY), ADDR(ZP0), 3 },{ "STA", OP(STA), ADDR(ZP0), 3 },{ "STX", OP(STX), ADDR(ZP0), 3 },{ "???", OP(XXX), ADDR(IMP), 3 },{ "DEY", OP(DEY), ADDR(IMP), 2 },{ "???", OP(NOP), ADDR(IMP), 2 },{ "TXA", OP(TXA), ADDR(IMP), 2 },{ "???", OP(XXX), ADDR(IMP), 2 },{ "STY", OP(STY), ADDR(ABS), 4 },{ "STA", OP(STA), ADDR(ABS), 4 },{ "STX", OP(STX), ADDR(ABS), 4 },{ "???", OP(XXX), ADDR(IMP), 4 },
	{ "BCC", OP(BCC), ADDR(REL), 2 },{ "STA", OP(STA), ADDR(IZY), 6 },{ "???", OP(XXX), ADDR(IMP), 2 },{ "???", OP(XXX), ADDR(IMP), 6 },{ "STY", OP(STY), ADDR(ZPX), 4 },{ "STA", OP(STA), ADDR(ZPX), 4 },{ "STX", OP(STX), ADDR(ZPY), 4 },{ "???", OP(XXX), ADDR(IMP), 4 },{ "TYA", OP(TYA), ADDR(IMP), 2 },{ "STA", OP(STA), ADDR(ABY), 5 },{ "TXS", OP(TXS), ADDR(IMP), 2 },{ "???", OP(XXX), ADDR(IMP), 5 },{ "???", OP(NOP), ADDR(IMP), 5 },{ "STA", OP(STA), ADDR(ABX), 5 },{ "???", OP(XXX), ADDR(IMP), 5 },{ "???", OP(XXX), ADDR(IMP), 5 },
	{ "LDY", OP(LDY), ADDR(IMM), 2 },{ "LDA", OP(LDA), ADDR(IZX), 6 },{ "LDX", OP(LDX), ADDR(IMM), 2 },{ "???", OP(XXX), ADDR(IMP), 6 },{ "LDY", OP(LDY), ADDR(ZP0), 3 },{ "LDA", OP(LDA), ADDR(ZP0), 3 },{ "LDX", OP(LDX), ADDR(ZP0), 3 },{ "???", OP(XXX), ADDR(IMP), 3 },{ "TAY", OP(TAY), ADDR(IMP), 2 },{ "LDA", OP(LDA), ADDR(IMM), 2 },{ "TAX", OP(TAX), ADDR(IMP), 2 },{ "???", OP(XXX), ADDR(IMP), 2 },{ "LDY", OP(LDY), ADDR(ABS), 4 },{ "LDA", OP(LDA), ADDR(ABS), 4 },{ "LDX", OP(LDX), ADDR(ABS), 4 },{ "???", OP(XXX), ADDR(IMP), 4 },
	{ "BCS", OP(BCS), ADDR(REL), 2 },{ "LDA", OP(LDA), ADDR(IZY), 5 },{ "???", OP(XXX), ADDR(IMP), 2 },{ "???", OP(XXX), ADDR(IMP), 5 },{ "LDY", OP(LDY), ADDR(ZPX), 4 },{ "LDA", OP(LDA), ADDR(ZPX), 4 },{ "LDX", OP(LDX), ADDR(ZPY), 4 },{ "???", OP(XXX), ADDR(IMP), 4 },{ "CLV", OP(CLV), ADDR(IMP), 2 },{ "LDA", OP(LDA), ADDR(ABY), 4 },{ "TSX", OP(TSX), ADDR(IMP), 2 },{ "???", OP(XXX), ADDR(IMP), 4 },{ "LDY", OP(LDY), ADDR(ABX), 4 },{ "LDA", OP(LDA), ADDR(ABX), 4 },{ "LDX", OP(LDX), ADDR(ABY), 4 },{ "???", OP(XXX), ADDR(IMP), 4 },
	{ "CPY", OP(CPY), ADDR(IMM), 2 },{ "CMP", OP(CMP), ADDR(IZX), 6 },{ "???", OP(NOP), ADDR(IMP), 2 },{ "???", OP(XXX), ADDR(IMP), 8 },{ "CPY", OP(CPY), ADDR(ZP0), 3 },{ "CMP", OP(CMP), ADDR(ZP0), 3 },{ "DEC", OP(DEC), ADDR(ZP0), 5 },{ "???", OP(XXX), ADDR(IMP), 5 },{ "INY", OP(INY), ADDR(IMP), 2 },{ "CMP", OP(CMP), ADDR(IMM), 2 },{ "DEX", OP(DEX), ADDR(IMP), 2 },{ "???", OP(XXX), ADDR(IMP), 2 },{ "CPY", OP(CPY), ADDR(ABS), 4 },{ "CMP", OP(CMP), ADDR(ABS), 4 },{ "DEC", OP(DEC), ADDR(ABS), 6 },{ "???", OP(XXX), ADDR(IMP), 6 },
	{ "BNE", OP(BNE), ADDR(REL), 2 },{ "CMP", OP(CMP), ADDR(IZY), 5 },{ "???", OP(XXX), ADDR(IMP), 2 },{ "???", OP(XXX), ADDR(IMP), 8 },{ "???", OP(NOP), ADDR(IMP), 4 },{ "CMP", OP(CMP), ADDR(ZPX), 4 },{ "DEC", OP(DEC), ADDR(ZPX), 6 },{ "???", OP(XXX), ADDR(IMP), 6 },{ "CLD", OP(CLD), ADDR(IMP), 2 },{ "CMP", OP(CMP), ADDR(ABY), 4 },{ "NOP", OP(NOP), ADDR(IMP), 2 },{ "???", OP(XXX), ADDR(IMP), 7 },{ "???", OP(NOP), ADDR(IMP), 4 },{ "CMP", OP(CMP), ADDR(ABX), 4 },{ "DEC", OP(DEC), ADDR(ABX), 7 },{ "???", OP(XXX), ADDR(IMP), 7 },
	{ "CPX", OP(CPX), ADDR(IMM), 2 },{ "SBC", OP(SBC), ADDR(IZX), 6 },{ "???", OP(NOP), ADDR(IMP), 2 },{ "???", OP(XXX), ADDR(IMP), 8 },{ "CPX", OP(CPX), ADDR(ZP0), 3 },{ "SBC", OP(SBC), ADDR(ZP0), 3 },{ "INC", OP(INC), ADDR(ZP0), 5 },{ "???", OP(XXX), ADDR(IMP), 5 },{ "INX", OP(INX), ADDR(IMP), 2 },{ "SBC", OP(SBC), ADDR(IMM), 2 },{ "NOP", OP(NOP), ADDR(IMP), 2 },{ "???", OP(SBC), ADDR(IMP), 2 },{ "CPX", OP(CPX), ADDR(ABS), 4 },{ "SBC", OP(SBC), ADDR(ABS), 4 },{ "INC", OP(INC), ADDR(ABS), 6 },{ "???", OP(XXX), ADDR(IMP), 6 },
	{ "BEQ", OP(BEQ), ADDR(REL), 2 },{ "SBC", OP(SBC), ADDR(IZY), 5 },{ "???", OP(XXX), ADDR(IMP), 2 },{ "???", OP(XXX), ADDR(IMP), 8 },{ "???", OP(NOP), ADDR(IMP), 4 },{ "SBC", OP(SBC), ADDR(ZPX), 4 },{ "INC", OP(INC), ADDR(ZPX), 6 },{ "???", OP(XXX), ADDR(IMP), 6 },{ "SED", OP(SED), ADDR(IMP), 2 },{ "SBC", OP(SBC), ADDR(ABY), 4 },{ "NOP", OP(NOP), ADDR(IMP), 2 },{ "???", OP(XXX), ADDR(IMP), 7 },{ "???", OP(NOP), ADDR(IMP), 4 },{ "SBC", OP(SBC), ADDR(ABX), 4 },{ "INC", OP(INC), ADDR(ABX), 7 },{ "???", OP(XXX), ADDR(IMP), 7 },
};

size_t lookupLength = sizeof(lookup) / sizeof(cpu_instruction);

void emu_basic6502_init(emu_basic6502* cpu)
{
	g_memory_zeroMem(cpu, sizeof(emu_basic6502));
}

uint8 emu_basic6502_read(emu_basic6502* const cpu, uint16 addr)
{
	return emu_bus_read(cpu->bus, addr, emu_readType_readOnly);
}

void emu_basic6502_write(emu_basic6502* const cpu, uint16 addr, uint8 data)
{
	emu_bus_write(cpu->bus, addr, data);
}

void emu_basic6502_clock(emu_basic6502* cpu)
{
	if (cpu->cycles == 0)
	{
		cpu->opcode = emu_basic6502_read(cpu, cpu->programCounter);
		cpu->programCounter++;

		// Get starting number of cycles
		g_logger_assert(cpu->opcode < lookupLength, "Invalid opcode: '%d'", cpu->opcode);
		cpu->cycles = lookup[cpu->opcode].numCycles;

		uint8 additionalCycle1 = setAddrMode(cpu, lookup[cpu->opcode].addrMode);
		uint8 additionalCycle2 = executeOpCode(cpu, lookup[cpu->opcode].opCode);

		cpu->cycles += (additionalCycle1 & additionalCycle2);
	}

	cpu->cycles--;
}

void emu_basic6502_reset(emu_basic6502* cpu)
{

}

void emu_basic6502_irq(emu_basic6502* cpu)
{

}

void emu_basic6502_nmi(emu_basic6502* cpu)
{

}

// -------------- Internal functions -------------- 
uint8 setAddrMode(emu_basic6502* cpu, emu_addrMode addrMode)
{
	switch (addrMode)
	{
		// Implied
		case ADDR(IMP):
		{
			cpu->fetched = cpu->accumulator;
			return 0;
		}
		case ADDR(IMM):
		{
			// With immediate mode, we're setting the address of our data to the next byte in our instruction
			// since the next byte is the data we're looking for
			cpu->absAddress = cpu->programCounter++;
			return 0;
		}
	}
}

uint8 executeOpCode(emu_basic6502* cpu, emu_opCode opCode)
{

}

uint8 emu_basic6502_getFlag(emu_basic6502_statusFlag flag)
{

}

void emu_basic6502_setFlag(emu_basic6502_statusFlag flag, bool value)
{

}

#undef OP
#undef ADDR
