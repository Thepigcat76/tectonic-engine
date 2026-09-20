#pragma once

#include "lilc/alloc.h"
#include "lilc/numbers.h"
#include "tt_shared.h"

// --BYTEBUF--

typedef struct tt_byte_buf {
  u8 *bytes;
  usz write_idx;
  usz read_idx;
} tt_byte_buf_t;

void tt_byte_buf_init(tt_byte_buf_t *byte_buf, allocator_t *alloc,
                      usz capacity);

u8 tt_byte_buf_read(tt_byte_buf_t *byte_buf);

void tt_byte_buf_write(tt_byte_buf_t *byte_buf, u8 val);

usz tt_byte_buf_len(tt_byte_buf_t *byte_buf);

// --PACKETS--

typedef enum tt_packet_dir {
  TT_PACKET_DIR_TWO_WAY,
  TT_PACKET_DIR_CLIENT,
  TT_PACKET_DIR_SERVER,
} tt_packet_dir_e;

typedef struct tt_packet tt_packet_t;

typedef struct tt_packet_context {
  tt_entity_manager_t *entities;
} tt_packet_context_t;

typedef struct tt_packet_handle_context {
  tt_entity_manager_t *entities;
  tt_entity_t *receivers;
} tt_packet_handle_context_t;

typedef void (*tt_packet_encode_func_t)(const tt_packet_t *, tt_byte_buf_t *,
                                        tt_packet_context_t);

typedef void (*tt_packet_decode_func_t)(tt_packet_t *, tt_byte_buf_t *,
                                        tt_packet_context_t);

typedef void (*tt_packet_handle_func_t)(tt_packet_t *,
                                        tt_packet_handle_context_t);

// --SERIALIZATION--
// The developer defines all fields of a packet that need to be serialized
// using a macro then we use a type table to look up the way to serialize this
// field. Alternatively, the developer can also provide a custom function for
// serializing the packet.

// EXPERIMENTAL
typedef enum tt_field_type {
  TT_FIELD_TY_U8,
  TT_FIELD_TY_U16,
  TT_FIELD_TY_U32,
  TT_FIELD_TY_U64,
  TT_FIELD_TY_I8,
  TT_FIELD_TY_I16,
  TT_FIELD_TY_I32,
  TT_FIELD_TY_I64,
} tt_field_type_e;
#define TT_MAX_STRUCT_FIELDS 32
typedef void (*tt_networking_encode_func_t)(const void *, tt_byte_buf_t *,
                                            tt_packet_context_t);
typedef void (*tt_networking_decode_func_t)(void *, tt_byte_buf_t *,
                                            tt_packet_context_t);
typedef struct tt_packet_struct_field {
  const char *name;
  i32 size;
  i32 offset;
  tt_field_type_e field_ty;
  tt_networking_encode_func_t encode_func;
  tt_networking_decode_func_t decode_func;
} tt_packet_struct_field_t;
typedef struct tt_struct_info {
  tt_packet_struct_field_t fields[TT_MAX_STRUCT_FIELDS];
  usz fields_count;
} tt_struct_info_tt;

typedef struct tt_packet_info {
  tt_packet_encode_func_t encode_func;
  tt_packet_decode_func_t decode_func;
  tt_packet_handle_func_t handle_func;
  tt_packet_dir_e packet_dir;
  usz payload_size;
  tt_struct_info_tt struct_info;
} tt_packet_info_t;

typedef struct tt_packet {
  tt_packet_id_t packet_id;
  void *payload;
} tt_packet_t;

struct serial_type_table_entry {
  tt_networking_encode_func_t encode_func;
  tt_networking_decode_func_t decode_func;
};

#define TTN_ST_TABLE_ENTRY(_encode_func, _decode_func)                         \
  (struct serial_type_table_entry) {                                           \
    .encode_func = (tt_networking_encode_func_t)_encode_func,                  \
    .decode_func = (tt_networking_decode_func_t)_decode_func                   \
  }

void tt_encode_i8(const i16 *val, tt_byte_buf_t *buf,
                   tt_packet_context_t ctx);

void tt_decode_i8(i16 *val, tt_byte_buf_t *buf, tt_packet_context_t ctx);

void tt_encode_i16(const i16 *val, tt_byte_buf_t *buf,
                   tt_packet_context_t ctx);

void tt_decode_i16(i16 *val, tt_byte_buf_t *buf, tt_packet_context_t ctx);

void tt_encode_i32(const i32 *val, tt_byte_buf_t *buf,
                   tt_packet_context_t ctx);

void tt_decode_i32(i32 *val, tt_byte_buf_t *buf, tt_packet_context_t ctx);

void tt_encode_i64(const i64 *val, tt_byte_buf_t *buf,
                   tt_packet_context_t ctx);

void tt_decode_i64(i64 *val, tt_byte_buf_t *buf, tt_packet_context_t ctx);

#define _TTN_LIB_ST_TABLE(ty, ...)                                             \
  _Generic(*((ty *)NULL),                                                      \
      u8: TTN_ST_TABLE_ENTRY(tt_encode_i32, tt_decode_i32),                        \
      i8: TTN_ST_TABLE_ENTRY(tt_encode_i32, tt_decode_i32),                        \
      u16: TTN_ST_TABLE_ENTRY(tt_encode_i32, tt_decode_i32),                       \
      i16: TTN_ST_TABLE_ENTRY(tt_encode_i32, tt_decode_i32),                       \
      u32: TTN_ST_TABLE_ENTRY(tt_encode_i32, tt_decode_i32),                       \
      i32: TTN_ST_TABLE_ENTRY(tt_encode_i32, tt_decode_i32),                       \
      u64: TTN_ST_TABLE_ENTRY(tt_encode_i64, tt_decode_i64),                       \
      i64: TTN_ST_TABLE_ENTRY(tt_encode_i64, tt_decode_i64) __VA_OPT__(, )         \
          __VA_OPT__(default : (__VA_ARGS__)))
