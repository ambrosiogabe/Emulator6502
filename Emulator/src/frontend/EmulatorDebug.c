#include "frontend/EmulatorDebug.h"
#include "frontend/ImGuiLayer.h"
#include "frontend/SyntaxHighlighter.h"
#include "frontend/SdlWrapper.h"
#include "Emulator/App.h"
#include "Emulator/VirtualMachine.h"
#include "Emulator/MemoryMap.h"

#include <IconsFontAwesome7.h>
#include <dcimgui.h>
#include <stdio.h>
#include <stdlib.h>

// ------------- Internal Definitions -------------
static uint32 calculateNumRows();
static uint16 getByteAddressFromRow(uint32 row);
static uint32 getRowFromByteAddress(uint16 byteAddress);
static void hexMemoryViewer(emu_app* app);

static emu_virtualMachine* vm = NULL;

static emu_MemoryMap const* mmap = NULL;
static uint32 mmapNumRows = 0;
static int mmapClipperStart = 0;
static uint32 mmapByteIndex = 0;
static int addressToSeek = 0;
static bool shouldSeekAddress = false;

SDL_GPUTexture* gpu_texture;
static int surfaceWidth = 16 * 8;
static int surfaceHeight = 16 * 8;

void emu_EmulatorDebug_tick(emu_app* app)
{
	if (ImGui_Begin("Debug", NULL, 0))
	{
		hexMemoryViewer(app);
	}

	ImGui_End();
}

void emu_EmulatorDebug_beginDebugging(emu_app* app)
{
	vm = app->vm;
	mmap = &app->vm->mmap;
	mmapNumRows = calculateNumRows();
	shouldSeekAddress = true;
	addressToSeek = app->vm->programCounter;

	SDL_Surface* surface = SDL_CreateSurface(surfaceWidth, surfaceHeight, SDL_PIXELFORMAT_ABGR8888);
	uint32* globalPixels = surface->pixels;
	// Render tiles to our surface to display in ImGui
	for (int tileIndex = 0; tileIndex <= 0xFF; tileIndex++)
	{
		emu_Tile tile = emu_chip_ppuLoadTile(vm, (uint8)tileIndex);
		uint8 tileY = (uint8)(tileIndex / 16);
		uint8 tileX = (uint8)(tileIndex % 16);

		for (uint8 x = 0; x < 8; x++)
		{
			for (uint8 y = 0; y < 8; y++)
			{
				uint8 pixel = tile.pixels[y][x];
				// Transparent = 0
				uint8 r = 0;
				uint8 g = 156;
				uint8 b = 218;
				uint8 a = 255;

				// Red = 1
				if (pixel == 1)
				{
					r = 235;
					g = 64;
					b = 52;
					a = 255;
				}
				// Skin Tone = 2
				else if (pixel == 2)
				{
					r = 254;
					g = 209;
					b = 176;
					a = 255;
				}
				// Brown = 3
				else if (pixel == 3)
				{
					r = 137;
					g = 76;
					b = 47;
					a = 255;
				}
				// Pink = NULL
				else if (pixel != 0)
				{
					r = 255;
					g = 46;
					b = 168;
					a = 255;
				}

				globalPixels[((tileY * 8 + y) * surfaceWidth) + (tileX * 8) + x] = (a << 24) | (b << 16) | (g << 8) | r;
			}
		}
	}

	// 1. Create GPU texture
	SDL_GPUTextureCreateInfo texture_info = { 0 };
	texture_info.type = SDL_GPU_TEXTURETYPE_2D;
	texture_info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
	texture_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
	texture_info.width = surfaceWidth;
	texture_info.height = surfaceHeight;
	texture_info.layer_count_or_depth = 1;
	texture_info.num_levels = 1;
	texture_info.sample_count = SDL_GPU_SAMPLECOUNT_1;

	gpu_texture = SDL_CreateGPUTexture(app->sdl->gpuDevice, &texture_info);

	// Create transfer buffer
	// FIXME: A real engine would likely keep one around, see what the SDL_GPU backend is doing.
	SDL_GPUTransferBufferCreateInfo transferbuffer_info = { 0 };
	transferbuffer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
	transferbuffer_info.size = surfaceWidth * surfaceHeight * 4;
	SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(app->sdl->gpuDevice, &transferbuffer_info);
	g_logger_assert(transferBuffer != NULL, "transfer buffer is null");

	// Copy to transfer buffer
	uint32_t upload_pitch = surfaceWidth * sizeof(uint32);
	uint32 source_pitch = surfaceWidth;
	void* texture_ptr = SDL_MapGPUTransferBuffer(app->sdl->gpuDevice, transferBuffer, true);
	for (int y = 0; y < surfaceHeight; y++)
		memcpy((void*)((uintptr_t)texture_ptr + y * upload_pitch), globalPixels + y * source_pitch, upload_pitch);
	SDL_UnmapGPUTransferBuffer(app->sdl->gpuDevice, transferBuffer);

	// 4. Record and Submit the GPU Copy Command
	SDL_GPUTextureTransferInfo transfer_info = { 0 };
	transfer_info.offset = 0;
	transfer_info.transfer_buffer = transferBuffer;

	SDL_GPUTextureRegion texture_region = { 0 };
	texture_region.texture = gpu_texture;
	texture_region.x = (Uint32)0;
	texture_region.y = (Uint32)0;
	texture_region.w = (Uint32)surfaceWidth;
	texture_region.h = (Uint32)surfaceHeight;
	texture_region.d = 1;

	// Upload
	SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(app->sdl->gpuDevice);
	SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(cmd);
	SDL_UploadToGPUTexture(copy_pass, &transfer_info, &texture_region, false);
	SDL_EndGPUCopyPass(copy_pass);
	SDL_SubmitGPUCommandBuffer(cmd);

	// 5. Clean up temporary host/staging assets
	// Release the staging buffer (safe to call immediately; execution happens safely via internal lifecycle reference counting)
	SDL_ReleaseGPUTransferBuffer(app->sdl->gpuDevice, transferBuffer);
	SDL_DestroySurface(surface);
	surface = NULL;
}

