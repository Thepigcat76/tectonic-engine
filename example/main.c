#include "../engine/include/tt_backend.h"
#include "../engine/include/tt_camera.h"
#include "../engine/include/tt_engine.h"
#include "../engine/include/tt_math.h"
#include "../engine/include/tt_translation.h"
#include "../engine/include/tt_ui.h"
#include "lilc/alloc.h"
#include "lilc/args.h"
#include "lilc/bump.h"
#include "lilc/dynstr.h"
#include "lilc/log.h"
#include "lilc/numbers.h"
#include "lilc/str.h"

#include <math.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>

#include "components.h"
#include "entity_manager.h"
#include "packets.h"
#include "shared.h"

#define _COMP_ZERO(ty, name, encode_fn, decode_fn, entities_ptr, player, ...)  \
  memset(&(entities_ptr)->name[player], 0, sizeof(ty));
#define COMPONENTS_ZERO(entities_ptr, player)                                  \
  do {                                                                         \
    _COMPONENTS_ITER(_COMP_ZERO, entities_ptr, player)                         \
  } while (0)

static i32 client_run(color_t color, const char *ip_addr, u32 port);
static i32 server_run(const char *ip_addr, u32 port);

i32 main(i32 argc, char *argv[]) {
#ifndef BUILTIN_SERVER
  bool server = args_contains(argc, argv, "--server", NULL);
#else
  bool server = true;
#endif

  color_t color = TT_COLOR_BLUE;

  isz color_idx = -1;
  if (args_contains(argc, argv, "--color", &color_idx)) {
    if (argc > color_idx + 1) {
      if (str_eq(argv[color_idx + 1], "blue")) {
        color = TT_COLOR_BLUE;
      } else if (str_eq(argv[color_idx + 1], "red")) {
        color = TT_COLOR_RED;
      } else if (str_eq(argv[color_idx + 1], "green")) {
        color = TT_COLOR_GREEN;
      } else {
        log_warn("Invalid color %s, expected one of [\"blue\", \"red\", "
                 "\"green\"]",
                 argv[color_idx + 1]);
      }
    } else {
      log_warn("No color was provided after --color");
    }
  }

  const char *ip_lit = "127.0.0.1";
  isz ip_idx = -1;
  if (args_contains(argc, argv, "--ip", &ip_idx)) {
    if (argc > ip_idx + 1) {
      ip_lit = argv[ip_idx + 1];
    }
  }

  u32 port = 12345;
  isz port_idx = -1;
  if (args_contains(argc, argv, "--port", &port_idx)) {
    if (argc > port_idx + 1) {
      port = atoi(argv[port_idx + 1]);
    }
  }

  if (server) {
    return server_run(ip_lit, port);
  } else {
    return client_run(color, ip_lit, port);
  }
}

static tt_asset_id_t player_tex;
static tt_asset_id_t crate_tex;

static tt_keymap_id_t move_forward_key;
static tt_keymap_id_t move_backward_key;
static tt_keymap_id_t move_left_key;
static tt_keymap_id_t move_right_key;

static tt_keymap_id_t open_esc_menu_key;

entity_t tt_entity_make(entity_manager_t *entities) {
  log_info("Created entity with id: %zu", entities->entities_amount);
  return entities->entities_amount++;
}

static bool player_by_client_id(entity_manager_t *entities, u64 client_id,
                                entity_t *player) {
  bool found_player = false;
  for (entity_t entity = 0; entity < entities->entities_amount; entity++) {
    component_entity_kind_t comp_ekind = entities->c_entity_kind[entity];
    component_client_id_t comp_cid = entities->c_client_id[entity];
    if (comp_ekind.present && comp_ekind.entity_kind == ENTITY_KIND_PLAYER &&
        comp_cid.present) {
      if (comp_cid.client_id == client_id) {
        found_player = true;
        if (player != NULL) {
          *player = entity;
          break;
        }
      }
    }
  }

  return found_player;
}

static void entity_render(entity_manager_t *entities, entity_t entity) {
  component_pos_t entity_pos = entities->c_position[entity];
  if (!entity_pos.present) {
    return;
  }

  component_texture_t entity_tex = entities->c_texture[entity];
  if (!entity_tex.present) {
    return;
  }

  color_t tint_color = TT_COLOR_WHITE;
  component_color_t entity_color = entities->c_color[entity];
  if (entity_color.present) {
    tint_color = entity_color.color;
  }

  ttb_render_cmd_t render_entity_texture = {
      .cmd_id = TTB_RENDER_CMD_TEXTURE,
      .cmd_data.cmd_tex =
          {
              .texture = entity_tex.texture,
              .dest.x = entity_pos.x,
              .dest.y = entity_pos.y,
              .tint_color = tint_color,
          },
  };

  ttb_render(entities->cengine.backend, render_entity_texture);
}

typedef enum {
  RENDER_LAYER_BEFORE_PLAYER,
  RENDER_LAYER_BEHIND_PLAYER,
} render_layer_e;

