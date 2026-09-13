#include "Emulator/App.h"
#include "Emulator/Debugger.h"
#include "Emulator/VirtualMachine.h"
#include "Emulator/Assembler.h"
#include "Emulator/Types.h"
#include "utils/FileHelper.h"
#include "frontend/SdlWrapper.h"
#include "frontend/NuklearLayer.h"

#include <stdio.h>
#include <conio.h>

#include <tree_sitter/api.h>
#include <assert.h>
#include <string.h>

// Declare the `tree_sitter_asm6502` function, which is
// implemented by the `tree-sitter-asm6502` library.
const TSLanguage* tree_sitter_asm6502(void);

int testTreeSitter(char* source_code)
{
	// Create a parser.
	TSParser* parser = ts_parser_new();

	// Set the parser's language (JSON in this case).
	ts_parser_set_language(parser, tree_sitter_asm6502());

	// Build a syntax tree based on source code stored in a string.
	TSTree* tree = ts_parser_parse_string(
		parser,
		NULL,
		source_code,
		(uint32)strlen(source_code)
	);

	// Get the root node of the syntax tree.
	TSNode root_node = ts_tree_root_node(tree);

	// Get some child nodes.
	//TSNode array_node = ts_node_named_child(root_node, 0);
	//TSNode number_node = ts_node_named_child(array_node, 0);

	// Print the syntax tree as an S-expression.
	char* string = ts_node_string(root_node);
	printf("Syntax tree: %s\n", string);

	// Free all of the heap-allocated memory.
	free(string);
	ts_tree_delete(tree);
	ts_parser_delete(parser);
	return 0;
}


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

emu_assembler_program* emu_app_loadProgram(emu_app* app)
{
	// For now, let's just read a file and parse it?
	const char* programFile = "G:\\dev\\6502\\testProject\\tutorial\\05_subroutines.s";
	const char* outputFile = "G:\\dev\\6502\\testProject\\tutorial\\05_subroutines.bin";

	emu_file sourceCodeFile = { 0 };
	emu_file_read(programFile, &sourceCodeFile);
	printf("%s\n", sourceCodeFile.data);
	testTreeSitter(sourceCodeFile.data);

	emu_assembler_program program = emu_assembler_assembleProgram(&app->vm->mmap, programFile, UINT16_MAX);
	emu_file_write(outputFile, program.data, program.dataSize);
	//emu_vm_printOpcodes(program.program, program.size);

	emu_vm_loadProgram(app->vm, &program);
	emu_vm_resetMachine(app->vm);

	emu_assembler_program* res = g_memory_allocate(sizeof(emu_assembler_program));
	g_memory_copyMem(res, &program, sizeof(emu_assembler_program));
	return res;
}

emu_app* emu_app_init(bool initializeGuiLayers)
{
	emu_debugger* debugger = (emu_debugger*)g_memory_allocate(sizeof(emu_debugger));
	emu_virtualMachine* vm = (emu_virtualMachine*)g_memory_allocate(sizeof(emu_virtualMachine));

	emu_vm_initDebug();
	*debugger = emu_debugger_init();
	*vm = emu_vm_init(emu_vmType_NES);

	emu_app* res = g_memory_allocate(sizeof(emu_app));
	*res = (emu_app){
		.debugger = debugger,
		.vm = vm,
		.sdl = NULL,
		.nuklear = NULL,
	};

	if (initializeGuiLayers) 
	{
		res->sdl = emu_frontend_initAndCreateWindow();
		if (res->sdl)
		{
			res->nuklear = emu_nuklear_init(res->sdl);
		}
	}

	return res;
}

SDL_AppResult emu_app_handleEvent(emu_app* app, SDL_Event* event)
{
	if (event->type == SDL_EVENT_QUIT)
	{
		return SDL_APP_SUCCESS;
	}

	return emu_nuklear_handleEvent(app->nuklear, app->sdl, event);
}

SDL_AppResult emu_app_tick(emu_app* app)
{
	SDL_AppResult res = emu_frontend_tick(app->sdl);
	if (res != SDL_APP_CONTINUE)
	{
		return res;
	}

	emu_nuklear_tickBegin(app->nuklear, app->sdl);
	res = emu_nuklear_tickEnd(app->nuklear, app->sdl);
	return res;
}

void emu_app_free(emu_app* app)
{
	if (app)
	{
		emu_nuklear_free(app->nuklear);
		emu_frontend_free(app->sdl);

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