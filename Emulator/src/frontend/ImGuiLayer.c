#include "frontend/ImGuiLayer.h"
#include "frontend/SdlWrapper.h"
#include "frontend/CodeEditor.h"
#include "frontend/MainMenuBar.h"
#include "utils/SafeVendor.h"

#include <dcimgui.h>
#include <backends/dcimgui_impl_sdl3.h>
#include <backends/dcimgui_impl_sdlrenderer3.h>

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
    ImFontAtlas_AddFontDefault(io->Fonts, NULL);
    monoFont = ImFontAtlas_AddFontFromFileTTF(io->Fonts, "C:/Windows/Fonts/UbuntuMono-Regular.ttf", 16.0f, NULL, NULL);
    IM_ASSERT(monoFont != NULL);
    defaultFont = io->FontDefault;

    clear_color.x = 0.45f;
    clear_color.y = 0.55f;
    clear_color.z = 0.60f;
    clear_color.w = 1.0f;

    // Init frontend
    emu_CodeEditor_init();

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

    ImGuiIO* io = ImGui_GetIO();

    // 1. Show the big demo window (Most of the sample code is in ImGui_ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
    if (show_demo_window)
        ImGui_ShowDemoWindow(&show_demo_window);

    // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
    {
        static float f = 0.0f;
        static int counter = 0;

        ImGui_Begin("Hello, world!", NULL, 0);                          // Create a window called "Hello, world!" and append into it.

        ImGui_Text("This is some useful text.");               // Display some text (you can use a format strings too)
        ImGui_Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
        ImGui_Checkbox("Another Window", &show_another_window);

        ImGui_SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
        ImGui_ColorEdit3("clear color", (float*)&clear_color, 0); // Edit 3 floats representing a color

        if (ImGui_Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
            counter++;
        ImGui_SameLine();
        ImGui_Text("counter = %d", counter);

        ImGui_Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io->Framerate, io->Framerate);
        ImGui_End();
    }

    // 3. Show another simple window.
    if (show_another_window)
    {
        ImGui_Begin("Another Window", &show_another_window, 0);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
        ImGui_Text("Hello from another window!");
        if (ImGui_Button("Close Me"))
            show_another_window = false;
        ImGui_End();
    }

    emu_MainMenuBar_tick();
    emu_CodeEditor_tick();
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