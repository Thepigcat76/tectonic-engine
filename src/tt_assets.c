#include "../include/tt_assets.h"
#include "../include/tt_backend.h"
#include "../include/tt_shared.h"
#include "../include/tt_toml.h"
#include "../vendor/toml.h"
#include "lilc/alloc.h"
#include "lilc/array.h"
#include "lilc/dir.h"
#include "lilc/dynstr.h"
#include "lilc/eq.h"
#include "lilc/file.h"
#include "lilc/hash.h"
#include "lilc/hashset.h"
#include "lilc/log.h"
#include "lilc/panic.h"
#include "lilc/path.h"
#include "lilc/str.h"
#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>

static inline void tt_asset_manager_init(tt_asset_manager_t *asset_manager) {
  bump_init(&asset_manager->asset_bump, 128000);
  bump_allocator_init(&asset_manager->asset_allocator,
                      &asset_manager->asset_bump);

  asset_manager->musics =
      array_new(tt_asset_music_t, &asset_manager->asset_allocator);
  asset_manager->shaders =
      array_new(tt_asset_shader_t, &asset_manager->asset_allocator);
  asset_manager->sounds =
      array_new(tt_asset_sound_t, &asset_manager->asset_allocator);
  asset_manager->textures =
      array_new(tt_asset_texture_t, &asset_manager->asset_allocator);

  bump_init(&asset_manager->asset_handle_bump, 16000);
  bump_allocator_init(&asset_manager->asset_handle_allocator,
                      &asset_manager->asset_handle_bump);

  asset_manager->asset_handles =
      array_new(tt_asset_handle_t, &asset_manager->asset_handle_allocator);
}

struct file_entry {
  dyn_string_t path;
  const char *name;
  const char *file_ext;
};

static void tt_walk_assets_dir(
    tt_asset_manager_t *assets, dyn_string_t path, hashset_t *visited_files,
    void (*visit_func)(tt_asset_manager_t *, hashset_t *, struct file_entry)) {
  struct dirent *entry;
  DIR *dp = opendir(path.string);
  if (dp == NULL) {
    perror("opendir");
    log_error("Path: %s\n", path.string);
    exit(1);
  }

  while ((entry = readdir(dp)) != NULL) {
    if (str_eq(entry->d_name, ".") || str_eq(entry->d_name, ".."))
      continue;

    dyn_string_t path0 = {0};
    dyn_string_init(&path0, &HEAP_ALLOCATOR);

    dyn_string_printf(&path0, "%s/%s", path.string, entry->d_name);
    file_path_resolve(&path0);

    if (entry->d_type == DT_DIR) {
      tt_walk_assets_dir(assets, path0, visited_files, visit_func);
      continue;
    } else {
      const char *dot = strrchr(entry->d_name, '.');
      const char *file_ext = NULL;
      if (dot != NULL) {
        file_ext = dot + 1;
      }
      struct file_entry file_entry = {
          .path = path0,
          .name = entry->d_name,
          .file_ext = file_ext,
      };
      // TODO: Use visited files and dealloc file_path_lit in visit func or use
      // bump for it
      visit_func(assets, visited_files, file_entry);
    }

    dyn_string_free(&path0);
  }

  closedir(dp);
}

