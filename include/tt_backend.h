#pragma once

#include "lilc/alloc.h"
#include "lilc/color.h"
#include "tt_shared.h"
#include <stdbool.h>

typedef struct tt_backend tt_backend_t;

typedef enum ttb_event_id {
  TTB_EVENT_WINDOW_CLOSED,
  TTB_EVENT_WINDOW_RESIZED,

  _amount_ttb_event_ids,
} ttb_event_id_e;

typedef struct ttb_event {
  ttb_event_id_e event_id;
  void *event_data;
} ttb_event_t;

typedef ttb_event_t *ttb_event_array_t;

tt_backend_t *tt_backend_make(Allocator *alloc, tt_asset_manager_t *assets);

void tt_backend_destroy(tt_backend_t *backend);

typedef struct ttb_render_cmd {
  enum ttb_render_cmd_id {
    TTB_RENDER_CMD_TEXTURE,
    TTB_RENDER_CMD_RECTANGLE,
    TTB_RENDER_CMD_TEXT,
    TTB_RENDER_CMD_CLEAR,
  } cmd_id;
  struct ttb_render_cmd_data {
    struct ttb_cmd_tex {
      tt_asset_id_t texture;
      f32 rotation;
      tt_rect_t atlas_src;
      tt_rect_t dest;
      color_t tint_color;
    } cmd_tex;
    struct ttb_cmd_rect {
      tt_rect_t rectangle;
      color_t color;
      bool outline_only;
    } cmd_rect;
    struct ttb_cmd_text {
      // TODO: Custom fonts
      i32 font_size;
      const char *text;
      color_t text_color;
    } cmd_text;
    struct ttb_cmd_clear {
      color_t bg_color;
    } cmd_clear;
  } cmd_data;
} ttb_render_cmd_t;

typedef enum ttb_render_cmd_id ttb_render_cmd_id_t;

typedef struct ttb_render_cmd_data ttb_render_cmd_data_t;

void ttb_render(tt_backend_t *backend, ttb_render_cmd_t render_cmd);

void _ttb_cmds_render(tt_backend_t *backend);

void *ttb_asset_load(tt_backend_t *backend, Allocator *asset_alloc, tt_asset_category_t category, const char *path);

void ttb_asset_unload(tt_backend_t *backend, Allocator *asset_alloc, tt_asset_category_t category, void *asset);

bool ttb_window_should_close(tt_backend_t *backend);

void ttb_events_poll(tt_backend_t *backend);

bool ttb_event_pop(tt_backend_t *backend, ttb_event_t *event);
