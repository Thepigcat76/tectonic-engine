#include "../include/tt_data.h"
#include "../vendor/cJSON.h"
#include "lilc/alloc.h"
#include "lilc/dynstr.h"
#include "lilc/file.h"
#include "lilc/log.h"
#include <stdio.h>

#define TT_DATA_LIST_DEFAULT_CAP 64
#define TT_DATA_OBJECT_DEFAULT_CAP 64

void tt_data_list_init(tt_data_list_t *list, allocator_t *alloc) {
  list->values =
      alloc->alloc(alloc, TT_DATA_LIST_DEFAULT_CAP * sizeof(tt_data_value_t));
  list->len = 0;
  list->capacity = TT_DATA_LIST_DEFAULT_CAP;
  list->alloc = alloc;
}

void tt_data_list_add(tt_data_list_t *list, tt_data_value_t value) {
  if (list->capacity >= list->len + 1) {
    list->values = list->alloc->realloc(
        list->alloc, list->values,
        list->capacity * TT_DATA_LIST_DEFAULT_CAP * sizeof(tt_data_value_t),
        list->capacity * TT_DATA_LIST_DEFAULT_CAP * sizeof(tt_data_value_t) *
            2);
    list->capacity *= 2;
  }

  list->values[list->len] = value;
  ++list->len;
}

void tt_data_object_init(tt_data_object_t *object, allocator_t *alloc) {
  object->capacity = TT_DATA_OBJECT_DEFAULT_CAP;
  object->len = 0;
  object->keys =
      alloc->alloc(alloc, TT_DATA_LIST_DEFAULT_CAP * sizeof(dyn_string_t));
  object->values =
      alloc->alloc(alloc, TT_DATA_LIST_DEFAULT_CAP * sizeof(tt_data_value_t));
  object->alloc = alloc;
}

void tt_data_object_insert(tt_data_object_t *object, dyn_string_t key,
                           tt_data_value_t value) {
  if (object->len + 1 >= object->capacity) {
    object->keys = object->alloc->realloc(
        object->alloc, object->keys, object->capacity * sizeof(dyn_string_t),
        object->capacity * sizeof(dyn_string_t) * 2);
    object->values = object->alloc->realloc(
        object->alloc, object->keys, object->capacity * sizeof(tt_data_value_t),
        object->capacity * sizeof(tt_data_value_t) * 2);
    object->capacity *= 2;
  }

  object->keys[object->len] = key;
  object->values[object->len] = value;
  object->len += 1;
}

tt_data_value_t *tt_data_object_get(tt_data_object_t *object, const char *key) {
  for (usz i = 0; i < object->len; i++) {
    if (str_eq(object->keys[i].string, key)) {
      return &object->values[i];
    }
  }

  return NULL;
}

static bool tt_data_encode_number(u64 num, usz num_size, u8 **bytes) {
  for (usz i = 0; i < num_size; i++) {
    array_add(*bytes, (u8)(num >> (((num_size - 1) - i) * 8)));
  }

  return true;
}

static bool tt_data_encode_string(dyn_string_t *str, u8 **bytes) {
  if (!tt_data_encode_number(str->len, sizeof(i64), bytes)) {
    log_error("Failed to encode string length");
    return false;
  }

  for (usz i = 0; i < str->len; i++) {
    array_add(*bytes, str->string[i]);
  }

  return true;
}

