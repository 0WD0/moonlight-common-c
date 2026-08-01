/**
 * @file test_host_info.c
 * @brief Native tests for the protocol version 1 GET_HOST_INFO response schema.
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
 * @brief Byte count of the canonical GET_HOST_INFO response golden.
 */
#define TEST_HOST_INFO_GOLDEN_SIZE \
  (MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE + 153u)

/**
 * @brief Fail the current boolean test when a condition is false.
 */
#define TEST_CHECK(condition) \
  do { \
    if (!(condition)) { \
      fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
      return false; \
    } \
  } while (0)

/**
 * @brief Fail the current boolean test when a codec result differs.
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
 * @brief Builds the response represented by the shared golden vector.
 *
 * @return Valid response with every defined capability and permission.
 */
static MoonlightProtocolV1HostInfoResponse valid_response(void) {
  MoonlightProtocolV1HostInfoResponse response = {
    .host_id = {
      0x00,
      0x01,
      0x02,
      0x03,
      0x04,
      0x05,
      0x06,
      0x07,
      0x08,
      0x09,
      0x0a,
      0x0b,
      0x0c,
      0x0d,
      0x0e,
      0x0f,
    },
    .display_name = {
      'S',
      'u',
      'n',
      's',
      'h',
      'i',
      'n',
      'e',
      ' ',
      0xf0,
      0x9f,
      0x8c,
      0x9e,
    },
    .display_name_size = 13,
    .software_version = {'2', '0', '2', '6', '.', '7', '.', '3', '0'},
    .software_version_size = 9,
    .configured_quic_port = 47989,
    .capability_bits = MOONLIGHT_PROTOCOL_V1_CAPABILITY_MASK,
    .maximum_active_stream_sessions = MOONLIGHT_PROTOCOL_V1_ACTIVE_STREAM_SESSION_MAX,
    .available_stream_session_slots = 3,
    .acl_permission_bits = MOONLIGHT_PROTOCOL_V1_ACL_PERMISSION_MASK,
    .authorization_generation = UINT64_C(0x1112131415161718),
    .instance_visibility = MOONLIGHT_PROTOCOL_V1_INSTANCE_VISIBILITY_SHARED,
  };

  return response;
}

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
 * @param name File name relative to the version 1 test-data directory.
 * @param output Destination byte buffer.
 * @param output_capacity Available bytes in `output`.
 * @param output_size Receives the decoded byte count.
 * @return True only for complete hexadecimal byte pairs and ASCII whitespace.
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
    fprintf(stderr, "golden path is too long: %s\n", name);
    return false;
  }
  file = fopen(path, "rb");
  if (file == NULL) {
    fprintf(stderr, "cannot open golden vector: %s\n", path);
    return false;
  }

  while ((character = fgetc(file)) != EOF) {
    const int nibble = hex_value(character);

    if (nibble >= 0) {
      if (high_nibble < 0) {
        high_nibble = nibble;
      } else {
        if (size >= output_capacity) {
          valid = false;
          break;
        }
        output[size++] = (uint8_t) ((high_nibble << 4) | nibble);
        high_nibble = -1;
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
    fprintf(stderr, "invalid strict hexadecimal golden vector: %s\n", path);
    return false;
  }
  *output_size = size;
  return true;
}

/**
 * @brief Encodes one valid response for mutation tests.
 *
 * @param response Valid host-order response.
 * @param output Maximum-size destination.
 * @param output_size Receives the encoded byte count.
 * @return True on success.
 */
static bool encode_response(
  const MoonlightProtocolV1HostInfoResponse *response,
  uint8_t *output,
  size_t *output_size
) {
  TEST_RESULT(
    MoonlightProtocolV1EncodeHostInfoResponse(
      response,
      output,
      MOONLIGHT_PROTOCOL_V1_HOST_INFO_RESPONSE_PAYLOAD_MAX,
      output_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  return true;
}

/**
 * @brief Verifies one rejected decode and failure output atomicity.
 *
 * @param input Complete candidate payload.
 * @param input_size Candidate payload size.
 * @param expected Expected classification.
 * @return True when both result and output atomicity match.
 */
static bool decode_rejects(
  const uint8_t *input,
  size_t input_size,
  MoonlightProtocolResult expected
) {
  MoonlightProtocolV1HostInfoResponse output;
  MoonlightProtocolV1HostInfoResponse unchanged;

  memset(&output, 0xa5, sizeof(output));
  unchanged = output;
  TEST_RESULT(
    MoonlightProtocolV1DecodeHostInfoResponse(
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
 * @brief Verifies the shared complete response golden in both directions.
 *
 * @return True on success.
 */
static bool test_golden(void) {
  uint8_t vector[TEST_HOST_INFO_GOLDEN_SIZE];
  uint8_t canonical[TEST_HOST_INFO_GOLDEN_SIZE];
  MoonlightProtocolV1MessageEnvelope envelope;
  MoonlightProtocolV1HostInfoResponse decoded;
  const MoonlightProtocolV1HostInfoResponse expected = valid_response();
  size_t vector_size = 0;
  size_t payload_size = 0;

  TEST_CHECK(
    load_golden(
      "host-info-success-response.hex",
      vector,
      sizeof(vector),
      &vector_size
    )
  );
  TEST_CHECK(vector_size == sizeof(vector));
  TEST_RESULT(
    MoonlightProtocolV1DecodeMessageEnvelope(
      vector,
      MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE,
      MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX,
      &envelope
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(
    envelope.message_type == MOONLIGHT_PROTOCOL_V1_MESSAGE_GET_HOST_INFO
  );
  TEST_CHECK(envelope.flags == MOONLIGHT_PROTOCOL_V1_ENVELOPE_FLAG_RESPONSE);
  TEST_CHECK(envelope.payload_length == 153u);
  TEST_CHECK(envelope.status == MOONLIGHT_PROTOCOL_V1_STATUS_OK);
  TEST_CHECK(envelope.correlation_id == UINT64_C(0x0102030405060708));
  TEST_RESULT(
    MoonlightProtocolV1DecodeHostInfoResponse(
      vector + MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE,
      envelope.payload_length,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(
    memcmp(decoded.host_id, expected.host_id, sizeof(decoded.host_id)) == 0
  );
  TEST_CHECK(decoded.display_name_size == expected.display_name_size);
  TEST_CHECK(
    memcmp(
      decoded.display_name,
      expected.display_name,
      expected.display_name_size
    ) == 0
  );
  TEST_CHECK(
    decoded.software_version_size == expected.software_version_size
  );
  TEST_CHECK(
    memcmp(
      decoded.software_version,
      expected.software_version,
      expected.software_version_size
    ) == 0
  );
  TEST_CHECK(decoded.configured_quic_port == expected.configured_quic_port);
  TEST_CHECK(decoded.capability_bits == expected.capability_bits);
  TEST_CHECK(
    decoded.maximum_active_stream_sessions ==
    expected.maximum_active_stream_sessions
  );
  TEST_CHECK(
    decoded.available_stream_session_slots ==
    expected.available_stream_session_slots
  );
  TEST_CHECK(decoded.acl_permission_bits == expected.acl_permission_bits);
  TEST_CHECK(
    decoded.authorization_generation == expected.authorization_generation
  );
  TEST_CHECK(decoded.instance_visibility == expected.instance_visibility);

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
    MoonlightProtocolV1EncodeHostInfoResponse(
      &decoded,
      canonical + MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE,
      sizeof(canonical) - MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE,
      &payload_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(payload_size == envelope.payload_length);
  TEST_CHECK(memcmp(canonical, vector, sizeof(vector)) == 0);
  return true;
}

/**
 * @brief Verifies minimum and maximum string/session/ACL bounds.
 *
 * @return True on success.
 */
static bool test_round_trip_bounds(void) {
  MoonlightProtocolV1HostInfoResponse response = valid_response();
  MoonlightProtocolV1HostInfoResponse decoded;
  uint8_t encoded[MOONLIGHT_PROTOCOL_V1_HOST_INFO_RESPONSE_PAYLOAD_MAX];
  size_t encoded_size = 0;
  size_t index;

  memset(response.display_name, 0, sizeof(response.display_name));
  response.display_name[0] = 'H';
  response.display_name_size = 1;
  memset(response.software_version, 0, sizeof(response.software_version));
  response.software_version[0] = '1';
  response.software_version_size = 1;
  response.capability_bits = 0;
  response.maximum_active_stream_sessions = 1;
  response.available_stream_session_slots = 0;
  response.acl_permission_bits = 0;
  response.instance_visibility =
    MOONLIGHT_PROTOCOL_V1_INSTANCE_VISIBILITY_OWNER_ONLY;
  TEST_CHECK(encode_response(&response, encoded, &encoded_size));
  TEST_CHECK(
    encoded_size == MOONLIGHT_PROTOCOL_V1_HOST_INFO_RESPONSE_PAYLOAD_MIN
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeHostInfoResponse(
      encoded,
      encoded_size,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(decoded.display_name_size == 1);
  TEST_CHECK(decoded.software_version_size == 1);
  TEST_CHECK(decoded.available_stream_session_slots == 0);
  TEST_CHECK(decoded.acl_permission_bits == 0);

  for (index = 0; index < sizeof(response.display_name) - 4u; ++index) {
    response.display_name[index] = (uint8_t) ('a' + index % 26u);
  }
  response.display_name[124] = 0xf0;
  response.display_name[125] = 0x9f;
  response.display_name[126] = 0x98;
  response.display_name[127] = 0x80;
  response.display_name_size = sizeof(response.display_name);
  for (index = 0; index < sizeof(response.software_version) - 4u; ++index) {
    response.software_version[index] = (uint8_t) ('0' + index % 10u);
  }
  response.software_version[60] = 0xf4;
  response.software_version[61] = 0x8f;
  response.software_version[62] = 0xbf;
  response.software_version[63] = 0xbf;
  response.software_version_size = sizeof(response.software_version);
  response.capability_bits = MOONLIGHT_PROTOCOL_V1_CAPABILITY_MASK;
  response.maximum_active_stream_sessions =
    MOONLIGHT_PROTOCOL_V1_ACTIVE_STREAM_SESSION_MAX;
  response.available_stream_session_slots =
    MOONLIGHT_PROTOCOL_V1_ACTIVE_STREAM_SESSION_MAX;
  response.acl_permission_bits = MOONLIGHT_PROTOCOL_V1_ACL_PERMISSION_MASK;
  response.instance_visibility =
    MOONLIGHT_PROTOCOL_V1_INSTANCE_VISIBILITY_SHARED;
  TEST_CHECK(encode_response(&response, encoded, &encoded_size));
  TEST_CHECK(
    encoded_size == MOONLIGHT_PROTOCOL_V1_HOST_INFO_RESPONSE_PAYLOAD_MAX
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeHostInfoResponse(
      encoded,
      encoded_size,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(
    decoded.display_name_size == sizeof(response.display_name)
  );
  TEST_CHECK(
    decoded.software_version_size == sizeof(response.software_version)
  );
  TEST_CHECK(
    decoded.available_stream_session_slots ==
    MOONLIGHT_PROTOCOL_V1_ACTIVE_STREAM_SESSION_MAX
  );
  return true;
}

/**
 * @brief Verifies strict canonical UTF-8 on encode and decode.
 *
 * @return True on success.
 */
static bool test_utf8_rejections(void) {
  static const uint8_t invalid_sequences[][4] = {
    {0x00, 0x00, 0x00, 0x00},
    {0x80, 0x00, 0x00, 0x00},
    {0xc0, 0x80, 0x00, 0x00},
    {0xc2, 0x20, 0x00, 0x00},
    {0xe0, 0x80, 0x80, 0x00},
    {0xed, 0xa0, 0x80, 0x00},
    {0xf4, 0x90, 0x80, 0x80},
    {0xf5, 0x80, 0x80, 0x80},
  };
  static const size_t invalid_sizes[] = {1, 1, 2, 2, 3, 3, 4, 4};
  MoonlightProtocolV1HostInfoResponse response;
  uint8_t encoded[MOONLIGHT_PROTOCOL_V1_HOST_INFO_RESPONSE_PAYLOAD_MAX];
  uint8_t mutated[MOONLIGHT_PROTOCOL_V1_HOST_INFO_RESPONSE_PAYLOAD_MAX];
  size_t encoded_size = 0;
  size_t index;

  for (index = 0; index < sizeof(invalid_sizes) / sizeof(invalid_sizes[0]); ++index) {
    response = valid_response();
    memset(response.display_name, 'A', sizeof(response.display_name));
    memcpy(
      response.display_name,
      invalid_sequences[index],
      invalid_sizes[index]
    );
    response.display_name_size = invalid_sizes[index];
    TEST_RESULT(
      MoonlightProtocolV1EncodeHostInfoResponse(
        &response,
        encoded,
        sizeof(encoded),
        &encoded_size
      ),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    );

    response = valid_response();
    memset(response.software_version, '1', sizeof(response.software_version));
    memcpy(
      response.software_version,
      invalid_sequences[index],
      invalid_sizes[index]
    );
    response.software_version_size = invalid_sizes[index];
    TEST_RESULT(
      MoonlightProtocolV1EncodeHostInfoResponse(
        &response,
        encoded,
        sizeof(encoded),
        &encoded_size
      ),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    );
  }

  response = valid_response();
  TEST_CHECK(encode_response(&response, encoded, &encoded_size));
  for (index = 0; index < sizeof(invalid_sizes) / sizeof(invalid_sizes[0]); ++index) {
    memcpy(mutated, encoded, encoded_size);
    memcpy(mutated + 32u, invalid_sequences[index], invalid_sizes[index]);
    TEST_CHECK(
      decode_rejects(
        mutated,
        encoded_size,
        MOONLIGHT_PROTOCOL_RESULT_MALFORMED
      )
    );

    memcpy(mutated, encoded, encoded_size);
    memcpy(mutated + 53u, invalid_sequences[index], invalid_sizes[index]);
    TEST_CHECK(
      decode_rejects(
        mutated,
        encoded_size,
        MOONLIGHT_PROTOCOL_RESULT_MALFORMED
      )
    );
  }
  return true;
}

/**
 * @brief Verifies encode arguments, ranges, and output atomicity.
 *
 * @return True on success.
 */
static bool test_encode_rejections(void) {
  MoonlightProtocolV1HostInfoResponse response = valid_response();
  uint8_t output[MOONLIGHT_PROTOCOL_V1_HOST_INFO_RESPONSE_PAYLOAD_MAX];
  uint8_t unchanged[sizeof(output)];
  size_t encoded_size = 777;

  memset(output, 0xa5, sizeof(output));
  memcpy(unchanged, output, sizeof(output));
  TEST_RESULT(
    MoonlightProtocolV1EncodeHostInfoResponse(
      NULL,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeHostInfoResponse(
      &response,
      NULL,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeHostInfoResponse(
      &response,
      output,
      sizeof(output),
      NULL
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );

  memset(response.host_id, 0, sizeof(response.host_id));
  TEST_RESULT(
    MoonlightProtocolV1EncodeHostInfoResponse(
      &response,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  response = valid_response();
  response.display_name_size = 0;
  TEST_RESULT(
    MoonlightProtocolV1EncodeHostInfoResponse(
      &response,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  response = valid_response();
  response.display_name_size =
    MOONLIGHT_PROTOCOL_V1_HOST_DISPLAY_NAME_MAX + 1u;
  TEST_RESULT(
    MoonlightProtocolV1EncodeHostInfoResponse(
      &response,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  response = valid_response();
  response.software_version_size = 0;
  TEST_RESULT(
    MoonlightProtocolV1EncodeHostInfoResponse(
      &response,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  response = valid_response();
  response.software_version_size =
    MOONLIGHT_PROTOCOL_V1_SOFTWARE_VERSION_MAX + 1u;
  TEST_RESULT(
    MoonlightProtocolV1EncodeHostInfoResponse(
      &response,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  response = valid_response();
  response.configured_quic_port = 0;
  TEST_RESULT(
    MoonlightProtocolV1EncodeHostInfoResponse(
      &response,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  response = valid_response();
  response.capability_bits = UINT64_C(0x80);
  TEST_RESULT(
    MoonlightProtocolV1EncodeHostInfoResponse(
      &response,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
  );
  response = valid_response();
  response.maximum_active_stream_sessions = 0;
  TEST_RESULT(
    MoonlightProtocolV1EncodeHostInfoResponse(
      &response,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  response.maximum_active_stream_sessions =
    MOONLIGHT_PROTOCOL_V1_ACTIVE_STREAM_SESSION_MAX + 1u;
  TEST_RESULT(
    MoonlightProtocolV1EncodeHostInfoResponse(
      &response,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  response = valid_response();
  response.available_stream_session_slots =
    response.maximum_active_stream_sessions + 1u;
  TEST_RESULT(
    MoonlightProtocolV1EncodeHostInfoResponse(
      &response,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  response = valid_response();
  response.acl_permission_bits = UINT64_C(0x80);
  TEST_RESULT(
    MoonlightProtocolV1EncodeHostInfoResponse(
      &response,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  response = valid_response();
  response.authorization_generation = 0;
  TEST_RESULT(
    MoonlightProtocolV1EncodeHostInfoResponse(
      &response,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  response = valid_response();
  response.instance_visibility = (MoonlightProtocolV1InstanceVisibility) 0;
  TEST_RESULT(
    MoonlightProtocolV1EncodeHostInfoResponse(
      &response,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  response.instance_visibility = (MoonlightProtocolV1InstanceVisibility) 3;
  TEST_RESULT(
    MoonlightProtocolV1EncodeHostInfoResponse(
      &response,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  response = valid_response();
  TEST_RESULT(
    MoonlightProtocolV1EncodeHostInfoResponse(
      &response,
      output,
      MOONLIGHT_PROTOCOL_V1_HOST_INFO_RESPONSE_PAYLOAD_MIN - 1u,
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL
  );
  TEST_CHECK(memcmp(output, unchanged, sizeof(output)) == 0);
  TEST_CHECK(encoded_size == 777u);
  return true;
}

/**
 * @brief Verifies field identity, length, flag, and trailing-data classification.
 *
 * @return True on success.
 */
static bool test_decode_structure_rejections(void) {
  static const size_t field_offsets[] = {
    0u,
    24u,
    45u,
    62u,
    72u,
    88u,
    100u,
    112u,
    128u,
    144u,
  };
  static const uint16_t known_wrong_ids[] = {
    2u,
    1u,
    2u,
    3u,
    4u,
    5u,
    6u,
    7u,
    8u,
    9u,
  };
  const MoonlightProtocolV1HostInfoResponse response = valid_response();
  MoonlightProtocolV1HostInfoResponse output;
  uint8_t valid[MOONLIGHT_PROTOCOL_V1_HOST_INFO_RESPONSE_PAYLOAD_MAX];
  uint8_t mutated[MOONLIGHT_PROTOCOL_V1_HOST_INFO_RESPONSE_PAYLOAD_MAX + 1u];
  size_t valid_size = 0;
  size_t index;

  TEST_CHECK(encode_response(&response, valid, &valid_size));
  TEST_CHECK(valid_size == 153u);
  TEST_RESULT(
    MoonlightProtocolV1DecodeHostInfoResponse(NULL, valid_size, &output),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeHostInfoResponse(valid, valid_size, NULL),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  memset(mutated, 0, sizeof(mutated));
  TEST_CHECK(
    decode_rejects(
      mutated,
      sizeof(mutated),
      MOONLIGHT_PROTOCOL_RESULT_LIMIT_EXCEEDED
    )
  );
  for (index = 0; index < valid_size; ++index) {
    TEST_CHECK(
      decode_rejects(
        valid,
        index,
        MOONLIGHT_PROTOCOL_RESULT_MALFORMED
      )
    );
  }

  for (index = 0; index < sizeof(field_offsets) / sizeof(field_offsets[0]); ++index) {
    memcpy(mutated, valid, valid_size);
    test_store_u16(mutated + field_offsets[index], known_wrong_ids[index]);
    TEST_CHECK(
      decode_rejects(
        mutated,
        valid_size,
        MOONLIGHT_PROTOCOL_RESULT_MALFORMED
      )
    );

    memcpy(mutated, valid, valid_size);
    test_store_u16(mutated + field_offsets[index], 11u);
    TEST_CHECK(
      decode_rejects(
        mutated,
        valid_size,
        MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
      )
    );

    memcpy(mutated, valid, valid_size);
    test_store_u16(
      mutated + field_offsets[index] + 2u,
      MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED
    );
    TEST_CHECK(
      decode_rejects(
        mutated,
        valid_size,
        MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
      )
    );

    memcpy(mutated, valid, valid_size);
    test_store_u32(mutated + field_offsets[index] + 4u, 0);
    TEST_CHECK(
      decode_rejects(
        mutated,
        valid_size,
        MOONLIGHT_PROTOCOL_RESULT_MALFORMED
      )
    );
  }
  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated, 0);
  TEST_CHECK(
    decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );
  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated + 2u, 2u);
  TEST_CHECK(
    decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );
  memcpy(mutated, valid, valid_size);
  test_store_u32(mutated + 4u, UINT32_MAX);
  TEST_CHECK(
    decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );

  memcpy(mutated, valid, valid_size);
  mutated[valid_size] = 0;
  TEST_CHECK(
    decode_rejects(
      mutated,
      valid_size + 1u,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  memcpy(mutated, valid, valid_size);
  memset(mutated + valid_size, 0, MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE);
  test_store_u16(mutated + valid_size, 11u);
  TEST_CHECK(
    decode_rejects(
      mutated,
      valid_size + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );
  test_store_u16(mutated + valid_size, 10u);
  TEST_CHECK(
    decode_rejects(
      mutated,
      valid_size + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  test_store_u16(mutated + valid_size, 11u);
  test_store_u32(mutated + valid_size + 4u, 1u);
  TEST_CHECK(
    decode_rejects(
      mutated,
      valid_size + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  return true;
}

/**
 * @brief Verifies every scalar semantic range on decode.
 *
 * @return True on success.
 */
static bool test_decode_value_rejections(void) {
  const MoonlightProtocolV1HostInfoResponse response = valid_response();
  uint8_t valid[MOONLIGHT_PROTOCOL_V1_HOST_INFO_RESPONSE_PAYLOAD_MAX];
  uint8_t mutated[MOONLIGHT_PROTOCOL_V1_HOST_INFO_RESPONSE_PAYLOAD_MAX];
  size_t valid_size = 0;

  TEST_CHECK(encode_response(&response, valid, &valid_size));

  memcpy(mutated, valid, valid_size);
  memset(mutated + 8u, 0, MOONLIGHT_PROTOCOL_V1_UUID_SIZE);
  TEST_CHECK(
    decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  memcpy(mutated, valid, valid_size);
  memset(mutated + 70u, 0, 2);
  TEST_CHECK(
    decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  memcpy(mutated, valid, valid_size);
  memset(mutated + 80u, 0, 8);
  mutated[87] = 0x80;
  TEST_CHECK(
    decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
    )
  );
  memcpy(mutated, valid, valid_size);
  memset(mutated + 96u, 0, 4);
  TEST_CHECK(
    decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  memcpy(mutated, valid, valid_size);
  test_store_u32(
    mutated + 96u,
    MOONLIGHT_PROTOCOL_V1_ACTIVE_STREAM_SESSION_MAX + 1u
  );
  TEST_CHECK(
    decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  memcpy(mutated, valid, valid_size);
  test_store_u32(mutated + 108u, 9u);
  TEST_CHECK(
    decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  memcpy(mutated, valid, valid_size);
  memset(mutated + 120u, 0, 8);
  mutated[127] = 0x80;
  TEST_CHECK(
    decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  memcpy(mutated, valid, valid_size);
  memset(mutated + 136u, 0, 8);
  TEST_CHECK(
    decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  memcpy(mutated, valid, valid_size);
  mutated[152] = 0;
  TEST_CHECK(
    decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  memcpy(mutated, valid, valid_size);
  mutated[152] = 3;
  TEST_CHECK(
    decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  return true;
}

/**
 * @brief Executes every GET_HOST_INFO schema test.
 *
 * @return Zero only when every test succeeds.
 */
int main(void) {
  static const struct {
    const char *name;  ///< Human-readable test name.
    bool (*run)(void);  ///< Test implementation.
  } tests[] = {
    {"golden", test_golden},
    {"round-trip bounds", test_round_trip_bounds},
    {"UTF-8 rejections", test_utf8_rejections},
    {"encode rejections", test_encode_rejections},
    {"decode structure rejections", test_decode_structure_rejections},
    {"decode value rejections", test_decode_value_rejections},
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
