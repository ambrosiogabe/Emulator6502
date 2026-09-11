#include "Emulator/App.h"
#include "Emulator/Debugger.h"
#include "Emulator/VirtualMachine.h"
#include "Emulator/Assembler.h"
#include "Emulator/Types.h"
#include "utils/FileHelper.h"
#include "frontend/SdlWrapper.h"

// Make sure to include implementations
#include "Emulator/VendorImpls.h"

#include <stdio.h>
#include <conio.h>

#define SDL_MAIN_USE_CALLBACKS 1 /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

emu_app app;

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[])
{
	g_memory_init(true, 32);
	app = emu_app_init();

	bool runTui = false;

	if (runTui)
	{
		emu_assembler_program* program = emu_app_loadProgram(&app);
		emu_app_runTuiMode(&app, program);
		return SDL_APP_SUCCESS;
	}

	return emu_frontend_initAndCreateWindow();
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event)
{
	if (event->type == SDL_EVENT_QUIT)
	{
		return SDL_APP_SUCCESS;  /* end the program, reporting success to the OS. */
	}
	return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void* appstate)
{
	return emu_frontend_tick();
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void* appstate, SDL_AppResult result)
{
	emu_frontend_free();

	/* SDL will clean up the window/renderer for us. */
	emu_app_free(&app);

	g_memory_dumpMemoryLeaks();
}
