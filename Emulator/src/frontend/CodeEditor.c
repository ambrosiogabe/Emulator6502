#include "frontend/CodeEditor.h"
#include "frontend/ImGuiLayer.h"
#include "frontend/SyntaxHighlighter.h"
#include "Emulator/App.h"
#include "utils/FileHelper.h"
#include "utils/SafeVendor.h"

#include <dcimgui.h>
#include <dcimgui_internal.h>
#include <stb/stb_ds.h>

typedef struct HighlightedCode
{
	ImVec4 color;
	uint32 startByte;
	uint32 endByte;
} HighlightedCode;

static HighlightedCode* codeHighlights = NULL;

// Declare the `tree_sitter_asm6502` function, which is
// implemented by the `tree-sitter-asm6502` library.
const TSLanguage* tree_sitter_asm6502(void);

static void getAllCaptures();

#define sourceCodeBufferMaxLength 4'096
static char sourceCodeBuffer[sourceCodeBufferMaxLength];
static int sourceCodeBufferLength;

static TSTree* syntaxTree = NULL;

static void generateSyntaxTree()
{
	syntaxTree = emu_SyntaxHighlighter_highlightSource(sourceCodeBuffer, sourceCodeBufferLength);
	getAllCaptures();
}

static void getAllCaptures()
{
	const char* asmQuery = "(opcode) @function.builtin\
		(num_literal) @constant.numeric\
		(register) @constant.builtin\
		(operator) @operator\
\
		(local_label) @variable\
		(global_label) @variable\
\
		(control_command) @operator\
		(file_name) @string\
		(section_name) @constant\
\
		(immediate\
			\"#\" @operator\
		)\
		(indirect\
			\"(\" @operator\
			\")\" @operator\
			)\
\
		(ERROR) @error\
		(comment) @comment\
		";

	uint32_t error_offset;
	TSQueryError error_type;
	TSQuery* query = ts_query_new(tree_sitter_asm6502(), asmQuery, (uint32)strlen(asmQuery), &error_offset, &error_type);

	if (!query)
	{
		g_logger_error("Query creation failed at offset %d:%d", error_offset, error_type);
		return;
	}

	TSQueryCursor* cursor = ts_query_cursor_new();
	ts_query_cursor_exec(cursor, query, ts_tree_root_node(syntaxTree));

	emu_CodeTheme const* theme = emu_SyntaxHighlighter_getTheme();

	TSQueryMatch match;
	uint32 captureIndex;
	HighlightedCode lastValue = {
		.color = emu_SyntaxHighlighter_getColor(theme , "default"),
		.startByte = 0,
		.endByte = 0
	};

	// Track the furthest byte position we have processed so far
	uint32_t last_end_byte = 0;
	while (ts_query_cursor_next_capture(cursor, &match, &captureIndex))
	{
		TSQueryCapture capture = match.captures[captureIndex];

		// Get the text metadata of the captured node
		uint32_t start = ts_node_start_byte(capture.node);
		uint32_t end = ts_node_end_byte(capture.node);

		// 3. Skip this capture if it starts before the last processed node ends
		if (start < last_end_byte)
		{
			// Overlap detected (either nested inside or intersecting). Skip it!
			continue;
		}

		uint32_t name_length;
		const char* capture_name = ts_query_capture_name_for_id(query, capture.index, &name_length);

		ImVec4 codeColor = emu_SyntaxHighlighter_getColor(theme, capture_name);
		HighlightedCode value = (HighlightedCode)
		{
			.color = codeColor,
			.startByte = start,
			.endByte = end
		};

		if (lastValue.endByte < value.startByte && value.startByte > 0)
		{
			HighlightedCode fillerValue = (HighlightedCode){
				.color = theme->defaultColor,
				.startByte = lastValue.endByte,
				.endByte = value.startByte
			};
			stbds_arrput(codeHighlights, fillerValue);
		}

		lastValue = stbds_arrput(codeHighlights, value);

		// 5. Update the tracking boundary to the end of this non-overlapping node
		last_end_byte = end;
	}

	ts_query_cursor_delete(cursor);
	ts_query_delete(query);
}

