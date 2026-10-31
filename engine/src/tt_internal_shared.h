#pragma once

#include "../include/tt_engine.h"
#include "../include/tt_packets.h"
#include "lilsockets.h"

typedef enum {
  TT_SIDE_CLIENT,
  TT_SIDE_SERVER,
} tt_side_e;

typedef enum {
  TT_CONNECTION_OPEN,
  TT_CONNECTION_ILLEGAL_PACKET,
  TT_CONNECTION_CLOSED
} tt_connection_status_e;

struct tt_connection {
  addr_t recv_addr;
  u64 client_id;
  const tt_packet_info_array_t *connection_packets;
  pthread_mutex_t *packet_queue_mutex;
  tt_packet_queue_t *packet_data_queue;
  tt_side_e side;
};

tt_connection_status_e tt_handle_connection(struct tt_connection *connection);

bool tt_packet_queue_pop(tt_packet_info_array_t *packet_infos,
                         tt_packet_queue_t *packet_queue,
                         pthread_mutex_t *packet_queue_mutex,
                         tt_packet_t *out_packet,
                         tt_packet_context_t *out_context,
                         allocator_t *packet_alloc, void *decode_ctx);