bool tt_data_value_write_bytes(tt_data_value_t *value, u8 **bytes) {
  array_add(*bytes, value->kind);

  switch (value->kind) {
  case TT_DATA_VALUE_I8: {
    array_add(*bytes, value->u.val_i8);
  } break;
  case TT_DATA_VALUE_I16: {
    if (!tt_data_encode_number(value->u.val_i16, sizeof(i16), bytes)) {
      log_error("Failed to encode number");
      return false;
    }
  } break;
  case TT_DATA_VALUE_I32: {
    if (!tt_data_encode_number(value->u.val_i32, sizeof(i32), bytes)) {
      log_error("Failed to encode number");
      return false;
    }
  } break;
  case TT_DATA_VALUE_I64: {
    if (!tt_data_encode_number(value->u.val_i64, sizeof(i64), bytes)) {
      log_error("Failed to encode number");
      return false;
    }
  } break;
  case TT_DATA_VALUE_STRING: {
    tt_data_encode_string(&value->u.val_string, bytes);
  } break;
  case TT_DATA_VALUE_LIST: {
    if (!tt_data_encode_number(value->u.val_list.len, sizeof(i64), bytes)) {
      log_error("Failed to encode list length");
      return false;
    }

    for (usz i = 0; i < value->u.val_list.len; i++) {
      tt_data_value_write_bytes(&value->u.val_list.values[i], bytes);
    }

  } break;
  case TT_DATA_VALUE_OBJECT: {
    if (!tt_data_encode_number(value->u.val_object.len, sizeof(i64), bytes)) {
      log_error("Failed to encode object length");
      return false;
    }

    for (usz i = 0; i < value->u.val_object.len; i++) {
      tt_data_encode_string(&value->u.val_object.keys[i], bytes);
      tt_data_value_write_bytes(&value->u.val_object.values[i], bytes);
    }

  } break;
  }

  return true;
}

bool tt_data_value_write(tt_data_value_t *value, const char *filepath) {
  u8 *bytes = array_new(u8, &HEAP_ALLOCATOR);
  bool success = tt_data_value_write_bytes(value, &bytes);
  if (!success) {
    return false;
  }

  FILE *f = fopen(filepath, "wb");
  fwrite(bytes, sizeof(u8), array_len(bytes), f);
  fclose(f);

  array_free(bytes);

  return true;
}

static bool tt_data_decode_number(u64 *out_num, usz num_size, usz *cur_byte_idx,
                                  const u8 *bytes, usz bytes_len) {
  if (out_num == NULL || cur_byte_idx == NULL || bytes == NULL ||
      num_size > sizeof(*out_num) || *cur_byte_idx > bytes_len ||
      bytes_len - *cur_byte_idx < num_size) {
    return false;
  }

  u64 value = 0;

  for (usz i = 0; i < num_size; ++i) {
    value = (value << 8) | bytes[*cur_byte_idx];
    *cur_byte_idx += 1;
  }

  *out_num = value;
  return true;
}

static bool tt_data_decode_string(dyn_string_t *str, usz *cur_byte_idx,
                                  u8 *bytes, usz bytes_len) {
  u64 string_len = 0;
  if (!tt_data_decode_number(&string_len, sizeof(i64), cur_byte_idx, bytes,
                             bytes_len)) {
    log_error("Failed to decode string length");
    return false;
  }

  dyn_string_ensure_size(str, string_len);

  if (string_len > bytes_len - *cur_byte_idx) {
    log_error("String length %zu exceeds remaining %zu bytes", string_len,
              bytes_len - *cur_byte_idx);
    return false;
  }

  for (u64 i = 0; i < string_len; i++) {
    dyn_string_add_char(str, bytes[(*cur_byte_idx)++]);
  }

  return true;
}

