#pragma once

#include "../engine/include/tt_camera.h"
#include "../engine/include/tt_engine.h"
#include "../engine/include/tt_keys.h"
#include "../engine/include/tt_shared.h"
#include "../engine/include/tt_translation.h"
#include "components.h"
#include "shared.h"
#include "packets.h"

struct move_input {
  bool forward;
  bool backward;
  bool left;
  bool right;
};

typedef struct {
#define DECL_COMP(ty, name, ...) ty name[MAX_ENTITIES_AMOUNT];
  _COMPONENTS_ITER(DECL_COMP)
#undef DECL_COMP

  entity_t client_player_handle;
  bool has_player;

  bool menu_open;

  usz entities_amount;

  tt_keymap_t keymap;
  tt_translation_manager_t translation_manager;
  tt_camera_t camera;

  struct move_input prev_move_input;

  u32 server_last_seq[MAX_ENTITIES_AMOUNT];
  mygame_packet_movement_input_t server_last_input[MAX_ENTITIES_AMOUNT];
  
  u64 server_last_cmd_tick[MAX_ENTITIES_AMOUNT];
  u64 server_tick_count;

  tt_engine_client_t cengine;
  tt_engine_server_t sengine;

  // For allocating info on the server
  // about the connected clients
  bump_t server_client_info_bump;

  bump_t temp_bump;
} entity_manager_t;

entity_t tt_entity_make(entity_manager_t *entities);

void entity_manager_init_client(entity_manager_t *entities);

void entity_manager_init_server(entity_manager_t *entities);

void entity_manager_deinit(entity_manager_t *entities);

#define _internal_set_component_parentheses_macro(...) {__VA_ARGS__}

#define set_component(entities_ptr, entity, comp, ...)                         \
  do {                                                                         \
    __VA_OPT__((entities_ptr)->c_##comp[entity] =                              \
                   (typeof(*((entities_ptr)->c_##comp)))                       \
                       _internal_set_component_parentheses_macro __VA_ARGS__;) \
    (entities_ptr)->c_##comp[entity].present = true;                           \
  } while (0)

static f32 entity_x(entity_manager_t *entities, entity_t entity) {
  if (entities->c_position[entity].present) {
    return entities->c_position[entity].x;
  }
  return 0;
}

static f32 entity_y(entity_manager_t *entities, entity_t entity) {
  if (entities->c_position[entity].present) {
    return entities->c_position[entity].y;
  }
  return 0;
}

static f32 entity_width(entity_manager_t *entities, entity_t entity) {
  if (entities->c_box[entity].present) {
    return entities->c_box[entity].width;
  }
  return 0;
}

static f32 entity_height(entity_manager_t *entities, entity_t entity) {
  if (entities->c_box[entity].present) {
    return entities->c_box[entity].height;
  }
  return 0;
}

static f32 entity_depth(entity_manager_t *entities, entity_t entity) {
  if (entities->c_box[entity].present) {
    return entities->c_box[entity].depth;
  }
  return 0;
}

static f32 player_x(entity_manager_t *entities) {
  if (entities->c_position[entities->client_player_handle].present) {
    return entities->c_position[entities->client_player_handle].x;
  }
  return 0;
}

static f32 player_y(entity_manager_t *entities) {
  if (entities->c_position[entities->client_player_handle].present) {
    return entities->c_position[entities->client_player_handle].y;
  }
  return 0;
}

static f32 entity_vel_x(entity_manager_t *entities, entity_t entity) {
  if (entities->c_velocity[entity].present) {
    return entities->c_velocity[entity].x_velocity;
  }
  return 0;
}

static f32 entity_vel_y(entity_manager_t *entities, entity_t entity) {
  if (entities->c_velocity[entity].present) {
    return entities->c_velocity[entity].y_velocity;
  }
  return 0;
}
