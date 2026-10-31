#pragma once

#include "lilc/numbers.h"

#include "../engine/include/tt_backend.h"
#include "../engine/include/tt_packets.h"
#include "components.h"
#include "shared.h"

#define PROTOCOL_VERSION 0x0000000000000001

typedef enum {
#define PACKETS_ITER(_F, ...)                                                  \
  _F(MYGAME_PACKET_PING, __VA_ARGS__)                                          \
  _F(MYGAME_PACKET_HANDSHAKE_REQ, __VA_ARGS__)                                 \
  _F(MYGAME_PACKET_HANDSHAKE_ACCEPT, __VA_ARGS__)                              \
  _F(MYGAME_PACKET_HANDSHAKE_REJECT, __VA_ARGS__)                              \
  _F(MYGAME_PACKET_SYNC_ENTITIES, __VA_ARGS__)                                 \
  _F(MYGAME_PACKET_CREATE_PLAYER, __VA_ARGS__)                                 \
  _F(MYGAME_PACKET_REMOVE_PLAYER, __VA_ARGS__)                                 \
  _F(MYGAME_PACKET_CREATE_ENTITY, __VA_ARGS__)                                 \
  _F(MYGAME_PACKET_MOUSE_INPUT, __VA_ARGS__)                                   \
  _F(MYGAME_PACKET_MOVEMENT_INPUT, __VA_ARGS__)                                \
  _F(MYGAME_PACKET_PLAYER_STATE, __VA_ARGS__)

#define PACKET_DECL(name, ...) name,
  PACKETS_ITER(PACKET_DECL)
#undef PACKET_DECL
} mygame_packet_id_e;

#define _COUNT_PACKETS(...) 1 +
#define PACKETS_SIZE PACKETS_ITER(_COUNT_PACKETS) 0

typedef struct {
  u64 time;
} mygame_packet_ping_t;

typedef struct {
  u64 protocol_version;
  char *player_name;
  color_t player_color;
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
  entity_t player_id;
  char *player_name;
  color_t player_color;
} mygame_packet_create_player_t;

typedef struct {
  entity_t player_id;
} mygame_packet_remove_player_t;

typedef struct {
  entity_t entity_id;

#define DECL_COMP(ty, name, ...) ty name;
  _COMPONENTS_ITER(DECL_COMP)
#undef DECL_COMP
} mygame_packet_create_entity_t;

typedef struct {
  ttb_event_mouse_input_t mouse_input;
  i32 world_click_x;
  i32 world_click_y;
} mygame_packet_mouse_input_t;

typedef struct {
  u32 seq;
  bool forward;
  bool backward;
  bool left;
  bool right;
} mygame_packet_movement_input_t;

typedef struct {
  u64 client_id;
  mygame_packet_movement_input_t input; // input.seq == last applied seq
  i32 x, y;
  i32 frac_x, frac_y; // sub-pixel remainder, fixed point 1/65536
} mygame_packet_player_state_t;

extern tt_packet_info_t mygame_packet_infos[];
