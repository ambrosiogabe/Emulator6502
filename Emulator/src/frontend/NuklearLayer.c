#include "frontend/NuklearLayer.h"
#include "frontend/SdlWrapper.h"
#include "utils/SafeVendor.h"

#include <SDL3/SDL.h>

#pragma warning(push, 0)
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_INCLUDE_COMMAND_USERDATA
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_STANDARD_IO
#define NK_IMPLEMENTATION
#include "nuklear.h"
#define NK_SDL3_RENDERER_IMPLEMENTATION
#include "demo/sdl3_renderer/nuklear_sdl3_renderer.h"

#define INCLUDE_OVERVIEW
#define INCLUDE_CONFIGURATOR
#define INCLUDE_STYLE

#include "demo/common/style.c"
#include "demo/common/overview.c"
#include "demo/common/style_configurator.c"
#pragma warning(pop)

/* ===============================================================
 *
 *                          DEMO
 *
 * ===============================================================*/

static SDL_AppResult nk_sdl_fail()
{
    SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "Error: %s", SDL_GetError());
    return SDL_APP_FAILURE;
}

emu_nuklear_layer* emu_nuklear_init(emu_sdl_wrapper* sdl)
{
    emu_nuklear_layer* layer = g_memory_allocate(sizeof(emu_nuklear_layer));
    *layer = (emu_nuklear_layer){ 0 };

    layer->bg.r = 0.10f;
    layer->bg.g = 0.18f;
    layer->bg.b = 0.24f;
    layer->bg.a = 1.0f;


    float fontScale = 1.0f;
    {
        /* This scaling logic was kept simple for the demo purpose.
         * On some platforms, this might not be the exact scale
         * that you want to use. For more information, see:
         * https://wiki.libsdl.org/SDL3/README-highdpi */
        const float scale = SDL_GetWindowDisplayScale(sdl->window);
        SDL_SetRenderScale(sdl->renderer, scale, scale);
        fontScale = scale;
    }

    layer->ctx = nk_sdl_init(sdl->window, sdl->renderer, nk_sdl_allocator());
    set_style(layer->ctx, THEME_CATPPUCCIN_MOCHA);

    {
        struct nk_font_atlas* atlas;
        struct nk_font_config config = nk_font_config(0);
        struct nk_font* font;

        /* set up the font atlas and add desired font; note that font sizes are
         * multiplied by font_scale to produce better results at higher DPIs */
        atlas = nk_sdl_font_stash_begin(layer->ctx);
        font = nk_font_atlas_add_default(atlas, 13 * fontScale, &config);
        /*font = nk_font_atlas_add_from_file(atlas, "../../../extra_font/DroidSans.ttf", 14 * font_scale, &config);*/
        /*font = nk_font_atlas_add_from_file(atlas, "../../../extra_font/Roboto-Regular.ttf", 16 * font_scale, &config);*/
        /*font = nk_font_atlas_add_from_file(atlas, "../../../extra_font/kenvector_future_thin.ttf", 13 * font_scale, &config);*/
        /*font = nk_font_atlas_add_from_file(atlas, "../../../extra_font/ProggyClean.ttf", 12 * font_scale, &config);*/
        /*font = nk_font_atlas_add_from_file(atlas, "../../../extra_font/ProggyTiny.ttf", 10 * font_scale, &config);*/
        /*font = nk_font_atlas_add_from_file(atlas, "../../../extra_font/Cousine-Regular.ttf", 13 * font_scale, &config);*/
        nk_sdl_font_stash_end(layer->ctx);

        /* this hack makes the font appear to be scaled down to the desired
         * size and is only necessary when font_scale > 1 */
        font->handle.height /= fontScale;
        /*nk_style_load_all_cursors(ctx, atlas->cursors);*/
        nk_style_set_font(layer->ctx, &font->handle);

        layer->AA = NK_ANTI_ALIASING_ON;
    }

    nk_input_begin(layer->ctx);

    return layer;
}

