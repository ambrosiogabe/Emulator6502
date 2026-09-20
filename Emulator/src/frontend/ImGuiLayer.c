#include "frontend/ImGuiLayer.h"
#include "frontend/SdlWrapper.h"
#include "frontend/CodeEditor.h"
#include "frontend/MainMenuBar.h"
#include "frontend/ConsoleOutput.h"
#include "frontend/EmulatorDebug.h"
#include "frontend/EmulatorViewport.h"
#include "utils/SafeVendor.h"

#include <IconsFontAwesome7.h>
#include <IconsFontAwesome7Brands.h>

#include <dcimgui.h>
#include <backends/dcimgui_impl_sdl3.h>
#include <backends/dcimgui_impl_sdlrenderer3.h>
#include <dcimgui_internal.h>

#include <float.h>

// Our state
bool show_demo_window = true;
bool show_another_window = false;
ImVec4 clear_color;

static ImFont* monoFont = NULL;
static ImFont* defaultFont = NULL;

ImGuiContext* emu_cimgui_init(emu_sdl_wrapper* sdl)
{
	float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());

	// Setup Dear ImGui context
	CIMGUI_CHECKVERSION();
	ImGuiContext* ctx = ImGui_CreateContext(NULL);
	ImGuiIO* io = ImGui_GetIO(); (void)io;
	io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io->ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	io->ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking

	ImGuiStyle* style = ImGui_GetStyle();

	// Setup Dear ImGui style
	ImGui_StyleColorsDark(style);

	// Setup scaling
	ImGuiStyle_ScaleAllSizes(style, main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
	style->FontScaleDpi = main_scale;        // Set initial font scale. (in docking branch: using io.ConfigDpiScaleFonts=true automatically overrides this for every window depending on the current monitor)
	//io.ConfigDpiScaleFonts = true;        // [Experimental] Automatically overwrite style.FontScaleDpi in Begin() when Monitor DPI changes. This will scale fonts but _NOT_ scale sizes/padding for now.
	//io.ConfigDpiScaleViewports = true;    // [Experimental] Scale Dear ImGui and Platform Windows when Monitor DPI changes.

	// Setup Platform/Renderer backends
	cImGui_ImplSDL3_InitForSDLRenderer(sdl->window, sdl->renderer);
	cImGui_ImplSDLRenderer3_Init(sdl->renderer);

	// Load Fonts
	// - If fonts are not explicitly loaded, Dear ImGui will select an embedded font: either AddFontDefaultVector() or AddFontDefaultBitmap().
	//   This selection is based on (style.FontSizeBase * style.FontScaleMain * style.FontScaleDpi) reaching a small threshold.
	// - You can load multiple fonts and use ImGui_PushFont()/PopFont() to select them.
	// - If a file cannot be loaded, AddFont functions will return a nullptr. Please handle those errors in your code (e.g. use an assertion, display an error and quit).
	// - Read 'docs/FONTS.md' for more instructions and details.
	// - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use FreeType for higher quality font rendering.
	// - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
	//style.FontSizeBase = 20.0f;
	//io.Fonts->AddFontDefaultVector();
	//io.Fonts->AddFontDefaultBitmap();
	//io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf");
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf");
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf");
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf");
	//ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf");
	//IM_ASSERT(font != nullptr);
	//ImFontAtlas_AddFontDefault(io->Fonts, NULL);

	// Add default font
	ImFontAtlas_AddFontFromFileTTF(io->Fonts, "C:/Windows/Fonts/segoeui.ttf", 18.0f, NULL, NULL);

	ImFontConfig config = (ImFontConfig){ 0 };
	config.MergeMode = true;
	config.GlyphMinAdvanceX = 16.0f; // Use if you want to make the icon monospaced
	config.FontDataOwnedByAtlas = true;
	config.ExtraSizeScale = 1.0f;
	config.GlyphMaxAdvanceX = FLT_MAX;
	config.RasterizerMultiply = 1.0f;
	config.RasterizerDensity = 1.0f;
	static const ImWchar icon_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
	//ImFontAtlas_AddFontFromFileTTF(io->Fonts, "./assets/fonts/fa-regular-400.ttf", 16.0f, &config, icon_ranges);
	ImFontAtlas_AddFontFromFileTTF(io->Fonts, "./assets/fonts/fa-solid-900.ttf", 16.0f, &config, icon_ranges);
	//ImFontAtlas_AddFontFromFileTTF(io->Fonts, "./assets/fonts/fa-brands-400.ttf", 16.0f, &config, NULL);

	monoFont = ImFontAtlas_AddFontFromFileTTF(io->Fonts, "C:/Windows/Fonts/UbuntuMono-Regular.ttf", 16.0f, NULL, NULL);
	IM_ASSERT(monoFont != NULL);
	defaultFont = io->FontDefault;

	clear_color.x = 0.45f;
	clear_color.y = 0.55f;
	clear_color.z = 0.60f;
	clear_color.w = 1.0f;

	// Init frontend
	emu_CodeEditor_init();
	emu_ConsoleOutput_init();

	emu_ConsoleOutput_info("This is a test");
	emu_ConsoleOutput_warn("This is a test with formatting: '%s'", "I'm formatted here.");
	emu_ConsoleOutput_error("%d:%d:%d", 11, 22, 33);

	return ctx;
}

