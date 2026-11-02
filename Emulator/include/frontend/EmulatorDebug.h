#ifndef EMU_EMULATOR_DEBUG_H
#define EMU_EMULATOR_DEBUG_H

typedef struct emu_MemoryMap emu_MemoryMap;
typedef struct emu_app emu_app;
typedef struct emu_virtualMachine emu_virtualMachine;

void emu_EmulatorDebug_tick(emu_app* app);

void emu_EmulatorDebug_generateDebugTextures(emu_app* app, emu_virtualMachine* vm);

void emu_EmulatorDebug_beginDebugging(emu_app* app);
void emu_EmulatorDebug_endDebugging();

#endif 