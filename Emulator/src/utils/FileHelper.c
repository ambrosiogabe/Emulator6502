#include "utils/FileHelper.h"
#include "Emulator/App.h"

#include <stdio.h>
#include <tinyfiledialogs.h>
#include <stb/stb_ds.h>
#include <IconsFontAwesome7.h>
#include <IconsFontAwesome7Brands.h>

#ifdef _WIN32
#include <Windows.h>
#endif 

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

const char* emu_file_openFolderDialog(const char* defaultDir)
{
	emu_app_pauseApp();
	const char* title = "";
	char const* folderName = tinyfd_selectFolderDialog(
		title,
		defaultDir
	);
	emu_app_resumeApp();

	return folderName;
}

const char* emu_file_saveFileDialog(int numFileFilters, const char** fileFilters)
{
	emu_app_pauseApp();
	const char* fileFilterDesc = "";
	const char* title = "";
	const char* defaultDir = "";
	char const* filename = tinyfd_saveFileDialog(
		title,
		defaultDir,
		numFileFilters,
		fileFilters,
		fileFilterDesc);
	emu_app_resumeApp();

	return filename;
}

#ifdef _WIN32
static void searchDirAndAddToChild(const char* dirToSearch, emu_file_data* child)
{
	if (!dirToSearch)
	{
		*child = (emu_file_data)
		{
			.children = NULL,
				.filename = NULL,
				.filenameLength = 0,
				.type = emu_file_type_unknown
		};
		return;
	}

	WIN32_FIND_DATAA findData;
	// Look for everything (*) in the current directory
	size_t directoryLength = strlen(dirToSearch);
	char* directorySearchPath = g_memory_allocate(directoryLength + 3);
	g_memory_copyMem(directorySearchPath, (char*)dirToSearch, directoryLength);
	directorySearchPath[directoryLength] = '\\';
	directorySearchPath[directoryLength + 1] = '*';
	directorySearchPath[directoryLength + 2] = '\0';
	HANDLE hFind = FindFirstFileA(directorySearchPath, &findData);

	char* justTheFolderName = (char*)(dirToSearch + directoryLength);
	for (int i = (int)directoryLength; i >= 0; i--)
	{
		if (dirToSearch[i] == '\\')
		{
			break;
		}

		justTheFolderName = (char*)(dirToSearch + i);
	}

	*child = (emu_file_data){
		.children = NULL,
		.filename = g_strcpy(justTheFolderName),
		.filenameLength = strlen(justTheFolderName),
		.type = emu_file_type_directory
	};

	if (hFind == INVALID_HANDLE_VALUE)
	{
		printf("Could not open the directory.\n");
		return;
	}

	do
	{
		if (strcmp(findData.cFileName, ".") == 0)
		{
			continue;
		}

		if (strcmp(findData.cFileName, "..") == 0)
		{
			continue;
		}

		size_t filenameLength = strlen(findData.cFileName);
		char* ext = (char*)(findData.cFileName + filenameLength);
		for (int i = (int)filenameLength; i >= 0; i--)
		{
			if (findData.cFileName[i] == '.')
			{
				ext = findData.cFileName + i;
				break;
			}
		}


		emu_file_extType extType = emu_file_extType_generic;
		if (strcmp(ext, ".txt") == 0)
		{
			extType = emu_file_extType_txt;
		}
		else if (strcmp(ext, ".s") == 0)
		{
			extType = emu_file_extType_asm;
		}
		else if (strcmp(ext, ".chr") == 0)
		{
			extType = emu_file_extType_chr;
		}
		else if (strcmp(ext, ".bin") == 0)
		{
			extType = emu_file_extType_bin;
		}
		else if (strcmp(ext, "") == 0)
		{
			extType = emu_file_extType_exe;
		}
		else if (strcmp(ext, ".exe") == 0)
		{
			extType = emu_file_extType_exe;
		}
		else if (strcmp(ext, ".yml") == 0)
		{
			extType = emu_file_extType_yml;
		}
		else if (strcmp(ext, ".yaml") == 0)
		{
			extType = emu_file_extType_yml;
		}
		else if (strcmp(ext, ".png") == 0)
		{
			extType = emu_file_extType_png;
		}
		else if (strcmp(ext, ".jpg") == 0)
		{
			extType = emu_file_extType_jpg;
		}
		else if (strcmp(ext, ".dbg") == 0)
		{
			extType = emu_file_extType_dbg;
		}
		else if (strcmp(ext, ".debu") == 0)
		{
			extType = emu_file_extType_dbg;
		}
		else if (strcmp(ext, ".o") == 0)
		{
			extType = emu_file_extType_bin;
		}
		else if (strcmp(ext, ".bat") == 0)
		{
			extType = emu_file_extType_bat;
		}
		else if (strcmp(ext, ".ps1") == 0)
		{
			extType = emu_file_extType_bat;
		}
		else if (strcmp(ext, ".gitignore") == 0)
		{
			extType = emu_file_extType_gitignore;
		}
		else if (strcmp(ext, ".nes") == 0)
		{
			extType = emu_file_extType_nes;
		}

		char* filenameCopy = g_strcpy(findData.cFileName);
		emu_file_data data = { 0 };
		data.filename = filenameCopy;
		data.filenameLength = strlen(filenameCopy);
		data.type = emu_file_type_unknown;
		data.extType = extType;

		if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			data.type |= emu_file_type_directory;
		}

		if ((findData.dwFileAttributes & FILE_ATTRIBUTE_NORMAL) || (findData.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE))
		{
			data.type |= emu_file_type_normal;
		}

		if (findData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)
		{
			data.type |= emu_file_type_hidden;
		}
		stbds_arrpush(child->children, data);
	} while (FindNextFileA(hFind, &findData));

	FindClose(hFind);
	g_memory_free(directorySearchPath);
}

