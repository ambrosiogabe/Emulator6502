#ifndef EMU_NUKLEAR_LAYER_H
#define EMU_NUKLEAR_LAYER_H
#include "utils/SafeVendor.h"
#include <SDL3/SDL.h>

typedef struct emu_sdl_wrapper emu_sdl_wrapper;

typedef struct emu_nuklear_layer
{
    struct nk_context* ctx;
    struct nk_colorf bg;
    enum nk_anti_aliasing AA;
} emu_nuklear_layer;

emu_nuklear_layer* emu_nuklear_init(emu_sdl_wrapper* sdl);
SDL_AppResult emu_nuklear_handleEvent(emu_nuklear_layer* layer, emu_sdl_wrapper* sdl, SDL_Event* event);
void emu_nuklear_tickBegin(emu_nuklear_layer* layer, emu_sdl_wrapper* sdl);
SDL_AppResult emu_nuklear_tickEnd(emu_nuklear_layer* layer, emu_sdl_wrapper* sdl);
void emu_nuklear_free(emu_nuklear_layer* layer);

#endif