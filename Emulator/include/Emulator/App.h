#ifndef EMULATOR_APP_H
#define EMULATOR_APP_H

typedef struct emu_debugger emu_debugger;
typedef struct emu_virtualMachine emu_virtualMachine;
typedef struct emu_assembler_program emu_assembler_program;

typedef struct emu_app
{
	emu_debugger* debugger;
	emu_virtualMachine* vm;
} emu_app;

emu_app emu_app_init();

emu_assembler_program* emu_app_loadProgram(emu_app* app);

void emu_app_free(emu_app* app);

void emu_app_runTuiMode(emu_app* app, emu_assembler_program* program);

#endif