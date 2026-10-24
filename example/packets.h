#pragma once

#include "lilc/numbers.h"

#include "../include/tt_packets.h"

#include "entity_manager.h"
#include <lilc/alloc.h>

#define PROTOCOL_VERSION 0x0000000000000001

typedef enum {
  MYGAME_PACKET_PING,
  MYGAME_PACKET_HANDSHAKE_REQ,
  MYGAME_PACKET_HANDSHAKE_ACCEPT,
  MYGAME_PACKET_HANDSHAKE_REJECT,
  MYGAME_PACKET_SYNC_ENTITIES,
  MYGAME_PACKET_CREATE_PLAYER,
} mygame_packet_id_e;

typedef struct {
  u64 time;
} mygame_packet_ping_t;

typedef struct {
  u64 protocol_version;
  char *player_name;
} mygame_packet_handshake_req_t;

typedef struct {
  u64 reject_reason;
} mygame_packet_handshake_reject_t;

typedef struct {
  u64 client_id;
} mygame_packet_handshake_accept_t;

typedef struct {
  usz entities_amount;

#define DECL_COMP(ty, name, ...) ty *name;
  _COMPONENTS_ITER(DECL_COMP)
#undef DECL_COMP
} mygame_packet_sync_entities_t;

typedef struct {
  u64 client_id;
  tt_entity_t player_id;
  char *player_name;
} mygame_packet_create_player_t;

static void mygame_packet_ping_encode(const tt_packet_t *packet,
                                      tt_byte_buf_t *buf, void *encode_ctx) {
  mygame_packet_ping_t *ping_packet = packet->payload;
  tt_encode_i64(ping_packet->time, buf, encode_ctx);
}
static void mygame_packet_ping_decode(tt_packet_t *packet, tt_byte_buf_t *buf,
                                      void *decode_ctx) {
  mygame_packet_ping_t *ping_packet = packet->payload;
  ping_packet->time = tt_decode_i64(buf, decode_ctx);
}

static void mygame_packet_handshake_req_encode(const tt_packet_t *packet,
                                               tt_byte_buf_t *buf,
                                               void *encode_ctx) {
  mygame_packet_handshake_req_t *payload = packet->payload;
  tt_encode_i64(payload->protocol_version, buf, encode_ctx);
  tt_encode_string(payload->player_name, buf, encode_ctx);
}

static void mygame_packet_handshake_req_decode(tt_packet_t *packet,
                                               tt_byte_buf_t *buf,
                                               void *decode_ctx) {
  tt_entity_manager_t *entities = decode_ctx;
  mygame_packet_handshake_req_t *payload = packet->payload;
  payload->protocol_version = tt_decode_i64(buf, decode_ctx);
  tt_decode_string(&payload->player_name, &entities->server_client_info_alloc,
                   buf, decode_ctx);
}

static void mygame_packet_handshake_accept_encode(const tt_packet_t *packet,
                                                  tt_byte_buf_t *buf,
                                                  void *encode_ctx) {
  mygame_packet_handshake_accept_t *payload = packet->payload;
  tt_encode_i64(payload->client_id, buf, encode_ctx);
}

static void mygame_packet_handshake_accept_decode(tt_packet_t *packet,
                                                  tt_byte_buf_t *buf,
                                                  void *decode_ctx) {
  mygame_packet_handshake_accept_t *payload = packet->payload;
  payload->client_id = tt_decode_i64(buf, decode_ctx);
}

static void mygame_packet_handshake_reject_encode(const tt_packet_t *packet,
                                                  tt_byte_buf_t *buf,
                                                  void *encode_ctx) {
  mygame_packet_handshake_reject_t *payload = packet->payload;
  tt_encode_i64(payload->reject_reason, buf, encode_ctx);
}

static void mygame_packet_handshake_reject_decode(tt_packet_t *packet,
                                                  tt_byte_buf_t *buf,
                                                  void *decode_ctx) {
  mygame_packet_handshake_reject_t *payload = packet->payload;
  payload->reject_reason = tt_decode_i64(buf, decode_ctx);
}

static void mygame_packet_sync_entities_encode(const tt_packet_t *packet,
                                               tt_byte_buf_t *buf,
                                               void *encode_ctx) {
  mygame_packet_sync_entities_t *payload = packet->payload;
  tt_encode_i64(payload->entities_amount, buf, encode_ctx);

#define COMP_ENCODE(ty, name, encode_fn, ...)                                  \
  for (usz i = 0; i < payload->entities_amount; i++)                           \
    encode_fn(&payload->name[i], buf, encode_ctx);
  _COMPONENTS_ITER(COMP_ENCODE)
#undef COMP_ENCODE
}

