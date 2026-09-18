#include "frontend/CodeEditor.h"
#include "Emulator/App.h"
#include "utils/FileHelper.h"
#include "utils/SafeVendor.h"

#include <dcimgui.h>

#define sourceCodeBufferMaxLength 4'096
static char sourceCodeBuffer[sourceCodeBufferMaxLength];
static int sourceCodeBufferLength;

void emu_CodeEditor_tick()
{
	if (ImGui_Begin("Code Editor", NULL, 0))
	{
		//nk_menubar_begin(ctx);
		//nk_layout_row_begin(ctx, NK_STATIC, 25, 5);
		//nk_layout_row_push(ctx, 45);
		//if (nk_menu_begin_label(ctx, "File", NK_TEXT_LEFT, nk_vec2(120, 200)))
		//{
		//	nk_layout_row_dynamic(ctx, 25, 1);
		//	if (nk_menu_item_label(ctx, "Open", NK_TEXT_LEFT))
		//	{
		//		const int numFileFilters = 2;
		//		const char* fileFilters[] = {"*.s", "*.txt"};
		//		const char* filename = emu_file_openFileDialog(numFileFilters, fileFilters);
		//		if (filename != NULL)
		//		{
		//			emu_file file;
		//			if (emu_file_read(filename, &file) == emu_fileResult_Success)
		//			{
		//				if (file.data_size + 1 < sourceCodeBufferMaxLength)
		//				{
		//					g_memory_copyMem(sourceCodeBuffer, file.data, file.data_size);
		//					sourceCodeBufferLength = (int)file.data_size;
		//					sourceCodeBuffer[file.data_size] = '\0';
		//				}
		//				emu_file_free(&file);
		//			}
		//		}
		//	}
		//	if (nk_menu_item_label(ctx, "Save", NK_TEXT_LEFT))
		//	{
		//		g_logger_info("Saving file");
		//	}
		//	nk_menu_end(ctx);
		//}
		//nk_menubar_end(ctx);

		//nk_edit_string(ctx, NK_EDIT_BOX, sourceCodeBuffer, &sourceCodeBufferLength, sourceCodeBufferMaxLength, nk_filter_default);
	}

	ImGui_End();
}