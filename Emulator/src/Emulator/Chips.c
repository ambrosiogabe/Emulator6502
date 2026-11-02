#include "Emulator/Chips.h"
#include "Emulator/VirtualMachine.h"
#include "Emulator/App.h"
#include "frontend/ConsoleOutput.h"
#include "frontend/EmulatorDebug.h"

// -------------------- Internal Functions --------------------
static void handlePpuRead(emu_virtualMachine* vm);
static void handlePpuWrite(emu_app* app, emu_virtualMachine* vm);

const char* emu_Chips_Strings[emu_Chips_Length] = {
	"emu_Chips_Null",
	"emu_Chips_Cpu",
	"emu_Chips_Ppu",
	"emu_Chips_Sram",
	"emu_Chips_CartridgeSram",
	"emu_Chips_CartridgeExpansion",
	"emu_Chips_PrgRom",
	"emu_Chips_ChrRom",
	"emu_Chips_Apu",
};

static bool rwToBinary(emu_RW rw)
{
	return rw == emu_RW_Read;
}

static emu_RW binaryToRw(uint8 value)
{
	return value == 0
		? emu_RW_Write
		: emu_RW_Read;
}

void emu_chip_setReadWrite(emu_virtualMachine* vm, emu_RW readWrite)
{
	vm->cpuChip.writePins.readWrite = rwToBinary(readWrite);
	switch (vm->chipEnable)
	{
	case emu_Chips_Apu:
	case emu_Chips_CartridgeExpansion:
	case emu_Chips_CartridgeSram:
	case emu_Chips_ChrRom:
	case emu_Chips_Cpu:
		g_logger_warning("Cannot handle setting read write to chip: '%s'", emu_Chips_Strings[vm->chipEnable]);
		break;
	case emu_Chips_PrgRom:
		vm->cartridge.pins.cpuReadWrite = rwToBinary(readWrite);
		break;
	case emu_Chips_Ppu:
		vm->ppuChip.pins.readWrite = rwToBinary(readWrite);
		break;
	case emu_Chips_Sram:
		vm->staticRam.pins.readWrite = rwToBinary(readWrite);
		break;
	case emu_Chips_Null:
		g_logger_warning("Trying to write to null chip data bus");
		break;
	}
}

void emu_chip_cycle(emu_app* app, emu_virtualMachine* vm)
{
	if (binaryToRw(vm->cpuChip.writePins.readWrite) == emu_RW_Read)
	{
		switch (vm->chipEnable)
		{
		case emu_Chips_Apu:
		case emu_Chips_CartridgeExpansion:
		case emu_Chips_CartridgeSram:
		case emu_Chips_ChrRom:
		case emu_Chips_Cpu:
			g_logger_warning("Cannot handle reading from chip: '%s'", emu_Chips_Strings[vm->chipEnable]);
			break;
		case emu_Chips_PrgRom:
			// TODO: Add support for mappers
			vm->cartridge.pins.cpuDataBus = vm->cartridge.prgRom[vm->cartridge.pins.cpuAddressBus - 0x8000];
			vm->cpuChip.readWritePins.dataBus = vm->cartridge.pins.cpuDataBus;
			break;
		case emu_Chips_Ppu:
			handlePpuRead(vm);
			break;
		case emu_Chips_Sram:
			vm->staticRam.pins.dataBus = vm->staticRam.ram[vm->staticRam.pins.addressBus];
			vm->cpuChip.readWritePins.dataBus = vm->staticRam.pins.dataBus;
			break;
		case emu_Chips_Null:
			g_logger_warning("Trying to write to null chip data bus");
			break;
		}
	}
	else
	{
		switch (vm->chipEnable)
		{
		case emu_Chips_Apu:
		case emu_Chips_CartridgeExpansion:
		case emu_Chips_CartridgeSram:
		case emu_Chips_ChrRom:
		case emu_Chips_Cpu:
			g_logger_warning("Cannot handle writing to chip: '%s'", emu_Chips_Strings[vm->chipEnable]);
			break;
		case emu_Chips_PrgRom:
			g_logger_warning("Illegal. Cannot write to ROM chip: '%s'", emu_Chips_Strings[vm->chipEnable]);
			emu_ConsoleOutput_error("Illegal. Cannot write to PRG ROM.");
			break;
		case emu_Chips_Ppu:
			handlePpuWrite(app, vm);
			break;
		case emu_Chips_Sram:
			vm->staticRam.ram[vm->staticRam.pins.addressBus] = vm->staticRam.pins.dataBus;
			break;
		case emu_Chips_Null:
			g_logger_warning("Trying to write to null chip data bus");
			break;
		}
	}
}

