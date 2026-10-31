#pragma once

#include "lilc/array.h"
#include "tt_backend.h"
#include "tt_shared.h"
#include <lilc/alloc.h>

typedef u64 tt_keymap_id_t;

typedef enum {
  TT_BUTTON_KEYBOARD,
  TT_BUTTON_MOUSE,
} tt_button_kind_e;

typedef struct tt_keymap_entry {
  tt_keymap_id_t id;
  const char *name;

  tt_button_kind_e default_kind;
  ttb_keyboard_key_e default_key;
  ttb_mouse_button_e default_mouse_btn;

  tt_button_kind_e selected_kind;
  ttb_keyboard_key_e selected_key;
  ttb_mouse_button_e selected_mouse_btn;

  bool pressed;
  bool released;
} tt_keymap_entry_t;

typedef struct {
  array_t(tt_keymap_entry_t) entries;
} tt_keymap_t;

void tt_keymap_init(tt_keymap_t *keymap, allocator_t *alloc);

void tt_keymap_deinit(tt_keymap_t *keymap);

void tt_keymap_load(tt_keymap_t *keymap, const char *filepath, bool load_default, allocator_t *alloc);

void tt_keymap_save(const tt_keymap_t *keymap, const char *filepath, bool save_default);

void tt_keymap_handle_input_event(tt_engine_client_t *engine,
                                  tt_keymap_t *keymap,
                                  const ttb_event_t *event);

tt_keymap_id_t tt_keymap_bind_key(tt_keymap_t *keymap, const char *name,
                                  ttb_keyboard_key_e default_key);

tt_keymap_id_t tt_keymap_bind_btn(tt_keymap_t *keymap, const char *name,
                                  ttb_mouse_button_e default_btn);

void tt_keymap_reset_key(tt_keymap_t *keymap, tt_keymap_id_t key_id);

bool ttec_key_pressed(tt_engine_client_t *engine, tt_keymap_t *keymap,
                      tt_keymap_id_t key_id);

bool ttec_key_released(tt_engine_client_t *engine, tt_keymap_t *keymap,
                       tt_keymap_id_t key_id);

bool ttec_key_down(tt_engine_client_t *engine, tt_keymap_t *keymap,
                   tt_keymap_id_t key_id);

bool ttec_key_up(tt_engine_client_t *engine, tt_keymap_t *keymap,
                 tt_keymap_id_t key_id);
