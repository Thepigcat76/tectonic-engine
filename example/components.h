#pragma once

#include "../engine/include/tt_packets.h"
#include "../engine/include/tt_shared.h"
#include "lilc/numbers.h"
#include "lilc/log.h"

typedef struct {
  f32 x;
  f32 y;

  bool present;
} component_pos_t;

static void component_pos_encode(const component_pos_t *comp,
                                 tt_byte_buf_t *buf, void *encode_ctx) {
  log_debug("Encoding comp pos, present: %s", comp->present ? "true" : "false");
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

static void component_box_encode(const component_box_t *comp,
                                 tt_byte_buf_t *buf, void *encode_ctx) {
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

static void component_velocity_encode(const component_velocity_t *comp,
                                      tt_byte_buf_t *buf, void *encode_ctx) {
  tt_byte_buf_write(buf, comp->present);
  if (comp->present) {
    tt_encode_i32(comp->x_velocity, buf, encode_ctx);
    tt_encode_i32(comp->y_velocity, buf, encode_ctx);
  }
}

static void component_velocity_decode(component_velocity_t *comp,
                                      tt_byte_buf_t *buf, void *encode_ctx) {
  comp->present = tt_byte_buf_read(buf);
  if (comp->present) {
    comp->x_velocity = tt_decode_i32(buf, encode_ctx);
    comp->y_velocity = tt_decode_i32(buf, encode_ctx);
  }
}

// TODO: Float encoding/decoding

typedef struct {
  tt_asset_id_t texture;

  bool present;
} component_texture_t;

static void component_texture_encode(const component_texture_t *comp,
                                     tt_byte_buf_t *buf, void *encode_ctx) {
  tt_byte_buf_write(buf, comp->present);
}

static void component_texture_decode(component_texture_t *comp,
                                     tt_byte_buf_t *buf, void *encode_ctx) {
  comp->present = tt_byte_buf_read(buf);
  comp->texture = 0;
}

typedef enum {
  ENTITY_KIND_PLAYER,
  ENTITY_KIND_CRATE,
} entity_kind_t;

typedef struct {
  entity_kind_t entity_kind;
  bool present;
} component_entity_kind_t;

static void component_entity_kind_encode(const component_entity_kind_t *comp,
                                         tt_byte_buf_t *buf, void *encode_ctx) {
  tt_byte_buf_write(buf, comp->present);
  if (comp->present) {
    tt_encode_i32(comp->entity_kind, buf, encode_ctx);
  }
}

static void component_entity_kind_decode(component_entity_kind_t *comp,
                                         tt_byte_buf_t *buf, void *encode_ctx) {
  comp->present = tt_byte_buf_read(buf);
  if (comp->present) {
    comp->entity_kind = tt_decode_i32(buf, encode_ctx);
  }
}

typedef struct {
  color_t color;
  bool present;
} component_color_t;

static void component_color_encode(const component_color_t *comp,
                                   tt_byte_buf_t *buf, void *encode_ctx) {
  tt_byte_buf_write(buf, comp->present);
  if (comp->present) {
    tt_encode_i32(comp->color, buf, encode_ctx);
  }
}

static void component_color_decode(component_color_t *comp, tt_byte_buf_t *buf,
                                   void *encode_ctx) {
  comp->present = tt_byte_buf_read(buf);
  if (comp->present) {
    comp->color = tt_decode_i32(buf, encode_ctx);
  }
}

typedef struct {
  u64 client_id;
  bool present;
} component_client_id_t;

static void component_client_id_encode(const component_client_id_t *comp,
                                   tt_byte_buf_t *buf, void *encode_ctx) {
  tt_byte_buf_write(buf, comp->present);
  if (comp->present) {
    tt_encode_i64(comp->client_id, buf, encode_ctx);
  }
}

static void component_client_id_decode(component_client_id_t *comp, tt_byte_buf_t *buf,
                                   void *decode_ctx) {
  comp->present = tt_byte_buf_read(buf);
  if (comp->present) {
    comp->client_id = tt_decode_i64(buf, decode_ctx);
  }
}

#define _COMPONENTS_ITER(_F, ...)                                              \
  _F(component_pos_t, c_position, component_pos_encode, component_pos_decode,  \
     __VA_ARGS__)                                                              \
  _F(component_color_t, c_color, component_color_encode,                       \
     component_color_decode, __VA_ARGS__)                                      \
  _F(component_texture_t, c_texture, component_texture_encode,                 \
     component_texture_decode, __VA_ARGS__)                                    \
  _F(component_box_t, c_box, component_box_encode, component_box_decode,       \
     __VA_ARGS__)                                                              \
  _F(component_velocity_t, c_velocity, component_velocity_encode,              \
     component_velocity_decode, __VA_ARGS__)                                   \
  _F(component_entity_kind_t, c_entity_kind, component_entity_kind_encode,     \
     component_entity_kind_decode, __VA_ARGS__)                                \
  _F(component_client_id_t, c_client_id, component_client_id_encode,           \
     component_client_id_decode, __VA_ARGS__)
