#pragma once

#include "../include/tt_engine.h"
#include "../include/tt_packets.h"
#include "lilsockets.h"

typedef enum {
  TT_CONNECTION_OPEN,
  TT_CONNECTION_ILLEGAL_PACKET,
  TT_CONNECTION_CLOSED
} tt_connection_status_e;

tt_connection_status_e
tt_handle_connection(addr_t recv_addr, pthread_mutex_t *packet_queue_mutex,
                     tt_packet_queue_t *packet_data_queue);

bool tt_packet_queue_pop(tt_packet_info_array_t *packet_infos,
                         tt_packet_queue_t *packet_queue,
                         pthread_mutex_t *packet_queue_mutex,
                         tt_packet_t *out_packet,
                         tt_packet_context_t *out_context,
                         allocator_t *packet_alloc, void *decode_ctx);
