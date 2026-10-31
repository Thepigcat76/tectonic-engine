#pragma once

#include "tt_camera.h"
#include "tt_shared.h"

#include "lilc/alloc.h"
#include "lilc/color.h"
#include "lilc/deque.h"

#include <stdbool.h>
#include <strings.h>

typedef struct ttb_event ttb_event_t;

// Create the backend data. This is usually handled by the engine
tt_backend_t *tt_backend_make(allocator_t *alloc, tt_asset_manager_t *assets);

// Delete the backend data. This is usually handled by the engine
void tt_backend_destroy(tt_backend_t *backend);

// Get the number of fps
i32 ttb_fps(tt_backend_t *backend);

// Get the frame rate
f64 ttb_frame_time(tt_backend_t *backend);

// Convert a screen coordinate's x-value to a world-space x-value using the
// current camera
i32 ttb_screen_to_cam_x(tt_backend_t *backend, i32 x, const tt_camera_t *cam);

// Convert a screen coordinate's y-value to a world-space y-value using the
// current camera
i32 ttb_screen_to_cam_y(tt_backend_t *backend, i32 y, const tt_camera_t *cam);

// Get the width of a text for a specific font
u32 ttb_text_width(tt_backend_t *backend, const char *text, usz font_size);

// Get the x-position of the mouse
i32 ttb_mouse_x(tt_backend_t *backend);

// Get the y-position of the mouse
i32 ttb_mouse_y(tt_backend_t *backend);

// Get the width of the screen
i32 ttb_screen_width(tt_backend_t *backend);

// Get the height of the screen
i32 ttb_screen_height(tt_backend_t *backend);

// Render all pushed render commands to the screen
void _ttb_cmds_render(tt_backend_t *backend);

// Load an asset at the given path of a specific category.
// The 'prev' param provides additional context for assets like
// previously loaded shaders with the same name of a different type.
// Returns a pointer to the asset data, allocated with 'asset_alloc'
void *ttb_asset_load(tt_backend_t *backend, allocator_t *asset_alloc,
                     tt_asset_category_t category, const char *path,
                     void *prev);

// Unload an asset of a specific category. It is assumed that the asset was
// allocated using 'asset_alloc'
void ttb_asset_unload(tt_backend_t *backend, allocator_t *asset_alloc,
                      tt_asset_category_t category, void *asset);

// Poll current events. This must only be called once per frame, otherwise you
// will get duplicate events in your queue. Polled events are stored in an
// internal events queue and can be retrieved using 'ttb_event_pop' and
// 'ttb_event_peek'
void ttb_events_poll(tt_backend_t *backend);

// Pop the last event of the event queue and write it to 'event'
bool ttb_event_pop(tt_backend_t *backend, ttb_event_t *event);

// Get the pointer to the last event of the event queue or NULL if the queue is
// empty
ttb_event_t *ttb_event_peek(tt_backend_t *backend);

bool ttb_camera_make_id(tt_backend_t *backend, u64 *cam_id);

typedef struct {
  i32 width;
  i32 height;
  const char *title;
  bool resizeable;
} ttb_window_settings_t;

// Open the native window with the given settings. Settings default to width:
// 640, height: 640, title: "Engine Example", resizeable: false
void ttb_open_window(tt_backend_t *backend, ttb_window_settings_t settings);

