#ifndef EMU_SDL_WRAPPER_H
#define EMU_SDL_WRAPPER_H
#include <SDL3/SDL.h>

SDL_AppResult emu_frontend_initAndCreateWindow();
SDL_AppResult emu_frontend_tick();
SDL_AppResult emu_frontend_free();

#endif