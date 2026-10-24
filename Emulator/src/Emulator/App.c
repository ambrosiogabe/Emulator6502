#include "Emulator/App.h"
#include "Emulator/Debugger.h"
#include "Emulator/VirtualMachine.h"
#include "Emulator/Assembler.h"
#include "Emulator/Types.h"
#include "Emulator/NesBinaryFile.h"
#include "utils/FileHelper.h"
#include "frontend/SdlWrapper.h"
#include "frontend/ImGuiLayer.h"
#include "frontend/SyntaxHighlighter.h"
#include "frontend/ConsoleOutput.h"
#include "frontend/EmulatorDebug.h"
#include "frontend/CodeEditor.h"

#include <conio.h>
#include <stdio.h>
#include <time.h>

#include <stb/stb_ds.h>
#include <cyaml.h>

#include <Windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

#define BEGIN_CRITICAL_SECTION(lock) {\
DWORD waitResult_##lock = WaitForSingleObject(lock, INFINITE);\
if (waitResult_##lock == WAIT_OBJECT_0) {

#define END_CRITICAL_SECTION(lock)\
ReleaseMutex(lock);\
}}\

#define NS_TO_MS(nanoseconds) (nanoseconds / 1'000'000LL)
#define MS_TO_NS(milliseconds) (milliseconds * 1'000'000LL)

static HANDLE cpuThreadBreakFlagLock;
static bool cpuThreadBreakFlag = false;
static HANDLE shouldStepOverFlagLock;
static bool shouldStepOverFlag = false;

static CRITICAL_SECTION shouldCpuEmulateCriticalSection;
static CONDITION_VARIABLE shouldCpuEmulateCondition;
static bool shouldCpuEmulate = false;

static HANDLE cpuThread;

static void saveAppMetadata(const char* lastLoadedProject);
static void loadAppMetadata(emu_app* app);
static void freeAppData(emu_app_data* metadata);

static bool isAppPaused = false;
static const char* appLoadedProjectFile = "./appMetadata.yml";

// Helper to get current time in nanoseconds
static int64 getTimeNanoseconds()
{
	LARGE_INTEGER freq, counter;
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&counter);
	return (counter.QuadPart * 1000000000LL) / freq.QuadPart; // nanoseconds
}

static void preciseSleepFor(int64 numNanoseconds)
{
	int64 nowNs = getTimeNanoseconds();
	int64 timeToSleepTil = nowNs + numNanoseconds;
	while (timeToSleepTil - nowNs > 0)
	{
		nowNs = getTimeNanoseconds();
		int64 remainderNs = timeToSleepTil - nowNs;

		if (nowNs <= 0)
		{
			break;
		}

		// If more than 3ms left, sleep for (remainder - 2ms) to avoid overshooting
		if (NS_TO_MS(remainderNs) > 3)
		{

			Sleep((DWORD)(NS_TO_MS(remainderNs) - 2));
		}
		else
		{
			// Spin-lock for the final few milliseconds/microseconds
			_mm_pause(); // Hints to the CPU that we are in a spin-wait loop
		}
	}
}

static void flushScanf()
{
	char c;
	while ((c = (char)getchar()) != '\n' && c != EOF);
}


static DWORD emulatorCpuThread(LPVOID lpParam)
{
	// Tell windows to give us 1ms sleep latency
	timeBeginPeriod(1);

	bool isBreakFlagEnabled = false;

	EnterCriticalSection(&shouldCpuEmulateCriticalSection);

	int64 frameStart = getTimeNanoseconds();
	emu_virtualMachine* vm = (emu_virtualMachine*)lpParam;
	emu_vmError error = emu_vmError_None;
	while (error == emu_vmError_None || error == emu_vmError_Nop || error == emu_vmError_Break)
	{
		bool sleepTilFrameEnd = false;
		BEGIN_CRITICAL_SECTION(shouldStepOverFlagLock);
		BEGIN_CRITICAL_SECTION(cpuThreadBreakFlagLock);

		error = emu_vmError_None;
		isBreakFlagEnabled = cpuThreadBreakFlag;
		if (shouldStepOverFlag || !cpuThreadBreakFlag)
		{
			error = emu_vm_tick(vm);
			shouldStepOverFlag = false;

			if (error == emu_vmError_Break)
			{
				cpuThreadBreakFlag = true;
			}

			if (error == emu_vmError_Vblank)
			{
				sleepTilFrameEnd = true;
			}
		}

		END_CRITICAL_SECTION(cpuThreadBreakFlagLock);
		END_CRITICAL_SECTION(shouldStepOverFlagLock);

		if (sleepTilFrameEnd)
		{
			int64 timeNow = getTimeNanoseconds();
			int64 remainingFrameTime = (frameStart + emu_vm_getFrameTimeNs()) - timeNow;
			if (remainingFrameTime < emu_vm_getFrameTimeNs())
			{
				preciseSleepFor(remainingFrameTime);
			}
			else
			{
				if (!isBreakFlagEnabled)
				{
					g_logger_warning("Over shot frame time while not in debug mode.");
				}

				frameStart = emu_vm_getFrameTimeNs();
			}
			// Set frame start to exactly the start of the last frame plus the frame time
			// This will hopefully keep us on track even if we sleep for an imprecise amount of time
			frameStart += emu_vm_getFrameTimeNs();
			error = emu_vmError_None;
		}

		// If the break flag is enabled, we can sleep for 100ms every loop just to make sure we don't spin tons of cycles waiting for it to change
		if (isBreakFlagEnabled || !shouldCpuEmulate)
		{
			// This puts the thread to sleep efficiently.
			// It atomically releases the lock and waits. 
			// When woken up, it automatically re-acquires the lock.
			SleepConditionVariableCS(&shouldCpuEmulateCondition, &shouldCpuEmulateCriticalSection, INFINITE);
		}

		if (!shouldCpuEmulate)
		{
			error = emu_vmError_ExitProgram;
		}
	}

	LeaveCriticalSection(&shouldCpuEmulateCriticalSection);

	timeEndPeriod(1);
	return 0;
}

