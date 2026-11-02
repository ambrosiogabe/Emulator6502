#include "renderer/TextureUtils.h"
#include "Emulator/App.h"
#include "frontend/SdlWrapper.h"

emu_texutreUtils_Texture emu_textureUtils_createTexture(emu_app* app, emu_textureUtils_opts options)
{
	// 1. Create GPU texture
	SDL_GPUTextureCreateInfo texture_info = { 0 };
	texture_info.type = SDL_GPU_TEXTURETYPE_2D;
	texture_info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
	texture_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
	texture_info.width = options.width;
	texture_info.height = options.height;
	texture_info.layer_count_or_depth = 1;
	texture_info.num_levels = 1;
	texture_info.sample_count = SDL_GPU_SAMPLECOUNT_1;

	SDL_GPUTexture* texture = SDL_CreateGPUTexture(app->sdl->gpuDevice, &texture_info);

	// Create transfer buffer
	// FIXME: A real engine would likely keep one around, see what the SDL_GPU backend is doing.
	SDL_GPUTransferBufferCreateInfo transferbuffer_info = { 0 };
	transferbuffer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
	transferbuffer_info.size = options.width * options.height * 4;
	SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(app->sdl->gpuDevice, &transferbuffer_info);
	g_logger_assert(transferBuffer != NULL, "transfer buffer is null");

	// Copy to transfer buffer
	{
		uint32_t uploadPitch = options.width * sizeof(uint32);
		void* texturePtr = SDL_MapGPUTransferBuffer(app->sdl->gpuDevice, transferBuffer, true);
		for (uint32 y = 0; y < options.height; y++)
			memcpy((void*)((uintptr_t)texturePtr + y * uploadPitch), options.pixels + y * uploadPitch, uploadPitch);
		SDL_UnmapGPUTransferBuffer(app->sdl->gpuDevice, transferBuffer);
	}

	// 4. Record and Submit the GPU Copy Command
	SDL_GPUTextureTransferInfo transferInfo = { 0 };
	transferInfo.offset = 0;
	transferInfo.transfer_buffer = transferBuffer;

	SDL_GPUTextureRegion texture_region = { 0 };
	texture_region.texture = texture;
	texture_region.x = 0;
	texture_region.y = 0;
	texture_region.w = options.width;
	texture_region.h = options.height;
	texture_region.d = 1;

	// Upload
	SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(app->sdl->gpuDevice);
	SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(cmd);
	SDL_UploadToGPUTexture(copy_pass, &transferInfo, &texture_region, false);
	SDL_EndGPUCopyPass(copy_pass);
	SDL_SubmitGPUCommandBuffer(cmd);

	// 5. Clean up temporary host/staging assets
	// Release the staging buffer (safe to call immediately; execution happens safely via internal lifecycle reference counting)
	SDL_ReleaseGPUTransferBuffer(app->sdl->gpuDevice, transferBuffer);

	return (emu_texutreUtils_Texture)
	{
		.width = options.width,
			.height = options.height,
			.texture = texture
	};
}