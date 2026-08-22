#include "../include/tt_backend.h"
#include "../include/tt_engine.h"
#include "lilc/alloc.h"
#include "lilc/array.h"
#include "lilc/log.h"
#include "lilc/numbers.h"
#include "lilc/todo.h"
#include <stddef.h>

#define MAX_ENTITIES_AMOUNT 4000

typedef struct {
  i32 x;
  i32 y;

  bool present;
} component_pos_t;

typedef struct {
  tt_asset_id_t texture;

  bool present;
} component_texture_t;

typedef enum {
  ENTITY_KIND_PLAYER,
} entity_kind_t;

typedef struct {
  entity_kind_t entity_kind;
  bool present;
} component_entity_kind_t;

struct tt_entity_manager {
  component_pos_t positions[MAX_ENTITIES_AMOUNT];
  component_texture_t textures[MAX_ENTITIES_AMOUNT];

  tt_entity_t player_handle;

  tt_entity_t cur_entity_id;
  size_t entities_amount;

  tt_engine_client_t engine;
};

tt_entity_t tt_entity_make(tt_entity_manager_t *entities) {
  entities->entities_amount += 1;
  return entities->cur_entity_id++;
}

static void system_render(tt_entity_manager_t *entities) {
  for (tt_entity_t entity = 0; entity < entities->entities_amount; entity++) {
    component_pos_t entity_pos = entities->positions[entity];
    if (!entity_pos.present) {
      continue;
    }

    component_texture_t entity_tex = entities->textures[entity];
    if (!entity_tex.present) {
      continue;
    }

    ttb_render_cmd_t render_cmd = {
        .cmd_id = TTB_RENDER_CMD_TEXTURE,
        .cmd_data.cmd_tex = {.texture = entity_tex.texture,
                             .atlas_src =
                                 {
                                     .width = 16,
                                     .height = 16,
                                 },
                             .dest =
                                 {
                                     .x = entity_pos.x,
                                     .y = entity_pos.y,
                                     .width = 16,
                                     .height = 16,
                                 },.tint_color = color_make(255, 255, 255, 255)},
    };

    ttb_render(entities->engine.backend, render_cmd);
  }
}

#define set_component(entities_ptr, entity, comp, ...)                         \
  do {                                                                         \
    __VA_OPT__((entities_ptr)->comp##s[entity] =                               \
                   (typeof(*((entities_ptr)->comp##s)))__VA_ARGS__;)           \
    (entities_ptr)->comp##s[entity].present = true;                            \
  } while (0)

static tt_asset_id_t player_tex;

i32 main(i32 argc, char **argv) {
  tt_engine_init_t engine_init_state = {
      .window_title = "Engine Example",
      .resizeable = true,
  };

  tt_engine_client_t _engine = {0};
  _engine.asset_manager.assets_path = "assets/";
  tt_engine_client_init(&_engine, engine_init_state);

  tt_entity_manager_t entity_manager = {
      .engine = _engine,
  };

  tt_asset_manager_t *assets = &entity_manager.engine.asset_manager;

  player_tex = tt_asset_bind(assets, "tex/playr.png");

  tt_entity_t player = tt_entity_make(&entity_manager);

  entity_manager.player_handle = player;

  set_component(&entity_manager, player, texture,
                {
                    .texture = player_tex,
                });

  set_component(&entity_manager, player, position,
                {
                    .x = 400,
                    .y = 400,
                });

  bool running = true;

  while (running) {
    ttb_render(entity_manager.engine.backend,
               (ttb_render_cmd_t){.cmd_id = TTB_RENDER_CMD_CLEAR,
                                  .cmd_data.cmd_clear.bg_color =
                                      color_make(255, 255, 255, 255)});

    ttb_events_poll(entity_manager.engine.backend,
                    &entity_manager.engine.events);

    ttb_event_t *event;
    array_foreach(entity_manager.engine.events, event) {
      switch (event->event_id) {
      case TTB_EVENT_WINDOW_CLOSED: {
        running = false;
      } break;
      default: {
      } break;
      }
    }

    system_render(&entity_manager);

    tt_engine_client_update(&entity_manager.engine);
  }

  tt_engine_client_deinit(&entity_manager.engine);
}