typedef struct ttb_render_cmd {
  enum ttb_render_cmd_id {
    TTB_RENDER_CMD_TEXTURE,
    TTB_RENDER_CMD_RECTANGLE,
    TTB_RENDER_CMD_TEXT,
    TTB_RENDER_CMD_APPLY_CAM,
    TTB_RENDER_CMD_UNAPPLY_CAM,
    TTB_RENDER_CMD_CLEAR,
  } cmd_id;
  union ttb_render_cmd_data {
    struct ttb_cmd_tex {
      tt_asset_id_t texture;
      f32 rotation;
      tt_rect_t atlas_src;
      tt_rect_t dest;
      color_t tint_color;
    } cmd_tex;
    struct ttb_cmd_rect {
      tt_rect_t rectangle;
      color_t color;
      bool outline_only;
    } cmd_rect;
    struct ttb_cmd_text {
      // TODO: Custom fonts
      i32 font_size;
      const char *text;
      color_t text_color;
      i32 x;
      i32 y;
    } cmd_text;
    struct ttb_cmd_apply_camera {
      tt_camera_t cam;
    } cmd_apply_cam;
    struct ttb_cmd_unapply_camera {
      u64 cam_id;
    } cmd_unapply_cam;
    struct ttb_cmd_clear {
      color_t bg_color;
    } cmd_clear;
  } cmd_data;
} ttb_render_cmd_t;

// Push 'render_cmd' to the internal render_cmd array to be rendered with
// '_ttb_cmds_render'
void ttb_render(tt_backend_t *backend, ttb_render_cmd_t render_cmd);

