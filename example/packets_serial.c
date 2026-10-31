#include "../engine/include/tt_packets.h"
#include "entity_manager.h"
#include "packets.h"

void encode_move_input(const mygame_packet_movement_input_t *in,
                       tt_byte_buf_t *buf, void *ctx) {
  tt_encode_i32((i32)in->seq, buf, ctx);
  tt_byte_buf_write(buf, in->forward);
  tt_byte_buf_write(buf, in->backward);
  tt_byte_buf_write(buf, in->left);
  tt_byte_buf_write(buf, in->right);
}
void decode_move_input(mygame_packet_movement_input_t *in, tt_byte_buf_t *buf,
                       void *ctx) {
  in->seq = (u32)tt_decode_i32(buf, ctx);
  in->forward = tt_byte_buf_read(buf);
  in->backward = tt_byte_buf_read(buf);
  in->left = tt_byte_buf_read(buf);
  in->right = tt_byte_buf_read(buf);
}

void mygame_packet_player_state_encode(const tt_packet_t *packet,
                                       tt_byte_buf_t *buf, void *encode_ctx) {
  mygame_packet_player_state_t *p = packet->payload;
  tt_encode_i64(p->client_id, buf, encode_ctx);
  encode_move_input(&p->input, buf, encode_ctx);
  tt_encode_i32(p->x, buf, encode_ctx);
  tt_encode_i32(p->y, buf, encode_ctx);
  tt_encode_i32(p->frac_x, buf, encode_ctx);
  tt_encode_i32(p->frac_y, buf, encode_ctx);
}
void mygame_packet_player_state_decode(tt_packet_t *packet, tt_byte_buf_t *buf,
                                       void *decode_ctx) {
  mygame_packet_player_state_t *p = packet->payload;
  p->client_id = tt_decode_i64(buf, decode_ctx);
  decode_move_input(&p->input, buf, decode_ctx);
  p->x = tt_decode_i32(buf, decode_ctx);
  p->y = tt_decode_i32(buf, decode_ctx);
  p->frac_x = tt_decode_i32(buf, decode_ctx);
  p->frac_y = tt_decode_i32(buf, decode_ctx);
}

void mygame_packet_ping_encode(const tt_packet_t *packet, tt_byte_buf_t *buf,
                               void *encode_ctx) {
  mygame_packet_ping_t *ping_packet = packet->payload;
  tt_encode_i64(ping_packet->time, buf, encode_ctx);
}
void mygame_packet_ping_decode(tt_packet_t *packet, tt_byte_buf_t *buf,
                               void *decode_ctx) {
  mygame_packet_ping_t *ping_packet = packet->payload;
  ping_packet->time = tt_decode_i64(buf, decode_ctx);
}

void mygame_packet_handshake_req_encode(const tt_packet_t *packet,
                                        tt_byte_buf_t *buf, void *encode_ctx) {
  mygame_packet_handshake_req_t *payload = packet->payload;
  tt_encode_i64(payload->protocol_version, buf, encode_ctx);
  tt_encode_string(payload->player_name, buf, encode_ctx);
  tt_encode_i32(payload->player_color, buf, encode_ctx);
}

void mygame_packet_handshake_req_decode(tt_packet_t *packet, tt_byte_buf_t *buf,
                                        void *decode_ctx) {
  entity_manager_t *entities = decode_ctx;
  mygame_packet_handshake_req_t *payload = packet->payload;
  payload->protocol_version = tt_decode_i64(buf, decode_ctx);
  tt_decode_string(&payload->player_name,
                   &entities->server_client_info_bump.alloc, buf, decode_ctx);
  payload->player_color = tt_decode_i32(buf, decode_ctx);
}

void mygame_packet_handshake_accept_encode(const tt_packet_t *packet,
                                           tt_byte_buf_t *buf,
                                           void *encode_ctx) {
  mygame_packet_handshake_accept_t *payload = packet->payload;
  tt_encode_i64(payload->client_id, buf, encode_ctx);
}

