#include "Emulator/Chips.h"
#include "Emulator/VirtualMachine.h"
#include "frontend/ConsoleOutput.h"

// -------------------- Internal Functions --------------------
static void handlePpuRead(emu_virtualMachine* vm);
static void handlePpuWrite(emu_virtualMachine* vm);

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

void emu_chip_cycle(emu_virtualMachine* vm)
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
			handlePpuWrite(vm);
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

static void handlePpuWrite(emu_virtualMachine* vm)
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
			vm->ppuChip.backgroundSpritePalettes[ppuAddr - vm->ppuMmap.backgroundAndSpritePalettes.start] = data;
		}
		else if (emu_mmap_inRange(&vm->ppuMmap.nameAttrTable0, ppuAddr))
		{
			vm->ppuChip.nameAndAttrTable0[ppuAddr - vm->ppuMmap.nameAttrTable0.start] = data;
		}
		else if (emu_mmap_inRange(&vm->ppuMmap.nameAttrTable1, ppuAddr))
		{
			vm->ppuChip.nameAndAttrTable1[ppuAddr - vm->ppuMmap.nameAttrTable1.start] = data;
		}
		else if (emu_mmap_inRange(&vm->ppuMmap.nameAttrTable2, ppuAddr))
		{
			vm->ppuChip.nameAndAttrTable2[ppuAddr - vm->ppuMmap.nameAttrTable2.start] = data;
		}
		else if (emu_mmap_inRange(&vm->ppuMmap.nameAttrTable3, ppuAddr))
		{
			vm->ppuChip.nameAndAttrTable3[ppuAddr - vm->ppuMmap.nameAttrTable3.start] = data;
		}
		else if (emu_mmap_inRange(&vm->ppuMmap.nameAttrTableMirrors, ppuAddr))
		{
			vm->ppuChip.nameAndAttrTableMirror[ppuAddr - vm->ppuMmap.nameAttrTableMirrors.start] = data;
		}
		else if (emu_mmap_inRange(&vm->ppuMmap.patternTable0, ppuAddr))
		{
			vm->ppuChip.patternTable0[ppuAddr - vm->ppuMmap.patternTable0.start] = data;
		}
		else if (emu_mmap_inRange(&vm->ppuMmap.patternTable1, ppuAddr))
		{
			vm->ppuChip.patternTable1[ppuAddr - vm->ppuMmap.patternTable1.start] = data;
		}

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
			emu_chip_cycle(vm);

			// Then trigger an OAM write by writing to the OAM Write address
			vm->cpuChip.writePins.addressBus = 0x2004;
			vm->cpuChip.writePins.readWrite = rwToBinary(emu_RW_Write);
			emu_chip_cycle(vm);
		}
	}
}