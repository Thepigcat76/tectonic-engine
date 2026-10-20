#include "../include/tt_backend.h"
#include "../include/tt_camera.h"
#include "../include/tt_data.h"
#include "../include/tt_engine.h"
#include "../include/tt_math.h"
#include "../include/tt_translation.h"
#include "../include/tt_ui.h"
#include "lilc/alloc.h"
#include "lilc/args.h"
#include "lilc/dynstr.h"
#include "lilc/log.h"
#include "lilc/numbers.h"

#include <lilc/bump.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "components.h"
#include "entity_manager.h"
#include "packets.h"

static i32 client_run(void);
static i32 server_run(void);

i32 main(i32 argc, char **argv) {
  if (args_contains(argc, argv, "--server", NULL)) {
    return server_run();
  } else {
    return client_run();
  }
}

static f32 entity_x(tt_entity_manager_t *entities, tt_entity_t entity) {
  if (entities->c_position[entity].present) {
    return entities->c_position[entity].x;
  }
  return 0;
}

static f32 entity_y(tt_entity_manager_t *entities, tt_entity_t entity) {
  if (entities->c_position[entity].present) {
    return entities->c_position[entity].y;
  }
  return 0;
}

static f32 entity_width(tt_entity_manager_t *entities, tt_entity_t entity) {
  if (entities->c_box[entity].present) {
    return entities->c_box[entity].width;
  }
  return 0;
}

static f32 entity_height(tt_entity_manager_t *entities, tt_entity_t entity) {
  if (entities->c_box[entity].present) {
    return entities->c_box[entity].height;
  }
  return 0;
}

static f32 entity_depth(tt_entity_manager_t *entities, tt_entity_t entity) {
  if (entities->c_box[entity].present) {
    return entities->c_box[entity].depth;
  }
  return 0;
}

static f32 player_x(tt_entity_manager_t *entities) {
  if (entities->c_position[entities->player_handle].present) {
    return entities->c_position[entities->player_handle].x;
  }
  return 0;
}

static f32 player_y(tt_entity_manager_t *entities) {
  if (entities->c_position[entities->player_handle].present) {
    return entities->c_position[entities->player_handle].y;
  }
  return 0;
}

static f32 entity_vel_x(tt_entity_manager_t *entities, tt_entity_t entity) {
  if (entities->c_velocity[entity].present) {
    return entities->c_velocity[entity].x_velocity;
  }
  return 0;
}

static f32 entity_vel_y(tt_entity_manager_t *entities, tt_entity_t entity) {
  if (entities->c_velocity[entity].present) {
    return entities->c_velocity[entity].y_velocity;
  }
  return 0;
}

static tt_asset_id_t player_tex;
static tt_asset_id_t crate_tex;

static tt_keymap_id_t move_forward_key;
static tt_keymap_id_t move_backward_key;
static tt_keymap_id_t move_left_key;
static tt_keymap_id_t move_right_key;

static tt_keymap_id_t open_esc_menu_key;

tt_entity_t tt_entity_make(tt_entity_manager_t *entities) {
  entities->entities_amount += 1;
  return entities->cur_entity_id++;
}

static void entity_render(tt_entity_manager_t *entities, tt_entity_t entity) {
  component_pos_t entity_pos = entities->c_position[entity];
  if (!entity_pos.present) {
    return;
  }

  component_texture_t entity_tex = entities->c_texture[entity];
  if (!entity_tex.present) {
    return;
  }

  ttb_render_cmd_t render_entity_texture = {
      .cmd_id = TTB_RENDER_CMD_TEXTURE,
      .cmd_data.cmd_tex =
          {
              .texture = entity_tex.texture,
              .dest.x = entity_pos.x,
              .dest.y = entity_pos.y,
              .tint_color = color_make(255, 255, 255, 255),
          },
  };

  ttb_render(entities->cengine.backend, render_entity_texture);
}

