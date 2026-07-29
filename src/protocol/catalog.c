/**
 * @file catalog.c
 * @brief Implements protocol version 1 GET_APP_LIST message schemas.
 */

#include <moonlight/protocol/control.h>
#include <moonlight/protocol/wire.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/**
 * @brief Largest field ID in a GET_APP_LIST request or response.
 */
#define APP_LIST_FIELD_MAX 2u

/**
 * @brief Largest field ID in a nested Application record.
 */
#define APPLICATION_RECORD_FIELD_MAX 3u

/**
 * @brief Reliably clears temporary catalog storage.
 *
 * Volatile byte stores prevent the compiler from removing the wipe after the
 * final non-volatile use of an opaque continuation cursor.
 *
 * @param data Writable storage to clear.
 * @param data_size Number of bytes to clear.
 */
static void catalog_secure_clear(void *data, size_t data_size) {
  volatile uint8_t *cursor = (volatile uint8_t *) data;

  while (data_size != 0) {
    *cursor++ = 0;
    --data_size;
  }
}

/**
 * @brief Stores one network-order 16-bit integer.
 *
 * @param output Two writable bytes.
 * @param value Host-order value.
 */
static void catalog_store_u16(uint8_t *output, uint16_t value) {
  output[0] = (uint8_t) (value >> 8u);
  output[1] = (uint8_t) value;
}

/**
 * @brief Loads one network-order 16-bit integer.
 *
 * @param input Two readable bytes.
 * @return Host-order value.
 */
static uint16_t catalog_load_u16(const uint8_t *input) {
  return (uint16_t) (((uint16_t) input[0] << 8u) | input[1]);
}

/**
 * @brief Tests whether bytes are a shortest-form Unicode scalar sequence.
 *
 * @param input Candidate UTF-8 bytes.
 * @param input_size Number of candidate bytes.
 * @return True only for canonical UTF-8 without NUL or surrogate code points.
 */
static bool catalog_utf8_is_valid(
  const uint8_t *input,
  size_t input_size
) {
  size_t offset = 0;

  while (offset < input_size) {
    const uint8_t first = input[offset];
    size_t continuation_count;
    uint32_t code_point;
    uint32_t minimum;
    size_t index;

    if (first <= 0x7fu) {
      if (first == 0) {
        return false;
      }
      ++offset;
      continue;
    }
    if (first >= 0xc2u && first <= 0xdfu) {
      continuation_count = 1;
      code_point = first & 0x1fu;
      minimum = 0x80u;
    } else if (first >= 0xe0u && first <= 0xefu) {
      continuation_count = 2;
      code_point = first & 0x0fu;
      minimum = 0x800u;
    } else if (first >= 0xf0u && first <= 0xf4u) {
      continuation_count = 3;
      code_point = first & 0x07u;
      minimum = 0x10000u;
    } else {
      return false;
    }
    if (continuation_count > input_size - offset - 1u) {
      return false;
    }
    for (index = 1; index <= continuation_count; ++index) {
      const uint8_t continuation = input[offset + index];

      if ((continuation & 0xc0u) != 0x80u) {
        return false;
      }
      code_point = (code_point << 6u) | (continuation & 0x3fu);
    }
    if (code_point < minimum || code_point > 0x10ffffu || (code_point >= 0xd800u && code_point <= 0xdfffu)) {
      return false;
    }
    offset += continuation_count + 1u;
  }
  return true;
}

/**
 * @brief Compares two Application IDs by their raw wire bytes.
 *
 * @param left Left record with a validated ID size.
 * @param right Right record with a validated ID size.
 * @return A negative, zero, or positive value for lexical ordering.
 */
static int catalog_compare_application_ids(
  const MoonlightProtocolV1ApplicationRecord *left,
  const MoonlightProtocolV1ApplicationRecord *right
) {
  const size_t common_size =
    left->application_id_size < right->application_id_size ?
      left->application_id_size :
      right->application_id_size;
  const int common_result = memcmp(
    left->application_id,
    right->application_id,
    common_size
  );

  if (common_result != 0) {
    return common_result;
  }
  if (left->application_id_size < right->application_id_size) {
    return -1;
  }
  if (left->application_id_size > right->application_id_size) {
    return 1;
  }
  return 0;
}

/**
 * @brief Validates one host-order GET_APP_LIST request.
 *
 * @param request Candidate request.
 * @return The codec result.
 */
