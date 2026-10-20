#include "../include/tt_ui.h"
#include "../include/tt_assets.h"
#include "../include/tt_backend.h"
#include "lilc/alloc.h"
#include "lilc/log.h"
#include <raylib.h>

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

typedef struct ttui_elem {
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

  if (layout->ui_scale == 0)
    layout->ui_scale = 1;

  if (layout->elem_bump.first_node == NULL) {
    bump_init(&layout->elem_bump, 16000);
    bump_allocator_init(&layout->elem_alloc, &layout->elem_bump);
    deque_init(layout->ui_elements, &layout->elem_alloc);
  }
}

void ttui_layout_finish(tt_backend_t *backend, ttui_layout_t *layout) {
  deque_t(ttui_elem_t) elems = layout->ui_elements;
  if (elems == NULL)
    return;

  usz elem_count = deque_len(elems);

  if (layout->render) {
    bump_reset(&layout->elem_bump);
    deque_init(layout->ui_elements, &layout->elem_alloc);
    layout->container_width = 0;
    layout->container_height = 0;
    layout->max_width = 0;
    layout->max_height = 0;
    return;
  }

  if (elem_count > 0) {
    i32 gaps = layout->padding * (elem_count - 1);

    i32 content_w, content_h;
    switch (layout->kind) {
    case TTUI_LAYOUT_HORIZONTAL:
      content_w = (layout->container_width + gaps) * layout->ui_scale;
      content_h = layout->max_height * layout->ui_scale;
      break;
    case TTUI_LAYOUT_VERTICAL:
      content_w = layout->max_width * layout->ui_scale;
      content_h = (layout->container_height + gaps) * layout->ui_scale;
      break;
    default:
      content_w = layout->max_width * layout->ui_scale;
      content_h = layout->max_height * layout->ui_scale;
      break;
    }

    i32 start_x = layout->x + (layout->width - content_w) / 2;
    i32 start_y = layout->y + (layout->height - content_h) / 2;

    i32 cur_x = start_x;
    i32 cur_y = start_y;

    ttui_elem_t *elem;
    deque_foreach(elems, elem) {
      switch (layout->kind) {
      case TTUI_LAYOUT_HORIZONTAL: {
        elem->x = cur_x;
        elem->y = start_y + (content_h - elem->height * layout->ui_scale) / 2;
        cur_x += (elem->width + layout->padding) * layout->ui_scale;
        elem->width = elem->width * layout->ui_scale;
        elem->height = elem->height * layout->ui_scale;
      } break;
      case TTUI_LAYOUT_VERTICAL: {
        elem->x = start_x + (content_w - elem->width * layout->ui_scale) / 2;
        elem->y = cur_y;
        cur_y += (elem->height + layout->padding) * layout->ui_scale;
        elem->width = elem->width * layout->ui_scale;
        elem->height = elem->height * layout->ui_scale;
      } break;
      default: {
      } break;
      }
    }

    ttb_render(backend, (ttb_render_cmd_t){.cmd_id = TTB_RENDER_CMD_RECTANGLE,
                                           .cmd_data.cmd_rect = {
                                               .outline_only = true,
                                               .color = TT_COLOR_BLACK,
                                               .rectangle =
                                                   {
                                                       .x = layout->x,
                                                       .y = layout->y,
                                                       .width = layout->width,
                                                       .height = layout->height,
                                                   },
                                           }});
    ttb_render(
        backend,
        (ttb_render_cmd_t){.cmd_id = TTB_RENDER_CMD_RECTANGLE,
                           .cmd_data.cmd_rect = {
                               .outline_only = false,
                               .color = TT_COLOR_BLACK,
                               .rectangle =
                                   {
                                       .x = layout->x + (layout->width - 4) / 2,
                                       .y = layout->y,
                                       .width = 4,
                                       .height = layout->height,
                                   },
                           }});
    ttb_render(backend,
               (ttb_render_cmd_t){
                   .cmd_id = TTB_RENDER_CMD_RECTANGLE,
                   .cmd_data.cmd_rect = {
                       .outline_only = false,
                       .color = TT_COLOR_BLACK,
                       .rectangle =
                           {
                               .x = layout->x,
                               .y = layout->y + (layout->height - 4) / 2,
                               .width = layout->width,
                               .height = 4,
                           },
                   }});

  }
}

// Return true to skip rendering
static bool ttui_layout_push_pop_elem(ttui_layout_t *layout,
                                      ttui_elem_t *elem) {

  if (!layout->render) {
    deque_push_back(layout->ui_elements, *elem);

    layout->container_width += elem->width;
    layout->container_height += elem->height;
    if (layout->max_width < elem->width)
      layout->max_width = elem->width;
    if (layout->max_height < elem->height)
      layout->max_height = elem->height;

    return true;
  } else {
    if (deque_len(layout->ui_elements) <= 0) {
      log_debug("no elems");
      return true;
    }

    ttui_elem_t *_elem = deque_pop_front(layout->ui_elements);
    if (_elem == NULL || _elem->kind != elem->kind) {
      log_debug("elem is null");
      return true;
    }

    *elem = *_elem;
  }

  return false;
}

