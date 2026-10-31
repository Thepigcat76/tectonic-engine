#define LILSOCKETS_IMPL
#include "../include/tt_engine.h"
#include "lilc/alloc.h"
#include "lilc/array.h"
#include "lilc/deque.h"
#include "lilc/dynstr.h"
#include "lilc/log.h"
#include "lilc/panic.h"
#include "lilsockets.h"
#include "tt_internal_shared.h"
#include <pthread.h>

void tt_client_connection_init(tt_client_connection_t *client_connection) {
  client_connection->server_addr = -1;
  client_connection->state = TT_CLIENT_RUNNING;

  pthread_rwlock_init(&client_connection->connection_rwlock, NULL);

  deque_init_cap(client_connection->packet_data_queue, 256, &HEAP_ALLOCATOR);
  pthread_mutex_init(&client_connection->packet_queue_mutex, NULL);

  bump_init(&client_connection->packet_bump, 16000);

  client_connection->packet_infos.infos =
      array_new(tt_packet_info_t, &HEAP_ALLOCATOR);
  client_connection->packet_infos.locked = false;
}

void tt_client_connection_deinit(tt_client_connection_t *client_connection) {
  deque_deinit(client_connection->packet_data_queue);
  array_free(client_connection->packet_infos.infos);
}

void tt_server_init(tt_server_t *server, bool integrated) {
  pthread_rwlock_init(&server->server_lock, NULL);

  server->state = TT_SERVER_RUNNING;

  server->integrated = integrated;
  server->connected_clients =
      array_new_capacity(tt_client_desc_t, 256, &HEAP_ALLOCATOR);
  server->poll_clients =
      array_new_capacity(poll_client_t, 256, &HEAP_ALLOCATOR);
  pthread_mutex_init(&server->poll_clients_mutex, NULL);

  deque_init_cap(server->packet_data_queue, 256, &HEAP_ALLOCATOR);
  pthread_mutex_init(&server->packet_queue_mutex, NULL);

  deque_init_cap(server->removed_clients, 256, &HEAP_ALLOCATOR);

  bump_init(&server->packet_bump, 16000);

  server->packet_infos.infos = array_new(tt_packet_info_t, &HEAP_ALLOCATOR);
  server->packet_infos.locked = false;
}

void tt_server_deinit(tt_server_t *server) {
  deque_deinit(server->packet_data_queue);
  deque_deinit(server->removed_clients);
  bump_free(&server->packet_bump);
  array_free(server->packet_infos.infos);

  array_free(server->connected_clients);
  array_free(server->poll_clients);
}

void tt_byte_buf_init(tt_byte_buf_t *byte_buf, allocator_t *alloc,
                      usz capacity) {
  byte_buf->bytes = array_new_capacity(u8, capacity, alloc);
  byte_buf->read_idx = 0;
  byte_buf->write_idx = 0;
}

void tt_byte_buf_deinit(tt_byte_buf_t *byte_buf) {
  array_free(byte_buf->bytes);
}

u8 tt_byte_buf_read(tt_byte_buf_t *byte_buf) {
  if (byte_buf->read_idx >= tt_byte_buf_len(byte_buf)) {
    panic("Reached end of bytebuf");
  }

  return byte_buf->bytes[byte_buf->read_idx++];
}

void tt_byte_buf_write(tt_byte_buf_t *byte_buf, u8 val) {
  array_add(byte_buf->bytes, val);
  ++byte_buf->write_idx;
}

usz tt_byte_buf_len(tt_byte_buf_t *byte_buf) {
  return array_len(byte_buf->bytes);
}

void tt_encode_packet(const tt_packet_t *packet, tt_packet_info_t info,
                      tt_byte_buf_t *buf, void *encode_ctx) {
  tt_encode_i32(info.packet_id, buf, encode_ctx);
  info.encode_func(packet, buf, encode_ctx);
}

void tt_decode_packet(tt_packet_t *packet, tt_packet_info_t info,
                      tt_byte_buf_t *buf, void *decode_ctx) {
  info.decode_func(packet, buf, decode_ctx);
}

