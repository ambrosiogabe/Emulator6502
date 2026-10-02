#ifndef EMU_FILE_EXPLORER_H
#define EMU_FILE_EXPLORER_H

typedef struct emu_app emu_app;

void emu_FileExplorer_init(const char* directory);
void emu_FileExplorer_tick(emu_app* app);
void emu_FileExplorer_free();

#endif 