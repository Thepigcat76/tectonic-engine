#include "../include/tt_keys.h"
#include "../include/tt_engine.h"
#include "../vendor/toml.h"
#include "lilc/alloc.h"
#include "lilc/log.h"
#include "lilc/panic.h"
#include "lilc/str.h"
#include <ctype.h>

void tt_keymap_init(tt_keymap_t *keymap, allocator_t *alloc) {
  keymap->entries = array_new(tt_keymap_entry_t, alloc);
}

void tt_keymap_deinit(tt_keymap_t *keymap) { array_free(keymap->entries); }

void tt_keymap_load(tt_keymap_t *keymap, const char *filepath) {
  panic("Unimplemented");
}

static bool tt_toml_write_key(toml_datum_t *tab, char *key, tt_button_kind_e kind,
                              ttb_mouse_button_e mouse,
                              ttb_keyboard_key_t keyboard, bump_t *bump, const char **errbuf) {
  toml_datum_t *key_entry = toml_tab_emplace(tab, key, errbuf);
  if (key_entry == NULL) {
    return false;
  }
  char *keybuf = bump_alloc(bump, 512);
  if (kind == TT_BUTTON_KEYBOARD) {
    char *tmp = str_fmt_temp("%s", ttb_key_to_string(keyboard) + 4);
    log_debug("tmp: %s", tmp);
    for (usz i = 0; i < strlen(tmp); i++) {
      tmp[i] = tolower(tmp[i]);
    }
    sprintf(keybuf, "keyboard_%s", tmp);
  } else {

  }

  key_entry->type = TOML_STRING;
  key_entry->u.str.ptr = keybuf;
  key_entry->u.str.len = strlen(keybuf);

  return true;
}

void tt_keymap_save(const tt_keymap_t *keymap, const char *filepath) {
  bump_t toml_bump = {0};
  bump_init(&toml_bump, 16000);
  allocator_t toml_alloc = {0};
  bump_allocator_init(&toml_alloc, &toml_bump);

  toml_datum_t toml = mkdatum(TOML_TABLE);

  const char *errbuf = bump_alloc(&toml_bump, 256);

  tt_keymap_entry_t *entry;
  array_foreach(keymap->entries, entry) {
    char *toml_key = str_dup(entry->name, &toml_alloc);
    toml_datum_t *val = toml_tab_emplace(&toml, toml_key, &errbuf);
    if (val == NULL) {
      log_error("Error placing value: %s", errbuf);
      continue;
    }
    val->type = TOML_TABLE;

    if (!tt_toml_write_key(val, "default", entry->default_kind, entry->default_mouse_btn, entry->default_key, &toml_bump, &errbuf)) {
      log_error("Failed to write default key: %s", errbuf);
    }

    if (!tt_toml_write_key(val, "selected", entry->selected_kind, entry->selected_mouse_btn, entry->selected_key, &toml_bump, &errbuf)) {
      log_error("Failed to write selected key: %s", errbuf);
    }
  }

  char buf[1024];
  toml_print(&toml, buf);

  FILE *toml_file = fopen(filepath, "w");
  if (toml_file == NULL) {
    perror("fopen");
    return;
  }

  fputs(buf, toml_file);

  fclose(toml_file);

  datum_free(&toml);

  bump_free(&toml_bump);
}

tt_keymap_id_t tt_keymap_bind_key(tt_keymap_t *keymap, const char *name,
                                  ttb_keyboard_key_t default_key) {
  u64 id = array_len(keymap->entries);
  tt_keymap_entry_t entry = {
      .id = id,
      .name = name,
      .default_kind = TT_BUTTON_KEYBOARD,
      .default_key = default_key,
      .selected_kind = TT_BUTTON_KEYBOARD,
      .selected_key = default_key,
  };
  array_add(keymap->entries, entry);
  return id;
}

tt_keymap_id_t tt_keymap_bind_btn(tt_keymap_t *keymap, const char *name,
                                  ttb_mouse_button_e default_btn) {
  u64 id = array_len(keymap->entries);
  tt_keymap_entry_t entry = {
      .id = id,
      .name = name,
      .default_kind = TT_BUTTON_MOUSE,
      .default_mouse_btn = default_btn,
      .selected_kind = TT_BUTTON_MOUSE,
      .selected_mouse_btn = default_btn,
  };
  array_add(keymap->entries, entry);
  return id;
}

void tt_keymap_reset_key(tt_keymap_t *keymap, tt_keymap_id_t key_id) {
  tt_keymap_entry_t *entry = &keymap->entries[key_id];
  entry->selected_key = entry->default_key;
  entry->default_mouse_btn = entry->default_mouse_btn;
  entry->selected_kind = entry->default_kind;
}

bool ttec_key_pressed(tt_engine_client_t *engine, tt_keymap_t *keymap,
                      tt_keymap_id_t key_id) {
  bool pressed = keymap->entries[key_id].pressed;
  keymap->entries[key_id].pressed = false;
  return pressed;
}

bool ttec_key_released(tt_engine_client_t *engine, tt_keymap_t *keymap,
                       tt_keymap_id_t key_id) {
  bool released = keymap->entries[key_id].released;
  keymap->entries[key_id].released = false;
  return released;
}

bool ttec_key_down(tt_engine_client_t *engine, tt_keymap_t *keymap,
                   tt_keymap_id_t key_id) {
  tt_keymap_entry_t keymap_entry = keymap->entries[key_id];
  if (keymap_entry.selected_kind == TT_BUTTON_KEYBOARD) {
    return ttb_key_down(engine->backend, keymap_entry.selected_key);
  }
  return ttb_mouse_down(engine->backend, keymap_entry.selected_mouse_btn);
}

bool ttec_key_up(tt_engine_client_t *engine, tt_keymap_t *keymap,
                 tt_keymap_id_t key_id) {
  return !ttec_key_down(engine, keymap, key_id);
}

void tt_keymap_handle_input_event(tt_engine_client_t *engine,
                                  tt_keymap_t *keymap,
                                  const ttb_event_t *event) {
  if (event->event_id != TTB_EVENT_KEYBOARD_INPUT &&
      event->event_id != TTB_EVENT_MOUSE_INPUT) {
    log_error("Event is not an input event! Failed to handle");
  }

  switch (event->event_id) {
  case TTB_EVENT_KEYBOARD_INPUT: {
    ttb_event_keyboard_input_t input_evt = event->event_data.evt_keyboard_input;
    tt_keymap_entry_t *keymap_entry;
    array_foreach(keymap->entries, keymap_entry) {
      if (keymap_entry->selected_kind == TT_BUTTON_KEYBOARD &&
          input_evt.key == keymap_entry->selected_key) {
        keymap_entry->pressed = input_evt.key_state == TTB_KEY_STATE_PRESSED;
        keymap_entry->released = input_evt.key_state == TTB_KEY_STATE_RELEASED;
      }
    }
  } break;
  case TTB_EVENT_MOUSE_INPUT: {

  } break;
  default: {
    panic("UNREACHABLE");
  } break;
  }
}
