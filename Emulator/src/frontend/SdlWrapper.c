#include "frontend/SdlWrapper.h"
#include "utils/SafeVendor.h"
#include "Emulator/App.h"

#include <SDL3/SDL.h>

emu_sdl_wrapper* emu_frontend_initAndCreateWindow()
{
	emu_sdl_wrapper* wrapper = g_memory_allocate(sizeof(emu_sdl_wrapper));
	*wrapper = (emu_sdl_wrapper){ 0 };

	SDL_SetAppMetadata("6502 Emulator", "1.0", "emulator");

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
	{
		g_logger_error("Couldn't initialize SDL: %s", SDL_GetError());
		g_memory_free(wrapper);
		return NULL;
	}

	if (!SDL_CreateWindowAndRenderer("6502 Emulator", 1920, 1080, SDL_WINDOW_RESIZABLE, &wrapper->window, &wrapper->renderer))
	{
		g_logger_error("Couldn't create window/renderer: %s", SDL_GetError());
		g_memory_free(wrapper);
		return NULL;
	}


	if (!SDL_SetRenderVSync(wrapper->renderer, 1))
	{
		g_logger_error("SDL_SetRenderVSync failed: %s", SDL_GetError());
		g_memory_free(wrapper);
		return NULL;
	}

	return wrapper;
}

SDL_AppResult emu_frontend_tick(emu_sdl_wrapper* sdl)
{
	return SDL_APP_CONTINUE;  /* carry on with the program! */
}

SDL_AppResult emu_frontend_free(emu_sdl_wrapper* wrapper)
{
	if (wrapper)
	{
		SDL_DestroyRenderer(wrapper->renderer);
		SDL_DestroyWindow(wrapper->window);
		g_memory_free(wrapper);
	}

	/* SDL will clean up the window/renderer for us. */
	return SDL_APP_CONTINUE;
}