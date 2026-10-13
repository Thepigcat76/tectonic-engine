#include "../include/tt_assets.h"
#include "../include/tt_backend.h"
#include "../include/tt_camera.h"
#include "../include/tt_shared.h"
#include "lilc/alloc.h"
#include "lilc/array.h"
#include "lilc/deque.h"
#include "lilc/log.h"
#include "lilc/todo.h"

#include "raylib.h"
#include <lilc/file.h>

struct tt_backend {
  ttb_render_cmd_t *render_cmds;
  ttb_event_t *event_queue;

  u64 cameras_amount;

  tt_asset_manager_t *assets_manager;

  allocator_t *backend_alloc;

  // Allocator for general render tasks
  bump_t render_bump;
  allocator_t render_alloc;

  // Allocator for render commands that need
  // to update every frame
  bump_t render_cmd_bump;
  allocator_t render_cmd_alloc;

  // Allocator for event queue and event related
  // data
  bump_t event_bump;
  allocator_t event_alloc;
};

tt_backend_t *tt_backend_make(allocator_t *alloc, tt_asset_manager_t *assets) {
  tt_backend_t *backend = alloc->alloc(alloc, sizeof(tt_backend_t));
  memset(backend, 0, sizeof(tt_backend_t));

  backend->backend_alloc = alloc;
  backend->assets_manager = assets;

  bump_init(&backend->render_cmd_bump, 128000);
  bump_allocator_init(&backend->render_cmd_alloc, &backend->render_cmd_bump);

  bump_init(&backend->render_bump, 128000);
  bump_allocator_init(&backend->render_alloc, &backend->render_bump);

  bump_init(&backend->event_bump, 128000);
  bump_allocator_init(&backend->event_alloc, &backend->event_bump);

  backend->render_cmds =
      array_new(ttb_render_cmd_t, &backend->render_cmd_alloc);
  // TODO: We should probably init this locally in the event poll function and
  // reset it.
  deque_init(backend->event_queue, &backend->event_alloc);

  backend->cameras_amount = 0;

  return backend;
}

void tt_backend_destroy(tt_backend_t *backend) {
  bump_free(&backend->render_cmd_bump);

  allocator_t *alloc = backend->backend_alloc;
  alloc->dealloc(alloc, backend);
}

static ttb_keyboard_key_e ttb_key_from_rl(KeyboardKey rl_key);

static KeyboardKey rl_key_from_ttb(ttb_keyboard_key_e key);

void ttb_events_poll(tt_backend_t *backend) {
  if (WindowShouldClose()) {
    log_debug("close window event");
    ttb_event_t window_close_event = {
        .event_id = TTB_EVENT_WINDOW_CLOSED,
    };
    deque_push_back(backend->event_queue, window_close_event);
  }

  KeyboardKey rl_key = GetKeyPressed();
  if (rl_key != KEY_NULL) {
    ttb_keyboard_key_e key = ttb_key_from_rl(rl_key);

    i32 key_state = -1;

    if (IsKeyPressed(rl_key)) {
      key_state = TTB_KEY_STATE_PRESSED;
    } else if (IsKeyReleased(rl_key)) {
      key_state = TTB_KEY_STATE_RELEASED;
    }

    if (key_state != -1) {
      ttb_event_t key_pressed_evt = {
          .event_id = TTB_EVENT_KEYBOARD_INPUT,
          .event_data.evt_keyboard_input =
              {
                  .key = key,
                  .key_state = key_state,
              },
      };
      deque_push_back(backend->event_queue, key_pressed_evt);
    }
  }

  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
      ttb_event_t key_pressed_evt = {
          .event_id = TTB_EVENT_MOUSE_INPUT,
          .event_data.evt_mouse_input =
              {
                  .button = TTB_MOUSE_BUTTON_LEFT,
                  .left_button = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) ? TTB_KEY_STATE_PRESSED : TTB_KEY_STATE_RELEASED,
              },
      };
      deque_push_back(backend->event_queue, key_pressed_evt);
  }

  if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) || IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) {
      ttb_event_t key_pressed_evt = {
          .event_id = TTB_EVENT_MOUSE_INPUT,
          .event_data.evt_mouse_input =
              {
                  .button = TTB_MOUSE_BUTTON_RIGHT,
                  .left_button = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) ? TTB_KEY_STATE_PRESSED : TTB_KEY_STATE_RELEASED,
              },
      };
      deque_push_back(backend->event_queue, key_pressed_evt);
  }

  if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE) || IsMouseButtonReleased(MOUSE_BUTTON_MIDDLE)) {
      ttb_event_t key_pressed_evt = {
          .event_id = TTB_EVENT_MOUSE_INPUT,
          .event_data.evt_mouse_input =
              {
                  .button = TTB_MOUSE_BUTTON_MIDDLE,
                  .left_button = IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE) ? TTB_KEY_STATE_PRESSED : TTB_KEY_STATE_RELEASED,
              },
      };
      deque_push_back(backend->event_queue, key_pressed_evt);
  }
  
}