static bool tt_texture_meta_load(tt_texture_meta_t *meta,
                                 const dyn_string_t *filepath,
                                 allocator_t *alloc) {
  toml_result_t result = toml_parse_file_ex(filepath->string);
  if (!result.ok) {
    log_debug("Failed to parse meta file");
    return false;
  }

  toml_datum_t animation = toml_get(result.toptab, "animation");
  if (animation.type == TOML_TABLE) {
    toml_datum_t enabled = toml_get(animation, "enabled");
    if (enabled.type == TOML_BOOLEAN) {
      meta->animation.enabled = enabled.u.boolean;
    } else if (enabled.type != TOML_UNKNOWN) {
      log_error("Property 'enabled' in table 'animation' has wrong type: %s, "
                "expected: TOML_BOOLEAN",
                tt_toml_type_to_string(enabled.type));
      return false;
    }

    toml_datum_t frame_height = toml_get(animation, "frame-height");
    if (frame_height.type == TOML_INT64) {
      meta->animation.frame_height = frame_height.u.int64;
    } else if (frame_height.type != TOML_UNKNOWN) {
      log_error(
          "Property 'frame_height' in table 'animation' has wrong type: %s, "
          "expected: TOML_INT64",
          tt_toml_type_to_string(frame_height.type));
      return false;
    }

    toml_datum_t frame_time = toml_get(animation, "frame-height");
    if (frame_time.type == TOML_INT64) {
      meta->animation.frame_time = frame_time.u.int64;
    } else if (frame_time.type != TOML_UNKNOWN) {
      log_error(
          "Property 'frame_time' in table 'animation' has wrong type: %s, "
          "expected: TOML_INT64",
          tt_toml_type_to_string(frame_time.type));
      return false;
    }

    toml_datum_t frames = toml_get(animation, "frames");
    if (frames.type == TOML_ARRAY) {
      array_t(u32) frames_arr = array_new(u32, alloc);
      for (i32 i = 0; i < frames.u.arr.size; i++) {
        toml_datum_t *elem = &frames.u.arr.elem[i];
        if (elem->type != TOML_INT64) {
          log_error("Element at index (%d) in 'frames' array in table "
                    "'animation' has wrong type: %s, "
                    "expected: TOML_INT64",
                    i, tt_toml_type_to_string(elem->type));
          return false;
        }

        array_add(frames_arr, elem->u.int64);
      }
      meta->animation.frames = frames_arr;
    } else if (frame_time.type != TOML_UNKNOWN) {
      log_error(
          "Property 'frame_time' in table 'animation' has wrong type: %s, "
          "expected: TOML_INT64",
          tt_toml_type_to_string(frame_time.type));
      return false;
    }

    log_info("Loaded animation meta");
  } else if (animation.type != TOML_UNKNOWN) {
    log_error("Table 'animation' has wrong type: %s, "
              "expected: TOML_TABLE",
              tt_toml_type_to_string(animation.type));
    return false;
  }

  toml_datum_t scaling = toml_get(result.toptab, "scaling");
  if (scaling.type == TOML_TABLE) {
    toml_datum_t enabled = toml_get(scaling, "enabled");
    if (enabled.type == TOML_BOOLEAN) {
      meta->scaling.enabled = enabled.u.boolean;
    } else if (enabled.type != TOML_UNKNOWN) {
      log_error("Property 'enabled' in table 'scaling' has wrong type: %s, "
                "expected: TOML_BOOLEAN",
                tt_toml_type_to_string(enabled.type));
      return false;
    }

    toml_datum_t kind = toml_get(scaling, "kind");
    meta->scaling.kind = TT_SCALING_DEFAULT;
    if (kind.type == TOML_STRING) {
      if (str_eq_len(kind.u.str.ptr, "default", kind.u.str.len)) {
        meta->scaling.kind = TT_SCALING_DEFAULT;
      } else if (str_eq_len(kind.u.str.ptr, "nine-slice", kind.u.str.len)) {
        meta->scaling.kind = TT_SCALING_NINE_SLICE;
      } else {
        log_error("Property 'kind' in table 'scaling' has invalid value "
                  "(%.*s). Allowed values: [\"default\", \"nine-slice\"]",
                  kind.u.str.len, kind.u.str.ptr);
        return false;
      }
    } else if (kind.type != TOML_UNKNOWN) {
      log_error("Property 'kind' in table 'scaling' has wrong type: %s, "
                "expected: TOML_STRING",
                tt_toml_type_to_string(enabled.type));
      return false;
    }

    switch (meta->scaling.kind) {
    case TT_SCALING_DEFAULT: {
    } break;
    case TT_SCALING_NINE_SLICE: {
      toml_datum_t border_width = toml_get(scaling, "border-width");
      if (border_width.type == TOML_INT64) {
        meta->scaling.u.nine_slice.border_width = border_width.u.int64;
      } else {
        log_error(
            "Property 'border-width' in table 'scaling' has wrong type: %s, "
            "expected: TOML_INT64",
            tt_toml_type_to_string(enabled.type));
        return false;
      }
    } break;
    }

    log_info("Loaded scaling meta");
  } else if (scaling.type != TOML_UNKNOWN) {
    log_error("Table 'scaling' has wrong type: %s, "
              "expected: TOML_TABLE",
              tt_toml_type_to_string(scaling.type));
    return false;
  }

  toml_datum_t variants = toml_get(result.toptab, "variants");
  if (variants.type == TOML_TABLE) {
    toml_datum_t enabled = toml_get(variants, "enabled");
    if (enabled.type == TOML_BOOLEAN) {
      meta->variants.enabled = enabled.u.boolean;
    } else if (enabled.type != TOML_UNKNOWN) {
      log_error("Property 'enabled' in table 'variants' has wrong type: %s, "
                "expected: TOML_BOOLEAN",
                tt_toml_type_to_string(enabled.type));
      return false;
    }

    array_t(const char *) variants_arr_final = array_new(const char *, alloc);

    toml_datum_t variants_arr = toml_get(variants, "variants");
    if (variants_arr.type == TOML_ARRAY) {
      for (i32 i = 0; i < variants_arr.u.arr.size; i++) {
        toml_datum_t *elem = &variants_arr.u.arr.elem[i];
        if (elem->type != TOML_STRING) {
          log_error("Element at index (%d) in 'variants' array in table "
                    "'variants' has wrong type: %s, "
                    "expected: TOML_STRING",
                    i, tt_toml_type_to_string(elem->type));
          return false;
        }

        array_add(variants_arr_final,
                  str_dup_len(elem->u.str.ptr, elem->u.str.len, alloc));
      }
    } else if (variants_arr.type == TOML_UNKNOWN) {
      log_error("Property 'variants' in table 'variants' has wrong type: %s, "
                "expected: TOML_ARRAY",
                tt_toml_type_to_string(variants_arr.type));
      return false;
    }

    meta->variants.variants = variants_arr_final;

    log_info("Loaded variants meta");
  } else if (variants.type != TOML_UNKNOWN) {
    log_error("Table 'variants' has wrong type: %s, "
              "expected: TOML_TABLE",
              tt_toml_type_to_string(variants.type));
    return false;
  }

  toml_datum_t tiling = toml_get(result.toptab, "tiling");
  if (tiling.type == TOML_TABLE) {
    toml_datum_t enabled = toml_get(variants, "enabled");
    if (enabled.type == TOML_BOOLEAN) {
      meta->tiling.enabled = enabled.u.boolean;
    } else if (enabled.type != TOML_UNKNOWN) {
      log_error("Property 'enabled' in table 'tiling' has wrong type: %s, "
                "expected: TOML_BOOLEAN",
                tt_toml_type_to_string(enabled.type));
      return false;
    }

    toml_datum_t fallback_tex = toml_get(variants, "fallback_tex");
    if (fallback_tex.type == TOML_STRING) {
      meta->tiling.fallback_tex =
          str_dup_len(fallback_tex.u.str.ptr, fallback_tex.u.str.len, alloc);
    } else if (fallback_tex.type != TOML_UNKNOWN) {
      log_error("Property 'fallback_tex' in table 'tiling' has wrong type: %s, "
                "expected: TOML_STRING",
                tt_toml_type_to_string(enabled.type));
      return false;
    }

    log_info("Loaded tiling meta");
  } else if (tiling.type != TOML_UNKNOWN) {
    log_error("Table 'tiling' has wrong type: %s, "
              "expected: TOML_TABLE",
              tt_toml_type_to_string(tiling.type));
    return false;
  }

  toml_free(result);

  return true;
}

