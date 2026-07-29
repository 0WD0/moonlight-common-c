/**
 * @file test_pairing.c
 * @brief Native tests for protocol version 1 Pair Control Lane pairing schemas.
 */

#include <moonlight/protocol/pairing.h>
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
 * @brief Complete byte count of the maximum PAIR_REQUEST golden message.
 */
#define TEST_PAIR_REQUEST_GOLDEN_SIZE \
  (MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE + \
   MOONLIGHT_PROTOCOL_V1_PAIR_REQUEST_PAYLOAD_MAX)

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
 * @brief Builds one valid PAIR_REQUEST request.
 *
 * @return Valid request with a six-byte Client name.
 */
static MoonlightProtocolV1PairRequest valid_request(void) {
  MoonlightProtocolV1PairRequest request = {
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
    .token_id = {
      0x10,
      0x11,
      0x12,
      0x13,
      0x14,
      0x15,
      0x16,
      0x17,
      0x18,
      0x19,
      0x1a,
      0x1b,
      0x1c,
      0x1d,
      0x1e,
      0x1f,
    },
    .invitation_secret = {
      0x20,
      0x21,
      0x22,
      0x23,
      0x24,
      0x25,
      0x26,
      0x27,
      0x28,
      0x29,
      0x2a,
      0x2b,
      0x2c,
      0x2d,
      0x2e,
      0x2f,
      0x30,
      0x31,
      0x32,
      0x33,
      0x34,
      0x35,
      0x36,
      0x37,
      0x38,
      0x39,
      0x3a,
      0x3b,
      0x3c,
      0x3d,
      0x3e,
      0x3f,
    },
    .client_name = {'C', 'l', 'i', 'e', 'n', 't'},
    .client_name_size = 6,
  };

  return request;
}

/**
 * @brief Builds one valid successful PAIR_REQUEST response.
 *
 * @return Valid response granting every defined permission.
 */
