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

void tt_byte_buf_deinit(tt_byte_buf_t *byte_buf);

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

typedef void *tt_packet_context_t;

typedef void (*tt_packet_encode_func_t)(const tt_packet_t *, tt_byte_buf_t *,
                                        tt_packet_context_t);

typedef void (*tt_packet_decode_func_t)(tt_packet_t *, tt_byte_buf_t *,
                                        tt_packet_context_t);

// --SERIALIZATION--
// The developer defines all fields of a packet that need to be serialized
// using a macro then we use a type table to look up the way to serialize this
// field. Alternatively, the developer can also provide a custom function for
// serializing the packet.

// EXPERIMENTAL
#define TT_MAX_STRUCT_FIELDS 32
typedef void (*tt_networking_encode_func_t)(const void *, tt_byte_buf_t *,
                                            tt_packet_context_t);
typedef void (*tt_networking_decode_func_t)(void *, tt_byte_buf_t *,
                                            tt_packet_context_t);
typedef struct tt_packet_struct_field {
  const char *name;
  i32 size;
  i32 offset;
  tt_networking_encode_func_t encode_func;
  tt_networking_decode_func_t decode_func;
} tt_packet_struct_field_t;
typedef struct tt_struct_info {
  tt_packet_struct_field_t fields[TT_MAX_STRUCT_FIELDS];
  usz fields_count;
} tt_struct_info_tt;

typedef struct tt_packet_info {
  tt_packet_id_t packet_id;
  tt_packet_encode_func_t encode_func;
  tt_packet_decode_func_t decode_func;
  tt_packet_dir_e packet_dir;
  usz payload_size;
  tt_struct_info_tt struct_info;
} tt_packet_info_t;

typedef struct tt_packet {
  tt_packet_handle_t packet_handle;
  tt_packet_id_t packet_id;
  void *payload;
} tt_packet_t;

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
