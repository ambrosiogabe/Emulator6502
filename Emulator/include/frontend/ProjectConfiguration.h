#ifndef EMU_PROJECT_CONFIGURATION_H
#define EMU_PROJECT_CONFIGURATION_H

typedef struct emu_app emu_app;

void emu_projectConfiguration_tick(emu_app* app);

void emu_projectConfiguration_open();
void emu_projectConfiguration_close();

#endif 