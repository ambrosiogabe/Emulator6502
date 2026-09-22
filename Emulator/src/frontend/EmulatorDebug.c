#include "frontend/EmulatorDebug.h"
#include "frontend/ImGuiLayer.h"
#include "Emulator/App.h"
#include "Emulator/VirtualMachine.h"
#include "Emulator/MemoryMap.h"

#include <dcimgui.h>
#include <stdio.h>
#include <stdlib.h>

static void hexMemoryViewer(emu_app const* app);

void emu_EmulatorDebug_tick(emu_app const* app)
{
	if (ImGui_Begin("Debug", NULL, 0))
	{
		hexMemoryViewer(app);
	}

	ImGui_End();
}

// ------------- Internal Definitions -------------s
static void hexMemoryViewer(emu_app const* app)
{
	static int addressToSeek = 0;
	static bool shouldSeekAddress = false;

	emu_cimgui_pushFont(CImGui_FontType_Mono);
	ImFont* font = ImGui_GetFont();

	ImVec2 childSize = ImGui_GetContentRegionAvail();
	childSize.y *= 0.25f;
	ImGui_BeginChild("##DebugHexMemoryViewer", childSize, ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_Borders, 0);

	if (ImGui_BeginTable("##DebugHexMemoryViewerTable", 4, ImGuiTableFlags_BordersOuter | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY))
	{
		if (shouldSeekAddress)
		{
			float rowHeight = ImGui_GetTextLineHeightWithSpacing();
			ImGui_SetScrollY(rowHeight * (float)(addressToSeek / 16));

			shouldSeekAddress = false;
		}

		emu_MemoryMap* mmap = &app->vm->mmap;
		ImGuiListClipper clipper;
		ImGuiListClipper_Begin(&clipper, (int)(mmap->physicalMemorySize / 16), -1.0f);
		while (ImGuiListClipper_Step(&clipper))
		{
			for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
			{
				size_t i = row * 16;

				ImGui_TableNextRow();
				ImGui_TableSetColumnIndex(0);
				ImGui_Text("0x%04X:", i);

				ImGui_TableSetColumnIndex(1);
				for (size_t byteIndex = i; byteIndex < i + 16; byteIndex++)
				{
					if (byteIndex == i + 8)
					{
						ImGui_SameLine();
						ImGui_Spacing();
						ImGui_TableSetColumnIndex(2);
					}

					bool useDisabled = mmap->physicalMemory[byteIndex] == 0;
					if (useDisabled) ImGui_TextDisabled("%02X", mmap->physicalMemory[byteIndex]);
					else ImGui_Text("%02X", mmap->physicalMemory[byteIndex]);
					if (byteIndex < i + 16)
						ImGui_SameLine();
				}

				ImGui_SameLine();
				ImGui_Spacing();

				ImGui_TableSetColumnIndex(3);
				for (size_t byteIndex = i; byteIndex < i + 16; byteIndex++)
				{
					bool useDarkColor = false;
					char c = mmap->physicalMemory[byteIndex];
					if (!ImFont_IsGlyphInFont(font, c) || c == 0)
					{
						useDarkColor = true;
						c = '.';
					}
					
					if (useDarkColor) ImGui_TextDisabled("%c", c);
					else ImGui_Text("%c", c);
					ImGui_SameLineEx(0.0f, 0.0f);
				}
			}
		}
		ImGui_EndTable();
	}

	ImGui_EndChild();

	static char byteSeekBuffer[5];
	static bool isError = false;
	ImGui_Text("0x");
	ImGui_SameLineEx(0.0f, 0.0f);
	ImGui_PushItemWidth(ImGui_GetWindowWidth() * 0.25f);
	ImGui_InputText("##Seek_Byte", byteSeekBuffer, sizeof(byteSeekBuffer), 0);
	ImGui_SameLine();
	if (ImGui_Button("Seek Byte"))
	{
		char* endPtr = byteSeekBuffer;
		int address = (int)strtol(byteSeekBuffer, &endPtr, 16);
		if (errno == ERANGE)
		{
			isError = true;
		}
		else
		{
			shouldSeekAddress = true;
			addressToSeek = address;
		}
	}

	emu_cimgui_popFont();
}