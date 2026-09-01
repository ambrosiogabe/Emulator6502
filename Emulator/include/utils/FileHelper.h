#ifndef EMULATOR_BUS_H
#define EMULATOR_BUS_H
#include <cppUtils/cppUtils.h>

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

emu_fileResult emu_file_read(const char* filename, emu_file* file);
void emu_file_free(emu_file* file);

#endif