void emu_chip_cpuWriteAddress(emu_virtualMachine* vm, uint16 address)
{
	vm->cpuChip.writePins.addressBus = address;
	vm->chipEnable = emu_Chips_Null;

	if (emu_mmap_inRange(&vm->mmap.sram, address))
	{
		vm->chipEnable = emu_Chips_Sram;
		vm->staticRam.pins.addressBus = address;
	}
	else if (emu_mmap_inRange(&vm->mmap.ppuRegisterRange, address))
	{
		vm->chipEnable = emu_Chips_Ppu;
	}
	else if (emu_mmap_inRange(&vm->mmap.apuRegisterRange, address))
	{
		vm->chipEnable = emu_Chips_Apu;
	}
	else if (emu_mmap_inRange(&vm->mmap.cartridgeExpansion, address))
	{
		vm->chipEnable = emu_Chips_CartridgeExpansion;
	}
	else if (emu_mmap_inRange(&vm->mmap.cartridgeSram, address))
	{
		vm->chipEnable = emu_Chips_CartridgeSram;
	}
	else if (emu_mmap_inRange(&vm->mmap.rom, address) || emu_mmap_inRange(&vm->mmap.romv, address))
	{
		vm->chipEnable = emu_Chips_PrgRom;
		vm->cartridge.pins.cpuAddressBus = address;
	}
	else
	{
		g_logger_warning("Cannot handle address '%4X'. Don't know what chip to enable.", address);
	}
}

void emu_chip_cpuWriteToDataBus(emu_virtualMachine* vm, uint8 data)
{
	vm->cpuChip.readWritePins.dataBus = data;

	switch (vm->chipEnable)
	{
	case emu_Chips_Apu:
	case emu_Chips_CartridgeExpansion:
	case emu_Chips_CartridgeSram:
	case emu_Chips_ChrRom:
	case emu_Chips_Cpu:
	case emu_Chips_PrgRom:
		g_logger_warning("Cannot write to data bus for chip: '%s'", emu_Chips_Strings[vm->chipEnable]);
		break;
	case emu_Chips_Ppu:
		vm->ppuChip.pins.dataBus = data;
		break;
	case emu_Chips_Sram:
		vm->staticRam.pins.dataBus = data;
		break;
	case emu_Chips_Null:
		g_logger_warning("Trying to write to null chip data bus");
		break;
	}
}

uint8 emu_chip_cpuReadDataBus(emu_virtualMachine* vm)
{
	return vm->cpuChip.readWritePins.dataBus;
}

void emu_chip_ppuLoadPatternTables(emu_virtualMachine* vm, uint8* chrRom, size_t chrRomSize)
{
	g_logger_assert(chrRomSize <= KB(8), "CHR ROM is too big to copy into PPU pattern tables.");
	size_t patternTable0Size = chrRomSize > KB(4) ? KB(4) : chrRomSize;
	g_memory_copyMem(vm->ppuChip.patternTable0, chrRom, patternTable0Size);
	size_t patternTable1Size = chrRomSize > patternTable0Size ? chrRomSize - patternTable0Size : 0;
	g_memory_copyMem(vm->ppuChip.patternTable1, chrRom + patternTable0Size, patternTable1Size);
}

emu_Tile emu_chip_ppuLoadTile(emu_virtualMachine* vm, uint8 tile)
{
	uint8* patternTable = tile > 0xff ? vm->ppuChip.patternTable1 : vm->ppuChip.patternTable0;

	size_t byteOffset = tile;
	if (patternTable == vm->ppuChip.patternTable1)
	{
		byteOffset = tile - 0xff;
	}
	byteOffset *= 16;

	if (byteOffset > KB(4) - 8)
	{
		g_logger_error("Ran out of room %d", byteOffset);
		return (emu_Tile) { 0 };
	}

	emu_Tile res = { 0 };
	for (uint8 byte = 0; byte < 8; byte++)
	{
		uint8 loByte = patternTable[byte + byteOffset];
		uint8 hiByte = patternTable[byte + byteOffset + 8];
		for (int8 bit = 7; bit >= 0; bit--)
		{
			uint8 hiBit = (hiByte >> bit) & 0x1;
			uint8 loBit = (loByte >> bit) & 0x1;
			uint8 pixel = (hiBit << 1) | loBit;

			uint8 y = byte;
			uint8 x = 7 - bit;
			res.pixels[y][x] = pixel;
		}
	}

	return res;
}