static bool ttui_button_render(tt_backend_t *backend, ttui_text_t text,
                               ttui_elem_t elem) {
  if (elem.kind != TTUI_BUTTON)
    return false;

  if (text.text_color == 0) {
    text.text_color = ttui_config.text_default_color;
  }
  if (text.text_size == 0) {
    text.text_size = ttui_config.text_default_size;
  }

  bool hovered = ttb_mouse_x(backend) > elem.x &&
                 ttb_mouse_x(backend) < elem.x + elem.width &&
                 ttb_mouse_y(backend) > elem.y &&
                 ttb_mouse_y(backend) < elem.y + elem.height;

  ttb_render_cmd_t bg_cmd = {
      .cmd_id = TTB_RENDER_CMD_TEXTURE,
      .cmd_data.cmd_tex = {
          .texture = hovered ? ttui_config.button_selected_bg_tex
                             : ttui_config.button_bg_tex,
          .tint_color = TT_COLOR_WHITE,
          .atlas_src.x = 0,
          .atlas_src.y = 0,
          .atlas_src.width = elem.width,
          .atlas_src.height = elem.height,
          .dest.x = elem.x,
          .dest.y = elem.y,
          .dest.width = elem.width,
          .dest.height = elem.height,
      }};
  ttb_render(backend, bg_cmd);

  u32 text_width = ttb_text_width(backend, text.text, text.text_size);

  ttb_render_cmd_t text_cmd = {
      .cmd_id = TTB_RENDER_CMD_TEXT,
      .cmd_data.cmd_text = {
          .text = text.text,
          .text_color = text.text_color,
          .font_size = text.text_size,
          .x = elem.x + (elem.width - text_width) / 2,
          .y = elem.y + (elem.height - text.text_size) / 2,
      }};

  ttb_render(backend, text_cmd);

  return hovered && elem.u.button.mouse_released;
}

bool ttui_button(tt_backend_t *backend, ttui_layout_t *layout, ttui_text_t text,
                 i32 x, i32 y, i32 width, i32 height, bool mouse_released) {
  ttui_elem_t elem = {.kind = TTUI_BUTTON,
                      .x = 0,
                      .y = 0,
                      .width = width,
                      .height = height,
                      .u.button = {
                          .mouse_released = mouse_released,
                      }};
  if (layout != NULL && layout->kind != TTUI_LAYOUT_NONE) {
    ttui_layout_init(layout);

    if (ttui_layout_push_pop_elem(layout, &elem)) {
      return false;
    }
  } else {
    elem.x = x;
    elem.y = y;
  }

  return ttui_button_render(backend, text, elem);
}

static void ttui_label_render(tt_backend_t *backend, ttui_text_t text,
                              ttui_elem_t elem) {
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
                                 .atlas_src.width = elem.width,
                                 .atlas_src.height = elem.height,
                                 .dest.x = elem.x,
                                 .dest.y = elem.y,
                                 .dest.width = elem.width,
                                 .dest.height = elem.height,
                             }};
  ttb_render(backend, bg_cmd);

  u32 text_width = ttb_text_width(backend, text.text, text.text_size);

  ttb_render_cmd_t text_cmd = {
      .cmd_id = TTB_RENDER_CMD_TEXT,
      .cmd_data.cmd_text = {
          .text = text.text,
          .text_color = text.text_color,
          .font_size = text.text_size,
          .x = elem.x + (elem.width - text_width) / 2,
          .y = elem.y + (elem.height - text.text_size) / 2,
      }};

  ttb_render(backend, text_cmd);
}

void ttui_label(tt_backend_t *backend, ttui_layout_t *layout, ttui_text_t text,
                i32 x, i32 y, i32 width, i32 height) {
  ttui_elem_t elem = {
      .kind = TTUI_LABEL,
      .x = 0,
      .y = 0,
      .width = width,
      .height = height,
  };

  if (layout != NULL && layout->kind != TTUI_LAYOUT_NONE) {
    ttui_layout_init(layout);

    if (ttui_layout_push_pop_elem(layout, &elem)) {
      return;
    }
  } else {
    elem.x = x;
    elem.y = y;
  }

  ttui_label_render(backend, text, elem);
}

void ttui_text_box(tt_backend_t *backend, ttui_layout_t *layout,
                   dyn_string_t *text_buf, i32 x, i32 y, i32 width, i32 height,
                   i32 max_lines) {}
