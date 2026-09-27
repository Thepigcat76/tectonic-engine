#include "../include/tt_backend.h"
#include "../include/tt_engine.h"
#include "../include/tt_translation.h"
#include "lilc/alloc.h"
#include "lilc/args.h"
#include "lilc/log.h"
#include "lilc/numbers.h"

#include <stddef.h>
#include <stdlib.h>

typedef u64 tt_entity_t;

#include "components.h"

#define MAX_ENTITIES_AMOUNT 4000

struct tt_entity_manager {
  component_pos_t positions[MAX_ENTITIES_AMOUNT];
  component_texture_t textures[MAX_ENTITIES_AMOUNT];

  tt_entity_t player_handle;

  tt_entity_t cur_entity_id;
  size_t entities_amount;

  tt_keymap_t keymap;
  tt_translation_manager_t translation_manager;

  tt_engine_client_t cengine;
  tt_engine_server_t sengine;
};

#define _F(...) {__VA_ARGS__}

#define set_component(entities_ptr, entity, comp, ...)                         \
  do {                                                                         \
    __VA_OPT__((entities_ptr)->comp##s[entity] =                               \
                   (typeof(*((entities_ptr)->comp##s)))_F __VA_ARGS__;)        \
    (entities_ptr)->comp##s[entity].present = true;                            \
  } while (0)

static tt_asset_id_t player_tex;
static tt_keymap_id_t move_forward_key;

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
    case TTB_EVENT_MOUSE_INPUT:
    case TTB_EVENT_KEYBOARD_INPUT: {
      tt_keymap_handle_input_event(&entities->cengine, &entities->keymap,
                                   &event);
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

static void mygame_packet_ping_encode(const tt_packet_t *packet,
                                      tt_byte_buf_t *buf, void *encode_ctx) {
  mygame_packet_ping_t *ping_packet = packet->payload;
  tt_encode_i64(ping_packet->time, buf, encode_ctx);
}
static void mygame_packet_ping_decode(tt_packet_t *packet, tt_byte_buf_t *buf,
                                      void *decode_ctx) {
  mygame_packet_ping_t *ping_packet = packet->payload;
  ping_packet->time = tt_decode_i64(buf, decode_ctx);
}

static tt_packet_info_t mygame_packet_infos[] = {
    [MYGAME_PACKET_PING] =
        {
            .packet_id = MYGAME_PACKET_PING,
            .packet_dir = TT_PACKET_DIR_TWO_WAY,
            .payload_size = sizeof(mygame_packet_ping_t),
            .encode_func = mygame_packet_ping_encode,
            .decode_func = mygame_packet_ping_decode,
        },
    [MYGAME_PACKET_HANDSHAKE] =
        {
            .packet_id = MYGAME_PACKET_PING,
            .packet_dir = TT_PACKET_DIR_CLIENT,
            .payload_size = 0,
            .encode_func = NULL,
            .decode_func = NULL,
        },
};

static void packets_add(tt_packet_info_array_t *packet_infos) {
  tt_packet_info_add(packet_infos, MYGAME_PACKET_PING,
                     mygame_packet_infos[MYGAME_PACKET_PING]);
  tt_packet_info_add(packet_infos, MYGAME_PACKET_HANDSHAKE,
                     mygame_packet_infos[MYGAME_PACKET_HANDSHAKE]);

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

  if (!ttes_subproc_start(&entity_manager.sengine,
                          TTES_SUBPROC_CLIENT_ACCEPT)) {
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

static void entity_manager_init(tt_entity_manager_t *entity_manager) {
  tt_keymap_init(&entity_manager->keymap, &HEAP_ALLOCATOR);
  tt_translation_manager_init(&entity_manager->translation_manager, "en_us",
                              &HEAP_ALLOCATOR);
}

static void entity_manager_deinit(tt_entity_manager_t *entity_manager) {
  tt_keymap_deinit(&entity_manager->keymap);
  tt_translation_manager_deinit(&entity_manager->translation_manager);
}

static void client_setup(tt_entity_manager_t *entity_manager) {
  entity_manager_init(entity_manager);

  // TODO: Seperate engine init and window opening
  tt_engine_init_t engine_init_state = {
      .window_title = "Engine Example",
      .resizeable = true,
  };
  entity_manager->cengine.asset_manager.assets_path = "assets/";
  ttec_init(&entity_manager->cengine, engine_init_state);

  packets_add(&entity_manager->cengine.client_connection.packet_infos);
  ttec_load_assets(&entity_manager->cengine);

  move_forward_key =
      tt_keymap_bind_key(&entity_manager->keymap, "Move Forward", TTB_KEY_W);

  tt_keymap_load(&entity_manager->keymap, "keys.toml", true, &HEAP_ALLOCATOR);
  tt_keymap_save(&entity_manager->keymap, "keys.toml", true);

  if (!ttec_subproc_start(&entity_manager->cengine, TTEC_SUBPROC_PACKET_RECV)) {
    log_error("Failed to start packet receiver on the client");
    exit(EXIT_FAILURE);
  }
}

static i32 client_run(void) {
  tt_entity_manager_t entity_manager = {0};

  client_setup(&entity_manager);

  tt_asset_manager_t *assets = &entity_manager.cengine.asset_manager;

  player_tex = tt_asset_bind(assets, "tex/player.png");

  tt_entity_t player = tt_entity_make(&entity_manager);

  entity_manager.player_handle = player;

  set_component(&entity_manager, player, texture, (.texture = player_tex));

  bool running = true;

  i32 connection_retry_timer = 0;
  bool initial_connection = false;

  set_component(&entity_manager, player, position, (.x = 400, .y = 400));

  while (running) {
    if (!initial_connection && connection_retry_timer <= 0) {
      log_info("Trying to connect to server");
      if (!ttec_connect(&entity_manager.cengine, "127.0.0.1", 12345)) {
        log_warn("Connection attempt failed, retrying");
        connection_retry_timer = 500;
      } else {
        log_info("Connected to server!");
        initial_connection = true;
      }
    }

    ttb_render(entity_manager.cengine.backend,
               (ttb_render_cmd_t){.cmd_id = TTB_RENDER_CMD_CLEAR,
                                  .cmd_data.cmd_clear.bg_color =
                                      color_make(255, 255, 255, 255)});

    ttb_render_cmd_t srvr_connect_text = {
        .cmd_id = TTB_RENDER_CMD_TEXT,
        .cmd_data.cmd_text = {.font_size = 36,
                              .text_color = color_make(0, 0, 0, 255),
                              .x = 300,
                              .y = 400},
    };

    if (!initial_connection) {
      srvr_connect_text.cmd_data.cmd_text.text = "Connecting to server";
    } else {
      srvr_connect_text.cmd_data.cmd_text.text = "Connected to server!";
    }

    ttb_render(entity_manager.cengine.backend, srvr_connect_text);

    srvr_connect_text.cmd_data.cmd_text.text = "Hiiiii";
    srvr_connect_text.cmd_data.cmd_text.font_size = 46;
    srvr_connect_text.cmd_data.cmd_text.text_color =
        color_make(255, 0, 145, 255);
    srvr_connect_text.cmd_data.cmd_text.x = 100;
    srvr_connect_text.cmd_data.cmd_text.y = 300;
    ttb_render(entity_manager.cengine.backend, srvr_connect_text);

    bool evts_res = system_events(&entity_manager);
    if (running)
      running = evts_res;

    bool packets_res = system_client_packets(&entity_manager);
    if (running)
      running = packets_res;

    if (ttec_key_down(&entity_manager.cengine, &entity_manager.keymap,
                      move_forward_key)) {
      set_component(
          &entity_manager, entity_manager.player_handle, position,
          (.x = entity_manager.positions[entity_manager.player_handle].x,
           .y = entity_manager.positions[entity_manager.player_handle].y - 10));
    }

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

  entity_manager_deinit(&entity_manager);

  return 0;
}