SDL_AppResult emu_nuklear_handleEvent(emu_nuklear_layer* layer, emu_sdl_wrapper* sdl, SDL_Event* event)
{
    switch (event->type)
    {
    case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;
    case SDL_EVENT_KEY_DOWN:
        if (event->key.key == SDLK_Q && event->key.mod & SDL_KMOD_CTRL)
        {
            return SDL_APP_SUCCESS;
        }
        break;
    case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
        /* You may wish to rescale the renderer and Nuklear during this event.
         * Without this the UI and Font could appear too small or too big.
         * This is not handled by the demo in order to keep it simple,
         * but you may wish to re-bake the Font whenever this happens. */
        SDL_Log("Unhandled scale event! Nuklear may appear blurry");
        return SDL_APP_CONTINUE;
    }

    /* Remember to always rescale the event coordinates,
     * if your renderer uses custom scale. */
    SDL_ConvertEventToRenderCoordinates(sdl->renderer, event);

    nk_sdl_handle_event(layer->ctx, event);

    return SDL_APP_CONTINUE;
}

SDL_AppResult emu_nuklear_tick(emu_nuklear_layer* layer, emu_sdl_wrapper* sdl)
{
    struct nk_context* ctx = layer->ctx;

#ifdef INCLUDE_CONFIGURATOR
    static struct nk_color color_table[NK_COLOR_COUNT];
    NK_MEMCPY(color_table, nk_default_color_style, sizeof(color_table));
#endif

    nk_input_end(ctx);

    /* GUI */
    if (nk_begin(ctx, "Demo", nk_rect(50, 50, 230, 250),
        NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE |
        NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE))
    {
        enum { EASY, HARD };
        static int op = EASY;
        static int property = 20;

        nk_layout_row_static(ctx, 30, 80, 1);
        if (nk_button_label(ctx, "button"))
        {
            SDL_Log("button pressed");
        }
        nk_layout_row_dynamic(ctx, 30, 2);
        if (nk_option_label(ctx, "easy", op == EASY)) op = EASY;
        if (nk_option_label(ctx, "hard", op == HARD)) op = HARD;
        nk_layout_row_dynamic(ctx, 25, 1);
        nk_property_int(ctx, "Compression:", 0, &property, 1000, 1, 1);

        nk_layout_row_dynamic(ctx, 20, 1);
        nk_label(ctx, "background:", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 25, 1);
        if (nk_combo_begin_color(ctx, nk_rgb_cf(layer->bg), nk_vec2(nk_widget_width(ctx), 400)))
        {
            nk_layout_row_dynamic(ctx, 120, 1);
            layer->bg = nk_color_picker(ctx, layer->bg, NK_RGBA);
            nk_layout_row_dynamic(ctx, 25, 1);
            layer->bg.r = nk_propertyf(ctx, "#R:", 0, layer->bg.r, 1.0f, 0.01f, 0.005f);
            layer->bg.g = nk_propertyf(ctx, "#G:", 0, layer->bg.g, 1.0f, 0.01f, 0.005f);
            layer->bg.b = nk_propertyf(ctx, "#B:", 0, layer->bg.b, 1.0f, 0.01f, 0.005f);
            layer->bg.a = nk_propertyf(ctx, "#A:", 0, layer->bg.a, 1.0f, 0.01f, 0.005f);
            nk_combo_end(ctx);
        }
    }
    nk_end(ctx);

    /* -------------- EXAMPLES ---------------- */
#ifdef INCLUDE_OVERVIEW
    overview(ctx);
#endif
#ifdef INCLUDE_CONFIGURATOR
    style_configurator(ctx, color_table);
#endif
    /* ----------------------------------------- */

    SDL_SetRenderDrawColorFloat(sdl->renderer, layer->bg.r, layer->bg.g, layer->bg.b, layer->bg.a);
    SDL_RenderClear(sdl->renderer);

    nk_sdl_render(ctx, layer->AA);
    nk_sdl_update_TextInput(ctx);

    /* show if TextInput is active for debug purpose. Feel free to remove this. */
    SDL_SetRenderDrawColor(sdl->renderer, 0xFF, 0xFF, 0xFF, 0xFF);
    SDL_RenderDebugTextFormat(sdl->renderer, 10, 10, "TextInputActive? %s",
        SDL_TextInputActive(sdl->window) ? "Yes" : "No");

    SDL_RenderPresent(sdl->renderer);

    nk_input_begin(ctx);
    return SDL_APP_CONTINUE;
}

void emu_nuklear_free(emu_nuklear_layer* layer)
{
    if (layer)
    {
        nk_input_end(layer->ctx);
        nk_sdl_shutdown(layer->ctx);
        g_memory_free(layer);
    }
}