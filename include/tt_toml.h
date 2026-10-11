#pragma once

#include "../vendor/toml.h"

typedef void (*tt_handle_toml_entry_t)(const char *key,
                                       const toml_datum_t *entry, void *args);

void tt_toml_table_visit_entries(const char *key0, const toml_datum_t *tab_val,
                                 tt_handle_toml_entry_t handle_table_fn,
                                 void *args, bool entry_is_table);

char *tt_toml_type_to_string(toml_type_t type);
