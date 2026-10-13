#pragma once

#include "../include/tt_packets.h"
#include "../include/tt_shared.h"
#include "lilc/numbers.h"

typedef struct {
  i32 x;
  i32 y;

  bool present;
} component_pos_t;

static void component_pos_encode(const component_pos_t *comp, tt_byte_buf_t *buf,
                                 void *encode_ctx) {
  tt_byte_buf_write(buf, comp->present);
  if (comp->present) {
    tt_encode_i32(comp->x, buf, encode_ctx);
    tt_encode_i32(comp->y, buf, encode_ctx);
  }
}

static void component_pos_decode(component_pos_t *comp, tt_byte_buf_t *buf,
                                 void *encode_ctx) {
  comp->present = tt_byte_buf_read(buf);
  if (comp->present) {
    comp->x = tt_decode_i32(buf, encode_ctx);
    comp->y = tt_decode_i32(buf, encode_ctx);
  }
}

typedef struct {
  i32 width;
  i32 height;
  i32 depth;

  bool present;
} component_box_t;

static void component_box_encode(const component_box_t *comp, tt_byte_buf_t *buf,
                                 void *encode_ctx) {
  tt_byte_buf_write(buf, comp->present);
  if (comp->present) {
    tt_encode_i32(comp->width, buf, encode_ctx);
    tt_encode_i32(comp->height, buf, encode_ctx);
    tt_encode_i32(comp->depth, buf, encode_ctx);
  }
}

static void component_box_decode(component_box_t *comp, tt_byte_buf_t *buf,
                                 void *encode_ctx) {
  comp->present = tt_byte_buf_read(buf);
  if (comp->present) {
    comp->width = tt_decode_i32(buf, encode_ctx);
    comp->height = tt_decode_i32(buf, encode_ctx);
    comp->depth = tt_decode_i32(buf, encode_ctx);
  }
}

typedef struct {
  f32 x_velocity;
  f32 y_velocity;

  bool present;
} component_velocity_t;

// TODO: Float encoding/decoding

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