static MoonlightProtocolV1PairResponse valid_response(void) {
  MoonlightProtocolV1PairResponse response = {
    .client_principal_id = {
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
    .host_id = {
      0xf0,
      0xf1,
      0xf2,
      0xf3,
      0xf4,
      0xf5,
      0xf6,
      0xf7,
      0xf8,
      0xf9,
      0xfa,
      0xfb,
      0xfc,
      0xfd,
      0xfe,
      0xff,
    },
    .granted_acl_permission_bits = MOONLIGHT_PROTOCOL_V1_ACL_PERMISSION_MASK,
    .authorization_generation = UINT64_C(0x0102030405060708),
    .credential_epoch = 1,
    .instance_visibility = MOONLIGHT_PROTOCOL_V1_INSTANCE_VISIBILITY_SHARED,
  };

  return response;
}

/**
 * @brief Stores one big-endian 16-bit value in a mutable test vector.
 *
 * @param output Two writable bytes.
 * @param value Host-order value.
 */
static void test_store_u16(uint8_t *output, uint16_t value) {
  output[0] = (uint8_t) (value >> 8u);
  output[1] = (uint8_t) value;
}

/**
 * @brief Stores one big-endian 32-bit value in a mutable test vector.
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
 * @brief Converts one ASCII hexadecimal digit to its integer value.
 *
 * @param character Character to convert.
 * @return A value in `[0, 15]`, or `-1` when the character is not hexadecimal.
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
 * @brief Tests whether a character is ASCII whitespace permitted in a golden file.
 *
 * @param character Character to test.
 * @return True for the six ASCII whitespace characters.
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
 * @brief Loads a strict hexadecimal golden file without comments or prefixes.
 *
 * @param name File name relative to the version 1 test-data directory.
 * @param output Destination byte buffer.
 * @param output_capacity Available bytes in `output`.
 * @param output_size Receives the decoded byte count.
 * @return True when the file contains only complete hexadecimal byte pairs and whitespace.
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
 * @brief Encodes one request for mutation tests.
 *
 * @param request Valid request.
 * @param output Destination with maximum request capacity.
 * @param output_size Receives the encoded size.
 * @return True on success.
 */
static bool encode_request(
  const MoonlightProtocolV1PairRequest *request,
  uint8_t *output,
  size_t *output_size
) {
  TEST_RESULT(
    MoonlightProtocolV1EncodePairRequest(
      request,
      output,
      MOONLIGHT_PROTOCOL_V1_PAIR_REQUEST_PAYLOAD_MAX,
      output_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  return true;
}

/**
 * @brief Encodes one valid response for mutation tests.
 *
 * @param output Destination with exact response capacity.
 * @return True on success.
 */
static bool encode_valid_response(uint8_t *output) {
  const MoonlightProtocolV1PairResponse response = valid_response();
  size_t output_size = 0;

  TEST_RESULT(
    MoonlightProtocolV1EncodePairResponse(
      &response,
      output,
      MOONLIGHT_PROTOCOL_V1_PAIR_RESPONSE_PAYLOAD_SIZE,
      &output_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(
    output_size == MOONLIGHT_PROTOCOL_V1_PAIR_RESPONSE_PAYLOAD_SIZE
  );
  return true;
}

/**
 * @brief Verifies one rejected request decode and its output atomicity.
 *
 * @param input Complete candidate payload.
 * @param input_size Candidate payload size.
 * @param expected Expected rejection.
 * @return True when the result and output atomicity match.
 */
static bool request_decode_rejects(
  const uint8_t *input,
  size_t input_size,
  MoonlightProtocolResult expected
) {
  MoonlightProtocolV1PairRequest output;
  MoonlightProtocolV1PairRequest unchanged;

  memset(&output, 0xa5, sizeof(output));
  unchanged = output;
  TEST_RESULT(
    MoonlightProtocolV1DecodePairRequest(input, input_size, &output),
    expected
  );
  TEST_CHECK(memcmp(&output, &unchanged, sizeof(output)) == 0);
  return true;
}

/**
 * @brief Verifies one rejected response decode and its output atomicity.
 *
 * @param input Complete candidate payload.
 * @param input_size Candidate payload size.
 * @param expected Expected rejection.
 * @return True when the result and output atomicity match.
 */
static bool response_decode_rejects(
  const uint8_t *input,
  size_t input_size,
  MoonlightProtocolResult expected
) {
  MoonlightProtocolV1PairResponse output;
  MoonlightProtocolV1PairResponse unchanged;

  memset(&output, 0xa5, sizeof(output));
  unchanged = output;
  TEST_RESULT(
    MoonlightProtocolV1DecodePairResponse(input, input_size, &output),
    expected
  );
  TEST_CHECK(memcmp(&output, &unchanged, sizeof(output)) == 0);
  return true;
}

/**
 * @brief Verifies the maximum PAIR_REQUEST message golden vector.
 *
 * @return True on success.
 */
static bool test_pair_request_golden(void) {
  uint8_t vector[TEST_PAIR_REQUEST_GOLDEN_SIZE];
  uint8_t canonical[TEST_PAIR_REQUEST_GOLDEN_SIZE];
  MoonlightProtocolV1MessageEnvelope envelope;
  MoonlightProtocolV1PairRequest request;
  size_t vector_size = 0;
  size_t payload_size = 0;
  size_t index;

  TEST_CHECK(
    load_golden(
      "pair-request-max.hex",
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
      MOONLIGHT_PROTOCOL_V1_PAIR_REQUEST_PAYLOAD_MAX,
      &envelope
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(envelope.message_type == MOONLIGHT_PROTOCOL_V1_MESSAGE_PAIR_REQUEST);
  TEST_CHECK(envelope.flags == 0);
  TEST_CHECK(
    envelope.payload_length ==
    MOONLIGHT_PROTOCOL_V1_PAIR_REQUEST_PAYLOAD_MAX
  );
  TEST_CHECK(envelope.status == MOONLIGHT_PROTOCOL_V1_STATUS_OK);
  TEST_CHECK(envelope.correlation_id == UINT64_C(0x0102030405060708));
  TEST_RESULT(
    MoonlightProtocolV1DecodePairRequest(
      vector + MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE,
      envelope.payload_length,
      &request
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  for (index = 0; index < sizeof(request.host_id); ++index) {
    TEST_CHECK(request.host_id[index] == (uint8_t) index);
    TEST_CHECK(request.token_id[index] == (uint8_t) (index + 0x10u));
  }
  for (index = 0; index < sizeof(request.invitation_secret); ++index) {
    TEST_CHECK(
      request.invitation_secret[index] == (uint8_t) (index + 0x20u)
    );
  }
  TEST_CHECK(
    request.client_name_size == MOONLIGHT_PROTOCOL_V1_PAIR_CLIENT_NAME_MAX
  );
  for (index = 0; index < request.client_name_size; ++index) {
    TEST_CHECK(request.client_name[index] == 'A');
  }

  TEST_RESULT(
    MoonlightProtocolV1EncodeMessageEnvelope(
      &envelope,
      MOONLIGHT_PROTOCOL_V1_PAIR_REQUEST_PAYLOAD_MAX,
      canonical,
      MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodePairRequest(
      &request,
      canonical + MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE,
      MOONLIGHT_PROTOCOL_V1_PAIR_REQUEST_PAYLOAD_MAX,
      &payload_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(
    payload_size == MOONLIGHT_PROTOCOL_V1_PAIR_REQUEST_PAYLOAD_MAX
  );
  TEST_CHECK(memcmp(canonical, vector, sizeof(vector)) == 0);
  return true;
}

/**
 * @brief Verifies canonical request encoding and decoding at both name bounds.
 *
 * @return True on success.
 */
static bool test_request_round_trip_bounds(void) {
  MoonlightProtocolV1PairRequest request = valid_request();
  MoonlightProtocolV1PairRequest decoded;
  uint8_t encoded[MOONLIGHT_PROTOCOL_V1_PAIR_REQUEST_PAYLOAD_MAX];
  size_t encoded_size = 0;
  size_t index;

  memset(request.client_name, 0, sizeof(request.client_name));
  request.client_name[0] = 'A';
  request.client_name_size = 1;
  TEST_CHECK(encode_request(&request, encoded, &encoded_size));
  TEST_CHECK(encoded_size == 97u);
  TEST_RESULT(
    MoonlightProtocolV1DecodePairRequest(
      encoded,
      encoded_size,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(memcmp(decoded.host_id, request.host_id, sizeof(request.host_id)) == 0);
  TEST_CHECK(memcmp(decoded.token_id, request.token_id, sizeof(request.token_id)) == 0);
  TEST_CHECK(
    memcmp(
      decoded.invitation_secret,
      request.invitation_secret,
      sizeof(request.invitation_secret)
    ) == 0
  );
  TEST_CHECK(decoded.client_name_size == 1);
  TEST_CHECK(decoded.client_name[0] == 'A');

  for (index = 0; index < 60u; ++index) {
    request.client_name[index] = (uint8_t) ('a' + index % 26u);
  }
  request.client_name[60] = 0xf0;
  request.client_name[61] = 0x9f;
  request.client_name[62] = 0x98;
  request.client_name[63] = 0x80;
  request.client_name_size = sizeof(request.client_name);
  TEST_CHECK(encode_request(&request, encoded, &encoded_size));
  TEST_CHECK(
    encoded_size == MOONLIGHT_PROTOCOL_V1_PAIR_REQUEST_PAYLOAD_MAX
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodePairRequest(
      encoded,
      encoded_size,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(decoded.client_name_size == sizeof(request.client_name));
  TEST_CHECK(
    memcmp(
      decoded.client_name,
      request.client_name,
      sizeof(request.client_name)
    ) == 0
  );
  return true;
}

/**
 * @brief Verifies exact successful-response bytes at both ACL bounds.
 *
 * @return True on success.
 */
static bool test_response_exact_round_trip(void) {
  static const uint8_t expected_zero_acl
    [MOONLIGHT_PROTOCOL_V1_PAIR_RESPONSE_PAYLOAD_SIZE] = {
      0x00,
      0x01,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x10,
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
      0x00,
      0x02,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x10,
      0xf0,
      0xf1,
      0xf2,
      0xf3,
      0xf4,
      0xf5,
      0xf6,
      0xf7,
      0xf8,
      0xf9,
      0xfa,
      0xfb,
      0xfc,
      0xfd,
      0xfe,
      0xff,
      0x00,
      0x03,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x08,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x04,
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
      0x00,
      0x05,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x08,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x01,
      0x00,
      0x06,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x01,
      0x02,
    };
  MoonlightProtocolV1PairResponse response = valid_response();
  MoonlightProtocolV1PairResponse decoded;
  uint8_t expected_max_acl[sizeof(expected_zero_acl)];
  uint8_t encoded[sizeof(expected_zero_acl)];
  size_t encoded_size = 0;

  response.granted_acl_permission_bits = 0;
  TEST_RESULT(
    MoonlightProtocolV1EncodePairResponse(
      &response,
      encoded,
      sizeof(encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(encoded_size == sizeof(encoded));
  TEST_CHECK(memcmp(encoded, expected_zero_acl, sizeof(encoded)) == 0);
  TEST_RESULT(
    MoonlightProtocolV1DecodePairResponse(
      expected_zero_acl,
      sizeof(expected_zero_acl),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(decoded.granted_acl_permission_bits == 0);
  TEST_CHECK(
    decoded.authorization_generation == response.authorization_generation
  );
  TEST_CHECK(decoded.credential_epoch == 1);
  TEST_CHECK(
    decoded.instance_visibility ==
    MOONLIGHT_PROTOCOL_V1_INSTANCE_VISIBILITY_SHARED
  );

  memcpy(expected_max_acl, expected_zero_acl, sizeof(expected_max_acl));
  expected_max_acl[63] = (uint8_t) MOONLIGHT_PROTOCOL_V1_ACL_PERMISSION_MASK;
  response.granted_acl_permission_bits =
    MOONLIGHT_PROTOCOL_V1_ACL_PERMISSION_MASK;
  TEST_RESULT(
    MoonlightProtocolV1EncodePairResponse(
      &response,
      encoded,
      sizeof(encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(memcmp(encoded, expected_max_acl, sizeof(encoded)) == 0);
  TEST_RESULT(
    MoonlightProtocolV1DecodePairResponse(
      expected_max_acl,
      sizeof(expected_max_acl),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(
    decoded.granted_acl_permission_bits ==
    MOONLIGHT_PROTOCOL_V1_ACL_PERMISSION_MASK
  );
  TEST_CHECK(
    memcmp(
      decoded.client_principal_id,
      response.client_principal_id,
      sizeof(response.client_principal_id)
    ) == 0
  );
  TEST_CHECK(memcmp(decoded.host_id, response.host_id, sizeof(response.host_id)) == 0);
  return true;
}

/**
 * @brief Verifies strict canonical Client-name UTF-8 handling.
 *
 * @return True on success.
 */
static bool test_request_utf8_validation(void) {
  static const uint8_t valid_sequences[][4] = {
    {0x41, 0x00, 0x00, 0x00},
    {0xc2, 0xa2, 0x00, 0x00},
    {0xe0, 0xa0, 0x80, 0x00},
    {0xed, 0x9f, 0xbf, 0x00},
    {0xee, 0x80, 0x80, 0x00},
    {0xf0, 0x90, 0x80, 0x80},
    {0xf4, 0x8f, 0xbf, 0xbf},
  };
  static const size_t valid_sizes[] = {1, 2, 3, 3, 3, 4, 4};
  static const uint8_t invalid_sequences[][4] = {
    {0x00, 0x00, 0x00, 0x00},
    {0x80, 0x00, 0x00, 0x00},
    {0xc0, 0x80, 0x00, 0x00},
    {0xff, 0x00, 0x00, 0x00},
    {0xf5, 0x80, 0x80, 0x80},
    {0xc2, 0x00, 0x00, 0x00},
    {0xc2, 0x20, 0x00, 0x00},
    {0xe2, 0x82, 0x00, 0x00},
    {0xe0, 0x80, 0x80, 0x00},
    {0xed, 0xa0, 0x80, 0x00},
    {0xf0, 0x80, 0x80, 0x80},
    {0xf0, 0x9f, 0x98, 0x00},
    {0xf4, 0x90, 0x80, 0x80},
  };
  static const size_t invalid_sizes[] = {
    1,
    1,
    2,
    1,
    4,
    1,
    2,
    2,
    3,
    3,
    4,
    3,
    4,
  };
  MoonlightProtocolV1PairRequest request = valid_request();
  MoonlightProtocolV1PairRequest decoded;
  uint8_t encoded[MOONLIGHT_PROTOCOL_V1_PAIR_REQUEST_PAYLOAD_MAX];
  size_t encoded_size = 0;
  size_t index;

  for (index = 0; index < sizeof(valid_sizes) / sizeof(valid_sizes[0]); ++index) {
    memset(request.client_name, 0, sizeof(request.client_name));
    memcpy(
      request.client_name,
      valid_sequences[index],
      valid_sizes[index]
    );
    request.client_name_size = valid_sizes[index];
    TEST_CHECK(encode_request(&request, encoded, &encoded_size));
    TEST_RESULT(
      MoonlightProtocolV1DecodePairRequest(
        encoded,
        encoded_size,
        &decoded
      ),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    TEST_CHECK(decoded.client_name_size == valid_sizes[index]);
    TEST_CHECK(
      memcmp(
        decoded.client_name,
        valid_sequences[index],
        valid_sizes[index]
      ) == 0
    );
  }

  for (
    index = 0;
    index < sizeof(invalid_sizes) / sizeof(invalid_sizes[0]);
    ++index) {
    memset(request.client_name, 'A', sizeof(request.client_name));
    memcpy(
      request.client_name,
      invalid_sequences[index],
      invalid_sizes[index]
    );
    request.client_name_size = invalid_sizes[index];
    TEST_RESULT(
      MoonlightProtocolV1EncodePairRequest(
        &request,
        encoded,
        sizeof(encoded),
        &encoded_size
      ),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    );

    memset(request.client_name, 'A', sizeof(request.client_name));
    request.client_name_size = invalid_sizes[index];
    TEST_CHECK(encode_request(&request, encoded, &encoded_size));
    memcpy(
      encoded + 96u,
      invalid_sequences[index],
      invalid_sizes[index]
    );
    TEST_CHECK(
      request_decode_rejects(
        encoded,
        encoded_size,
        MOONLIGHT_PROTOCOL_RESULT_MALFORMED
      )
    );
  }
  return true;
}

/**
 * @brief Verifies request encode arguments, limits, and output atomicity.
 *
 * @return True on success.
 */
static bool test_request_encode_rejections(void) {
  MoonlightProtocolV1PairRequest request = valid_request();
  uint8_t output[MOONLIGHT_PROTOCOL_V1_PAIR_REQUEST_PAYLOAD_MAX];
  uint8_t unchanged[sizeof(output)];
  size_t encoded_size = 123;

  memset(output, 0xa5, sizeof(output));
  memcpy(unchanged, output, sizeof(output));
  TEST_RESULT(
    MoonlightProtocolV1EncodePairRequest(
      NULL,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodePairRequest(
      &request,
      NULL,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodePairRequest(
      &request,
      output,
      sizeof(output),
      NULL
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );

  request.client_name_size = 0;
  TEST_RESULT(
    MoonlightProtocolV1EncodePairRequest(
      &request,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  request.client_name_size =
    MOONLIGHT_PROTOCOL_V1_PAIR_CLIENT_NAME_MAX + 1u;
  TEST_RESULT(
    MoonlightProtocolV1EncodePairRequest(
      &request,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  request = valid_request();
  TEST_RESULT(
    MoonlightProtocolV1EncodePairRequest(
      &request,
      output,
      101u,
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL
  );
  TEST_CHECK(memcmp(output, unchanged, sizeof(output)) == 0);
  TEST_CHECK(encoded_size == 123);
  return true;
}

/**
 * @brief Verifies request field structure, limits, and decode atomicity.
 *
 * @return True on success.
 */
static bool test_request_decode_rejections(void) {
  static const size_t id_offsets[] = {0u, 24u, 48u, 88u};
  static const size_t flag_offsets[] = {2u, 26u, 50u, 90u};
  static const size_t length_offsets[] = {4u, 28u, 52u, 92u};
  static const uint16_t known_wrong_ids[] = {2u, 1u, 2u, 3u};
  MoonlightProtocolV1PairRequest request = valid_request();
  MoonlightProtocolV1PairRequest output;
  uint8_t valid[MOONLIGHT_PROTOCOL_V1_PAIR_REQUEST_PAYLOAD_MAX];
  uint8_t mutated[MOONLIGHT_PROTOCOL_V1_PAIR_REQUEST_PAYLOAD_MAX + 1u];
  size_t valid_size = 0;
  size_t index;

  memset(request.client_name, 0, sizeof(request.client_name));
  request.client_name[0] = 'A';
  request.client_name_size = 1;
  TEST_CHECK(encode_request(&request, valid, &valid_size));
  TEST_CHECK(valid_size == 97u);

  TEST_RESULT(
    MoonlightProtocolV1DecodePairRequest(NULL, valid_size, &output),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodePairRequest(valid, valid_size, NULL),
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
  for (index = 0; index < valid_size; ++index) {
    TEST_CHECK(
      request_decode_rejects(
        valid,
        index,
        MOONLIGHT_PROTOCOL_RESULT_MALFORMED
      )
    );
  }

  for (index = 0; index < sizeof(length_offsets) / sizeof(length_offsets[0]); ++index) {
    memcpy(mutated, valid, valid_size);
    test_store_u32(mutated + length_offsets[index], 0);
    TEST_CHECK(
      request_decode_rejects(
        mutated,
        valid_size,
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
  test_store_u32(mutated + length_offsets[0], UINT32_MAX);
  TEST_CHECK(
    request_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );

  for (index = 0; index < sizeof(id_offsets) / sizeof(id_offsets[0]); ++index) {
    memcpy(mutated, valid, valid_size);
    test_store_u16(mutated + id_offsets[index], known_wrong_ids[index]);
    TEST_CHECK(
      request_decode_rejects(
        mutated,
        valid_size,
        MOONLIGHT_PROTOCOL_RESULT_MALFORMED
      )
    );

    memcpy(mutated, valid, valid_size);
    test_store_u16(mutated + id_offsets[index], 5u);
    TEST_CHECK(
      request_decode_rejects(
        mutated,
        valid_size,
        MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
      )
    );

    memcpy(mutated, valid, valid_size);
    test_store_u16(
      mutated + flag_offsets[index],
      MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED
    );
    TEST_CHECK(
      request_decode_rejects(
        mutated,
        valid_size,
        MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
      )
    );
  }
  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated + flag_offsets[0], 2u);
  TEST_CHECK(
    request_decode_rejects(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );

  memcpy(mutated, valid, valid_size);
  mutated[valid_size] = 0;
  TEST_CHECK(
    request_decode_rejects(
      mutated,
      valid_size + 1u,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  memcpy(mutated, valid, valid_size);
  memset(mutated + valid_size, 0, MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE);
  test_store_u16(mutated + valid_size, 5u);
  TEST_CHECK(
    request_decode_rejects(
      mutated,
      valid_size + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );
  test_store_u16(mutated + valid_size + 2u, 2u);
  TEST_CHECK(
    request_decode_rejects(
      mutated,
      valid_size + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );
  test_store_u16(mutated + valid_size + 2u, 0);
  test_store_u16(mutated + valid_size, 0);
  TEST_CHECK(
    request_decode_rejects(
      mutated,
      valid_size + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );
  test_store_u16(mutated + valid_size, 4u);
  TEST_CHECK(
    request_decode_rejects(
      mutated,
      valid_size + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  test_store_u16(mutated + valid_size, 5u);
  test_store_u32(mutated + valid_size + 4u, 1u);
  TEST_CHECK(
    request_decode_rejects(
      mutated,
      valid_size + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  return true;
}

/**
 * @brief Verifies response encode arguments, values, and output atomicity.
 *
 * @return True on success.
 */
static bool test_response_encode_rejections(void) {
  MoonlightProtocolV1PairResponse response = valid_response();
  uint8_t output[MOONLIGHT_PROTOCOL_V1_PAIR_RESPONSE_PAYLOAD_SIZE];
  uint8_t unchanged[sizeof(output)];
  size_t encoded_size = 321;

  memset(output, 0xa5, sizeof(output));
  memcpy(unchanged, output, sizeof(output));
  TEST_RESULT(
    MoonlightProtocolV1EncodePairResponse(
      NULL,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodePairResponse(
      &response,
      NULL,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodePairResponse(
      &response,
      output,
      sizeof(output),
      NULL
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );

  response.granted_acl_permission_bits =
    MOONLIGHT_PROTOCOL_V1_ACL_PERMISSION_MASK | UINT64_C(0x80);
  TEST_RESULT(
    MoonlightProtocolV1EncodePairResponse(
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
    MoonlightProtocolV1EncodePairResponse(
      &response,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  response = valid_response();
  response.credential_epoch = 0;
  TEST_RESULT(
    MoonlightProtocolV1EncodePairResponse(
      &response,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  response.credential_epoch = 2;
  TEST_RESULT(
    MoonlightProtocolV1EncodePairResponse(
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
    MoonlightProtocolV1EncodePairResponse(
      &response,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  response.instance_visibility = (MoonlightProtocolV1InstanceVisibility) 3;
  TEST_RESULT(
    MoonlightProtocolV1EncodePairResponse(
      &response,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  response = valid_response();
  TEST_RESULT(
    MoonlightProtocolV1EncodePairResponse(
      &response,
      output,
      sizeof(output) - 1u,
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL
  );
  TEST_CHECK(memcmp(output, unchanged, sizeof(output)) == 0);
  TEST_CHECK(encoded_size == 321);
  return true;
}

/**
 * @brief Verifies response field structure and decode atomicity.
 *
 * @return True on success.
 */
static bool test_response_decode_structure(void) {
  static const size_t id_offsets[] = {0u, 24u, 48u, 64u, 80u, 96u};
  static const size_t flag_offsets[] = {2u, 26u, 50u, 66u, 82u, 98u};
  static const size_t length_offsets[] = {4u, 28u, 52u, 68u, 84u, 100u};
  static const uint16_t known_wrong_ids[] = {2u, 1u, 2u, 3u, 4u, 5u};
  MoonlightProtocolV1PairResponse output;
  uint8_t valid[MOONLIGHT_PROTOCOL_V1_PAIR_RESPONSE_PAYLOAD_SIZE];
  uint8_t mutated[MOONLIGHT_PROTOCOL_V1_PAIR_RESPONSE_PAYLOAD_SIZE + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE];
  size_t index;

  TEST_CHECK(encode_valid_response(valid));
  TEST_RESULT(
    MoonlightProtocolV1DecodePairResponse(NULL, sizeof(valid), &output),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodePairResponse(valid, sizeof(valid), NULL),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  for (index = 0; index < sizeof(valid); ++index) {
    TEST_CHECK(
      response_decode_rejects(
        valid,
        index,
        MOONLIGHT_PROTOCOL_RESULT_MALFORMED
      )
    );
  }

  for (index = 0; index < sizeof(length_offsets) / sizeof(length_offsets[0]); ++index) {
    memcpy(mutated, valid, sizeof(valid));
    test_store_u32(mutated + length_offsets[index], 0);
    TEST_CHECK(
      response_decode_rejects(
        mutated,
        sizeof(valid),
        MOONLIGHT_PROTOCOL_RESULT_MALFORMED
      )
    );
  }
  memcpy(mutated, valid, sizeof(valid));
  test_store_u32(mutated + length_offsets[0], UINT32_MAX);
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      sizeof(valid),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );

  for (index = 0; index < sizeof(id_offsets) / sizeof(id_offsets[0]); ++index) {
    memcpy(mutated, valid, sizeof(valid));
    test_store_u16(mutated + id_offsets[index], known_wrong_ids[index]);
    TEST_CHECK(
      response_decode_rejects(
        mutated,
        sizeof(valid),
        MOONLIGHT_PROTOCOL_RESULT_MALFORMED
      )
    );

    memcpy(mutated, valid, sizeof(valid));
    test_store_u16(mutated + id_offsets[index], 7u);
    TEST_CHECK(
      response_decode_rejects(
        mutated,
        sizeof(valid),
        MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
      )
    );

    memcpy(mutated, valid, sizeof(valid));
    test_store_u16(
      mutated + flag_offsets[index],
      MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED
    );
    TEST_CHECK(
      response_decode_rejects(
        mutated,
        sizeof(valid),
        MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
      )
    );
  }
  memcpy(mutated, valid, sizeof(valid));
  test_store_u16(mutated + flag_offsets[0], 2u);
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      sizeof(valid),
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );

  memcpy(mutated, valid, sizeof(valid));
  mutated[sizeof(valid)] = 0;
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      sizeof(valid) + 1u,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  memcpy(mutated, valid, sizeof(valid));
  memset(mutated + sizeof(valid), 0, MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE);
  test_store_u16(mutated + sizeof(valid), 7u);
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      sizeof(mutated),
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );
  test_store_u16(mutated + sizeof(valid), 6u);
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      sizeof(mutated),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  test_store_u16(mutated + sizeof(valid), 7u);
  test_store_u32(mutated + sizeof(valid) + 4u, 1u);
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      sizeof(mutated),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  return true;
}

/**
 * @brief Verifies response ACL, generation, epoch, and visibility ranges.
 *
 * @return True on success.
 */
static bool test_response_decode_values(void) {
  uint8_t valid[MOONLIGHT_PROTOCOL_V1_PAIR_RESPONSE_PAYLOAD_SIZE];
  uint8_t mutated[sizeof(valid)];
  MoonlightProtocolV1PairResponse decoded;

  TEST_CHECK(encode_valid_response(valid));

  memcpy(mutated, valid, sizeof(mutated));
  memset(mutated + 56u, 0, 8u);
  TEST_RESULT(
    MoonlightProtocolV1DecodePairResponse(
      mutated,
      sizeof(mutated),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(decoded.granted_acl_permission_bits == 0);

  memcpy(mutated, valid, sizeof(mutated));
  memset(mutated + 56u, 0, 8u);
  mutated[63] = 0x80;
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      sizeof(mutated),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  memcpy(mutated, valid, sizeof(mutated));
  memset(mutated + 72u, 0, 8u);
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      sizeof(mutated),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  memcpy(mutated, valid, sizeof(mutated));
  memset(mutated + 88u, 0, 8u);
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      sizeof(mutated),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  memcpy(mutated, valid, sizeof(mutated));
  mutated[95] = 2;
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      sizeof(mutated),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  memcpy(mutated, valid, sizeof(mutated));
  mutated[104] = 0;
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      sizeof(mutated),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  mutated[104] = 3;
  TEST_CHECK(
    response_decode_rejects(
      mutated,
      sizeof(mutated),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );

  memcpy(mutated, valid, sizeof(mutated));
  memset(mutated + 72u, 0xff, 8u);
  mutated[104] = MOONLIGHT_PROTOCOL_V1_INSTANCE_VISIBILITY_OWNER_ONLY;
  TEST_RESULT(
    MoonlightProtocolV1DecodePairResponse(
      mutated,
      sizeof(mutated),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(decoded.authorization_generation == UINT64_MAX);
  TEST_CHECK(
    decoded.instance_visibility ==
    MOONLIGHT_PROTOCOL_V1_INSTANCE_VISIBILITY_OWNER_ONLY
  );
  return true;
}

/**
 * @brief Executes all Pairing schema tests.
 *
 * @return Zero only when every test succeeds.
 */
int main(void) {
  static const struct {
    const char *name;  ///< Human-readable test name.
    bool (*run)(void);  ///< Test implementation.
  } tests[] = {
    {"PAIR_REQUEST golden", test_pair_request_golden},
    {"request round-trip bounds", test_request_round_trip_bounds},
    {"response exact round-trip", test_response_exact_round_trip},
    {"request UTF-8 validation", test_request_utf8_validation},
    {"request encode rejections", test_request_encode_rejections},
    {"request decode rejections", test_request_decode_rejections},
    {"response encode rejections", test_response_encode_rejections},
    {"response decode structure", test_response_decode_structure},
    {"response decode values", test_response_decode_values},
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
