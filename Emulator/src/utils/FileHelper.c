#include "utils/FileHelper.h"
#include "Emulator/App.h"

#include <stdio.h>
#include <tinyfiledialogs.h>

emu_fileResult emu_file_read(const char* filename, emu_file* result)
{
	g_logger_assert(result != NULL, "Cannot read file. File is null.");
	g_memory_zeroMem(result, sizeof(emu_file));

	FILE* file = fopen(filename, "rb");
	if (file == NULL)
	{
		g_logger_error("Could not open file '%s'.", filename);
		return emu_fileResult_Fail;
	}

	fseek(file, 0, SEEK_END);
	size_t fileSize = ftell(file);

	result->data = g_memory_allocate(fileSize + 1);
	result->data_size = fileSize;

	rewind(file);

	fread(result->data, result->data_size, 1, file);
	result->data[fileSize] = '\0';

	fclose(file);

	return emu_fileResult_Success;
}

void emu_file_free(emu_file* file)
{
	g_logger_assert(file != NULL, "Cannot free null file.");
	if (!file->data) return;

	g_memory_free(file->data);
	g_memory_zeroMem(file, sizeof(emu_file));
}

void emu_file_write(const char* filename, uint8* binaryData, size_t dataSize)
{
	// Open file in "wb" (write binary) mode
	FILE* file = fopen(filename, "wb");
	if (file == NULL)
	{
		g_logger_error("Could not open file '%s'.", filename);
		return;
	}

	// Write the entire array to the file
	fwrite(binaryData, dataSize, 1, file);
	fclose(file);
}

const char* emu_file_openFileDialog(int numFileFilters, const char** fileFilters)
{
	emu_app_pauseApp();
	const int multipleFilesAllowed = 0;
	const char* fileFilterDesc = "";
	const char* title = "";
	const char* defaultDir = "";
	char const* filename = tinyfd_openFileDialog(
		title,
		defaultDir,
		numFileFilters,
		fileFilters,
		fileFilterDesc,
		multipleFilesAllowed);
	emu_app_resumeApp();

	return filename;
}