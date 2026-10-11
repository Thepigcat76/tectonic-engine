#include "../include/tt_translation.h"
#include "../include/tt_toml.h"
#include "../vendor/toml.h"
#include "lilc/alloc.h"
#include "lilc/dynstr.h"
#include "lilc/eq.h"
#include "lilc/file.h"
#include "lilc/hash.h"
#include "lilc/log.h"
#include "lilc/panic.h"
#include "lilc/str.h"
#include <stdio.h>

void tt_translation_manager_init(tt_translation_manager_t *manager,
                                 char *locale, allocator_t *alloc) {
  manager->default_locale = locale;
  manager->selected_locale = locale;

  hashmap_init(&manager->translations, alloc, char *, tt_translations_t,
               str_ptrv_hash, str_ptrv_eq, NULL);

  bump_init(&manager->translation_bump, 128000);
  bump_allocator_init(&manager->translation_alloc, &manager->translation_bump);
}

void tt_translation_manager_deinit(tt_translation_manager_t *manager) {
  hashmap_deinit(&manager->translations);
  bump_free(&manager->translation_bump);
}

struct tt_toml_translation_args {
  tt_translations_t *translations;
  allocator_t *translation_alloc;
};

static void tt_translation_entry_visit(const char *key,
                                       const toml_datum_t *entry, void *args) {
  struct tt_toml_translation_args *translations = args;

  if (entry->type != TOML_STRING) {
    log_warn("Entry %s in translation file not of type string", key);
    return;
  }

  const char *key_cpy = str_dup(key, translations->translation_alloc);
  const char *translation_val = entry->u.str.ptr;
  usz val_len = entry->u.str.len;
  translation_val =
      str_dup_len(translation_val, val_len, translations->translation_alloc);

  hashmap_insert(translations->translations, &key_cpy, &translation_val);

  log_info("Loaded Translation: %s => %s", key_cpy, translation_val);
}

void tt_translation_load(tt_translation_manager_t *manager, char *locale,
                         const char *filepath) {
  toml_result_t res = toml_parse_file_ex(filepath);
  if (!res.ok) {
    log_error("Failed to load translation file at %s", filepath);
    return;
  }

  tt_translations_t translations = {0};
  hashmap_init(&translations, &manager->translation_alloc, char *, char *,
               str_ptrv_hash, str_ptrv_eq, NULL);

  const toml_datum_t *tab = &res.toptab;

  struct tt_toml_translation_args args = {
      .translations = &translations,
      .translation_alloc = &manager->translation_alloc,
  };

  tt_toml_table_visit_entries(NULL, tab, tt_translation_entry_visit, &args, false);

  for (i32 ientry = 0; ientry < tab->u.tab.size; ientry++) {
    const char *key = tab->u.tab.key[ientry];
    i32 key_len = tab->u.tab.len[ientry];
    const toml_datum_t *val = &tab->u.tab.value[ientry];

    key = str_dup_len(key, key_len, &manager->translation_alloc);

    if (val->type != TOML_STRING) {
      log_warn(
          "Value in translation file %s for key '%.*s' not a string but %d",
          filepath, key_len, key, val->type);
      continue;
    }

    const char *translation_val = val->u.str.ptr;
    usz val_len = val->u.str.len;
    translation_val =
        str_dup_len(translation_val, val_len, &manager->translation_alloc);

    hashmap_insert(&translations, &key, &translation_val);
  }

  hashmap_insert(&manager->translations, &locale, &translations);

  log_info("Loaded file %s with %zu translations, size: %d", filepath,
           translations.len, tab->u.tab.size);
}

void tt_translation_save(tt_translation_manager_t *manager, char *locale,
                         const char *filepath) {
  tt_translations_t *translations =
      hashmap_value(&manager->translations, &locale);
  if (translations == NULL) {
    log_warn("Tried saving an empty translation");
    return;
  }

  toml_datum_t datum = mkdatum(TOML_TABLE);

  char *errbuf = heap_alloc(200);

  char **key;
  char **val;
  hashmap_foreach(translations, key, val) {
    toml_datum_t *toml_val =
        toml_tab_emplace(&datum, *key, (const char **)&errbuf);
    toml_val->type = TOML_STRING;
    toml_val->u.str.len = strlen(*val);
    toml_val->u.str.ptr = *val;
  }

  usz len = toml_print(&datum, NULL);
  char file_buf[len + 1];
  toml_print(&datum, file_buf);

  FILE *f = fopen(filepath, "w");
  if (f == NULL) {
    log_error("Failed to open file: %s", filepath);
    return;
  }

  fputs(file_buf, f);

  fclose(f);

  heap_dealloc(errbuf);

  log_info("Saved translation file %s with translations: %zu", filepath,
           translations->len);
}

void tt_translation_insert(tt_translation_manager_t *manager, char *locale,
                           char *key, char *val) {
  tt_translations_t *translations =
      hashmap_value(&manager->translations, &manager->selected_locale);

  bool new_map = false;
  if (translations == NULL) {
    tt_translations_t translations0 = {0};
    hashmap_init(&translations0, &manager->translation_alloc, char *, char *,
                 str_ptrv_hash, str_ptrv_eq, NULL);
    translations = &translations0;
    new_map = true;
  }

  hashmap_insert(translations, &key, &val);

  if (new_map) {
    hashmap_insert(&manager->translations, &locale, translations);
  }
}

char *tt_translations_lookup(tt_translation_manager_t *manager, char *key) {
  tt_translations_t *translations =
      hashmap_value(&manager->translations, &manager->selected_locale);
  if (translations != NULL) {
    char **val = hashmap_value(translations, &key);
    return val == NULL ? key : *val;
  }

  return key;
}
