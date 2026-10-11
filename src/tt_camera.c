#include "../include/tt_camera.h"
#include "../include/tt_backend.h"

#include "lilc/log.h"
#include "lilc/alloc.h"

void tt_camera_init(tt_camera_t *cam, tt_backend_t *backend) {
  cam->zoom = 1;
  
  if (!ttb_camera_make_id(backend, &cam->cam_id)) {
    log_error("Failed to create camera");
    return;
  }

}
