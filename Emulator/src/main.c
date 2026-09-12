#include "Emulator/App.h"

// Make sure to include implementations
#include "Emulator/VendorImpls.h"

#define SDL_MAIN_USE_CALLBACKS 1 /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[])
{
	bool runTui = false;

	g_memory_init(true, 32);
	emu_app* app = emu_app_init(!runTui);
	*appstate = app;

	if (runTui)
	{
		emu_assembler_program* program = emu_app_loadProgram(app);
		emu_app_runTuiMode(app, program);
		*appstate = NULL;
		return SDL_APP_SUCCESS;
	}

	if (!app->sdl || !app->nuklear)
	{
		emu_app_free(app);
		*appstate = NULL;
		return SDL_APP_FAILURE;
	}

	return SDL_APP_CONTINUE;
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event)
{
	return emu_app_handleEvent((emu_app*)appstate, event);
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void* appstate)
{
	return emu_app_tick((emu_app*)appstate);
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void* appstate, SDL_AppResult result)
{
	/* SDL will clean up the window/renderer for us. */
	emu_app_free((emu_app*)appstate);

	g_memory_dumpMemoryLeaks();
}
