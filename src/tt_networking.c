#include <lilc/log.h>
#define LILSOCKETS_IMPL
#include "../include/tt_engine.h"
#include "lilc/alloc.h"
#include "lilc/array.h"
#include "lilc/deque.h"
#include "lilc/panic.h"
#include "lilsockets.h"
#include <pthread.h>

void tt_client_connection_init(tt_client_connection_t *client_connection) {
  client_connection->server_addr = -1;
  client_connection->state = TT_CLIENT_RUNNING;

  pthread_rwlock_init(&client_connection->connection_rwlock, NULL);

  deque_init_cap(client_connection->packet_data_queue, 256, &HEAP_ALLOCATOR);
  pthread_mutex_init(&client_connection->packet_queue_mutex, NULL);

  bump_init(&client_connection->packet_bump, 16000);
  bump_allocator_init(&client_connection->packet_alloc,
                      &client_connection->packet_bump);

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

  deque_init_cap(server->packet_data_queue, 256, &HEAP_ALLOCATOR);
  pthread_mutex_init(&server->packet_queue_mutex, NULL);

  bump_init(&server->packet_bump, 16000);
  bump_allocator_init(&server->packet_alloc, &server->packet_bump);

  server->packet_infos.infos = array_new(tt_packet_info_t, &HEAP_ALLOCATOR);
  server->packet_infos.locked = false;
}

void tt_server_deinit(tt_server_t *server) {
  deque_deinit(server->packet_data_queue);
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
  tt_encode_i32(info.packet_handle, buf, encode_ctx);
  info.encode_func(packet, buf, encode_ctx);
}

void tt_decode_packet(tt_packet_t *packet, tt_packet_info_t info,
                      tt_byte_buf_t *buf, void *decode_ctx) {
  info.decode_func(packet, buf, decode_ctx);
}

void tt_packet_send_client(tt_engine_server_t *srvr_engine, addr_t client_addr,
                           tt_packet_t packet, void *encode_ctx) {
  pthread_rwlock_wrlock(&srvr_engine->server.server_lock);
  tt_byte_buf_t byte_buf = {0};
  tt_byte_buf_init(&byte_buf, &srvr_engine->server.packet_alloc, 256);

  tt_encode_packet(&packet,
                   srvr_engine->server.packet_infos.infos[packet.packet_handle],
                   &byte_buf, encode_ctx);

  u32 bytebuf_len = tt_byte_buf_len(&byte_buf);

  sockets_send(client_addr, &bytebuf_len, sizeof(u32));
  sockets_send(client_addr, byte_buf.bytes, bytebuf_len);
  pthread_rwlock_unlock(&srvr_engine->server.server_lock);
}

void tt_packet_send_all_clients(tt_engine_server_t *srvr_engine,
                                tt_packet_t packet, void *encode_ctx) {
  tt_client_desc_t *client_desc;
  array_foreach(srvr_engine->server.connected_clients, client_desc) {
    tt_packet_send_client(srvr_engine, client_desc->address, packet,
                          encode_ctx);
  }
}

void tt_packet_send_server(tt_engine_client_t *client_engine,
                           tt_packet_t packet, void *encode_ctx) {
  pthread_rwlock_wrlock(&client_engine->client_connection.connection_rwlock);
  tt_byte_buf_t byte_buf = {0};
  tt_byte_buf_init(&byte_buf, &client_engine->client_connection.packet_alloc,
                   256);

  tt_encode_packet(&packet,
                   client_engine->client_connection.packet_infos.infos[packet.packet_handle],
                   &byte_buf, encode_ctx);

  u32 bytebuf_len = tt_byte_buf_len(&byte_buf);

  sockets_send(client_engine->client_connection.server_addr, &bytebuf_len, sizeof(u32));
  sockets_send(client_engine->client_connection.server_addr, byte_buf.bytes, bytebuf_len);
  pthread_rwlock_unlock(&client_engine->client_connection.connection_rwlock);
}

static void tt_encode_num(i64 num, u64 num_size, tt_byte_buf_t *buf,
                          void *encode_ctx) {
  (void)encode_ctx;

  if (buf == NULL || num_size == 0 || num_size > sizeof(i64)) {
    return;
  }

  const i64 val = *(i64 *)num;
  for (usz i = 1; i <= num_size; i++) {
    tt_byte_buf_write(buf, val >> ((num_size - i) * 8));
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

void tt_encode_i64(i64 val, tt_byte_buf_t *buf, tt_packet_context_t ctx) {
  tt_encode_num(val, sizeof(i64), buf, ctx);
}

i64 tt_decode_i64(tt_byte_buf_t *buf, tt_packet_context_t ctx) {
  return tt_decode_num(sizeof(i64), buf, ctx);
}

void tt_encode_i32(i32 val, tt_byte_buf_t *buf, tt_packet_context_t ctx) {
  tt_encode_num(val, sizeof(i32), buf, ctx);
}

i32 tt_decode_i32(tt_byte_buf_t *buf, tt_packet_context_t ctx) {
  return tt_decode_num(sizeof(i32), buf, ctx);
}

void tt_encode_i16(i16 val, tt_byte_buf_t *buf, tt_packet_context_t ctx) {
  tt_encode_num(val, sizeof(i16), buf, ctx);
}

i16 tt_decode_i16(tt_byte_buf_t *buf, tt_packet_context_t ctx) {
  return tt_decode_num(sizeof(i16), buf, ctx);
}
