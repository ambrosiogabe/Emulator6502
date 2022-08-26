#ifndef EMULATOR_DEBUGGER_H
#define EMULATOR_DEBUGGER_H

typedef struct emu_debugger {
	void* empty;
} emu_debugger;

emu_debugger emu_debugger_init();

void emu_debugger_update(emu_debugger* debugger);

void emu_debugger_free(emu_debugger* debugger);

#endif 