static void drawColoredText()
{
	emu_CodeTheme const* theme = emu_SyntaxHighlighter_getTheme();

	// 1. Make the text color transparent so the cursor and box background still render normally
	int flags = ImGuiInputTextFlags_AllowTabInput;
	ImVec2 availableSize = ImGui_GetContentRegionAvail();
	emu_cimgui_pushFont(CImGui_FontType_Mono);

	ImGui_PushStyleColor(ImGuiCol_Text, IM_COL32_BLACK_TRANS);
	ImGui_PushStyleColor(ImGuiCol_FrameBg, ImGui_ColorConvertFloat4ToU32(theme->bgColor));
	ImGui_PushStyleColor(ImGuiCol_InputTextCursor, ImGui_ColorConvertFloat4ToU32(theme->cursorColor));
	ImGui_InputTextMultilineEx("##Source_Code", sourceCodeBuffer, sourceCodeBufferMaxLength, availableSize, flags, NULL, NULL);
	ImGuiContext* g = ImGui_GetCurrentContext();
	const char* child_window_name = NULL;
	cImFormatStringToTempBuffer(&child_window_name, NULL, "%s/%s_%08X", g->CurrentWindow->Name, "##Source_Code", ImGui_GetID("##Source_Code"));
	ImGuiWindow* window = ImGui_FindWindowByName(child_window_name);

	float scroll_x = 0.0f;
	float scroll_y = 0.0f;
	if (window)
	{
		scroll_x = window->Scroll.x;
		scroll_y = window->Scroll.y;
	}
	ImGui_PopStyleColor();
	ImGui_PopStyleColor();
	ImGui_PopStyleColor();

	// 2. Calculate the screen boundaries where the text was just drawn
	ImVec2 min_pos = ImGui_GetItemRectMin();
	ImVec2 max_pos = { .x = min_pos.x + availableSize.x, .y = min_pos.y + availableSize.y }; //ImGui_GetItemRectMax();

	// 3. Render your custom colored substrings manually inside that bounding region
	ImDrawList* draw_list = ImGui_GetWindowDrawList();
	ImDrawList_PushClipRect(draw_list, min_pos, max_pos, true);

	float xStart = min_pos.x + ImGui_GetStyle()->FramePadding.x - scroll_x;
	ImVec2 text_pos = (ImVec2){
		.x = xStart,
		.y = min_pos.y + ImGui_GetStyle()->FramePadding.y - scroll_y
	};

	// Example of drawing substrings manually with split colors
	ImGui_PushFont(ImGui_GetFont()); // Match the active font

	for (size_t i = 0; i < (size_t)stbds_arrlen(codeHighlights); i++)
	{
		char* start = sourceCodeBuffer + codeHighlights[i].startByte;
		char* end = sourceCodeBuffer + codeHighlights[i].endByte;
		uint32 color = ImGui_ColorConvertFloat4ToU32(codeHighlights[i].color);

		for (size_t charIndex = codeHighlights[i].startByte; charIndex < codeHighlights[i].endByte; charIndex++)
		{
			if (sourceCodeBuffer[charIndex] == '\n')
			{
				ImDrawList_AddTextEx(draw_list, text_pos, color, start, end);
				text_pos.y += ImGui_GetTextLineHeight();
				text_pos.x = xStart;

				start = sourceCodeBuffer + charIndex;
			}
		}

		ImDrawList_AddTextEx(draw_list, text_pos, color, start, end);
		ImVec2 textSize = ImGui_CalcTextSizeEx(start, end, false, -1.0f);
		text_pos.x += textSize.x;
	}

	ImGui_PopFont();
	ImDrawList_PopClipRect(draw_list);

	emu_cimgui_popFont();
}

void emu_CodeEditor_tick()
{
	emu_CodeTheme const* theme = emu_SyntaxHighlighter_getTheme();

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
					if (syntaxTree)
					{
						emu_SyntaxHighlighter_freeSource(syntaxTree);
						syntaxTree = NULL;
					}

					emu_file file;
					if (emu_file_read(filename, &file) == emu_fileResult_Success)
					{
						if (file.data_size + 1 < sourceCodeBufferMaxLength)
						{
							g_memory_copyMem(sourceCodeBuffer, file.data, file.data_size);
							sourceCodeBufferLength = (int)file.data_size;
							sourceCodeBuffer[file.data_size] = '\0';
							generateSyntaxTree();
						}
						emu_file_free(&file);
					}
					else
					{
						sourceCodeBuffer[0] = '\0';
						sourceCodeBufferLength = 0;
					}
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

	ImGui_PushStyleColor(ImGuiCol_WindowBg, ImGui_ColorConvertFloat4ToU32(theme->bgColor));
	if (ImGui_Begin("Code Editor", NULL, 0))
	{
		drawColoredText();
	}
	ImGui_PopStyleColor();

	ImGui_End();
}