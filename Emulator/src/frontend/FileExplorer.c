#include "frontend/FileExplorer.h"
#include "frontend/ConsoleOutput.h"
#include "frontend/CodeEditor.h"
#include "Emulator/App.h"
#include "utils/SafeVendor.h"
#include "utils/FileHelper.h"

#include <dcimgui.h>
#include <minmax.h>
#include <stb/stb_ds.h>
#include <IconsFontAwesome7.h>

static void showDefaultView(emu_app_data* app);
static int showDirectory(int uid, emu_file_data* dir, emu_app* app);
static void freeDirectory(emu_file_data* dir);

static emu_file_data openDirectory;

void emu_FileExplorer_init(const char* directory)
{
	openDirectory = emu_file_getAllFilesInDirectory(directory);
}

void emu_FileExplorer_tick(emu_app* app)
{
	if (app->data.projectDirectory == NULL)
	{
		showDefaultView(&app->data);
		return;
	}


	if (ImGui_Begin("Explorer", NULL, 0))
	{
		showDirectory(0, &openDirectory, app);

		//for (int i = 0; i < 5; i++)
		//{
		//	// Use SetNextItemOpen() so set the default state of a node to be open. We could
		//	// also use TreeNodeEx() with the ImGuiTreeNodeFlags_DefaultOpen flag to achieve the same thing!
		//	if (i == 0)
		//		ImGui::SetNextItemOpen(true, ImGuiCond_Once);

		//	// Here we use PushID() to generate a unique base ID, and then the "" used as TreeNode id won't conflict.
		//	// An alternative to using 'PushID() + TreeNode("", ...)' to generate a unique ID is to use 'TreeNode((void*)(intptr_t)i, ...)',
		//	// aka generate a dummy pointer-sized value to be hashed. The demo below uses that technique. Both are fine.
		//	ImGui::PushID(i);
		//	if (ImGui::TreeNode("", "Child %d", i))
		//	{
		//		ImGui::Text("blah blah");
		//		ImGui::SameLine();
		//		if (ImGui::SmallButton("button")) {}
		//		ImGui::TreePop();
		//	}
		//	ImGui::PopID();
		//}
		//ImGui::TreePop();
	}

	ImGui_End();
}

void emu_FileExplorer_free()
{
	freeDirectory(&openDirectory);
}

// -------------------------- Internal Definitions --------------------------
static void showDefaultView(emu_app_data* app)
{
	if (ImGui_Begin("No Open Folders", NULL, 0))
	{
		float windowWidth = ImGui_GetWindowWidth();

		const char* displayText = "You have not yet opened a folder.";
		ImVec2 textSize = ImGui_CalcTextSize(displayText);
		ImGui_SetCursorPosX((windowWidth - textSize.x) * 0.5f);
		ImGui_Text(displayText);

		ImGui_Dummy((ImVec2) { .x = 0, .y = 5 });

		float buttonWidth = min(windowWidth - ImGui_GetStyle()->WindowPadding.x * 2.0f, textSize.x);
		ImGui_SetCursorPosX((windowWidth - buttonWidth) * 0.5f);
		if (ImGui_ButtonEx("Open Folder", (ImVec2) { .x = buttonWidth, .y = 0 }))
		{
			const char* folderName = emu_file_openFolderDialog("");
			if (folderName)
			{
				app->projectDirectory = g_strcpy(folderName);
			}
		}
	}
	ImGui_End();
}

static int showDirectory(int uid, emu_file_data* dir, emu_app* app)
{
	ImGuiTreeNodeFlags_ flags = dir == &openDirectory
		? ImGuiTreeNodeFlags_DefaultOpen
		: ImGuiTreeNodeFlags_None;
	flags |= ImGuiTreeNodeFlags_SpanAvailWidth;
	ImGui_PushIDInt(uid);
	uid++;
	if (ImGui_TreeNodeExStr(dir->filename, flags, "%s %s", ICON_FA_FOLDER, dir->filename))
	{
		// First display all directories
		for (int i = 0; i < stbds_arrlen(dir->children); i++)
		{
			emu_file_data* child = dir->children + i;
			if (child->type & emu_file_type_directory)
			{
				uid = showDirectory(uid, child, app);
			}
		}

		// Then show all files
		for (int i = 0; i < stbds_arrlen(dir->children); i++)
		{
			emu_file_data* child = dir->children + i;
			if (child->type & emu_file_type_normal)
			{
				ImGui_PushIDInt(uid);
				const char* icon = emu_file_getExtIcon(child->extType);
				if (ImGui_TreeNodeExStr(child->filename, flags | ImGuiTreeNodeFlags_Leaf, "%s %s", icon, child->filename))
				{
					if (ImGui_IsItemClicked())
					{
						if (child->extType == emu_file_extType_asm)
						{
							emu_CodeEditor_openFile(app, child->fullFilepath);
						}
						else
						{
							emu_ConsoleOutput_error("Cannot open file '%s'. File must be assembly file with extension '.s'.", child->filename);
						}
					}
					ImGui_TreePop();
				}
				ImGui_PopID();

				if (ImGui_BeginPopupContextItem())
				{
					if (ImGui_MenuItem("Add to Project"))
					{
						if (child->extType == emu_file_extType_asm)
						{
							stbds_arrpush(app->data.sourceFiles, g_strcpy(child->fullFilepath));
						}
						else
						{
							emu_ConsoleOutput_error("Cannot add file '%s' to project. File must be assembly file with extension '.s'.", child->filename);
						}
					}
					ImGui_EndPopup();
				}
				uid++;
			}
		}
		ImGui_TreePop();
	}
	ImGui_PopID();

	return uid;
}

static void freeDirectory(emu_file_data* dir)
{
	if (!dir)
	{
		return;
	}

	for (int i = 0; i < stbds_arrlen(dir->children); i++)
	{
		emu_file_data* child = dir->children + i;
		if (child->type & emu_file_type_directory)
		{
			freeDirectory(child);
		}

		if (child->filename)
		{
			g_memory_free(child->filename);
			child->filename = NULL;
			child->filenameLength = 0;
		}

		if (child->fullFilepath)
		{
			g_memory_free(child->fullFilepath);
			child->fullFilepath = NULL;
			child->fullFilepathLength = 0;
		}
	}

	stbds_arrfree(dir->children);
	if (dir->filename)
	{
		g_memory_free(dir->filename);
		dir->filename = NULL;
		dir->filenameLength = 0;
	}

	if (dir->fullFilepath)
	{
		g_memory_free(dir->fullFilepath);
		dir->fullFilepath = NULL;
		dir->fullFilepathLength = 0;
	}
}