static void ttb_camera_copy(Camera2D *rl_cam, const tt_camera_t *cam) {
  rl_cam->offset.x = cam->offset_x;
  rl_cam->offset.y = cam->offset_y;
  rl_cam->target.x = cam->target_x;
  rl_cam->target.y = cam->target_y;
  rl_cam->rotation = cam->rotation;
  rl_cam->zoom = cam->zoom;
}

i32 ttb_screen_to_cam_x(tt_backend_t *backend, i32 x, const tt_camera_t *cam) {
  Camera2D rl_cam;
  ttb_camera_copy(&rl_cam, cam);
  return GetScreenToWorld2D((Vector2){.x = x}, rl_cam).x;
}

i32 ttb_screen_to_cam_y(tt_backend_t *backend, i32 y, const tt_camera_t *cam) {
  Camera2D rl_cam;
  ttb_camera_copy(&rl_cam, cam);
  return GetScreenToWorld2D((Vector2){.y = y}, rl_cam).y;}

f64 ttb_frame_time(tt_backend_t *backend) {
  return GetFrameTime();
}

bool ttb_key_down(tt_backend_t *backend, ttb_keyboard_key_e key) {
  (void)backend;
  return IsKeyDown(rl_key_from_ttb(key));
}

bool ttb_mouse_down(tt_backend_t *backend, ttb_mouse_button_e btn) {
  (void)backend;
  (void)btn;
  return TODO("Mouse down not implemented yet");
}

ttb_event_t *ttb_event_peek(tt_backend_t *backend) {
  if (deque_len(backend->event_queue) == 0)
    return NULL;

  return deque_front(backend->event_queue);
}

bool ttb_event_pop(tt_backend_t *backend, ttb_event_t *event) {
  if (deque_len(backend->event_queue) == 0)
    return false;

  ttb_event_t *evt = deque_pop_front(backend->event_queue);
  if (evt != NULL) {
    *event = *evt;
    return true;
  }
  return false;
}

#define rect_convert(tt_rect)                                                  \
  (Rectangle) {                                                                \
    .x = tt_rect.x, .y = tt_rect.y, .width = tt_rect.width,                    \
    .height = tt_rect.height                                                   \
  }

#define rl_rect(_x, _y, _width, _height)                                       \
  (Rectangle) { .x = _x, .y = _y, .width = _width, .height = _height, }

#define color_convert(color)                                                   \
  (Color) {                                                                    \
    .r = color_red(color), .g = color_green(color), .b = color_blue(color),    \
    .a = color_alpha(color)                                                    \
  }

u32 ttb_text_width(tt_backend_t *backend, const char *text, usz font_size) {
  (void)backend;
  return MeasureText(text, font_size);
}

void ttb_render(tt_backend_t *backend, ttb_render_cmd_t render_cmd) {
  array_add(backend->render_cmds, render_cmd);
}

typedef struct rl_shader {
  Shader shader;
  const char *vs_path;
  const char *fs_path;
} rl_shader_t;