static MoonlightProtocolResult catalog_validate_request(
  const MoonlightProtocolV1GetAppListRequest *request
) {
  if (request == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (request->cursor_size != 0 && request->cursor_size != MOONLIGHT_PROTOCOL_V1_APP_LIST_CURSOR_SIZE) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  if (request->maximum_entries > MOONLIGHT_PROTOCOL_V1_APP_LIST_MAX_ENTRIES) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Validates one host-order Application record.
 *
 * @param record Candidate record.
 * @return The codec result.
 */
static MoonlightProtocolResult catalog_validate_record(
  const MoonlightProtocolV1ApplicationRecord *record
) {
  if (record->application_id_size == 0 || record->application_id_size > MOONLIGHT_PROTOCOL_V1_APPLICATION_ID_MAX || !catalog_utf8_is_valid(record->application_id, record->application_id_size)) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  if (record->display_name_size == 0 || record->display_name_size > MOONLIGHT_PROTOCOL_V1_APPLICATION_DISPLAY_NAME_MAX || !catalog_utf8_is_valid(record->display_name, record->display_name_size)) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  if (record->icon_asset_sha256_size != 0 && record->icon_asset_sha256_size != MOONLIGHT_PROTOCOL_V1_APPLICATION_ICON_DIGEST_SIZE) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Validates one host-order GET_APP_LIST response.
 *
 * @param response Candidate response.
 * @return The codec result.
 */
static MoonlightProtocolResult catalog_validate_response(
  const MoonlightProtocolV1GetAppListResponse *response
) {
  size_t index;
  MoonlightProtocolResult result;

  if (response == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (response->entry_count > MOONLIGHT_PROTOCOL_V1_APP_LIST_MAX_ENTRIES) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  if (response->next_cursor_size != 0 && response->next_cursor_size != MOONLIGHT_PROTOCOL_V1_APP_LIST_CURSOR_SIZE) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  if (response->entry_count == 0 && response->next_cursor_size != 0) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  for (index = 0; index < response->entry_count; ++index) {
    result = catalog_validate_record(&response->entries[index]);
    if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
      return result;
    }
    if (index != 0 && catalog_compare_application_ids(&response->entries[index - 1u], &response->entries[index]) >= 0) {
      return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
    }
  }
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Appends one canonical field to a fixed local buffer.
 *
 * @param output Destination positioned at the field header.
 * @param field_id Field ID.
 * @param flags Canonical schema flags.
 * @param value Field bytes.
 * @param value_size Number of field bytes.
 * @return Number of encoded bytes.
 */
static size_t catalog_encode_field(
  uint8_t *output,
  uint16_t field_id,
  uint16_t flags,
  const uint8_t *value,
  size_t value_size
) {
  catalog_store_u16(output, field_id);
  catalog_store_u16(output + 2u, flags);
  output[4] = (uint8_t) (value_size >> 24u);
  output[5] = (uint8_t) (value_size >> 16u);
  output[6] = (uint8_t) (value_size >> 8u);
  output[7] = (uint8_t) value_size;
  memcpy(
    output + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
    value,
    value_size
  );
  return MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + value_size;
}

/**
 * @brief Encodes one validated nested Application record.
 *
 * @param record Validated record.
 * @param output Maximum-size nested-record destination.
 * @return Number of encoded bytes.
 */
static size_t catalog_encode_record(
  const MoonlightProtocolV1ApplicationRecord *record,
  uint8_t *output
) {
  size_t size = 0;

  size += catalog_encode_field(
    output + size,
    1,
    0,
    record->application_id,
    record->application_id_size
  );
  size += catalog_encode_field(
    output + size,
    2,
    0,
    record->display_name,
    record->display_name_size
  );
  if (record->icon_asset_sha256_size != 0) {
    size += catalog_encode_field(
      output + size,
      3,
      0,
      record->icon_asset_sha256,
      record->icon_asset_sha256_size
    );
  }
  return size;
}

/**
 * @brief Decodes one complete contiguous TLV field.
 *
 * Truncated headers and values are normalized to a malformed complete
 * message. Unknown flag bits remain unsupported.
 *
 * @param input Complete containing payload.
 * @param input_size Number of containing bytes.
 * @param offset Current field offset, advanced on success.
 * @param field Receives the validated header.
 * @param value Receives a borrowed value pointer.
 * @return The codec result.
 */
static MoonlightProtocolResult catalog_decode_field(
  const uint8_t *input,
  size_t input_size,
  size_t *offset,
  MoonlightProtocolV1TlvField *field,
  const uint8_t **value
) {
  const size_t remaining = input_size - *offset;
  MoonlightProtocolResult result;

  result = MoonlightProtocolV1DecodeTlvFieldHeader(
    input + *offset,
    remaining,
    field
  );
  if (result == MOONLIGHT_PROTOCOL_RESULT_TRUNCATED) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (field->field_length > remaining - MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }

  *value = input + *offset + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE;
  *offset += MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + field->field_length;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Classifies an unexpected field ID within one known schema.
 *
 * @param field_id Unexpected field ID.
 * @param maximum_known_id Largest field ID in the schema.
 * @return Malformed for a known duplicate/order violation, otherwise unsupported.
 */
static MoonlightProtocolResult catalog_classify_unexpected_id(
  uint16_t field_id,
  uint16_t maximum_known_id
) {
  return field_id >= 1u && field_id <= maximum_known_id ?
           MOONLIGHT_PROTOCOL_RESULT_MALFORMED :
           MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED;
}

/**
 * @brief Decodes one required nested Application-record scalar field.
 *
 * @param input Complete nested record.
 * @param input_size Number of nested bytes.
 * @param offset Current nested offset, advanced on success.
 * @param expected_id Exact required field ID.
 * @param value Receives a borrowed field value.
 * @param value_size Receives the field-value size.
 * @return The codec result.
 */
static MoonlightProtocolResult catalog_decode_required_record_field(
  const uint8_t *input,
  size_t input_size,
  size_t *offset,
  uint16_t expected_id,
  const uint8_t **value,
  size_t *value_size
) {
  MoonlightProtocolV1TlvField field;
  MoonlightProtocolResult result;

  if (*offset == input_size) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  result = catalog_decode_field(
    input,
    input_size,
    offset,
    &field,
    value
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (field.field_id != expected_id) {
    return catalog_classify_unexpected_id(
      field.field_id,
      APPLICATION_RECORD_FIELD_MAX
    );
  }
  if (field.flags != 0) {
    return MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED;
  }
  *value_size = field.field_length;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Decodes one complete nested Application record.
 *
 * @param input Complete nested record bytes.
 * @param input_size Number of nested bytes.
 * @param record Receives validated owned values only on success.
 * @return The codec result.
 */
static MoonlightProtocolResult catalog_decode_record(
  const uint8_t *input,
  size_t input_size,
  MoonlightProtocolV1ApplicationRecord *record
) {
  MoonlightProtocolV1ApplicationRecord decoded;
  MoonlightProtocolV1TlvField field;
  const uint8_t *application_id;
  const uint8_t *display_name;
  const uint8_t *icon_digest = NULL;
  const uint8_t *value;
  size_t application_id_size;
  size_t display_name_size;
  size_t icon_digest_size = 0;
  size_t offset = 0;
  MoonlightProtocolResult result;

  memset(&decoded, 0, sizeof(decoded));
  result = catalog_decode_required_record_field(
    input,
    input_size,
    &offset,
    1,
    &application_id,
    &application_id_size
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  result = catalog_decode_required_record_field(
    input,
    input_size,
    &offset,
    2,
    &display_name,
    &display_name_size
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (offset != input_size) {
    result = catalog_decode_field(
      input,
      input_size,
      &offset,
      &field,
      &value
    );
    if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
      return result;
    }
    if (field.field_id != 3u) {
      return catalog_classify_unexpected_id(
        field.field_id,
        APPLICATION_RECORD_FIELD_MAX
      );
    }
    if (field.flags != 0) {
      return MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED;
    }
    icon_digest = value;
    icon_digest_size = field.field_length;
  }
  if (offset != input_size) {
    result = catalog_decode_field(
      input,
      input_size,
      &offset,
      &field,
      &value
    );
    if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
      return result;
    }
    return catalog_classify_unexpected_id(
      field.field_id,
      APPLICATION_RECORD_FIELD_MAX
    );
  }

  if (application_id_size <= sizeof(decoded.application_id)) {
    memcpy(
      decoded.application_id,
      application_id,
      application_id_size
    );
  }
  decoded.application_id_size = application_id_size;
  if (display_name_size <= sizeof(decoded.display_name)) {
    memcpy(decoded.display_name, display_name, display_name_size);
  }
  decoded.display_name_size = display_name_size;
  if (icon_digest_size <= sizeof(decoded.icon_asset_sha256) && icon_digest != NULL) {
    memcpy(
      decoded.icon_asset_sha256,
      icon_digest,
      icon_digest_size
    );
  }
  decoded.icon_asset_sha256_size = icon_digest_size;

  result = catalog_validate_record(&decoded);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  *record = decoded;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

MoonlightProtocolResult MoonlightProtocolV1EncodeGetAppListRequest(
  const MoonlightProtocolV1GetAppListRequest *request,
  uint8_t *output,
  size_t output_size,
  size_t *encoded_size
) {
  uint8_t encoded[MOONLIGHT_PROTOCOL_V1_GET_APP_LIST_REQUEST_PAYLOAD_MAX];
  uint8_t maximum_entries[2];
  size_t size = 0;
  MoonlightProtocolResult result;

  if (encoded_size == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  result = catalog_validate_request(request);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (request->cursor_size != 0) {
    size += catalog_encode_field(
      encoded + size,
      1,
      0,
      request->cursor,
      request->cursor_size
    );
  }
  if (request->maximum_entries != 0) {
    catalog_store_u16(maximum_entries, request->maximum_entries);
    size += catalog_encode_field(
      encoded + size,
      2,
      0,
      maximum_entries,
      sizeof(maximum_entries)
    );
  }
  if (size != 0 && output == NULL) {
    result = MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
    goto cleanup;
  }
  if (output_size < size) {
    result = MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL;
    goto cleanup;
  }
  if (size != 0) {
    memcpy(output, encoded, size);
  }
  *encoded_size = size;
  result = MOONLIGHT_PROTOCOL_RESULT_OK;

cleanup:
  catalog_secure_clear(maximum_entries, sizeof(maximum_entries));
  catalog_secure_clear(encoded, sizeof(encoded));
  return result;
}

MoonlightProtocolResult MoonlightProtocolV1DecodeGetAppListRequest(
  const uint8_t *input,
  size_t input_size,
  MoonlightProtocolV1GetAppListRequest *request
) {
  MoonlightProtocolV1GetAppListRequest decoded;
  MoonlightProtocolV1TlvField field;
  const uint8_t *value;
  size_t offset = 0;
  uint16_t last_field_id = 0;
  MoonlightProtocolResult result;

  if (request == NULL || (input == NULL && input_size != 0)) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (input_size > MOONLIGHT_PROTOCOL_V1_GET_APP_LIST_REQUEST_PAYLOAD_MAX) {
    return MOONLIGHT_PROTOCOL_RESULT_LIMIT_EXCEEDED;
  }

  memset(&decoded, 0, sizeof(decoded));
  while (offset != input_size) {
    result = catalog_decode_field(
      input,
      input_size,
      &offset,
      &field,
      &value
    );
    if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
      goto cleanup;
    }
    if (field.field_id < 1u || field.field_id > APP_LIST_FIELD_MAX) {
      result = MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED;
      goto cleanup;
    }
    if (field.field_id <= last_field_id) {
      result = MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
      goto cleanup;
    }
    if (field.flags != 0) {
      result = MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED;
      goto cleanup;
    }
    if (field.field_id == 1u) {
      if (field.field_length != MOONLIGHT_PROTOCOL_V1_APP_LIST_CURSOR_SIZE) {
        result = MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
        goto cleanup;
      }
      memcpy(decoded.cursor, value, sizeof(decoded.cursor));
      decoded.cursor_size = sizeof(decoded.cursor);
    } else {
      if (field.field_length != 2u) {
        result = MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
        goto cleanup;
      }
      decoded.maximum_entries = catalog_load_u16(value);
      if (decoded.maximum_entries == 0) {
        result = MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
        goto cleanup;
      }
    }
    last_field_id = field.field_id;
  }

  result = catalog_validate_request(&decoded);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    goto cleanup;
  }
  *request = decoded;
  result = MOONLIGHT_PROTOCOL_RESULT_OK;

cleanup:
  catalog_secure_clear(&decoded, sizeof(decoded));
  return result;
}

MoonlightProtocolResult MoonlightProtocolV1EncodeGetAppListResponse(
  const MoonlightProtocolV1GetAppListResponse *response,
  uint8_t *output,
  size_t output_size,
  size_t *encoded_size
) {
  uint8_t encoded[MOONLIGHT_PROTOCOL_V1_GET_APP_LIST_RESPONSE_PAYLOAD_MAX];
  uint8_t record[MOONLIGHT_PROTOCOL_V1_APPLICATION_RECORD_PAYLOAD_MAX];
  size_t size = 0;
  size_t index;
  MoonlightProtocolResult result;

  if (encoded_size == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  result = catalog_validate_response(response);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }

  for (index = 0; index < response->entry_count; ++index) {
    const size_t record_size = catalog_encode_record(
      &response->entries[index],
      record
    );

    size += catalog_encode_field(
      encoded + size,
      1,
      MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED,
      record,
      record_size
    );
  }
  if (response->next_cursor_size != 0) {
    size += catalog_encode_field(
      encoded + size,
      2,
      0,
      response->next_cursor,
      response->next_cursor_size
    );
  }
  if (size != 0 && output == NULL) {
    result = MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
    goto cleanup;
  }
  if (output_size < size) {
    result = MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL;
    goto cleanup;
  }
  if (size != 0) {
    memcpy(output, encoded, size);
  }
  *encoded_size = size;
  result = MOONLIGHT_PROTOCOL_RESULT_OK;

cleanup:
  catalog_secure_clear(record, sizeof(record));
  catalog_secure_clear(encoded, sizeof(encoded));
  return result;
}

MoonlightProtocolResult MoonlightProtocolV1DecodeGetAppListResponse(
  const uint8_t *input,
  size_t input_size,
  MoonlightProtocolV1GetAppListResponse *response
) {
  MoonlightProtocolV1GetAppListResponse decoded;
  MoonlightProtocolV1TlvField field;
  const uint8_t *value;
  size_t offset = 0;
  bool saw_cursor = false;
  MoonlightProtocolResult result;

  if (response == NULL || (input == NULL && input_size != 0)) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (input_size > MOONLIGHT_PROTOCOL_V1_GET_APP_LIST_RESPONSE_PAYLOAD_MAX) {
    return MOONLIGHT_PROTOCOL_RESULT_LIMIT_EXCEEDED;
  }

  memset(&decoded, 0, sizeof(decoded));
  while (offset != input_size) {
    result = catalog_decode_field(
      input,
      input_size,
      &offset,
      &field,
      &value
    );
    if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
      goto cleanup;
    }
    if (field.field_id < 1u || field.field_id > APP_LIST_FIELD_MAX) {
      result = MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED;
      goto cleanup;
    }
    if (field.field_id == 1u) {
      if (saw_cursor) {
        result = MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
        goto cleanup;
      }
      if (field.flags != MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED) {
        result = MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED;
        goto cleanup;
      }
      if (decoded.entry_count >= MOONLIGHT_PROTOCOL_V1_APP_LIST_MAX_ENTRIES) {
        result = MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
        goto cleanup;
      }
      if (field.field_length < MOONLIGHT_PROTOCOL_V1_APPLICATION_RECORD_PAYLOAD_MIN || field.field_length > MOONLIGHT_PROTOCOL_V1_APPLICATION_RECORD_PAYLOAD_MAX) {
        result = MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
        goto cleanup;
      }
      result = catalog_decode_record(
        value,
        field.field_length,
        &decoded.entries[decoded.entry_count]
      );
      if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
        goto cleanup;
      }
      ++decoded.entry_count;
    } else {
      if (saw_cursor) {
        result = MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
        goto cleanup;
      }
      if (field.flags != 0) {
        result = MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED;
        goto cleanup;
      }
      if (field.field_length != MOONLIGHT_PROTOCOL_V1_APP_LIST_CURSOR_SIZE) {
        result = MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
        goto cleanup;
      }
      memcpy(decoded.next_cursor, value, sizeof(decoded.next_cursor));
      decoded.next_cursor_size = sizeof(decoded.next_cursor);
      saw_cursor = true;
    }
  }

  result = catalog_validate_response(&decoded);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    goto cleanup;
  }
  *response = decoded;
  result = MOONLIGHT_PROTOCOL_RESULT_OK;

cleanup:
  catalog_secure_clear(&decoded, sizeof(decoded));
  return result;
}
