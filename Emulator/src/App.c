#include "Emulator/App.h"
#include "Emulator/Debugger.h"
#include "Emulator/VirtualMachine.h"

#include <cppUtils/cppUtils.h>

emu_app emu_app_init()
{
	emu_debugger* debugger = (emu_debugger*)g_memory_allocate(sizeof(emu_debugger));
	emu_virtualMachine* vm = (emu_virtualMachine*)g_memory_allocate(sizeof(emu_virtualMachine));

	*debugger = emu_debugger_init();
	*vm = emu_vm_init(emu_vmType_NES);

	emu_app app = {
		.debugger = debugger,
		.vm = vm
	};
	return app;
}

void emu_app_run(emu_app* app)
{
	//emu_vm_loadProgram(app->vm, program, programSize);
	
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