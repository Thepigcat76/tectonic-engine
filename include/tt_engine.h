#pragma once

#include "tt_backend.h"
#include "tt_shared.h"
#include "lilc/alloc.h"
#include "lilc/bump.h"
#include "lilsockets.h"
#include <pthread.h>

typedef struct tt_entity_manager tt_entity_manager_t;

tt_entity_t tt_entity_make(tt_entity_manager_t *entities);

// -- ASSETS --

typedef struct tt_texture_meta tt_texture_meta_t;

typedef struct tt_asset_texture {
  const char *path;
  void *tex_data;
  tt_texture_meta_t *meta;
} tt_asset_texture_t;

typedef struct tt_sound_meta tt_sound_meta_t;

typedef struct tt_asset_sound {
  const char *path;
  void *sound_data;
  tt_sound_meta_t *meta;
} tt_asset_sound_t;

typedef struct tt_shader_meta tt_shader_meta_t;

typedef struct tt_asset_shader {
  const char *path;
  void *shader_data;
  tt_shader_meta_t *meta;
} tt_asset_shader_t;

typedef struct tt_music_meta tt_music_meta_t;

typedef struct tt_asset_music_t {
  const char *path;
  void *music_data;
  tt_music_meta_t *meta;
} tt_asset_music_t;

typedef struct tt_asset_handle {
  tt_asset_id_t asset_id;
  tt_asset_category_t asset_category;
  const char *path;
  void *asset;
} tt_asset_handle_t;

#ifndef _CUSTOM_ASSET_MANAGER_IMPL
struct tt_asset_manager {
  const char *assets_path;

  tt_asset_texture_t *textures;
  tt_asset_sound_t *sounds;
  tt_asset_shader_t *shaders;
  tt_asset_music_t *musics;

  Bump asset_bump;
  Allocator asset_allocator;

  tt_asset_handle_t *asset_handles;

  Bump asset_handle_bump;
  Allocator asset_handle_allocator;

  tt_backend_t *backend;
};
#endif

void tt_assets_load(tt_asset_manager_t *assets);

// Can only be called after tt_assets_load
tt_asset_id_t tt_asset_bind(tt_asset_manager_t *assets, const char *asset_path);

void tt_assets_unload(tt_asset_manager_t *assets, bool deinit);

void *tt_asset_by_id(tt_asset_manager_t *assets, tt_asset_id_t asset_id);

// -- NETWORKING --

typedef struct tt_byte_buf {
  u8 *bytes;
  usz write_idx;
  usz read_idx;
} tt_byte_buf_t;

typedef enum tt_packet_dir {
  TT_PACKET_DIR_TWO_WAY,
  TT_PACKET_DIR_CLIENT,
  TT_PACKET_DIR_SERVER,
} tt_packet_dir_e;

typedef struct tt_packet tt_packet_t;

typedef struct tt_packet_context {
  tt_entity_manager_t *entities;
} tt_packet_context_t;

typedef struct tt_packet_handle_context {
  tt_entity_manager_t *entities;
  tt_entity_t *receivers;
} tt_packet_handle_context_t;

typedef void (*tt_packet_encode_func_t)(const tt_packet_t *, tt_byte_buf_t *, tt_packet_context_t);

typedef void (*tt_packet_decode_func_t)(tt_packet_t *, tt_byte_buf_t *, tt_packet_context_t);

typedef void (*tt_packet_handle_func_t)(tt_packet_t *, tt_packet_handle_context_t);


// EXPERIMENTAL
typedef enum tt_field_type {
  TT_FIELD_TY_U8,
  TT_FIELD_TY_U16,
  TT_FIELD_TY_U32,
  TT_FIELD_TY_U64,
  TT_FIELD_TY_I8,
  TT_FIELD_TY_I16,
  TT_FIELD_TY_I32,
  TT_FIELD_TY_I64,
} tt_field_type_e;
#define TT_MAX_STRUCT_FIELDS 32
typedef struct tt_packet_struct_field {
  const char *name;
  i32 size;
  i32 offset;
  tt_field_type_e field_ty;
} tt_packet_struct_field_t;
typedef struct tt_struct_info {
  tt_packet_struct_field_t fields[TT_MAX_STRUCT_FIELDS];
  usz fields_count;
} tt_struct_info_tt;

typedef struct tt_packet_info {
  tt_packet_encode_func_t encode_func;
  tt_packet_decode_func_t decode_func;
  tt_packet_handle_func_t handle_func;
  tt_packet_dir_e packet_dir;
  usz payload_size;
  tt_struct_info_tt struct_info;
} tt_packet_info_t;

typedef struct tt_packet {
  tt_packet_id_t packet_id;
  void *payload;
} tt_packet_t;

typedef struct tt_client_desc {
  addr_t address;
} tt_client_desc_t;

typedef struct tt_server tt_server_t;

#ifndef _CUSTOM_SERVER_IMPL
struct tt_server {
  tt_client_desc_t *connected_clients;

  tt_packet_t *packet_queue;
  pthread_mutex_t packet_queue_mutex;
};
#endif

// -- ENGINE --

// -- CLIENT-ENGINE --

typedef struct tt_engine_client {
  ttb_event_array_t events;
  tt_backend_t *backend;

  tt_asset_manager_t asset_manager;

  Bump event_bump;
  Allocator event_alloc;
} tt_engine_client_t;

typedef struct tt_engine_init {
  i32 window_width;
  i32 window_height;
  const char *window_title;
  bool resizeable;
} tt_engine_init_t;

void tt_engine_client_init(tt_engine_client_t *engine,
                           tt_engine_init_t init_state);

void tt_engine_client_deinit(tt_engine_client_t *engine);

void tt_engine_client_update(tt_engine_client_t *engine);

// -- SERVER-ENGINE --

typedef struct tt_engine_server {
  ttb_event_array_t events;
  tt_backend_t *backend;


} tt_engine_server_t;
