#include <stdio.h>
#include <cppUtils/cppUtils.h>

#include "Emulator/VendorImpls.h"
#include "Emulator/App.h"

int main()
{
	g_memory_init(true, 32);

	emu_app app = emu_app_init();
	emu_app_run(&app);
	emu_app_free(&app);

	g_memory_dumpMemoryLeaks();
}