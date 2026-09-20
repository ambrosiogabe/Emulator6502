#include "frontend/EmulatorDebug.h"

#include <dcimgui.h>

void emu_EmulatorDebug_tick()
{
	if (ImGui_Begin("Debug", NULL, 0))
	{
		ImGui_Text("Placeholder");
	}

	ImGui_End();
}