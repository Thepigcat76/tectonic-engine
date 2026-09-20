#pragma once

#include "../include/tt_engine.h"
#include "lilc/array.h"
#include "lilc/numbers.h"
#include <stddef.h>

#define TTN_ST_TABLE(ty) _TTN_LIB_ST_TABLE(ty)

typedef u8 payload_none;

typedef struct {
  i64 time;

  #define PACKET_PING_INFO { .direction = TT_PACKET_DIR_TWO_WAY }
  #define PACKET_PING_FIELDS(_F, ...) \
    _F(time, __VA_ARGS__)
} payload_ping;

typedef struct {
  u32 client_id;
} payload_handshake;

typedef struct {

} payload_disconnect;

#define PACKETS_ITER(_F, ...)                                                  \
  _F(PACKET_PING,       payload_ping,       __VA_ARGS__)                       \
  //_F(PACKET_HANDSHAKE,  payload_handshake,  __VA_ARGS__)                       
  //_F(PACKET_DISCONNECT, payload_disconnect, __VA_ARGS__)                       
  //_F(PACKET_NONE,       payload_none,       __VA_ARGS__)      

enum custom_packets {
  #define PACKET_DECL_ENUM(id, ...) id,
  PACKETS_ITER(PACKET_DECL_ENUM)
  _amount_custom_packets,
};

#define _TYPE_OF_FIELD(_struct, _field_name) typeof(((_struct *) NULL)->_field_name)

tt_packet_info_t packet_infos[_amount_custom_packets] = {
#define PACKET_MAKE_FIELD(_name, type, ...) {.name = #_name, .size = sizeof((*(type *) NULL)._name), .offset = offsetof(type, _name), .encode_func = TTN_ST_TABLE(_TYPE_OF_FIELD(type, _name)).encode_func, .decode_func = TTN_ST_TABLE(_TYPE_OF_FIELD(type, _name)).decode_func},
#define PACKET_FIELDS_COUNTER(...) 1 + 
#define PACKET_MAKE_INFO(id, type, ...) [id] = {.payload_size = sizeof(type), .struct_info = {.fields_count = id##_FIELDS(PACKET_FIELDS_COUNTER) 0, .fields = {\
  id##_FIELDS(PACKET_MAKE_FIELD, type)\
}}},

  PACKETS_ITER(PACKET_MAKE_INFO)
};

void test() {
  // Custom encode/decode function can be provided in PACKET_name_INFO, if that is NULL, we use the default based on fields

  #define PACKET_SIZE(id, type, ...) do {         \
    tt_packet_info_t info = {.payload_size = sizeof(type)};  \
  } while (0);

  PACKETS_ITER(PACKET_SIZE)
}