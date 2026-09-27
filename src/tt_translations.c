#include "../include/tt_translation.h"
#include "lilc/alloc.h"
#include "lilc/eq.h"
#include "lilc/hash.h"
#include "lilc/panic.h"

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

void tt_translation_load(tt_translation_manager_t *manager, char *locale,
                         const char *filepath) {
  panic("Unimplemented");
}

void tt_translation_save(tt_translation_manager_t *manager, char *locale,
                         tt_translations_t translations, const char *filepath) {
  panic("Unimplemented");
}

char *tt_translations_lookup(tt_translation_manager_t *manager, char *key) {
  tt_translations_t *translations =
      hashmap_value(&manager->translations, &manager->selected_locale);
  if (translations != NULL) {
    return hashmap_value(translations, &key);
  }

  return NULL;
}
