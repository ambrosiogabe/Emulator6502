#include "utest.h"
#include "Emulator/VirtualMachine.h"

static emu_virtualMachine emu_runProgram(uint8* program, size_t programSize)
{
	emu_virtualMachine result = emu_vm_init(emu_vmType_NES);
	emu_vm_resetMachine(&result);
	emu_vm_loadProgram(&result, program, programSize);

	emu_vmError error = emu_vmError_None;
	while (error == emu_vmError_None)
	{
		error = emu_vm_tick(&result);
	}

	return result;
}

UTEST(VirtualMachine, AddWithCarry_HappyPath)
{
	uint8 program[] = {
		emu_vmInstruction_LDA_IMM,
		10,
		emu_vmInstruction_ADC_IMM,
		10
	};

	emu_virtualMachine machine = emu_runProgram(program, sizeof(program));

	ASSERT_EQ(machine.accumulatorReg, 20);
	for (int i = 0; i < 8; i++)
	{
		emu_vmStatus status = (emu_vmStatus)(1 << i);
		ASSERT_FALSE(emu_vm_getStatus(&machine, status));
	}
}

UTEST(VirtualMachine, AddWithCarry_UnsignedOverflow)
{
	uint8 program[] = {
		emu_vmInstruction_LDA_IMM,
		255,
		emu_vmInstruction_ADC_IMM,
		2
	};

	emu_virtualMachine machine = emu_runProgram(program, sizeof(program));

	ASSERT_EQ(machine.accumulatorReg, 1);
	ASSERT_TRUE(emu_vm_getStatus(&machine, emu_vmStatus_Carry));
	for (int i = 0; i < 8; i++)
	{
		emu_vmStatus status = (emu_vmStatus)(1 << i);
		if (status == emu_vmStatus_Carry) continue;
		ASSERT_FALSE(emu_vm_getStatus(&machine, status));
	}
}

UTEST(VirtualMachine, AddWithCarry_NegativeResult)
{
	uint8 program[] = {
		emu_vmInstruction_LDA_IMM,
		0x50,
		emu_vmInstruction_ADC_IMM,
		0x40
	};

	emu_virtualMachine machine = emu_runProgram(program, sizeof(program));

	ASSERT_EQ(machine.accumulatorReg, 0x90);
	ASSERT_TRUE(emu_vm_getStatus(&machine, emu_vmStatus_Negative));
	for (int i = 0; i < 8; i++)
	{
		emu_vmStatus status = (emu_vmStatus)(1 << i);
		if (status == emu_vmStatus_Negative) continue;
		ASSERT_FALSE(emu_vm_getStatus(&machine, status));
	}
}

UTEST(VirtualMachine, AddWithCarry_ZeroResult)
{
	uint8 program[] = {
		emu_vmInstruction_LDA_IMM,
		0xFF,
		emu_vmInstruction_ADC_IMM,
		0x1
	};

	emu_virtualMachine machine = emu_runProgram(program, sizeof(program));

	ASSERT_EQ(machine.accumulatorReg, 0x0);
	ASSERT_TRUE(emu_vm_getStatus(&machine, emu_vmStatus_Carry));
	ASSERT_TRUE(emu_vm_getStatus(&machine, emu_vmStatus_Zero));
	for (int i = 0; i < 8; i++)
	{
		emu_vmStatus status = (emu_vmStatus)(1 << i);
		if (status == emu_vmStatus_Zero) continue;
		if (status == emu_vmStatus_Carry) continue;
		ASSERT_FALSE(emu_vm_getStatus(&machine, status));
	}
}

