#pragma once

#include "../include/tt_camera.h"
#include "../include/tt_engine.h"
#include "../include/tt_keys.h"
#include "../include/tt_shared.h"
#include "../include/tt_translation.h"
#include "components.h"

typedef u64 tt_entity_t;

#define MAX_ENTITIES_AMOUNT 4000

struct tt_entity_manager {
#define _COMPONENTS_ITER(_F, ...)                                                        \
  _F(component_pos_t, c_position, component_pos_encode, component_pos_decode, __VA_ARGS__)                                              \
  _F(component_texture_t, c_texture, component_texture_encode, component_texture_decode, __VA_ARGS__)                                           \
  _F(component_box_t, c_box, component_box_encode, component_box_decode, __VA_ARGS__)                                                   \
  _F(component_velocity_t, c_velocity, component_velocity_encode, component_velocity_decode, __VA_ARGS__)                                         \
  _F(component_entity_kind_t, c_entity_kind, component_entity_kind_encode, component_entity_kind_decode, __VA_ARGS__)

#define DECL_COMP(ty, name, ...) ty name[MAX_ENTITIES_AMOUNT];
  _COMPONENTS_ITER(DECL_COMP)
#undef DECL_COMP

  tt_entity_t client_player_handle;
  bool has_player;

  bool menu_open;

  usz entities_amount;

  tt_keymap_t keymap;
  tt_translation_manager_t translation_manager;
  tt_camera_t camera;

  tt_engine_client_t cengine;
  tt_engine_server_t sengine;

  // For allocating info on the server
  // about the connected clients
  bump_t server_client_info_bump;
  allocator_t server_client_info_alloc;
};

tt_entity_t tt_entity_make(tt_entity_manager_t *entities);

void entity_manager_init_client(tt_entity_manager_t *entities);

void entity_manager_init_server(tt_entity_manager_t *entities);

void entity_manager_deinit(tt_entity_manager_t *entities);

#define _internal_set_component_parentheses_macro(...) {__VA_ARGS__}

#define set_component(entities_ptr, entity, comp, ...)                         \
  do {                                                                         \
    __VA_OPT__((entities_ptr)->c_##comp[entity] =                              \
                   (typeof(*((entities_ptr)->c_##comp)))                       \
                       _internal_set_component_parentheses_macro __VA_ARGS__;) \
    (entities_ptr)->c_##comp[entity].present = true;                           \
  } while (0)

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
  if (entities->c_position[entities->client_player_handle].present) {
    return entities->c_position[entities->client_player_handle].x;
  }
  return 0;
}

static f32 player_y(tt_entity_manager_t *entities) {
  if (entities->c_position[entities->client_player_handle].present) {
    return entities->c_position[entities->client_player_handle].y;
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
