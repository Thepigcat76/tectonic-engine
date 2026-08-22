#include "../include/tt_engine.h"
#include "lilc/alloc.h"
#include "lilc/array.h"
#include "lilc/eq.h"
#include "lilc/file.h"
#include "lilc/hash.h"
#include "lilc/hashset.h"
#include "lilc/log.h"
#include "lilc/panic.h"
#include "lilc/str.h"
#include <dirent.h>
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
  const char *path;
  const char *name;
  const char *file_ext;
};

static void tt_walk_assets_dir(
    tt_asset_manager_t *assets, const char *path, hashset_t *visited_files,
    void (*visit_func)(tt_asset_manager_t *, hashset_t *, struct file_entry)) {
  struct dirent *entry;
  DIR *dp = opendir(path);
  if (dp == NULL) {
    perror("opendir");
    log_error("Path: %s\n", path);
    exit(1);
  }

  while ((entry = readdir(dp)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;

    char dir_buf[256];
    sprintf(dir_buf, "%s/%s", path, entry->d_name);
    if (entry->d_type == DT_DIR) {
      tt_walk_assets_dir(assets, dir_buf, visited_files, visit_func);
      continue;
    } else {
      const char *dot = strrchr(entry->d_name, '.');
      const char *file_ext = NULL;
      if (dot != NULL) {
        file_ext = dot + 1;
      }
      struct file_entry file_entry = {
          .path = dir_buf,
          .name = entry->d_name,
          .file_ext = file_ext,
      };
      visit_func(assets, visited_files, file_entry);
    }
  }

  closedir(dp);
}

static void tt_asset_visit(tt_asset_manager_t *assets, hashset_t *visited_files,
                           struct file_entry entry) {
  hashset_insert(visited_files, &entry.path);

  if (str_eq(entry.file_ext, "png")) {
    void *tex_data = ttb_asset_load(assets->backend, &assets->asset_allocator,
                                    TT_ASSET_TEXTURE, entry.path);
    tt_asset_texture_t asset_tex = {
        .path = str_dup(entry.path, &assets->asset_allocator),
        .meta = NULL,
        .tex_data = tex_data,
    };

    array_add(assets->textures, asset_tex);
  }
}

static tt_asset_id_t missing_tex = 0;

static void tt_asset_rebind(tt_asset_manager_t *assets, const char *asset_path,
                            tt_asset_handle_t *asset_handle) {
  if (assets->asset_handles == NULL) {
    panic("Assets need to be loaded/engine needs to be initialized before you "
          "can bind assets");
    return;
  }

  asset_handle->path =
      dyn_string_makef(&assets->asset_handle_allocator, "%s/%s",
                       assets->assets_path, asset_path)
          .string;

  switch ((tt_asset_category_t)asset_handle->asset_category) {
  case TT_ASSET_TEXTURE: {
    bool found_tex = false;
    tt_asset_texture_t *tex;
    array_foreach(assets->textures, tex) {
      log_debug("Tex path: %s, asset path: %s",
                str_fmt_temp("%s/%s", assets->assets_path, tex->path),
                asset_path);
      if (str_eq(str_fmt_temp("%s/%s", assets->assets_path, asset_path),
                 tex->path)) {
        asset_handle->asset = tex;
        found_tex = true;
        break;
      }
    }
    if (!found_tex) {
      log_error("[ASSETS] Missing texture for %s", asset_path);
      asset_handle->asset = tt_asset_by_id(assets, missing_tex);
    }
  } break;
  case TT_ASSET_SOUND: {
    tt_asset_sound_t *sound;
    array_foreach(assets->sounds, sound) {
      if (str_eq(sound->path, asset_path)) {
        asset_handle->asset = sound;
        break;
      }
    }
  } break;
  case TT_ASSET_SHADER: {
    tt_asset_shader_t *shader;
    array_foreach(assets->shaders, shader) {
      if (str_eq(shader->path, asset_path)) {
        asset_handle->asset = shader;
        break;
      }
    }
  } break;
  case TT_ASSET_MUSIC: {
    tt_asset_music_t *music;
    array_foreach(assets->musics, music) {
      if (str_eq(music->path, asset_path)) {
        asset_handle->asset = music;
        break;
      }
    }
  } break;
  }
}

void tt_assets_load(tt_asset_manager_t *assets) {
  bool initialize = assets->asset_handles == NULL;

  if (initialize)
    tt_asset_manager_init(assets);

  if (assets->assets_path == NULL) {
    log_error("[ASSETS] No asset path provided");
    return;
  }

  DIR *dir = opendir(assets->assets_path);
  if (dir == NULL) {
    log_error("[ASSETS] Failed to find assets directory");
    return;
  }

  hashset_t visited_files = {0};
  hashset_init(&visited_files, &assets->asset_allocator, char *, str_ptrv_hash,
               str_ptrv_eq);

  tt_walk_assets_dir(assets, assets->assets_path, &visited_files,
                     tt_asset_visit);

  if (initialize) {
    missing_tex = tt_asset_bind(assets, "tex/errtex.png");
  } else {
    tt_asset_handle_t *asset_handle;
    array_foreach(assets->asset_handles, asset_handle) {
      tt_asset_rebind(assets, asset_handle->path, asset_handle);
    }
  }
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
                            const char *asset_path) {
  i32 asset_category = tt_category_by_file_ext(file_extension(asset_path));
  if (asset_category == -1) {
    panic("Invalid file extension for binding asset");
    return 0;
  }

  tt_asset_id_t asset_id = array_len(assets->asset_handles);
  tt_asset_handle_t asset_handle = {
      .asset_id = asset_id,
      .asset_category = asset_category,
  };

  tt_asset_rebind(assets, asset_path, &asset_handle);

  array_add(assets->asset_handles, asset_handle);

  log_debug("Adding handle");
  return asset_id;
}

void *tt_asset_by_id(tt_asset_manager_t *assets, tt_asset_id_t asset_id) {
  if (array_len(assets->asset_handles) <= asset_id) {
    return panic("Failed to get asset (index out of bounds)");
  }
  return assets->asset_handles[asset_id].asset;
}