static void stopAndCleanUpCpuThread()
{
	if (cpuThread)
	{
		emu_app_debugBreak();
		EnterCriticalSection(&shouldCpuEmulateCriticalSection);
		shouldCpuEmulate = false;
		LeaveCriticalSection(&shouldCpuEmulateCriticalSection);

		// Wake up one thread waiting on this condition variable
		WakeConditionVariable(&shouldCpuEmulateCondition);

		WaitForSingleObject(cpuThread, INFINITE);

		// Clean up thread resources
		CloseHandle(cpuThread);
		DeleteCriticalSection(&shouldCpuEmulateCriticalSection);
		CloseHandle(shouldStepOverFlagLock);
		CloseHandle(cpuThreadBreakFlagLock);
		cpuThread = NULL;
		shouldCpuEmulateCriticalSection = (CRITICAL_SECTION){ 0 };
		shouldStepOverFlagLock = NULL;
		cpuThreadBreakFlagLock = NULL;
	}
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

void emu_app_loadProgram(emu_app* app)
{
	if (app->program)
	{
		emu_NesBinaryFile_free(app->program);
		g_memory_free(app->program);
		app->program = NULL;
	}

	// We'll assume max cartridge space, and then adjust accordingly once we have a completed program
	// that gives us a header telling what mapper it's expecting
	size_t prgRomSize = KB(16) * 2;
	size_t chrRomSize = KB(8);
	// 1. We assemble the program
	emu_assembler_program program = emu_assembler_assembleAndLinkProgram(&app->vm->mmap, app->data.sourceFiles, prgRomSize, chrRomSize);

	// 2. We generate a .nes binary file from our assembled program
	emu_NesBinaryFile_Raw binary = emu_NesBinaryFile_generateBinary(program.header, program.prgRom, program.prgRomSize, program.chrRom, program.chrRomSize, NULL, 0);
#define MAX_BIN_FILE_NAME_SIZE 1024
	char binFileName[MAX_BIN_FILE_NAME_SIZE];
	{
		// TODO: Add project name instead of using project directory as name of project
		size_t prjDirectoryLen = strlen(app->data.projectDirectory);
		g_memory_copyMem(binFileName, app->data.projectDirectory, prjDirectoryLen);
		binFileName[prjDirectoryLen] = '\0';
		strncat(binFileName, ".nes", MAX_BIN_FILE_NAME_SIZE - prjDirectoryLen);
		emu_file_write(binFileName, binary.bin, binary.binSize);
	}
#undef MAX_BIN_FILE_NAME_SIZE

	// 3. Then we'll load that binary file into a structured data source
	emu_file file;
	emu_file_read(binFileName, &file);
	emu_NesBinaryFile nesProgram = emu_NesBinaryFile_parse(&file);
	emu_file_free(&file);

	// 4. Finally, we'll load that program as we would if the user just opened a binary
	emu_vmError err = emu_vm_loadProgram(app->vm, &nesProgram);
	if (err != emu_vmError_None)
	{
		emu_ConsoleOutput_error("VM is not valid. Cannot debug.");
		return;
	}

	// 5. Free all resources used to generate the nes program
	emu_assembler_free(&program);
	emu_NesBinaryFile_freeRaw(&binary);

	emu_vm_resetMachine(app->vm);

	emu_NesBinaryFile* res = g_memory_allocate(sizeof(emu_NesBinaryFile));
	g_memory_zeroMem(res, sizeof(emu_NesBinaryFile));
	g_memory_copyMem(res, &nesProgram, sizeof(emu_NesBinaryFile));
	app->program = res;
	app->isDebugging = true;

	emu_EmulatorDebug_beginDebugging(app);

	// Only begin emulating if we're not already debugging
	if (!cpuThread)
	{
		// Initialize state, for now we'll initialize with the program already on break
		// until we add proper breakpoint support
		shouldStepOverFlag = false;
		cpuThreadBreakFlag = false;
		shouldCpuEmulate = true;

		// Create the locks for our thread
		shouldStepOverFlagLock = CreateMutexA(NULL, FALSE, NULL);
		cpuThreadBreakFlagLock = CreateMutexA(NULL, FALSE, NULL);

		InitializeCriticalSection(&shouldCpuEmulateCriticalSection);
		InitializeConditionVariable(&shouldCpuEmulateCondition);

		// Create thread for cpu to run on
		cpuThread = CreateThread(NULL, 0, emulatorCpuThread, app->vm, 0, NULL);
	}
	else
	{
		emu_ConsoleOutput_error("Cannot begin debugging. Debugging already in progress.");
	}
}

emu_app* emu_app_init(bool initializeGuiLayers)
{
	emu_debugger* debugger = (emu_debugger*)g_memory_allocate(sizeof(emu_debugger));
	emu_virtualMachine* vm = (emu_virtualMachine*)g_memory_allocate(sizeof(emu_virtualMachine));

	emu_vm_initMeta();
	emu_SyntaxHighlighter_init();
	*debugger = emu_debugger_init();
	*vm = emu_vm_init(emu_vmType_NES);

	emu_app* res = g_memory_allocate(sizeof(emu_app));
	*res = (emu_app){
		.debugger = debugger,
		.vm = vm,
		.sdl = NULL,
		.program = NULL,
		.isDebugging = false,
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

void emu_app_debugBreak()
{
	BEGIN_CRITICAL_SECTION(cpuThreadBreakFlagLock);
	cpuThreadBreakFlag = true;
	END_CRITICAL_SECTION(cpuThreadBreakFlagLock);
}

void emu_app_debugStepOver()
{
	BEGIN_CRITICAL_SECTION(shouldStepOverFlagLock);
	shouldStepOverFlag = true;
	END_CRITICAL_SECTION(shouldStepOverFlagLock);

	WakeConditionVariable(&shouldCpuEmulateCondition);
}

void emu_app_debugContinue()
{
	BEGIN_CRITICAL_SECTION(cpuThreadBreakFlagLock);
	cpuThreadBreakFlag = false;
	END_CRITICAL_SECTION(cpuThreadBreakFlagLock);

	WakeConditionVariable(&shouldCpuEmulateCondition);
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

	// If we've stopped debugging for some reason, then clean up the thread resources
	// and tell the thread to stop emulating
	static bool lastFrameWasDebugging = false;
	if (lastFrameWasDebugging && !app->isDebugging)
	{
		stopAndCleanUpCpuThread();
	}
	lastFrameWasDebugging = app->isDebugging;

	SDL_AppResult res = emu_frontend_tick(app->sdl);
	if (res != SDL_APP_CONTINUE)
	{
		return res;
	}

	emu_cimgui_tickBegin(app);
	res = emu_cimgui_tickEnd(app->sdl);
	return res;
}

void emu_app_free(emu_app* app)
{
	if (app)
	{
		stopAndCleanUpCpuThread();

		// Save project on exit
		emu_app_saveProject(app, app->appLoadedProjectFile);

		emu_cimgui_free(app->imgui);
		emu_SyntaxHighlighter_free();
		emu_frontend_free(app->sdl);

		if (app->program)
		{
			emu_NesBinaryFile_free(app->program);
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
			emu_appData_SourceFile* sourceFile = app->data.sourceFiles + i;
			cyaml_node_t* sourceFileObj = cyaml_new_map(doc);
			// Create source file yaml object
			{
				cyaml_map_set(doc, sourceFileObj, "filepath", cyaml_new_cstr(doc, sourceFile->filepath));

				cyaml_node_t* breakpointSeq = cyaml_new_seq(doc);
				for (int j = 0; j < stbds_arrlen(sourceFile->breakpoints); j++)
				{
					cyaml_seq_push(breakpointSeq, cyaml_new_int(doc, sourceFile->breakpoints[j]));
				}
				cyaml_map_set(doc, sourceFileObj, "breakpoints", breakpointSeq);
			}

			// Push source file to sequence in root
			cyaml_seq_push(sourceFiles, sourceFileObj);
		}

		// Set sourceFiles in root of doc
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


				cyaml_node_t* fileToLoadObj = cyaml_get(doc, sourceFileToLoad, "filepath");
				char* fileToLoadStr = cyaml_scalar_str(doc, fileToLoadObj);

				int* breakpoints = NULL;
				cyaml_node_t* breakpointsSeq = cyaml_get(doc, sourceFileToLoad, "breakpoints");
				uint32 numBreakpoints = cyaml_seq_len(breakpointsSeq);
				for (uint32 j = 0; j < numBreakpoints; j++)
				{
					int64 breakpoint;
					cyaml_as_int(doc, cyaml_seq_get(breakpointsSeq, j), &breakpoint);
					stbds_arrpush(breakpoints, (int32)breakpoint);
				}

				stbds_arrpush(app->data.sourceFiles, ((emu_appData_SourceFile)
				{
					.filepath = g_strcpy(fileToLoadStr),
						.breakpoints = breakpoints
				}));
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
			g_memory_free(data->sourceFiles[i].filepath);
			stbds_arrfree(data->sourceFiles[i].breakpoints);
		}
		stbds_arrfree(data->sourceFiles);

		*data = (emu_app_data){ 0 };
	}
}