SDL_AppResult emu_cimgui_handleEvent(SDL_Event* event)
{
	cImGui_ImplSDL3_ProcessEvent(event);
	return SDL_APP_CONTINUE;
}

void emu_cimgui_tickBegin(emu_sdl_wrapper* sdl)
{
	// Start the Dear ImGui frame
	cImGui_ImplSDLRenderer3_NewFrame();
	cImGui_ImplSDL3_NewFrame();
	ImGui_NewFrame();

	// Orgnaize Docking layout
	{
		static bool first_time = true;
		ImGuiID dockspace_id = ImGui_DockSpaceOverViewportEx(0, ImGui_GetMainViewport(), ImGuiDockNodeFlags_NoUndocking, NULL);

		if (first_time)
		{
			first_time = false;

			// Clear out any existing invalid layout
			ImGui_DockBuilderRemoveNode(dockspace_id);

			// Add a fresh dockspace node
			ImGui_DockBuilderAddNodeEx(dockspace_id, ImGuiDockNodeFlags_DockSpace);
			ImGui_DockBuilderSetNodeSize(dockspace_id, ImGui_GetMainViewport()->Size);

			// Split the dockspace node into different regions (e.g., Left panel for controls)
			ImGuiID dock_main_id = dockspace_id;
			ImGuiID dock_left_id = ImGui_DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.25f, NULL, &dock_main_id);
			ImGuiID dock_down_id = ImGui_DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.30f, NULL, &dock_main_id);

			// Assign specific window names to specific dock IDs
			ImGui_DockBuilderDockWindow("Debug", dock_left_id);
			ImGui_DockBuilderDockWindow("Dear ImGui Demo", dock_left_id);
			ImGui_DockBuilderDockWindow("Console Output", dock_down_id);
			ImGui_DockBuilderDockWindow("Code Editor", dock_main_id);
			ImGui_DockBuilderDockWindow("Viewport", dock_main_id);

			// Complete the builder logic
			ImGui_DockBuilderFinish(dockspace_id);
		}
	}

	// Draw windows of the app
	emu_MainMenuBar_tick();
	emu_CodeEditor_tick();
	emu_ConsoleOutput_tick();
	emu_EmulatorDebug_tick();
	emu_EmulatorViewport_tick();

	if (show_demo_window)
		ImGui_ShowDemoWindow(&show_demo_window);
}

SDL_AppResult emu_cimgui_tickEnd(emu_sdl_wrapper* sdl)
{
	ImGuiIO* io = ImGui_GetIO();

	// Rendering
	ImGui_Render();
	SDL_SetRenderScale(sdl->renderer, io->DisplayFramebufferScale.x, io->DisplayFramebufferScale.y);
	SDL_SetRenderDrawColorFloat(sdl->renderer, clear_color.x, clear_color.y, clear_color.z, clear_color.w);
	SDL_RenderClear(sdl->renderer);
	cImGui_ImplSDLRenderer3_RenderDrawData(ImGui_GetDrawData(), sdl->renderer);
	SDL_RenderPresent(sdl->renderer);

	return SDL_APP_CONTINUE;
}

void emu_cimgui_free(ImGuiContext* ctx)
{
	// Free frontend
	emu_CodeEditor_free();
	emu_ConsoleOutput_free();

	// Cleanup
	// [If using SDL_MAIN_USE_CALLBACKS: all code below would likely be your SDL_AppQuit() function]
	cImGui_ImplSDLRenderer3_Shutdown();
	cImGui_ImplSDL3_Shutdown();
	ImGui_DestroyContext(ctx);
}

void emu_cimgui_pushFont(CImGui_FontType fontType)
{
	switch (fontType)
	{
	case CImGui_FontType_Default:
		ImGui_PushFont(defaultFont);
		break;
	case CImGui_FontType_Mono:
		ImGui_PushFont(monoFont);
		break;
	}
}

void emu_cimgui_popFont()
{
	ImGui_PopFont();
}