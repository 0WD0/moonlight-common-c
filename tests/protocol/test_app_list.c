/**
 * @file test_app_list.c
 * @brief Native tests for protocol version 1 GET_APP_LIST schemas.
 */

#include <moonlight/protocol/control.h>
#include <moonlight/protocol/wire.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef MOONLIGHT_PROTOCOL_V1_TESTDATA_DIR
  #error MOONLIGHT_PROTOCOL_V1_TESTDATA_DIR must name the version 1 golden-vector directory
#endif

/**
 * @brief Complete byte count of the GET_APP_LIST response golden.
 */
#define TEST_APP_LIST_GOLDEN_SIZE 173u

/**
 * @brief Fails the current boolean test when a condition is false.
 */
#define TEST_CHECK(condition) \
  do { \
    if (!(condition)) { \
      fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
      return false; \
    } \
  } while (0)

/**
 * @brief Fails the current boolean test when a codec result differs.
 */
#define TEST_RESULT(expression, expected) \
  do { \
    const MoonlightProtocolResult test_result_value = (expression); \
    if (test_result_value != (expected)) { \
      fprintf( \
        stderr, \
        "%s:%d: result %d, expected %d: %s\n", \
        __FILE__, \
        __LINE__, \
        (int) test_result_value, \
        (int) (expected), \
        #expression \
      ); \
      return false; \
    } \
  } while (0)

/**
 * @brief Stores one big-endian 16-bit integer in a mutable test vector.
 *
 * @param output Two writable bytes.
 * @param value Host-order value.
 */
static void test_store_u16(uint8_t *output, uint16_t value) {
  output[0] = (uint8_t) (value >> 8u);
  output[1] = (uint8_t) value;
}

/**
 * @brief Stores one big-endian 32-bit integer in a mutable test vector.
 *
 * @param output Four writable bytes.
 * @param value Host-order value.
 */
static void test_store_u32(uint8_t *output, uint32_t value) {
  output[0] = (uint8_t) (value >> 24u);
  output[1] = (uint8_t) (value >> 16u);
  output[2] = (uint8_t) (value >> 8u);
  output[3] = (uint8_t) value;
}

/**
 * @brief Appends one direct TLV field to a test vector.
 *
 * @param output Destination positioned at the field header.
 * @param field_id Field ID.
 * @param flags Field flags.
 * @param value Field bytes.
 * @param value_size Number of field bytes.
 * @return Number of appended bytes.
 */
static size_t test_append_field(
  uint8_t *output,
  uint16_t field_id,
  uint16_t flags,
  const uint8_t *value,
  size_t value_size
) {
  test_store_u16(output, field_id);
  test_store_u16(output + 2u, flags);
  test_store_u32(output + 4u, (uint32_t) value_size);
  if (value_size != 0) {
    memcpy(
      output + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
      value,
      value_size
    );
  }
  return MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + value_size;
}

/**
 * @brief Creates one bounded record from ASCII text.
 *
 * @param application_id NUL-terminated Application ID.
 * @param display_name NUL-terminated display name.
 * @param with_icon Whether to populate the optional icon digest.
 * @return Initialized record.
 */
static MoonlightProtocolV1ApplicationRecord make_record(
  const char *application_id,
  const char *display_name,
  bool with_icon
) {
  MoonlightProtocolV1ApplicationRecord record;
  size_t index;

  memset(&record, 0, sizeof(record));
  record.application_id_size = strlen(application_id);
  memcpy(
    record.application_id,
    application_id,
    record.application_id_size
  );
  record.display_name_size = strlen(display_name);
  memcpy(record.display_name, display_name, record.display_name_size);
  if (with_icon) {
    for (index = 0; index < sizeof(record.icon_asset_sha256); ++index) {
      record.icon_asset_sha256[index] = (uint8_t) index;
    }
    record.icon_asset_sha256_size = sizeof(record.icon_asset_sha256);
  }
  return record;
}

/**
 * @brief Creates the two-record response represented by the golden.
 *
 * @return Initialized response.
 */
static MoonlightProtocolV1GetAppListResponse golden_response(void) {
  MoonlightProtocolV1GetAppListResponse response;
  static const uint8_t moon[] = {
    'M',
    'o',
    'o',
    'n',
    ' ',
    'G',
    'a',
    'm',
    'e',
    ' ',
    0xf0,
    0x9f,
    0x8c,
    0x99,
  };
  size_t index;

  memset(&response, 0, sizeof(response));
  response.entries[0] = make_record("desktop", "Desktop", false);
  response.entries[1] = make_record("game.moon", "placeholder", true);
  memset(
    response.entries[1].display_name,
    0,
    sizeof(response.entries[1].display_name)
  );
  memcpy(response.entries[1].display_name, moon, sizeof(moon));
  response.entries[1].display_name_size = sizeof(moon);
  response.entry_count = 2;
  for (index = 0; index < sizeof(response.next_cursor); ++index) {
    response.next_cursor[index] = (uint8_t) (0xa0u + index);
  }
  response.next_cursor_size = sizeof(response.next_cursor);
  return response;
}

/**
 * @brief Tests whether one character is an ASCII hexadecimal digit.
 *
 * @param character Character to test.
 * @return A value in `[0, 15]`, or `-1` when not hexadecimal.
 */