static void entities_render(entity_manager_t *entities, render_layer_e layer) {
  for (entity_t entity = 0; entity < entities->entities_amount; entity++) {
    if (entities->has_player && entity == entities->client_player_handle)
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

static void system_entities_render(entity_manager_t *entities) {
  entities_render(entities, RENDER_LAYER_BEHIND_PLAYER);
  if (entities->has_player) {
    entity_render(entities, entities->client_player_handle);
  }
  entities_render(entities, RENDER_LAYER_BEFORE_PLAYER);
}

static bool entity_collides(const entity_manager_t *entities, entity_t entity0,
                            entity_t entity1) {
  component_pos_t pos0 = entities->c_position[entity0];
  component_pos_t pos1 = entities->c_position[entity1];

  component_box_t box0 = entities->c_box[entity0];
  component_box_t box1 = entities->c_box[entity1];

  return tt_collides(pos0.x, pos0.y + box0.height, box0.width, box0.depth,
                     pos1.x, pos1.y + box1.height, box1.width, box1.depth);
}

#define PLAYER_SPEED 100.0f
#define TICK_RATE 60
#define TICK_DT (1.0 / TICK_RATE)
#define SNAPSHOT_INTERVAL 3 // ticks -> 20 Hz

static void entity_step(entity_manager_t *entities, entity_t entity_id,
                        f64 dt) {
  if (!entities->c_velocity[entity_id].present ||
      !entities->c_position[entity_id].present)
    return;

  component_pos_t *entity_pos = &entities->c_position[entity_id];
  component_velocity_t *entity_vel = &entities->c_velocity[entity_id];
  component_box_t entity_box = entities->c_box[entity_id];

  entity_pos->x += entity_vel->x_velocity * dt;
  if (entity_box.present) {
    for (entity_t collision_entity = 0;
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

  entity_pos->y += entity_vel->y_velocity * dt;
  if (entity_box.present) {
    for (entity_t collision_entity = 0;
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

static void system_velocity(entity_manager_t *entities, f64 dt) {
  for (entity_t id = 0; id < entities->entities_amount; id++)
    entity_step(entities, id, dt);
}

// One command = one tick of movement. Used by client AND server.
static void apply_move_cmd(entity_manager_t *entities, entity_t player,
                           const mygame_packet_movement_input_t *in, f64 dt) {
  f32 dx = (f32)in->right - (f32)in->left;
  f32 dy = (f32)in->backward - (f32)in->forward;
  f32 len = sqrtf(dx * dx + dy * dy);
  if (len > 0.0f) {
    dx /= len;
    dy /= len;
  }
  set_component(
      entities, player, velocity,
      (.x_velocity = dx * PLAYER_SPEED, .y_velocity = dy * PLAYER_SPEED));
  entity_step(entities, player, dt);
  // Velocity is derived from input every tick, never persisted
  set_component(entities, player, velocity, (.x_velocity = 0, .y_velocity = 0));
}

#define PENDING_MAX 128
static mygame_packet_movement_input_t pending[PENDING_MAX];
static usz pending_len = 0;
static u32 next_seq = 0;
static mygame_packet_movement_input_t prev_cmd = {0};
static mygame_packet_movement_input_t remote_keys[MAX_ENTITIES_AMOUNT];

static void pending_push(const mygame_packet_movement_input_t *cmd) {
  if (pending_len == PENDING_MAX) {
    memmove(pending, pending + 1, (PENDING_MAX - 1) * sizeof(*pending));
    pending_len--;
  }
  pending[pending_len++] = *cmd;
}

static void pending_ack(u32 seq) {
  usz drop = 0;
  while (drop < pending_len && pending[drop].seq <= seq)
    drop++;
  memmove(pending, pending + drop, (pending_len - drop) * sizeof(*pending));
  pending_len -= drop;
}

static bool cmd_any(const mygame_packet_movement_input_t *c) {
  return c->forward || c->backward || c->left || c->right;
}

static void client_tick(entity_manager_t *entities) {
  for (entity_t e = 0; e < entities->entities_amount; e++) {
    if (entities->has_player && e == entities->client_player_handle)
      continue;
    if (entities->c_entity_kind[e].present &&
        entities->c_entity_kind[e].entity_kind == ENTITY_KIND_PLAYER &&
        entities->c_position[e].present) {
      apply_move_cmd(entities, e, &remote_keys[e], TICK_DT);
    }
  }

  if (!entities->has_player)
    return;

  mygame_packet_movement_input_t cmd = {0};
  struct {
    tt_keymap_id_t key;
    bool *out;
  } keys[] = {
      {move_forward_key, &cmd.forward},
      {move_backward_key, &cmd.backward},
      {move_left_key, &cmd.left},
      {move_right_key, &cmd.right},
  };
  for (usz i = 0; i < 4; i++)
    *keys[i].out =
        ttec_key_down(&entities->cengine, &entities->keymap, keys[i].key);

  // Nothing pressed now or last tick: nothing to simulate or send
  if (!cmd_any(&cmd) && !cmd_any(&prev_cmd))
    return;
  prev_cmd = cmd;

  cmd.seq = ++next_seq;
  pending_push(&cmd);
  apply_move_cmd(entities, entities->client_player_handle, &cmd, TICK_DT);

  tt_packet_t packet = {0};
  ttec_packet_init(&entities->cengine, &packet, MYGAME_PACKET_MOVEMENT_INPUT,
                   &cmd);
  tt_packet_send_server(&entities->cengine, packet, entities);
}

static bool system_input(entity_manager_t *entities) {
  if (ttec_key_pressed(&entities->cengine, &entities->keymap,
                       open_esc_menu_key)) {
    entities->menu_open = !entities->menu_open;
  }
  return true;
}

static void send_entity_create_to_client(entity_manager_t *entities,
                                         entity_t entity) {
  tt_packet_t create_entity_packet = {0};
  mygame_packet_create_entity_t payload = {
      .entity_id = entity,
  };
#define COMP_COPY(ty, name, e_fn, d_fn, entity, ...)                           \
  payload.name = entities->name[entity];
  _COMPONENTS_ITER(COMP_COPY, entity)
#undef COMP_COPY

  ttes_packet_init(&entities->sengine, &create_entity_packet,
                   MYGAME_PACKET_CREATE_ENTITY, &payload);

  tt_packet_send_all_clients(&entities->sengine, create_entity_packet,
                             entities);
}

static entity_t crate_spawn(entity_manager_t *entities, i32 x, i32 y) {
  entity_t crate_entity = tt_entity_make(entities);
  set_component(entities, crate_entity, position, (.x = x, .y = y));
  set_component(entities, crate_entity, texture, (.texture = crate_tex));
  set_component(entities, crate_entity, box,
                (.width = 24, .height = 14, .depth = 10));
  set_component(entities, crate_entity, entity_kind,
                (.entity_kind = ENTITY_KIND_CRATE));

  return crate_entity;
}

static struct {
  bool mouse_left_button_released;
} client_mouse_input = {0};

static void handle_mouse_input(entity_manager_t *entities,
                               ttb_event_mouse_input_t *input,
                               i32 world_click_x, i32 world_click_y,
                               bool clientside) {
  if (clientside) {
    client_mouse_input.mouse_left_button_released =
        input->buttons[TTB_MOUSE_BUTTON_LEFT] == TTB_KEY_STATE_RELEASED;
  }

  if (!clientside &&
      input->buttons[TTB_MOUSE_BUTTON_RIGHT] == TTB_KEY_STATE_RELEASED) {
    entity_t crate = crate_spawn(entities, world_click_x, world_click_y);
    send_entity_create_to_client(entities, crate);
  }
}

static bool system_events(entity_manager_t *entities) {
  memset(&client_mouse_input, 0, sizeof(client_mouse_input));

  ttb_events_poll(entities->cengine.backend);

  ttb_event_t event;
  while (ttb_event_pop(entities->cengine.backend, &event)) {
    switch (event.event_id) {
    case TTB_EVENT_WINDOW_CLOSED: {
      return false;
    } break;
    case TTB_EVENT_MOUSE_INPUT: {
      ttb_event_mouse_input_t input = event.event_data.evt_mouse_input;
      tt_backend_t *backend = entities->cengine.backend;
      i32 x =
          ttb_screen_to_cam_x(backend, ttb_mouse_x(backend), &entities->camera);
      i32 y =
          ttb_screen_to_cam_y(backend, ttb_mouse_y(backend), &entities->camera);
      handle_mouse_input(entities, &input, x, y, true);

      // Send to server so it can be handled on server asw
      tt_packet_t mouse_input_packet = {0};
      mygame_packet_mouse_input_t mouse_payload = {
          .mouse_input = input,
          .world_click_x = x,
          .world_click_y = y,
      };
      ttec_packet_init(&entities->cengine, &mouse_input_packet,
                       MYGAME_PACKET_MOUSE_INPUT, &mouse_payload);

      tt_packet_send_server(&entities->cengine, mouse_input_packet, entities);
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

static ui_result_e ui_render(entity_manager_t *entities,
                             ttui_layout_t *layout) {
  ui_result_e ui_res = UI_RESULT_PASS;

  if (entities->menu_open) {
    if (ttui_button(entities->cengine.backend, layout,
                    ttui_text({.text = "Hello",
                               .text_size = 10,
                               .text_color = TT_COLOR_WHITE}),
                    0, 0, 100, 30,
                    client_mouse_input.mouse_left_button_released)) {
      ui_res = UI_RESULT_CONSUME_CLICK;
    }
    if (ttui_button(entities->cengine.backend, layout,
                    ttui_text({.text = "World",
                               .text_size = 10,
                               .text_color = TT_COLOR_WHITE}),
                    0, 0, 100, 30,
                    client_mouse_input.mouse_left_button_released)) {
      ui_res = UI_RESULT_CONSUME_CLICK;
    }
    ttui_label(
        entities->cengine.backend, layout,
        ttui_text(
            {.text = "Label", .text_size = 10, .text_color = TT_COLOR_WHITE}),
        0, 0, 100, 30, false);
    if (ttui_button(entities->cengine.backend, layout,
                    ttui_text({.text = "Hello",
                               .text_size = 10,
                               .text_color = TT_COLOR_WHITE}),
                    0, 0, 100, 30,
                    client_mouse_input.mouse_left_button_released)) {
      ui_res = UI_RESULT_CONSUME_CLICK;
    }
  }

  ttui_label(
      entities->cengine.backend, layout,
      ttui_text({
          .text = dyn_string_makef(
                      &entities->temp_bump.alloc, "Player x: %.1f, y: %.1f",
                      entities->c_position[entities->client_player_handle].x,
                      entities->c_position[entities->client_player_handle].y)
                      .string,
          .text_size = 20,
          .text_color = TT_COLOR_BLACK,
      }),
      0, 0, 100, 30, false);

  return ui_res;
}

static ui_result_e system_ui(entity_manager_t *entities) {
  static ttui_layout_t layout = {
      .kind = TTUI_LAYOUT_VERTICAL,
      .padding = 32,
      .width = 400,
      .height = 400,
      .ui_scale = 2.4,
      .debug = false,
  };
  layout.x = (ttb_screen_width(entities->cengine.backend) - 400) / 2;
  layout.y = (ttb_screen_height(entities->cengine.backend) - 400) / 2;

  ui_result_e res = UI_RESULT_PASS;
  // First ui pass, calculate positions and width
  layout.render = false;

  ui_render(entities, &layout);

  ttui_layout_finish(entities->cengine.backend, &layout);

  // Second ui pass, render elements and calculated positions and widths
  layout.render = true;

  res = ui_render(entities, &layout);

  ttui_layout_finish(entities->cengine.backend, &layout);

  return res;
}

static void sync_entities(entity_manager_t *entities,
                          mygame_packet_sync_entities_t *se_packet) {
  if (se_packet->entities_amount > MAX_ENTITIES_AMOUNT) {
    log_error("Attempted syncing too many entities");
    return;
  }

#define COMP_COPY(ty, name, ...)                                               \
  memcpy(entities->name, se_packet->name,                                      \
         sizeof(ty) * se_packet->entities_amount);                             \
  for (usz i = 0; i < se_packet->entities_amount; i++) {                       \
    if (entities->name[i].present)                                             \
      log_info("Syncing component " #name " for entity %zu", i);               \
    else                                                                       \
      log_info("Component " #name " not present on entity %zu", i);            \
  }
  _COMPONENTS_ITER(COMP_COPY)
#undef COMP_COPY

  for (usz i = 0; i < se_packet->entities_amount; i++) {
    if (entities->c_entity_kind[i].present) {
      entity_kind_t entity_kind = entities->c_entity_kind[i].entity_kind;
      if (entity_kind == ENTITY_KIND_PLAYER) {
        set_component(entities, i, texture, (.texture = player_tex));
      } else if (entity_kind == ENTITY_KIND_CRATE) {
        set_component(entities, i, texture, (.texture = crate_tex));
      }
    }
  }

  entities->entities_amount = se_packet->entities_amount;
  log_info("Synced components for %zu entities", se_packet->entities_amount);
}

static void player_init(entity_manager_t *entities, entity_t player,
                        color_t player_color, u64 client_id,
                        bool has_client_id) {
  set_component(entities, player, position, (.x = 0, .y = 0));
  set_component(entities, player, box, (.width = 8, .height = 12, .depth = 4));
  set_component(entities, player, texture, (.texture = player_tex));
  set_component(entities, player, color, (.color = player_color));
  set_component(entities, player, entity_kind,
                (.entity_kind = ENTITY_KIND_PLAYER));
  if (has_client_id) {
    set_component(entities, player, client_id, (.client_id = client_id));
  }
}

static bool system_client_packets(entity_manager_t *entities) {
  // pthread_mutex_lock(&entities->cengine.client_connection.packet_queue_mutex);
  // tt_packet_queue_log(&entities->cengine.client_connection.packet_data_queue);
  // pthread_mutex_unlock(&entities->cengine.client_connection.packet_queue_mutex);
  tt_packet_t packet;
  while (ttec_packet_pop(&entities->cengine, &packet, NULL, entities)) {
    switch ((mygame_packet_id_e)packet.packet_id) {
    case MYGAME_PACKET_PING: {
    } break;
    case MYGAME_PACKET_HANDSHAKE_REQ: {
    } break;
    case MYGAME_PACKET_HANDSHAKE_ACCEPT: {
      log_debug("Received client accept");
      pthread_rwlock_wrlock(
          &entities->cengine.client_connection.connection_rwlock);
      entities->cengine.client_connection.client_id =
          ((mygame_packet_handshake_accept_t *)packet.payload)->client_id;
      pthread_rwlock_unlock(
          &entities->cengine.client_connection.connection_rwlock);
      log_info("client accepted");

      // Reset entity manager
      entities->entities_amount = 0;
      entities->client_player_handle = 0;
      entities->has_player = false;
      log_info("Reset entity manager");

      pending_len = 0;
      next_seq = 0;
      prev_cmd = (mygame_packet_movement_input_t){0};
      memset(remote_keys, 0, sizeof(remote_keys));
    } break;
    case MYGAME_PACKET_HANDSHAKE_REJECT: {
      log_info("client rejected");
    } break;
    case MYGAME_PACKET_PLAYER_STATE: {
      const mygame_packet_player_state_t *st = packet.payload;
      entity_t player;
      if (!player_by_client_id(entities, st->client_id, &player))
        break;

      entities->c_position[player].x = st->x;
      entities->c_position[player].y = st->y;

      if (entities->has_player && player == entities->client_player_handle) {
        // Reconcile: drop acked cmds, replay the rest on top of server state
        pending_ack(st->input.seq);
        for (usz i = 0; i < pending_len; i++)
          apply_move_cmd(entities, player, &pending[i], TICK_DT);
      } else {
        remote_keys[player] = st->input;
      }
    } break;

    case MYGAME_PACKET_MOVEMENT_INPUT: {
      log_error("Received server-bound packet on client");
    } break;
    case MYGAME_PACKET_SYNC_ENTITIES: {
      log_info(
          "Synced entities: cur entities: %zu, payload entities: %zu",
          entities->entities_amount,
          ((mygame_packet_sync_entities_t *)packet.payload)->entities_amount);
      sync_entities(entities, packet.payload);
    } break;
    case MYGAME_PACKET_CREATE_PLAYER: {
      mygame_packet_create_player_t *payload = packet.payload;
      pthread_rwlock_rdlock(
          &entities->cengine.client_connection.connection_rwlock);
      u64 client_id = entities->cengine.client_connection.client_id;
      pthread_rwlock_unlock(
          &entities->cengine.client_connection.connection_rwlock);

      // Latest entity is player, otherwise its out of sync
      entity_t player_entity = tt_entity_make(entities);
      if (player_entity != payload->player_id) {
        log_error("Entities on server and client are out of sync: "
                  "client-player-id: "
                  "%zu, server-player-id: %zu, client-entities-amount: %zu",
                  player_entity, payload->player_id, entities->entities_amount);
        break;
      }
      player_init(entities, player_entity, payload->player_color,
                  payload->client_id, true);
      set_component(entities, player_entity, texture, (.texture = player_tex));

      if (client_id == payload->client_id) {
        entities->client_player_handle = player_entity;
        entities->has_player = true;
      }

    } break;
    case MYGAME_PACKET_REMOVE_PLAYER: {
      mygame_packet_remove_player_t *payload = packet.payload;

      COMPONENTS_ZERO(entities, payload->player_id);
    } break;
    case MYGAME_PACKET_CREATE_ENTITY: {
      mygame_packet_create_entity_t *payload = packet.payload;
      // Latest entity is the new entity, otherwise its out of sync
      entity_t new_entity = tt_entity_make(entities);
      if (new_entity != payload->entity_id) {
        log_error("Entities on server and client are out of sync: "
                  "client-new-entity-id: "
                  "%zu, server-new-entity-id: %zu, client-entities-amount: %zu",
                  new_entity, payload->entity_id, entities->entities_amount);
        break;
      }

#define COMP_COPY(ty, name, e_fn, d_fn, ...)                                   \
  entities->name[new_entity] = payload->name;
      _COMPONENTS_ITER(COMP_COPY)
#undef COMP_COPY

      component_entity_kind_t entity_kind = entities->c_entity_kind[new_entity];
      if (entity_kind.present && entity_kind.entity_kind == ENTITY_KIND_CRATE) {
        set_component(entities, new_entity, texture, (.texture = crate_tex));
      }

      log_info("Created new entity");

    } break;
    }
  }

  return true;
}

static bool system_server_packets(entity_manager_t *entities) {
  tt_packet_context_t packet_ctx;
  tt_packet_t packet;
  while (ttes_packet_pop(&entities->sengine, &packet, &packet_ctx, entities)) {
    switch ((mygame_packet_id_e)packet.packet_id) {
    case MYGAME_PACKET_PING: {
    } break;
    case MYGAME_PACKET_HANDSHAKE_REQ: {
      mygame_packet_handshake_req_t *payload = packet.payload;
      log_info("Received handshake from client");
      if (payload->protocol_version != PROTOCOL_VERSION) {
        log_warn(
            "Client with invalid protocol version %zu attempted to connect",
            payload->protocol_version);

        tt_packet_t reject_packet = {0};
        mygame_packet_handshake_reject_t reject_payload = {.reject_reason = 0};
        ttes_packet_init(&entities->sengine, &reject_packet,
                         MYGAME_PACKET_HANDSHAKE_REJECT, &reject_payload);

        tt_packet_send_client(&entities->sengine, packet_ctx.client_id,
                              reject_packet, entities);

        // TODO: Remove connected client and poll client
      } else {
        log_info("Accepted Client %s", payload->player_name);

        tt_packet_t accept_packet = {0};

        mygame_packet_handshake_accept_t accept_payload = {
            .client_id = packet_ctx.client_id,
        };
        ttes_packet_init(&entities->sengine, &accept_packet,
                         MYGAME_PACKET_HANDSHAKE_ACCEPT, &accept_payload);

        tt_packet_send_client(&entities->sengine, packet_ctx.client_id,
                              accept_packet, entities);

        tt_packet_t sync_packet = {0};
#define COMP_COPY(ty, name, ...)                                               \
  sync_entities_payload.name =                                                 \
      heap_alloc(sizeof(ty) * entities->entities_amount);                      \
  memcpy(sync_entities_payload.name, entities->name,                           \
         sizeof(ty) * entities->entities_amount);

        mygame_packet_sync_entities_t sync_entities_payload = {
            .entities_amount = entities->entities_amount,
        };
        _COMPONENTS_ITER(COMP_COPY)
#undef COMP_COPY

        ttes_packet_init(&entities->sengine, &sync_packet,
                         MYGAME_PACKET_SYNC_ENTITIES, &sync_entities_payload);

        tt_packet_send_client(&entities->sengine, packet_ctx.client_id,
                              sync_packet, entities);

        // Create player
        entity_t player = tt_entity_make(entities);
        player_init(entities, player, payload->player_color,
                    packet_ctx.client_id, true);

        tt_packet_t create_player = {0};
        mygame_packet_create_player_t create_player_payload = {
            .client_id = packet_ctx.client_id,
            .player_id = player,
            .player_name = payload->player_name,
            .player_color = payload->player_color,
        };

        ttes_packet_init(&entities->sengine, &create_player,
                         MYGAME_PACKET_CREATE_PLAYER, &create_player_payload);

        tt_packet_send_all_clients(&entities->sengine, create_player, entities);
      }
    } break;
    case MYGAME_PACKET_MOUSE_INPUT: {
      mygame_packet_mouse_input_t *mouse_input = packet.payload;
      handle_mouse_input(entities, &mouse_input->mouse_input,
                         mouse_input->world_click_x, mouse_input->world_click_y,
                         false);
    } break;
    case MYGAME_PACKET_MOVEMENT_INPUT: {
      entity_t player;
      if (!player_by_client_id(entities, packet_ctx.client_id, &player))
        break;
      const mygame_packet_movement_input_t *in = packet.payload;
      if (in->seq <= entities->server_last_seq[player])
        break; // stale or duplicate

      apply_move_cmd(entities, player, in, TICK_DT);
      entities->server_last_seq[player] = in->seq;
      entities->server_last_input[player] = *in;
      entities->server_last_cmd_tick[player] = entities->server_tick_count;
    } break;
    case MYGAME_PACKET_CREATE_ENTITY:
    case MYGAME_PACKET_HANDSHAKE_ACCEPT:
    case MYGAME_PACKET_HANDSHAKE_REJECT:
    case MYGAME_PACKET_SYNC_ENTITIES: {
      log_error("Received client packet with id: %zu on server",
                packet.packet_id);
    } break;
    }
  }

  return true;
}

static void packets_add(tt_packet_info_array_t *packet_infos) {
#define PACKET_NAME(name, ...) [name] = #name,
  char *packet_names[] = {PACKETS_ITER(PACKET_NAME)};
#undef PACKET_NAME

  for (usz i = 0; i < PACKETS_SIZE; i++) {
    tt_packet_info_t packet_info = mygame_packet_infos[i];
    packet_info.packet_name = packet_names[i];
    packet_info.packet_id = i;
    tt_packet_info_add(packet_infos, packet_info);
  }

  tt_packet_info_lock(packet_infos);
}

static void server_broadcast_player_states(entity_manager_t *entities) {
  for (entity_t e = 0; e < entities->entities_amount; e++) {
    if (!entities->c_entity_kind[e].present ||
        entities->c_entity_kind[e].entity_kind != ENTITY_KIND_PLAYER ||
        !entities->c_client_id[e].present || !entities->c_position[e].present)
      continue;

    mygame_packet_player_state_t st = {
        .client_id = entities->c_client_id[e].client_id,
        .input = entities->server_last_input[e],
        .x = entities->c_position[e].x,
        .y = entities->c_position[e].y,
    };
    st.input.seq = entities->server_last_seq[e];

    // Player went quiet (disconnect/lag): stop clients extrapolating them
    if (entities->server_tick_count - entities->server_last_cmd_tick[e] > 15) {
      st.input.forward = st.input.backward = false;
      st.input.left = st.input.right = false;
    }

    tt_packet_t packet = {0};
    ttes_packet_init(&entities->sengine, &packet, MYGAME_PACKET_PLAYER_STATE,
                     &st);
    tt_packet_send_all_clients(&entities->sengine, packet, entities);
  }
}

static f64 now_seconds(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (f64)ts.tv_sec + (f64)ts.tv_nsec * 1e-9;
}

static i32 server_run(const char *ip_addr, u32 port) {
  entity_manager_t entity_manager = {0};

  entity_manager_init_server(&entity_manager);

  ttes_init(&entity_manager.sengine, false);

  packets_add(&entity_manager.sengine.server.packet_infos);

  ttes_server_host(&entity_manager.sengine, ip_addr, port);

  log_info("Server hosted at ip: %s with port %u", ip_addr, port);

  bool running = true;
  f64 prev = now_seconds();
  f64 accumulator = 0.0;

  u32 snapshot_ticks = 0;

  while (running) {
    f64 cur = now_seconds();
    accumulator += cur - prev;
    prev = cur;
    if (accumulator > 0.25)
      accumulator = 0.25;

    bool packets_running = system_server_packets(&entity_manager);
    if (running)
      running = packets_running;

    // TICKING
    while (accumulator >= TICK_DT) {
      system_velocity(&entity_manager, TICK_DT);
      entity_manager.server_tick_count++;
      if (++snapshot_ticks >= SNAPSHOT_INTERVAL) {
        snapshot_ticks = 0;
        server_broadcast_player_states(&entity_manager);
      }
      accumulator -= TICK_DT;
    }

    tt_server_t *server = &entity_manager.sengine.server;
    pthread_rwlock_wrlock(&server->server_lock);
    tt_client_desc_t *removed_client = NULL;
    while ((removed_client = deque_pop_back(server->removed_clients)) != NULL) {
      tt_packet_t remove_player_packet = {0};
      entity_t player;
      if (!player_by_client_id(&entity_manager, removed_client->client_id,
                               &player))
        continue;

      mygame_packet_remove_player_t remove_player_payload = {
          .player_id = player,
      };
      ttes_packet_init(&entity_manager.sengine, &remove_player_packet,
                       MYGAME_PACKET_REMOVE_PLAYER, &remove_player_payload);
      tt_packet_send_all_clients(&entity_manager.sengine, remove_player_packet,
                                 &entity_manager);

      COMPONENTS_ZERO(&entity_manager, player);
    }
    pthread_rwlock_unlock(&server->server_lock);

    if (running)
      running = ttes_update(&entity_manager.sengine);
    else
      ttes_update(&entity_manager.sengine);

    tt_wait(1);
  }

  ttes_server_stop(&entity_manager.sengine);

  ttes_deinit(&entity_manager.sengine);

  return 0;
}

static void keybinds_setup(entity_manager_t *entities) {
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

#define EXAMPLE_RES "example/res/"
#define ENGINE_RES "engine/res/"

static void client_setup(entity_manager_t *entity_manager) {
  tt_assets_add_asset_path(&entity_manager->cengine.asset_manager, EXAMPLE_RES);
  tt_assets_add_asset_path(&entity_manager->cengine.asset_manager, ENGINE_RES);
  ttec_init(&entity_manager->cengine);

  ttb_window_settings_t settings = {
      .title = "Engine Example",
      .resizeable = true,
  };
  ttb_open_window(entity_manager->cengine.backend, settings);

  packets_add(&entity_manager->cengine.client_connection.packet_infos);
  ttec_load_assets(&entity_manager->cengine);

  entity_manager_init_client(entity_manager);
  keybinds_setup(entity_manager);

  bump_init(&entity_manager->temp_bump, 128000);

  tt_keymap_load(&entity_manager->keymap, "example/res/keys.toml", true,
                 &HEAP_ALLOCATOR);
  tt_keymap_save(&entity_manager->keymap, "example/res/keys.toml", true);

  tt_translation_manager_init(&entity_manager->translation_manager, "en_us",
                              &HEAP_ALLOCATOR);

  tt_translation_load(&entity_manager->translation_manager, "en_us",
                      EXAMPLE_RES "en_us.toml");

  tt_translation_insert(&entity_manager->translation_manager, "en_us",
                        "text.example", "Some Example Text lol");
  tt_translation_insert(&entity_manager->translation_manager, "en_us",
                        "sussy.baka", "Its 2020 again :p");
  tt_translation_insert(&entity_manager->translation_manager, "en_us",
                        "msg.welcome", "Welcome :3");

  tt_translation_save(&entity_manager->translation_manager, "en_us",
                      EXAMPLE_RES "en_us.toml");

  ttui_set_config_default(&entity_manager->cengine.asset_manager,
                          ENGINE_RES "tex/");
}

static void cam_focus(tt_camera_t *cam, i32 entity_x, i32 entity_y,
                      i32 entity_w, i32 entity_h) {
  cam->target_x = entity_x + entity_w / 2;
  cam->target_y = entity_y + entity_h / 2;
}

static void draw_text(tt_engine_client_t *engine, const char *text, i32 size,
                      i32 x, i32 y) {
  ttb_render_cmd_t render_cmd = {.cmd_id = TTB_RENDER_CMD_TEXT,
                                 .cmd_data.cmd_text = {
                                     .text = text,
                                     .text_color = TT_COLOR_BLACK,
                                     .x = x,
                                     .y = y,
                                     .font_size = size,
                                 }};
  ttb_render(engine->backend, render_cmd);
}

static i32 client_run(color_t color, const char *ip_addr, u32 port) {
  entity_manager_t entity_manager = {0};

  client_setup(&entity_manager);

  tt_asset_manager_t *assets = &entity_manager.cengine.asset_manager;

  player_tex = tt_asset_bind(assets, EXAMPLE_RES "tex/player.png");
  crate_tex = tt_asset_bind(assets, EXAMPLE_RES "tex/crate.png");

  entity_t _player = tt_entity_make(&entity_manager);
  player_init(&entity_manager, _player, color, 0, false);

  entity_manager.client_player_handle = _player;
  entity_manager.has_player = true;

  bool running = true;

  i32 connection_retry_timer = 0;
  bool initial_connection = false;

  while (running) {
    if (!initial_connection && connection_retry_timer <= 0) {
      log_info("Trying to connect to server at %s with port %d", ip_addr, port);
      if (!ttec_connect(&entity_manager.cengine, ip_addr, port)) {
        log_warn("Connection attempt failed, retrying");
        connection_retry_timer = 500;
      } else {
        log_info("Connected to server!");
        initial_connection = true;

        mygame_packet_handshake_req_t payload = {
            .protocol_version = PROTOCOL_VERSION,
            .player_name = "Dev",
            .player_color = color,
        };
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

    static f64 tick_acc = 0.0; // or hoist above the while loop
    f64 dt = ttb_frame_time(entity_manager.cengine.backend);
    tick_acc += dt;
    if (tick_acc > 0.25)
      tick_acc = 0.25;

    while (tick_acc >= TICK_DT) {
      client_tick(&entity_manager);
      tick_acc -= TICK_DT;
    }

    component_pos_t player_pos =
        entity_manager.c_position[entity_manager.client_player_handle];
    if (entity_manager.has_player && player_pos.present) {
      cam_focus(
          &entity_manager.camera, player_pos.x, player_pos.y,
          entity_width(&entity_manager, entity_manager.client_player_handle),
          entity_depth(&entity_manager, entity_manager.client_player_handle));
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
                                      color_make(200, 200, 200, 255)});

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

    draw_text(&entity_manager.cengine,
              !initial_connection ? "Connecting to server"
                                  : "Connected to server!",
              36, 0, 400);

    draw_text(&entity_manager.cengine,
              dyn_string_makef(
                  &entity_manager.temp_bump.alloc, "%s, %d",
                  tt_translations_lookup(&entity_manager.translation_manager,
                                         "msg.welcome"),
                  ttb_fps(entity_manager.cengine.backend))
                  .string,
              46, 100, 300);

    if (initial_connection) {
      i32 text_y = 200;
      tt_client_connection_t *connection =
          &entity_manager.cengine.client_connection;
      pthread_rwlock_rdlock(&connection->connection_rwlock);
      if (connection->server_addr != -1) {
        for (entity_t e = 0; e < entity_manager.entities_amount; e++) {
          if (entity_manager.c_client_id[e].present) {
            draw_text(
                &entity_manager.cengine,
                dyn_string_makef(&entity_manager.temp_bump.alloc, "").string,
                16, 200, text_y += 16);
          }
        }
      }
      pthread_rwlock_unlock(&connection->connection_rwlock);
    }

    ui_result_e ui_res = system_ui(&entity_manager);
    if (ui_res == UI_RESULT_EXIT_GAME) {
      running = false;
    }

    if (running) {
      running = ttec_update(&entity_manager.cengine);
    } else {
      ttec_update(&entity_manager.cengine);
    }

    bump_reset(&entity_manager.temp_bump);

    connection_retry_timer--;
  }

  ttec_client_stop(&entity_manager.cengine);

  ttec_deinit(&entity_manager.cengine);

  bump_free(&entity_manager.temp_bump);

  entity_manager_deinit(&entity_manager);

  return 0;
}

void entity_manager_init_client(entity_manager_t *entity_manager) {
  tt_keymap_init(&entity_manager->keymap, &HEAP_ALLOCATOR);
  tt_translation_manager_init(&entity_manager->translation_manager, "en_us",
                              &HEAP_ALLOCATOR);

  tt_camera_init(&entity_manager->camera, entity_manager->cengine.backend);
  entity_manager->camera.zoom = 4;
}

void entity_manager_init_server(entity_manager_t *entity_manager) {
  bump_init(&entity_manager->server_client_info_bump, 4800);
}

void entity_manager_deinit(entity_manager_t *entity_manager) {
  tt_keymap_deinit(&entity_manager->keymap);
  tt_translation_manager_deinit(&entity_manager->translation_manager);
}
