#ifndef EMU_TEXTURE_UTILS_H
#define EMU_TEXTURE_UTILS_H
#include "utils/SafeVendor.h"

#include <SDL3/SDL.h>

typedef struct emu_app emu_app;

typedef struct emu_textureUtils_opts
{
	uint32 width;
	uint32 height;
	// Should be in the format "AGBR", "AGBR", "AGBR"
	// where each component is one byte
	uint8* pixels;
} emu_textureUtils_opts;

typedef struct emu_texutreUtils_Texture
{
	SDL_GPUTexture* texture;
	uint32 width;
	uint32 height;
} emu_texutreUtils_Texture;

emu_texutreUtils_Texture emu_textureUtils_createTexture(emu_app* app, emu_textureUtils_opts options);

#endif