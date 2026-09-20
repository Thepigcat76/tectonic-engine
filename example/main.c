#define TTN_PACKETS_ITER

#include "../include/tt_backend.h"
#include "../include/tt_engine.h"
#include "lilc/args.h"
#include "lilc/array.h"
#include "lilc/log.h"
#include "lilc/numbers.h"
#include <stddef.h>

#include "components.h"

#define MAX_ENTITIES_AMOUNT 4000

struct tt_entity_manager {
  component_pos_t positions[MAX_ENTITIES_AMOUNT];
  component_texture_t textures[MAX_ENTITIES_AMOUNT];

  tt_entity_t player_handle;

  tt_entity_t cur_entity_id;
  size_t entities_amount;

  tt_engine_client_t cengine;
  tt_engine_server_t sengine;
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
                                 },
                             .tint_color = color_make(255, 255, 255, 255)},
    };

    ttb_render(entities->cengine.backend, render_cmd);
  }
}

#define _F(...) {__VA_ARGS__}

#define set_component(entities_ptr, entity, comp, ...)                         \
  do {                                                                         \
    __VA_OPT__((entities_ptr)->comp##s[entity] =                               \
                   (typeof(*((entities_ptr)->comp##s)))_F __VA_ARGS__;)        \
    (entities_ptr)->comp##s[entity].present = true;                            \
  } while (0)

static tt_asset_id_t player_tex;

static i32 server_run(void);

static i32 client_run(void);

i32 main(i32 argc, char **argv) {
  if (args_contains(argc, argv, "--server", NULL)) {
    return server_run();
  } else {
    return client_run();
  }
}

static i32 server_run(void) {
  tt_engine_server_t _engine = {0};
  ttes_init(&_engine, false);

  if (!ttes_subproc_start(&_engine, TTES_SUBPROC_PACKET_RECV)) {
    log_error("Failed to start packet receiver on the server");
    return 1;
  }

  if (!ttes_subproc_start(&_engine, TTES_SUBPROC_CLIENT_ACCEPT)) {
    log_error("Failed to start client acceptor on the server");
    return 1;
  }

  tt_entity_manager_t entity_manager = {.sengine = _engine};

  ttes_server_host(&entity_manager.sengine, "127.0.0.1", 12345);

  bool running = true;

  while (running) {
    if (running) {
      running = ttes_update(&_engine);
    } else {
      ttes_update(&_engine);
    }
  }

  ttes_server_stop(&entity_manager.sengine);

  ttes_deinit(&_engine);

  return 0;
}

static i32 client_run(void) {
  tt_engine_init_t engine_init_state = {
      .window_title = "Engine Example",
      .resizeable = true,
  };

  tt_engine_client_t _engine = {0};
  _engine.asset_manager.assets_path = "assets/";
  ttec_init(&_engine, engine_init_state);

  ttec_load_assets(&_engine);

  if (!ttec_subproc_start(&_engine, TTEC_SUBPROC_PACKET_RECV)) {
    log_error("Failed to start packet receiver on the client");
    return 1;
  }

  tt_entity_manager_t entity_manager = {
      .cengine = _engine,
  };

  tt_asset_manager_t *assets = &entity_manager.cengine.asset_manager;

  player_tex = tt_asset_bind(assets, "tex/playr.png");

  tt_entity_t player = tt_entity_make(&entity_manager);

  entity_manager.player_handle = player;

  set_component(&entity_manager, player, texture, (.texture = player_tex));

  set_component(&entity_manager, player, position, (.x = 400, .y = 400));

  log_info("Trying to connect to server");
  while (!ttec_connect(&_engine, "127.0.0.1", 12345)) {
    log_warn("Connection attempt failed, retrying");
    tt_wait(500);
  }

  bool running = true;

  while (running) {
    ttb_render(entity_manager.cengine.backend,
               (ttb_render_cmd_t){.cmd_id = TTB_RENDER_CMD_CLEAR,
                                  .cmd_data.cmd_clear.bg_color =
                                      color_make(255, 255, 255, 255)});

    ttb_events_poll(entity_manager.cengine.backend,
                    &entity_manager.cengine.events);

    ttb_event_t *event;
    array_foreach(entity_manager.sengine.events, event) {
      switch (event->event_id) {
      case TTB_EVENT_WINDOW_CLOSED: {
        running = false;
      } break;
      default: {
      } break;
      }
    }

    system_render(&entity_manager);

    if (running) {
      running = ttec_update(&entity_manager.cengine);
    } else {
      ttec_update(&entity_manager.cengine);
    }
  }

  ttec_deinit(&entity_manager.cengine);

  return 0;
}
