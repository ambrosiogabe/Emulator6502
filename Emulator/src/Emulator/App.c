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
#include "frontend/CodeEditor.h"

#include <conio.h>
#include <stdio.h>

#include <stb/stb_ds.h>
#include <cyaml.h>

static void saveAppMetadata(const char* lastLoadedProject);
static void loadAppMetadata(emu_app* app);
static void freeAppData(emu_app_data* metadata);

static bool isAppPaused = false;
static const char* appLoadedProjectFile = "./appMetadata.yml";

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

void emu_app_loadProgram(emu_app* app, const char** files, size_t numFiles)
{
	if (app->program)
	{
		emu_assembler_free(app->program);
		g_memory_free(app->program);
		app->program = NULL;
	}

	emu_assembler_program program = emu_assembler_assembleAndLinkProgram(&app->vm->mmap, files, numFiles, UINT16_MAX);

	emu_vmError err = emu_vm_loadProgram(app->vm, &program);
	if (err != emu_vmError_None)
	{
		emu_ConsoleOutput_error("VM is not valid. Cannot debug.");
		return;
	}

	emu_vm_resetMachine(app->vm);

	emu_assembler_program* res = g_memory_allocate(sizeof(emu_assembler_program));
	g_memory_copyMem(res, &program, sizeof(emu_assembler_program));
	app->program = res;
	app->isDebugging = true;

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
	loadAppMetadata(res);

	if (initializeGuiLayers)
	{
		res->sdl = emu_frontend_initAndCreateWindow();
		if (res->sdl)
		{
			res->imgui = emu_cimgui_init(res, res->sdl);
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
		// Save project on exit
		emu_app_saveProject(app, app->appLoadedProjectFile);

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

		if (app->appLoadedProjectFile)
		{
			g_memory_free(app->appLoadedProjectFile);
		}

		freeAppData(&app->data);
		g_memory_free(app);
	}
}

void emu_app_saveProject(emu_app* app, const char* filename)
{
	cyaml_doc_t* doc = cyaml_doc_new();
	cyaml_node_t* root = cyaml_new_map(doc);
	cyaml_set_root(doc, root);

	cyaml_node_t* openFiles = cyaml_new_seq(doc);
	size_t numOpenFiles = emu_CodeEditor_getNumOpenFiles();
	for (size_t i = 0; i < numOpenFiles; i++)
	{
		cyaml_seq_push(openFiles, cyaml_new_cstr(doc, emu_CodeEditor_getFullFilepath(i)));
	}
	cyaml_map_set(doc, root, "openFiles", openFiles);

	if (app->data.sourceFiles)
	{
		cyaml_node_t* sourceFiles = cyaml_new_seq(doc);
		size_t numSourceFiles = stbds_arrlen(app->data.sourceFiles);
		for (size_t i = 0; i < numSourceFiles; i++)
		{
			const char* sourceFile = app->data.sourceFiles[i];
			cyaml_seq_push(sourceFiles, cyaml_new_cstr(doc, sourceFile));
		}
		cyaml_map_set(doc, root, "sourceFiles", sourceFiles);
	}

	if (app->data.projectDirectory)
	{
		cyaml_map_set(doc, root, "projectDirectory", cyaml_new_cstr(doc, app->data.projectDirectory));
	}

	char* output = cyaml_emit(doc, NULL, NULL);
	emu_file_write(filename, (uint8*)output, strlen(output));
	free(output);
	cyaml_free(doc);

	saveAppMetadata(filename);
}

void emu_app_loadProject(emu_app* app, const char* filename)
{
	freeAppData(&app->data);

	emu_file file;
	if (emu_file_read(filename, &file) == emu_fileResult_Success)
	{
		cyaml_error_t err;
		cyaml_doc_t* doc = cyaml_parse(file.data, file.data_size, NULL, &err);

		if (!doc)
		{
			g_logger_error("Parse error at line %u: %s", err.span.start_line, err.msg);
			return;
		}

		cyaml_node_t* root = cyaml_root(doc);
		cyaml_node_t* openFiles = cyaml_get(doc, root, "openFiles");
		if (openFiles)
		{
			uint32 numOpenFiles = cyaml_seq_len(openFiles);
			for (uint32 i = 0; i < numOpenFiles; i++)
			{
				cyaml_node_t* fileToLoad = cyaml_seq_get(openFiles, i);
				char* fileToLoadStr = cyaml_scalar_str(doc, fileToLoad);
				emu_CodeEditor_openFile(app, fileToLoadStr);
				free(fileToLoadStr);
			}
		}

		cyaml_node_t* sourceFiles = cyaml_get(doc, root, "sourceFiles");
		if (sourceFiles)
		{
			uint32 numSourceFiles = cyaml_seq_len(sourceFiles);
			for (uint32 i = 0; i < numSourceFiles; i++)
			{
				cyaml_node_t* sourceFileToLoad = cyaml_seq_get(sourceFiles, i);
				char* fileToLoadStr = cyaml_scalar_str(doc, sourceFileToLoad);
				stbds_arrpush(app->data.sourceFiles, g_strcpy(fileToLoadStr));
				free(fileToLoadStr);
			}
		}

		cyaml_node_t* prjDirectoryNode = cyaml_get(doc, root, "projectDirectory");
		if (prjDirectoryNode)
		{
			char* prjDirectory = cyaml_scalar_str(doc, prjDirectoryNode);
			size_t prjDirectoryLength = strlen(prjDirectory);

			char* copy = g_memory_allocate(prjDirectoryLength + 1);
			g_memory_copyMem(copy, prjDirectory, prjDirectoryLength);
			copy[prjDirectoryLength] = '\0';
			app->data.projectDirectory = copy;

			free(prjDirectory);
		}


		cyaml_free(doc);
		emu_file_free(&file);

		size_t filenameLength = strlen(filename);
		app->appLoadedProjectFile = g_memory_allocate(filenameLength + 1);
		g_memory_copyMem(app->appLoadedProjectFile, (char*)filename, filenameLength);
		app->appLoadedProjectFile[filenameLength] = '\0';
	}
	else
	{
		g_logger_error("Cannot load project '%s'.", filename);
	}
}

// ------------------------- Internal Definitions -------------------------
static void saveAppMetadata(const char* lastLoadedProject)
{
	cyaml_doc_t* doc = cyaml_doc_new();
	cyaml_node_t* root = cyaml_new_map(doc);
	cyaml_set_root(doc, root);

	cyaml_map_set(doc, root, "lastLoadedProject", cyaml_new_cstr(doc, lastLoadedProject));

	char* output = cyaml_emit(doc, NULL, NULL);
	emu_file_write(appLoadedProjectFile, (uint8*)output, strlen(output));
	free(output);
	cyaml_free(doc);
}

static void loadAppMetadata(emu_app* app)
{
	emu_file file;
	if (emu_file_read(appLoadedProjectFile, &file) == emu_fileResult_Success)
	{
		cyaml_error_t err;
		cyaml_doc_t* doc = cyaml_parse(file.data, file.data_size, NULL, &err);

		if (!doc)
		{
			g_logger_error("Parse error at line %u: %s", err.span.start_line, err.msg);
			return;
		}

		cyaml_node_t* root = cyaml_root(doc);
		cyaml_node_t* lastLoadedProject = cyaml_get(doc, root, "lastLoadedProject");
		char* lastLoadedProjectFile = cyaml_scalar_str(doc, lastLoadedProject);
		emu_app_loadProject(app, lastLoadedProjectFile);
		free(lastLoadedProjectFile);

		cyaml_free(doc);
		emu_file_free(&file);
	}
	else
	{
		g_logger_error("Cannot load app metadata.");
	}
}

static void freeAppData(emu_app_data* data)
{
	if (data)
	{
		if (data->projectDirectory)
		{
			g_memory_free(data->projectDirectory);
		}

		for (int i = 0; i < stbds_arrlen(data->sourceFiles); i++)
		{
			g_memory_free(data->sourceFiles[i]);
		}
		stbds_arrfree(data->sourceFiles);

		*data = (emu_app_data){ 0 };
	}
}