static void tt_asset_visit(tt_asset_manager_t *assets, hashset_t *visited_files,
                           struct file_entry entry) {
  dyn_string_t file_path_cpy = {0};
  dyn_string_init(&file_path_cpy, &assets->asset_allocator);
  dyn_string_copy(&file_path_cpy, &entry.path);

  hashset_insert(visited_files, &file_path_cpy.string);

  if (str_eq(entry.file_ext, "png")) {
    void *tex_data =
        ttb_asset_load(assets->backend, &assets->asset_allocator,
                       TT_ASSET_TEXTURE, file_path_cpy.string, NULL);

    dyn_string_t meta_file_path = {0};
    dyn_string_init(&meta_file_path, &HEAP_ALLOCATOR);

    dyn_string_copy_str_len(&meta_file_path, entry.path.string,
                            entry.path.len - (sizeof(".png") - 1));

    dyn_string_add_str(&meta_file_path, "_meta.toml");

    file_path_resolve(&meta_file_path);

    tt_texture_meta_t meta = {0};
    if (file_exists(meta_file_path.string)) {
      tt_texture_meta_load(&meta, &meta_file_path, &assets->asset_allocator);
      log_info("Loaded meta file for texture '%s'", entry.path.string);
    }

    dyn_string_free(&meta_file_path);

    tt_asset_texture_t asset_tex = {
        .meta = assets->asset_allocator.alloc(&assets->asset_allocator,
                                              sizeof(tt_texture_meta_t)),
        .tex_data = tex_data,
        .path = file_path_cpy,
    };

    *asset_tex.meta = meta;

    array_add(assets->textures, asset_tex);
  } else if (str_eq(entry.file_ext, "vs") || str_eq(entry.file_ext, "fs")) {
    // Shader creates two different assets that point to the same shader data
    void *prev_shader_data = NULL;
    char *otherpath = str_fmt_temp("%.*s.%s", (i32)file_path_cpy.len - 2,
                                   file_path_cpy.string,
                                   str_eq(entry.file_ext, "vs") ? "fs" : "vs");
    if (hashset_contains(visited_files, &otherpath)) {
      tt_asset_shader_t *shader;
      array_foreach(assets->shaders, shader) {
        if (str_eq(file_path_cpy.string, otherpath)) {
          prev_shader_data = shader->shader_data;
          break;
        }
      }
    }

    void *shader_data =
        ttb_asset_load(assets->backend, &assets->asset_allocator,
                       TT_ASSET_SHADER, file_path_cpy.string, prev_shader_data);

    tt_asset_shader_t asset_shader = {
        .meta = NULL,
        .shader_data = shader_data,
        .path = file_path_cpy,
    };

    array_add(assets->shaders, asset_shader);
  }
}