void mygame_packet_handshake_accept_decode(tt_packet_t *packet,
                                           tt_byte_buf_t *buf,
                                           void *decode_ctx) {
  mygame_packet_handshake_accept_t *payload = packet->payload;
  payload->client_id = tt_decode_i64(buf, decode_ctx);
}

void mygame_packet_handshake_reject_encode(const tt_packet_t *packet,
                                           tt_byte_buf_t *buf,
                                           void *encode_ctx) {
  mygame_packet_handshake_reject_t *payload = packet->payload;
  tt_encode_i64(payload->reject_reason, buf, encode_ctx);
}

void mygame_packet_handshake_reject_decode(tt_packet_t *packet,
                                           tt_byte_buf_t *buf,
                                           void *decode_ctx) {
  mygame_packet_handshake_reject_t *payload = packet->payload;
  payload->reject_reason = tt_decode_i64(buf, decode_ctx);
}

void mygame_packet_sync_entities_encode(const tt_packet_t *packet,
                                        tt_byte_buf_t *buf, void *encode_ctx) {
  mygame_packet_sync_entities_t *payload = packet->payload;
  tt_encode_i64(payload->entities_amount, buf, encode_ctx);

#define COMP_ENCODE(ty, name, encode_fn, decode_fn, i, ...)                    \
  encode_fn(&payload->name[i], buf, encode_ctx);

  for (usz i = 0; i < payload->entities_amount; i++) {
    _COMPONENTS_ITER(COMP_ENCODE, i)
  }
#undef COMP_ENCODE
}

void mygame_packet_sync_entities_decode(tt_packet_t *packet, tt_byte_buf_t *buf,
                                        void *decode_ctx) {
  mygame_packet_sync_entities_t *payload = packet->payload;
  payload->entities_amount = tt_decode_i64(buf, decode_ctx);

#define COMP_DECODE(ty, name, encode_fn, decode_fn, ...)                       \
  payload->name = heap_alloc(sizeof(ty) * payload->entities_amount);           \
  for (usz i = 0; i < payload->entities_amount; i++)                           \
    decode_fn(&payload->name[i], buf, decode_ctx);
  _COMPONENTS_ITER(COMP_DECODE)
#undef COMP_DECODE
}

void mygame_packet_create_player_encode(const tt_packet_t *packet,
                                        tt_byte_buf_t *buf, void *encode_ctx) {
  mygame_packet_create_player_t *payload = packet->payload;
  tt_encode_i64(payload->client_id, buf, encode_ctx);
  tt_encode_i64(payload->player_id, buf, encode_ctx);
  tt_encode_string(payload->player_name, buf, encode_ctx);
  tt_encode_i32(payload->player_color, buf, encode_ctx);
}
void mygame_packet_create_player_decode(tt_packet_t *packet, tt_byte_buf_t *buf,
                                        void *decode_ctx) {
  mygame_packet_create_player_t *payload = packet->payload;
  payload->client_id = tt_decode_i64(buf, decode_ctx);
  payload->player_id = tt_decode_i64(buf, decode_ctx);
  tt_decode_string(&payload->player_name, &HEAP_ALLOCATOR, buf, decode_ctx);
  payload->player_color = tt_decode_i32(buf, decode_ctx);
}

void mygame_packet_remove_player_encode(const tt_packet_t *packet,
                                        tt_byte_buf_t *buf, void *encode_ctx) {
  mygame_packet_remove_player_t *payload = packet->payload;
  tt_encode_i64(payload->player_id, buf, encode_ctx);
}
void mygame_packet_remove_player_decode(tt_packet_t *packet, tt_byte_buf_t *buf,
                                        void *decode_ctx) {
  mygame_packet_remove_player_t *payload = packet->payload;
  payload->player_id = tt_decode_i64(buf, decode_ctx);
}

void mygame_packet_movement_input_encode(const tt_packet_t *packet,
                                         tt_byte_buf_t *buf, void *encode_ctx) {
  encode_move_input(packet->payload, buf, encode_ctx);
}
void mygame_packet_movement_input_decode(tt_packet_t *packet,
                                         tt_byte_buf_t *buf, void *decode_ctx) {
  decode_move_input(packet->payload, buf, decode_ctx);
}

