#include "utils/SafeVendor.h"

#include <string.h>

char* g_strcpy(const char* str)
{
	size_t strLength = strlen(str);
	return g_strcpy_sized(str, strLength);
}

char* g_strcpyEx(const char* start, const char* end)
{
	return g_strcpy_sized(start, end - start);
}

char* g_strcpy_sized(const char* str, size_t strLength)
{
	char* copy = g_memory_allocate(strLength + 1);
	g_memory_copyMem(copy, (char*)str, strLength);
	copy[strLength] = '\0';

	return copy;
}