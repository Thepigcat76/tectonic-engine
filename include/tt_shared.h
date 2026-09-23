#pragma once

#include "lilc/numbers.h"

#ifndef TT_ENTTIY_TYPE
#define TT_ENTITY_TYPE u64
#endif

#ifndef TT_ASSET_ID_TYPE
#define TT_ASSET_ID_TYPE u64
#endif

#ifndef TT_PACKET_ID_TYPE
#define TT_PACKET_ID_TYPE i64
#endif

#ifndef TT_SUBPROC_ID_TYPE
#define TT_SUBPROC_ID_TYPE u64
#endif

typedef TT_ASSET_ID_TYPE tt_asset_id_t;

typedef TT_PACKET_ID_TYPE tt_packet_id_t;

typedef u64 tt_packet_handle_t;

typedef TT_SUBPROC_ID_TYPE tt_subproc_id_t;

typedef struct tt_rect {
  i32 x;
  i32 y;
  i32 width;
  i32 height;
} tt_rect_t;

typedef enum tt_asset_category {
  TT_ASSET_TEXTURE,
  TT_ASSET_SOUND,
  TT_ASSET_SHADER,
  TT_ASSET_MUSIC,
} tt_asset_category_t;

typedef struct tt_asset_manager tt_asset_manager_t;

typedef struct tt_entity_manager tt_entity_manager_t;

void tt_wait(i32 millis);
