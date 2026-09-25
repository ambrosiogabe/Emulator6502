#include "Emulator/App.h"
#include "Emulator/Debugger.h"
#include "Emulator/VirtualMachine.h"
#include "Emulator/Assembler.h"
#include "Emulator/Types.h"
#include "utils/FileHelper.h"
#include "frontend/SdlWrapper.h"
#include "frontend/ImGuiLayer.h"
#include "frontend/SyntaxHighlighter.h"
#include "frontend/ConsoleOutput.h"
#include "frontend/EmulatorDebug.h"

#include <stdio.h>
#include <conio.h>

#include <tree_sitter/api.h>
#include <assert.h>
#include <string.h>

static bool isAppPaused = false;

static void flushScanf()
{
	char c;
	while ((c = (char)getchar()) != '\n' && c != EOF);
}

void emu_app_runTuiMode(emu_app* app, emu_assembler_program* program)
{
	emu_vmError error = emu_vmError_None;
	uint8* memory = app->vm->mmap.physicalMemory;
	while (error == emu_vmError_None)
	{
		uint8 nextInstruction = memory[app->vm->programCounter];
		error = emu_vm_tick(app->vm);

		if (!error)
		{
			printf(">| <%X>: '%s'\n", app->vm->programCounter, emu_vmInstructions[nextInstruction]);
			printf(">| Press S to VM Status, R to see ram, any other key to continue: ");
			char input = (char)_getch();
			printf("\n");

			if (input == 'S' || input == 's')
			{
				printf("\n");
				emu_vm_printStatusFlags(app->vm);
				printf("\n");
			}
			else if (input == 'R' || input == 'r')
			{
				printf(">| RAM Address: $0x");
				uint32 address;
				if (!scanf("%x", &address))
				{
					printf("Invalid address.");
					continue;
				}

				flushScanf();

				printf(">| Number of bytes: ");
				int32 numBytes;
				if (!scanf("%d", &numBytes))
				{
					printf("Invalid number of bytes");
					continue;
				}
				if (numBytes <= 0)
				{
					printf("Invalid number of bytes");
					continue;
				}

				flushScanf();
				printf("\n");
				emu_vm_printRam(app->vm, (uint16)address, (uint16)numBytes);
				printf("\n");
			}
		}
	}

	emu_assembler_free(program);
	g_memory_free(program);
}

void emu_app_loadProgram(emu_app* app, const char* fullFilepath)
{
	if (app->program)
	{
		emu_assembler_free(app->program);
		g_memory_free(app->program);
		app->program = NULL;
	}

	emu_assembler_program program = emu_assembler_assembleProgram(&app->vm->mmap, fullFilepath, UINT16_MAX);

	emu_vmError err = emu_vm_loadProgram(app->vm, &program);
	if (err != emu_vmError_None)
	{
		emu_ConsoleOutput_error("VM is not valid. Cannot debug.", fullFilepath);
		return;
	}

	emu_vm_resetMachine(app->vm);

	emu_assembler_program* res = g_memory_allocate(sizeof(emu_assembler_program));
	g_memory_copyMem(res, &program, sizeof(emu_assembler_program));
	app->program = res;
	app->isDebugging = true;

	emu_vm_loadProgram(app->vm, res);
	emu_EmulatorDebug_beginDebugging(app);
}

emu_app* emu_app_init(bool initializeGuiLayers)
{
	emu_debugger* debugger = (emu_debugger*)g_memory_allocate(sizeof(emu_debugger));
	emu_virtualMachine* vm = (emu_virtualMachine*)g_memory_allocate(sizeof(emu_virtualMachine));

	emu_vm_initDebug();
	emu_SyntaxHighlighter_init();
	*debugger = emu_debugger_init();
	*vm = emu_vm_init(emu_vmType_NES);

	emu_app* res = g_memory_allocate(sizeof(emu_app));
	*res = (emu_app){
		.debugger = debugger,
		.vm = vm,
		.sdl = NULL,
		.program = NULL,
		.isDebugging = true,
	};

	if (initializeGuiLayers) 
	{
		res->sdl = emu_frontend_initAndCreateWindow();
		if (res->sdl)
		{
			res->imgui = emu_cimgui_init(res->sdl);
		}
	}

	return res;
}

void emu_app_pauseApp()
{
	isAppPaused = true;
}

void emu_app_resumeApp()
{
	isAppPaused = false;
}

SDL_AppResult emu_app_handleEvent(emu_app* app, SDL_Event* event)
{
	if (isAppPaused)
	{
		return SDL_APP_CONTINUE;
	}

	if (event->type == SDL_EVENT_QUIT)
	{
		return SDL_APP_SUCCESS;
	}

	return emu_cimgui_handleEvent(event);
}

SDL_AppResult emu_app_tick(emu_app* app)
{
	if (isAppPaused)
	{
		return SDL_APP_CONTINUE;
	}

	SDL_AppResult res = emu_frontend_tick(app->sdl);
	if (res != SDL_APP_CONTINUE)
	{
		return res;
	}

	if (!app->isDebugging && app->vm)
	{
		if (emu_vm_tick(app->vm) != emu_vmError_None)
		{
			app->isDebugging = true;
		}
	}

	emu_cimgui_tickBegin(app);
	res = emu_cimgui_tickEnd(app->sdl);
	return res;
}

void emu_app_free(emu_app* app)
{
	if (app)
	{
		emu_cimgui_free(app->imgui);
		emu_SyntaxHighlighter_free();
		emu_frontend_free(app->sdl);

		if (app->program)
		{
			emu_assembler_free(app->program);
			g_memory_free(app->program);
		}

		if (app->debugger)
		{
			emu_debugger_free(app->debugger);
			g_memory_free(app->debugger);
			app->debugger = NULL;
		}

		if (app->vm)
		{
			emu_vm_free(app->vm);
			g_memory_free(app->vm);
			app->vm = NULL;
		}

		g_memory_free(app);
	}
}