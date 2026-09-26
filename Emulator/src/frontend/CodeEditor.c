#include "frontend/CodeEditor.h"
#include "frontend/ImGuiLayer.h"
#include "frontend/SyntaxHighlighter.h"
#include "Emulator/App.h"
#include "Emulator/Assembler.h"
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

typedef struct CodeEditorPanel
{
	HighlightedCode* codeHighlights;
	TSTree* syntaxTree;
	char* sourceCodeBuffer;
	size_t sourceCodeBufferLength;
	size_t sourceCodeBufferCapacity;
	char* filename;
	size_t filenameLength;
	char* fullFilepath;
	size_t fullFilepathLength;
	int* breakpoints;
	bool open;

	int totalNumLines;
	int selectedLine;
	int cursorBytePos;
	bool dirty;
} CodeEditorPanel;

static CodeEditorPanel* panels;

// Declare the `tree_sitter_asm6502` function, which is
// implemented by the `tree-sitter-asm6502` library.
const TSLanguage* tree_sitter_asm6502(void);

static HighlightedCode* getAllCaptures(TSTree* syntaxTree, bool printCaptures, const char* source);
static TSTree* generateSyntaxTree(const char* sourceCodeBuffer, size_t sourceCodeBufferLength);
static TSTree* updateSyntaxTree(CodeEditorPanel* panel, const char* sourceCodeBuffer, size_t sourceCodeBufferLength);
static int handleTextResizing(CodeEditorPanel* panel, ImGuiInputTextCallbackData* data);
static void handleTextEdit(CodeEditorPanel* panel);
static int inputTextCallback(ImGuiInputTextCallbackData* data);
static void renderCodePanel(CodeEditorPanel* panel);
static void freePanel(CodeEditorPanel* panel);

void emu_CodeEditor_init()
{
	panels = NULL;
}

void emu_CodeEditor_free()
{
	for (int i = 0; i < stbds_arrlen(panels); i++)
	{
		freePanel(panels + i);
	}
}

void emu_CodeEditor_openFile(emu_app* app, const char* fullFilepath)
{
	CodeEditorPanel res = (CodeEditorPanel){ 0 };

	size_t fullFilepathLength = strlen(fullFilepath);
	size_t lastSlashIndex = 0;
	for (size_t i = fullFilepathLength; i > 0; i--)
	{
		if (fullFilepath[i] == '/' || fullFilepath[i] == '\\')
		{
			lastSlashIndex = i;
			break;
		}
	}

	// The extra -1 is for the / character
	size_t filenameLength = fullFilepathLength - lastSlashIndex - (lastSlashIndex == 0 ? 0 : 1);
	res.filename = g_memory_allocate(filenameLength + 1);
	g_memory_copyMem(res.filename, (char*)fullFilepath + lastSlashIndex + 1, filenameLength);
	res.filename[filenameLength] = '\0';
	res.filenameLength = filenameLength;

	res.fullFilepath = g_memory_allocate(fullFilepathLength + 1);
	g_memory_copyMem(res.fullFilepath, (char*)fullFilepath, fullFilepathLength);
	res.fullFilepathLength = fullFilepathLength;
	res.fullFilepath[fullFilepathLength] = '\0';

	emu_file file;
	if (emu_file_read(fullFilepath, &file) == emu_fileResult_Success)
	{
		// Multiply by 1.5 to get some buffer room
		res.sourceCodeBufferCapacity = (size_t)((float)file.data_size * 1.5f);
		res.sourceCodeBufferLength = file.data_size;
		res.sourceCodeBuffer = g_memory_allocate(res.sourceCodeBufferCapacity);

		g_memory_copyMem(res.sourceCodeBuffer, file.data, file.data_size);
		res.sourceCodeBuffer[file.data_size] = '\0';
		res.syntaxTree = generateSyntaxTree(res.sourceCodeBuffer, res.sourceCodeBufferLength);
		res.codeHighlights = getAllCaptures(res.syntaxTree, false, res.sourceCodeBuffer);
		emu_file_free(&file);

		// Calculate some stuff...
		res.totalNumLines = 1;
		for (size_t i = 0; i < res.sourceCodeBufferLength; i++)
		{
			if (res.sourceCodeBuffer[i] == '\n')
			{
				res.totalNumLines++;
			}
		}

		emu_app_loadProgram(app, fullFilepath);
	}
	else
	{
		const size_t defaultCapacity = 1'024;
		res.sourceCodeBuffer = g_memory_allocate(defaultCapacity);
		res.sourceCodeBufferCapacity = defaultCapacity;
		res.sourceCodeBufferLength = 0;
		res.sourceCodeBuffer[0] = '\0';
		res.syntaxTree = NULL;
		res.codeHighlights = NULL;
	}

	res.open = true;
	stbds_arrpush(panels, res);
}

