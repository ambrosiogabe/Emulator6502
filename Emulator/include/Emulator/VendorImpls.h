#define GABE_CPP_UTILS_IMPL
#include <cppUtils/cppUtils.h>

// TODO: Fix stupid realloc in my utils library. It's broken.
//#define STBDS_REALLOC(context,ptr,size) g_memory_realloc(ptr, size)
//#define STBDS_FREE(context,ptr)         g_memory_free(ptr)

#define STB_DS_IMPLEMENTATION
#include <stb/stb_ds.h>