void *ttb_asset_load(tt_backend_t *backend, allocator_t *asset_alloc,
                     tt_asset_category_t asset_category, const char *path,
                     void *prev) {
  (void)backend;
  switch (asset_category) {
  case TT_ASSET_TEXTURE: {
    Texture2D *tex = asset_alloc->alloc(asset_alloc, sizeof(Texture2D));
    Texture2D loaded_tex = LoadTexture(path);
    memcpy(tex, &loaded_tex, sizeof(Texture2D));
    return tex;
  } break;
  case TT_ASSET_SOUND: {
    Sound *sound = asset_alloc->alloc(asset_alloc, sizeof(Sound));
    Sound loaded_sound = LoadSound(path);
    memcpy(sound, &loaded_sound, sizeof(Sound));
    return sound;
  } break;
  case TT_ASSET_SHADER: {
    rl_shader_t *shader = prev;
    if (shader == NULL) {
      shader = asset_alloc->alloc(asset_alloc, sizeof(rl_shader_t));
      memset(shader, 0, sizeof(rl_shader_t));
    }

    bool valid_shader_path = false;
    const char *file_ext = file_extension(path);
    if (str_eq(file_ext, "vs")) {
      shader->vs_path = path;
      valid_shader_path = true;
    } else if (str_eq(file_ext, "fs")) {
      shader->fs_path = path;
      valid_shader_path = true;
    }

    if (valid_shader_path) {
      if (prev != NULL) {
        UnloadShader(shader->shader);
      }

      shader->shader = LoadShader(shader->vs_path, shader->fs_path);
      return shader;
    }
    return NULL;
    // TODO: Load other assets
  } break;
  case TT_ASSET_MUSIC: {
  } break;
  }

  return NULL;
}

void ttb_asset_unload(tt_backend_t *backend, allocator_t *asset_alloc,
                      tt_asset_category_t asset_category, void *asset) {
  (void)backend;
  switch (asset_category) {
  case TT_ASSET_TEXTURE: {
    UnloadTexture(*(Texture2D *)asset);
    asset_alloc->dealloc(asset_alloc, asset);
  } break;
  case TT_ASSET_SOUND: {

  } break;
  case TT_ASSET_SHADER: {

  } break;
  case TT_ASSET_MUSIC: {

  } break;
  }
}

static void ttb_nine_slice_render(Texture2D tex, u32 border, Rectangle dest,
                                  Rectangle src, Vector2 origin, i32 rotation,
                                  Color tint) {
  u32 dw = dest.width - border * 2;  // scalable interior width in destination
  u32 dh = dest.height - border * 2; // scalable interior height in destination
  u32 sw = src.width - border * 2;   // scalable interior width in source
  u32 sh = src.height - border * 2;  // scalable interior height in source

  // 1. TOP-LEFT CORNER
  DrawTexturePro(tex, rl_rect(0, 0, border, border),      // src
                 rl_rect(dest.x, dest.y, border, border), // dst
                 origin, rotation, tint);

  // 2. TOP EDGE
  DrawTexturePro(tex, rl_rect(border, 0, sw, border), // src
                 rl_rect(dest.x + border, dest.y, dw,
                         border), // dst (stretched horizontally)
                 origin, rotation, tint);

  // 3. TOP-RIGHT CORNER
  DrawTexturePro(tex, rl_rect(border + sw, 0, border, border),          // src
                 rl_rect(dest.x + border + dw, dest.y, border, border), // dst
                 origin, rotation, tint);

  // 4. LEFT EDGE
  DrawTexturePro(tex, rl_rect(0, border, border, sh), // src
                 rl_rect(dest.x, dest.y + border, border,
                         dh), // dst (stretched vertically)
                 origin, rotation, tint);

  // 5. CENTER
  DrawTexturePro(
      tex, rl_rect(border, border, sw, sh),              // src
      rl_rect(dest.x + border, dest.y + border, dw, dh), // dst (stretched both)
      origin, rotation, tint);

  // 6. RIGHT EDGE
  DrawTexturePro(tex, rl_rect(border + sw, border, border, sh), // src
                 rl_rect(dest.x + border + dw, dest.y + border, border,
                         dh), // dst (stretched vertically)
                 origin, rotation, tint);

  // 7. BOTTOM-LEFT CORNER
  DrawTexturePro(tex, rl_rect(0, border + sh, border, border),          // src
                 rl_rect(dest.x, dest.y + dh + border, border, border), // dst
                 origin, rotation, tint);

  // 8. BOTTOM EDGE
  DrawTexturePro(tex, rl_rect(border, border + sh, sw, border), // src
                 rl_rect(dest.x + border, dest.y + dh + border, dw,
                         border), // dst (stretched horizontally)
                 origin, rotation, tint);

  // 9. BOTTOM-RIGHT CORNER
  DrawTexturePro(tex, rl_rect(border + sw, border + sh, border, border), // src
                 rl_rect(dest.x + dw + border, dest.y + dh + border, border,
                         border), // dst
                 origin, rotation, tint);
}

