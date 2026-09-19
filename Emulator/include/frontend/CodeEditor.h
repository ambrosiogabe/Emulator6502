#ifndef EMU_CODE_EDITOR_H
#define EMU_CODE_EDITOR_H
#include "utils/SafeVendor.h"

void emu_CodeEditor_init();
void emu_CodeEditor_free();

void emu_CodeEditor_openFile(const char* filename);

void emu_CodeEditor_tick();

#endif