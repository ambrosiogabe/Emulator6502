#ifndef EMU_CODE_EDITOR_H
#define EMU_CODE_EDITOR_H
#include "utils/SafeVendor.h"

typedef struct emu_app emu_app;

void emu_CodeEditor_init();
void emu_CodeEditor_free();

void emu_CodeEditor_openFile(emu_app* app, const char* filename);

void emu_CodeEditor_tick(emu_app* app);

size_t emu_CodeEditor_getNumOpenFiles();
const char* emu_CodeEditor_getFullFilepath(size_t index);

#endif