uint32 emu_chip_ppuPaletteColorToU32(uint8 paletteColor)
{
	static int32 paletteColors[64] = {
		(0xFF << 24) | (0x80 << 0) | (0x80 << 8) | (0x80 << 16), (0xFF << 24) | (0x00 << 0) | (0x3D << 8) | (0xA6 << 16), 
		(0xFF << 24) | (0x00 << 0) | (0x12 << 8) | (0xB0 << 16), (0xFF << 24) | (0x44 << 0) | (0x00 << 8) | (0x96 << 16), 
		(0xFF << 24) | (0xA1 << 0) | (0x00 << 8) | (0x5E << 16), (0xFF << 24) | (0xC7 << 0) | (0x00 << 8) | (0x28 << 16),
		(0xFF << 24) | (0xBA << 0) | (0x06 << 8) | (0x00 << 16), (0xFF << 24) | (0x8C << 0) | (0x17 << 8) | (0x00 << 16),
		(0xFF << 24) | (0x5C << 0) | (0x2F << 8) | (0x00 << 16), (0xFF << 24) | (0x10 << 0) | (0x45 << 8) | (0x00 << 16),
		(0xFF << 24) | (0x05 << 0) | (0x4A << 8) | (0x00 << 16), (0xFF << 24) | (0x00 << 0) | (0x47 << 8) | (0x2E << 16),
		(0xFF << 24) | (0x00 << 0) | (0x41 << 8) | (0x66 << 16), (0xFF << 24) | (0x00 << 0) | (0x00 << 8) | (0x00 << 16),
		(0xFF << 24) | (0x05 << 0) | (0x05 << 8) | (0x05 << 16), (0xFF << 24) | (0x05 << 0) | (0x05 << 8) | (0x05 << 16),
		(0xFF << 24) | (0xC7 << 0) | (0xC7 << 8) | (0xC7 << 16), (0xFF << 24) | (0x00 << 0) | (0x77 << 8) | (0xFF << 16),
		(0xFF << 24) | (0x21 << 0) | (0x55 << 8) | (0xFF << 16), (0xFF << 24) | (0x82 << 0) | (0x37 << 8) | (0xFA << 16),
		(0xFF << 24) | (0xEB << 0) | (0x2F << 8) | (0xB5 << 16), (0xFF << 24) | (0xFF << 0) | (0x29 << 8) | (0x50 << 16), 
		(0xFF << 24) | (0xFF << 0) | (0x22 << 8) | (0x00 << 16), (0xFF << 24) | (0xD6 << 0) | (0x32 << 8) | (0x00 << 16),
		(0xFF << 24) | (0xC4 << 0) | (0x62 << 8) | (0x00 << 16), (0xFF << 24) | (0x35 << 0) | (0x80 << 8) | (0x00 << 16),
		(0xFF << 24) | (0x05 << 0) | (0x8F << 8) | (0x00 << 16), (0xFF << 24) | (0x00 << 0) | (0x8A << 8) | (0x55 << 16),
		(0xFF << 24) | (0x00 << 0) | (0x99 << 8) | (0xCC << 16), (0xFF << 24) | (0x21 << 0) | (0x21 << 8) | (0x21 << 16),
		(0xFF << 24) | (0x09 << 0) | (0x09 << 8) | (0x09 << 16), (0xFF << 24) | (0x09 << 0) | (0x09 << 8) | (0x09 << 16),
		(0xFF << 24) | (0xFF << 0) | (0xFF << 8) | (0xFF << 16), (0xFF << 24) | (0x0F << 0) | (0xD7 << 8) | (0xFF << 16),
		(0xFF << 24) | (0x69 << 0) | (0xA2 << 8) | (0xFF << 16), (0xFF << 24) | (0xD4 << 0) | (0x80 << 8) | (0xFF << 16), 
		(0xFF << 24) | (0xFF << 0) | (0x45 << 8) | (0xF3 << 16), (0xFF << 24) | (0xFF << 0) | (0x61 << 8) | (0x8B << 16), 
		(0xFF << 24) | (0xFF << 0) | (0x88 << 8) | (0x33 << 16), (0xFF << 24) | (0xFF << 0) | (0x9C << 8) | (0x12 << 16),
		(0xFF << 24) | (0xFA << 0) | (0xBC << 8) | (0x20 << 16), (0xFF << 24) | (0x9F << 0) | (0xE3 << 8) | (0x0E << 16),
		(0xFF << 24) | (0x2B << 0) | (0xF0 << 8) | (0x35 << 16), (0xFF << 24) | (0x0C << 0) | (0xF0 << 8) | (0xA4 << 16),
		(0xFF << 24) | (0x05 << 0) | (0xFB << 8) | (0xFF << 16), (0xFF << 24) | (0x5E << 0) | (0x5E << 8) | (0x5E << 16), 
		(0xFF << 24) | (0x0D << 0) | (0x0D << 8) | (0x0D << 16), (0xFF << 24) | (0x0D << 0) | (0x0D << 8) | (0x0D << 16), 
		(0xFF << 24) | (0xFF << 0) | (0xFF << 8) | (0xFF << 16), (0xFF << 24) | (0xA6 << 0) | (0xFC << 8) | (0xFF << 16),
		(0xFF << 24) | (0xB3 << 0) | (0xEC << 8) | (0xFF << 16), (0xFF << 24) | (0xDA << 0) | (0xAB << 8) | (0xEB << 16),
		(0xFF << 24) | (0xFF << 0) | (0xA8 << 8) | (0xF9 << 16), (0xFF << 24) | (0xFF << 0) | (0xAB << 8) | (0xB3 << 16),
		(0xFF << 24) | (0xFF << 0) | (0xD2 << 8) | (0xB0 << 16), (0xFF << 24) | (0xFF << 0) | (0xEF << 8) | (0xA6 << 16),
		(0xFF << 24) | (0xFF << 0) | (0xF7 << 8) | (0x9C << 16), (0xFF << 24) | (0xD7 << 0) | (0xE8 << 8) | (0x95 << 16),
		(0xFF << 24) | (0xA6 << 0) | (0xED << 8) | (0xAF << 16), (0xFF << 24) | (0xA2 << 0) | (0xF2 << 8) | (0xDA << 16),
		(0xFF << 24) | (0x99 << 0) | (0xFF << 8) | (0xFC << 16), (0xFF << 24) | (0xDD << 0) | (0xDD << 8) | (0xDD << 16),
		(0xFF << 24) | (0x11 << 0) | (0x11 << 8) | (0x11 << 16), (0xFF << 24) | (0x11 << 0) | (0x11 << 8) | (0x11 << 16)
	};

	if (paletteColor >= 64)
	{
		g_logger_error("Tried to access palette color out of range. No color: %0x4 exists", paletteColor);
		return 0;
	}

	return (uint32)paletteColors[paletteColor];
}

