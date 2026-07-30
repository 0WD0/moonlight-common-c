/**
 * @file test_display_list.c
 * @brief Native tests for protocol version 1 GET_DISPLAY_LIST schemas.
 */

#include <limits.h>
#include <moonlight/protocol/control.h>
#include <moonlight/protocol/wire.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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
 * @brief Stores one big-endian 64-bit integer in a mutable test vector.
 *
 * @param output Eight writable bytes.
 * @param value Host-order value.
 */
static void test_store_u64(uint8_t *output, uint64_t value) {
  test_store_u32(output, (uint32_t) (value >> 32u));
  test_store_u32(output + 4u, (uint32_t) value);
}

/**
 * @brief Loads one big-endian 32-bit integer from a test vector.
 *
 * @param input Four readable bytes.
 * @return Host-order value.
 */
static uint32_t test_load_u32(const uint8_t *input) {
  return ((uint32_t) input[0] << 24u) |
         ((uint32_t) input[1] << 16u) |
         ((uint32_t) input[2] << 8u) |
         input[3];
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
 * @brief Creates one valid Host Display record with an ASCII label.
 *
 * @param display_number Nonzero final Display-ID byte.
 * @param label NUL-terminated ASCII label.
 * @param metadata_known Whether to populate authoritative display metadata.
 * @return Initialized record.
 */
static MoonlightProtocolV1DisplayRecord make_record(
  uint8_t display_number,
  const char *label,
  bool metadata_known
) {
  MoonlightProtocolV1DisplayRecord record;

  memset(&record, 0, sizeof(record));
  record.display_id[MOONLIGHT_PROTOCOL_V1_DISPLAY_ID_SIZE - 1u] =
    display_number;
  record.label_size = strlen(label);
  memcpy(record.label, label, record.label_size);
  if (metadata_known) {
    record.width = 3840;
    record.height = 2160;
    record.refresh_rate_numerator = 60000;
    record.refresh_rate_denominator = 1001;
    record.origin_x = -1920;
    record.origin_y = -1080;
    record.flags =
      MOONLIGHT_PROTOCOL_V1_DISPLAY_FLAG_PRIMARY |
      MOONLIGHT_PROTOCOL_V1_DISPLAY_FLAG_HDR_ENABLED |
      MOONLIGHT_PROTOCOL_V1_DISPLAY_FLAG_METADATA_KNOWN;
  }
  return record;
}

/**
 * @brief Creates a representative ordered two-display response.
 *
 * @return Initialized response with owned UTF-8 labels and metadata.
 */
static MoonlightProtocolV1GetDisplayListResponse representative_response(void) {
  static const uint8_t primary_label[] = {
    'M',
    'a',
    'i',
    'n',
    ' ',
    0xf0,
    0x9f,
    0x96,
    0xa5,
  };
  static const uint8_t secondary_label[] = {
    0xe5,
    0xa4,
    0x96,
    0xe6,
    0x8e,
    0xa5,
    0xe5,
    0xb1,
    0x8f,
  };
  MoonlightProtocolV1GetDisplayListResponse response;

  memset(&response, 0, sizeof(response));
  response.catalog_revision = UINT64_C(0x0102030405060708);
  response.entries[0] = make_record(1, "placeholder", true);
  memset(response.entries[0].label, 0, sizeof(response.entries[0].label));
  memcpy(
    response.entries[0].label,
    primary_label,
    sizeof(primary_label)
  );
  response.entries[0].label_size = sizeof(primary_label);
  response.entries[0].origin_x = INT32_MIN;
  response.entries[0].origin_y = -1;

  response.entries[1] = make_record(2, "placeholder", false);
  memset(response.entries[1].label, 0, sizeof(response.entries[1].label));
  memcpy(
    response.entries[1].label,
    secondary_label,
    sizeof(secondary_label)
  );
  response.entries[1].label_size = sizeof(secondary_label);
  response.entry_count = 2;
  return response;
}

/**
 * @brief Encodes a nested Host Display record without validating its values.
 *
 * @param record Source record.
 * @param output Maximum-size nested-record destination.
 * @return Number of encoded bytes.
 */
static size_t test_append_nested_record(
  const MoonlightProtocolV1DisplayRecord *record,
  uint8_t *output
) {
  uint8_t scalars[7][4];
  size_t size = 0;
  size_t index;

  test_store_u32(scalars[0], record->width);
  test_store_u32(scalars[1], record->height);
  test_store_u32(scalars[2], record->refresh_rate_numerator);
  test_store_u32(scalars[3], record->refresh_rate_denominator);
  test_store_u32(scalars[4], (uint32_t) record->origin_x);
  test_store_u32(scalars[5], (uint32_t) record->origin_y);
  test_store_u32(scalars[6], record->flags);

  size += test_append_field(
    output + size,
    1,
    0,
    record->display_id,
    sizeof(record->display_id)
  );
  size += test_append_field(
    output + size,
    2,
    0,
    record->label,
    record->label_size
  );
  for (index = 0; index < 7u; ++index) {
    size += test_append_field(
      output + size,
      (uint16_t) (index + 3u),
      0,
      scalars[index],
      sizeof(scalars[index])
    );
  }
  return size;
}

/**
 * @brief Builds one complete response around a manually serialized record.
 *
 * @param record Candidate record, including intentionally invalid values.
 * @param output Maximum-size response destination.
 * @return Number of encoded bytes.
 */
static size_t test_build_single_response(
  const MoonlightProtocolV1DisplayRecord *record,
  uint8_t *output
) {
  uint8_t revision[8];
  uint8_t nested[MOONLIGHT_PROTOCOL_V1_DISPLAY_RECORD_PAYLOAD_MAX];
  size_t nested_size;
  size_t size = 0;

  test_store_u64(revision, UINT64_C(0x0102030405060708));
  nested_size = test_append_nested_record(record, nested);
  size += test_append_field(output + size, 1, 0, revision, sizeof(revision));
  size += test_append_field(
    output + size,
    2,
    MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED,
    nested,
    nested_size
  );
  return size;
}

/**
 * @brief Builds one complete response around arbitrary nested record bytes.
 *
 * @param nested Complete nested candidate.
 * @param nested_size Nested candidate byte count.
 * @param output Maximum-size response destination.
 * @return Number of encoded bytes.
 */
static size_t test_wrap_nested_record(
  const uint8_t *nested,
  size_t nested_size,
  uint8_t *output
) {
  uint8_t revision[8];
  size_t size = 0;

  test_store_u64(revision, UINT64_C(0x0102030405060708));
  size += test_append_field(output + size, 1, 0, revision, sizeof(revision));
  size += test_append_field(
    output + size,
    2,
    MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED,
    nested,
    nested_size
  );
  return size;
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
  MoonlightProtocolV1GetDisplayListResponse output;
  MoonlightProtocolV1GetDisplayListResponse unchanged;

  memset(&output, 0xa5, sizeof(output));
  unchanged = output;
  TEST_RESULT(
    MoonlightProtocolV1DecodeGetDisplayListResponse(
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
 * @brief Verifies one rejected response encode and output atomicity.
 *
 * @param response Candidate response.
 * @param expected Expected codec result.
 * @return True when classification and output atomicity match.
 */
static bool response_encode_rejects(
  const MoonlightProtocolV1GetDisplayListResponse *response,
  MoonlightProtocolResult expected
) {
  uint8_t output[MOONLIGHT_PROTOCOL_V1_GET_DISPLAY_LIST_RESPONSE_PAYLOAD_MAX];
  uint8_t unchanged[sizeof(output)];
  size_t encoded_size = 777;

  memset(output, 0xa5, sizeof(output));
  memcpy(unchanged, output, sizeof(output));
  TEST_RESULT(
    MoonlightProtocolV1EncodeGetDisplayListResponse(
      response,
      output,
      sizeof(output),
      &encoded_size
    ),
    expected
  );
  TEST_CHECK(memcmp(output, unchanged, sizeof(output)) == 0);
  TEST_CHECK(encoded_size == 777u);
  return true;
}

/**
 * @brief Encodes the representative response into a maximum-size buffer.
 *
 * @param output Destination.
 * @param output_size Receives the encoded byte count.
 * @return True on success.
 */
static bool encode_representative_response(
  uint8_t *output,
  size_t *output_size
) {
  const MoonlightProtocolV1GetDisplayListResponse response =
    representative_response();

  TEST_RESULT(
    MoonlightProtocolV1EncodeGetDisplayListResponse(
      &response,
      output,
      MOONLIGHT_PROTOCOL_V1_GET_DISPLAY_LIST_RESPONSE_PAYLOAD_MAX,
      output_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  return true;
}

/**
 * @brief Returns the outer-field offset for a representative response record.
 *
 * @param record_index Zero-based record index.
 * @return Byte offset of the outer record field.
 */
static size_t representative_outer_record_offset(size_t record_index) {
  const MoonlightProtocolV1GetDisplayListResponse response =
    representative_response();
  size_t offset = MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + 8u;
  size_t index;

  for (index = 0; index < record_index; ++index) {
    offset +=
      MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE +
      MOONLIGHT_PROTOCOL_V1_DISPLAY_RECORD_PAYLOAD_MIN -
      1u +
      response.entries[index].label_size;
  }
  return offset;
}

/**
 * @brief Returns one nested-field offset for a one-record payload.
 *
 * @param label_size Record label byte count.
 * @param field_id Nested field ID in `[1, 9]`.
 * @return Byte offset of the nested field header.
 */
static size_t single_response_nested_field_offset(
  size_t label_size,
  uint16_t field_id
) {
  const size_t nested_offset =
    MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + 8u +
    MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE;

  if (field_id == 1u) {
    return nested_offset;
  }
  if (field_id == 2u) {
    return nested_offset +
           MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE +
           MOONLIGHT_PROTOCOL_V1_DISPLAY_ID_SIZE;
  }
  return nested_offset +
         2u * MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE +
         MOONLIGHT_PROTOCOL_V1_DISPLAY_ID_SIZE +
         label_size +
         (size_t) (field_id - 3u) *
           (MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + 4u);
}

/**
 * @brief Verifies the canonical revision-only empty catalog form.
 *
 * @return True on success.
 */
static bool test_empty_catalog_canonical(void) {
  static const uint8_t canonical[] = {
    0x00,
    0x01,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x08,
    0x01,
    0x02,
    0x03,
    0x04,
    0x05,
    0x06,
    0x07,
    0x08,
  };
  MoonlightProtocolV1GetDisplayListResponse response;
  MoonlightProtocolV1GetDisplayListResponse decoded;
  uint8_t encoded[sizeof(canonical)];
  size_t encoded_size = 0;

  memset(&response, 0, sizeof(response));
  response.catalog_revision = UINT64_C(0x0102030405060708);
  TEST_RESULT(
    MoonlightProtocolV1EncodeGetDisplayListResponse(
      &response,
      encoded,
      sizeof(encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(encoded_size == sizeof(canonical));
  TEST_CHECK(memcmp(encoded, canonical, sizeof(canonical)) == 0);
  TEST_RESULT(
    MoonlightProtocolV1DecodeGetDisplayListResponse(
      canonical,
      sizeof(canonical),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(memcmp(&decoded, &response, sizeof(decoded)) == 0);
  return true;
}

/**
 * @brief Verifies multiple records, UTF-8, metadata flags, and signed origins.
 *
 * @return True on success.
 */
static bool test_multiple_record_round_trip(void) {
  const MoonlightProtocolV1GetDisplayListResponse response =
    representative_response();
  MoonlightProtocolV1GetDisplayListResponse decoded;
  uint8_t encoded[MOONLIGHT_PROTOCOL_V1_GET_DISPLAY_LIST_RESPONSE_PAYLOAD_MAX];
  uint8_t canonical[sizeof(encoded)];
  size_t encoded_size = 0;
  size_t canonical_size = 0;

  TEST_RESULT(
    MoonlightProtocolV1EncodeGetDisplayListResponse(
      &response,
      encoded,
      sizeof(encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeGetDisplayListResponse(
      encoded,
      encoded_size,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(memcmp(&decoded, &response, sizeof(decoded)) == 0);
  TEST_CHECK(decoded.entries[0].origin_x == INT32_MIN);
  TEST_CHECK(decoded.entries[0].origin_y == -1);
  TEST_CHECK(
    decoded.entries[0].flags == MOONLIGHT_PROTOCOL_V1_DISPLAY_FLAG_MASK
  );
  TEST_CHECK(decoded.entries[1].flags == 0);

  TEST_RESULT(
    MoonlightProtocolV1EncodeGetDisplayListResponse(
      &decoded,
      canonical,
      sizeof(canonical),
      &canonical_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(canonical_size == encoded_size);
  TEST_CHECK(memcmp(canonical, encoded, encoded_size) == 0);

  memset(encoded, 0, encoded_size);
  TEST_CHECK(memcmp(&decoded, &response, sizeof(decoded)) == 0);
  return true;
}

/**
 * @brief Verifies exact maximum record count and payload length.
 *
 * @return True on success.
 */
static bool test_maximum_catalog_round_trip(void) {
  MoonlightProtocolV1GetDisplayListResponse response;
  MoonlightProtocolV1GetDisplayListResponse decoded;
  uint8_t encoded[MOONLIGHT_PROTOCOL_V1_GET_DISPLAY_LIST_RESPONSE_PAYLOAD_MAX];
  size_t encoded_size = 0;
  size_t index;

  memset(&response, 0, sizeof(response));
  response.catalog_revision = UINT64_MAX;
  response.entry_count = MOONLIGHT_PROTOCOL_V1_DISPLAY_LIST_MAX_ENTRIES;
  for (index = 0; index < response.entry_count; ++index) {
    MoonlightProtocolV1DisplayRecord *record = &response.entries[index];

    record->display_id[MOONLIGHT_PROTOCOL_V1_DISPLAY_ID_SIZE - 1u] =
      (uint8_t) (index + 1u);
    memset(record->label, 'L', sizeof(record->label));
    record->label_size = sizeof(record->label);
    record->width = UINT32_MAX;
    record->height = UINT32_MAX;
    record->refresh_rate_numerator = UINT32_MAX;
    record->refresh_rate_denominator = UINT32_MAX;
    record->origin_x = (int32_t) index - 16;
    record->origin_y = INT32_MAX - (int32_t) index;
    record->flags = MOONLIGHT_PROTOCOL_V1_DISPLAY_FLAG_METADATA_KNOWN;
  }

  TEST_RESULT(
    MoonlightProtocolV1EncodeGetDisplayListResponse(
      &response,
      encoded,
      sizeof(encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(
    encoded_size ==
    MOONLIGHT_PROTOCOL_V1_GET_DISPLAY_LIST_RESPONSE_PAYLOAD_MAX
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeGetDisplayListResponse(
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
 * @brief Verifies that a complete catalog contains at most one PRIMARY record.
 *
 * @return True on success.
 */
static bool test_primary_cardinality(void) {
  MoonlightProtocolV1GetDisplayListResponse response;
  MoonlightProtocolV1GetDisplayListResponse decoded;
  uint8_t payload[MOONLIGHT_PROTOCOL_V1_GET_DISPLAY_LIST_RESPONSE_PAYLOAD_MAX];
  uint8_t nested[MOONLIGHT_PROTOCOL_V1_DISPLAY_RECORD_PAYLOAD_MAX];
  uint8_t revision[8];
  size_t payload_size;
  size_t nested_size;
  size_t encoded_size = 0;
  size_t index;

  memset(&response, 0, sizeof(response));
  response.catalog_revision = 1;
  response.entries[0] = make_record(1, "Primary candidate", true);
  response.entries[1] = make_record(2, "Secondary candidate", true);
  response.entries[0].flags &= ~MOONLIGHT_PROTOCOL_V1_DISPLAY_FLAG_PRIMARY;
  response.entries[1].flags &= ~MOONLIGHT_PROTOCOL_V1_DISPLAY_FLAG_PRIMARY;
  response.entry_count = 2;

  TEST_RESULT(
    MoonlightProtocolV1EncodeGetDisplayListResponse(
      &response,
      payload,
      sizeof(payload),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeGetDisplayListResponse(
      payload,
      encoded_size,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );

  response.entries[0].flags |= MOONLIGHT_PROTOCOL_V1_DISPLAY_FLAG_PRIMARY;
  TEST_RESULT(
    MoonlightProtocolV1EncodeGetDisplayListResponse(
      &response,
      payload,
      sizeof(payload),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeGetDisplayListResponse(
      payload,
      encoded_size,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );

  response.entries[1].flags |= MOONLIGHT_PROTOCOL_V1_DISPLAY_FLAG_PRIMARY;
  TEST_CHECK(
    response_encode_rejects(
      &response,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );

  test_store_u64(revision, response.catalog_revision);
  payload_size = test_append_field(
    payload,
    1,
    0,
    revision,
    sizeof(revision)
  );
  for (index = 0; index < response.entry_count; ++index) {
    nested_size = test_append_nested_record(&response.entries[index], nested);
    payload_size += test_append_field(
      payload + payload_size,
      2,
      MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED,
      nested,
      nested_size
    );
  }
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
 * @brief Verifies invalid encode arguments and semantic values.
 *
 * @return True on success.
 */
static bool test_encode_rejections(void) {
  MoonlightProtocolV1GetDisplayListResponse response =
    representative_response();
  uint8_t output[MOONLIGHT_PROTOCOL_V1_GET_DISPLAY_LIST_RESPONSE_PAYLOAD_MAX];
  uint8_t unchanged[sizeof(output)];
  size_t encoded_size = 777;

  TEST_CHECK(
    response_encode_rejects(
      NULL,
      MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
    )
  );
  memset(output, 0xa5, sizeof(output));
  memcpy(unchanged, output, sizeof(output));
  TEST_RESULT(
    MoonlightProtocolV1EncodeGetDisplayListResponse(
      &response,
      output,
      sizeof(output),
      NULL
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_CHECK(memcmp(output, unchanged, sizeof(output)) == 0);
  TEST_RESULT(
    MoonlightProtocolV1EncodeGetDisplayListResponse(
      &response,
      NULL,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_CHECK(encoded_size == 777u);

  memset(output, 0xa5, sizeof(output));
  memcpy(unchanged, output, sizeof(output));
  TEST_RESULT(
    MoonlightProtocolV1EncodeGetDisplayListResponse(
      &response,
      output,
      1,
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL
  );
  TEST_CHECK(memcmp(output, unchanged, sizeof(output)) == 0);
  TEST_CHECK(encoded_size == 777u);

  response.catalog_revision = 0;
  TEST_CHECK(
    response_encode_rejects(
      &response,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  response = representative_response();
  response.entry_count = MOONLIGHT_PROTOCOL_V1_DISPLAY_LIST_MAX_ENTRIES + 1u;
  TEST_CHECK(
    response_encode_rejects(
      &response,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );

  response = representative_response();
  memset(
    response.entries[0].display_id,
    0,
    sizeof(response.entries[0].display_id)
  );
  TEST_CHECK(
    response_encode_rejects(
      &response,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  response = representative_response();
  response.entries[0].label_size = 0;
  TEST_CHECK(
    response_encode_rejects(
      &response,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  response = representative_response();
  response.entries[0].label_size =
    MOONLIGHT_PROTOCOL_V1_DISPLAY_LABEL_MAX + 1u;
  TEST_CHECK(
    response_encode_rejects(
      &response,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  response = representative_response();
  response.entries[0].flags |= UINT32_C(0x08);
  TEST_CHECK(
    response_encode_rejects(
      &response,
      MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
    )
  );

  response = representative_response();
  response.entries[1] = response.entries[0];
  TEST_CHECK(
    response_encode_rejects(
      &response,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  response = representative_response();
  response.entries[0].display_id[MOONLIGHT_PROTOCOL_V1_DISPLAY_ID_SIZE - 1u] = 3;
  TEST_CHECK(
    response_encode_rejects(
      &response,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  return true;
}

/**
 * @brief Verifies every metadata-known and metadata-unknown invariant.
 *
 * @return True on success.
 */
static bool test_metadata_rejections(void) {
  MoonlightProtocolV1GetDisplayListResponse response;
  size_t index;

  memset(&response, 0, sizeof(response));
  response.catalog_revision = 1;
  response.entries[0] = make_record(1, "Display", true);
  response.entry_count = 1;

  for (index = 0; index < 4u; ++index) {
    uint32_t *required[] = {
      &response.entries[0].width,
      &response.entries[0].height,
      &response.entries[0].refresh_rate_numerator,
      &response.entries[0].refresh_rate_denominator,
    };
    const uint32_t original = *required[index];

    *required[index] = 0;
    TEST_CHECK(
      response_encode_rejects(
        &response,
        MOONLIGHT_PROTOCOL_RESULT_MALFORMED
      )
    );
    *required[index] = original;
  }

  response.entries[0] = make_record(1, "Display", false);
  response.entries[0].width = 1;
  TEST_CHECK(
    response_encode_rejects(
      &response,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  response.entries[0] = make_record(1, "Display", false);
  response.entries[0].height = 1;
  TEST_CHECK(
    response_encode_rejects(
      &response,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  response.entries[0] = make_record(1, "Display", false);
  response.entries[0].refresh_rate_numerator = 1;
  TEST_CHECK(
    response_encode_rejects(
      &response,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  response.entries[0] = make_record(1, "Display", false);
  response.entries[0].refresh_rate_denominator = 1;
  TEST_CHECK(
    response_encode_rejects(
      &response,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  response.entries[0] = make_record(1, "Display", false);
  response.entries[0].origin_x = -1;
  TEST_CHECK(
    response_encode_rejects(
      &response,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  response.entries[0] = make_record(1, "Display", false);
  response.entries[0].origin_y = 1;
  TEST_CHECK(
    response_encode_rejects(
      &response,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  response.entries[0] = make_record(1, "Display", false);
  response.entries[0].flags = MOONLIGHT_PROTOCOL_V1_DISPLAY_FLAG_PRIMARY;
  TEST_CHECK(
    response_encode_rejects(
      &response,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  return true;
}

/**
 * @brief Verifies canonical UTF-8 acceptance and malformed sequence rejection.
 *
 * @return True on success.
 */
static bool test_utf8_boundaries(void) {
  static const uint8_t valid_label[] = {
    0xc2,
    0x80,
    0xee,
    0x80,
    0x80,
    0xf4,
    0x8f,
    0xbf,
    0xbf,
  };
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
  MoonlightProtocolV1GetDisplayListResponse response;
  MoonlightProtocolV1GetDisplayListResponse decoded;
  uint8_t payload[MOONLIGHT_PROTOCOL_V1_GET_DISPLAY_LIST_RESPONSE_PAYLOAD_MAX];
  size_t payload_size;
  size_t index;

  memset(&response, 0, sizeof(response));
  response.catalog_revision = 1;
  response.entries[0] = make_record(1, "placeholder", false);
  memset(response.entries[0].label, 0, sizeof(response.entries[0].label));
  memcpy(response.entries[0].label, valid_label, sizeof(valid_label));
  response.entries[0].label_size = sizeof(valid_label);
  response.entry_count = 1;
  TEST_RESULT(
    MoonlightProtocolV1EncodeGetDisplayListResponse(
      &response,
      payload,
      sizeof(payload),
      &payload_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeGetDisplayListResponse(
      payload,
      payload_size,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(memcmp(&decoded, &response, sizeof(decoded)) == 0);

  for (index = 0; index < sizeof(invalid_sizes) / sizeof(invalid_sizes[0]); ++index) {
    response.entries[0] = make_record(1, "placeholder", false);
    memset(response.entries[0].label, 0, sizeof(response.entries[0].label));
    memcpy(
      response.entries[0].label,
      invalid_sequences[index],
      invalid_sizes[index]
    );
    response.entries[0].label_size = invalid_sizes[index];
    TEST_CHECK(
      response_encode_rejects(
        &response,
        MOONLIGHT_PROTOCOL_RESULT_MALFORMED
      )
    );

    payload_size = test_build_single_response(
      &response.entries[0],
      payload
    );
    TEST_CHECK(
      response_decode_rejects(
        payload,
        payload_size,
        MOONLIGHT_PROTOCOL_RESULT_MALFORMED
      )
    );
  }
  return true;
}

/**
 * @brief Verifies invalid decode pointers, sizes, and outer TLV structure.
 *
 * @return True on success.
 */
static bool test_decode_outer_rejections(void) {
  MoonlightProtocolV1GetDisplayListResponse decoded;
  uint8_t valid[MOONLIGHT_PROTOCOL_V1_GET_DISPLAY_LIST_RESPONSE_PAYLOAD_MAX];
  uint8_t mutated[MOONLIGHT_PROTOCOL_V1_GET_DISPLAY_LIST_RESPONSE_PAYLOAD_MAX + 1u];
  size_t valid_size = 0;
  size_t first_record_offset;

  TEST_CHECK(encode_representative_response(valid, &valid_size));
  TEST_RESULT(
    MoonlightProtocolV1DecodeGetDisplayListResponse(
      NULL,
      valid_size,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeGetDisplayListResponse(
      valid,
      valid_size,
      NULL
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_CHECK(
    response_decode_rejects(
      NULL,
      0,
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
  TEST_CHECK(
    response_decode_rejects(
      valid,
      MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + 7u,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
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
  test_store_u16(mutated, 2);
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
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
  test_store_u16(mutated + 2u, MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED);
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );
  memcpy(mutated, valid, valid_size);
  test_store_u32(mutated + 4u, 7);
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
  memset(mutated + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE, 0, 8u);
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );

  first_record_offset = representative_outer_record_offset(0);
  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated + first_record_offset, 1);
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated + first_record_offset, 3);
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );
  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated + first_record_offset + 2u, 0);
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );
  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated + first_record_offset + 2u, UINT16_C(0x0002));
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );
  memcpy(mutated, valid, valid_size);
  test_store_u32(
    mutated + first_record_offset + 4u,
    MOONLIGHT_PROTOCOL_V1_DISPLAY_RECORD_PAYLOAD_MIN - 1u
  );
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  memcpy(mutated, valid, valid_size);
  test_store_u32(
    mutated + first_record_offset + 4u,
    MOONLIGHT_PROTOCOL_V1_DISPLAY_RECORD_PAYLOAD_MAX + 1u
  );
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  TEST_CHECK(
    response_decode_rejects(
      valid,
      valid_size - 1u,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  return true;
}

/**
 * @brief Verifies nested field IDs, flags, lengths, and trailing fields.
 *
 * @return True on success.
 */
static bool test_decode_nested_rejections(void) {
  MoonlightProtocolV1GetDisplayListResponse response;
  uint8_t valid[MOONLIGHT_PROTOCOL_V1_GET_DISPLAY_LIST_RESPONSE_PAYLOAD_MAX];
  uint8_t mutated[MOONLIGHT_PROTOCOL_V1_GET_DISPLAY_LIST_RESPONSE_PAYLOAD_MAX];
  uint8_t nested[MOONLIGHT_PROTOCOL_V1_DISPLAY_RECORD_PAYLOAD_MAX + 1u];
  uint8_t extra[4] = {0, 0, 0, 0};
  size_t valid_size = 0;
  size_t candidate_size;
  size_t nested_field_offset;
  size_t outer_record_offset;
  size_t nested_size;
  uint16_t field_id;

  memset(&response, 0, sizeof(response));
  response.catalog_revision = 1;
  response.entries[0] = make_record(1, "A", true);
  response.entry_count = 1;
  TEST_RESULT(
    MoonlightProtocolV1EncodeGetDisplayListResponse(
      &response,
      valid,
      sizeof(valid),
      &valid_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  outer_record_offset = MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + 8u;
  nested_size = test_load_u32(valid + outer_record_offset + 4u);

  for (field_id = 1; field_id <= 9u; ++field_id) {
    nested_field_offset = single_response_nested_field_offset(
      response.entries[0].label_size,
      field_id
    );
    memcpy(mutated, valid, valid_size);
    test_store_u16(
      mutated + nested_field_offset + 2u,
      MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED
    );
    TEST_CHECK(
      response_decode_rejects(
        mutated,
        valid_size,
        MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
      )
    );
  }

  nested_field_offset = single_response_nested_field_offset(1, 1);
  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated + nested_field_offset, 2);
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated + nested_field_offset, 10);
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );
  memcpy(mutated, valid, valid_size);
  test_store_u32(
    mutated + nested_field_offset + 4u,
    MOONLIGHT_PROTOCOL_V1_DISPLAY_ID_SIZE - 1u
  );
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );

  nested_field_offset = single_response_nested_field_offset(1, 3);
  memcpy(mutated, valid, valid_size);
  test_store_u32(mutated + nested_field_offset + 4u, 3);
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );

  memcpy(mutated, valid, valid_size);
  candidate_size = valid_size;
  candidate_size += test_append_field(
    mutated + candidate_size,
    10,
    0,
    extra,
    0
  );
  test_store_u32(
    mutated + outer_record_offset + 4u,
    (uint32_t) (nested_size + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE)
  );
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      candidate_size,
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );

  memcpy(mutated, valid, valid_size);
  candidate_size = valid_size;
  candidate_size += test_append_field(
    mutated + candidate_size,
    9,
    0,
    extra,
    0
  );
  test_store_u32(
    mutated + outer_record_offset + 4u,
    (uint32_t) (nested_size + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE)
  );
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      candidate_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );

  memcpy(mutated, valid, valid_size);
  candidate_size = valid_size;
  mutated[candidate_size] = 0;
  ++candidate_size;
  test_store_u32(
    mutated + outer_record_offset + 4u,
    (uint32_t) (nested_size + 1u)
  );
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      candidate_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );

  response.entries[0] = make_record(1, "1234567890123", false);
  nested_size = test_append_nested_record(&response.entries[0], nested);
  TEST_CHECK(
    nested_size ==
    MOONLIGHT_PROTOCOL_V1_DISPLAY_RECORD_PAYLOAD_MIN +
      response.entries[0].label_size -
      1u
  );
  nested_size -= MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + 4u;
  candidate_size = test_wrap_nested_record(
    nested,
    nested_size,
    mutated
  );
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      candidate_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );

  nested_size = test_append_nested_record(&response.entries[0], nested);
  --nested_size;
  candidate_size = test_wrap_nested_record(
    nested,
    nested_size,
    mutated
  );
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      candidate_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );

  response.entries[0] = make_record(1, "A", false);
  memset(response.entries[0].label, 'L', sizeof(response.entries[0].label));
  response.entries[0].label_size = sizeof(response.entries[0].label);
  nested_size = test_append_nested_record(&response.entries[0], nested);
  TEST_CHECK(
    nested_size == MOONLIGHT_PROTOCOL_V1_DISPLAY_RECORD_PAYLOAD_MAX
  );
  test_store_u32(
    nested +
      MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE +
      MOONLIGHT_PROTOCOL_V1_DISPLAY_ID_SIZE +
      4u,
    MOONLIGHT_PROTOCOL_V1_DISPLAY_LABEL_MAX + 1u
  );
  candidate_size = test_wrap_nested_record(
    nested,
    nested_size,
    mutated
  );
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      candidate_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );

  memset(nested, 0, sizeof(nested));
  candidate_size = test_wrap_nested_record(
    nested,
    sizeof(nested),
    mutated
  );
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      candidate_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  return true;
}

/**
 * @brief Verifies decoded Display IDs, ordering, and record-count limits.
 *
 * @return True on success.
 */
static bool test_decode_identity_and_count_rejections(void) {
  const MoonlightProtocolV1GetDisplayListResponse response =
    representative_response();
  MoonlightProtocolV1DisplayRecord minimal;
  uint8_t valid[MOONLIGHT_PROTOCOL_V1_GET_DISPLAY_LIST_RESPONSE_PAYLOAD_MAX];
  uint8_t mutated[MOONLIGHT_PROTOCOL_V1_GET_DISPLAY_LIST_RESPONSE_PAYLOAD_MAX];
  uint8_t nested[MOONLIGHT_PROTOCOL_V1_DISPLAY_RECORD_PAYLOAD_MAX];
  uint8_t revision[8];
  size_t valid_size = 0;
  size_t first_id_offset;
  size_t second_id_offset;
  size_t second_outer_offset;
  size_t nested_size;
  size_t payload_size;
  size_t index;

  TEST_RESULT(
    MoonlightProtocolV1EncodeGetDisplayListResponse(
      &response,
      valid,
      sizeof(valid),
      &valid_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  first_id_offset =
    representative_outer_record_offset(0) +
    2u * MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE;
  second_outer_offset = representative_outer_record_offset(1);
  second_id_offset =
    second_outer_offset + 2u * MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE;

  memcpy(mutated, valid, valid_size);
  memset(
    mutated + first_id_offset,
    0,
    MOONLIGHT_PROTOCOL_V1_DISPLAY_ID_SIZE
  );
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  memcpy(mutated, valid, valid_size);
  memcpy(
    mutated + second_id_offset,
    mutated + first_id_offset,
    MOONLIGHT_PROTOCOL_V1_DISPLAY_ID_SIZE
  );
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  memcpy(mutated, valid, valid_size);
  mutated[second_id_offset + MOONLIGHT_PROTOCOL_V1_DISPLAY_ID_SIZE - 1u] =
    0;
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );

  minimal = make_record(1, "A", false);
  nested_size = test_append_nested_record(&minimal, nested);
  test_store_u64(revision, 1);
  payload_size = test_append_field(
    mutated,
    1,
    0,
    revision,
    sizeof(revision)
  );
  for (index = 0; index <= MOONLIGHT_PROTOCOL_V1_DISPLAY_LIST_MAX_ENTRIES; ++index) {
    memset(minimal.display_id, 0, sizeof(minimal.display_id));
    minimal.display_id[MOONLIGHT_PROTOCOL_V1_DISPLAY_ID_SIZE - 1u] = (uint8_t) (index + 1u);
    nested_size = test_append_nested_record(&minimal, nested);
    payload_size += test_append_field(
      mutated + payload_size,
      2,
      MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED,
      nested,
      nested_size
    );
  }
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      payload_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  return true;
}

/**
 * @brief Verifies decoder classification for metadata and flag violations.
 *
 * @return True on success.
 */
static bool test_decode_metadata_rejections(void) {
  MoonlightProtocolV1GetDisplayListResponse response;
  uint8_t valid[MOONLIGHT_PROTOCOL_V1_GET_DISPLAY_LIST_RESPONSE_PAYLOAD_MAX];
  uint8_t mutated[MOONLIGHT_PROTOCOL_V1_GET_DISPLAY_LIST_RESPONSE_PAYLOAD_MAX];
  size_t valid_size = 0;
  size_t scalar_offset;
  size_t index;

  memset(&response, 0, sizeof(response));
  response.catalog_revision = 1;
  response.entries[0] = make_record(1, "A", true);
  response.entry_count = 1;
  TEST_RESULT(
    MoonlightProtocolV1EncodeGetDisplayListResponse(
      &response,
      valid,
      sizeof(valid),
      &valid_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  for (index = 0; index < 4u; ++index) {
    scalar_offset =
      single_response_nested_field_offset(1, (uint16_t) (index + 3u)) +
      MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE;
    memcpy(mutated, valid, valid_size);
    memset(mutated + scalar_offset, 0, 4u);
    TEST_CHECK(
      response_decode_rejects(
        mutated,
        valid_size,
        MOONLIGHT_PROTOCOL_RESULT_MALFORMED
      )
    );
  }
  scalar_offset =
    single_response_nested_field_offset(1, 9) +
    MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE;
  memcpy(mutated, valid, valid_size);
  test_store_u32(
    mutated + scalar_offset,
    MOONLIGHT_PROTOCOL_V1_DISPLAY_FLAG_METADATA_KNOWN | UINT32_C(0x08)
  );
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );

  response.entries[0] = make_record(1, "A", false);
  TEST_RESULT(
    MoonlightProtocolV1EncodeGetDisplayListResponse(
      &response,
      valid,
      sizeof(valid),
      &valid_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  for (index = 0; index < 6u; ++index) {
    scalar_offset =
      single_response_nested_field_offset(1, (uint16_t) (index + 3u)) +
      MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE;
    memcpy(mutated, valid, valid_size);
    test_store_u32(mutated + scalar_offset, 1);
    TEST_CHECK(
      response_decode_rejects(
        mutated,
        valid_size,
        MOONLIGHT_PROTOCOL_RESULT_MALFORMED
      )
    );
  }
  scalar_offset =
    single_response_nested_field_offset(1, 9) +
    MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE;
  memcpy(mutated, valid, valid_size);
  test_store_u32(
    mutated + scalar_offset,
    MOONLIGHT_PROTOCOL_V1_DISPLAY_FLAG_HDR_ENABLED
  );
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  return true;
}

/**
 * @brief Executes every GET_DISPLAY_LIST schema test.
 *
 * @return Zero only when every test succeeds.
 */
int main(void) {
  static const struct {
    const char *name;  ///< Human-readable test name.
    bool (*run)(void);  ///< Test implementation.
  } tests[] = {
    {"empty catalog canonical", test_empty_catalog_canonical},
    {"multiple-record round trip", test_multiple_record_round_trip},
    {"maximum catalog round trip", test_maximum_catalog_round_trip},
    {"PRIMARY cardinality", test_primary_cardinality},
    {"encode rejections", test_encode_rejections},
    {"metadata rejections", test_metadata_rejections},
    {"UTF-8 boundaries", test_utf8_boundaries},
    {"decode outer rejections", test_decode_outer_rejections},
    {"decode nested rejections", test_decode_nested_rejections},
    {"decode identity and count rejections", test_decode_identity_and_count_rejections},
    {"decode metadata rejections", test_decode_metadata_rejections},
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