// TODO: Encode/Decode all packets not just one

static void mygame_packet_sync_entities_decode(tt_packet_t *packet,
                                               tt_byte_buf_t *buf,
                                               void *decode_ctx) {
  mygame_packet_sync_entities_t *payload = packet->payload;
  payload->entities_amount = tt_decode_i64(buf, decode_ctx);

#define COMP_DECODE(ty, name, decode_fn, ...)                                  \
  payload->name = heap_alloc(sizeof(ty) * payload->entities_amount);           \
  for (usz i = 0; i < payload->entities_amount; i++)                           \
    decode_fn(&payload->name[i], buf, decode_ctx);
  _COMPONENTS_ITER(COMP_DECODE)
#undef COMP_ENCODE
}

static void mygame_packet_create_player_encode(const tt_packet_t *packet,
                                               tt_byte_buf_t *buf,
                                               void *encode_ctx) {
  mygame_packet_create_player_t *payload = packet->payload;
  tt_encode_i64(payload->client_id, buf, encode_ctx);
  tt_encode_i64(payload->player_id, buf, encode_ctx);
  tt_encode_string(payload->player_name, buf, encode_ctx);
}

static void mygame_packet_create_player_decode(tt_packet_t *packet,
                                               tt_byte_buf_t *buf,
                                               void *decode_ctx) {
  mygame_packet_create_player_t *payload = packet->payload;
  payload->client_id = tt_decode_i64(buf, decode_ctx);
  payload->player_id = tt_decode_i64(buf, decode_ctx);
  tt_decode_string(&payload->player_name, &HEAP_ALLOCATOR, buf, decode_ctx);
}

tt_packet_info_t mygame_packet_infos[] = {
    [MYGAME_PACKET_PING] =
        {
            .packet_id = MYGAME_PACKET_PING,
            .packet_dir = TT_PACKET_DIR_TWO_WAY,
            .payload_size = sizeof(mygame_packet_ping_t),
            .encode_func = mygame_packet_ping_encode,
            .decode_func = mygame_packet_ping_decode,
        },
    [MYGAME_PACKET_HANDSHAKE_REQ] =
        {
            .packet_id = MYGAME_PACKET_HANDSHAKE_REQ,
            .packet_dir = TT_PACKET_DIR_SERVER,
            .payload_size = sizeof(mygame_packet_handshake_req_t),
            .encode_func = mygame_packet_handshake_req_encode,
            .decode_func = mygame_packet_handshake_req_decode,
        },
    [MYGAME_PACKET_HANDSHAKE_ACCEPT] =
        {
            .packet_id = MYGAME_PACKET_HANDSHAKE_ACCEPT,
            .packet_dir = TT_PACKET_DIR_CLIENT,
            .payload_size = sizeof(mygame_packet_handshake_accept_t),
            .encode_func = mygame_packet_handshake_accept_encode,
            .decode_func = mygame_packet_handshake_accept_decode,
        },
    [MYGAME_PACKET_HANDSHAKE_REJECT] =
        {
            .packet_id = MYGAME_PACKET_HANDSHAKE_REJECT,
            .packet_dir = TT_PACKET_DIR_CLIENT,
            .payload_size = sizeof(mygame_packet_handshake_reject_t),
            .encode_func = mygame_packet_handshake_reject_encode,
            .decode_func = mygame_packet_handshake_reject_decode,
        },
    [MYGAME_PACKET_SYNC_ENTITIES] =
        {
            .packet_id = MYGAME_PACKET_SYNC_ENTITIES,
            .packet_dir = TT_PACKET_DIR_CLIENT,
            .payload_size = sizeof(mygame_packet_sync_entities_t),
            .encode_func = mygame_packet_sync_entities_encode,
            .decode_func = mygame_packet_sync_entities_decode,
        },
    [MYGAME_PACKET_CREATE_PLAYER] =
        {
            .packet_id = MYGAME_PACKET_CREATE_PLAYER,
            .packet_dir = TT_PACKET_DIR_CLIENT,
            .payload_size = sizeof(mygame_packet_create_player_t),
            .encode_func = mygame_packet_create_player_encode,
            .decode_func = mygame_packet_create_player_decode,
        },
};
