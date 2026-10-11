#pragma once

#include "lilc/numbers.h"
#include "tt_shared.h"


typedef struct tt_camera {
  u64 cam_id;

  i32 target_x;
  i32 target_y;
  i32 offset_x;
  i32 offset_y;

  f32 rotation;
  f32 zoom;

  tt_backend_t *backend;
} tt_camera_t;

void tt_camera_init(tt_camera_t *cam, tt_backend_t *backend);
