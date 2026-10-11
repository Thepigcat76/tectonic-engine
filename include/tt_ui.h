#pragma once

#include "tt_shared.h"
#include "lilc/dynstr.h"

typedef void (*tt_render_text_func_t)(tt_backend_t *, i32 x, i32 y, const char *text, usz text_len, color_t text_color, usz font_size);

typedef struct {
  tt_asset_id_t button_bg_tex;
  tt_asset_id_t button_selected_bg_tex;
  tt_asset_id_t textbox_bg_tex;
  tt_asset_id_t textbox_selected_bg_tex;
  tt_asset_id_t label_bg_tex;

  color_t text_default_color;
  usz text_default_size;

} ttui_config_t;

typedef struct {
  const char *text;
  color_t text_color;
  usz text_size;
} ttui_text_t;

#define ttui_text(...) (ttui_text_t)__VA_ARGS__

#define ttui_text_default(_text) (ttui_text_t){.text = _text, .text_color = TT_COLOR_WHITE, .text_size = 0}

typedef struct {
  tt_asset_id_t tex;
  color_t tex_tint;
  bool use_default;
} ttui_texture_t;

#define ttui_texture(...) (ttui_texture_t)__VA_ARGS__

#define ttui_texture_default (ttui_texture_t){.use_default = true}

void ttui_set_config(tt_asset_manager_t *assets, ttui_config_t config);

void ttui_set_config_default(tt_asset_manager_t *assets);

bool ttui_button_render(tt_backend_t *backend, ttui_texture_t tex, ttui_text_t text, i32 x, i32 y, i32 width, i32 height, bool mouse_released);

void ttui_label_render(tt_backend_t *backend, ttui_texture_t tex, ttui_text_t text, i32 x, i32 y, i32 width, i32 height);

void ttui_text_box_render(tt_backend_t *backend, ttui_texture_t tex, dyn_string_t *text_buf, i32 x, i32 y, i32 width, i32 height, bool multiline);
