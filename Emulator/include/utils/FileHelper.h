#ifndef EMULATOR_BUS_H
#define EMULATOR_BUS_H
#include "utils/SafeVendor.h"

typedef struct emu_file
{
	char* data;
	size_t data_size;
} emu_file;

typedef enum emu_fileResult
{
	emu_fileResult_Success = 0,
	emu_fileResult_Fail
} emu_fileResult;

typedef enum emu_file_type
{
	emu_file_type_unknown = 0,
	emu_file_type_directory = 1 << 1,
	emu_file_type_normal = 1 << 2,
	emu_file_type_hidden = 1 << 3,
} emu_file_type;

typedef enum emu_file_extType
{
	emu_file_extType_generic = 0,
	emu_file_extType_txt = 0,
	emu_file_extType_chr,
	emu_file_extType_yml,
	emu_file_extType_png,
	emu_file_extType_jpg,
	emu_file_extType_asm,
	emu_file_extType_bin,
	emu_file_extType_exe,
	emu_file_extType_dbg,
	emu_file_extType_ps1,
	emu_file_extType_bat,
	emu_file_extType_gitignore,
	emu_file_extType_nes,
	emu_file_extType_Length,
} emu_file_extType;

typedef struct emu_file_data emu_file_data;

typedef struct emu_file_data
{
	emu_file_extType extType;
	emu_file_type type;
	char* filename;
	size_t filenameLength;
	emu_file_data* children;
} emu_file_data;

emu_fileResult emu_file_read(const char* filename, emu_file* file);
void emu_file_free(emu_file* file);

void emu_file_write(const char* filename, uint8* binaryData, size_t dataSize);

const char* emu_file_openFileDialog(int numFileFilters, const char** fileFilters);
const char* emu_file_openFolderDialog(const char* defaultDir /* Default "" */);
const char* emu_file_saveFileDialog(int numFileFilters, const char** fileFilters);

emu_file_data emu_file_getAllFilesInDirectory(const char* directory);

const char* emu_file_getExtIcon(emu_file_extType ext);

#endif