emu_file_data emu_file_getAllFilesInDirectory(const char* directory)
{
	emu_file_data res = { 0 };

	searchDirAndAddToChild(directory, &res);
	for (int i = 0; i < stbds_arrlen(res.children); i++)
	{
		emu_file_data* child = res.children + i;
		if (child->type & emu_file_type_directory)
		{
			size_t directoryLength = strlen(directory);
			size_t recursedFullPathLength = child->filenameLength + directoryLength + 2;
			char* recursedFilepath = g_memory_allocate(recursedFullPathLength);
			g_memory_copyMem(recursedFilepath, (char*)directory, directoryLength);
			recursedFilepath[directoryLength] = '\\';
			g_memory_copyMem(recursedFilepath + directoryLength + 1, child->filename, child->filenameLength);
			recursedFilepath[child->filenameLength + directoryLength + 1] = '\0';

			emu_file_data recursedChild = emu_file_getAllFilesInDirectory(recursedFilepath);
			g_memory_free(child->filename);
			g_memory_free(recursedFilepath);
			*child = recursedChild;
		}
	}

	return res;
}
#endif

const char* emu_file_getExtIcon(emu_file_extType ext)
{
	switch (ext)
	{
		case emu_file_extType_txt:
		case emu_file_extType_bin:
		case emu_file_extType_exe:
		case emu_file_extType_dbg:
			return ICON_FA_ALIGN_LEFT;
		case emu_file_extType_chr:
			return ICON_FA_PICTURE_IN_PICTURE;
		case emu_file_extType_yml:
			return ICON_FA_EXCLAMATION;
		case emu_file_extType_png:
		case emu_file_extType_jpg:
			return ICON_FA_IMAGE;
		case emu_file_extType_asm:
			return ICON_FA_CODE;
		case emu_file_extType_ps1:
		case emu_file_extType_bat:
			return ICON_FA_TERMINAL;
		case emu_file_extType_gitignore:
			return ICON_FA_GIT_ALT;
		case emu_file_extType_nes:
			return ICON_FA_GAMEPAD;
	}

	return ICON_FA_ALIGN_LEFT;
}