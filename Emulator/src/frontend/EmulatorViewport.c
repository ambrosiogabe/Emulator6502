#include "frontend/EmulatorViewport.h"

#include <dcimgui.h>

void emu_EmulatorViewport_tick()
{
	if (ImGui_Begin("Viewport", NULL, 0))
	{
		ImGui_Text("Placeholder");
	}
	ImGui_End();
}