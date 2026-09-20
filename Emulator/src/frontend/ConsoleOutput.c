#include "frontend/ConsoleOutput.h"

#include <dcimgui.h>

void emu_ConsoleOutput_tick()
{
	if (ImGui_Begin("Console Output", NULL, 0))
	{
		ImGui_Text("Placeholder");
	}
	ImGui_End();
}