static void tt_send_packet_clients(addr_t *addresses, usz addresses_size,
                                   tt_packet_t *packet,
                                   tt_packet_info_t *packet_info,
                                   void *encode_ctx, tt_side_e side) {
  tt_byte_buf_t byte_buf = {0};
  tt_byte_buf_init(&byte_buf, &HEAP_ALLOCATOR, 1024);

  tt_encode_packet(packet, *packet_info, &byte_buf, encode_ctx);
  u64 bytebuf_len = tt_byte_buf_len(&byte_buf);

  for (usz i = 0; i < addresses_size; i++) {
    sockets_send(addresses[i], &bytebuf_len, sizeof(u64));
    sockets_send(addresses[i], byte_buf.bytes, bytebuf_len);
  }

  tt_byte_buf_deinit(&byte_buf);

  char *side_lit = side == TT_SIDE_CLIENT ? "CLIENT" : "SERVER";
  char *packet_name = NULL;
  if (packet_info != NULL) {
    packet_name = packet_info->packet_name;
  }

  if (packet_name != NULL) {
    log_info("[%s] Sent Packet %s", side_lit, packet_name);
  } else {
    log_info("[%s] Sent Packet %zu", side_lit, packet->packet_id);
  }
}

void tt_packet_send_client(tt_engine_server_t *srvr_engine, u64 client_id,
                           tt_packet_t packet, void *encode_ctx) {
  pthread_rwlock_rdlock(&srvr_engine->server.server_lock);
  tt_packet_info_t packet_info =
      srvr_engine->server.packet_infos.infos[packet.packet_id];
  addr_t addr = srvr_engine->server.connected_clients[client_id].address;
  pthread_rwlock_unlock(&srvr_engine->server.server_lock);

  if (packet_info.packet_dir != TT_PACKET_DIR_TWO_WAY &&
      packet_info.packet_dir != TT_PACKET_DIR_CLIENT) {
    log_warn("Tried sending packet %s with id %zu to client, even though it is "
             "a server bound packet",
             packet_info.packet_name, packet_info.packet_id);
    return;
  }

  tt_send_packet_clients(&addr, 1, &packet, &packet_info, encode_ctx,
                         TT_SIDE_SERVER);
}

void tt_packet_send_all_clients(tt_engine_server_t *srvr_engine,
                                tt_packet_t packet, void *encode_ctx) {
  pthread_rwlock_rdlock(&srvr_engine->server.server_lock);
  tt_packet_info_t packet_info =
      srvr_engine->server.packet_infos.infos[packet.packet_id];
  pthread_rwlock_unlock(&srvr_engine->server.server_lock);

  if (packet_info.packet_dir != TT_PACKET_DIR_TWO_WAY &&
      packet_info.packet_dir != TT_PACKET_DIR_CLIENT) {
    log_warn("Tried sending packet %s with id %zu to client, even though it is "
             "a server bound packet",
             packet_info.packet_name, packet_info.packet_id);
    return;
  }

  usz len = array_len(srvr_engine->server.connected_clients);
  addr_t addresses[len];
  tt_client_desc_t *client_desc;
  array_foreach(srvr_engine->server.connected_clients, client_desc) {
    addresses[_arr_foreach_idx] = client_desc->address;
  }

  tt_send_packet_clients(addresses, len, &packet, &packet_info, encode_ctx,
                         TT_SIDE_SERVER);
}

void tt_packet_send_server(tt_engine_client_t *client_engine,
                           tt_packet_t packet, void *encode_ctx) {
  pthread_rwlock_rdlock(&client_engine->client_connection.connection_rwlock);
  tt_packet_info_t packet_info =
      client_engine->client_connection.packet_infos.infos[packet.packet_id];
  addr_t server_addr = client_engine->client_connection.server_addr;
  pthread_rwlock_unlock(&client_engine->client_connection.connection_rwlock);

  if (packet_info.packet_dir != TT_PACKET_DIR_TWO_WAY &&
      packet_info.packet_dir != TT_PACKET_DIR_SERVER) {
    log_warn("Tried sending packet %s with id %zu to server, even though it is "
             "a client bound packet",
             packet_info.packet_name, packet_info.packet_id);
    return;
  }

  tt_send_packet_clients(&server_addr, 1, &packet, &packet_info, encode_ctx,
                         TT_SIDE_CLIENT);
}

