#include "../include/tt_backend.h"
#include "../include/tt_engine.h"
#include "lilc/alloc.h"
#include "lilc/array.h"
#include "lilc/log.h"
#include "lilc/todo.h"

#include "raylib.h"

struct tt_backend {
  ttb_render_cmd_t *render_cmds;

  tt_asset_manager_t *assets_manager;

  Allocator *backend_alloc;

  Bump render_cmd_bump;
  Allocator render_cmd_alloc;
};

tt_backend_t *tt_backend_make(Allocator *alloc, tt_asset_manager_t *assets) {
  tt_backend_t *backend = alloc->alloc(alloc, sizeof(tt_backend_t));

  backend->backend_alloc = alloc;
  backend->assets_manager = assets;

  bump_init(&backend->render_cmd_bump, 128000);
  bump_allocator_init(&backend->render_cmd_alloc, &backend->render_cmd_bump);

  backend->render_cmds =
      array_new(ttb_render_cmd_t, &backend->render_cmd_alloc);

  return backend;
}

void tt_backend_destroy(tt_backend_t *backend) {
  bump_free(&backend->render_cmd_bump);

  Allocator *alloc = backend->backend_alloc;
  alloc->dealloc(alloc, backend);
}

void ttb_events_poll(tt_backend_t *backend, ttb_event_array_t *events) {
  array_clear(*events);

  if (WindowShouldClose()) {
    ttb_event_t window_close_event = {
        .event_id = TTB_EVENT_WINDOW_CLOSED,
    };
    array_add(*events, window_close_event);
  }
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

void *ttb_asset_load(tt_backend_t *backend, Allocator *asset_alloc,
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

void ttb_asset_unload(tt_backend_t *backend, Allocator *asset_alloc,
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

      log_debug("Rendering: %s", tex->path);

      Texture2D *texture = tex->tex_data;

      DrawTexturePro(*texture, rect_convert(cmd_tex.dest),
                     rect_convert(cmd_tex.atlas_src), (Vector2){0},
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
