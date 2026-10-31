#include "../include/tt_packets.h"
#include "../include/tt_engine.h"

#include "lilc/log.h"

#include <lilc/deque.h>
#include <stdio.h>

void tt_packet_queue_log(const tt_packet_queue_t *packet_queue) {
  if (deque_len(*packet_queue) > 0) {
    log_info("Packet Queue: ");
    struct tt_packet_queue_entry *entry;
    deque_foreach(*packet_queue, entry) {
      // Local copy of bytebuf so we can peek at the id
      tt_byte_buf_t entry_buf = entry->packet_bytes;
      tt_packet_id_t packet_id = tt_decode_i32(&entry_buf, NULL);
      printf("Packet: %zu\n", packet_id);
    }
  }
}

void tt_packet_info_add(tt_packet_info_array_t *packet_infos,
                        tt_packet_info_t packet_info) {
  if (packet_infos->locked)
    return;

  array_add(packet_infos->infos, packet_info);
}

void tt_packet_info_lock(tt_packet_info_array_t *packet_infos) {
  packet_infos->locked = true;
}
