#include "../include/tt_toml.h"
#include "lilc/dynstr.h"
#include "lilc/log.h"
#include "lilc/numbers.h"

void tt_toml_table_visit_entries(const char *key0, const toml_datum_t *tab_val,
                                 tt_handle_toml_entry_t handle_table_fn,
                                 void *args, bool entry_is_table) {
  if (tab_val->type == TOML_TABLE) {
    for (i32 ientry = 0; ientry < tab_val->u.tab.size; ientry++) {
      dyn_string_t tmp = {0};
      dyn_string_init(&tmp, &HEAP_ALLOCATOR);

      const char *key = tab_val->u.tab.key[ientry];
      i32 key_len = tab_val->u.tab.len[ientry];
      toml_datum_t *val = &tab_val->u.tab.value[ientry];

      if (key_len >= 512) {
        log_warn("Key is longer than 512 characters, skipping");
        continue;
      }

      if (key0 != NULL) {
        dyn_string_printf(&tmp, "%s.%.*s", key0, key_len, key);
      } else {
        dyn_string_printf(&tmp, "%.*s", key_len, key);
      }

      bool entry_is_last_table = false;
      if (entry_is_table && val->type == TOML_TABLE && val->u.tab.size > 0) {
        for (i32 ientry = 0; ientry < val->u.tab.size; ientry++) {
          if (val->u.tab.value[ientry].type != TOML_TABLE) {
            entry_is_last_table = true;
            break;
          }
        }
      }

      if (!entry_is_last_table) {
        tt_toml_table_visit_entries(tmp.string, val, handle_table_fn, args,
                                    entry_is_table);
      } else {
        handle_table_fn(tmp.string, val, args);
      }

      dyn_string_free(&tmp);
    }
  } else {
    handle_table_fn(key0, tab_val, args);
  }
}

char *tt_toml_type_to_string(toml_type_t type) {
  switch (type) {
  case TOML_UNKNOWN: {
    return "TOML_UNKNOWN";
  } break;
  case TOML_STRING: {
    return "TOML_STRING";
  } break;
  case TOML_INT64: {
    return "TOML_INT64";
  } break;
  case TOML_FP64: {
    return "TOML_FP64";
  } break;
  case TOML_BOOLEAN: {
    return "TOML_BOOLEAN";
  } break;
  case TOML_DATE: {
    return "TOML_DATE";
  } break;
  case TOML_TIME: {
    return "TOML_TIME";
  } break;
  case TOML_DATETIME: {
    return "TOML_DATETIME";
  } break;
  case TOML_DATETIMETZ: {
    return "TOML_DATETIMETZ";
  } break;
  case TOML_ARRAY: {
    return "TOML_ARRAY";
  } break;
  case TOML_TABLE: {
    return "TOML_TABLE";
  } break;
  }
}