typedef enum {
  RENDER_LAYER_BEFORE_PLAYER,
  RENDER_LAYER_BEHIND_PLAYER,
} render_layer_e;

static void entities_render(tt_entity_manager_t *entities,
                            render_layer_e layer) {
  for (tt_entity_t entity = 0; entity < entities->entities_amount; entity++) {
    if (entity == entities->player_handle)
      continue;
    if (!entities->c_position[entity].present)
      continue;

    bool entity_has_box = entities->c_box[entity].present;
    component_box_t entity_box = entities->c_box[entity];

    bool before_player =
        entity_has_box && layer == RENDER_LAYER_BEFORE_PLAYER &&
        entities->c_position[entity].y + entity_box.height + entity_box.depth >=
            player_y(entities) + 16;
    bool behind_player =
        entity_has_box && layer == RENDER_LAYER_BEHIND_PLAYER && !before_player;
    if (behind_player || before_player || !entity_has_box) {
      entity_render(entities, entity);
    }
  }
}

static void system_entities_render(tt_entity_manager_t *entities) {
  entities_render(entities, RENDER_LAYER_BEHIND_PLAYER);
  entity_render(entities, entities->player_handle);
  entities_render(entities, RENDER_LAYER_BEFORE_PLAYER);
}

static bool entity_collides(const tt_entity_manager_t *entities,
                            tt_entity_t entity0, tt_entity_t entity1) {
  component_pos_t pos0 = entities->c_position[entity0];
  component_pos_t pos1 = entities->c_position[entity1];

  component_box_t box0 = entities->c_box[entity0];
  component_box_t box1 = entities->c_box[entity1];

  return tt_collides(pos0.x, pos0.y + box0.height, box0.width, box0.depth,
                     pos1.x, pos1.y + box1.height, box1.width, box1.depth);
}

static void system_velocity(tt_entity_manager_t *entities) {
  f64 frame_time = ttb_frame_time(entities->cengine.backend);
  for (tt_entity_t entity_id = 0; entity_id < entities->entities_amount;
       entity_id++) {
    if (!entities->c_velocity[entity_id].present ||
        !entities->c_position[entity_id].present)
      continue;

    component_pos_t *entity_pos = &entities->c_position[entity_id];
    component_velocity_t *entity_vel = &entities->c_velocity[entity_id];
    component_box_t entity_box = entities->c_box[entity_id];

    // Apply x-movement and collision checking
    entity_pos->x += entity_vel->x_velocity * frame_time;
    if (entity_box.present) {
      for (tt_entity_t collision_entity = 0;
           collision_entity < entities->entities_amount; collision_entity++) {
        component_box_t collision_box = entities->c_box[collision_entity];
        if (entity_id == collision_entity || !collision_box.present ||
            !entities->c_position[collision_entity].present)
          continue;

        if (entity_collides(entities, entity_id, collision_entity)) {
          if (entity_vel->x_velocity < 0) {
            entity_pos->x =
                entity_x(entities, collision_entity) + collision_box.width;
          } else if (entity_vel->x_velocity > 0) {
            entity_pos->x =
                entity_x(entities, collision_entity) - entity_box.width;
          }
          entity_vel->x_velocity = 0;
        }
      }
    }

    // Apply y-movement and collision checking
    entity_pos->y += entity_vel->y_velocity * frame_time;
    if (entity_box.present) {
      for (tt_entity_t collision_entity = 0;
           collision_entity < entities->entities_amount; collision_entity++) {
        component_box_t collision_box = entities->c_box[collision_entity];
        if (entity_id == collision_entity || !collision_box.present ||
            !entities->c_position[collision_entity].present)
          continue;

        if (entity_collides(entities, entity_id, collision_entity)) {
          if (entity_vel->y_velocity < 0) {
            entity_pos->y = entity_y(entities, collision_entity) +
                            collision_box.height + collision_box.depth -
                            entity_box.height;
          } else if (entity_vel->y_velocity > 0) {
            entity_pos->y = entity_y(entities, collision_entity) +
                            collision_box.height - entity_box.height -
                            entity_box.depth;
          }
          entity_vel->y_velocity = 0;
        }
      }
    }
  }
}

