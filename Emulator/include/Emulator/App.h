#ifndef EMULATOR_APP_H
#define EMULATOR_APP_H

typedef struct emu_debugger emu_debugger;
typedef struct emu_virtualMachine emu_virtualMachine;

typedef struct emu_app
{
	emu_debugger* debugger;
	emu_virtualMachine* vm;
} emu_app;

emu_app emu_app_init();

void emu_app_run(emu_app* app);

void emu_app_free(emu_app* app);

#endif