void mygame_packet_mouse_input_encode(const tt_packet_t *packet,
                                      tt_byte_buf_t *buf, void *encode_ctx) {
  mygame_packet_mouse_input_t *payload = packet->payload;
  for (usz i = 0; i < _amount_ttb_mouse_btns; i++) {
    tt_byte_buf_write(buf, payload->mouse_input.buttons[i]);
  }
  tt_encode_i32(payload->world_click_x, buf, encode_ctx);
  tt_encode_i32(payload->world_click_y, buf, encode_ctx);
}
void mygame_packet_mouse_input_decode(tt_packet_t *packet, tt_byte_buf_t *buf,
                                      void *decode_ctx) {
  mygame_packet_mouse_input_t *payload = packet->payload;
  for (usz i = 0; i < _amount_ttb_mouse_btns; i++) {
    payload->mouse_input.buttons[i] = tt_byte_buf_read(buf);
  }
  payload->world_click_x = tt_decode_i32(buf, decode_ctx);
  payload->world_click_y = tt_decode_i32(buf, decode_ctx);
}

void mygame_packet_create_entity_encode(const tt_packet_t *packet,
                                        tt_byte_buf_t *buf, void *encode_ctx) {
  mygame_packet_create_entity_t *payload = packet->payload;
  tt_encode_i64(payload->entity_id, buf, encode_ctx);

#define COMP_ENCODE(ty, name, encode_fn, ...)                                  \
  encode_fn(&payload->name, buf, encode_ctx);
  _COMPONENTS_ITER(COMP_ENCODE)
#undef COMP_ENCODE
}
void mygame_packet_create_entity_decode(tt_packet_t *packet, tt_byte_buf_t *buf,
                                        void *decode_ctx) {
  mygame_packet_create_entity_t *payload = packet->payload;
  payload->entity_id = tt_decode_i64(buf, decode_ctx);

#define COMP_DECODE(ty, name, encode_fn, decode_fn, ...)                       \
  decode_fn(&payload->name, buf, decode_ctx);
  _COMPONENTS_ITER(COMP_DECODE)
#undef COMP_DECODE
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
    [MYGAME_PACKET_REMOVE_PLAYER] =
        {
            .packet_id = MYGAME_PACKET_REMOVE_PLAYER,
            .packet_dir = TT_PACKET_DIR_CLIENT,
            .payload_size = sizeof(mygame_packet_remove_player_t),
            .encode_func = mygame_packet_remove_player_encode,
            .decode_func = mygame_packet_remove_player_decode,
        },
    [MYGAME_PACKET_CREATE_ENTITY] =
        {
            .packet_id = MYGAME_PACKET_CREATE_ENTITY,
            .packet_dir = TT_PACKET_DIR_CLIENT,
            .payload_size = sizeof(mygame_packet_create_entity_t),
            .encode_func = mygame_packet_create_entity_encode,
            .decode_func = mygame_packet_create_entity_decode,
        },
    [MYGAME_PACKET_MOUSE_INPUT] =
        {
            .packet_id = MYGAME_PACKET_MOUSE_INPUT,
            .packet_dir = TT_PACKET_DIR_SERVER,
            .payload_size = sizeof(mygame_packet_mouse_input_t),
            .encode_func = mygame_packet_mouse_input_encode,
            .decode_func = mygame_packet_mouse_input_decode,
        },
    [MYGAME_PACKET_MOVEMENT_INPUT] =
        {
            .packet_id = MYGAME_PACKET_MOVEMENT_INPUT,
            .packet_dir = TT_PACKET_DIR_SERVER,
            .payload_size = sizeof(mygame_packet_movement_input_t),
            .encode_func = mygame_packet_movement_input_encode,
            .decode_func = mygame_packet_movement_input_decode,
        },
    [MYGAME_PACKET_PLAYER_STATE] = {
        .packet_id = MYGAME_PACKET_PLAYER_STATE,
        .packet_dir = TT_PACKET_DIR_CLIENT,
        .payload_size = sizeof(mygame_packet_player_state_t),
        .encode_func = mygame_packet_player_state_encode,
        .decode_func = mygame_packet_player_state_decode,
    }};

