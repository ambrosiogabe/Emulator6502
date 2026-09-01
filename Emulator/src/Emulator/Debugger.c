#include "Emulator/Debugger.h"

emu_debugger emu_debugger_init()
{
	emu_debugger dbg = { 0 };
	return dbg;
}

void emu_debugger_update(emu_debugger* debugger)
{
	// NOP
}

void emu_debugger_free(emu_debugger* debugger)
{
	// NOP
}