#pragma once

#include "tt_shared.h"

#include "lilc/dynstr.h"
#include "lilc/deque.h"

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

typedef enum {
  TTUI_LAYOUT_NONE,
  TTUI_LAYOUT_HORIZONTAL,
  TTUI_LAYOUT_VERTICAL,
} ttui_layout_kind_e;

typedef struct ttui_elem ttui_elem_t;

typedef struct {
  ttui_layout_kind_e kind;
  i32 x;
  i32 y;
  i32 width;
  i32 height;
  i32 padding;
  bool render;
  f32 ui_scale;

  deque_t(ttui_elem_t) ui_elements;
  i32 container_width;
  i32 max_width;
  i32 container_height;
  i32 max_height;
  bool finished;

  bump_t elem_bump;
  allocator_t elem_alloc;
} ttui_layout_t;

void ttui_layout_finish(tt_backend_t *backend, ttui_layout_t *layout);

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

bool ttui_button(tt_backend_t *backend, ttui_layout_t *layout, ttui_text_t text, i32 x, i32 y, i32 width, i32 height, bool mouse_released);

void ttui_label(tt_backend_t *backend, ttui_layout_t *layout, ttui_text_t text, i32 x, i32 y, i32 width, i32 height);

void ttui_text_box(tt_backend_t *backend, ttui_layout_t *layout, dyn_string_t *text_buf, i32 x, i32 y, i32 width, i32 height, i32 max_lines);
