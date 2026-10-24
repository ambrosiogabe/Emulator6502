#include "Emulator/NesBinaryFile.h"
#include "Emulator/Types.h"

emu_NesBinaryFile_Raw emu_NesBinaryFile_generateBinary(uint8 header[16], uint8* prgRom, size_t prgRomSize, uint8* chrRom, size_t chrRomSize, uint8* title, size_t titleSize)
{
	uint8 numPrgRomPages = header[4];
	uint8 numChrRomPages = header[5];
	size_t truePrgRomSize = numPrgRomPages * KB(16);
	size_t trueChrRomSize = numChrRomPages * KB(8);
	size_t binarySize = 16
		+ truePrgRomSize
		+ trueChrRomSize
		+ titleSize;
	g_logger_assert(truePrgRomSize <= prgRomSize, "Incorrect mapped file. PRG ROM.");
	g_logger_assert(trueChrRomSize <= chrRomSize, "Incorrect mapped file. CHR ROM.");

	uint8* binary = g_memory_allocate(binarySize);
	uint8* current = binary;
	g_memory_copyMem(current, header, 16);
	current += 16;
	// TODO: Implement trainer (maybe important for some games?)
	g_memory_copyMem(current, prgRom, truePrgRomSize);
	current += truePrgRomSize;
	// Copy hardware vectors over last 6 bytes

	// Fix 
	g_memory_copyMem(current, chrRom, trueChrRomSize);
	current += trueChrRomSize;
	// TODO: Implement 8K Play Choice Z80-ROM
	g_memory_copyMem(current, title, titleSize);

	g_logger_assert((size_t)((current + titleSize) - binary) <= binarySize, "Did not allocate enough memory for binary.");

	return (emu_NesBinaryFile_Raw)
	{
		.bin = binary,
			.binSize = (current + titleSize) - binary,
	};
}

emu_NesBinaryFile emu_NesBinaryFile_parse(emu_file* file)
{
	uint8* binary = (uint8*)file->data;
	size_t binarySize = file->data_size;

	emu_NesBinaryFile res = { 0 };
	uint8* current = binary;
	size_t currentSize = 0;
	if (currentSize + sizeof(res.header) < binarySize)
	{
		g_memory_copyMem(&res.header, current, sizeof(res.header));
		currentSize += sizeof(res.header);
		current += sizeof(res.header);
	}
	else
	{
		g_logger_warning("Cannot parse file as NES file. Not large enough to contain NES header.");
		return res;
	}

	if (res.header.cartridgeTypeLo.trainerPatch && currentSize + 512 < binarySize)
	{
		res.binaryTrainer = g_memory_allocate(512);
		g_memory_copyMem(res.binaryTrainer, current, 512);
		currentSize += 512;
		current += 512;
	}
	else if (res.header.cartridgeTypeLo.trainerPatch)
	{
		g_logger_warning("Cannot parse file as NES file. Header specified binary trainer, but file is not large enough to parse a trainer ROM.");
		return res;
	}

	size_t prgRomSize = res.header.numPrgRomPages * KB(16);
	if (currentSize + prgRomSize < binarySize)
	{
		res.prgRom = g_memory_allocate(prgRomSize);
		g_memory_copyMem(res.prgRom, current, prgRomSize);
		currentSize += prgRomSize;
		current += prgRomSize;
	}
	else
	{
		g_logger_warning("Cannot parse file as NES file. Header specified '%d' PRG-ROM banks, but file is not large enough to hold that many PRG-ROM pages", res.header.numPrgRomPages);
		return res;
	}

	size_t chrRomSize = res.header.numChrRomPages * KB(1);
	if (currentSize + chrRomSize < binarySize)
	{
		res.chrRom = g_memory_allocate(chrRomSize);
		g_memory_copyMem(res.chrRom, current, chrRomSize);
		currentSize += chrRomSize;
		current += chrRomSize;
	}
	else
	{
		g_logger_warning("Cannot parse file as NES file. Header specified '%d' CHR-ROM banks, but file is not large enough to hold that many CHR-ROM pages", res.header.numChrRomPages);
		return res;
	}

	if (res.header.cartridgeTypeHi.pc10Game && currentSize + KB(8) < binarySize)
	{
		res.playChoiceZ80Rom = g_memory_allocate(KB(8));
		// TODO: Implement play choice ROM
		currentSize += KB(8);
		current += KB(8);
	}
	else if (res.header.cartridgeTypeHi.pc10Game)
	{
		g_logger_warning("Cannot parse file as NES file. Header specified PC-10 game, but file is not large enough to hold Z80-ROM.");
		return res;
	}

	// If we have 127 or 128 extra bytes left, this file has a title
	if (currentSize + 128 < binarySize || currentSize + 127 < binarySize)
	{
		size_t numBytes = currentSize + 128 < binarySize ? 128 : 127;
		res.title = g_memory_allocate(numBytes);
		g_memory_copyMem(res.title, current, numBytes);
		currentSize += numBytes;
		current += numBytes;
	}

	// Sanity check
	if (currentSize != binarySize && (size_t)(current - binary) != binarySize - 1)
	{
		g_logger_error("We read less bytes than the NES file had available to us. We only read '%d' out of '%d' bytes.", currentSize, binarySize);
	}

	return res;
}

void emu_NesBinaryFile_free(emu_NesBinaryFile* const file)
{
	if (file->binaryTrainer)
	{
		g_memory_free(file->binaryTrainer);
	}

	if (file->chrRom)
	{
		g_memory_free(file->chrRom);
	}

	if (file->playChoiceZ80Rom)
	{
		g_memory_free(file->playChoiceZ80Rom);
	}

	if (file->prgRom)
	{
		g_memory_free(file->prgRom);
	}

	if (file->title)
	{
		g_memory_free(file->title);
	}

	g_memory_zeroMem(file, sizeof(emu_NesBinaryFile));
}

void emu_NesBinaryFile_freeRaw(emu_NesBinaryFile_Raw* const file)
{
	if (file->bin)
	{
		g_memory_free(file->bin);
	}

	g_memory_zeroMem(file, sizeof(emu_NesBinaryFile_Raw));
}