typedef enum ttb_keyboard_key {
#define TTB_KEYBOARD_ITER(_F)                                                  \
  _F(TTB_KEY_NULL)                                                             \
  /* Letters */                                                                \
  _F(TTB_KEY_A)                                                                \
  _F(TTB_KEY_B)                                                                \
  _F(TTB_KEY_C)                                                                \
  _F(TTB_KEY_D)                                                                \
  _F(TTB_KEY_E)                                                                \
  _F(TTB_KEY_F)                                                                \
  _F(TTB_KEY_G)                                                                \
  _F(TTB_KEY_H)                                                                \
  _F(TTB_KEY_I)                                                                \
  _F(TTB_KEY_J)                                                                \
  _F(TTB_KEY_K)                                                                \
  _F(TTB_KEY_L)                                                                \
  _F(TTB_KEY_M)                                                                \
  _F(TTB_KEY_N)                                                                \
  _F(TTB_KEY_O)                                                                \
  _F(TTB_KEY_P)                                                                \
  _F(TTB_KEY_Q)                                                                \
  _F(TTB_KEY_R)                                                                \
  _F(TTB_KEY_S)                                                                \
  _F(TTB_KEY_T)                                                                \
  _F(TTB_KEY_U)                                                                \
  _F(TTB_KEY_V)                                                                \
  _F(TTB_KEY_W)                                                                \
  _F(TTB_KEY_X)                                                                \
  _F(TTB_KEY_Y)                                                                \
  _F(TTB_KEY_Z)                                                                \
                                                                               \
  /* Numbers (top row) */                                                      \
  _F(TTB_KEY_0)                                                                \
  _F(TTB_KEY_1)                                                                \
  _F(TTB_KEY_2)                                                                \
  _F(TTB_KEY_3)                                                                \
  _F(TTB_KEY_4)                                                                \
  _F(TTB_KEY_5)                                                                \
  _F(TTB_KEY_6)                                                                \
  _F(TTB_KEY_7)                                                                \
  _F(TTB_KEY_8)                                                                \
  _F(TTB_KEY_9)                                                                \
                                                                               \
  /* Function keys */                                                          \
  _F(TTB_KEY_F1)                                                               \
  _F(TTB_KEY_F2)                                                               \
  _F(TTB_KEY_F3)                                                               \
  _F(TTB_KEY_F4)                                                               \
  _F(TTB_KEY_F5)                                                               \
  _F(TTB_KEY_F6)                                                               \
  _F(TTB_KEY_F7)                                                               \
  _F(TTB_KEY_F8)                                                               \
  _F(TTB_KEY_F9)                                                               \
  _F(TTB_KEY_F10)                                                              \
  _F(TTB_KEY_F11)                                                              \
  _F(TTB_KEY_F12)                                                              \
                                                                               \
  /* Arrow keys */                                                             \
  _F(TTB_KEY_UP)                                                               \
  _F(TTB_KEY_DOWN)                                                             \
  _F(TTB_KEY_LEFT)                                                             \
  _F(TTB_KEY_RIGHT)                                                            \
                                                                               \
  /* Modifier keys */                                                          \
  _F(TTB_KEY_LEFT_SHIFT)                                                       \
  _F(TTB_KEY_RIGHT_SHIFT)                                                      \
  _F(TTB_KEY_LEFT_CTRL)                                                        \
  _F(TTB_KEY_RIGHT_CTRL)                                                       \
  _F(TTB_KEY_LEFT_ALT)                                                         \
  _F(TTB_KEY_RIGHT_ALT)                                                        \
  _F(TTB_KEY_LEFT_SUPER)                                                       \
  _F(TTB_KEY_RIGHT_SUPER)                                                      \
                                                                               \
  /* Control keys */                                                           \
  _F(TTB_KEY_ESCAPE)                                                           \
  _F(TTB_KEY_ENTER)                                                            \
  _F(TTB_KEY_TAB)                                                              \
  _F(TTB_KEY_BACKSPACE)                                                        \
  _F(TTB_KEY_DELETE)                                                           \
  _F(TTB_KEY_INSERT)                                                           \
  _F(TTB_KEY_HOME)                                                             \
  _F(TTB_KEY_END)                                                              \
  _F(TTB_KEY_PAGE_UP)                                                          \
  _F(TTB_KEY_PAGE_DOWN)                                                        \
  _F(TTB_KEY_CAPS_LOCK)                                                        \
  _F(TTB_KEY_PRINT_SCREEN)                                                     \
  _F(TTB_KEY_SCROLL_LOCK)                                                      \
  _F(TTB_KEY_PAUSE)                                                            \
  _F(TTB_KEY_SPACE)                                                            \
                                                                               \
  /* Punctuation / symbols */                                                  \
  _F(TTB_KEY_APOSTROPHE)                                                       \
  _F(TTB_KEY_COMMA)                                                            \
  _F(TTB_KEY_MINUS)                                                            \
  _F(TTB_KEY_PERIOD)                                                           \
  _F(TTB_KEY_SLASH)                                                            \
  _F(TTB_KEY_SEMICOLON)                                                        \
  _F(TTB_KEY_EQUAL)                                                            \
  _F(TTB_KEY_LEFT_BRACKET)                                                     \
  _F(TTB_KEY_BACKSLASH)                                                        \
  _F(TTB_KEY_RIGHT_BRACKET)                                                    \
  _F(TTB_KEY_GRAVE)                                                            \
                                                                               \
  /* Numpad */                                                                 \
  _F(TTB_KEY_NUM_LOCK)                                                         \
  _F(TTB_KEY_NUMPAD_0)                                                         \
  _F(TTB_KEY_NUMPAD_1)                                                         \
  _F(TTB_KEY_NUMPAD_2)                                                         \
  _F(TTB_KEY_NUMPAD_3)                                                         \
  _F(TTB_KEY_NUMPAD_4)                                                         \
  _F(TTB_KEY_NUMPAD_5)                                                         \
  _F(TTB_KEY_NUMPAD_6)                                                         \
  _F(TTB_KEY_NUMPAD_7)                                                         \
  _F(TTB_KEY_NUMPAD_8)                                                         \
  _F(TTB_KEY_NUMPAD_9)                                                         \
  _F(TTB_KEY_NUMPAD_ADD)                                                       \
  _F(TTB_KEY_NUMPAD_SUBTRACT)                                                  \
  _F(TTB_KEY_NUMPAD_MULTIPLY)                                                  \
  _F(TTB_KEY_NUMPAD_DIVIDE)                                                    \
  _F(TTB_KEY_NUMPAD_DECIMAL)                                                   \
  _F(TTB_KEY_NUMPAD_ENTER)
#define ENUM_ITEM(name) name,

  TTB_KEYBOARD_ITER(ENUM_ITEM)

      _amount_ttb_keys,
#undef ENUM_ITEM
} ttb_keyboard_key_e;

