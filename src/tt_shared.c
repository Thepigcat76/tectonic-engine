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

static bool tt_packet_decode(tt_packet_info_array_t *packet_infos,
                             tt_byte_buf_t *packet_bytes, tt_packet_t *packet,
                             allocator_t *packet_alloc,
                             tt_packet_context_t packet_decode_ctx) {
  if (!packet_infos->locked) {
    log_error("Packet infos needs to be locked, before using packets");
    return false;
  }
  if (tt_byte_buf_len(packet_bytes) < 4) {
    log_error("Invalid packet header");
    return false;
  }

  u8 packet_info_idx0 = tt_byte_buf_read(packet_bytes);
  u8 packet_info_idx1 = tt_byte_buf_read(packet_bytes);
  u8 packet_info_idx2 = tt_byte_buf_read(packet_bytes);
  u8 packet_info_idx3 = tt_byte_buf_read(packet_bytes);

  u32 packet_info_idx = (packet_info_idx0 << 24) | (packet_info_idx1 << 16) |
                        (packet_info_idx2 << 8) | (packet_info_idx3 << 0);
  tt_packet_info_t packet_info = packet_infos->infos[packet_info_idx];

  packet->packet_id = packet_info.packet_id;
  packet->payload = packet_alloc->alloc(packet_alloc, packet_info.payload_size);

  packet_info.decode_func(packet, packet_bytes, packet_decode_ctx);

  return true;
}

bool tt_packet_queue_pop(tt_packet_info_array_t *packet_infos, tt_byte_buf_t *packet_queue,
                         pthread_mutex_t *packet_queue_mutex,
                         tt_packet_t *out_packet, allocator_t *packet_alloc,
                         void *decode_ctx) {

  bool success = false;
  pthread_mutex_lock(packet_queue_mutex);

  if (deque_len(packet_queue) > 0) {
    tt_byte_buf_t *bytes = deque_pop_front(packet_queue);
    if (bytes != NULL &&
        tt_packet_decode(packet_infos, bytes, out_packet, packet_alloc, decode_ctx)) {
      success = true;
    } else {
      log_error("Failed to decode packet");
      success = false;
    }

    tt_byte_buf_deinit(bytes);
  }

  pthread_mutex_unlock(packet_queue_mutex);

  return success;
}