void emu_EmulatorDebug_endDebugging()
{
	mmap = NULL;
	vm = NULL;
	mmapNumRows = 0;
}

// ------------- Internal Definitions -------------
static uint32 calculateNumRows()
{
	return getRowFromByteAddress(0xFFFF);
}

static uint32 getRowFromByteAddress(uint16 byteAddress)
{
	if (!mmap)
	{
		return 0;
	}

	uint32 numRows = 0;
	for (size_t byteIndex = 0; byteIndex < mmap->physicalMemorySize; byteIndex++)
	{
		emu_vmInstruction instruction = mmap->physicalMemory[byteIndex];
		uint8 numArgs = emu_vm_instructionNumArgs(instruction);

		if (byteIndex >= byteAddress && byteAddress <= byteIndex + numArgs)
		{
			return numRows;
		}

		byteIndex += numArgs;
		numRows++;
	}

	return numRows;
}

static uint16 getByteAddressFromRow(uint32 row)
{
	if (!mmap)
	{
		return 0;
	}

	uint32 numRows = 0;
	for (size_t byteIndex = 0; byteIndex < mmap->physicalMemorySize; byteIndex++)
	{
		emu_vmInstruction instruction = mmap->physicalMemory[byteIndex];
		uint8 numArgs = emu_vm_instructionNumArgs(instruction);

		if (row == numRows)
		{
			return (uint16)byteIndex;
		}

		byteIndex += numArgs;
		numRows++;
	}

	return 0xFFFF;
}

