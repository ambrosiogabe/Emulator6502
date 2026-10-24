#ifndef EMU_NES_BINARY_FILE_H
#define EMU_NES_BINARY_FILE_H
#include "utils/SafeVendor.h"
#include "utils/FileHelper.h"

typedef struct emu_NesBinaryFile_Raw
{
	uint8* bin;
	size_t binSize;
} emu_NesBinaryFile_Raw;

typedef struct emu_NesBinaryFile
{
	struct
	{
		// -------- File Header --------
		uint32 fileId; // 'NES', 0x1A
		uint8 numPrgRomPages; // 16k PRG-ROM pages
		uint8 numChrRomPages; // 8k  CHR-ROM pages (0x00=None / VRAM)
		struct
		{
			uint8 mirroring : 1; // 0=HZ mirroring, 1=Vt mirroring
			uint8 batterySram : 1; // 1=Battery-backed SRAM at 0x6000-0x7000, Set only if battery-backed
			uint8 trainerPatch : 1; // 1=512-byte trainer/patch at 0x7000-0x71ff
			uint8 fourScreen : 1; // 1=Four-screen VRAM layout
			uint8 mapperNumberLo : 4;
		} cartridgeTypeLo;
		struct
		{
			uint8 unisystemGame : 1; // 1=VS Unisystem game (arcade machine with different palette)
			uint8 pc10Game : 1; // 1=PC10 game (arcade machine with additional 8K Z80-ROM)
			uint8 reserved : 2;
			uint8 mapperNumberHi : 4;
		} cartridgeTypeHi;
		uint8 numRamPages; // Usually 0x00=None-or-not-specified
		uint8 reserved[6];
		uint8 garbageData; // If non-zero, assume cartridgeType-reserved are all 0
	} header;

	// -------- File Contents --------
	uint8* binaryTrainer; // 512 bytes for binaryTrainer if trainerPatch is set, otherwise NULL
	uint8* prgRom; // 16K * numPrgRomPages
	uint8* chrRom; // 8K * numChrRomPages
	uint8* playChoiceZ80Rom; // NULL, unless 
	uint8* title;
} emu_NesBinaryFile;

emu_NesBinaryFile_Raw emu_NesBinaryFile_generateBinary(uint8 header[16], uint8* prgRom, size_t prgRomSize, uint8* chrRom, size_t chrRomSize, uint8* title, size_t titleSize);
emu_NesBinaryFile emu_NesBinaryFile_parse(emu_file* file);
void emu_NesBinaryFile_free(emu_NesBinaryFile* const file);
void emu_NesBinaryFile_freeRaw(emu_NesBinaryFile_Raw* const file);

#endif