#ifndef EMULATOR_APP_H
#define EMULATOR_APP_H
#include <stdbool.h>
#include <SDL3/SDL.h>

typedef struct emu_debugger emu_debugger;
typedef struct emu_virtualMachine emu_virtualMachine;
typedef struct emu_assembler_program emu_assembler_program;
typedef struct emu_sdl_wrapper emu_sdl_wrapper;
typedef struct emu_nuklear_layer emu_nuklear_layer;

typedef struct emu_app
{
	emu_debugger* debugger;
	emu_virtualMachine* vm;
	emu_sdl_wrapper* sdl;
	emu_nuklear_layer* nuklear;
} emu_app;

emu_app* emu_app_init(bool initializeGuiLayers);

emu_assembler_program* emu_app_loadProgram(emu_app* app);

SDL_AppResult emu_app_handleEvent(emu_app* app, SDL_Event* event);

SDL_AppResult emu_app_tick(emu_app* app);

void emu_app_free(emu_app* app);

void emu_app_runTuiMode(emu_app* app, emu_assembler_program* program);

#endif