UTEST(VirtualMachine, GetStatus)
{
	uint8 program[] = {
		emu_vmInstruction_ILLEGAL
	};

	emu_virtualMachine machine = emu_runProgram(program, sizeof(program));

	// Test simple case, when 1 bit is set
	for (int i = 0; i < 8; i++)
	{
		emu_vmStatus status = (emu_vmStatus)(1 << i);
		machine.statusReg = status;
		ASSERT_TRUE(emu_vm_getStatus(&machine, status));

		for (int j = 0; j < 8; j++)
		{
			if (j == i) continue;

			emu_vmStatus otherStatus = (emu_vmStatus)(1 << j);
			ASSERT_FALSE(emu_vm_getStatus(&machine, otherStatus));
		}
	}

	// Test multiple bits set
	machine.statusReg = emu_vmStatus_Negative | emu_vmStatus_Carry;
	ASSERT_TRUE(emu_vm_getStatus(&machine, emu_vmStatus_Negative));
	ASSERT_TRUE(emu_vm_getStatus(&machine, emu_vmStatus_Carry));
	ASSERT_FALSE(emu_vm_getStatus(&machine, emu_vmStatus_Zero));
	ASSERT_FALSE(emu_vm_getStatus(&machine, emu_vmStatus_InterruptDisable));
}

UTEST(VirtualMachine, SetStatus)
{
	uint8 program[] = {
		emu_vmInstruction_ILLEGAL
	};

	emu_virtualMachine machine = emu_runProgram(program, sizeof(program));

	// Test simple case, when 1 bit is set
	for (int i = 0; i < 8; i++)
	{
		emu_vmStatus status = (emu_vmStatus)(1 << i);
		emu_vm_setStatus(&machine, status);
		ASSERT_TRUE(emu_vm_getStatus(&machine, status));

		for (int j = 0; j < 8; j++)
		{
			if (j == i) continue;

			emu_vmStatus otherStatus = (emu_vmStatus)(1 << j);
			ASSERT_FALSE(emu_vm_getStatus(&machine, otherStatus));
		}

		emu_vm_clearStatus(&machine, status);
	}

	// Test multiple bits set
	emu_vm_setStatus(&machine, emu_vmStatus_Negative);
	emu_vm_setStatus(&machine, emu_vmStatus_Carry);
	ASSERT_TRUE(emu_vm_getStatus(&machine, emu_vmStatus_Negative));
	ASSERT_TRUE(emu_vm_getStatus(&machine, emu_vmStatus_Carry));
	ASSERT_FALSE(emu_vm_getStatus(&machine, emu_vmStatus_Zero));
	ASSERT_FALSE(emu_vm_getStatus(&machine, emu_vmStatus_InterruptDisable));
}

UTEST(VirtualMachine, ClearStatus)
{
	uint8 program[] = {
		emu_vmInstruction_ILLEGAL
	};

	emu_virtualMachine machine = emu_runProgram(program, sizeof(program));

	// Test simple case, when 1 bit is set
	for (int i = 0; i < 8; i++)
	{
		emu_vmStatus status = (emu_vmStatus)(1 << i);
		emu_vm_setStatus(&machine, status);
		ASSERT_TRUE(emu_vm_getStatus(&machine, status));

		for (int j = 0; j < 8; j++)
		{
			if (j == i) continue;

			emu_vmStatus otherStatus = (emu_vmStatus)(1 << j);
			ASSERT_FALSE(emu_vm_getStatus(&machine, otherStatus));
		}

		emu_vm_clearStatus(&machine, status);
		ASSERT_FALSE(emu_vm_getStatus(&machine, status));
	}

	// Test multiple bits set
	emu_vm_setStatus(&machine, emu_vmStatus_Negative);
	emu_vm_setStatus(&machine, emu_vmStatus_Carry);
	emu_vm_setStatus(&machine, emu_vmStatus_Zero);
	ASSERT_TRUE(emu_vm_getStatus(&machine, emu_vmStatus_Negative));
	ASSERT_TRUE(emu_vm_getStatus(&machine, emu_vmStatus_Carry));
	ASSERT_TRUE(emu_vm_getStatus(&machine, emu_vmStatus_Zero));
	ASSERT_FALSE(emu_vm_getStatus(&machine, emu_vmStatus_InterruptDisable));

	emu_vm_clearStatus(&machine, emu_vmStatus_Zero);
	ASSERT_TRUE(emu_vm_getStatus(&machine, emu_vmStatus_Negative));
	ASSERT_TRUE(emu_vm_getStatus(&machine, emu_vmStatus_Carry));
	ASSERT_FALSE(emu_vm_getStatus(&machine, emu_vmStatus_Zero));
	ASSERT_FALSE(emu_vm_getStatus(&machine, emu_vmStatus_InterruptDisable));
}