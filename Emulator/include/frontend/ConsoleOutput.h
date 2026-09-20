#ifndef EMU_CONSOLE_OUTPUT_H
#define EMU_CONSOLE_OUTPUT_H

typedef enum emu_ConsoleLogLevel
{
	emu_ConsoleLogLevel_Info,
	emu_ConsoleLogLevel_Warning,
	emu_ConsoleLogLevel_Error,
} emu_ConsoleLogLevel;

void emu_ConsoleOutput_init();
void emu_ConsoleOutput_free();

void emu_ConsoleOutput_tick();

void emu_ConsoleOutput_info(const char* fmt, ...);
void emu_ConsoleOutput_warn(const char* fmt, ...);
void emu_ConsoleOutput_error(const char* fmt, ...);

#endif 