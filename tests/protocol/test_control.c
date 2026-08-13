/**
 * @file test_control.c
 * @brief Native tests for protocol version 1 Control Lane schemas.
 */

#include <moonlight/protocol/control.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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
 * @brief Builds one valid CLIENT_HELLO request.
 *
 * @return Valid request with a three-byte software version.
 */
static MoonlightProtocolV1ClientHelloRequest valid_request(void) {
  MoonlightProtocolV1ClientHelloRequest request = {
    .protocol_minor = MOONLIGHT_PROTOCOL_V1_MINOR,
    .capability_bits = MOONLIGHT_PROTOCOL_V1_CAPABILITY_MASK,
    .software_version = {'1', '.', '0'},
    .software_version_size = 3,
  };

  return request;
}

/**
 * @brief Builds one valid CLIENT_HELLO response.
 *
 * @return Valid response at both protocol payload maxima.
 */
static MoonlightProtocolV1ClientHelloResponse valid_response(void) {
  MoonlightProtocolV1ClientHelloResponse response = {
    .protocol_minor = MOONLIGHT_PROTOCOL_V1_MINOR,
    .capability_bits = MOONLIGHT_PROTOCOL_V1_CAPABILITY_MASK,
    .host_id = {
      0x00,
      0x11,
      0x22,
      0x33,
      0x44,
      0x55,
      0x66,
      0x77,
      0x88,
      0x99,
      0xaa,
      0xbb,
      0xcc,
      0xdd,
      0xee,
      0xff,
    },
    .maximum_control_payload = MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX,
    .maximum_bulk_payload = MOONLIGHT_PROTOCOL_V1_BULK_PAYLOAD_MAX,
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
 * @brief Encodes one valid request for mutation tests.
 *
 * @param output Destination with maximum request capacity.
 * @param output_size Receives encoded size.
 * @return True on success.
 */
static bool encode_valid_request(uint8_t *output, size_t *output_size) {
  const MoonlightProtocolV1ClientHelloRequest request = valid_request();

  TEST_RESULT(
    MoonlightProtocolV1EncodeClientHelloRequest(
      &request,
      output,
      MOONLIGHT_PROTOCOL_V1_CLIENT_HELLO_REQUEST_PAYLOAD_MAX,
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
  const MoonlightProtocolV1ClientHelloResponse response = valid_response();
  size_t output_size = 0;

  TEST_RESULT(
    MoonlightProtocolV1EncodeClientHelloResponse(
      &response,
      output,
      MOONLIGHT_PROTOCOL_V1_CLIENT_HELLO_RESPONSE_PAYLOAD_SIZE,
      &output_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(
    output_size == MOONLIGHT_PROTOCOL_V1_CLIENT_HELLO_RESPONSE_PAYLOAD_SIZE
  );
  return true;
}

/**
 * @brief Verifies canonical request encoding and decoding at both length bounds.
 *
 * @return True on success.
 */
static bool test_request_round_trip_bounds(void) {
  MoonlightProtocolV1ClientHelloRequest request = valid_request();
  MoonlightProtocolV1ClientHelloRequest decoded;
  uint8_t encoded[MOONLIGHT_PROTOCOL_V1_CLIENT_HELLO_REQUEST_PAYLOAD_MAX];
  size_t encoded_size = 0;
  size_t index;

  TEST_CHECK(MOONLIGHT_PROTOCOL_V1_MINOR == 1u);
  TEST_CHECK(
    MOONLIGHT_PROTOCOL_V1_CAPABILITY_AUDIO_DATAGRAM == UINT64_C(0x40)
  );
  TEST_CHECK(MOONLIGHT_PROTOCOL_V1_CAPABILITY_MASK == UINT64_C(0x3ff));

  request.software_version_size = 1;
  TEST_RESULT(
    MoonlightProtocolV1EncodeClientHelloRequest(
      &request,
      encoded,
      sizeof(encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(encoded_size == 35u);
  TEST_CHECK(encoded[0] == 0 && encoded[1] == 1);
  TEST_CHECK(encoded[4] == 0 && encoded[7] == 2);
  TEST_CHECK(encoded[8] == 0 && encoded[9] == 1);
  TEST_CHECK(encoded[10] == 0 && encoded[11] == 2);
  TEST_CHECK(encoded[14] == 0 && encoded[17] == 8);
  TEST_CHECK(encoded[18] == 0 && encoded[24] == 0x03 && encoded[25] == 0xff);
  TEST_CHECK(encoded[26] == 0 && encoded[27] == 3);
  TEST_CHECK(encoded[30] == 0 && encoded[33] == 1);
  TEST_CHECK(encoded[34] == '1');
  TEST_RESULT(
    MoonlightProtocolV1DecodeClientHelloRequest(
      encoded,
      encoded_size,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(decoded.protocol_minor == request.protocol_minor);
  TEST_CHECK(decoded.capability_bits == request.capability_bits);
  TEST_CHECK(decoded.software_version_size == 1);
  TEST_CHECK(decoded.software_version[0] == '1');

  for (index = 0; index < sizeof(request.software_version); ++index) {
    request.software_version[index] = (uint8_t) ('a' + index % 26u);
  }
  request.software_version_size = sizeof(request.software_version);
  TEST_RESULT(
    MoonlightProtocolV1EncodeClientHelloRequest(
      &request,
      encoded,
      sizeof(encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(
    encoded_size == MOONLIGHT_PROTOCOL_V1_CLIENT_HELLO_REQUEST_PAYLOAD_MAX
  );
  TEST_CHECK(
    MOONLIGHT_PROTOCOL_V1_EARLY_HELLO_SIZE_MAX == 138u
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeClientHelloRequest(
      encoded,
      encoded_size,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(
    decoded.software_version_size == sizeof(request.software_version)
  );
  TEST_CHECK(
    memcmp(
      decoded.software_version,
      request.software_version,
      sizeof(request.software_version)
    ) == 0
  );
  return true;
}

/**
 * @brief Verifies canonical response encoding and decoding.
 *
 * @return True on success.
 */
static bool test_response_round_trip(void) {
  const MoonlightProtocolV1ClientHelloResponse response = valid_response();
  MoonlightProtocolV1ClientHelloResponse decoded;
  uint8_t encoded[MOONLIGHT_PROTOCOL_V1_CLIENT_HELLO_RESPONSE_PAYLOAD_SIZE];
  size_t encoded_size = 0;

  TEST_RESULT(
    MoonlightProtocolV1EncodeClientHelloResponse(
      &response,
      encoded,
      sizeof(encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(encoded_size == sizeof(encoded));
  TEST_CHECK(encoded[0] == 0 && encoded[1] == 1 && encoded[7] == 2);
  TEST_CHECK(encoded[8] == 0 && encoded[9] == 1);
  TEST_CHECK(encoded[10] == 0 && encoded[11] == 2 && encoded[17] == 8);
  TEST_CHECK(encoded[18] == 0 && encoded[25] == 0xff);
  TEST_CHECK(encoded[26] == 0 && encoded[27] == 3 && encoded[33] == 16);
  TEST_CHECK(encoded[50] == 0 && encoded[51] == 4 && encoded[57] == 4);
  TEST_CHECK(encoded[58] == 0x00 && encoded[59] == 0x01);
  TEST_CHECK(encoded[60] == 0x00 && encoded[61] == 0x00);
  TEST_CHECK(encoded[62] == 0 && encoded[63] == 5 && encoded[69] == 4);
  TEST_CHECK(encoded[70] == 0x01 && encoded[73] == 0x00);
  TEST_RESULT(
    MoonlightProtocolV1DecodeClientHelloResponse(
      encoded,
      sizeof(encoded),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(decoded.protocol_minor == response.protocol_minor);
  TEST_CHECK(decoded.capability_bits == response.capability_bits);
  TEST_CHECK(memcmp(decoded.host_id, response.host_id, sizeof(response.host_id)) == 0);
  TEST_CHECK(
    decoded.maximum_control_payload == response.maximum_control_payload
  );
  TEST_CHECK(decoded.maximum_bulk_payload == response.maximum_bulk_payload);
  return true;
}

/**
 * @brief Verifies strict canonical UTF-8 handling.
 *
 * @return True on success.
 */
static bool test_request_utf8_validation(void) {
  static const uint8_t valid_sequences[][4] = {
    {0xc2, 0xa2, 0x00, 0x00},
    {0xe2, 0x82, 0xac, 0x00},
    {0xf0, 0x9f, 0x98, 0x80},
  };
  static const size_t valid_sizes[] = {2, 3, 4};
  static const uint8_t invalid_sequences[][4] = {
    {0x00, 0x00, 0x00, 0x00},
    {0x80, 0x00, 0x00, 0x00},
    {0xc0, 0x80, 0x00, 0x00},
    {0xf5, 0x80, 0x80, 0x80},
    {0xc2, 0x00, 0x00, 0x00},
    {0xc2, 0x20, 0x00, 0x00},
    {0xe0, 0x80, 0x80, 0x00},
    {0xed, 0xa0, 0x80, 0x00},
    {0xf4, 0x90, 0x80, 0x80},
  };
  static const size_t invalid_sizes[] = {1, 1, 2, 4, 1, 2, 3, 3, 4};
  MoonlightProtocolV1ClientHelloRequest request = valid_request();
  uint8_t encoded[MOONLIGHT_PROTOCOL_V1_CLIENT_HELLO_REQUEST_PAYLOAD_MAX];
  size_t encoded_size = 0;
  size_t index;

  for (index = 0; index < sizeof(valid_sizes) / sizeof(valid_sizes[0]); ++index) {
    memset(request.software_version, 0, sizeof(request.software_version));
    memcpy(
      request.software_version,
      valid_sequences[index],
      valid_sizes[index]
    );
    request.software_version_size = valid_sizes[index];
    TEST_RESULT(
      MoonlightProtocolV1EncodeClientHelloRequest(
        &request,
        encoded,
        sizeof(encoded),
        &encoded_size
      ),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
  }

  for (index = 0; index < sizeof(invalid_sizes) / sizeof(invalid_sizes[0]); ++index) {
    memset(request.software_version, 0, sizeof(request.software_version));
    memcpy(
      request.software_version,
      invalid_sequences[index],
      invalid_sizes[index]
    );
    request.software_version_size = invalid_sizes[index];
    TEST_RESULT(
      MoonlightProtocolV1EncodeClientHelloRequest(
        &request,
        encoded,
        sizeof(encoded),
        &encoded_size
      ),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    );
  }
  return true;
}

/**
 * @brief Verifies request argument, state, and output atomicity checks.
 *
 * @return True on success.
 */
static bool test_request_encode_rejections(void) {
  MoonlightProtocolV1ClientHelloRequest request = valid_request();
  uint8_t output[MOONLIGHT_PROTOCOL_V1_CLIENT_HELLO_REQUEST_PAYLOAD_MAX];
  uint8_t unchanged[sizeof(output)];
  size_t encoded_size = 123;

  memset(output, 0xa5, sizeof(output));
  memcpy(unchanged, output, sizeof(output));
  TEST_RESULT(
    MoonlightProtocolV1EncodeClientHelloRequest(
      NULL,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeClientHelloRequest(
      &request,
      NULL,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeClientHelloRequest(
      &request,
      output,
      sizeof(output),
      NULL
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );

  request.protocol_minor = 0;
  TEST_RESULT(
    MoonlightProtocolV1EncodeClientHelloRequest(
      &request,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
  );
  request = valid_request();
  request.capability_bits = UINT64_C(0x400);  // Undefined capability bit (bit 10).
  TEST_RESULT(
    MoonlightProtocolV1EncodeClientHelloRequest(
      &request,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
  );
  request = valid_request();
  request.software_version_size = 0;
  TEST_RESULT(
    MoonlightProtocolV1EncodeClientHelloRequest(
      &request,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  request.software_version_size =
    MOONLIGHT_PROTOCOL_V1_SOFTWARE_VERSION_MAX + 1u;
  TEST_RESULT(
    MoonlightProtocolV1EncodeClientHelloRequest(
      &request,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  request = valid_request();
  TEST_RESULT(
    MoonlightProtocolV1EncodeClientHelloRequest(
      &request,
      output,
      1,
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL
  );
  TEST_CHECK(memcmp(output, unchanged, sizeof(output)) == 0);
  TEST_CHECK(encoded_size == 123);
  return true;
}

/**
 * @brief Verifies malformed and unsupported request payload classification.
 *
 * @return True on success.
 */
static bool test_request_decode_rejections(void) {
  uint8_t valid[MOONLIGHT_PROTOCOL_V1_CLIENT_HELLO_REQUEST_PAYLOAD_MAX];
  uint8_t mutated[sizeof(valid) + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE];
  MoonlightProtocolV1ClientHelloRequest output;
  MoonlightProtocolV1ClientHelloRequest unchanged;
  size_t valid_size = 0;

  TEST_CHECK(encode_valid_request(valid, &valid_size));
  memset(&output, 0xa5, sizeof(output));
  unchanged = output;
  TEST_RESULT(
    MoonlightProtocolV1DecodeClientHelloRequest(NULL, valid_size, &output),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeClientHelloRequest(valid, valid_size, NULL),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeClientHelloRequest(valid, 1, &output),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memset(mutated, 0, sizeof(mutated));
  TEST_RESULT(
    MoonlightProtocolV1DecodeClientHelloRequest(
      mutated,
      sizeof(mutated),
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_LIMIT_EXCEEDED
  );

  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated, 4);
  TEST_RESULT(
    MoonlightProtocolV1DecodeClientHelloRequest(
      mutated,
      valid_size,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated, 2);
  TEST_RESULT(
    MoonlightProtocolV1DecodeClientHelloRequest(
      mutated,
      valid_size,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated, 0);
  TEST_RESULT(
    MoonlightProtocolV1DecodeClientHelloRequest(
      mutated,
      valid_size,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated + 10u, 1);
  TEST_RESULT(
    MoonlightProtocolV1DecodeClientHelloRequest(
      mutated,
      valid_size,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated + 26u, 4);
  TEST_RESULT(
    MoonlightProtocolV1DecodeClientHelloRequest(
      mutated,
      valid_size,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated + 2u, MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED);
  TEST_RESULT(
    MoonlightProtocolV1DecodeClientHelloRequest(
      mutated,
      valid_size,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated + 2u, 2);
  TEST_RESULT(
    MoonlightProtocolV1DecodeClientHelloRequest(
      mutated,
      valid_size,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  memcpy(mutated, valid, valid_size);
  test_store_u32(mutated + 4u, (uint32_t) valid_size);
  TEST_RESULT(
    MoonlightProtocolV1DecodeClientHelloRequest(
      mutated,
      valid_size,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );

  memcpy(mutated, valid, valid_size);
  test_store_u32(mutated + 4u, 1);
  TEST_RESULT(
    MoonlightProtocolV1DecodeClientHelloRequest(
      mutated,
      valid_size,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, valid, valid_size);
  test_store_u32(mutated + 14u, 7);
  TEST_RESULT(
    MoonlightProtocolV1DecodeClientHelloRequest(
      mutated,
      valid_size,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, valid, valid_size);
  test_store_u32(mutated + 30u, 0);
  TEST_RESULT(
    MoonlightProtocolV1DecodeClientHelloRequest(
      mutated,
      valid_size,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );

  memcpy(mutated, valid, valid_size);
  mutated[9] = 0;
  TEST_RESULT(
    MoonlightProtocolV1DecodeClientHelloRequest(
      mutated,
      valid_size,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
  );
  memcpy(mutated, valid, valid_size);
  mutated[18] = 0x01;  // Undefined capability bit (bit 8).
  TEST_RESULT(
    MoonlightProtocolV1DecodeClientHelloRequest(
      mutated,
      valid_size,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
  );
  memcpy(mutated, valid, valid_size);
  mutated[34] = 0x80;
  TEST_RESULT(
    MoonlightProtocolV1DecodeClientHelloRequest(
      mutated,
      valid_size,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, valid, valid_size);
  mutated[valid_size] = 0;
  TEST_RESULT(
    MoonlightProtocolV1DecodeClientHelloRequest(
      mutated,
      valid_size + 1u,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, valid, valid_size);
  memset(mutated + valid_size, 0, MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE);
  test_store_u16(mutated + valid_size, 4);
  TEST_RESULT(
    MoonlightProtocolV1DecodeClientHelloRequest(
      mutated,
      valid_size + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  test_store_u16(mutated + valid_size + 2u, 2);
  TEST_RESULT(
    MoonlightProtocolV1DecodeClientHelloRequest(
      mutated,
      valid_size + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  memcpy(mutated, valid, valid_size);
  memset(mutated + valid_size, 0, MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE);
  TEST_RESULT(
    MoonlightProtocolV1DecodeClientHelloRequest(
      mutated,
      valid_size + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  memcpy(mutated, valid, valid_size);
  memset(mutated + valid_size, 0, MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE);
  test_store_u16(mutated + valid_size, 4);
  test_store_u32(mutated + valid_size + 4u, 1);
  TEST_RESULT(
    MoonlightProtocolV1DecodeClientHelloRequest(
      mutated,
      valid_size + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, valid, valid_size);
  memset(mutated + valid_size, 0, MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE);
  test_store_u16(mutated + valid_size, 3);
  TEST_RESULT(
    MoonlightProtocolV1DecodeClientHelloRequest(
      mutated,
      valid_size + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  TEST_CHECK(memcmp(&output, &unchanged, sizeof(output)) == 0);
  return true;
}

/**
 * @brief Verifies response argument, state, and output atomicity checks.
 *
 * @return True on success.
 */
static bool test_response_encode_rejections(void) {
  MoonlightProtocolV1ClientHelloResponse response = valid_response();
  uint8_t output[MOONLIGHT_PROTOCOL_V1_CLIENT_HELLO_RESPONSE_PAYLOAD_SIZE];
  uint8_t unchanged[sizeof(output)];
  size_t encoded_size = 321;

  memset(output, 0xa5, sizeof(output));
  memcpy(unchanged, output, sizeof(output));
  TEST_RESULT(
    MoonlightProtocolV1EncodeClientHelloResponse(
      NULL,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeClientHelloResponse(
      &response,
      NULL,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeClientHelloResponse(
      &response,
      output,
      sizeof(output),
      NULL
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );

  response.protocol_minor = 0;
  TEST_RESULT(
    MoonlightProtocolV1EncodeClientHelloResponse(
      &response,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
  );
  response = valid_response();
  response.capability_bits = UINT64_C(0x400);  // Undefined capability bit (bit 10).
  TEST_RESULT(
    MoonlightProtocolV1EncodeClientHelloResponse(
      &response,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
  );
  response = valid_response();
  response.maximum_control_payload = 0;
  TEST_RESULT(
    MoonlightProtocolV1EncodeClientHelloResponse(
      &response,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  response.maximum_control_payload =
    MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX + 1u;
  TEST_RESULT(
    MoonlightProtocolV1EncodeClientHelloResponse(
      &response,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  response = valid_response();
  response.maximum_bulk_payload = 0;
  TEST_RESULT(
    MoonlightProtocolV1EncodeClientHelloResponse(
      &response,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  response.maximum_bulk_payload = MOONLIGHT_PROTOCOL_V1_BULK_PAYLOAD_MAX + 1u;
  TEST_RESULT(
    MoonlightProtocolV1EncodeClientHelloResponse(
      &response,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  response = valid_response();
  TEST_RESULT(
    MoonlightProtocolV1EncodeClientHelloResponse(
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
 * @brief Verifies malformed response payload classification.
 *
 * @return True on success.
 */
static bool test_response_decode_rejections(void) {
  uint8_t valid[MOONLIGHT_PROTOCOL_V1_CLIENT_HELLO_RESPONSE_PAYLOAD_SIZE];
  uint8_t mutated[sizeof(valid) + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE];
  MoonlightProtocolV1ClientHelloResponse output;
  MoonlightProtocolV1ClientHelloResponse unchanged;
  static const size_t length_offsets[] = {4, 14, 30, 54, 66};
  static const uint32_t wrong_lengths[] = {1, 7, 15, 3, 3};
  size_t index;

  TEST_CHECK(encode_valid_response(valid));
  memset(&output, 0xa5, sizeof(output));
  unchanged = output;
  TEST_RESULT(
    MoonlightProtocolV1DecodeClientHelloResponse(
      NULL,
      sizeof(valid),
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeClientHelloResponse(
      valid,
      sizeof(valid),
      NULL
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeClientHelloResponse(valid, 1, &output),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, valid, sizeof(valid));
  mutated[sizeof(valid)] = 0;
  TEST_RESULT(
    MoonlightProtocolV1DecodeClientHelloResponse(
      mutated,
      sizeof(valid) + 1u,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, valid, sizeof(valid));
  memset(
    mutated + sizeof(valid),
    0,
    MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE
  );
  test_store_u16(mutated + sizeof(valid), 6);
  TEST_RESULT(
    MoonlightProtocolV1DecodeClientHelloResponse(
      mutated,
      sizeof(mutated),
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  memcpy(mutated, valid, sizeof(valid));
  memset(
    mutated + sizeof(valid),
    0,
    MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE
  );
  test_store_u16(mutated + sizeof(valid), 5);
  TEST_RESULT(
    MoonlightProtocolV1DecodeClientHelloResponse(
      mutated,
      sizeof(mutated),
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );

  for (index = 0; index < sizeof(length_offsets) / sizeof(length_offsets[0]); ++index) {
    memcpy(mutated, valid, sizeof(valid));
    test_store_u32(
      mutated + length_offsets[index],
      wrong_lengths[index]
    );
    TEST_RESULT(
      MoonlightProtocolV1DecodeClientHelloResponse(
        mutated,
        sizeof(valid),
        &output
      ),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    );
  }

  memcpy(mutated, valid, sizeof(valid));
  test_store_u16(mutated, 6);
  TEST_RESULT(
    MoonlightProtocolV1DecodeClientHelloResponse(
      mutated,
      sizeof(valid),
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  memcpy(mutated, valid, sizeof(valid));
  test_store_u16(mutated, 2);
  TEST_RESULT(
    MoonlightProtocolV1DecodeClientHelloResponse(
      mutated,
      sizeof(valid),
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, valid, sizeof(valid));
  mutated[9] = 0;
  TEST_RESULT(
    MoonlightProtocolV1DecodeClientHelloResponse(
      mutated,
      sizeof(valid),
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
  );
  memcpy(mutated, valid, sizeof(valid));
  mutated[18] = 0x01;  // Undefined capability bit (bit 8).
  TEST_RESULT(
    MoonlightProtocolV1DecodeClientHelloResponse(
      mutated,
      sizeof(valid),
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
  );
  memcpy(mutated, valid, sizeof(valid));
  memset(mutated + 58u, 0, 4);
  TEST_RESULT(
    MoonlightProtocolV1DecodeClientHelloResponse(
      mutated,
      sizeof(valid),
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, valid, sizeof(valid));
  memset(mutated + 70u, 0, 4);
  TEST_RESULT(
    MoonlightProtocolV1DecodeClientHelloResponse(
      mutated,
      sizeof(valid),
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  TEST_CHECK(memcmp(&output, &unchanged, sizeof(output)) == 0);
  return true;
}

/**
 * @brief Verifies canonical PING payloads at the absent and token bounds.
 *
 * @return True on success.
 */
static bool test_ping_round_trip_bounds(void) {
  MoonlightProtocolV1PingPayload payload = {0};
  MoonlightProtocolV1PingPayload decoded;
  uint8_t encoded[MOONLIGHT_PROTOCOL_V1_PING_PAYLOAD_MAX];
  size_t encoded_size = 123;
  size_t index;

  TEST_RESULT(
    MoonlightProtocolV1EncodePingPayload(
      &payload,
      NULL,
      0,
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(encoded_size == 0);
  memset(&decoded, 0xa5, sizeof(decoded));
  TEST_RESULT(
    MoonlightProtocolV1DecodePingPayload(NULL, 0, &decoded),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(decoded.token_size == 0);

  payload.token[0] = 0x7e;
  payload.token_size = 1;
  TEST_RESULT(
    MoonlightProtocolV1EncodePingPayload(
      &payload,
      encoded,
      sizeof(encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(encoded_size == MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + 1u);
  TEST_CHECK(encoded[0] == 0 && encoded[1] == 1);
  TEST_CHECK(encoded[2] == 0 && encoded[3] == 0);
  TEST_CHECK(encoded[4] == 0 && encoded[7] == 1);
  TEST_CHECK(encoded[8] == 0x7e);
  TEST_RESULT(
    MoonlightProtocolV1DecodePingPayload(
      encoded,
      encoded_size,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(decoded.token_size == 1);
  TEST_CHECK(decoded.token[0] == 0x7e);

  for (index = 0; index < sizeof(payload.token); ++index) {
    payload.token[index] = (uint8_t) index;
  }
  payload.token_size = sizeof(payload.token);
  TEST_RESULT(
    MoonlightProtocolV1EncodePingPayload(
      &payload,
      encoded,
      sizeof(encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(encoded_size == sizeof(encoded));
  TEST_RESULT(
    MoonlightProtocolV1DecodePingPayload(
      encoded,
      encoded_size,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(decoded.token_size == sizeof(payload.token));
  TEST_CHECK(memcmp(decoded.token, payload.token, sizeof(payload.token)) == 0);
  return true;
}

/**
 * @brief Verifies PING argument, bound, schema, and failure atomicity checks.
 *
 * @return True on success.
 */
static bool test_ping_rejections(void) {
  MoonlightProtocolV1PingPayload payload = {
    .token = {0x42},
    .token_size = 1,
  };
  MoonlightProtocolV1PingPayload decoded;
  MoonlightProtocolV1PingPayload unchanged_decoded;
  uint8_t valid[MOONLIGHT_PROTOCOL_V1_PING_PAYLOAD_MAX];
  uint8_t mutated[MOONLIGHT_PROTOCOL_V1_PING_PAYLOAD_MAX + 1u];
  uint8_t output[MOONLIGHT_PROTOCOL_V1_PING_PAYLOAD_MAX];
  uint8_t unchanged_output[sizeof(output)];
  size_t valid_size = 0;
  size_t encoded_size = 77;

  memset(output, 0xa5, sizeof(output));
  memcpy(unchanged_output, output, sizeof(output));
  TEST_RESULT(
    MoonlightProtocolV1EncodePingPayload(
      NULL,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodePingPayload(
      &payload,
      output,
      sizeof(output),
      NULL
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodePingPayload(
      &payload,
      NULL,
      0,
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  payload.token_size = MOONLIGHT_PROTOCOL_V1_PING_TOKEN_MAX + 1u;
  TEST_RESULT(
    MoonlightProtocolV1EncodePingPayload(
      &payload,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  payload.token_size = 1;
  TEST_RESULT(
    MoonlightProtocolV1EncodePingPayload(
      &payload,
      output,
      MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL
  );
  TEST_CHECK(memcmp(output, unchanged_output, sizeof(output)) == 0);
  TEST_CHECK(encoded_size == 77);

  TEST_RESULT(
    MoonlightProtocolV1EncodePingPayload(
      &payload,
      valid,
      sizeof(valid),
      &valid_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  memset(&decoded, 0xa5, sizeof(decoded));
  unchanged_decoded = decoded;
  TEST_RESULT(
    MoonlightProtocolV1DecodePingPayload(NULL, 1, &decoded),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodePingPayload(valid, valid_size, NULL),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  memset(mutated, 0, sizeof(mutated));
  TEST_RESULT(
    MoonlightProtocolV1DecodePingPayload(
      mutated,
      sizeof(mutated),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_LIMIT_EXCEEDED
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodePingPayload(valid, 1, &decoded),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );

  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated, 2);
  TEST_RESULT(
    MoonlightProtocolV1DecodePingPayload(
      mutated,
      valid_size,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated, 0);
  TEST_RESULT(
    MoonlightProtocolV1DecodePingPayload(
      mutated,
      valid_size,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated + 2u, MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED);
  TEST_RESULT(
    MoonlightProtocolV1DecodePingPayload(
      mutated,
      valid_size,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  memcpy(mutated, valid, valid_size);
  test_store_u32(mutated + 4u, 0);
  TEST_RESULT(
    MoonlightProtocolV1DecodePingPayload(
      mutated,
      MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, valid, valid_size);
  test_store_u32(mutated + 4u, 2);
  TEST_RESULT(
    MoonlightProtocolV1DecodePingPayload(
      mutated,
      valid_size,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );

  memcpy(mutated, valid, valid_size);
  mutated[valid_size] = 0;
  TEST_RESULT(
    MoonlightProtocolV1DecodePingPayload(
      mutated,
      valid_size + 1u,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, valid, valid_size);
  memset(
    mutated + valid_size,
    0,
    MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE
  );
  test_store_u16(mutated + valid_size, 1);
  TEST_RESULT(
    MoonlightProtocolV1DecodePingPayload(
      mutated,
      valid_size + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  test_store_u16(mutated + valid_size, 2);
  TEST_RESULT(
    MoonlightProtocolV1DecodePingPayload(
      mutated,
      valid_size + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  test_store_u32(mutated + valid_size + 4u, 1);
  TEST_RESULT(
    MoonlightProtocolV1DecodePingPayload(
      mutated,
      valid_size + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  TEST_CHECK(memcmp(&decoded, &unchanged_decoded, sizeof(decoded)) == 0);
  return true;
}

/**
 * @brief Executes all Control schema tests.
 *
 * @return Zero only when every test succeeds.
 */
int main(void) {
  static const struct {
    const char *name;  ///< Human-readable test name.
    bool (*run)(void);  ///< Test implementation.
  } tests[] = {
    {"request round-trip bounds", test_request_round_trip_bounds},
    {"response round-trip", test_response_round_trip},
    {"request UTF-8 validation", test_request_utf8_validation},
    {"request encode rejections", test_request_encode_rejections},
    {"request decode rejections", test_request_decode_rejections},
    {"response encode rejections", test_response_encode_rejections},
    {"response decode rejections", test_response_decode_rejections},
    {"PING round-trip bounds", test_ping_round_trip_bounds},
    {"PING rejections", test_ping_rejections},
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
