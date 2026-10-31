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

	wrapper->window = SDL_CreateWindow("6502 Emulator", 1920, 1080, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY);
	if (!wrapper->window)
	{
		g_logger_error("Couldn't create window: %s", SDL_GetError());
		g_memory_free(wrapper);
		return NULL;
	}

	wrapper->gpuDevice = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL, true, NULL);
	if (!SDL_ClaimWindowForGPUDevice(wrapper->gpuDevice, wrapper->window))
	{
		g_logger_info("Failed to claim window: %s", SDL_GetError());
		g_memory_free(wrapper);
		return NULL;
	}

	if (!SDL_SetGPUSwapchainParameters(wrapper->gpuDevice, wrapper->window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC))
	{
		g_logger_error("SDL_SetGPUSwapchainParameters failed: %s", SDL_GetError());
		g_memory_free(wrapper);
		return NULL;
	}

	SDL_SetWindowPosition(wrapper->window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
	SDL_MaximizeWindow(wrapper->window);
	SDL_ShowWindow(wrapper->window);

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