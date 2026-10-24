#ifndef EMU_ROM_MAPPERS_H
#define EMU_ROM_MAPPERS_H
#include "utils/SafeVendor.h"
#include "emulator/Types.h"

typedef struct emu_Mapper_Vrc6
{
	uint8 prgRom[KB(16)];
	uint8 chrRom[KB(8)];
	uint8 vRom[KB(1)];

	uint8 dataBus;
};

const size_t s = sizeof(emu_Mapper_Vrc6);
#endif 