#include <pthread.h>
#define LILSOCKETS_IMPL
#include "../include/tt_engine.h"
#include "lilc/array.h"
#include "lilc/panic.h"
#include "lilc/deque.h"
#include <lilc/alloc.h>
#include "lilsockets.h"

void tt_client_connection_init(tt_client_connection_t *client_connection) {
  client_connection->server_addr = -1;
  client_connection->connected = false;

  client_connection->client_running = true;

  pthread_cond_init(&client_connection->connected_cond, NULL);
  pthread_mutex_init(&client_connection->connected_mutex, NULL);

  pthread_rwlock_init(&client_connection->connection_rwlock, NULL);

  deque_init_cap(client_connection->packet_queue, 256, &HEAP_ALLOCATOR);
  pthread_mutex_init(&client_connection->packet_queue_mutex, NULL);

  bump_init(&client_connection->packet_bump, 16000);
  bump_allocator_init(&client_connection->packet_alloc, &client_connection->packet_bump);
}

void tt_server_init(tt_server_t *server, bool integrated) {
  pthread_rwlock_init(&server->server_lock, NULL);

  server->state = TT_SERVER_RUNNING;

  server->integrated = integrated;
  server->connected_clients = array_new_capacity(tt_client_desc_t, 256, &HEAP_ALLOCATOR);

  deque_init_cap(server->packet_queue, 256, &HEAP_ALLOCATOR);
  pthread_mutex_init(&server->packet_queue_mutex, NULL);

  bump_init(&server->packet_bump, 16000);
  bump_allocator_init(&server->packet_alloc, &server->packet_bump);
}

void tt_server_deinit(tt_server_t *server) {
  deque_deinit(server->packet_queue);
  bump_free(&server->packet_bump);
}

void tt_byte_buf_init(tt_byte_buf_t *byte_buf, Allocator *alloc, usz capacity) {
  byte_buf->bytes = array_new_capacity(u8, capacity, alloc);
  byte_buf->read_idx = 0;
  byte_buf->write_idx = 0;
}

u8 tt_byte_buf_read(tt_byte_buf_t *byte_buf) {
  if (byte_buf->read_idx >= array_len(byte_buf->bytes)) {
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

void tt_packet_encode(const tt_packet_t *packet, tt_packet_info_t *packet_info,
                      tt_byte_buf_t *byte_buf, tt_packet_context_t context) {
  tt_struct_info_tt struct_info = packet_info->struct_info;
  u8 *payload = packet->payload;

  for (usz i = 0; i < struct_info.fields_count; i++) {
    tt_packet_struct_field_t field = struct_info.fields[i];
    const void *field_data = payload + field.offset;
    field.encode_func(field_data, byte_buf, context);
  }
}

void tt_packet_decode(tt_packet_t *packet, tt_packet_info_t *packet_info,
                      tt_byte_buf_t *byte_buf, tt_packet_context_t context) {
  tt_struct_info_tt struct_info = packet_info->struct_info;
  u8 *payload = packet->payload;

  for (usz i = 0; i < struct_info.fields_count; i++) {
    tt_packet_struct_field_t field = struct_info.fields[i];
    void *field_data = payload + field.offset;
    field.decode_func(field_data, byte_buf, context);
  }
}

void tt_packet_send_client(tt_engine_server_t *srvr_engine, addr_t client_addr,
                           tt_packet_t packet) {
  tt_packet_info_t *info = NULL;

  tt_byte_buf_t byte_buf = {0};
  tt_byte_buf_init(&byte_buf, &srvr_engine->server.packet_alloc, 256);

  tt_packet_encode(&packet, info, &byte_buf,
                   (tt_packet_context_t){.entities = NULL});

  sockets_send(client_addr, byte_buf.bytes, tt_byte_buf_len(&byte_buf));
}

void tt_packet_send_all_clients(tt_engine_server_t *srvr_engine,
                                tt_packet_t packet) {
  tt_client_desc_t *client_desc;
  array_foreach(srvr_engine->server.connected_clients, client_desc) {
    tt_packet_send_client(srvr_engine, client_desc->address, packet);
  }
}

void tt_packet_send_server(tt_engine_client_t *client_engine,
                           tt_packet_t packet);

void tt_encode_i64(const i64 *i64_val, tt_byte_buf_t *buf,
                   tt_packet_context_t ctx) {
  const i64 val = *(i64 *)i64_val;
  for (usz i = 1; i <= sizeof(i64); i++) {
    tt_byte_buf_write(buf, val >> ((sizeof(i64) - i) * 8));
  }
}

void tt_decode_i64(i64 *i64_val, tt_byte_buf_t *buf, tt_packet_context_t ctx) {
  i64 val = 0;
  for (usz i = 1; i <= sizeof(i64); i++) {
    val |= tt_byte_buf_read(buf) << ((sizeof(i64) - i) * 8);
  }

  *(i64 *)i64_val = val;
}
