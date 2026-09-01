#include "utils/FileHelper.h"

#include <stdio.h>

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

	// Remove carriage returns
	{
		size_t write_index = 0;

		for (size_t read_index = 0; read_index < fileSize; read_index++)
		{
			if (result->data[read_index] != '\r')
			{
				result->data[write_index] = result->data[read_index];
				write_index++;
			}
		}

		// Update the buffer size to reflect the omitted characters
		result->data = g_memory_realloc(result->data, write_index);
		result->data_size = write_index;
	}

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