i32 ttb_mouse_x(tt_backend_t *backend) {
  (void)backend;
  return GetMousePosition().x;
}

i32 ttb_mouse_y(tt_backend_t *backend) {
  (void)backend;
  return GetMousePosition().y;
}

i32 ttb_screen_width(tt_backend_t *backend) {
  (void) backend;
  return GetScreenWidth();
}

i32 ttb_screen_height(tt_backend_t *backend) {
  (void) backend;
  return GetScreenHeight();
}

bool ttb_camera_make_id(tt_backend_t *backend, u64 *cam_id) {
  *cam_id = backend->cameras_amount++;
  
  return true;
}

void _ttb_cmds_render(tt_backend_t *backend) {
  BeginDrawing();

  bool has_camera = false;
  i64 cam_id = -1;

  ttb_render_cmd_t *render_cmd;
  array_foreach(backend->render_cmds, render_cmd) {
    switch (render_cmd->cmd_id) {
    case TTB_RENDER_CMD_TEXTURE: {
      struct ttb_cmd_tex cmd_tex = render_cmd->cmd_data.cmd_tex;

      tt_asset_texture_t *tex =
          tt_asset_by_id(backend->assets_manager, cmd_tex.texture);

      Texture2D *texture = tex->tex_data;
      if (cmd_tex.atlas_src.width == 0) {
        cmd_tex.atlas_src.width = texture->width;
      }
      if (cmd_tex.atlas_src.height == 0) {
        cmd_tex.atlas_src.height = texture->height;
      }
      if (cmd_tex.dest.width == 0) {
        cmd_tex.dest.width = texture->width;
      }
      if (cmd_tex.dest.height == 0) {
        cmd_tex.dest.height = texture->height;
      }

      if (tex->meta->scaling.enabled &&
          tex->meta->scaling.kind != TT_SCALING_DEFAULT) {
        if (tex->meta->scaling.kind == TT_SCALING_NINE_SLICE) {
          u32 border_width = tex->meta->scaling.u.nine_slice.border_width;

          ttb_nine_slice_render(
              *texture, border_width, rect_convert(cmd_tex.dest),
              rl_rect(0, 0, texture->width, texture->height), (Vector2){0},
              cmd_tex.rotation, color_convert(cmd_tex.tint_color));
        } else {
          log_warn("[Rendering Texture] Scaling kind %d not implemented",
                   tex->meta->scaling.kind);
        }
      } else {
        DrawTexturePro(*texture, rect_convert(cmd_tex.atlas_src),
                       rect_convert(cmd_tex.dest), (Vector2){0},
                       cmd_tex.rotation, color_convert(cmd_tex.tint_color));
      }

    } break;
    case TTB_RENDER_CMD_RECTANGLE: {
    } break;
    case TTB_RENDER_CMD_TEXT: {
      struct ttb_cmd_text cmd_text = render_cmd->cmd_data.cmd_text;

      DrawText(cmd_text.text, cmd_text.x, cmd_text.y, cmd_text.font_size,
               color_convert(cmd_text.text_color));
    } break;
    case TTB_RENDER_CMD_CLEAR: {
      struct ttb_cmd_clear cmd_clear = render_cmd->cmd_data.cmd_clear;

      ClearBackground(color_convert(cmd_clear.bg_color));
    } break;
    case TTB_RENDER_CMD_APPLY_CAM: {
      if (!has_camera) {
        tt_camera_t cam = render_cmd->cmd_data.cmd_apply_cam.cam;

        Camera2D rl_cam = {0};
        ttb_camera_copy(&rl_cam, &cam);

        BeginMode2D(rl_cam);

        has_camera = true;
        cam_id = cam.cam_id;
      } else {
        log_warn("Tried applying camera, even though camera is already active");
      }
    } break;
    case TTB_RENDER_CMD_UNAPPLY_CAM: {
      if (!has_camera) {
        log_warn("Tried unapplying camera, even though none was active");
      } else {
        if (cam_id != (i64) render_cmd->cmd_data.cmd_unapply_cam.cam_id) {
          log_warn("Tried unapplying different camera (%zu) from the one active: %zu", render_cmd->cmd_data.cmd_unapply_cam.cam_id, cam_id);
        }
        EndMode2D();
        has_camera = false;
        cam_id = -1;
      }
    } break;
    }
  }

  EndDrawing();

  if (has_camera) {
    log_warn("Last camera was not un-applied");
    EndMode2D();
  }

  bump_reset(&backend->render_cmd_bump);
  bump_reset(&backend->render_bump);

  backend->render_cmds =
      array_new(ttb_render_cmd_t, &backend->render_cmd_alloc);
}

