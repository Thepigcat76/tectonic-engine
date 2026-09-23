#define TTN_PACKETS_ITER

#include "../include/tt_backend.h"
#include "../include/tt_engine.h"
#include "lilc/args.h"
#include "lilc/array.h"
#include "lilc/log.h"
#include "lilc/numbers.h"
#include <stddef.h>

typedef u64 tt_entity_t;

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
        .cmd_data.cmd_tex =
            {
                .texture = entity_tex.texture,
                .atlas_src =
                    {
                        .width = 16,
                        .height = 16,
                    },
                .dest =
                    {
                        .x = entity_pos.x,
                        .y = entity_pos.y,
                        .width = 16 * 10,
                        .height = 16 * 10,
                    },
                .tint_color = color_make(255, 255, 255, 255),
            },
    };

    ttb_render(entities->cengine.backend, render_cmd);
  }
}

static bool system_events(tt_entity_manager_t *entities) {
  ttb_events_poll(entities->cengine.backend);

  ttb_event_t event;
  while (ttb_event_pop(entities->cengine.backend, &event)) {
    switch (event.event_id) {
    case TTB_EVENT_WINDOW_CLOSED: {
      return false;
    } break;
    default: {
    } break;
    }
  }

  return true;
}

static bool system_client_packets(tt_entity_manager_t *entities) {
  tt_packet_t packet;
  while (ttec_packet_pop(&entities->cengine, &packet, entities)) {
    switch (packet.packet_id) {}
  }

  return true;
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

typedef enum {
  MYGAME_PACKET_PING,
  MYGAME_PACKET_HANDSHAKE,
} mygame_packet_id_e;

typedef struct {
  u64 time;
} mygame_packet_ping_t;

static tt_packet_info_t mygame_packet_infos[] = {
    {
        .packet_id = MYGAME_PACKET_PING,
        .packet_dir = TT_PACKET_DIR_TWO_WAY,
        .payload_size = sizeof(mygame_packet_ping_t),
        .encode_func = NULL,
        .decode_func = NULL,
    },
    {
        .packet_id = MYGAME_PACKET_HANDSHAKE,
        .packet_dir = TT_PACKET_DIR_CLIENT,
        .payload_size = 0,
        .encode_func = NULL,
        .decode_func = NULL,
    },
};

static void packets_add(tt_packet_info_array_t *packet_infos) {
  for (u64 ipacket_info = 0;
       ipacket_info < sizeof(mygame_packet_infos) / sizeof(tt_packet_info_t);
       ipacket_info++) {
    tt_packet_info_add(packet_infos, mygame_packet_infos[ipacket_info]);
  }

  tt_packet_info_lock(packet_infos);
}

static i32 server_run(void) {
  tt_entity_manager_t entity_manager = {.sengine = {0}};

  ttes_init(&entity_manager.sengine, false);

  packets_add(&entity_manager.sengine.server.packet_infos);

  if (!ttes_subproc_start(&entity_manager.sengine, TTES_SUBPROC_PACKET_RECV)) {
    log_error("Failed to start packet receiver on the server");
    return 1;
  }

  if (!ttes_subproc_start(&entity_manager.sengine, TTES_SUBPROC_CLIENT_ACCEPT)) {
    log_error("Failed to start client acceptor on the server");
    return 1;
  }

  ttes_server_host(&entity_manager.sengine, "127.0.0.1", 12345);

  bool running = true;

  while (running) {
    if (running) {
      running = ttes_update(&entity_manager.sengine);
    } else {
      ttes_update(&entity_manager.sengine);
    }
  }

  ttes_server_stop(&entity_manager.sengine);

  ttes_deinit(&entity_manager.sengine);

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

  packets_add(&_engine.client_connection.packet_infos);

  ttec_load_assets(&_engine);

  tt_entity_manager_t entity_manager = {
      .cengine = _engine,
  };

  if (!ttec_subproc_start(&entity_manager.cengine, TTEC_SUBPROC_PACKET_RECV)) {
    log_error("Failed to start packet receiver on the client");
    return 1;
  }

  tt_asset_manager_t *assets = &entity_manager.cengine.asset_manager;

  player_tex = tt_asset_bind(assets, "tex/playr.png");

  tt_entity_t player = tt_entity_make(&entity_manager);

  entity_manager.player_handle = player;

  set_component(&entity_manager, player, texture, (.texture = player_tex));

  bool running = true;

  i32 connection_retry_timer = 0;
  bool initial_connection = false;

  while (running) {
    set_component(&entity_manager, player, position, (.x = 400, .y = 400));

    if (!initial_connection && connection_retry_timer <= 0) {
      log_info("Trying to connect to server");
      if (!ttec_connect(&_engine, "127.0.0.1", 12345)) {
        log_warn("Connection attempt failed, retrying");
        connection_retry_timer = 500;
      } else {
        initial_connection = true;
      }
    }

    ttb_render(entity_manager.cengine.backend,
               (ttb_render_cmd_t){.cmd_id = TTB_RENDER_CMD_CLEAR,
                                  .cmd_data.cmd_clear.bg_color =
                                      color_make(255, 255, 255, 255)});

    bool evts_res = system_events(&entity_manager);
    if (running)
      running = evts_res;

    bool packets_res = system_client_packets(&entity_manager);
    if (running)
      running = packets_res;

    system_render(&entity_manager);

    if (running) {
      running = ttec_update(&entity_manager.cengine);
    } else {
      ttec_update(&entity_manager.cengine);
    }

    connection_retry_timer--;
  }

  ttec_client_stop(&entity_manager.cengine);

  ttec_deinit(&entity_manager.cengine);

  return 0;
}