// -------------------- Internal Functions --------------------
static void handlePpuRead(emu_virtualMachine* vm)
{
	uint16 address = vm->cpuChip.writePins.addressBus;

	// PPUSTATUS Read
	if (address == 0x2002)
	{
		uint8 data = vm->ppuChip.status & 0b10000000;
		vm->ppuChip.pins.dataBus = data;
		vm->cpuChip.readWritePins.dataBus = data;

		// Reading this address clears bit 7 (Vblank flag) and returns it
		vm->ppuChip.status &= 0b01111111;
		// It alsdo clears internal w register
		vm->ppuChip.wReg = 0;
	}
	// OAMData Sprite ram data Read
	else if (address == 0x2004)
	{
		// TODO: Implement me
	}
}

static void handlePpuWrite(emu_app* app, emu_virtualMachine* vm)
{
	uint16 address = vm->cpuChip.writePins.addressBus;

	// OAM Addr, sprite RAM address write
	if (address == 0x2003)
	{
		uint8 loByte = vm->ppuChip.pins.dataBus;
		vm->cpuChip.oamAddr = loByte;
	}
	// OAMData Sprite ram data Write
	else if (address == 0x2004)
	{
		// Write the data we just read from the CPU to the oam ram
		vm->ppuChip.oamRam[vm->ppuChip.wReg] = vm->ppuChip.pins.dataBus;
		// Then, increment the oam register
		vm->ppuChip.wReg++;
		// Then, increment the oam address register
		uint16 fullAddress = (vm->cpuChip.oamDma << 8) | vm->cpuChip.oamAddr;
		fullAddress++;
		vm->cpuChip.oamDma = fullAddress >> 8;
		vm->cpuChip.oamAddr = fullAddress & 0xf;
	}
	// PPU Addr VRAM Address write
	else if (address == 0x2006)
	{
		uint8 byte = vm->ppuChip.pins.dataBus;
		// Writing hi byte
		if (vm->ppuChip.wReg == 0)
		{
			vm->ppuChip.tReg = 0;
			vm->ppuChip.vReg = ((byte << 8) & 0b0011'1111'1111'1111);
			vm->ppuChip.wReg++;
		}
		// Writing lo byte
		else if (vm->ppuChip.wReg == 1)
		{
			vm->ppuChip.vReg |= byte;
			vm->ppuChip.wReg++;
		}
		else
		{
			g_logger_warning("PPU Received invalid w register value");
		}
	}
	// PPUDATA VRAM data write
	else if (address == 0x2007)
	{
		uint16 ppuAddr = vm->ppuChip.vReg;
		uint8 data = vm->ppuChip.pins.dataBus;
		if (emu_mmap_inRange(&vm->ppuMmap.backgroundAndSpritePalettes, ppuAddr))
		{
			vm->ppuChip.backgroundSpritePalettes[(ppuAddr - vm->ppuMmap.backgroundAndSpritePalettes.start) % 32] = data;
		}
		else if (emu_mmap_inRange(&vm->ppuMmap.nameAttrTable0, ppuAddr))
		{
			vm->ppuChip.nameAndAttrTable0[(ppuAddr - vm->ppuMmap.nameAttrTable0.start) % 1024] = data;
		}
		else if (emu_mmap_inRange(&vm->ppuMmap.nameAttrTable1, ppuAddr))
		{
			vm->ppuChip.nameAndAttrTable1[(ppuAddr - vm->ppuMmap.nameAttrTable1.start) % 1024] = data;
		}
		else if (emu_mmap_inRange(&vm->ppuMmap.nameAttrTable2, ppuAddr))
		{
			vm->ppuChip.nameAndAttrTable2[(ppuAddr - vm->ppuMmap.nameAttrTable2.start) % 1024] = data;
		}
		else if (emu_mmap_inRange(&vm->ppuMmap.nameAttrTable3, ppuAddr))
		{
			vm->ppuChip.nameAndAttrTable3[(ppuAddr - vm->ppuMmap.nameAttrTable3.start) % 1024] = data;
		}
		else if (emu_mmap_inRange(&vm->ppuMmap.nameAttrTableMirrors, ppuAddr))
		{
			vm->ppuChip.nameAndAttrTableMirror[(ppuAddr - vm->ppuMmap.nameAttrTableMirrors.start) % 4096] = data;
		}
		else if (emu_mmap_inRange(&vm->ppuMmap.patternTable0, ppuAddr))
		{
			vm->ppuChip.patternTable0[(ppuAddr - vm->ppuMmap.patternTable0.start) % 4096] = data;
		}
		else if (emu_mmap_inRange(&vm->ppuMmap.patternTable1, ppuAddr))
		{
			vm->ppuChip.patternTable1[(ppuAddr - vm->ppuMmap.patternTable1.start) % 4096] = data;
		}

		emu_EmulatorDebug_generateDebugTextures(app, app->vm);

		// TODO: This should increment by 1 if bit 2 of ppuctrl reg is + 0 and increment by 32 if it's 1
		vm->ppuChip.vReg += 1;
	}
	// OAMDMA, Sprite DMA write
	else if (address == 0x4014)
	{
		uint8 hiByte = vm->ppuChip.pins.dataBus;
		vm->cpuChip.oamDma = hiByte;

		// Once we trigger an OMA transfer here, we will copy the bytes from
		//   startByte = (hiByte << 16 | loByte << 16) 
		// all the way to
		//   endByte = startByte + 0xff
		// So we copy 256 bytes from the CPU to our PPU
		for (int i = 0; i <= 0xff; i++)
		{
			// First load CPU data to the data bus
			vm->cpuChip.writePins.addressBus = (vm->cpuChip.oamDma << 8) | vm->cpuChip.oamAddr;
			vm->cpuChip.writePins.readWrite = rwToBinary(emu_RW_Read);
			emu_chip_cycle(app, vm);

			// Then trigger an OAM write by writing to the OAM Write address
			vm->cpuChip.writePins.addressBus = 0x2004;
			vm->cpuChip.writePins.readWrite = rwToBinary(emu_RW_Write);
			emu_chip_cycle(app, vm);
		}
	}
}