static ttb_keyboard_key_e ttb_key_from_rl(KeyboardKey rl_key) {
  switch (rl_key) {
  case KEY_NULL:
    return TTB_KEY_NULL;
  // Letters
  case KEY_A:
    return TTB_KEY_A;
  case KEY_B:
    return TTB_KEY_B;
  case KEY_C:
    return TTB_KEY_C;
  case KEY_D:
    return TTB_KEY_D;
  case KEY_E:
    return TTB_KEY_E;
  case KEY_F:
    return TTB_KEY_F;
  case KEY_G:
    return TTB_KEY_G;
  case KEY_H:
    return TTB_KEY_H;
  case KEY_I:
    return TTB_KEY_I;
  case KEY_J:
    return TTB_KEY_J;
  case KEY_K:
    return TTB_KEY_K;
  case KEY_L:
    return TTB_KEY_L;
  case KEY_M:
    return TTB_KEY_M;
  case KEY_N:
    return TTB_KEY_N;
  case KEY_O:
    return TTB_KEY_O;
  case KEY_P:
    return TTB_KEY_P;
  case KEY_Q:
    return TTB_KEY_Q;
  case KEY_R:
    return TTB_KEY_R;
  case KEY_S:
    return TTB_KEY_S;
  case KEY_T:
    return TTB_KEY_T;
  case KEY_U:
    return TTB_KEY_U;
  case KEY_V:
    return TTB_KEY_V;
  case KEY_W:
    return TTB_KEY_W;
  case KEY_X:
    return TTB_KEY_X;
  case KEY_Y:
    return TTB_KEY_Y;
  case KEY_Z:
    return TTB_KEY_Z;

  // Numbers
  case KEY_ZERO:
    return TTB_KEY_0;
  case KEY_ONE:
    return TTB_KEY_1;
  case KEY_TWO:
    return TTB_KEY_2;
  case KEY_THREE:
    return TTB_KEY_3;
  case KEY_FOUR:
    return TTB_KEY_4;
  case KEY_FIVE:
    return TTB_KEY_5;
  case KEY_SIX:
    return TTB_KEY_6;
  case KEY_SEVEN:
    return TTB_KEY_7;
  case KEY_EIGHT:
    return TTB_KEY_8;
  case KEY_NINE:
    return TTB_KEY_9;

  // Function keys
  case KEY_F1:
    return TTB_KEY_F1;
  case KEY_F2:
    return TTB_KEY_F2;
  case KEY_F3:
    return TTB_KEY_F3;
  case KEY_F4:
    return TTB_KEY_F4;
  case KEY_F5:
    return TTB_KEY_F5;
  case KEY_F6:
    return TTB_KEY_F6;
  case KEY_F7:
    return TTB_KEY_F7;
  case KEY_F8:
    return TTB_KEY_F8;
  case KEY_F9:
    return TTB_KEY_F9;
  case KEY_F10:
    return TTB_KEY_F10;
  case KEY_F11:
    return TTB_KEY_F11;
  case KEY_F12:
    return TTB_KEY_F12;

  // Arrow keys
  case KEY_UP:
    return TTB_KEY_UP;
  case KEY_DOWN:
    return TTB_KEY_DOWN;
  case KEY_LEFT:
    return TTB_KEY_LEFT;
  case KEY_RIGHT:
    return TTB_KEY_RIGHT;

  // Modifiers
  case KEY_LEFT_SHIFT:
    return TTB_KEY_LEFT_SHIFT;
  case KEY_RIGHT_SHIFT:
    return TTB_KEY_RIGHT_SHIFT;
  case KEY_LEFT_CONTROL:
    return TTB_KEY_LEFT_CTRL;
  case KEY_RIGHT_CONTROL:
    return TTB_KEY_RIGHT_CTRL;
  case KEY_LEFT_ALT:
    return TTB_KEY_LEFT_ALT;
  case KEY_RIGHT_ALT:
    return TTB_KEY_RIGHT_ALT;
  case KEY_LEFT_SUPER:
    return TTB_KEY_LEFT_SUPER;
  case KEY_RIGHT_SUPER:
    return TTB_KEY_RIGHT_SUPER;

  // Control keys
  case KEY_ESCAPE:
    return TTB_KEY_ESCAPE;
  case KEY_ENTER:
    return TTB_KEY_ENTER;
  case KEY_TAB:
    return TTB_KEY_TAB;
  case KEY_BACKSPACE:
    return TTB_KEY_BACKSPACE;
  case KEY_DELETE:
    return TTB_KEY_DELETE;
  case KEY_INSERT:
    return TTB_KEY_INSERT;
  case KEY_HOME:
    return TTB_KEY_HOME;
  case KEY_END:
    return TTB_KEY_END;
  case KEY_PAGE_UP:
    return TTB_KEY_PAGE_UP;
  case KEY_PAGE_DOWN:
    return TTB_KEY_PAGE_DOWN;
  case KEY_CAPS_LOCK:
    return TTB_KEY_CAPS_LOCK;
  case KEY_PRINT_SCREEN:
    return TTB_KEY_PRINT_SCREEN;
  case KEY_SCROLL_LOCK:
    return TTB_KEY_SCROLL_LOCK;
  case KEY_PAUSE:
    return TTB_KEY_PAUSE;
  case KEY_SPACE:
    return TTB_KEY_SPACE;

  // Punctuation
  case KEY_APOSTROPHE:
    return TTB_KEY_APOSTROPHE;
  case KEY_COMMA:
    return TTB_KEY_COMMA;
  case KEY_MINUS:
    return TTB_KEY_MINUS;
  case KEY_PERIOD:
    return TTB_KEY_PERIOD;
  case KEY_SLASH:
    return TTB_KEY_SLASH;
  case KEY_SEMICOLON:
    return TTB_KEY_SEMICOLON;
  case KEY_EQUAL:
    return TTB_KEY_EQUAL;
  case KEY_LEFT_BRACKET:
    return TTB_KEY_LEFT_BRACKET;
  case KEY_BACKSLASH:
    return TTB_KEY_BACKSLASH;
  case KEY_RIGHT_BRACKET:
    return TTB_KEY_RIGHT_BRACKET;
  case KEY_GRAVE:
    return TTB_KEY_GRAVE;

  // Numpad
  case KEY_NUM_LOCK:
    return TTB_KEY_NUM_LOCK;
  case KEY_KP_0:
    return TTB_KEY_NUMPAD_0;
  case KEY_KP_1:
    return TTB_KEY_NUMPAD_1;
  case KEY_KP_2:
    return TTB_KEY_NUMPAD_2;
  case KEY_KP_3:
    return TTB_KEY_NUMPAD_3;
  case KEY_KP_4:
    return TTB_KEY_NUMPAD_4;
  case KEY_KP_5:
    return TTB_KEY_NUMPAD_5;
  case KEY_KP_6:
    return TTB_KEY_NUMPAD_6;
  case KEY_KP_7:
    return TTB_KEY_NUMPAD_7;
  case KEY_KP_8:
    return TTB_KEY_NUMPAD_8;
  case KEY_KP_9:
    return TTB_KEY_NUMPAD_9;
  case KEY_KP_ADD:
    return TTB_KEY_NUMPAD_ADD;
  case KEY_KP_SUBTRACT:
    return TTB_KEY_NUMPAD_SUBTRACT;
  case KEY_KP_MULTIPLY:
    return TTB_KEY_NUMPAD_MULTIPLY;
  case KEY_KP_DIVIDE:
    return TTB_KEY_NUMPAD_DIVIDE;
  case KEY_KP_DECIMAL:
    return TTB_KEY_NUMPAD_DECIMAL;
  case KEY_KP_ENTER:
    return TTB_KEY_NUMPAD_ENTER;

  default:
    return -1; // unknown key
  }
}

