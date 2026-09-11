#include "Emulator/App.h"
#include "Emulator/Debugger.h"
#include "Emulator/VirtualMachine.h"
#include "Emulator/Assembler.h"
#include "Emulator/Types.h"
#include "utils/FileHelper.h"

// Make sure to include implementations
#include "Emulator/VendorImpls.h"

#include <stdio.h>
#include <conio.h>

#define SDL_MAIN_USE_CALLBACKS 1 /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

static void flushScanf()
{
	char c;
	while ((c = (char)getchar()) != '\n' && c != EOF);
}

/* We will use this renderer to draw into this window every frame. */
static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;
static emu_app app;

static void runTuiMode(uint8* memory)
{
	emu_vmError error = emu_vmError_None;
	while (error == emu_vmError_None)
	{
		uint8 nextInstruction = memory[app.vm->programCounter];
		error = emu_vm_tick(app.vm);

		if (!error)
		{
			printf(">| <%X>: '%s'\n", app.vm->programCounter, emu_vmInstructions[nextInstruction]);
			printf(">| Press S to VM Status, R to see ram, any other key to continue: ");
			char input = (char)_getch();
			printf("\n");

			if (input == 'S' || input == 's')
			{
				printf("\n");
				emu_vm_printStatusFlags(app.vm);
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
				emu_vm_printRam(app.vm, (uint16)address, (uint16)numBytes);
				printf("\n");
			}
		}
	}
}

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[])
{
	g_memory_init(true, 32);

	app = emu_app_init();
	bool runInteractive = false;

	// For now, let's just read a file and parse it?
	const char* programFile = "G:\\dev\\6502\\testProject\\tutorial\\05_subroutines.s";
	const char* outputFile = "G:\\dev\\6502\\testProject\\tutorial\\05_subroutines.bin";

	emu_assembler_program program = emu_assembler_assembleProgram(&app.vm->mmap, programFile, UINT16_MAX);
	emu_file_write(outputFile, program.data, program.dataSize);
	//emu_vm_printOpcodes(program.program, program.size);

	emu_vm_loadProgram(app.vm, &program);
	emu_vm_resetMachine(app.vm);

	if (runInteractive)
	{
		runTuiMode(app.vm->mmap.physicalMemory);
	}

	emu_assembler_free(&program);

	if (runInteractive)
	{
		return SDL_APP_SUCCESS;
	}

	SDL_SetAppMetadata("6502 Emulator", "1.0", "emulator");

	if (!SDL_Init(SDL_INIT_VIDEO))
	{
		g_logger_error("Couldn't initialize SDL: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}

	if (!SDL_CreateWindowAndRenderer("6502 Emulator", 1920, 1080, SDL_WINDOW_RESIZABLE, &window, &renderer))
	{
		g_logger_error("Couldn't create window/renderer: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}
	SDL_SetRenderLogicalPresentation(renderer, 640, 480, SDL_LOGICAL_PRESENTATION_LETTERBOX);

	return SDL_APP_CONTINUE;  /* carry on with the program! */
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
	const double now = ((double)SDL_GetTicks()) / 1000.0;  /* convert from milliseconds to seconds. */
	/* choose the color for the frame we will draw. The sine wave trick makes it fade between colors smoothly. */
	const float red = (float)(0.5 + 0.5 * SDL_sin(now));
	const float green = (float)(0.5 + 0.5 * SDL_sin(now + SDL_PI_D * 2 / 3));
	const float blue = (float)(0.5 + 0.5 * SDL_sin(now + SDL_PI_D * 4 / 3));
	SDL_SetRenderDrawColorFloat(renderer, red, green, blue, SDL_ALPHA_OPAQUE_FLOAT);  /* new color, full alpha. */

	/* clear the window to the draw color. */
	SDL_RenderClear(renderer);

	/* put the newly-cleared rendering on the screen. */
	SDL_RenderPresent(renderer);

	return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void* appstate, SDL_AppResult result)
{
	/* SDL will clean up the window/renderer for us. */
	emu_app_free(&app);

	g_memory_dumpMemoryLeaks();
}

emu_app emu_app_init()
{
	emu_debugger* debugger = (emu_debugger*)g_memory_allocate(sizeof(emu_debugger));
	emu_virtualMachine* vm = (emu_virtualMachine*)g_memory_allocate(sizeof(emu_virtualMachine));

	emu_vm_initDebug();
	*debugger = emu_debugger_init();
	*vm = emu_vm_init(emu_vmType_NES);

	emu_app res = {
		.debugger = debugger,
		.vm = vm
	};
	return res;
}

void emu_app_free(emu_app* a)
{
	if (a)
	{
		if (a->debugger)
		{
			emu_debugger_free(a->debugger);
			g_memory_free(a->debugger);
			a->debugger = NULL;
		}

		if (a->vm)
		{
			emu_vm_free(a->vm);
			g_memory_free(a->vm);
			a->vm = NULL;
		}
	}
}