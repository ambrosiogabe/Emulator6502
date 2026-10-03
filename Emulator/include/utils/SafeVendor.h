#ifndef SAFE_CPP_UTILS_H
#define SAFE_CPP_UTILS_H

#pragma warning(push, 0)
#include <cppUtils/cppUtils.h>
#pragma warning(pop)

char* g_strcpy(const char* str);
char* g_strcpyEx(const char* str, const char* end);
char* g_strcpy_sized(const char* str, size_t length);

#endif