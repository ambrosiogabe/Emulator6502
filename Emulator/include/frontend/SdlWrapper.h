#ifndef EMU_SDL_WRAPPER_H
#define EMU_SDL_WRAPPER_H
#include <SDL3/SDL.h>

typedef struct emu_sdl_wrapper
{
	SDL_Window* window;
	SDL_Renderer* renderer;
} emu_sdl_wrapper;

emu_sdl_wrapper* emu_frontend_initAndCreateWindow();
SDL_AppResult emu_frontend_tick(emu_sdl_wrapper* wrapper);
SDL_AppResult emu_frontend_free(emu_sdl_wrapper* wrapper);

#endif