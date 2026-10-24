#pragma once

#include "lilc/alloc.h"
#include "lilc/deque.h"
#include "lilc/dynstr.h"
#include "lilc/numbers.h"
#include "lilsockets.h"
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

typedef void (*tt_packet_encode_func_t)(const tt_packet_t *, tt_byte_buf_t *,
                                        void *);

typedef void (*tt_packet_decode_func_t)(tt_packet_t *, tt_byte_buf_t *, void *);

typedef struct tt_packet_info {
  tt_packet_id_t packet_id;
  tt_packet_encode_func_t encode_func;
  tt_packet_decode_func_t decode_func;
  tt_packet_dir_e packet_dir;
  usz payload_size;
} tt_packet_info_t;

typedef struct tt_packet {
  tt_packet_id_t packet_id;
  void *payload;
} tt_packet_t;

typedef struct tt_packet_context {
  addr_t sender;
  u64 client_id;
} tt_packet_context_t;

struct tt_packet_queue_entry {
  tt_byte_buf_t packet_bytes;
  tt_packet_context_t packet_ctx;
};

typedef deque_t(struct tt_packet_queue_entry) tt_packet_queue_t;

void tt_packet_queue_log(const tt_packet_queue_t *packet_queue);

// TODO: Check packet dir

void tt_encode_packet(const tt_packet_t *packet, tt_packet_info_t info,
                      tt_byte_buf_t *buf, void *encode_ctx);

// Preceeding the packet bytes, there are 4 bytes containing the packet id.
// This id is required for initializing the packet parameter
void tt_decode_packet(tt_packet_t *packet, tt_packet_info_t info,
                      tt_byte_buf_t *buf, void *decode_ctx);

void tt_encode_string(const char *string, tt_byte_buf_t *buf, void *encode_ctx);

void tt_decode_string(char **string, allocator_t *alloc, tt_byte_buf_t *buf,
                      void *decode_ctx);

void tt_encode_dynstr(const dyn_string_t *str, tt_byte_buf_t *buf,
                      void *encode_ctx);

void tt_decode_dynstr(dyn_string_t *str, tt_byte_buf_t *buf, void *decode_ctx);

void tt_encode_i16(i16 val, tt_byte_buf_t *buf, void *ctx);

i16 tt_decode_i16(tt_byte_buf_t *buf, void *ctx);

void tt_encode_i32(i32 val, tt_byte_buf_t *buf, void *ctx);

i32 tt_decode_i32(tt_byte_buf_t *buf, void *ctx);

void tt_encode_i64(i64 val, tt_byte_buf_t *buf, void *ctx);

i64 tt_decode_i64(tt_byte_buf_t *buf, void *ctx);