// Check if a specific key is held down. To check for key pressing and
// releasing, use events
bool ttb_key_down(tt_backend_t *backend, ttb_keyboard_key_e key);

typedef enum {
#define TTB_MOUSE_ITER(_F)                                                     \
  _F(TTB_MOUSE_BUTTON_NULL)                                                    \
  _F(TTB_MOUSE_BUTTON_LEFT)                                                    \
  _F(TTB_MOUSE_BUTTON_RIGHT)                                                   \
  _F(TTB_MOUSE_BUTTON_MIDDLE)
#define ENUM_ITEM(name) name,

  TTB_MOUSE_ITER(ENUM_ITEM)

      _amount_ttb_mouse_btns,
#undef ENUM_ITEM
} ttb_mouse_button_e;

// Check if a specific mouse button is held down. To check for mouse button pressing and
// releasing, use events
bool ttb_mouse_down(tt_backend_t *backend, ttb_mouse_button_e btn);

typedef enum ttb_event_id {
  TTB_EVENT_WINDOW_CLOSED,
  TTB_EVENT_WINDOW_RESIZED,

  TTB_EVENT_KEYBOARD_INPUT,
  TTB_EVENT_MOUSE_INPUT,

  _amount_ttb_event_ids,
} ttb_event_id_e;

typedef enum ttb_key_state {
  TTB_KEY_STATE_NONE,
  TTB_KEY_STATE_PRESSED,
  TTB_KEY_STATE_RELEASED,
} ttb_key_state_t;

typedef struct ttb_event_mouse_input {
  ttb_key_state_t buttons[_amount_ttb_mouse_btns];
} ttb_event_mouse_input_t;

static ttb_keyboard_key_e ttb_key_from_str(const char *key_str) {
#define STR_CASE(name)                                                         \
  if (strcasecmp(key_str, #name) == 0)                                         \
    return name;
  TTB_KEYBOARD_ITER(STR_CASE)
#undef STR_CASE

  return TTB_KEY_NULL;
}

static ttb_mouse_button_e ttb_mouse_btn_from_str(const char *key_str) {
#define STR_CASE(name)                                                         \
  if (strcasecmp(key_str, #name))                                              \
    return name;
  TTB_MOUSE_ITER(STR_CASE)
#undef STR_CASE

  return TTB_MOUSE_BUTTON_NULL;
}

static char *ttb_mouse_btn_to_string(ttb_mouse_button_e key) {
  static char *mouse_str_lits[_amount_ttb_mouse_btns] = {
#define STR_LIT(name) #name,
      TTB_MOUSE_ITER(STR_LIT)
#undef STR_LIT
  };

  return mouse_str_lits[key];
}

static char *ttb_key_to_string(ttb_keyboard_key_e key) {
  static char *key_str_lits[_amount_ttb_keys] = {
#define STR_LIT(name) #name,
      TTB_KEYBOARD_ITER(STR_LIT)
#undef STR_LIT
  };

  return key_str_lits[key];
}

typedef struct ttb_event_keyboard_input {
  ttb_key_state_t key_state;
  ttb_keyboard_key_e key;
} ttb_event_keyboard_input_t;

struct ttb_event {
  ttb_event_id_e event_id;
  union ttb_event_data {
    struct ttb_event_resized {
      i32 prev_width;
      i32 prev_height;
    } evt_resized;
    ttb_event_keyboard_input_t evt_keyboard_input;
    ttb_event_mouse_input_t evt_mouse_input;
  } event_data;
};

typedef deque_t(ttb_event_t) ttb_event_deque_t;

typedef enum ttb_render_cmd_id ttb_render_cmd_id_t;

typedef union ttb_render_cmd_data ttb_render_cmd_data_t;
