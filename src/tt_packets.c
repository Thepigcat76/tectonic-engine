#include "../include/tt_packets.h"
#include "../include/tt_engine.h"

void tt_packet_info_add(tt_packet_info_array_t *packet_infos, tt_packet_info_t packet_info) {
  if (packet_infos->locked) return;

  array_add(packet_infos->infos, packet_info);
}

void tt_packet_info_lock(tt_packet_info_array_t *packet_infos) {
  packet_infos->locked = true;
}
