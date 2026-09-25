#include "frontend/ConsoleOutput.h"
#include "frontend/ImGuiLayer.h"
#include "utils/SafeVendor.h"

#include <IconsFontAwesome7.h>
#include <IconsFontAwesome7Brands.h>

#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include <dcimgui.h>
#include <stb/stb_ds.h>

typedef struct LogMessage
{
	char* message;
	int messageLength;
	char* timestampStr;
	emu_ConsoleLogLevel level;
} LogMessage;

static LogMessage* messages = NULL;

static LogMessage getFormattedString(const char* msg, emu_ConsoleLogLevel level, va_list args);

void emu_ConsoleOutput_init()
{

}

void emu_ConsoleOutput_free()
{
	for (int i = 0; i < stbds_arrlen(messages); i++)
	{
		if (messages[i].message) g_memory_free(messages[i].message);
		if (messages[i].timestampStr) g_memory_free(messages[i].timestampStr);
	}

	stbds_arrfree(messages);
	messages = NULL;
}

void emu_ConsoleOutput_tick()
{
	if (ImGui_Begin("Console Output", NULL, ImGuiWindowFlags_MenuBar))
	{
		if (ImGui_BeginMenuBar())
		{
			if (ImGui_MenuItem("Clear"))
			{
				emu_ConsoleOutput_free();
			}

			ImGui_EndMenuBar();
		}

		for (int i = 0; i < stbds_arrlen(messages); i++)
		{
			const char* levelMsg =
				messages[i].level == emu_ConsoleLogLevel_Info ? ICON_FA_CIRCLE_INFO
				: messages[i].level == emu_ConsoleLogLevel_Warning ? ICON_FA_TRIANGLE_EXCLAMATION
				: ICON_FA_CIRCLE_XMARK;
			uint32 color = messages[i].level == emu_ConsoleLogLevel_Info ? 0xffF2EBB3
				: messages[i].level == emu_ConsoleLogLevel_Warning ? 0xff02D5E9
				: 0xff6C74FF;

			ImGui_Spacing();
			ImGui_Spacing();

			ImGui_TextColored(ImGui_ColorConvertU32ToFloat4(color), levelMsg);
			ImGui_SameLineEx(0.0f, 18.0f);
			

			emu_cimgui_pushFont(CImGui_FontType_Mono);
			if (messages[i].timestampStr)
			{
				ImGui_Text("[%s]: %s", messages[i].timestampStr, messages[i].message);
			}
			else
			{
				ImGui_Text("%s", messages[i].message);
			}
			emu_cimgui_popFont();

			ImGui_Spacing();
			ImGui_Spacing();
			ImGui_Separator();
		}
	}
	ImGui_End();
}

void emu_ConsoleOutput_info(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	LogMessage msg = getFormattedString(format, emu_ConsoleLogLevel_Info, args);
	va_end(args);

	stbds_arrput(messages, msg);
}

void emu_ConsoleOutput_warn(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	LogMessage msg = getFormattedString(format, emu_ConsoleLogLevel_Warning, args);
	va_end(args);

	stbds_arrput(messages, msg);
}

void emu_ConsoleOutput_error(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	LogMessage msg = getFormattedString(format, emu_ConsoleLogLevel_Error, args);
	va_end(args);

	stbds_arrput(messages, msg);
}

// ------------- Internal ------------------
static LogMessage getFormattedString(const char* format, emu_ConsoleLogLevel level, va_list args)
{
#define bufferSize 1'024
	static char buffer[bufferSize];

	int strLength = vsnprintf(buffer, bufferSize, format, args);

	char* res = g_memory_allocate(strLength + 1);
	g_memory_copyMem(res, buffer, strLength);
	res[strLength] = '\0';

	char* timeBufferStr = NULL;
	struct timespec ts;

	// Get current time with nanosecond precision
	if (timespec_get(&ts, TIME_UTC))
	{
		// Convert seconds to a readable local time structure
		struct tm* timeInfo = localtime(&ts.tv_sec);

		// Format buffer for date and time
		char timeBuffer[70];
		size_t timeBufferLength = strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", timeInfo);

		if (timeBufferLength > 0)
		{
			timeBufferStr = g_memory_allocate(timeBufferLength + 1);
			g_memory_copyMem(timeBufferStr, timeBuffer, timeBufferLength);
			timeBufferStr[timeBufferLength] = '\0';
		}
	}

	return (LogMessage)
	{
		.level = level,
			.message = res,
			.messageLength = strLength,
			.timestampStr = timeBufferStr
	};
#undef bufferSize
}