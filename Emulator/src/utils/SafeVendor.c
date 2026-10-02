#include "utils/SafeVendor.h"

#include <string.h>

char* g_strcpy(const char* str)
{
	size_t strLength = strlen(str);
	char* copy = g_memory_allocate(strLength + 1);
	g_memory_copyMem(copy, (char*)str, strLength);
	copy[strLength] = '\0';

	return copy;
}