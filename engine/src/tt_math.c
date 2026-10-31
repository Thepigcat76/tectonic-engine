#include "../include/tt_math.h"

bool tt_collides(f32 x0, f32 y0, i32 width0, i32 height0, f32 x1,
                 f32 y1, i32 width1, i32 height1) {
  return x0 < x1 + width1 && x1 < x0 + width0 && y0 < y1 + height1 &&
         y1 < y0 + height0;
}
