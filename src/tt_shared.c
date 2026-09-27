#include "../include/tt_shared.h"
#include "../include/tt_engine.h"
#include "../include/tt_packets.h"

#include "lilc/alloc.h"
#include "lilc/deque.h"
#include "lilc/log.h"

#include <pthread.h>
#include <time.h>

void tt_wait(i32 millis) {
  struct timespec ts;
  ts.tv_sec = millis / 1000;
  ts.tv_nsec = (millis % 1000) * 1000000L;
  nanosleep(&ts, NULL);
}

static void tt_packet_init(allocator_t *alloc, tt_packet_t *packet,
                           tt_packet_info_t packet_info) {
  packet->packet_id = packet_info.packet_id;
  packet->packet_handle = packet_info.packet_handle;
  packet->payload = alloc->alloc(alloc, packet_info.payload_size);
}

bool tt_handle_connection(addr_t recv_addr,
                                 allocator_t *packet_alloc,
                                 pthread_mutex_t *packet_queue_mutex,
                                 tt_byte_buf_t **packet_data_queue) {
  u8 len_buf[8];
  i64 len_res = sockets_receive(recv_addr, len_buf, 8);
  if (len_res != 4) {
    return false;
  }

  u32 len = (len_buf[0] << 24) | (len_buf[1] << 16) | (len_buf[2] << 8) |
            (len_buf[3] << 0);

  tt_byte_buf_t bytebuf = {0};
  tt_byte_buf_init(&bytebuf, packet_alloc, len);

  i64 payload_res = sockets_receive(recv_addr, bytebuf.bytes, len);
  if (payload_res == -1) {
    return false;
  }

  pthread_mutex_lock(packet_queue_mutex);
  deque_push_back(*packet_data_queue, bytebuf);
  pthread_mutex_unlock(packet_queue_mutex);

  return true;
}

static bool tt_decode_packet_with_handle(tt_packet_info_array_t *packet_infos,
                                         tt_byte_buf_t *packet_bytes,
                                         tt_packet_t *packet,
                                         allocator_t *packet_alloc,
                                         void *decode_ctx) {
  if (!packet_infos->locked) {
    log_error("Packet infos needs to be locked, before using packets");
    return false;
  }
  if (tt_byte_buf_len(packet_bytes) < 4) {
    log_error("Invalid packet header");
    return false;
  }

  tt_packet_handle_t packet_handle = tt_decode_i32(packet_bytes, decode_ctx);
  tt_packet_info_t packet_info = packet_infos->infos[packet_handle];

  tt_packet_init(packet_alloc, packet, packet_info);

  packet_info.decode_func(packet, packet_bytes, decode_ctx);

  return true;
}

bool tt_packet_queue_pop(tt_packet_info_array_t *packet_infos,
                         tt_byte_buf_t *packet_queue,
                         pthread_mutex_t *packet_queue_mutex,
                         tt_packet_t *out_packet, allocator_t *packet_alloc,
                         void *decode_ctx) {
  bool success = false;
  pthread_mutex_lock(packet_queue_mutex);

  if (deque_len(packet_queue) > 0) {
    tt_byte_buf_t *bytes = deque_pop_front(packet_queue);
    if (bytes != NULL) {
      success = tt_decode_packet_with_handle(packet_infos, bytes, out_packet, packet_alloc,
                                   decode_ctx);
      if (!success) {
        log_error("Failed to decode packet");
      }

      tt_byte_buf_deinit(bytes);
    }
  }

  pthread_mutex_unlock(packet_queue_mutex);

  return success;
}
