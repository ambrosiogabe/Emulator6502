#include "frontend/MainMenuBar.h"
#include "frontend/SyntaxHighlighter.h"
#include "frontend/CodeEditor.h"
#include "utils/FileHelper.h"
#include "Emulator/Assembler.h"
#include "Emulator/VirtualMachine.h"
#include "Emulator/App.h"

#include <dcimgui.h>

void emu_MainMenuBar_tick(emu_app* app)
{
	if (ImGui_BeginMainMenuBar())
	{
		if (ImGui_BeginMenu("File"))
		{
			if (ImGui_MenuItemEx("Open", "Ctrl+O", false, true))
			{
				const int numFileFilters = 2;
				const char* fileFilters[] = { "*.s", "*.txt" };
				const char* filename = emu_file_openFileDialog(numFileFilters, fileFilters);
				if (filename != NULL)
				{
					emu_CodeEditor_openFile(filename);

					emu_file sourceCodeFile = { 0 };
					emu_file_read(filename, &sourceCodeFile);

					emu_assembler_program program = emu_assembler_assembleProgram(&app->vm->mmap, filename, UINT16_MAX);
					//emu_vm_printOpcodes(program.program, program.size);

					emu_vm_loadProgram(app->vm, &program);
					emu_vm_resetMachine(app->vm);

					// TODO: Figure out where to store program
					// emu_assembler_program* res = g_memory_allocate(sizeof(emu_assembler_program));
					// g_memory_copyMem(res, &program, sizeof(emu_assembler_program));
					emu_assembler_free(&program);

					emu_file_free(&sourceCodeFile);
				}
			}
			if (ImGui_MenuItemEx("Save", "Ctrl+S", false, true))
			{

			}
			ImGui_EndMenu();
		}
		if (ImGui_BeginMenu("Edit"))
		{
			if (ImGui_MenuItemEx("Undo", "Ctrl+Z", false, true)) g_logger_warning("TODO: Implement Undo");
			if (ImGui_MenuItemEx("Redo", "Ctrl+Y", false, false)) g_logger_warning("TODO: Implement Redo");
			ImGui_Separator();
			if (ImGui_MenuItemEx("Cut", "Ctrl+X", false, true)) g_logger_warning("TODO: Implement Cut");
			if (ImGui_MenuItemEx("Copy", "Ctrl+C", false, true)) g_logger_warning("TODO: Implement Copy");
			if (ImGui_MenuItemEx("Paste", "Ctrl+V", false, true)) g_logger_warning("TODO: Implement Paste");
			ImGui_Separator();
			if (ImGui_BeginMenu("Set Theme"))
			{
				if (ImGui_MenuItem("Github Dark")) emu_SyntaxHighlighter_setTheme(emu_CodeTheme_Type_Dark);
				if (ImGui_MenuItem("Github Light")) emu_SyntaxHighlighter_setTheme(emu_CodeTheme_Type_Light);
				if (ImGui_MenuItem("Catpuccin Mocha")) emu_SyntaxHighlighter_setTheme(emu_CodeTheme_Type_CatpuccinMocha);
				if (ImGui_MenuItem("Ayu Dark")) emu_SyntaxHighlighter_setTheme(emu_CodeTheme_Type_AyuDark);
				if (ImGui_MenuItem("Dracula")) emu_SyntaxHighlighter_setTheme(emu_CodeTheme_Type_Dracula);
				if (ImGui_MenuItem("Everforest Dark")) emu_SyntaxHighlighter_setTheme(emu_CodeTheme_Type_EverforestDark);
				if (ImGui_MenuItem("Gruvbox Material")) emu_SyntaxHighlighter_setTheme(emu_CodeTheme_Type_GruvboxMaterial);
				if (ImGui_MenuItem("Gruvbox")) emu_SyntaxHighlighter_setTheme(emu_CodeTheme_Type_Gruvbox);
				if (ImGui_MenuItem("Kanagawa")) emu_SyntaxHighlighter_setTheme(emu_CodeTheme_Type_Kanagawa);
				if (ImGui_MenuItem("Nord")) emu_SyntaxHighlighter_setTheme(emu_CodeTheme_Type_Nord);
				if (ImGui_MenuItem("One Dark")) emu_SyntaxHighlighter_setTheme(emu_CodeTheme_Type_OneDark);
				if (ImGui_MenuItem("Rose Pine")) emu_SyntaxHighlighter_setTheme(emu_CodeTheme_Type_RosePine);
				if (ImGui_MenuItem("Solarized Light")) emu_SyntaxHighlighter_setTheme(emu_CodeTheme_Type_SolarizedLight);
				if (ImGui_MenuItem("Tokyo Night")) emu_SyntaxHighlighter_setTheme(emu_CodeTheme_Type_TokyoNight);

				ImGui_EndMenu();
			}
			ImGui_EndMenu();
		}
		ImGui_EndMainMenuBar();
	}
}