static KeyboardKey rl_key_from_ttb(ttb_keyboard_key_e key) {
  switch (key) {
  case TTB_KEY_NULL:
    return KEY_NULL;
  // Letters
  case TTB_KEY_A:
    return KEY_A;
  case TTB_KEY_B:
    return KEY_B;
  case TTB_KEY_C:
    return KEY_C;
  case TTB_KEY_D:
    return KEY_D;
  case TTB_KEY_E:
    return KEY_E;
  case TTB_KEY_F:
    return KEY_F;
  case TTB_KEY_G:
    return KEY_G;
  case TTB_KEY_H:
    return KEY_H;
  case TTB_KEY_I:
    return KEY_I;
  case TTB_KEY_J:
    return KEY_J;
  case TTB_KEY_K:
    return KEY_K;
  case TTB_KEY_L:
    return KEY_L;
  case TTB_KEY_M:
    return KEY_M;
  case TTB_KEY_N:
    return KEY_N;
  case TTB_KEY_O:
    return KEY_O;
  case TTB_KEY_P:
    return KEY_P;
  case TTB_KEY_Q:
    return KEY_Q;
  case TTB_KEY_R:
    return KEY_R;
  case TTB_KEY_S:
    return KEY_S;
  case TTB_KEY_T:
    return KEY_T;
  case TTB_KEY_U:
    return KEY_U;
  case TTB_KEY_V:
    return KEY_V;
  case TTB_KEY_W:
    return KEY_W;
  case TTB_KEY_X:
    return KEY_X;
  case TTB_KEY_Y:
    return KEY_Y;
  case TTB_KEY_Z:
    return KEY_Z;

  // Numbers
  case TTB_KEY_0:
    return KEY_ZERO;
  case TTB_KEY_1:
    return KEY_ONE;
  case TTB_KEY_2:
    return KEY_TWO;
  case TTB_KEY_3:
    return KEY_THREE;
  case TTB_KEY_4:
    return KEY_FOUR;
  case TTB_KEY_5:
    return KEY_FIVE;
  case TTB_KEY_6:
    return KEY_SIX;
  case TTB_KEY_7:
    return KEY_SEVEN;
  case TTB_KEY_8:
    return KEY_EIGHT;
  case TTB_KEY_9:
    return KEY_NINE;

  // Function keys
  case TTB_KEY_F1:
    return KEY_F1;
  case TTB_KEY_F2:
    return KEY_F2;
  case TTB_KEY_F3:
    return KEY_F3;
  case TTB_KEY_F4:
    return KEY_F4;
  case TTB_KEY_F5:
    return KEY_F5;
  case TTB_KEY_F6:
    return KEY_F6;
  case TTB_KEY_F7:
    return KEY_F7;
  case TTB_KEY_F8:
    return KEY_F8;
  case TTB_KEY_F9:
    return KEY_F9;
  case TTB_KEY_F10:
    return KEY_F10;
  case TTB_KEY_F11:
    return KEY_F11;
  case TTB_KEY_F12:
    return KEY_F12;

  // Arrow keys
  case TTB_KEY_UP:
    return KEY_UP;
  case TTB_KEY_DOWN:
    return KEY_DOWN;
  case TTB_KEY_LEFT:
    return KEY_LEFT;
  case TTB_KEY_RIGHT:
    return KEY_RIGHT;

  // Modifiers
  case TTB_KEY_LEFT_SHIFT:
    return KEY_LEFT_SHIFT;
  case TTB_KEY_RIGHT_SHIFT:
    return KEY_RIGHT_SHIFT;
  case TTB_KEY_LEFT_CTRL:
    return KEY_LEFT_CONTROL;
  case TTB_KEY_RIGHT_CTRL:
    return KEY_RIGHT_CONTROL;
  case TTB_KEY_LEFT_ALT:
    return KEY_LEFT_ALT;
  case TTB_KEY_RIGHT_ALT:
    return KEY_RIGHT_ALT;
  case TTB_KEY_LEFT_SUPER:
    return KEY_LEFT_SUPER;
  case TTB_KEY_RIGHT_SUPER:
    return KEY_RIGHT_SUPER;

  // Control keys
  case TTB_KEY_ESCAPE:
    return KEY_ESCAPE;
  case TTB_KEY_ENTER:
    return KEY_ENTER;
  case TTB_KEY_TAB:
    return KEY_TAB;
  case TTB_KEY_BACKSPACE:
    return KEY_BACKSPACE;
  case TTB_KEY_DELETE:
    return KEY_DELETE;
  case TTB_KEY_INSERT:
    return KEY_INSERT;
  case TTB_KEY_HOME:
    return KEY_HOME;
  case TTB_KEY_END:
    return KEY_END;
  case TTB_KEY_PAGE_UP:
    return KEY_PAGE_UP;
  case TTB_KEY_PAGE_DOWN:
    return KEY_PAGE_DOWN;
  case TTB_KEY_CAPS_LOCK:
    return KEY_CAPS_LOCK;
  case TTB_KEY_PRINT_SCREEN:
    return KEY_PRINT_SCREEN;
  case TTB_KEY_SCROLL_LOCK:
    return KEY_SCROLL_LOCK;
  case TTB_KEY_PAUSE:
    return KEY_PAUSE;
  case TTB_KEY_SPACE:
    return KEY_SPACE;

  // Punctuation
  case TTB_KEY_APOSTROPHE:
    return KEY_APOSTROPHE;
  case TTB_KEY_COMMA:
    return KEY_COMMA;
  case TTB_KEY_MINUS:
    return KEY_MINUS;
  case TTB_KEY_PERIOD:
    return KEY_PERIOD;
  case TTB_KEY_SLASH:
    return KEY_SLASH;
  case TTB_KEY_SEMICOLON:
    return KEY_SEMICOLON;
  case TTB_KEY_EQUAL:
    return KEY_EQUAL;
  case TTB_KEY_LEFT_BRACKET:
    return KEY_LEFT_BRACKET;
  case TTB_KEY_BACKSLASH:
    return KEY_BACKSLASH;
  case TTB_KEY_RIGHT_BRACKET:
    return KEY_RIGHT_BRACKET;
  case TTB_KEY_GRAVE:
    return KEY_GRAVE;

  // Numpad
  case TTB_KEY_NUM_LOCK:
    return KEY_NUM_LOCK;
  case TTB_KEY_NUMPAD_0:
    return KEY_KP_0;
  case TTB_KEY_NUMPAD_1:
    return KEY_KP_1;
  case TTB_KEY_NUMPAD_2:
    return KEY_KP_2;
  case TTB_KEY_NUMPAD_3:
    return KEY_KP_3;
  case TTB_KEY_NUMPAD_4:
    return KEY_KP_4;
  case TTB_KEY_NUMPAD_5:
    return KEY_KP_5;
  case TTB_KEY_NUMPAD_6:
    return KEY_KP_6;
  case TTB_KEY_NUMPAD_7:
    return KEY_KP_7;
  case TTB_KEY_NUMPAD_8:
    return KEY_KP_8;
  case TTB_KEY_NUMPAD_9:
    return KEY_KP_9;
  case TTB_KEY_NUMPAD_ADD:
    return KEY_KP_ADD;
  case TTB_KEY_NUMPAD_SUBTRACT:
    return KEY_KP_SUBTRACT;
  case TTB_KEY_NUMPAD_MULTIPLY:
    return KEY_KP_MULTIPLY;
  case TTB_KEY_NUMPAD_DIVIDE:
    return KEY_KP_DIVIDE;
  case TTB_KEY_NUMPAD_DECIMAL:
    return KEY_KP_DECIMAL;
  case TTB_KEY_NUMPAD_ENTER:
    return KEY_KP_ENTER;

  default:
    return -1; // unknown key
  }
}