static tt_asset_id_t missing_tex = 0;

static void tt_asset_rebind(tt_asset_manager_t *assets, dyn_string_t asset_path,
                            tt_asset_handle_t *asset_handle) {
  if (assets->asset_handles == NULL) {
    panic("Assets need to be loaded/engine needs to be initialized before you "
          "can bind assets");
    return;
  }

  asset_handle->path = asset_path;

  dyn_string_t asset_path_cpy = {0};
  dyn_string_init(&asset_path_cpy, &assets->asset_handle_allocator);

  switch ((tt_asset_category_t)asset_handle->asset_category) {
  case TT_ASSET_TEXTURE: {
    bool found_tex = false;
    tt_asset_texture_t *tex;
    array_foreach(assets->textures, tex) {
      dyn_string_printf(&asset_path_cpy, "%s/%s", assets->assets_path,
                        asset_path.string);
      file_path_resolve(&asset_path_cpy);

      if (str_eq(asset_path_cpy.string, tex->path.string)) {
        asset_handle->asset = tex;
        found_tex = true;
        break;
      }
    }
    if (!found_tex) {
      log_error("[ASSETS] Missing texture for %s", asset_path.string);
      asset_handle->asset = tt_asset_by_id(assets, missing_tex);
    }
  } break;
  case TT_ASSET_SOUND: {
    tt_asset_sound_t *sound;
    array_foreach(assets->sounds, sound) {
      if (str_eq(sound->path.string, asset_path.string)) {
        asset_handle->asset = sound;
        break;
      }
    }
  } break;
  case TT_ASSET_SHADER: {
    tt_asset_shader_t *shader;
    array_foreach(assets->shaders, shader) {
      if (str_eq(shader->path.string, asset_path.string)) {
        asset_handle->asset = shader;
        break;
      }
    }
  } break;
  case TT_ASSET_MUSIC: {
    tt_asset_music_t *music;
    array_foreach(assets->musics, music) {
      if (str_eq(music->path.string, asset_path.string)) {
        asset_handle->asset = music;
        break;
      }
    }
  } break;
  }
}

