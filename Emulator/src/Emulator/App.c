#include "Emulator/App.h"
#include "Emulator/Debugger.h"
#include "Emulator/VirtualMachine.h"
#include "Emulator/Assembler.h"
#include "Emulator/Types.h"
#include "utils/FileHelper.h"
#include "utils/SafeVendor.h"

#include <stb/stb_ds.h>
#include <stdio.h>
#include <conio.h>

emu_app emu_app_init()
{
	emu_debugger* debugger = (emu_debugger*)g_memory_allocate(sizeof(emu_debugger));
	emu_virtualMachine* vm = (emu_virtualMachine*)g_memory_allocate(sizeof(emu_virtualMachine));

	emu_vm_initDebug();
	*debugger = emu_debugger_init();
	*vm = emu_vm_init(emu_vmType_NES);

	emu_app app = {
		.debugger = debugger,
		.vm = vm
	};
	return app;
}

typedef struct HashMapTest
{
	char* key;
	uint8 value;
} HashMapTest;

static void flushScanf()
{
	char c;
	while ((c = (char)getchar()) != '\n' && c != EOF);
}

void emu_app_run(emu_app* app)
{
	bool runInteractive = true;

	// For now, let's just read a file and parse it?
	const char* programFile = "G:\\dev\\6502\\testProject\\tutorial\\05_subroutines.s";
	const char* outputFile = "G:\\dev\\6502\\testProject\\tutorial\\05_subroutines.bin";

	emu_assembler_program program = emu_assembler_assembleProgram(&app->vm->mmap, programFile, UINT16_MAX);
	emu_file_write(outputFile, program.data, program.dataSize);
	//emu_vm_printOpcodes(program.program, program.size);

	emu_vm_loadProgram(app->vm, &program);
	emu_vm_resetMachine(app->vm);

	emu_vmError error = emu_vmError_None;
	uint8* memory = app->vm->mmap.physicalMemory;
	while (error == emu_vmError_None)
	{
		uint8 nextInstruction = memory[app->vm->programCounter];
		error = emu_vm_tick(app->vm);

		if (runInteractive && !error)
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

	emu_assembler_free(&program);
}

void emu_app_free(emu_app* app)
{
	if (app)
	{
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
	}
}