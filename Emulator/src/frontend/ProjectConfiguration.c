#include "frontend/ProjectConfiguration.h"
#include "Emulator/App.h"
#include "utils/SafeVendor.h"

#include <stb/stb_ds.h>

#include <stdbool.h>
#include <dcimgui.h>

static bool isOpen = false;

void emu_projectConfiguration_tick(emu_app* app)
{
	if (!isOpen)
	{
		return;
	}

	if (ImGui_Begin("Project Configuration", &isOpen, 0))
	{
		if (ImGui_BeginChild("##SourceFiles", (ImVec2) { .x = 0, .y = 0 }, ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY, 0))
		{
			ImGui_BeginDisabled(true);
			ImGui_Text("cl65 ");
			for (int i = 0; i < stbds_arrlen(app->data.sourceFiles); i++)
			{
				ImGui_SameLineEx(0.0f, 0.0f);
				ImGui_Text(" %s", app->data.sourceFiles[i]);
			}
			ImGui_SameLineEx(0.0f, 0.0f);
			ImGui_Text(" --verbose --target nes");
			ImGui_EndDisabled();
		}
		ImGui_EndChild();
	}

	ImGui_End();
}

void emu_projectConfiguration_open()
{
	g_logger_info("Opening!");
	isOpen = true;
}

void emu_projectConfiguration_close()
{
	isOpen = false;
}