tt_asset_id_t none_tex = 0;

void tt_assets_load(tt_asset_manager_t *assets) {
  bool initialize = assets->asset_handles == NULL;

  if (initialize)
    tt_asset_manager_init(assets);

  if (assets->assets_path == NULL) {
    log_error("[ASSETS] No asset path provided");
    return;
  }

  dyn_string_t assets_path = {0};
  dyn_string_init(&assets_path, &HEAP_ALLOCATOR);
  dyn_string_copy_str(&assets_path, assets->assets_path);
  file_path_resolve(&assets_path);

  if (!dir_exists(assets_path.string)) {
    log_error("[ASSETS] Failed to find assets directory");
    return;
    dyn_string_free(&assets_path);
  }

  hashset_t visited_files = {0};
  hashset_init(&visited_files, &assets->asset_allocator, char *, str_ptrv_hash,
               str_ptrv_eq);

  tt_walk_assets_dir(assets, assets_path, &visited_files, tt_asset_visit);

  if (initialize) {
    none_tex = tt_asset_bind(assets, NULL);
    missing_tex = tt_asset_bind(assets, "tex/errtex.png");
  } else {
    tt_asset_handle_t *asset_handle;
    array_foreach(assets->asset_handles, asset_handle) {
      tt_asset_rebind(assets, asset_handle->path, asset_handle);
    }
  }
  dyn_string_free(&assets_path);
}

static inline void tt_assets_deinit(tt_asset_manager_t *asset_manager) {
  array_clear(asset_manager->asset_handles);

  array_free(asset_manager->asset_handles);

  bump_free(&asset_manager->asset_bump);
  bump_free(&asset_manager->asset_handle_bump);
}

void tt_assets_unload(tt_asset_manager_t *assets, bool deinit) {
  array_clear(assets->musics);
  array_clear(assets->shaders);
  array_clear(assets->sounds);
  array_clear(assets->textures);

  if (deinit) {
    tt_assets_deinit(assets);
  } else {
    bump_reset(&assets->asset_bump);
  }
}

static i32 tt_category_by_file_ext(const char *file_ext) {
  if (str_eq(file_ext, "png"))
    return TT_ASSET_TEXTURE;
  return -1;
}

tt_asset_id_t tt_asset_bind(tt_asset_manager_t *assets,
                            const char *asset_path_fmt, ...) {
  dyn_string_t file_path = {0};
  tt_asset_category_t asset_category = TT_ASSET_TEXTURE;
  bool none = false;

  if (asset_path_fmt != NULL) {
    dyn_string_init(&file_path, &assets->asset_handle_allocator);

    va_list args;
    va_start(args, asset_path_fmt);

    char buf[PATH_MAX + 1];
    usz len = vsnprintf(buf, PATH_MAX + 1, asset_path_fmt, args);
    if (len > PATH_MAX) {
      va_end(args);
      panic("Path longer than PATH_MAX");
      return 0;
    }

    va_end(args);

    dyn_string_copy_str(&file_path, buf);

    if (!file_path_resolve(&file_path)) {
      panic("Failed to parse file path");
      return 0;
    }

    i32 asset_category0 = tt_category_by_file_ext(file_extension(buf));
    if (asset_category0 == -1) {
      panic("Invalid file extension for binding asset");
      return 0;
    }

    asset_category = asset_category0;
  } else {
    none = true;
  }

  tt_asset_id_t asset_id = array_len(assets->asset_handles);
  tt_asset_handle_t asset_handle = {
      .asset_id = asset_id,
      .asset_category = asset_category,
      .none = none,
  };

  if (asset_path_fmt != NULL) {
    tt_asset_rebind(assets, file_path, &asset_handle);

    array_add(assets->asset_handles, asset_handle);
  }

  return asset_id;
}

void *tt_asset_by_id(tt_asset_manager_t *assets, tt_asset_id_t asset_id) {
  if (array_len(assets->asset_handles) <= asset_id) {
    return panic("Failed to get asset %zu (index out of bounds)", asset_id);
  }
  return assets->asset_handles[asset_id].asset;
}
