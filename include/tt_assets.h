#pragma once

#include "tt_shared.h"
#include "lilc/alloc.h"
#include "lilc/dynstr.h"
#include "lilc/array.h"

// -- ASSETS --

typedef enum {
  TT_SCALING_DEFAULT,
  TT_SCALING_NINE_SLICE,
} tt_scaling_kind_e;

typedef struct tt_texture_meta {
  struct tt_animation {
    bool enabled;
    u32 frame_height;
    u32 frame_time;
    array_t(u32) frames;
  } animation;
  struct tt_variants {
    bool enabled;
    array_t(const char *) variants;
  } variants;
  struct tt_tiling {
    bool enabled;
    const char *fallback_tex;
  } tiling;
  struct tt_scaling {
    bool enabled;
    tt_scaling_kind_e kind;
    union tt_scaling_variant {
      struct tt_scaling_nine_slice {
        u32 border_width;
      } nine_slice;
    } u;
  } scaling;
} tt_texture_meta_t;

typedef struct tt_asset_texture {
  dyn_string_t path;
  void *tex_data;
  tt_texture_meta_t *meta;
} tt_asset_texture_t;

typedef struct tt_sound_meta tt_sound_meta_t;

typedef struct tt_asset_sound {
  dyn_string_t path;
  void *sound_data;
  tt_sound_meta_t *meta;
} tt_asset_sound_t;

typedef struct tt_shader_meta tt_shader_meta_t;

typedef struct tt_asset_shader {
  dyn_string_t path;
  void *shader_data;
  tt_shader_meta_t *meta;
} tt_asset_shader_t;

typedef struct tt_music_meta tt_music_meta_t;

typedef struct tt_asset_music_t {
  dyn_string_t path;
  void *music_data;
  tt_music_meta_t *meta;
} tt_asset_music_t;

typedef struct tt_asset_handle {
  tt_asset_id_t asset_id;
  tt_asset_category_t asset_category;
  dyn_string_t path;
  void *asset;
  bool none;
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
#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 2, 3)))
#endif
tt_asset_id_t tt_asset_bind(tt_asset_manager_t *assets, const char *asset_path_fmt, ...);

void tt_assets_unload(tt_asset_manager_t *assets, bool deinit);

void *tt_asset_by_id(tt_asset_manager_t *assets, tt_asset_id_t asset_id);
