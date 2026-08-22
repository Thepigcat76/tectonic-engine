#include "../include/tt_engine.h"
#include "lilc/alloc.h"
#include "lilc/array.h"
#include <raylib.h>

void tt_engine_client_init(tt_engine_client_t *engine, tt_engine_init_t init_state) {
  engine->backend = tt_backend_make(&HEAP_ALLOCATOR, &engine->asset_manager);

  bump_init(&engine->event_bump, 16000);
  bump_allocator_init(&engine->event_alloc, &engine->event_bump);

  engine->events = array_new(ttb_event_t, &engine->event_alloc);

  SetTargetFPS(60);

  u32 window_flags = 0;

  const char *window_title = init_state.window_title;
  if (window_title == NULL) {
    window_title = "Tectonic Engine Example";
  }

  i32 window_width = init_state.window_width;
  if (window_width == 0) {
    window_width = 640;
  }

  i32 window_height = init_state.window_height;
  if (window_height == 0) {
    window_height = 480;
  }

  if (init_state.resizeable) {
    window_flags |= FLAG_WINDOW_RESIZABLE;
  }

  if (window_flags != 0) {
    SetConfigFlags(window_flags);
  }
  InitWindow(window_width, window_height, window_title);

  engine->asset_manager.asset_handles = NULL;
  
  engine->asset_manager.backend = engine->backend;

  tt_assets_load(&engine->asset_manager);

}

void tt_engine_client_deinit(tt_engine_client_t *engine) {
  CloseWindow();

  tt_assets_unload(&engine->asset_manager, true);

  engine->asset_manager.asset_handles = NULL;

  bump_free(&engine->event_bump);

  tt_backend_destroy(engine->backend);
}

void tt_engine_client_update(tt_engine_client_t *engine) {
  _ttb_cmds_render(engine->backend);
}
