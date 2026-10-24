#ifndef EMU_CHIPS_H
#define EMU_CHIPS_H
#include "utils/SafeVendor.h"
#include "Emulator/Types.h"

typedef struct emu_virtualMachine emu_virtualMachine;

typedef enum emu_Chips
{
	emu_Chips_Null = 0,
	emu_Chips_Cpu,
	emu_Chips_Ppu,
	emu_Chips_Sram,
	emu_Chips_CartridgeSram,
	emu_Chips_CartridgeExpansion,
	emu_Chips_PrgRom,
	emu_Chips_ChrRom,
	emu_Chips_Apu,
	emu_Chips_Length,
} emu_Chips;

typedef enum emu_RW
{
	emu_RW_Read,
	emu_RW_Write
} emu_RW;

typedef struct emu_Cpu
{
	struct
	{
		//uint8 rout : 1;
		//uint8 cout : 1;
		uint8 reset : 1;
		uint8 clock : 1;
		//uint8 ground : 1;
		//uint8 null : 1;
		//uint8 phi2 : 1;
		uint8 irq : 1;
		uint8 nmi : 1;
		//uint8 joy2 : 1; // Low if A0-A15=4017h, R/W=0, PHI2=1
		//uint8 joy1 : 1; // Low if A0-A15=4016h, R/W=0, PHI2=1
		//uint8 j20 : 3;
		//uint8 vcc : 1;
	} readPins;

	struct
	{
		uint16 addressBus : 16; // A0-A15
		uint8 readWrite : 1;
	} writePins;

	struct
	{
		uint8 dataBus : 8; // D0-D7
	} readWritePins;

	// Registers
	uint8 oamDma;
	uint8 oamAddr;
} emu_Cpu;

typedef struct emu_Cartridge
{
	struct
	{
		uint16 cpuAddressBus : 16; // CPU A0-A15
		uint16 ppuAddressBus : 14; // PPU A0-A13
		uint8 cpuDataBus : 8; // CPU D0-D7
		uint8 cpuReadWrite : 1;
	} pins;

	uint8* prgRom;
	uint8* chrRom;
	uint16 numPrgRomPages;
	uint16 numChrRomPages;
} emu_Cartridge;

typedef struct emu_SRam
{
	struct
	{
		uint8 readWrite : 1;
		uint8 dataBus : 8; // CPU D0-D7
		uint16 addressBus : 11; // CPU A0-A10
	} pins;
	//uint32 pins;
	uint8 ram[KB(2)];
} emu_SRam;

typedef struct emu_Ppu
{
	struct
	{
		uint16 addressBus : 14; // PPU A0-A13
		uint8 dataBus : 8; // CPU D0-D7
		uint8 readWrite : 1;
	} pins;

	uint8 status;

	// Internal Registers

	// Write latch
	uint8 wReg;
	// Current VRAM address outside of rendering, during rendering scroll position
	uint16 vReg;
	// During rendering coarse-x scroll and starting y, outside of rendering, scroll or VRAM address before transfer to vReg
	uint8 tReg;
	// fine-x position fo current scroll
	uint8 xReg;

	uint8 patternTable0[KB(4)];
	uint8 patternTable1[KB(4)];
	uint8 nameAndAttrTable0[KB(1)];
	uint8 nameAndAttrTable1[KB(1)];
	uint8 nameAndAttrTable2[KB(1)];
	uint8 nameAndAttrTable3[KB(1)];
	uint8 nameAndAttrTableMirror[KB(4)];
	uint8 backgroundSpritePalettes[32];

	// This is technically a separate chip that lives inside the PPU
	uint8 oamRam[256];
} emu_Ppu;

void emu_chip_setReadWrite(emu_virtualMachine* vm, emu_RW readWrite);
void emu_chip_cycle(emu_virtualMachine* vm);

void emu_chip_cpuWriteAddress(emu_virtualMachine* vm, uint16 address);
void emu_chip_cpuWriteToDataBus(emu_virtualMachine* vm, uint8 data);
uint8 emu_chip_cpuReadDataBus(emu_virtualMachine* vm);

#endif