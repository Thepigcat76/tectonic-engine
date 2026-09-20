#pragma once

#include "../include/tt_shared.h"
#include "lilc/numbers.h"

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