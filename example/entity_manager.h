#pragma once

#include "../include/tt_shared.h"
#include "../include/tt_engine.h"
#include "../include/tt_camera.h"
#include "../include/tt_keys.h"
#include "../include/tt_translation.h"
#include "components.h"

typedef u64 tt_entity_t;

#define MAX_ENTITIES_AMOUNT 4000

struct tt_entity_manager {
  component_pos_t c_position[MAX_ENTITIES_AMOUNT];
  component_texture_t c_texture[MAX_ENTITIES_AMOUNT];
  component_box_t c_box[MAX_ENTITIES_AMOUNT];
  component_entity_kind_t c_entity_kind[MAX_ENTITIES_AMOUNT];
  component_velocity_t c_velocity[MAX_ENTITIES_AMOUNT];

  tt_entity_t player_handle;

  tt_entity_t cur_entity_id;
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
                   (typeof(*((entities_ptr)->c_##comp)))_internal_set_component_parentheses_macro __VA_ARGS__;)       \
    (entities_ptr)->c_##comp[entity].present = true;                           \
  } while (0)
