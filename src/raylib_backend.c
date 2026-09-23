#include "../include/tt_backend.h"
#include "../include/tt_engine.h"
#include "lilc/alloc.h"
#include "lilc/array.h"
#include "lilc/deque.h"
#include "lilc/log.h"

#include "raylib.h"

struct tt_backend {
  ttb_render_cmd_t *render_cmds;
  ttb_event_t *event_queue;

  tt_asset_manager_t *assets_manager;

  allocator_t *backend_alloc;

  bump_t render_cmd_bump;
  allocator_t render_cmd_alloc;
  bump_t event_bump;
  allocator_t event_alloc;
};

tt_backend_t *tt_backend_make(allocator_t *alloc, tt_asset_manager_t *assets) {
  tt_backend_t *backend = alloc->alloc(alloc, sizeof(tt_backend_t));

  backend->backend_alloc = alloc;
  backend->assets_manager = assets;

  bump_init(&backend->render_cmd_bump, 128000);
  bump_allocator_init(&backend->render_cmd_alloc, &backend->render_cmd_bump);

  bump_init(&backend->event_bump, 128000);
  bump_allocator_init(&backend->event_alloc, &backend->event_bump);

  backend->render_cmds =
      array_new(ttb_render_cmd_t, &backend->render_cmd_alloc);
  deque_init(backend->event_queue, &backend->event_alloc);

  return backend;
}

void tt_backend_destroy(tt_backend_t *backend) {
  bump_free(&backend->render_cmd_bump);

  allocator_t *alloc = backend->backend_alloc;
  alloc->dealloc(alloc, backend);
}

void ttb_events_poll(tt_backend_t *backend) {
  if (WindowShouldClose()) {
    log_debug("close window event");
    ttb_event_t window_close_event = {
        .event_id = TTB_EVENT_WINDOW_CLOSED,
    };
    deque_push_back(backend->event_queue, window_close_event);
  }
}

bool ttb_event_pop(tt_backend_t *backend, ttb_event_t *event) {
  if (deque_len(backend->event_queue) == 0)
    return false;

  ttb_event_t *evt = deque_pop_front(backend->event_queue);
  if (evt != NULL) {
    *event = *evt;
    return true;
  }
  return false;
}

#define rect_convert(tt_rect)                                                  \
  (Rectangle) {                                                                \
    .x = tt_rect.x, .y = tt_rect.y, .width = tt_rect.width,                    \
    .height = tt_rect.height                                                   \
  }

#define color_convert(color)                                                   \
  (Color) {                                                                    \
    .r = color_red(color), .g = color_green(color), .b = color_blue(color),    \
    .a = color_alpha(color)                                                    \
  }

void ttb_render(tt_backend_t *backend, ttb_render_cmd_t render_cmd) {
  array_add(backend->render_cmds, render_cmd);
}

void *ttb_asset_load(tt_backend_t *backend, allocator_t *asset_alloc,
                     tt_asset_category_t asset_category, const char *path) {
  switch (asset_category) {
  case TT_ASSET_TEXTURE: {
    Texture2D *tex = asset_alloc->alloc(asset_alloc, sizeof(Texture2D));
    Texture2D loaded_tex = LoadTexture(path);
    memcpy(tex, &loaded_tex, sizeof(Texture2D));
    return tex;
  } break;
  case TT_ASSET_SOUND: {

  } break;
  case TT_ASSET_SHADER: {

  } break;
  case TT_ASSET_MUSIC: {

  } break;
  }

  return NULL;
}

void ttb_asset_unload(tt_backend_t *backend, allocator_t *asset_alloc,
                      tt_asset_category_t asset_category, void *asset) {
  switch (asset_category) {
  case TT_ASSET_TEXTURE: {
    UnloadTexture(*(Texture2D *)asset);
    asset_alloc->dealloc(asset_alloc, asset);
  } break;
  case TT_ASSET_SOUND: {

  } break;
  case TT_ASSET_SHADER: {

  } break;
  case TT_ASSET_MUSIC: {

  } break;
  }
}

void _ttb_cmds_render(tt_backend_t *backend) {
  BeginDrawing();

  ttb_render_cmd_t *render_cmd;
  array_foreach(backend->render_cmds, render_cmd) {
    switch (render_cmd->cmd_id) {
    case TTB_RENDER_CMD_TEXTURE: {
      struct ttb_cmd_tex cmd_tex = render_cmd->cmd_data.cmd_tex;

      tt_asset_texture_t *tex =
          tt_asset_by_id(backend->assets_manager, cmd_tex.texture);

      Texture2D *texture = tex->tex_data;

      DrawTexturePro(*texture, rect_convert(cmd_tex.atlas_src),
                     rect_convert(cmd_tex.dest), (Vector2){0},
                     cmd_tex.rotation, color_convert(cmd_tex.tint_color));
    } break;
    case TTB_RENDER_CMD_RECTANGLE: {
    } break;
    case TTB_RENDER_CMD_TEXT: {
    } break;
    case TTB_RENDER_CMD_CLEAR: {
      struct ttb_cmd_clear cmd_clear = render_cmd->cmd_data.cmd_clear;

      ClearBackground(color_convert(cmd_clear.bg_color));
    } break;
    }
  }

  EndDrawing();

  array_clear(backend->render_cmds);
  bump_reset(&backend->render_cmd_bump);
}
