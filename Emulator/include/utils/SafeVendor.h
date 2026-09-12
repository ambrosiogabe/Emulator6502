#ifndef SAFE_CPP_UTILS_H
#define SAFE_CPP_UTILS_H

#pragma warning(push, 0)
#include <cppUtils/cppUtils.h>
#pragma warning(pop)

#pragma warning(push, 0)
#pragma warning(disable: 4701)
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_INCLUDE_COMMAND_USERDATA
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_STANDARD_IO
#include <nuklear.h>
#pragma warning(pop)

#endif