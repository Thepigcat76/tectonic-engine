#include "../include/tt_ui.h"
#include "../include/tt_assets.h"
#include "../include/tt_backend.h"
#include <lilc/alloc.h>

static ttui_config_t ttui_config = {0};

void ttui_set_config(tt_asset_manager_t *assets, ttui_config_t config) {
  (void)assets;
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

typedef struct {
  enum {
    TTUI_BUTTON,
    TTUI_LABEL,
    TTUI_TEXTBOX,
  } kind;
  union {
    struct ttui_elem_button {
      bool mouse_released;
    } button;
    struct ttui_elem_text_box {
      i32 max_lines;
    } text_box;
  } u;
  i32 x;
  i32 y;
  i32 width;
  i32 height;
} ttui_elem_t;

static void ttui_layout_init(ttui_layout_t *layout) {
  if (layout == NULL)
    return;

  if (layout->elem_bump.first_node == NULL) {
    bump_init(&layout->elem_bump, 16000);
    bump_allocator_init(&layout->elem_alloc, &layout->elem_bump);
  }

  deque_init(layout->ui_elements, &layout->elem_alloc);
}

void ttui_layout_finish(tt_backend_t *backend, ttui_layout_t *layout) {
  deque_t(ttui_elem_t) elems = layout->ui_elements;
  if (!layout->render) {
    i32 cur_x = layout->x + layout->padding;
    i32 cur_y = layout->y + layout->padding;

    ttui_elem_t *elem;
    deque_foreach(elems, elem) {
      switch (layout->kind) {
      case TTUI_LAYOUT_HORIZONTAL: {
        
      } break;
      case TTUI_LAYOUT_VERTICAL: {

      } break;
      default: {
      } break;
      }
    }
  } else {
    bump_reset(&layout->elem_bump);
  }
}

// Return true to skip rendering
static bool ttui_layout_push_pop_elem(ttui_layout_t *layout,
                                      ttui_elem_t *elem) {
  ttui_elem_t **elems = (ttui_elem_t **)&layout->ui_elements;

  if (!layout->render) {
    deque_push_back(*elems, *elem);

    return true;
  } else {
    if (deque_len(*elems) <= 0) {
      return true;
    }

    ttui_elem_t *_elem = deque_pop_front(*elems);
    if (_elem == NULL || _elem->kind != elem->kind)
      return true;

    *elem = *_elem;
  }

  return false;
}

static bool ttui_button_render(tt_backend_t *backend, ttui_text_t text,
                               struct ttui_elem_button button) {
  if (text.text_color == 0) {
    text.text_color = ttui_config.text_default_color;
  }
  if (text.text_size == 0) {
    text.text_size = ttui_config.text_default_size;
  }

  bool hovered = ttb_mouse_x(backend) > button.x &&
                 ttb_mouse_x(backend) < button.x + button.width &&
                 ttb_mouse_y(backend) > button.y &&
                 ttb_mouse_y(backend) < button.y + button.height;

  ttb_render_cmd_t bg_cmd = {
      .cmd_id = TTB_RENDER_CMD_TEXTURE,
      .cmd_data.cmd_tex = {
          .texture = hovered ? ttui_config.button_selected_bg_tex
                             : ttui_config.button_bg_tex,
          .tint_color = TT_COLOR_WHITE,
          .atlas_src.x = 0,
          .atlas_src.y = 0,
          .atlas_src.width = button.width,
          .atlas_src.height = button.height,
          .dest.x = button.x,
          .dest.y = button.y,
          .dest.width = button.width,
          .dest.height = button.height,
      }};
  ttb_render(backend, bg_cmd);

  u32 text_width = ttb_text_width(backend, text.text, text.text_size);

  ttb_render_cmd_t text_cmd = {
      .cmd_id = TTB_RENDER_CMD_TEXT,
      .cmd_data.cmd_text = {
          .text = text.text,
          .text_color = text.text_color,
          .font_size = text.text_size,
          .x = button.x + (button.width - text_width) / 2,
          .y = button.y + (button.height - text.text_size) / 2,
      }};

  ttb_render(backend, text_cmd);

  return hovered && button.mouse_released;
}

bool ttui_button(tt_backend_t *backend, ttui_layout_t *layout, ttui_text_t text,
                 i32 x, i32 y, i32 width, i32 height, bool mouse_released) {
  ttui_elem_t elem = {.kind = TTUI_BUTTON,
                      .u.button = {
                          .x = 0,
                          .y = 0,
                          .width = width,
                          .height = height,
                          .mouse_released = mouse_released,
                      }};

  if (layout != NULL && layout->kind != TTUI_LAYOUT_NONE) {
    if (ttui_layout_push_pop_elem(layout, &elem)) {
      return false;
    }
  } else {
    elem.u.button.x = x;
    elem.u.button.y = y;
  }

  return ttui_button_render(backend, text, elem.u.button);
}

static void ttui_label_render(tt_backend_t *backend, ttui_text_t text,
                              struct ttui_elem_label label) {
  if (text.text_color == 0) {
    text.text_color = ttui_config.text_default_color;
  }
  if (text.text_size == 0) {
    text.text_size = ttui_config.text_default_size;
  }
  ttb_render_cmd_t bg_cmd = {.cmd_id = TTB_RENDER_CMD_TEXTURE,
                             .cmd_data.cmd_tex = {
                                 .texture = ttui_config.label_bg_tex,
                                 .tint_color = TT_COLOR_WHITE,
                                 .atlas_src.x = 0,
                                 .atlas_src.y = 0,
                                 .atlas_src.width = label.width,
                                 .atlas_src.height = label.height,
                                 .dest.x = label.x,
                                 .dest.y = label.y,
                                 .dest.width = label.width,
                                 .dest.height = label.height,
                             }};
  ttb_render(backend, bg_cmd);

  u32 text_width = ttb_text_width(backend, text.text, text.text_size);

  ttb_render_cmd_t text_cmd = {
      .cmd_id = TTB_RENDER_CMD_TEXT,
      .cmd_data.cmd_text = {
          .text = text.text,
          .text_color = text.text_color,
          .font_size = text.text_size,
          .x = label.x + (label.width - text_width) / 2,
          .y = label.y + (label.height - text.text_size) / 2,
      }};

  ttb_render(backend, text_cmd);
}

void ttui_label(tt_backend_t *backend, ttui_layout_t *layout, ttui_text_t text,
                i32 x, i32 y, i32 width, i32 height) {
  ttui_elem_t elem = {.kind = TTUI_LABEL,
                      .u.label = {
                          .x = 0,
                          .y = 0,
                          .width = width,
                          .height = height,
                      }};

  if (layout != NULL && layout->kind != TTUI_LAYOUT_NONE) {
    ttui_layout_init(layout);

    if (ttui_layout_push_pop_elem(layout, &elem)) {
      return;
    }
  } else {
    elem.u.label.x = x;
    elem.u.label.y = y;
  }

  ttui_label_render(backend, text, elem.u.label);
}

void ttui_text_box(tt_backend_t *backend, ttui_layout_t *layout,
                   dyn_string_t *text_buf, i32 x, i32 y, i32 width, i32 height,
                   i32 max_lines) {}
