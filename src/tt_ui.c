#include "../include/tt_ui.h"
#include "../include/tt_assets.h"
#include "../include/tt_backend.h"

static ttui_config_t ttui_config = {0};

void ttui_set_config(tt_asset_manager_t *assets, ttui_config_t config) {
  ttui_config = config;
}

void ttui_set_config_default(tt_asset_manager_t *assets) {
  static bool default_config_loaded = false;
  static ttui_config_t default_config = {0};

  if (!default_config_loaded) {
    default_config_loaded = true;

    default_config.text_default_color = TT_COLOR_BLACK;
    default_config.text_default_size = 16;

    default_config.textbox_bg_tex =
        tt_asset_bind(assets, "engine/tex/textbox_bg.png");
    default_config.textbox_selected_bg_tex =
        tt_asset_bind(assets, "engine/tex/textbox_selected_bg.png");
    default_config.button_bg_tex =
        tt_asset_bind(assets, "engine/tex/button_bg.png");
    default_config.button_selected_bg_tex =
        tt_asset_bind(assets, "engine/tex/button_selected_bg.png");
    default_config.label_bg_tex =
        tt_asset_bind(assets, "engine/tex/label_bg.png");
  }

  ttui_config = default_config;
}

bool ttui_button(tt_backend_t *backend,
                        ttui_text_t text, i32 x, i32 y, i32 width, i32 height, bool mouse_released) {
  if (text.text_color == 0) {
    text.text_color = ttui_config.text_default_color;
  }
  if (text.text_size == 0) {
    text.text_size = ttui_config.text_default_size;
  }
  
  bool hovered = ttb_mouse_x(backend) > x && ttb_mouse_x(backend) < x + width && ttb_mouse_y(backend) > y && ttb_mouse_y(backend) < y + height;

  ttb_render_cmd_t bg_cmd = {
      .cmd_id = TTB_RENDER_CMD_TEXTURE,
      .cmd_data.cmd_tex = {
          .texture = hovered ? ttui_config.button_selected_bg_tex : ttui_config.button_bg_tex,
          .tint_color = TT_COLOR_WHITE,
          .atlas_src.x = 0,
          .atlas_src.y = 0,
          .atlas_src.width = width,
          .atlas_src.height = height,
          .dest.x = x,
          .dest.y = y,
          .dest.width = width,
          .dest.height = height,
      }};
  ttb_render(backend, bg_cmd);

  u32 text_width = ttb_text_width(backend, text.text, text.text_size);

  ttb_render_cmd_t text_cmd = {.cmd_id = TTB_RENDER_CMD_TEXT,
                               .cmd_data.cmd_text = {
                                   .text = text.text,
                                   .text_color = text.text_color,
                                   .font_size = text.text_size,
                                   .x = x + (width - text_width) / 2,
                                   .y = y + (height - text.text_size) / 2,
                               }};

  ttb_render(backend, text_cmd);

  return false;
}

void ttui_label(tt_backend_t *backend,
                       ttui_text_t text, i32 x, i32 y, i32 width, i32 height) {
  if (text.text_color == 0) {
    text.text_color = ttui_config.text_default_color;
  }
  if (text.text_size == 0) {
    text.text_size = ttui_config.text_default_size;
  }
  ttb_render_cmd_t bg_cmd = {
      .cmd_id = TTB_RENDER_CMD_TEXTURE,
      .cmd_data.cmd_tex = {
          .texture = ttui_config.label_bg_tex,
          .tint_color = TT_COLOR_WHITE,
          .atlas_src.x = 0,
          .atlas_src.y = 0,
          .atlas_src.width = width,
          .atlas_src.height = height,
          .dest.x = x,
          .dest.y = y,
          .dest.width = width,
          .dest.height = height,
      }};
  ttb_render(backend, bg_cmd);

  u32 text_width = ttb_text_width(backend, text.text, text.text_size);

  ttb_render_cmd_t text_cmd = {.cmd_id = TTB_RENDER_CMD_TEXT,
                               .cmd_data.cmd_text = {
                                   .text = text.text,
                                   .text_color = text.text_color,
                                   .font_size = text.text_size,
                                   .x = x + (width - text_width) / 2,
                                   .y = y + (height - text.text_size) / 2,
                               }};

  ttb_render(backend, text_cmd);
}

void ttui_text_box_render(tt_backend_t *backend, ttui_texture_t tex,
                          dyn_string_t *text_buf, i32 x, i32 y, i32 width,
                          i32 height, bool multiline) {}