static bool tt_data_value_read_bytes(tt_data_value_t *value, u8 *bytes,
                                     usz *cur_byte_idx, usz bytes_len,
                                     allocator_t *alloc) {
  value->kind = bytes[*cur_byte_idx];

  *cur_byte_idx += 1;
  if (*cur_byte_idx >= bytes_len) {
    return false;
  }

  switch (value->kind) {
  case TT_DATA_VALUE_I8: {
    value->u.val_i8 = bytes[*cur_byte_idx];
  } break;
  case TT_DATA_VALUE_I16: {
    if (!tt_data_decode_number((u64 *)&value->u.val_i16, sizeof(i16),
                               cur_byte_idx, bytes, bytes_len)) {
      log_error("Failed to decode number");
      return false;
    }
  } break;
  case TT_DATA_VALUE_I32: {
    if (!tt_data_decode_number((u64 *)&value->u.val_i32, sizeof(i32),
                               cur_byte_idx, bytes, bytes_len)) {
      log_error("Failed to decode number");
      return false;
    }
  } break;
  case TT_DATA_VALUE_I64: {
    if (!tt_data_decode_number((u64 *)&value->u.val_i64, sizeof(i64),
                               cur_byte_idx, bytes, bytes_len)) {
      log_error("Failed to decode number");
      return false;
    }
  } break;
  case TT_DATA_VALUE_STRING: {
    dyn_string_init(&value->u.val_string, alloc);

    if (!tt_data_decode_string(&value->u.val_string, cur_byte_idx, bytes,
                               bytes_len)) {
      log_error("Failed to decode key for object");
      return false;
    }
  } break;
  case TT_DATA_VALUE_LIST: {
    u64 list_len = 0;
    if (!tt_data_decode_number(&list_len, sizeof(i64), cur_byte_idx, bytes,
                               bytes_len)) {
      log_error("Failed to decode list length");
      return false;
    }

    tt_data_list_init(&value->u.val_list, alloc);

    for (u32 i = 0; i < list_len; i++) {
      tt_data_value_t data_val = {0};
      if (!tt_data_value_read_bytes(&data_val, bytes, cur_byte_idx, bytes_len,
                                    alloc)) {
        log_error("Failed to read data value");
        return false;
      }

      tt_data_list_add(&value->u.val_list, data_val);
    }

  } break;
  case TT_DATA_VALUE_OBJECT: {
    u64 object_len = 0;
    if (!tt_data_decode_number(&object_len, sizeof(i64), cur_byte_idx, bytes,
                               bytes_len)) {
      log_error("Failed to decode object length");
      return false;
    }

    tt_data_object_init(&value->u.val_object, alloc);

    for (u32 i = 0; i < object_len; i++) {
      dyn_string_t key_str = {0};
      dyn_string_init(&key_str, alloc);

      if (!tt_data_decode_string(&key_str, cur_byte_idx, bytes, bytes_len)) {
        log_error("Failed to decode key for object");
        return false;
      }

      tt_data_value_t data_val = {0};
      if (!tt_data_value_read_bytes(&data_val, bytes, cur_byte_idx, bytes_len,
                                    alloc)) {
        log_error("Failed to read data value");
        return false;
      }

      tt_data_object_insert(&value->u.val_object, key_str, data_val);
    }

  } break;
  default: {
    log_error("Unknown data value kind %u at idx: %zu", bytes[*cur_byte_idx],
              *cur_byte_idx);
    return false;
  } break;
  }

  return true;
}

bool tt_data_value_read(tt_data_value_t *value, const char *filepath,
                        allocator_t *alloc) {
  usz len;
  u8 *bytes = file_read_to_bytes(filepath, &len, &HEAP_ALLOCATOR);
  for (usz i = 0; i < len; i++) {
    printf("%u, ", bytes[i]);
  }
  puts("");
  usz idx = 0;
  return tt_data_value_read_bytes(value, bytes, &idx, len, alloc);
}

static bool tt_data_value_dump_json(tt_data_value_t *value, cJSON **json) {
  switch (value->kind) {
  case TT_DATA_VALUE_I8: {
    *json = cJSON_CreateNumber(value->u.val_i8);
  } break;
  case TT_DATA_VALUE_I16: {
    *json = cJSON_CreateNumber(value->u.val_i16);
  } break;
  case TT_DATA_VALUE_I32: {
    *json = cJSON_CreateNumber(value->u.val_i32);
  } break;
  case TT_DATA_VALUE_I64: {
    *json = cJSON_CreateNumber(value->u.val_i64);
  } break;
  case TT_DATA_VALUE_STRING: {
    *json = cJSON_CreateString(value->u.val_string.string);
  } break;
  case TT_DATA_VALUE_LIST: {
    *json = cJSON_CreateArray();

    for (usz i = 0; i < value->u.val_list.len; i++) {
      cJSON *val;
      tt_data_value_dump_json(&value->u.val_list.values[i], &val);
      cJSON_AddItemToArray(*json, val);
    }
  } break;
  case TT_DATA_VALUE_OBJECT: {
    *json = cJSON_CreateObject();

    for (usz i = 0; i < value->u.val_object.len; i++) {
      cJSON *val;
      tt_data_value_dump_json(&value->u.val_object.values[i], &val);
      cJSON_AddItemToObject(*json, value->u.val_object.keys[i].string, val);
    }
  } break;
  }

  return true;
}

bool tt_data_value_dump(tt_data_value_t *value, dyn_string_t *fmt_buf) {
  cJSON *json;
  if (!tt_data_value_dump_json(value, &json)) {
    return false;
  }

  dyn_string_add_str(fmt_buf, cJSON_Print(json));

  cJSON_Delete(json);

  return true;
}