static void tt_encode_num(i64 num, u64 num_size, tt_byte_buf_t *buf,
                          void *encode_ctx) {
  (void)encode_ctx;

  if (buf == NULL || num_size == 0 || num_size > sizeof(i64)) {
    return;
  }

  for (usz i = 1; i <= num_size; i++) {
    tt_byte_buf_write(buf, num >> ((num_size - i) * 8));
  }
}

static i64 tt_decode_num(u64 num_size, tt_byte_buf_t *buf, void *encode_ctx) {
  (void)encode_ctx;

  if (buf == NULL || num_size == 0 || num_size > sizeof(i64)) {
    return 0;
  }

  u64 val = 0;

  for (u64 i = 0; i < num_size; ++i) {
    val = (val << 8) | (u64)tt_byte_buf_read(buf);
  }

  if (num_size < sizeof(i64) &&
      (val & (UINT64_C(1) << (num_size * 8 - 1))) != 0) {
    val |= UINT64_MAX << (num_size * 8);
  }

  return val;
}

void tt_encode_i64(i64 val, tt_byte_buf_t *buf, void *ctx) {
  tt_encode_num(val, sizeof(i64), buf, ctx);
}

i64 tt_decode_i64(tt_byte_buf_t *buf, void *ctx) {
  return tt_decode_num(sizeof(i64), buf, ctx);
}

void tt_encode_i32(i32 val, tt_byte_buf_t *buf, void *ctx) {
  tt_encode_num(val, sizeof(i32), buf, ctx);
}

i32 tt_decode_i32(tt_byte_buf_t *buf, void *ctx) {
  return tt_decode_num(sizeof(i32), buf, ctx);
}

void tt_encode_i16(i16 val, tt_byte_buf_t *buf, void *ctx) {
  tt_encode_num(val, sizeof(i16), buf, ctx);
}

i16 tt_decode_i16(tt_byte_buf_t *buf, void *ctx) {
  return tt_decode_num(sizeof(i16), buf, ctx);
}

void tt_encode_string(const char *string, tt_byte_buf_t *buf,
                      void *encode_ctx) {
  if (string == NULL) {
    tt_encode_i64(0, buf, encode_ctx);
  }
  usz len = strlen(string);
  tt_encode_i64(len, buf, encode_ctx);

  for (usz i = 0; i < len; ++i) {
    tt_byte_buf_write(buf, string[i]);
  }
}

// TODO: Make this safe, so we dont just allocate an arbitrary number of bytes
void tt_decode_string(char **string, allocator_t *alloc, tt_byte_buf_t *buf,
                      void *decode_ctx) {
  usz len = tt_decode_i64(buf, decode_ctx);
  if (len == 0)
    return;

  if (string != NULL)
    *string = alloc->alloc(alloc, len + 1);

  for (usz i = 0; i < len; ++i) {
    u8 b = tt_byte_buf_read(buf);
    if (string != NULL && *string != NULL)
      (*string)[i] = b;
  }

  if (string != NULL && *string != NULL)
    (*string)[len] = '\0';
}

void tt_encode_dynstr(const dyn_string_t *str, tt_byte_buf_t *buf,
                      void *encode_ctx) {
  tt_encode_string(str->string, buf, encode_ctx);
}

void tt_decode_dynstr(dyn_string_t *str, tt_byte_buf_t *buf, void *decode_ctx) {
  usz len = tt_decode_i64(buf, decode_ctx);
  dyn_string_ensure_size(str, len + 1);

  for (usz i = 0; i < len; ++i) {
    dyn_string_add_char(str, tt_byte_buf_read(buf));
  }
}