#define PLAYER_SPEED 100.0f

static bool system_input(tt_entity_manager_t *entities) {
  f32 dx = 0, dy = 0;

  if (ttec_key_down(&entities->cengine, &entities->keymap, move_forward_key)) {
    dy -= 1;
  }

  if (ttec_key_down(&entities->cengine, &entities->keymap, move_backward_key)) {
    dy += 1;
  }

  if (ttec_key_down(&entities->cengine, &entities->keymap, move_left_key)) {
    dx -= 1;
  }

  if (ttec_key_down(&entities->cengine, &entities->keymap, move_right_key)) {
    dx += 1;
  }

  f32 len = sqrtf(dx * dx + dy * dy);

  if (len > 0.0f) {
    dx /= len;
    dy /= len;
  }

  set_component(
      entities, entities->player_handle, velocity,
      (.x_velocity = dx * PLAYER_SPEED, .y_velocity = dy * PLAYER_SPEED));

  if (ttec_key_pressed(&entities->cengine, &entities->keymap,
                       open_esc_menu_key)) {
    entities->menu_open = !entities->menu_open;
  }

  return true;
}

static struct {
  bool mouse_left_button_released;
} mouse_input = {0};

static bool system_events(tt_entity_manager_t *entities) {
  memset(&mouse_input, 0, sizeof(mouse_input));

  ttb_events_poll(entities->cengine.backend);

  ttb_event_t event;
  while (ttb_event_pop(entities->cengine.backend, &event)) {
    switch (event.event_id) {
    case TTB_EVENT_WINDOW_CLOSED: {
      return false;
    } break;
    case TTB_EVENT_MOUSE_INPUT: {
      if (event.event_data.evt_mouse_input.left_button ==
          TTB_KEY_STATE_RELEASED) {
        mouse_input.mouse_left_button_released = true;
      }
    } break;
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

typedef enum {
  UI_RESULT_PASS,
  UI_RESULT_CONSUME_CLICK,
  UI_RESULT_EXIT_GAME,
} ui_result_e;

static ui_result_e ui_render(tt_entity_manager_t *entities,
                             ttui_layout_t *layout) {
  ui_result_e ui_res = UI_RESULT_PASS;

  if (ttui_button(
          entities->cengine.backend, layout,
          ttui_text(
              {.text = "Hello", .text_size = 10, .text_color = TT_COLOR_WHITE}),
          0, 0, 100, 30, mouse_input.mouse_left_button_released)) {
    log_debug("hello 0 button pressed");
    ui_res = UI_RESULT_CONSUME_CLICK;
  }
  if (ttui_button(
          entities->cengine.backend, layout,
          ttui_text(
              {.text = "World", .text_size = 10, .text_color = TT_COLOR_WHITE}),
          0, 0, 100, 30, mouse_input.mouse_left_button_released)) {
    log_debug("world button pressed");
    ui_res = UI_RESULT_CONSUME_CLICK;
  }
  ttui_label(
      entities->cengine.backend, layout,
      ttui_text(
          {.text = "Label", .text_size = 10, .text_color = TT_COLOR_WHITE}),
      0, 0, 100, 30);
  if (ttui_button(
          entities->cengine.backend, layout,
          ttui_text(
              {.text = "Hello", .text_size = 10, .text_color = TT_COLOR_WHITE}),
          0, 0, 100, 30, mouse_input.mouse_left_button_released)) {
    log_debug("hello 1 button pressed");
    ui_res = UI_RESULT_CONSUME_CLICK;
  }

  return ui_res;
}

static ui_result_e system_ui(tt_entity_manager_t *entities) {
  static ttui_layout_t layout = {
      .kind = TTUI_LAYOUT_VERTICAL,
      .padding = 32,
      .width = 400,
      .height = 400,
      .ui_scale = 2.4,
  };
  layout.x = (ttb_screen_width(entities->cengine.backend) - 400) / 2;
  layout.y = (ttb_screen_height(entities->cengine.backend) - 400) / 2;

  ui_result_e res = UI_RESULT_PASS;
  if (entities->menu_open) {
    // First ui pass, calculate positions and width
    layout.render = false;

    ui_render(entities, &layout);

    ttui_layout_finish(entities->cengine.backend, &layout);

    // Second ui pass, render elements and calculated positions and widths
    layout.render = true;

    res = ui_render(entities, &layout);

    ttui_layout_finish(entities->cengine.backend, &layout);
  }
  return res;
}

static bool system_client_packets(tt_entity_manager_t *entities) {
  tt_packet_t packet;
  while (ttec_packet_pop(&entities->cengine, &packet, NULL, entities)) {
    log_debug("[CLIENT] Received Packet: %zu", packet.packet_id);
    switch ((mygame_packet_id_e)packet.packet_id) {
    case MYGAME_PACKET_PING: {
    } break;
    case MYGAME_PACKET_HANDSHAKE_REQ: {
    } break;
    case MYGAME_PACKET_HANDSHAKE_ACCEPT: {
    } break;
    case MYGAME_PACKET_HANDSHAKE_REJECT: {
    } break;
    }
  }

  return true;
}

static void packets_add(tt_packet_info_array_t *packet_infos) {
  tt_packet_info_add(packet_infos, mygame_packet_infos[MYGAME_PACKET_PING]);
  tt_packet_info_add(packet_infos,
                     mygame_packet_infos[MYGAME_PACKET_HANDSHAKE_REQ]);
  tt_packet_info_add(packet_infos,
                     mygame_packet_infos[MYGAME_PACKET_HANDSHAKE_ACCEPT]);
  tt_packet_info_add(packet_infos,
                     mygame_packet_infos[MYGAME_PACKET_HANDSHAKE_REJECT]);

  tt_packet_info_lock(packet_infos);
}

static void server_system_cli(tt_entity_manager_t *entities) {}

static i32 server_run(void) {
  tt_entity_manager_t entity_manager = {.sengine = {0}};

  entity_manager_init_server(&entity_manager);

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
    tt_packet_context_t packet_ctx;
    tt_packet_t packet;
    if (ttes_packet_pop(&entity_manager.sengine, &packet, &packet_ctx,
                        &entity_manager)) {
      log_debug("[SERVER] Received Packet : %zu", packet.packet_id);
      switch ((mygame_packet_id_e)packet.packet_id) {
      case MYGAME_PACKET_PING: {
      } break;
      case MYGAME_PACKET_HANDSHAKE_REQ: {
        mygame_packet_handshake_req_t *payload = packet.payload;
        if (payload->protocol_version != PROTOCOL_VERSION) {
          log_warn(
              "Client with invalid protocol version %zu attempted to connect",
              payload->protocol_version);

          tt_packet_t reject_packet = {0};
          mygame_packet_handshake_reject_t reject_payload = {.reject_reason =
                                                                 0};
          ttes_packet_init(&entity_manager.sengine, &reject_packet,
                           MYGAME_PACKET_HANDSHAKE_REJECT, &reject_payload);

          tt_packet_send_client(&entity_manager.sengine, packet_ctx.sender,
                                reject_packet, &entity_manager);
        } else {
          log_info("Accepted Client %s", payload->player_name);

          tt_packet_t accept_packet = {0};
          mygame_packet_handshake_accept_t accept_payload = {.client_id = 0};
          ttes_packet_init(&entity_manager.sengine, &accept_packet,
                           MYGAME_PACKET_HANDSHAKE_ACCEPT, &accept_payload);

          tt_packet_send_client(&entity_manager.sengine, packet_ctx.sender,
                                accept_packet, &entity_manager);
        }
      } break;
      case MYGAME_PACKET_HANDSHAKE_ACCEPT: {
      } break;
      case MYGAME_PACKET_HANDSHAKE_REJECT: {
      } break;
      }
    }

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

static void crate_spawn(tt_entity_manager_t *entities) {
  tt_entity_t crate_entity = tt_entity_make(entities);
  set_component(
      entities, crate_entity, position,
      (.x = ttb_screen_to_cam_x(entities->cengine.backend,
                                ttb_mouse_x(entities->cengine.backend),
                                &entities->camera),
       .y = ttb_screen_to_cam_y(entities->cengine.backend,
                                ttb_mouse_y(entities->cengine.backend),
                                &entities->camera)));
  set_component(entities, crate_entity, texture, (.texture = crate_tex));
  set_component(entities, crate_entity, box,
                (.width = 24, .height = 14, .depth = 10));
  log_debug("Spawned crate");
}

static void keybinds_setup(tt_entity_manager_t *entities) {
  move_forward_key =
      tt_keymap_bind_key(&entities->keymap, "keybind.move.forward", TTB_KEY_W);
  move_backward_key =
      tt_keymap_bind_key(&entities->keymap, "keybind.move.backward", TTB_KEY_S);
  move_left_key =
      tt_keymap_bind_key(&entities->keymap, "keybind.move.left", TTB_KEY_A);
  move_right_key =
      tt_keymap_bind_key(&entities->keymap, "keybind.move.right", TTB_KEY_D);
  open_esc_menu_key = tt_keymap_bind_key(
      &entities->keymap, "keybind.open_esc_menu", TTB_KEY_ESCAPE);
}

static void client_setup(tt_entity_manager_t *entity_manager) {
  // TODO: Seperate engine init and window opening
  tt_engine_init_t engine_init_state = {
      .window_title = "Engine Example",
      .resizeable = true,
  };
  entity_manager->cengine.asset_manager.assets_path = "assets/";
  ttec_init(&entity_manager->cengine, engine_init_state);

  packets_add(&entity_manager->cengine.client_connection.packet_infos);
  ttec_load_assets(&entity_manager->cengine);

  entity_manager_init_client(entity_manager);
  keybinds_setup(entity_manager);

  tt_keymap_load(&entity_manager->keymap, "keys.toml", true, &HEAP_ALLOCATOR);
  tt_keymap_save(&entity_manager->keymap, "keys.toml", true);

  tt_translation_manager_init(&entity_manager->translation_manager, "en_us",
                              &HEAP_ALLOCATOR);

  tt_translation_load(&entity_manager->translation_manager, "en_us",
                      "en_us.toml");

  // tt_translation_insert(&entity_manager->translation_manager, "en_us",
  // "text.example", "Some Example Text lol");
  // tt_translation_insert(&entity_manager->translation_manager, "en_us",
  // "sussy.baka", "Its 2020 again :p");
  //
  // tt_translation_save(&entity_manager->translation_manager, "en_us",
  // "en_us.toml");

  tt_data_object_t data = {0};
  tt_data_object_init(&data, &HEAP_ALLOCATOR);
  //
  // tt_data_object_insert(&data, dyn_string_makef(&HEAP_ALLOCATOR,
  // "example_key"), tt_data_value_i64(67)); tt_data_object_insert(&data,
  // dyn_string_makef(&HEAP_ALLOCATOR, "sussy_baka"), tt_data_value_i64(43));
  // tt_data_object_insert(&data, dyn_string_makef(&HEAP_ALLOCATOR, "polaroid"),
  // tt_data_value_string(&HEAP_ALLOCATOR, "Hiii"));

  tt_data_value_t obj_val = tt_data_value_object(data);

  // if (!tt_data_value_write(&obj_val, "saved_data.dat")) {
  //   log_error("Failed to write data");
  //   return;
  // }

  if (!tt_data_value_read(&obj_val, "saved_data.dat", &HEAP_ALLOCATOR)) {
    log_error("Failed to read data");
    return;
  }

  dyn_string_t str = {0};
  dyn_string_init(&str, &HEAP_ALLOCATOR);
  tt_data_value_dump(&obj_val, &str);

  FILE *f = fopen("dumped_data.json", "w");
  if (f == NULL) {
    log_error("Failed to open dumped data file for writing");
    return;
  }

  fputs(str.string, f);

  fclose(f);

  if (!ttec_subproc_start(&entity_manager->cengine, TTEC_SUBPROC_PACKET_RECV)) {
    log_error("Failed to start packet receiver on the client");
    exit(EXIT_FAILURE);
  }

  ttui_set_config_default(&entity_manager->cengine.asset_manager);
}

static i32 client_run(void) {
  tt_entity_manager_t entity_manager = {0};

  client_setup(&entity_manager);

  bump_t temp_bump = {0};
  bump_init(&temp_bump, 128000);

  allocator_t temp_alloc = {0};
  bump_allocator_init(&temp_alloc, &temp_bump);

  tt_asset_manager_t *assets = &entity_manager.cengine.asset_manager;

  player_tex = tt_asset_bind(assets, "tex/player.png");
  crate_tex = tt_asset_bind(assets, "tex/crate.png");

  tt_entity_t _player = tt_entity_make(&entity_manager);
  set_component(&entity_manager, _player, position, (.x = 0, .y = 0));
  set_component(&entity_manager, _player, box,
                (.width = 8, .height = 12, .depth = 4));
  set_component(&entity_manager, _player, texture, (.texture = player_tex));

  entity_manager.player_handle = _player;

  bool running = true;

  i32 connection_retry_timer = 0;
  bool initial_connection = false;

  while (running) {
    if (!initial_connection && connection_retry_timer <= 0) {
      log_info("Trying to connect to server");
      if (!ttec_connect(&entity_manager.cengine, "127.0.0.1", 12345)) {
        log_warn("Connection attempt failed, retrying");
        connection_retry_timer = 500;
      } else {
        log_info("Connected to server!");
        initial_connection = true;

        mygame_packet_handshake_req_t payload = {
            .protocol_version = PROTOCOL_VERSION, .player_name = "Dev"};
        tt_packet_t handshake_req = {0};
        ttec_packet_init(&entity_manager.cengine, &handshake_req,
                         MYGAME_PACKET_HANDSHAKE_REQ, &payload);

        tt_packet_send_server(&entity_manager.cengine, handshake_req,
                              &entity_manager);
        log_info("[CLIENT] Requested handshake from server");
      }
    }

    bool evts_res = system_events(&entity_manager);
    if (running)
      running = evts_res;

    bool packets_res = system_client_packets(&entity_manager);
    if (running)
      running = packets_res;

    bool input_res = system_input(&entity_manager);
    if (running)
      running = input_res;

    system_velocity(&entity_manager);

    if (entity_manager.c_position[entity_manager.player_handle].present) {
      entity_manager.camera.target_x =
          player_x(&entity_manager) +
          entity_width(&entity_manager, entity_manager.player_handle) / 2;
      entity_manager.camera.target_y =
          player_y(&entity_manager) +
          entity_height(&entity_manager, entity_manager.player_handle) / 2;
    } else {
      entity_manager.camera.target_x =
          ttb_screen_width(entity_manager.cengine.backend) / 2;
      entity_manager.camera.target_y =
          ttb_screen_height(entity_manager.cengine.backend) / 2;
    }
    entity_manager.camera.offset_x =
        ttb_screen_width(entity_manager.cengine.backend) / 2;
    entity_manager.camera.offset_y =
        ttb_screen_height(entity_manager.cengine.backend) / 2;

    ttb_render(entity_manager.cengine.backend,
               (ttb_render_cmd_t){.cmd_id = TTB_RENDER_CMD_CLEAR,
                                  .cmd_data.cmd_clear.bg_color =
                                      color_make(255, 255, 255, 255)});

    ttb_render(entity_manager.cengine.backend,
               (ttb_render_cmd_t){
                   .cmd_id = TTB_RENDER_CMD_APPLY_CAM,
                   .cmd_data.cmd_apply_cam.cam = entity_manager.camera,
               });

    system_entities_render(&entity_manager);

    ttb_render(
        entity_manager.cengine.backend,
        (ttb_render_cmd_t){
            .cmd_id = TTB_RENDER_CMD_UNAPPLY_CAM,
            .cmd_data.cmd_unapply_cam.cam_id = entity_manager.camera.cam_id,
        });

    ttb_render_cmd_t srvr_connect_text = {
        .cmd_id = TTB_RENDER_CMD_TEXT,
        .cmd_data.cmd_text = {.font_size = 36,
                              .text_color = color_make(0, 0, 0, 255),
                              .x = 0,
                              .y = 400},
    };

    if (!initial_connection) {
      srvr_connect_text.cmd_data.cmd_text.text = "Connecting to server";
    } else {
      srvr_connect_text.cmd_data.cmd_text.text = "Connected to server!";
    }

    ttb_render(entity_manager.cengine.backend, srvr_connect_text);

    srvr_connect_text.cmd_data.cmd_text.text =
        dyn_string_makef(
            &temp_alloc, "%s, %d",
            tt_translations_lookup(&entity_manager.translation_manager,
                                   "msg.welcome"),
            ttb_fps(entity_manager.cengine.backend))
            .string;
    srvr_connect_text.cmd_data.cmd_text.font_size = 46;
    srvr_connect_text.cmd_data.cmd_text.text_color =
        color_make(255, 0, 145, 255);
    srvr_connect_text.cmd_data.cmd_text.x = 100;
    srvr_connect_text.cmd_data.cmd_text.y = 300;
    ttb_render(entity_manager.cengine.backend, srvr_connect_text);

    ui_result_e ui_res = system_ui(&entity_manager);
    if (ui_res == UI_RESULT_EXIT_GAME) {
      running = false;
    } else if ((ui_res == UI_RESULT_PASS || !entity_manager.menu_open) &&
               mouse_input.mouse_left_button_released) {
      crate_spawn(&entity_manager);
    }

    if (running) {
      running = ttec_update(&entity_manager.cengine);
    } else {
      ttec_update(&entity_manager.cengine);
    }

    bump_reset(&temp_bump);

    connection_retry_timer--;
  }

  ttec_client_stop(&entity_manager.cengine);

  ttec_deinit(&entity_manager.cengine);

  bump_free(&temp_bump);

  entity_manager_deinit(&entity_manager);

  return 0;
}

void entity_manager_init_client(tt_entity_manager_t *entity_manager) {
  tt_keymap_init(&entity_manager->keymap, &HEAP_ALLOCATOR);
  tt_translation_manager_init(&entity_manager->translation_manager, "en_us",
                              &HEAP_ALLOCATOR);

  tt_camera_init(&entity_manager->camera, entity_manager->cengine.backend);
  entity_manager->camera.zoom = 4;
}

void entity_manager_init_server(tt_entity_manager_t *entity_manager) {
  bump_init(&entity_manager->server_client_info_bump, 4800);
  bump_allocator_init(&entity_manager->server_client_info_alloc,
                      &entity_manager->server_client_info_bump);
}

void entity_manager_deinit(tt_entity_manager_t *entity_manager) {
  tt_keymap_deinit(&entity_manager->keymap);
  tt_translation_manager_deinit(&entity_manager->translation_manager);
}