static int hex_value(int character) {
  if (character >= '0' && character <= '9') {
    return character - '0';
  }
  if (character >= 'a' && character <= 'f') {
    return character - 'a' + 10;
  }
  if (character >= 'A' && character <= 'F') {
    return character - 'A' + 10;
  }
  return -1;
}

/**
 * @brief Tests whether one character is permitted golden-vector whitespace.
 *
 * @param character Character to test.
 * @return True only for ASCII whitespace.
 */
static bool is_ascii_whitespace(int character) {
  return character == ' ' ||
         character == '\t' ||
         character == '\n' ||
         character == '\r' ||
         character == '\f' ||
         character == '\v';
}

/**
 * @brief Loads one strict hexadecimal golden vector.
 *
 * @param name File name relative to the version 1 corpus.
 * @param output Destination byte buffer.
 * @param output_capacity Available bytes in `output`.
 * @param output_size Receives the decoded byte count.
 * @return True only for complete hexadecimal pairs and ASCII whitespace.
 */
static bool load_golden(
  const char *name,
  uint8_t *output,
  size_t output_capacity,
  size_t *output_size
) {
  char path[1024];
  FILE *file;
  int character;
  int high_nibble = -1;
  int path_length;
  size_t size = 0;
  bool valid = true;

  path_length = snprintf(
    path,
    sizeof(path),
    "%s/%s",
    MOONLIGHT_PROTOCOL_V1_TESTDATA_DIR,
    name
  );
  if (path_length < 0 || (size_t) path_length >= sizeof(path)) {
    return false;
  }
  file = fopen(path, "rb");
  if (file == NULL) {
    return false;
  }
  while ((character = fgetc(file)) != EOF) {
    const int nibble = hex_value(character);

    if (nibble >= 0) {
      if (high_nibble < 0) {
        high_nibble = nibble;
      } else if (size < output_capacity) {
        output[size++] = (uint8_t) ((high_nibble << 4) | nibble);
        high_nibble = -1;
      } else {
        valid = false;
        break;
      }
    } else if (!is_ascii_whitespace(character)) {
      valid = false;
      break;
    }
  }
  if (ferror(file) != 0 || high_nibble >= 0 || fclose(file) != 0) {
    valid = false;
  }
  if (!valid) {
    return false;
  }
  *output_size = size;
  return true;
}

/**
 * @brief Verifies one rejected request decode and output atomicity.
 *
 * @param input Complete candidate payload.
 * @param input_size Candidate byte count.
 * @param expected Expected codec result.
 * @return True when classification and output atomicity match.
 */
static bool request_decode_rejects(
  const uint8_t *input,
  size_t input_size,
  MoonlightProtocolResult expected
) {
  MoonlightProtocolV1GetAppListRequest output;
  MoonlightProtocolV1GetAppListRequest unchanged;

  memset(&output, 0xa5, sizeof(output));
  unchanged = output;
  TEST_RESULT(
    MoonlightProtocolV1DecodeGetAppListRequest(
      input,
      input_size,
      &output
    ),
    expected
  );
  TEST_CHECK(memcmp(&output, &unchanged, sizeof(output)) == 0);
  return true;
}

/**
 * @brief Verifies one rejected response decode and output atomicity.
 *
 * @param input Complete candidate payload.
 * @param input_size Candidate byte count.
 * @param expected Expected codec result.
 * @return True when classification and output atomicity match.
 */
static bool response_decode_rejects(
  const uint8_t *input,
  size_t input_size,
  MoonlightProtocolResult expected
) {
  MoonlightProtocolV1GetAppListResponse output;
  MoonlightProtocolV1GetAppListResponse unchanged;

  memset(&output, 0xa5, sizeof(output));
  unchanged = output;
  TEST_RESULT(
    MoonlightProtocolV1DecodeGetAppListResponse(
      input,
      input_size,
      &output
    ),
    expected
  );
  TEST_CHECK(memcmp(&output, &unchanged, sizeof(output)) == 0);
  return true;
}

/**
 * @brief Wraps one nested record and verifies its decode rejection.
 *
 * @param nested Complete nested candidate bytes.
 * @param nested_size Nested candidate byte count.
 * @param expected Expected codec result.
 * @return True when classification and output atomicity match.
 */
static bool nested_record_decode_rejects(
  const uint8_t *nested,
  size_t nested_size,
  MoonlightProtocolResult expected
) {
  uint8_t payload[MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + MOONLIGHT_PROTOCOL_V1_APPLICATION_RECORD_PAYLOAD_MAX];
  const size_t payload_size = test_append_field(
    payload,
    1,
    MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED,
    nested,
    nested_size
  );

  return response_decode_rejects(payload, payload_size, expected);
}

/**
 * @brief Verifies all optional request forms and exact boundaries.
 *
 * @return True on success.
 */
