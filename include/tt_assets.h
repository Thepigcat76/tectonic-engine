#pragma once

#include "tt_shared.h"
#include "tt_backend.h"
#include "lilc/alloc.h"

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

  bump_t asset_bump;
  allocator_t asset_allocator;

  tt_asset_handle_t *asset_handles;

  bump_t asset_handle_bump;
  allocator_t asset_handle_allocator;

  tt_backend_t *backend;
};
#endif

void tt_assets_load(tt_asset_manager_t *assets);

// Can only be called after tt_assets_load
tt_asset_id_t tt_asset_bind(tt_asset_manager_t *assets, const char *asset_path);

void tt_assets_unload(tt_asset_manager_t *assets, bool deinit);

void *tt_asset_by_id(tt_asset_manager_t *assets, tt_asset_id_t asset_id);
