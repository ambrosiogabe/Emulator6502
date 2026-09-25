#ifndef EMU_EMULATOR_DEBUG_H
#define EMU_EMULATOR_DEBUG_H

typedef struct emu_MemoryMap emu_MemoryMap;
typedef struct emu_app emu_app;

void emu_EmulatorDebug_tick(emu_app* app);

void emu_EmulatorDebug_beginDebugging(emu_app* app);
void emu_EmulatorDebug_endDebugging();

#endif 