static void hexMemoryViewer(emu_app* app)
{
	if (!mmap)
	{
		ImGui_Text("Open a program and press `Debug` to use this debugger.");
		return;
	}

	emu_cimgui_pushFont(CImGui_FontType_Mono);
	ImFont* font = ImGui_GetFont();

	emu_CodeTheme const* theme = emu_SyntaxHighlighter_getTheme();
	ImGui_PushStyleColor(ImGuiCol_ChildBg, ImGui_ColorConvertFloat4ToU32(theme->bgColor));
	ImGui_PushStyleColor(ImGuiCol_Text, ImGui_ColorConvertFloat4ToU32(theme->defaultColor));
	ImVec4 commentColor = emu_SyntaxHighlighter_getColor(theme, "comment");
	ImVec4 builtinFunctionColor = emu_SyntaxHighlighter_getColor(theme, "function.builtin");
	ImVec4 errorColor = emu_SyntaxHighlighter_getColor(theme, "error");

	ImVec2 childSize = ImGui_GetContentRegionAvail();
	childSize.y *= 0.25f;
	ImGui_BeginChild("##DebugHexMemoryViewer", childSize, ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_Borders, 0);

	if (ImGui_BeginTabBar("##RawMemoryViewers", 0))
	{
		if (ImGui_BeginTabItem("ASM View", NULL, 0))
		{
			if (ImGui_BeginTable("##DebugHexMemoryViewerTable", 4, ImGuiTableFlags_BordersOuter | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY))
			{
				ImGuiListClipper clipper;
				ImGuiListClipper_Begin(&clipper, (int)mmapNumRows, -1.0f);

				if (shouldSeekAddress)
				{
					float rowHeight = ImGui_GetTextLineHeightWithSpacing();
					uint32 rowToSeek = getRowFromByteAddress((uint16)addressToSeek);
					ImGui_SetScrollY(rowHeight * (float)rowToSeek);

					shouldSeekAddress = false;
				}

				while (ImGuiListClipper_Step(&clipper))
				{
					if (clipper.DisplayStart != mmapClipperStart)
					{
						mmapClipperStart = clipper.DisplayStart;
						mmapByteIndex = getByteAddressFromRow(clipper.DisplayStart);
					}

					for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
					{
						emu_vmInstruction instruction = mmap->physicalMemory[mmapByteIndex];
						uint8 numArgs = emu_vm_instructionNumArgs(instruction);

						ImGui_TableNextRow();
						ImGui_TableSetColumnIndex(0);

						if (vm->programCounter >= mmapByteIndex && vm->programCounter <= mmapByteIndex + numArgs)
						{
							emu_cimgui_popFont();
							ImGui_TextColored(errorColor, ICON_FA_CIRCLE_ARROW_RIGHT);
							emu_cimgui_pushFont(CImGui_FontType_Mono);
							ImGui_SameLine();
						}
						ImGui_Text("0x%04X:", mmapByteIndex);

						ImGui_TableSetColumnIndex(1);
						for (size_t byteIndex = mmapByteIndex; byteIndex < mmapByteIndex + numArgs + 1; byteIndex++)
						{
							ImGui_Text("%02X", mmap->physicalMemory[byteIndex]);
							if (byteIndex < mmapByteIndex + numArgs + 1)
								ImGui_SameLine();
						}

						ImGui_TableSetColumnIndex(2);
						ImGui_TextColored(builtinFunctionColor, "%s", emu_vm_disassembleInstruction(instruction));

						ImGui_TableSetColumnIndex(3);
						if (numArgs > 0)
						{
							ImGui_Text("$");
							ImGui_SameLineEx(0.0f, 0.0f);
						}

						for (int64 byteIndex = mmapByteIndex + numArgs; byteIndex > mmapByteIndex; byteIndex--)
						{
							ImGui_Text("%02X", mmap->physicalMemory[byteIndex]);
							if (byteIndex > mmapByteIndex)
								ImGui_SameLineEx(0.0f, 0.0f);
						}

						mmapByteIndex += numArgs + 1;
					}
				}

				ImGui_EndTable();
			}

			ImGui_EndTabItem();
		}

		if (ImGui_BeginTabItem("Static RAM", NULL, 0))
		{
			if (ImGui_BeginTable("##DebugHexStaticRamViewerTable", 4, ImGuiTableFlags_BordersOuter | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY))
			{
				if (shouldSeekAddress)
				{
					float rowHeight = ImGui_GetTextLineHeightWithSpacing();
					ImGui_SetScrollY(rowHeight * (float)(addressToSeek / 16));

					shouldSeekAddress = false;
				}

				ImGuiListClipper clipper;
				ImGuiListClipper_Begin(&clipper, (int)(sizeof(vm->staticRam) / 16), -1.0f);
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

							uint8 value = vm->staticRam.ram[byteIndex];
							ImVec4 color = value == 0
								? commentColor
								: theme->defaultColor;
							ImGui_TextColored(color, "%02X", value);
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

							ImVec4 color = useDarkColor
								? commentColor
								: theme->defaultColor;
							ImGui_TextColored(color, "%c", c);
							ImGui_SameLineEx(0.0f, 0.0f);
						}
					}
				}
				ImGui_EndTable();
			}

			ImGui_EndTabItem();
		}

		if (ImGui_BeginTabItem("Raw Memory", NULL, 0))
		{
			if (ImGui_BeginTable("##DebugHexMemoryViewerTable", 4, ImGuiTableFlags_BordersOuter | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY))
			{
				if (shouldSeekAddress)
				{
					float rowHeight = ImGui_GetTextLineHeightWithSpacing();
					ImGui_SetScrollY(rowHeight * (float)(addressToSeek / 16));

					shouldSeekAddress = false;
				}

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

							ImVec4 color = mmap->physicalMemory[byteIndex] == 0
								? commentColor
								: theme->defaultColor;
							ImGui_TextColored(color, "%02X", mmap->physicalMemory[byteIndex]);
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

							ImVec4 color = useDarkColor
								? commentColor
								: theme->defaultColor;
							ImGui_TextColored(color, "%c", c);
							ImGui_SameLineEx(0.0f, 0.0f);
						}
					}
				}
				ImGui_EndTable();
			}

			ImGui_EndTabItem();
		}

		ImGui_EndTabBar();
	}

	ImGui_PopStyleColorEx(2);
	ImGui_EndChild();

	static char byteSeekBuffer[5];
	static bool isError = false;
	ImGui_Text("0x");
	ImGui_SameLineEx(0.0f, 0.0f);
	ImGui_PushItemWidth(ImGui_GetWindowWidth() * 0.25f);
	ImGui_InputText("##Seek_Byte", byteSeekBuffer, sizeof(byteSeekBuffer), 0);

	emu_cimgui_popFont();

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

	if (ImGui_Button("Step Into"))
	{
		emu_app_debugStepOver();
	}
	ImGui_SameLine();

	if (ImGui_Button("Continue"))
	{
		emu_app_debugContinue();
	}

	if (ImGui_Button("Break"))
	{
		emu_app_debugBreak();
	}
	ImGui_SameLine();

	if (ImGui_Button("Stop Debugging"))
	{
		app->isDebugging = false;
	}

	if (ImGui_BeginChild("Status Flags", (ImVec2) { 0 }, ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY, 0))
	{
		ImGui_BeginDisabled(true);

		bool negativeStatus = emu_vm_getStatus(vm, emu_vmStatus_Negative);
		ImGui_Checkbox("N", &negativeStatus);
		ImGui_SetItemTooltip("Negative Status. Set when the accumulator is negative.");
		ImGui_SameLine();

		bool overflowStatus = emu_vm_getStatus(vm, emu_vmStatus_Overflow);
		ImGui_Checkbox("V", &overflowStatus);
		ImGui_SetItemTooltip("Overflow Status. Set on signed overflow for the accumulator.");
		ImGui_SameLine();

		bool oneStatus = emu_vm_getStatus(vm, emu_vmStatus_1);
		ImGui_Checkbox("1", &oneStatus);
		ImGui_SetItemTooltip("1 Status. Reserved for special use.");
		ImGui_SameLine();

		bool bStatus = emu_vm_getStatus(vm, emu_vmStatus_B);
		ImGui_Checkbox("B", &bStatus);
		ImGui_SetItemTooltip("Break Status. Set when interrupt was caused by a BRK.");

		bool decimalStatus = emu_vm_getStatus(vm, emu_vmStatus_Decimal);
		ImGui_Checkbox("D", &decimalStatus);
		ImGui_SetItemTooltip("Decimal Status. Set when CPU is in BCD mode.");
		ImGui_SameLine();

		bool interruptStatus = emu_vm_getStatus(vm, emu_vmStatus_InterruptDisable);
		ImGui_Checkbox("I", &interruptStatus);
		ImGui_SetItemTooltip("IRQ Status. When set, no interrupts will occur (exceptions are IRQs forced by BRK and NMIs)");
		ImGui_SameLine();

		bool zeroStatus = emu_vm_getStatus(vm, emu_vmStatus_Zero);
		ImGui_Checkbox("Z", &zeroStatus);
		ImGui_SetItemTooltip("Zero Status. Set when all bits in accumulator are 0.");
		ImGui_SameLine();

		bool carryStatus = emu_vm_getStatus(vm, emu_vmStatus_Carry);
		ImGui_Checkbox("C", &carryStatus);
		ImGui_SetItemTooltip("Carry Status. Set on unsigned overflow for accumulator.");

		ImGui_EndDisabled();

		ImGui_EndChild();
	}

	ImGui_Text("A: 0x%02X", vm->accumulatorReg);
	ImGui_SetItemTooltip("Accumulator register. Used for storing results of mathematical/logical operations.");
	ImGui_SameLine();

	ImGui_Text("X: 0x%02X", vm->xReg);
	ImGui_SetItemTooltip("X register. Used for storing information.");
	ImGui_SameLine();

	ImGui_Text("Y: 0x%02X", vm->yReg);
	ImGui_SetItemTooltip("Y register. Used for storing information.");
	ImGui_SameLine();

	ImGui_Text("SP: 0x%02X", vm->stackPointer);
	ImGui_SetItemTooltip("Stack pointer. The stack points to 0x1ff - SP.");

	// Section to show the PPU data
	if (ImGui_BeginChild("PPU", (ImVec2) { 0 }, ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY, 0))
	{
		ImDrawList* draw_list = ImGui_GetWindowDrawList();
		ImVec2 p_min = ImGui_GetCursorScreenPos();
		float scale = 10.0f;
		ImVec2 p_max = {
			.x = p_min.x + (float)surfaceWidth * scale,
			.y = p_min.y + (float)surfaceHeight * scale
		};

		ImGui_Dummy((ImVec2){ .x = (float)surfaceWidth * scale, .y = (float)surfaceHeight * scale });

		ImDrawList_AddCallback(draw_list, ImGui_GetPlatformIO()->DrawCallback_SetSamplerNearest);
		ImDrawList_AddImage(draw_list, (ImTextureRef) { ._TexData = NULL, ._TexID = (ImTextureID)(intptr_t)gpu_texture }, p_min, p_max);

		ImGui_EndChild();
	}
}