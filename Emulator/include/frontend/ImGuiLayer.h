#ifndef EMU_IMGUI_LAYER_H
#define EMU_IMGUI_LAYER_H
#include <SDL3/SDL.h>

typedef struct emu_sdl_wrapper emu_sdl_wrapper;
typedef struct ImGuiContext_t ImGuiContext;

ImGuiContext* emu_cimgui_init(emu_sdl_wrapper* sdl);
SDL_AppResult emu_cimgui_handleEvent(SDL_Event* event);
void emu_cimgui_tickBegin(emu_sdl_wrapper* sdl);
SDL_AppResult emu_cimgui_tickEnd(emu_sdl_wrapper* sdl);
void emu_cimgui_free(ImGuiContext* ctx);

#endif