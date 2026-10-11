#pragma once

#include "lilc/alloc.h"
#include "lilc/dynstr.h"
#include "lilc/numbers.h"

typedef struct tt_data_list_t {
  struct tt_data_value *values;
  usz len;
  usz capacity;
  allocator_t *alloc;
} tt_data_list_t;

typedef struct tt_data_object_t {
  dyn_string_t *keys;
  struct tt_data_value *values;
  usz len;
  usz capacity;
  allocator_t *alloc;
} tt_data_object_t;

typedef struct tt_data_value {
  enum {
    TT_DATA_VALUE_I8 = 0x1,
    TT_DATA_VALUE_I16 = 0x2,
    TT_DATA_VALUE_I32 = 0x3,
    TT_DATA_VALUE_I64 = 0x4,
    TT_DATA_VALUE_STRING = 0x5,
    TT_DATA_VALUE_LIST = 0x6,
    TT_DATA_VALUE_OBJECT = 0x7,
  } kind;
  union {
    i8 val_i8;
    i16 val_i16;
    i32 val_i32;
    i64 val_i64;
    dyn_string_t val_string;
    tt_data_list_t val_list;
    tt_data_object_t val_object;
  } u;
} tt_data_value_t;

#define tt_data_value_i8(...)                                                  \
  (tt_data_value_t) { .kind = TT_DATA_VALUE_I8, .u.val_i8 = __VA_ARGS__ }
#define tt_data_value_i16(...)                                                 \
  (tt_data_value_t) { .kind = TT_DATA_VALUE_I16, .u.val_i16 = __VA_ARGS__ }
#define tt_data_value_i32(...)                                                 \
  (tt_data_value_t) { .kind = TT_DATA_VALUE_I32, .u.val_i32 = __VA_ARGS__ }
#define tt_data_value_i64(...)                                                 \
  (tt_data_value_t) { .kind = TT_DATA_VALUE_I64, .u.val_i64 = __VA_ARGS__ }
#define tt_data_value_string(alloc, ...)                                              \
  (tt_data_value_t) {                                                          \
    .kind = TT_DATA_VALUE_STRING, .u.val_string = dyn_string_makef(alloc, __VA_ARGS__)                  \
  }
#define tt_data_value_dyn_string(...)                                              \
  (tt_data_value_t) {                                                          \
    .kind = TT_DATA_VALUE_STRING, .u.val_string = __VA_ARGS__                  \
  }
#define tt_data_value_list(...)                                              \
  (tt_data_value_t) {                                                          \
    .kind = TT_DATA_VALUE_LIST, .u.val_list = __VA_ARGS__                  \
  }
#define tt_data_value_object(...)                                              \
  (tt_data_value_t) {                                                          \
    .kind = TT_DATA_VALUE_OBJECT, .u.val_object = __VA_ARGS__                  \
  }

void tt_data_list_init(tt_data_list_t *list, allocator_t *alloc);

void tt_data_list_add(tt_data_list_t *list, tt_data_value_t value);

void tt_data_object_init(tt_data_object_t *object, allocator_t *alloc);

void tt_data_object_insert(tt_data_object_t *object, dyn_string_t key,
                           tt_data_value_t value);

tt_data_value_t *tt_data_object_get(tt_data_object_t *object, const char *key);

// If file ending is json, data will be written as plain json, otherwise
// the data will be encoded
bool tt_data_value_write(tt_data_value_t *value, const char *filepath);

bool tt_data_value_read(tt_data_value_t *value, const char *filepath,
                        allocator_t *alloc);

bool tt_data_value_dump(tt_data_value_t *value, dyn_string_t *fmt_buf);
