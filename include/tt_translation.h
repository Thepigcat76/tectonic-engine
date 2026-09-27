#pragma once

#include "lilc/hashmap.h"

/* char * -> char * */
typedef hashmap_t tt_translations_t; 

typedef struct tt_translation_manager {
  char *default_locale;
  char *selected_locale;
  hashmap_t translations; /* char * -> tt_translations_t */

  bump_t translation_bump;
  allocator_t translation_alloc;
} tt_translation_manager_t;

void tt_translation_manager_init(tt_translation_manager_t *manager, char *locale, allocator_t *alloc);

void tt_translation_manager_deinit(tt_translation_manager_t *manager);

void tt_translation_load(tt_translation_manager_t *manager, char *locale, const char *filepath);

void tt_translation_save(tt_translation_manager_t *manager, char *locale, tt_translations_t translations, const char *filepath);

char *tt_translations_lookup(tt_translation_manager_t *manager, char *key);
