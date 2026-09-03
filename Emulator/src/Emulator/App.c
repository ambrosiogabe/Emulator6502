#include "Emulator/App.h"
#include "Emulator/Debugger.h"
#include "Emulator/VirtualMachine.h"
#include "Emulator/Assembler.h"
#include "Emulator/Types.h"

#include <cppUtils/cppUtils.h>
#include <stb/stb_ds.h>

emu_app emu_app_init()
{
	emu_debugger* debugger = (emu_debugger*)g_memory_allocate(sizeof(emu_debugger));
	emu_virtualMachine* vm = (emu_virtualMachine*)g_memory_allocate(sizeof(emu_virtualMachine));

	emu_vm_initDebug();
	*debugger = emu_debugger_init();
	*vm = emu_vm_init(emu_vmType_NES);

	emu_app app = {
		.debugger = debugger,
		.vm = vm
	};
	return app;
}

typedef struct HashMapTest
{
	char* key;
	uint8 value;
} HashMapTest;

void emu_app_run(emu_app* app)
{
	// For now, let's just read a file and parse it?
	const char* programFile = "G:\\dev\\6502\\testProject\\tutorial\\03_branching.s";

	emu_assembler_program program = emu_assembler_assembleProgram(programFile, KB(512));
	//emu_vm_printOpcodes(program.program, program.size);
	
	emu_vm_resetMachine(app->vm);
	emu_vm_loadProgram(app->vm, program.program, program.size);

	emu_vmError error = emu_vmError_None;
	while (error == emu_vmError_None)
	{
		error = emu_vm_tick(app->vm);
	}

	emu_assembler_free(&program);
}

void emu_app_free(emu_app* app)
{
	if (app)
	{
		if (app->debugger)
		{
			emu_debugger_free(app->debugger);
			g_memory_free(app->debugger);
			app->debugger = NULL;
		}

		if (app->vm)
		{
			emu_vm_free(app->vm);
			g_memory_free(app->vm);
			app->vm = NULL;
		}
	}
}