void emu_CodeEditor_tick()
{
	emu_CodeTheme const* theme = emu_SyntaxHighlighter_getTheme();

	ImGui_PushStyleColor(ImGuiCol_WindowBg, ImGui_ColorConvertFloat4ToU32(theme->bgColor));
	if (ImGui_Begin("Code Editor", NULL, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
	{
		if (ImGui_BeginMenuBar())
		{
			if (ImGui_MenuItem("Debug"))
			{
				emu_cimgui_focusWindow(CImGui_WindowType_EmulatorDebug);
			}

			ImGui_EndMenuBar();
		}

		static ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs;
		if (ImGui_BeginTabBar("MyTabBar", tab_bar_flags))
		{
			for (int i = 0; i < stbds_arrlen(panels); i++)
			{
				CodeEditorPanel* panel = panels + i;
				ImGuiTabItemFlags flags = panel->dirty
					? ImGuiTabItemFlags_UnsavedDocument
					: ImGuiTabItemFlags_None;
				if (panel->open && ImGui_BeginTabItem(panel->filename, &panel->open, flags))
				{
					renderCodePanel(panel);
					ImGui_EndTabItem();
				}

				// Save file if needed
				if (panel->dirty && ImGui_Shortcut(ImGuiKey_S | ImGuiMod_Ctrl, 0))
				{
					panel->dirty = false;
					emu_file_write(panel->fullFilepath, (uint8*)panel->sourceCodeBuffer, panel->sourceCodeBufferLength);
				}

				if (!panel->open)
				{
					g_logger_info("Freeing tab '%s'!", panel->filename);
					freePanel(panel);
					stbds_arrdel(panels, i);
					i--;
				}
			}

			ImGui_EndTabBar();
		}
	}
	ImGui_PopStyleColor();

	ImGui_End();
}

// ----------- Internal Definitions -------------

static TSTree* generateSyntaxTree(const char* sourceCodeBuffer, size_t sourceCodeBufferLength)
{
	return emu_SyntaxHighlighter_highlightSource(sourceCodeBuffer, (uint32)sourceCodeBufferLength);
}

static TSTree* updateSyntaxTree(CodeEditorPanel* panel, const char* sourceCodeBuffer, size_t sourceCodeBufferLength)
{
	return emu_SyntaxHighlighter_editHighlights(panel->syntaxTree, sourceCodeBuffer, (uint32)sourceCodeBufferLength);
}

static HighlightedCode* getAllCaptures(TSTree* syntaxTree, bool printCaptures, const char* source)
{
	static const char* asmHighlightsQuery = "(opcode) @function.builtin\
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
	TSQuery* query = ts_query_new(tree_sitter_asm6502(), asmHighlightsQuery, (uint32)strlen(asmHighlightsQuery), &error_offset, &error_type);

	if (!query)
	{
		g_logger_error("Query creation failed at offset %d:%d", error_offset, error_type);
		return NULL;
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
	HighlightedCode* codeHighlights = NULL;
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

		if (printCaptures)
			g_logger_info("Capture<@%s>: %.*s", capture_name, end - start, source + start);
	}

	ts_query_cursor_delete(cursor);
	ts_query_delete(query);

	return codeHighlights;
}

static int handleTextResizing(CodeEditorPanel* panel, ImGuiInputTextCallbackData* data)
{
	if ((float)panel->sourceCodeBufferLength > (float)panel->sourceCodeBufferCapacity * 0.99f)
	{
		// If we exceed 99% of capacity, grow by 1.5
		panel->sourceCodeBufferCapacity = (size_t)((float)panel->sourceCodeBufferCapacity * 1.5f);
		panel->sourceCodeBuffer = g_memory_realloc(panel->sourceCodeBuffer, panel->sourceCodeBufferCapacity);

		data->Buf = panel->sourceCodeBuffer;
		data->BufSize = (int)panel->sourceCodeBufferCapacity;
	}
	return 0;
}

static void handleTextEdit(CodeEditorPanel* panel)
{
	// TODO: Disgustingly in-efficient. Fix this at some point...
	panel->sourceCodeBufferLength = strlen(panel->sourceCodeBuffer);
	panel->dirty = true;
	ts_tree_delete(panel->syntaxTree);
	panel->syntaxTree = generateSyntaxTree(panel->sourceCodeBuffer, panel->sourceCodeBufferLength);
	stbds_arrfree(panel->codeHighlights);
	panel->codeHighlights = getAllCaptures(panel->syntaxTree, false, panel->sourceCodeBuffer);
}

static int inputTextCallback(ImGuiInputTextCallbackData* data)
{
	CodeEditorPanel* panel = (CodeEditorPanel*)data->UserData;
	panel->cursorBytePos = data->CursorPos;

	switch (data->EventFlag)
	{
	case ImGuiInputTextFlags_CallbackResize:
		return handleTextResizing(panel, data);
	}

	return 0;
}

static void renderCodePanel(CodeEditorPanel* panel)
{
	emu_CodeTheme const* theme = emu_SyntaxHighlighter_getTheme();
	emu_cimgui_pushFont(CImGui_FontType_Mono);

	ImVec4 lineNumberColor = emu_SyntaxHighlighter_getColor(emu_SyntaxHighlighter_getTheme(), "ui.linenr");
	ImVec4 selectedLineNumberColor = emu_SyntaxHighlighter_getColor(emu_SyntaxHighlighter_getTheme(), "ui.linenr.selected");
	ImVec4 textFocusColor = emu_SyntaxHighlighter_getColor(emu_SyntaxHighlighter_getTheme(), "ui.text.focus");
	ImVec4 errorColor = emu_SyntaxHighlighter_getColor(emu_SyntaxHighlighter_getTheme(), "error");
	uint32 textFocusColorU32 = ImGui_ColorConvertFloat4ToU32(textFocusColor);
	uint32 errorColorU32 = ImGui_ColorConvertFloat4ToU32(errorColor);
	uint32 selectedLineNumberColorU32 = ImGui_ColorConvertFloat4ToU32(selectedLineNumberColor);

	float lineHeight = ImGui_GetTextLineHeight();
	float yFramePadding = ImGui_GetStyle()->FramePadding.y;
	float xFramePadding = ImGui_GetStyle()->FramePadding.x;

	// First draw the code gutter (the line numbers/breakpoints/etc)
	static int lineNumberStart = 0;
	static int numberOfLinesVisible = 0;
	static float gutterScrollOffset = 0.0f;
	float gutterWidth = ImGui_GetFontSize();
	if (ImGui_BeginChild(
		"##CodeEditor_Gutter_Breakpoints",
		(ImVec2)
	{
		0
	},
		ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AutoResizeX,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
	))
	{
		ImGui_SetCursorPosY(gutterScrollOffset);
		float cursorY = gutterScrollOffset;
		for (int line = lineNumberStart; line <= lineNumberStart + numberOfLinesVisible + 1; line++)
		{
			if (line != 0 && line <= panel->totalNumLines)
			{
				ImGui_SetCursorPosY(cursorY);
				const ImVec2 p1 = ImGui_GetCursorScreenPos();
				ImGui_Dummy((ImVec2) { .x = gutterWidth, .y = lineHeight });
				if (ImGui_IsItemHovered(0))
				{
					ImVec2 circlePos = {
						.x = p1.x + lineHeight / 2.0f,
						.y = p1.y + lineHeight / 2.0f
					};
					ImDrawList_AddCircleFilled(ImGui_GetWindowDrawList(), circlePos, lineHeight / 2.0f, selectedLineNumberColorU32, 0);
				}

				if (ImGui_IsItemClicked())
				{
					bool deleted = false;
					for (int i = 0; i < stbds_arrlen(panel->breakpoints); i++)
					{
						if (panel->breakpoints[i] == line)
						{
							stbds_arrdel(panel->breakpoints, i);
							deleted = true;
							break;
						}
					}

					if (!deleted)
					{
						stbds_arrpush(panels->breakpoints, line);
					}
				}
			}

			cursorY += lineHeight;
		}

		for (int i = 0; i < stbds_arrlen(panel->breakpoints); i++)
		{
			int breakpoint = panel->breakpoints[i];
			if (breakpoint >= lineNumberStart && breakpoint <= lineNumberStart + numberOfLinesVisible + 1)
			{
				ImGui_SetCursorPosY(gutterScrollOffset + lineHeight * (breakpoint - lineNumberStart));
				const ImVec2 p1 = ImGui_GetCursorScreenPos();
				ImVec2 circlePos = {
							.x = p1.x + lineHeight / 2.0f,
							.y = p1.y + lineHeight / 2.0f
				};
				ImDrawList_AddCircleFilled(ImGui_GetWindowDrawList(), circlePos, lineHeight / 2.0f, errorColorU32, 0);
			}
		}

		ImGui_EndChild();
	}

	ImGui_SameLineEx(0.0f, 0.0f);

	// Leave room for number of digits needed
	if (ImGui_BeginChild(
		"##CodeEditor_Gutter_LineNumbers",
		(ImVec2)
	{
		0
	},
		ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AutoResizeX,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoInputs
	))
	{
		int numDigitsNeeded = (int)log10((double)panel->totalNumLines) + 1;
		ImGui_SetCursorPosY(gutterScrollOffset);
		float cursorY = gutterScrollOffset;
		for (int line = lineNumberStart; line <= lineNumberStart + numberOfLinesVisible + 1; line++)
		{
			if (line != 0 && line <= panel->totalNumLines)
			{
				ImGui_SetCursorPosY(cursorY);
				if (line == panel->selectedLine)
				{
					ImGui_TextColored(selectedLineNumberColor, "%*d", numDigitsNeeded, line);
				}
				else
				{
					ImGui_TextColored(lineNumberColor, "%*d", numDigitsNeeded, line);
				}
			}

			cursorY += lineHeight;
		}

		ImGui_EndChild();
	}

	ImGui_SameLineEx(0.0f, 0.0f);
	ImGui_SetCursorPosX(ImGui_GetCursorPosX() + xFramePadding * 2.0f);

	// 1. Make the text color transparent so the cursor and box background still render normally
	int flags = ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CallbackAlways;
	ImVec2 availableSize = ImGui_GetContentRegionAvail();

	ImGui_PushStyleColor(ImGuiCol_Text, IM_COL32_BLACK_TRANS);
	ImGui_PushStyleColor(ImGuiCol_FrameBg, ImGui_ColorConvertFloat4ToU32(theme->bgColor));
	ImGui_PushStyleColor(ImGuiCol_InputTextCursor, ImGui_ColorConvertFloat4ToU32(theme->cursorColor));

	// Calculate scrollable area. It should be a bit bigger than the text so we can scroll past the bottom.
	float totalHeight = panel->totalNumLines * lineHeight;
	ImVec2 contentSize = {
		.x = availableSize.x,
		.y = (numberOfLinesVisible - 1) * lineHeight + totalHeight - yFramePadding * 2
	};
	ImGui_SetNextWindowContentSize(contentSize);
	if (ImGui_InputTextMultilineEx("##Source_Code", panel->sourceCodeBuffer, panel->sourceCodeBufferCapacity, availableSize, flags, inputTextCallback, (void*)panel))
	{
		handleTextEdit(panel);
	}
	ImGuiContext* g = ImGui_GetCurrentContext();
	const char* child_window_name = NULL;
	cImFormatStringToTempBuffer(&child_window_name, NULL, "%s/%s_%08X", g->CurrentWindow->Name, "##Source_Code", ImGui_GetID("##Source_Code"));
	ImGuiWindow* window = ImGui_FindWindowByName(child_window_name);

	float scrollX = 0.0f;
	float scrollY = 0.0f;
	if (window)
	{
		scrollX = window->Scroll.x;
		scrollY = window->Scroll.y;

		lineNumberStart = (int)((scrollY - yFramePadding) / lineHeight);
		numberOfLinesVisible = (int)(availableSize.y / lineHeight) + 1;
		gutterScrollOffset = (float)(lineNumberStart - 1) * lineHeight - scrollY + yFramePadding;
	}
	ImGui_PopStyleColor();
	ImGui_PopStyleColor();
	ImGui_PopStyleColor();

	// Subtract scrollbar width to make future rendering calculations easier
	availableSize.x -= ImGui_GetStyle()->ScrollbarSize + xFramePadding;

	// 2. Calculate the screen boundaries where the text was just drawn
	ImVec2 min_pos = ImGui_GetItemRectMin();
	ImVec2 max_pos = ImGui_GetItemRectMax();
	max_pos.x -= ImGui_GetStyle()->ScrollbarSize;

	// 3. Render your custom colored substrings manually inside that bounding region
	ImDrawList* draw_list = ImGui_GetWindowDrawList();
	ImDrawList_PushClipRect(draw_list, min_pos, max_pos, true);

	float xStart = min_pos.x + ImGui_GetStyle()->FramePadding.x - scrollX;
	ImVec2 text_pos = (ImVec2){
		.x = xStart,
		.y = min_pos.y + yFramePadding - scrollY
	};

	// Example of drawing substrings manually with split colors
	ImGui_PushFont(ImGui_GetFont()); // Match the active font

	int currentLine = 1;
	ImVec2 lineDrawStartPos = text_pos;
	ImVec2 lineDrawEndPos = { .x = text_pos.x + availableSize.x, text_pos.y + lineHeight };
	for (size_t i = 0; i < (size_t)stbds_arrlen(panel->codeHighlights); i++)
	{
		char* start = panel->sourceCodeBuffer + panel->codeHighlights[i].startByte;
		char* end = panel->sourceCodeBuffer + panel->codeHighlights[i].endByte;
		uint32 color = ImGui_ColorConvertFloat4ToU32(panel->codeHighlights[i].color);

		for (size_t charIndex = panel->codeHighlights[i].startByte; charIndex < panel->codeHighlights[i].endByte; charIndex++)
		{
			if (charIndex == panel->cursorBytePos)
			{
				panel->selectedLine = currentLine;

				// Draw a box around the line
				ImDrawList_AddRect(draw_list, lineDrawStartPos, lineDrawEndPos, textFocusColorU32);
			}

			if (panel->sourceCodeBuffer[charIndex] == '\n')
			{
				ImDrawList_AddTextEx(draw_list, text_pos, color, start, end);
				text_pos.y += lineHeight;
				text_pos.x = xStart;

				start = panel->sourceCodeBuffer + charIndex;
				currentLine++;

				lineDrawStartPos = text_pos;
				lineDrawEndPos = (ImVec2){ .x = text_pos.x + availableSize.x, text_pos.y + lineHeight };
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

static void freePanel(CodeEditorPanel* panel)
{
	if (panel->codeHighlights)
	{
		stbds_arrfree(panel->codeHighlights);
	}

	if (panel->breakpoints)
	{
		stbds_arrfree(panel->breakpoints);
	}

	if (panel->filename)
	{
		g_memory_free(panel->filename);
	}

	if (panel->fullFilepath)
	{
		g_memory_free(panel->fullFilepath);
	}

	if (panel->sourceCodeBuffer)
	{
		g_memory_free(panel->sourceCodeBuffer);
	}

	if (panel->syntaxTree)
	{
		ts_tree_delete(panel->syntaxTree);
	}

	*panel = (CodeEditorPanel){ 0 };
}