static bool test_request_round_trip(void) {
  MoonlightProtocolV1GetAppListRequest request;
  MoonlightProtocolV1GetAppListRequest decoded;
  uint8_t encoded[MOONLIGHT_PROTOCOL_V1_GET_APP_LIST_REQUEST_PAYLOAD_MAX];
  size_t encoded_size = 99;
  size_t index;

  memset(&request, 0, sizeof(request));
  TEST_RESULT(
    MoonlightProtocolV1EncodeGetAppListRequest(
      &request,
      NULL,
      0,
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(encoded_size == 0);
  TEST_RESULT(
    MoonlightProtocolV1DecodeGetAppListRequest(NULL, 0, &decoded),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(decoded.cursor_size == 0);
  TEST_CHECK(decoded.maximum_entries == 0);

  for (index = 0; index < sizeof(request.cursor); ++index) {
    request.cursor[index] = (uint8_t) index;
  }
  request.cursor_size = sizeof(request.cursor);
  request.maximum_entries = 1;
  TEST_RESULT(
    MoonlightProtocolV1EncodeGetAppListRequest(
      &request,
      encoded,
      sizeof(encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(encoded_size == sizeof(encoded));
  TEST_RESULT(
    MoonlightProtocolV1DecodeGetAppListRequest(
      encoded,
      encoded_size,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(memcmp(&decoded, &request, sizeof(decoded)) == 0);

  memset(&request, 0, sizeof(request));
  request.maximum_entries = MOONLIGHT_PROTOCOL_V1_APP_LIST_MAX_ENTRIES;
  TEST_RESULT(
    MoonlightProtocolV1EncodeGetAppListRequest(
      &request,
      encoded,
      sizeof(encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(encoded_size == MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + 2u);
  TEST_RESULT(
    MoonlightProtocolV1DecodeGetAppListRequest(
      encoded,
      encoded_size,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(decoded.cursor_size == 0);
  TEST_CHECK(
    decoded.maximum_entries == MOONLIGHT_PROTOCOL_V1_APP_LIST_MAX_ENTRIES
  );
  return true;
}

/**
 * @brief Verifies request argument, range, structure, and atomicity failures.
 *
 * @return True on success.
 */
static bool test_request_rejections(void) {
  MoonlightProtocolV1GetAppListRequest request;
  uint8_t valid[MOONLIGHT_PROTOCOL_V1_GET_APP_LIST_REQUEST_PAYLOAD_MAX];
  uint8_t mutated[MOONLIGHT_PROTOCOL_V1_GET_APP_LIST_REQUEST_PAYLOAD_MAX + 1u];
  uint8_t output[sizeof(valid)];
  uint8_t unchanged[sizeof(output)];
  size_t valid_size = 0;
  size_t encoded_size = 777;
  size_t index;

  memset(&request, 0, sizeof(request));
  request.cursor_size = 1;
  TEST_RESULT(
    MoonlightProtocolV1EncodeGetAppListRequest(
      &request,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  request.cursor_size = MOONLIGHT_PROTOCOL_V1_APP_LIST_CURSOR_SIZE + 1u;
  TEST_RESULT(
    MoonlightProtocolV1EncodeGetAppListRequest(
      &request,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  request.cursor_size = 0;
  request.maximum_entries =
    MOONLIGHT_PROTOCOL_V1_APP_LIST_MAX_ENTRIES + 1u;
  TEST_RESULT(
    MoonlightProtocolV1EncodeGetAppListRequest(
      &request,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeGetAppListRequest(
      NULL,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  request.maximum_entries = 1;
  TEST_RESULT(
    MoonlightProtocolV1EncodeGetAppListRequest(
      &request,
      output,
      sizeof(output),
      NULL
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeGetAppListRequest(
      &request,
      NULL,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );

  memset(&request, 0, sizeof(request));
  request.cursor_size = sizeof(request.cursor);
  request.maximum_entries = 128;
  for (index = 0; index < sizeof(request.cursor); ++index) {
    request.cursor[index] = (uint8_t) index;
  }
  TEST_RESULT(
    MoonlightProtocolV1EncodeGetAppListRequest(
      &request,
      valid,
      sizeof(valid),
      &valid_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  memset(output, 0xa5, sizeof(output));
  memcpy(unchanged, output, sizeof(output));
  TEST_RESULT(
    MoonlightProtocolV1EncodeGetAppListRequest(
      &request,
      output,
      valid_size - 1u,
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL
  );
  TEST_CHECK(memcmp(output, unchanged, sizeof(output)) == 0);
  TEST_CHECK(encoded_size == 777u);

  TEST_RESULT(
    MoonlightProtocolV1DecodeGetAppListRequest(NULL, valid_size, &request),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeGetAppListRequest(valid, valid_size, NULL),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  memset(mutated, 0, sizeof(mutated));
  TEST_CHECK(
    request_decode_rejects(
      mutated,
      sizeof(mutated),
      MOONLIGHT_PROTOCOL_RESULT_LIMIT_EXCEEDED
    )
  );
  for (index = 1; index < MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE; ++index) {
    TEST_CHECK(
      request_decode_rejects(
        valid,
        index,
        MOONLIGHT_PROTOCOL_RESULT_MALFORMED
      )
    );
  }

  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated, 0);
  TEST_CHECK(
    request_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );
  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated, 3);
  TEST_CHECK(
    request_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );
  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated + 2u, MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED);
  TEST_CHECK(
    request_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );
  memcpy(mutated, valid, valid_size);
  test_store_u32(mutated + 4u, 15u);
  TEST_CHECK(
    request_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated + 24u, 1);
  TEST_CHECK(
    request_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated + 26u, MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED);
  TEST_CHECK(
    request_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );
  memcpy(mutated, valid, valid_size);
  test_store_u32(mutated + 28u, 1);
  TEST_CHECK(
    request_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  memcpy(mutated, valid, valid_size);
  mutated[32] = 0;
  mutated[33] = 0;
  TEST_CHECK(
    request_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated + 32u, 129);
  TEST_CHECK(
    request_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated + 32u, UINT16_MAX);
  TEST_CHECK(
    request_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  memcpy(mutated, valid, valid_size);
  test_store_u32(mutated + 4u, UINT32_MAX);
  TEST_CHECK(
    request_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  return true;
}

/**
 * @brief Verifies response empty, optional, and exact maximum forms.
 *
 * @return True on success.
 */
static bool test_response_round_trip_bounds(void) {
  MoonlightProtocolV1GetAppListResponse response;
  MoonlightProtocolV1GetAppListResponse decoded;
  uint8_t encoded[MOONLIGHT_PROTOCOL_V1_GET_APP_LIST_RESPONSE_PAYLOAD_MAX];
  size_t encoded_size = 99;
  size_t index;

  memset(&response, 0, sizeof(response));
  TEST_RESULT(
    MoonlightProtocolV1EncodeGetAppListResponse(
      &response,
      NULL,
      0,
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(encoded_size == 0);
  TEST_RESULT(
    MoonlightProtocolV1DecodeGetAppListResponse(NULL, 0, &decoded),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(decoded.entry_count == 0);
  TEST_CHECK(decoded.next_cursor_size == 0);

  memset(&response, 0, sizeof(response));
  for (index = 0; index < MOONLIGHT_PROTOCOL_V1_APP_LIST_MAX_ENTRIES; ++index) {
    const int result = snprintf(
      (char *) response.entries[index].application_id,
      sizeof(response.entries[index].application_id),
      "%03zu",
      index
    );

    TEST_CHECK(result == 3);
    response.entries[index].application_id_size = 128;
    memset(response.entries[index].application_id + 3u, 'i', 125u);
    memset(
      response.entries[index].display_name,
      'D',
      sizeof(response.entries[index].display_name)
    );
    response.entries[index].display_name_size =
      sizeof(response.entries[index].display_name);
    memset(
      response.entries[index].icon_asset_sha256,
      (int) index,
      sizeof(response.entries[index].icon_asset_sha256)
    );
    response.entries[index].icon_asset_sha256_size =
      sizeof(response.entries[index].icon_asset_sha256);
  }
  response.entry_count = MOONLIGHT_PROTOCOL_V1_APP_LIST_MAX_ENTRIES;
  memset(response.next_cursor, 0xff, sizeof(response.next_cursor));
  response.next_cursor_size = sizeof(response.next_cursor);
  TEST_RESULT(
    MoonlightProtocolV1EncodeGetAppListResponse(
      &response,
      encoded,
      sizeof(encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(
    encoded_size == MOONLIGHT_PROTOCOL_V1_GET_APP_LIST_RESPONSE_PAYLOAD_MAX
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeGetAppListResponse(
      encoded,
      encoded_size,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(memcmp(&decoded, &response, sizeof(decoded)) == 0);
  return true;
}

/**
 * @brief Verifies response semantic validation and encode atomicity.
 *
 * @return True on success.
 */
static bool test_response_encode_rejections(void) {
  MoonlightProtocolV1GetAppListResponse response = golden_response();
  uint8_t output[MOONLIGHT_PROTOCOL_V1_GET_APP_LIST_RESPONSE_PAYLOAD_MAX];
  uint8_t unchanged[sizeof(output)];
  size_t encoded_size = 777;

  memset(output, 0xa5, sizeof(output));
  memcpy(unchanged, output, sizeof(output));
  TEST_RESULT(
    MoonlightProtocolV1EncodeGetAppListResponse(
      NULL,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeGetAppListResponse(
      &response,
      output,
      sizeof(output),
      NULL
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeGetAppListResponse(
      &response,
      NULL,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );

  response.entry_count = MOONLIGHT_PROTOCOL_V1_APP_LIST_MAX_ENTRIES + 1u;
  TEST_RESULT(
    MoonlightProtocolV1EncodeGetAppListResponse(
      &response,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memset(&response, 0, sizeof(response));
  response.entries[0] = make_record("a", "A", false);
  response.entries[1] = make_record("aa", "AA", false);
  response.entry_count = 2;
  TEST_RESULT(
    MoonlightProtocolV1EncodeGetAppListResponse(
      &response,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  response.entries[0] = make_record("aa", "AA", false);
  response.entries[1] = make_record("a", "A", false);
  TEST_RESULT(
    MoonlightProtocolV1EncodeGetAppListResponse(
      &response,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memset(output, 0xa5, sizeof(output));
  memcpy(unchanged, output, sizeof(output));
  encoded_size = 777;
  response = golden_response();
  response.next_cursor_size = 1;
  TEST_RESULT(
    MoonlightProtocolV1EncodeGetAppListResponse(
      &response,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  response.next_cursor_size = MOONLIGHT_PROTOCOL_V1_APP_LIST_CURSOR_SIZE + 1u;
  TEST_RESULT(
    MoonlightProtocolV1EncodeGetAppListResponse(
      &response,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memset(&response, 0, sizeof(response));
  response.next_cursor_size = sizeof(response.next_cursor);
  TEST_RESULT(
    MoonlightProtocolV1EncodeGetAppListResponse(
      &response,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );

  response = golden_response();
  response.entries[0].application_id_size = 0;
  TEST_RESULT(
    MoonlightProtocolV1EncodeGetAppListResponse(
      &response,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  response = golden_response();
  response.entries[0].application_id_size =
    MOONLIGHT_PROTOCOL_V1_APPLICATION_ID_MAX + 1u;
  TEST_RESULT(
    MoonlightProtocolV1EncodeGetAppListResponse(
      &response,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  response = golden_response();
  response.entries[0].display_name_size = 0;
  TEST_RESULT(
    MoonlightProtocolV1EncodeGetAppListResponse(
      &response,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  response = golden_response();
  response.entries[0].display_name_size =
    MOONLIGHT_PROTOCOL_V1_APPLICATION_DISPLAY_NAME_MAX + 1u;
  TEST_RESULT(
    MoonlightProtocolV1EncodeGetAppListResponse(
      &response,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  response = golden_response();
  response.entries[0].icon_asset_sha256_size = 1;
  TEST_RESULT(
    MoonlightProtocolV1EncodeGetAppListResponse(
      &response,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  response.entries[0].icon_asset_sha256_size =
    MOONLIGHT_PROTOCOL_V1_APPLICATION_ICON_DIGEST_SIZE + 1u;
  TEST_RESULT(
    MoonlightProtocolV1EncodeGetAppListResponse(
      &response,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );

  response = golden_response();
  response.entries[1] = response.entries[0];
  TEST_RESULT(
    MoonlightProtocolV1EncodeGetAppListResponse(
      &response,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  response = golden_response();
  response.entries[0] = make_record("z", "Z", false);
  TEST_RESULT(
    MoonlightProtocolV1EncodeGetAppListResponse(
      &response,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  response = golden_response();
  TEST_RESULT(
    MoonlightProtocolV1EncodeGetAppListResponse(
      &response,
      output,
      1,
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL
  );
  TEST_CHECK(memcmp(output, unchanged, sizeof(output)) == 0);
  TEST_CHECK(encoded_size == 777u);
  return true;
}

/**
 * @brief Builds one structurally valid two-record response payload.
 *
 * @param output Maximum-size output buffer.
 * @param output_size Receives the encoded byte count.
 * @return True on success.
 */
static bool encode_small_response(
  uint8_t *output,
  size_t *output_size
) {
  const MoonlightProtocolV1GetAppListResponse response = golden_response();

  TEST_RESULT(
    MoonlightProtocolV1EncodeGetAppListResponse(
      &response,
      output,
      MOONLIGHT_PROTOCOL_V1_GET_APP_LIST_RESPONSE_PAYLOAD_MAX,
      output_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  return true;
}

/**
 * @brief Verifies outer and nested response structural failures.
 *
 * @return True on success.
 */
static bool test_response_decode_structure(void) {
  uint8_t valid[MOONLIGHT_PROTOCOL_V1_GET_APP_LIST_RESPONSE_PAYLOAD_MAX];
  uint8_t mutated[MOONLIGHT_PROTOCOL_V1_GET_APP_LIST_RESPONSE_PAYLOAD_MAX + 1u];
  uint8_t one_record[64];
  size_t valid_size = 0;
  size_t one_record_size;
  size_t offset;
  size_t index;

  TEST_CHECK(encode_small_response(valid, &valid_size));
  TEST_CHECK(valid_size == 149u);
  TEST_RESULT(
    MoonlightProtocolV1DecodeGetAppListResponse(
      NULL,
      valid_size,
      (MoonlightProtocolV1GetAppListResponse *) mutated
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeGetAppListResponse(valid, valid_size, NULL),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_CHECK(
    response_decode_rejects(
      valid,
      MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE - 1u,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  memset(mutated, 0, sizeof(mutated));
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      sizeof(mutated),
      MOONLIGHT_PROTOCOL_RESULT_LIMIT_EXCEEDED
    )
  );

  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated, 0);
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );
  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated, 3);
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );
  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated + 2u, 0);
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );
  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated + 2u, 3);
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );
  memcpy(mutated, valid, valid_size);
  test_store_u32(mutated + 4u, 17);
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  memcpy(mutated, valid, valid_size);
  test_store_u32(mutated + 4u, UINT32_MAX);
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );

  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated + 8u, 2);
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated + 8u, 4);
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );
  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated + 10u, MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED);
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );
  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated + 23u, 1);
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );

  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated + 46u, 2);
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated + 46u, 4);
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );
  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated + 48u, MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED);
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );

  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated + 127u, MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED);
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );
  memcpy(mutated, valid, valid_size);
  test_store_u32(mutated + 129u, 15);
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  memcpy(mutated, valid, valid_size);
  memcpy(mutated + valid_size, valid + 125u, 24u);
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      valid_size + 24u,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  memcpy(mutated, valid, valid_size);
  memcpy(mutated + valid_size, valid, 38u);
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      valid_size + 38u,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );

  memcpy(mutated, valid, valid_size);
  memset(mutated + 16u, 'z', 7u);
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  memcpy(mutated, valid, valid_size);
  memset(mutated + 54u, 'a', 9u);
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );

  one_record_size = test_append_field(
    one_record,
    1,
    0,
    (const uint8_t *) "a",
    1
  );
  one_record_size += test_append_field(
    one_record + one_record_size,
    2,
    0,
    (const uint8_t *) "A",
    1
  );
  offset = 0;
  offset += test_append_field(
    mutated + offset,
    1,
    MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED,
    one_record,
    one_record_size
  );
  offset += test_append_field(
    mutated + offset,
    1,
    MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED,
    one_record,
    one_record_size
  );
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      offset,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );

  offset = 0;
  for (index = 0; index <= MOONLIGHT_PROTOCOL_V1_APP_LIST_MAX_ENTRIES; ++index) {
    offset += test_append_field(
      mutated + offset,
      1,
      MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED,
      one_record,
      one_record_size
    );
  }
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      offset,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  return true;
}

/**
 * @brief Verifies nested optional-field and length edge failures.
 *
 * @return True on success.
 */
static bool test_response_nested_edges(void) {
  uint8_t nested[MOONLIGHT_PROTOCOL_V1_APPLICATION_RECORD_PAYLOAD_MAX + 1u];
  uint8_t payload[MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + MOONLIGHT_PROTOCOL_V1_APPLICATION_RECORD_PAYLOAD_MAX + 1u];
  uint8_t long_value[MOONLIGHT_PROTOCOL_V1_APPLICATION_DISPLAY_NAME_MAX + 1u];
  uint8_t digest[MOONLIGHT_PROTOCOL_V1_APPLICATION_ICON_DIGEST_SIZE + 1u];
  uint8_t cursor[MOONLIGHT_PROTOCOL_V1_APP_LIST_CURSOR_SIZE] = {0};
  size_t nested_size;
  size_t optional_offset;
  size_t payload_size;

  memset(long_value, 'i', sizeof(long_value));
  memset(digest, 0xa5, sizeof(digest));

  nested_size = test_append_field(
    nested,
    0,
    0,
    (const uint8_t *) "a",
    1
  );
  nested_size += test_append_field(
    nested + nested_size,
    2,
    0,
    (const uint8_t *) "A",
    1
  );
  TEST_CHECK(
    nested_record_decode_rejects(
      nested,
      nested_size,
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );

  nested_size = test_append_field(
    nested,
    1,
    0,
    (const uint8_t *) "abcdefghij",
    10
  );
  TEST_CHECK(
    nested_size == MOONLIGHT_PROTOCOL_V1_APPLICATION_RECORD_PAYLOAD_MIN
  );
  TEST_CHECK(
    nested_record_decode_rejects(
      nested,
      nested_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );

  nested_size = test_append_field(
    nested,
    1,
    0,
    (const uint8_t *) "a",
    1
  );
  test_store_u16(nested + nested_size, 2);
  test_store_u16(nested + nested_size + 2u, 0);
  test_store_u32(nested + nested_size + 4u, 2);
  nested[nested_size + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE] = 'A';
  nested_size += MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + 1u;
  TEST_CHECK(
    nested_record_decode_rejects(
      nested,
      nested_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );

  nested_size = test_append_field(
    nested,
    1,
    0,
    (const uint8_t *) "a",
    1
  );
  nested_size += test_append_field(
    nested + nested_size,
    2,
    0,
    (const uint8_t *) "A",
    1
  );
  optional_offset = nested_size;
  nested[nested_size++] = 0;
  TEST_CHECK(
    nested_record_decode_rejects(
      nested,
      nested_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );

  nested_size = optional_offset;
  nested_size += test_append_field(
    nested + nested_size,
    2,
    0,
    digest,
    0
  );
  TEST_CHECK(
    nested_record_decode_rejects(
      nested,
      nested_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  test_store_u16(nested + optional_offset, 4);
  TEST_CHECK(
    nested_record_decode_rejects(
      nested,
      nested_size,
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );
  test_store_u16(nested + optional_offset, 3);
  test_store_u16(
    nested + optional_offset + 2u,
    MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED
  );
  TEST_CHECK(
    nested_record_decode_rejects(
      nested,
      nested_size,
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );

  test_store_u16(nested + optional_offset + 2u, 0);
  test_store_u32(
    nested + optional_offset + 4u,
    MOONLIGHT_PROTOCOL_V1_APPLICATION_ICON_DIGEST_SIZE + 1u
  );
  memcpy(
    nested + optional_offset + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
    digest,
    sizeof(digest)
  );
  nested_size =
    optional_offset +
    MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE +
    sizeof(digest);
  TEST_CHECK(
    nested_record_decode_rejects(
      nested,
      nested_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );

  nested_size = optional_offset;
  nested_size += test_append_field(
    nested + nested_size,
    3,
    0,
    digest,
    MOONLIGHT_PROTOCOL_V1_APPLICATION_ICON_DIGEST_SIZE
  );
  optional_offset = nested_size;
  nested[nested_size++] = 0;
  TEST_CHECK(
    nested_record_decode_rejects(
      nested,
      nested_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );

  nested_size = optional_offset;
  nested_size += test_append_field(
    nested + nested_size,
    3,
    0,
    digest,
    0
  );
  TEST_CHECK(
    nested_record_decode_rejects(
      nested,
      nested_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  test_store_u16(nested + optional_offset, 4);
  TEST_CHECK(
    nested_record_decode_rejects(
      nested,
      nested_size,
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );

  nested_size = test_append_field(
    nested,
    1,
    0,
    long_value,
    MOONLIGHT_PROTOCOL_V1_APPLICATION_ID_MAX + 1u
  );
  nested_size += test_append_field(
    nested + nested_size,
    2,
    0,
    (const uint8_t *) "A",
    1
  );
  TEST_CHECK(
    nested_record_decode_rejects(
      nested,
      nested_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );

  nested_size = test_append_field(
    nested,
    1,
    0,
    (const uint8_t *) "a",
    1
  );
  nested_size += test_append_field(
    nested + nested_size,
    2,
    0,
    long_value,
    sizeof(long_value)
  );
  TEST_CHECK(
    nested_record_decode_rejects(
      nested,
      nested_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );

  memset(
    nested,
    0,
    MOONLIGHT_PROTOCOL_V1_APPLICATION_RECORD_PAYLOAD_MAX + 1u
  );
  payload_size = test_append_field(
    payload,
    1,
    MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED,
    nested,
    MOONLIGHT_PROTOCOL_V1_APPLICATION_RECORD_PAYLOAD_MAX + 1u
  );
  TEST_CHECK(
    response_decode_rejects(
      payload,
      payload_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );

  payload_size = test_append_field(
    payload,
    2,
    0,
    cursor,
    sizeof(cursor)
  );
  TEST_CHECK(
    response_decode_rejects(
      payload,
      payload_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  return true;
}

/**
 * @brief Verifies canonical UTF-8 rejection on encode and decode.
 *
 * @return True on success.
 */
static bool test_utf8_rejections(void) {
  static const uint8_t invalid_sequences[][4] = {
    {0x00, 0x00, 0x00, 0x00},
    {0x80, 0x00, 0x00, 0x00},
    {0xc0, 0x80, 0x00, 0x00},
    {0xe0, 0x80, 0x80, 0x00},
    {0xed, 0xa0, 0x80, 0x00},
    {0xed, 0xbf, 0xbf, 0x00},
    {0xf4, 0x90, 0x80, 0x80},
    {0xf5, 0x80, 0x80, 0x80},
    {0xc2, 0x00, 0x00, 0x00},
    {0xc2, 0x20, 0x00, 0x00},
  };
  static const size_t invalid_sizes[] = {
    1,
    1,
    2,
    3,
    3,
    3,
    4,
    4,
    1,
    2,
  };
  MoonlightProtocolV1GetAppListResponse response;
  uint8_t nested[MOONLIGHT_PROTOCOL_V1_APPLICATION_RECORD_PAYLOAD_MAX];
  uint8_t payload[MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + MOONLIGHT_PROTOCOL_V1_APPLICATION_RECORD_PAYLOAD_MAX];
  uint8_t encoded[MOONLIGHT_PROTOCOL_V1_GET_APP_LIST_RESPONSE_PAYLOAD_MAX];
  size_t encoded_size;
  size_t nested_size;
  size_t payload_size;
  size_t index;

  for (index = 0; index < sizeof(invalid_sizes) / sizeof(invalid_sizes[0]); ++index) {
    memset(&response, 0, sizeof(response));
    response.entries[0] = make_record("a", "A", false);
    memcpy(
      response.entries[0].application_id,
      invalid_sequences[index],
      invalid_sizes[index]
    );
    response.entries[0].application_id_size = invalid_sizes[index];
    response.entry_count = 1;
    TEST_RESULT(
      MoonlightProtocolV1EncodeGetAppListResponse(
        &response,
        encoded,
        sizeof(encoded),
        &encoded_size
      ),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    );

    response.entries[0] = make_record("a", "A", false);
    memcpy(
      response.entries[0].display_name,
      invalid_sequences[index],
      invalid_sizes[index]
    );
    response.entries[0].display_name_size = invalid_sizes[index];
    TEST_RESULT(
      MoonlightProtocolV1EncodeGetAppListResponse(
        &response,
        encoded,
        sizeof(encoded),
        &encoded_size
      ),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    );

    nested_size = test_append_field(
      nested,
      1,
      0,
      invalid_sequences[index],
      invalid_sizes[index]
    );
    nested_size += test_append_field(
      nested + nested_size,
      2,
      0,
      (const uint8_t *) "A",
      1
    );
    payload_size = test_append_field(
      payload,
      1,
      MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED,
      nested,
      nested_size
    );
    TEST_CHECK(
      response_decode_rejects(
        payload,
        payload_size,
        MOONLIGHT_PROTOCOL_RESULT_MALFORMED
      )
    );

    nested_size = test_append_field(
      nested,
      1,
      0,
      (const uint8_t *) "a",
      1
    );
    nested_size += test_append_field(
      nested + nested_size,
      2,
      0,
      invalid_sequences[index],
      invalid_sizes[index]
    );
    payload_size = test_append_field(
      payload,
      1,
      MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED,
      nested,
      nested_size
    );
    TEST_CHECK(
      response_decode_rejects(
        payload,
        payload_size,
        MOONLIGHT_PROTOCOL_RESULT_MALFORMED
      )
    );
  }

  memset(&response, 0, sizeof(response));
  response.entries[0] = make_record("a", "A", false);
  response.entries[0].display_name[0] = 0xc2;
  response.entries[0].display_name[1] = 0x80;
  response.entries[0].display_name[2] = 0xee;
  response.entries[0].display_name[3] = 0x80;
  response.entries[0].display_name[4] = 0x80;
  response.entries[0].display_name[5] = 0xf4;
  response.entries[0].display_name[6] = 0x8f;
  response.entries[0].display_name[7] = 0xbf;
  response.entries[0].display_name[8] = 0xbf;
  response.entries[0].display_name_size = 9;
  response.entry_count = 1;
  TEST_RESULT(
    MoonlightProtocolV1EncodeGetAppListResponse(
      &response,
      encoded,
      sizeof(encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  return true;
}

/**
 * @brief Verifies the decoder copies all caller-owned values.
 *
 * @return True on success.
 */
static bool test_response_ownership(void) {
  const MoonlightProtocolV1GetAppListResponse expected = golden_response();
  MoonlightProtocolV1GetAppListResponse decoded;
  uint8_t encoded[MOONLIGHT_PROTOCOL_V1_GET_APP_LIST_RESPONSE_PAYLOAD_MAX];
  size_t encoded_size = 0;

  TEST_CHECK(encode_small_response(encoded, &encoded_size));
  TEST_RESULT(
    MoonlightProtocolV1DecodeGetAppListResponse(
      encoded,
      encoded_size,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  memset(encoded, 0, encoded_size);
  TEST_CHECK(memcmp(&decoded, &expected, sizeof(decoded)) == 0);
  return true;
}

/**
 * @brief Verifies the shared complete GET_APP_LIST response golden.
 *
 * @return True on success.
 */
static bool test_golden(void) {
  MoonlightProtocolV1GetAppListResponse response = golden_response();
  MoonlightProtocolV1GetAppListResponse decoded;
  MoonlightProtocolV1MessageEnvelope envelope = {
    .message_type = MOONLIGHT_PROTOCOL_V1_MESSAGE_GET_APP_LIST,
    .flags = MOONLIGHT_PROTOCOL_V1_ENVELOPE_FLAG_RESPONSE,
    .payload_length = 149,
    .status = MOONLIGHT_PROTOCOL_V1_STATUS_OK,
    .correlation_id = UINT64_C(0x0102030405060708),
  };
  MoonlightProtocolV1MessageEnvelope decoded_envelope;
  uint8_t golden[TEST_APP_LIST_GOLDEN_SIZE];
  uint8_t canonical[TEST_APP_LIST_GOLDEN_SIZE];
  size_t golden_size = 0;
  size_t payload_size = 0;

  TEST_CHECK(
    load_golden(
      "app-list-success-response.hex",
      golden,
      sizeof(golden),
      &golden_size
    )
  );
  TEST_CHECK(golden_size == sizeof(golden));
  TEST_RESULT(
    MoonlightProtocolV1DecodeMessageEnvelope(
      golden,
      MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE,
      MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX,
      &decoded_envelope
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(decoded_envelope.message_type == envelope.message_type);
  TEST_CHECK(decoded_envelope.flags == envelope.flags);
  TEST_CHECK(decoded_envelope.payload_length == envelope.payload_length);
  TEST_CHECK(decoded_envelope.status == envelope.status);
  TEST_CHECK(decoded_envelope.correlation_id == envelope.correlation_id);
  TEST_RESULT(
    MoonlightProtocolV1DecodeGetAppListResponse(
      golden + MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE,
      decoded_envelope.payload_length,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(memcmp(&decoded, &response, sizeof(decoded)) == 0);

  TEST_RESULT(
    MoonlightProtocolV1EncodeMessageEnvelope(
      &envelope,
      MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX,
      canonical,
      MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeGetAppListResponse(
      &response,
      canonical + MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE,
      sizeof(canonical) - MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE,
      &payload_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(payload_size == envelope.payload_length);
  TEST_CHECK(memcmp(canonical, golden, sizeof(golden)) == 0);
  return true;
}

/**
 * @brief Executes every GET_APP_LIST schema test.
 *
 * @return Zero only when every test succeeds.
 */
int main(void) {
  static const struct {
    const char *name;  ///< Human-readable test name.
    bool (*run)(void);  ///< Test implementation.
  } tests[] = {
    {"golden", test_golden},
    {"request round trip", test_request_round_trip},
    {"request rejections", test_request_rejections},
    {"response round-trip bounds", test_response_round_trip_bounds},
    {"response encode rejections", test_response_encode_rejections},
    {"response decode structure", test_response_decode_structure},
    {"response nested edges", test_response_nested_edges},
    {"UTF-8 rejections", test_utf8_rejections},
    {"response ownership", test_response_ownership},
  };

  size_t index;

  for (index = 0; index < sizeof(tests) / sizeof(tests[0]); ++index) {
    if (!tests[index].run()) {
      fprintf(stderr, "FAILED: %s\n", tests[index].name);
      return 1;
    }
    printf("PASS: %s\n", tests[index].name);
  }
  return 0;
}
