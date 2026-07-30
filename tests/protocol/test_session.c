/**
 * @file test_session.c
 * @brief Native tests for protocol version 1 direct-display Stream Sessions.
 */

#include <moonlight/protocol/session.h>
#include <stdbool.h>
#include <stddef.h>
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
 * @brief Canonical one-codec START_SESSION request golden.
 */
static const uint8_t start_request_golden[] = {
  0x00,
  0x01,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x10,
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
  0x10,
  0x00,
  0x02,
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
  0x03,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x02,
  0x07,
  0x80,
  0x00,
  0x04,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x02,
  0x04,
  0x38,
  0x00,
  0x05,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x04,
  0x00,
  0x00,
  0xea,
  0x60,
  0x00,
  0x06,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x04,
  0x00,
  0x00,
  0x03,
  0xe9,
  0x00,
  0x07,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x04,
  0x00,
  0x00,
  0x4e,
  0x20,
  0x00,
  0x08,
  0x00,
  0x01,
  0x00,
  0x00,
  0x00,
  0x02,
  0x00,
  0x01,
  0x00,
  0x09,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x01,
  0x00,
  0x00,
  0x0a,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x01,
  0x00,
};

/**
 * @brief Canonical successful START_SESSION response golden.
 */
static const uint8_t start_response_golden[] = {
  0x00,
  0x01,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x10,
  0x10,
  0x0f,
  0x0e,
  0x0d,
  0x0c,
  0x0b,
  0x0a,
  0x09,
  0x08,
  0x07,
  0x06,
  0x05,
  0x04,
  0x03,
  0x02,
  0x01,
  0x00,
  0x02,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x04,
  0x11,
  0x22,
  0x33,
  0x44,
  0x00,
  0x03,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x04,
  0x00,
  0x00,
  0x00,
  0x01,
  0x00,
  0x04,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x02,
  0x04,
  0xb0,
  0x00,
  0x05,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x02,
  0x04,
  0x80,
  0x00,
  0x06,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x8e,
  0x00,
  0x01,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x02,
  0x00,
  0x01,
  0x00,
  0x02,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x02,
  0x00,
  0x64,
  0x00,
  0x03,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x01,
  0x08,
  0x00,
  0x04,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x01,
  0x00,
  0x00,
  0x05,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x01,
  0x00,
  0x00,
  0x06,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x02,
  0x07,
  0x80,
  0x00,
  0x07,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x02,
  0x04,
  0x38,
  0x00,
  0x08,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x04,
  0x00,
  0x00,
  0xea,
  0x60,
  0x00,
  0x09,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x04,
  0x00,
  0x00,
  0x03,
  0xe9,
  0x00,
  0x0a,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x04,
  0x00,
  0x00,
  0x3a,
  0x98,
  0x00,
  0x0b,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x01,
  0x05,
  0x00,
  0x0c,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x04,
  0x00,
  0x00,
  0x00,
  0x01,
  0x00,
  0x0d,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x01,
  0x00,
  0x00,
  0x0e,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x01,
  0x28,
};

/**
 * @brief Canonical SESSION_READY request golden.
 */
static const uint8_t ready_request_golden[] = {
  0x00,
  0x01,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x10,
  0x10,
  0x0f,
  0x0e,
  0x0d,
  0x0c,
  0x0b,
  0x0a,
  0x09,
  0x08,
  0x07,
  0x06,
  0x05,
  0x04,
  0x03,
  0x02,
  0x01,
  0x00,
  0x02,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x04,
  0x11,
  0x22,
  0x33,
  0x44,
  0x00,
  0x03,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x04,
  0x00,
  0x00,
  0x00,
  0x01,
  0x00,
  0x04,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x02,
  0x04,
  0xb0,
  0x00,
  0x05,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x02,
  0x04,
  0x80,
  0x00,
  0x06,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x04,
  0x00,
  0x80,
  0x00,
  0x00,
};

/**
 * @brief Builds the request represented by `start_request_golden`.
 *
 * @return Valid one-codec request.
 */
static MoonlightProtocolV1StartSessionRequest valid_start_request(void) {
  MoonlightProtocolV1StartSessionRequest request = {
    .display_id = {
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
      0x10,
    },
    .catalog_revision = UINT64_C(0x0102030405060708),
    .width = 1920,
    .height = 1080,
    .frame_rate_numerator = 60000,
    .frame_rate_denominator = 1001,
    .bitrate_kbps = 20000,
    .codec_preferences = {MOONLIGHT_PROTOCOL_V1_VIDEO_CODEC_H264},
    .codec_preference_count = 1,
    .dynamic_range = MOONLIGHT_PROTOCOL_V1_VIDEO_DYNAMIC_RANGE_SDR,
    .chroma = MOONLIGHT_PROTOCOL_V1_VIDEO_CHROMA_420,
  };

  return request;
}

/**
 * @brief Builds the response represented by `start_response_golden`.
 *
 * @return Valid successful response.
 */
static MoonlightProtocolV1StartSessionResponse valid_start_response(void) {
  MoonlightProtocolV1StartSessionResponse response = {
    .session_id = {
      0x10,
      0x0f,
      0x0e,
      0x0d,
      0x0c,
      0x0b,
      0x0a,
      0x09,
      0x08,
      0x07,
      0x06,
      0x05,
      0x04,
      0x03,
      0x02,
      0x01,
    },
    .session_wire_id = UINT32_C(0x11223344),
    .media_epoch = MOONLIGHT_PROTOCOL_V1_INITIAL_MEDIA_EPOCH,
    .maximum_complete_datagram = 1200,
    .maximum_video_shard = 1152,
    .video = {
      .codec = MOONLIGHT_PROTOCOL_V1_VIDEO_CODEC_H264,
      .profile = 100,
      .bit_depth = 8,
      .chroma = MOONLIGHT_PROTOCOL_V1_VIDEO_CHROMA_420,
      .dynamic_range = MOONLIGHT_PROTOCOL_V1_VIDEO_DYNAMIC_RANGE_SDR,
      .width = 1920,
      .height = 1080,
      .frame_rate_numerator = 60000,
      .frame_rate_denominator = 1001,
      .bitrate_kbps = 15000,
      .initial_fec_percentage = 5,
      .codec_configuration_generation = 1,
      .minimum_fec_percentage = 0,
      .maximum_fec_percentage = 40,
    },
  };

  return response;
}

/**
 * @brief Builds the request represented by `ready_request_golden`.
 *
 * @return Valid readiness request.
 */
static MoonlightProtocolV1SessionReadyRequest valid_ready_request(void) {
  MoonlightProtocolV1SessionReadyRequest request = {
    .session_id = {
      0x10,
      0x0f,
      0x0e,
      0x0d,
      0x0c,
      0x0b,
      0x0a,
      0x09,
      0x08,
      0x07,
      0x06,
      0x05,
      0x04,
      0x03,
      0x02,
      0x01,
    },
    .session_wire_id = UINT32_C(0x11223344),
    .media_epoch = MOONLIGHT_PROTOCOL_V1_INITIAL_MEDIA_EPOCH,
    .complete_datagram = 1200,
    .video_shard = 1152,
    .maximum_video_access_unit_bytes = 8u * 1024u * 1024u,
  };

  return request;
}

/**
 * @brief Builds one valid IDR-only refresh request.
 *
 * @return Valid active-session refresh request.
 */
static MoonlightProtocolV1RequestIdrRequest valid_request_idr_request(void) {
  const MoonlightProtocolV1SessionReadyRequest ready = valid_ready_request();
  MoonlightProtocolV1RequestIdrRequest request = {
    .highest_complete_video_frame = 17u,
    .preferred_repair = MOONLIGHT_PROTOCOL_V1_VIDEO_REPAIR_IDR,
  };

  memcpy(request.session_id, ready.session_id, sizeof(request.session_id));
  return request;
}

/**
 * @brief Loads a network-order 16-bit integer from test bytes.
 *
 * @param input Two readable bytes.
 * @return Host-order value.
 */
static uint16_t test_load_u16(const uint8_t *input) {
  return (uint16_t) (((uint16_t) input[0] << 8u) | input[1]);
}

/**
 * @brief Loads a network-order 32-bit integer from test bytes.
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
 * @brief Stores a network-order 16-bit integer in test bytes.
 *
 * @param output Two writable bytes.
 * @param value Host-order value.
 */
static void test_store_u16(uint8_t *output, uint16_t value) {
  output[0] = (uint8_t) (value >> 8u);
  output[1] = (uint8_t) value;
}

/**
 * @brief Stores a network-order 32-bit integer in test bytes.
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
 * @brief Locates one field occurrence in a complete test payload.
 *
 * @param input Complete payload.
 * @param input_size Number of payload bytes.
 * @param field_id Field ID to locate.
 * @param occurrence Zero-based occurrence among matching fields.
 * @param field_offset Receives the field-header offset.
 * @param value_offset Receives the field-value offset.
 * @param value_size Receives the field-value size.
 * @return True only when the requested complete field exists.
 */
static bool find_field(
  const uint8_t *input,
  size_t input_size,
  uint16_t field_id,
  size_t occurrence,
  size_t *field_offset,
  size_t *value_offset,
  size_t *value_size
) {
  size_t offset = 0;

  while (input_size - offset >= MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE) {
    const uint16_t current_id = test_load_u16(input + offset);
    const uint32_t current_size = test_load_u32(input + offset + 4u);
    const size_t next =
      offset + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + current_size;

    if (next > input_size) {
      return false;
    }
    if (current_id == field_id) {
      if (occurrence == 0) {
        *field_offset = offset;
        *value_offset = offset + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE;
        *value_size = current_size;
        return true;
      }
      --occurrence;
    }
    offset = next;
  }
  return false;
}

/**
 * @brief Encodes one valid START_SESSION request.
 *
 * @param request Valid request.
 * @param output Maximum-size destination.
 * @param output_size Receives the encoded size.
 * @return True on success.
 */
static bool encode_start_request(
  const MoonlightProtocolV1StartSessionRequest *request,
  uint8_t *output,
  size_t *output_size
) {
  TEST_RESULT(
    MoonlightProtocolV1EncodeStartSessionRequest(
      request,
      output,
      MOONLIGHT_PROTOCOL_V1_START_SESSION_REQUEST_PAYLOAD_MAX,
      output_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  return true;
}

/**
 * @brief Encodes one valid START_SESSION response.
 *
 * @param response Valid response.
 * @param output Exact-size destination.
 * @param output_size Receives the encoded size.
 * @return True on success.
 */
static bool encode_start_response(
  const MoonlightProtocolV1StartSessionResponse *response,
  uint8_t *output,
  size_t *output_size
) {
  TEST_RESULT(
    MoonlightProtocolV1EncodeStartSessionResponse(
      response,
      output,
      MOONLIGHT_PROTOCOL_V1_START_SESSION_RESPONSE_PAYLOAD_SIZE,
      output_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  return true;
}

/**
 * @brief Encodes one valid SESSION_READY request.
 *
 * @param request Valid request.
 * @param output Exact-size destination.
 * @param output_size Receives the encoded size.
 * @return True on success.
 */
static bool encode_ready_request(
  const MoonlightProtocolV1SessionReadyRequest *request,
  uint8_t *output,
  size_t *output_size
) {
  TEST_RESULT(
    MoonlightProtocolV1EncodeSessionReadyRequest(
      request,
      output,
      MOONLIGHT_PROTOCOL_V1_SESSION_READY_REQUEST_PAYLOAD_SIZE,
      output_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  return true;
}

/**
 * @brief Verifies START_SESSION request decode rejection and output atomicity.
 *
 * @param input Candidate payload.
 * @param input_size Candidate payload size.
 * @param expected Expected codec result.
 * @return True on success.
 */
static bool start_request_decode_rejects(
  const uint8_t *input,
  size_t input_size,
  MoonlightProtocolResult expected
) {
  MoonlightProtocolV1StartSessionRequest output;
  MoonlightProtocolV1StartSessionRequest unchanged;

  memset(&output, 0xa5, sizeof(output));
  unchanged = output;
  TEST_RESULT(
    MoonlightProtocolV1DecodeStartSessionRequest(
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
 * @brief Verifies START_SESSION response decode rejection and output atomicity.
 *
 * @param input Candidate payload.
 * @param input_size Candidate payload size.
 * @param expected Expected codec result.
 * @return True on success.
 */
static bool start_response_decode_rejects(
  const uint8_t *input,
  size_t input_size,
  MoonlightProtocolResult expected
) {
  MoonlightProtocolV1StartSessionResponse output;
  MoonlightProtocolV1StartSessionResponse unchanged;

  memset(&output, 0xa5, sizeof(output));
  unchanged = output;
  TEST_RESULT(
    MoonlightProtocolV1DecodeStartSessionResponse(
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
 * @brief Verifies SESSION_READY decode rejection and output atomicity.
 *
 * @param input Candidate payload.
 * @param input_size Candidate payload size.
 * @param expected Expected codec result.
 * @return True on success.
 */
static bool ready_request_decode_rejects(
  const uint8_t *input,
  size_t input_size,
  MoonlightProtocolResult expected
) {
  MoonlightProtocolV1SessionReadyRequest output;
  MoonlightProtocolV1SessionReadyRequest unchanged;

  memset(&output, 0xa5, sizeof(output));
  unchanged = output;
  TEST_RESULT(
    MoonlightProtocolV1DecodeSessionReadyRequest(
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
 * @brief Verifies the three canonical payload goldens in both directions.
 *
 * @return True on success.
 */
static bool test_goldens(void) {
  const MoonlightProtocolV1StartSessionRequest request = valid_start_request();
  const MoonlightProtocolV1StartSessionResponse response =
    valid_start_response();
  const MoonlightProtocolV1SessionReadyRequest ready = valid_ready_request();
  MoonlightProtocolV1StartSessionRequest decoded_request;
  MoonlightProtocolV1StartSessionResponse decoded_response;
  MoonlightProtocolV1SessionReadyRequest decoded_ready;
  uint8_t encoded_response[sizeof(start_response_golden)];
  uint8_t encoded_request[MOONLIGHT_PROTOCOL_V1_START_SESSION_REQUEST_PAYLOAD_MAX];
  uint8_t encoded_ready[sizeof(ready_request_golden)];
  size_t encoded_size = 0;

  TEST_CHECK(
    sizeof(start_request_golden) ==
    MOONLIGHT_PROTOCOL_V1_START_SESSION_REQUEST_PAYLOAD_MIN
  );
  TEST_CHECK(
    sizeof(start_response_golden) ==
    MOONLIGHT_PROTOCOL_V1_START_SESSION_RESPONSE_PAYLOAD_SIZE
  );
  TEST_CHECK(
    sizeof(ready_request_golden) ==
    MOONLIGHT_PROTOCOL_V1_SESSION_READY_REQUEST_PAYLOAD_SIZE
  );

  TEST_CHECK(encode_start_request(&request, encoded_request, &encoded_size));
  TEST_CHECK(encoded_size == sizeof(start_request_golden));
  TEST_CHECK(
    memcmp(encoded_request, start_request_golden, encoded_size) == 0
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeStartSessionRequest(
      start_request_golden,
      sizeof(start_request_golden),
      &decoded_request
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(decoded_request.catalog_revision == request.catalog_revision);
  TEST_CHECK(decoded_request.width == request.width);
  TEST_CHECK(decoded_request.height == request.height);
  TEST_CHECK(
    decoded_request.frame_rate_numerator == request.frame_rate_numerator
  );
  TEST_CHECK(
    decoded_request.frame_rate_denominator == request.frame_rate_denominator
  );
  TEST_CHECK(decoded_request.bitrate_kbps == request.bitrate_kbps);
  TEST_CHECK(decoded_request.codec_preference_count == 1u);
  TEST_CHECK(
    decoded_request.codec_preferences[0] ==
    MOONLIGHT_PROTOCOL_V1_VIDEO_CODEC_H264
  );

  TEST_CHECK(
    encode_start_response(&response, encoded_response, &encoded_size)
  );
  TEST_CHECK(encoded_size == sizeof(start_response_golden));
  TEST_CHECK(
    memcmp(encoded_response, start_response_golden, encoded_size) == 0
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeStartSessionResponse(
      start_response_golden,
      sizeof(start_response_golden),
      &decoded_response
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(decoded_response.session_wire_id == response.session_wire_id);
  TEST_CHECK(decoded_response.media_epoch == response.media_epoch);
  TEST_CHECK(
    decoded_response.maximum_complete_datagram ==
    response.maximum_complete_datagram
  );
  TEST_CHECK(
    decoded_response.maximum_video_shard == response.maximum_video_shard
  );
  TEST_CHECK(decoded_response.video.codec == response.video.codec);
  TEST_CHECK(decoded_response.video.profile == response.video.profile);
  TEST_CHECK(decoded_response.video.bit_depth == response.video.bit_depth);
  TEST_CHECK(decoded_response.video.width == response.video.width);
  TEST_CHECK(decoded_response.video.height == response.video.height);
  TEST_CHECK(
    decoded_response.video.codec_configuration_generation ==
    response.video.codec_configuration_generation
  );

  TEST_CHECK(encode_ready_request(&ready, encoded_ready, &encoded_size));
  TEST_CHECK(encoded_size == sizeof(ready_request_golden));
  TEST_CHECK(memcmp(encoded_ready, ready_request_golden, encoded_size) == 0);
  TEST_RESULT(
    MoonlightProtocolV1DecodeSessionReadyRequest(
      ready_request_golden,
      sizeof(ready_request_golden),
      &decoded_ready
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(decoded_ready.session_wire_id == ready.session_wire_id);
  TEST_CHECK(decoded_ready.media_epoch == ready.media_epoch);
  TEST_CHECK(decoded_ready.complete_datagram == ready.complete_datagram);
  TEST_CHECK(decoded_ready.video_shard == ready.video_shard);
  TEST_CHECK(
    decoded_ready.maximum_video_access_unit_bytes ==
    ready.maximum_video_access_unit_bytes
  );
  return true;
}

/**
 * @brief Verifies valid boundary values and codec preference ordering.
 *
 * @return True on success.
 */
static bool test_round_trip_bounds(void) {
  MoonlightProtocolV1StartSessionRequest request = valid_start_request();
  MoonlightProtocolV1StartSessionRequest decoded_request;
  MoonlightProtocolV1StartSessionResponse response = valid_start_response();
  MoonlightProtocolV1StartSessionResponse decoded_response;
  MoonlightProtocolV1SessionReadyRequest ready = valid_ready_request();
  MoonlightProtocolV1SessionReadyRequest decoded_ready;
  uint8_t start_encoded[MOONLIGHT_PROTOCOL_V1_START_SESSION_REQUEST_PAYLOAD_MAX];
  uint8_t response_encoded[MOONLIGHT_PROTOCOL_V1_START_SESSION_RESPONSE_PAYLOAD_SIZE];
  uint8_t ready_encoded[MOONLIGHT_PROTOCOL_V1_SESSION_READY_REQUEST_PAYLOAD_SIZE];
  size_t encoded_size = 0;

  request.width = MOONLIGHT_PROTOCOL_V1_VIDEO_WIDTH_MIN;
  request.height = MOONLIGHT_PROTOCOL_V1_VIDEO_HEIGHT_MIN;
  request.frame_rate_numerator = UINT32_MAX;
  request.frame_rate_denominator = UINT32_MAX - 1u;
  request.bitrate_kbps = UINT32_MAX;
  request.codec_preferences[0] = MOONLIGHT_PROTOCOL_V1_VIDEO_CODEC_AV1;
  request.codec_preferences[1] = MOONLIGHT_PROTOCOL_V1_VIDEO_CODEC_H264;
  request.codec_preferences[2] = MOONLIGHT_PROTOCOL_V1_VIDEO_CODEC_HEVC;
  request.codec_preference_count = 3;
  request.dynamic_range = MOONLIGHT_PROTOCOL_V1_VIDEO_DYNAMIC_RANGE_HDR10;
  request.chroma = MOONLIGHT_PROTOCOL_V1_VIDEO_CHROMA_444;
  TEST_CHECK(encode_start_request(&request, start_encoded, &encoded_size));
  TEST_CHECK(
    encoded_size == MOONLIGHT_PROTOCOL_V1_START_SESSION_REQUEST_PAYLOAD_MAX
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeStartSessionRequest(
      start_encoded,
      encoded_size,
      &decoded_request
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(decoded_request.codec_preference_count == 3u);
  TEST_CHECK(
    decoded_request.codec_preferences[0] ==
    MOONLIGHT_PROTOCOL_V1_VIDEO_CODEC_AV1
  );
  TEST_CHECK(
    decoded_request.codec_preferences[1] ==
    MOONLIGHT_PROTOCOL_V1_VIDEO_CODEC_H264
  );
  TEST_CHECK(
    decoded_request.codec_preferences[2] ==
    MOONLIGHT_PROTOCOL_V1_VIDEO_CODEC_HEVC
  );

  response.maximum_complete_datagram = UINT16_MAX;
  response.maximum_video_shard =
    MOONLIGHT_PROTOCOL_V1_SESSION_VIDEO_SHARD_MAX;
  response.video.codec = MOONLIGHT_PROTOCOL_V1_VIDEO_CODEC_AV1;
  response.video.profile = UINT16_MAX;
  response.video.bit_depth = 10;
  response.video.chroma = MOONLIGHT_PROTOCOL_V1_VIDEO_CHROMA_444;
  response.video.dynamic_range =
    MOONLIGHT_PROTOCOL_V1_VIDEO_DYNAMIC_RANGE_HDR10;
  response.video.width = MOONLIGHT_PROTOCOL_V1_VIDEO_WIDTH_MAX;
  response.video.height = MOONLIGHT_PROTOCOL_V1_VIDEO_HEIGHT_MAX;
  response.video.frame_rate_numerator = UINT32_MAX;
  response.video.frame_rate_denominator = UINT32_MAX - 1u;
  response.video.bitrate_kbps = UINT32_MAX;
  response.video.initial_fec_percentage =
    MOONLIGHT_PROTOCOL_V1_VIDEO_INITIAL_FEC_MAX;
  response.video.codec_configuration_generation = UINT32_MAX;
  response.video.minimum_fec_percentage =
    MOONLIGHT_PROTOCOL_V1_VIDEO_INITIAL_FEC_MAX;
  response.video.maximum_fec_percentage =
    MOONLIGHT_PROTOCOL_V1_VIDEO_INITIAL_FEC_MAX;
  TEST_CHECK(
    encode_start_response(&response, response_encoded, &encoded_size)
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeStartSessionResponse(
      response_encoded,
      encoded_size,
      &decoded_response
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(
    decoded_response.maximum_video_shard ==
    MOONLIGHT_PROTOCOL_V1_SESSION_VIDEO_SHARD_MAX
  );
  TEST_CHECK(decoded_response.video.bit_depth == 10u);
  TEST_CHECK(
    decoded_response.video.maximum_fec_percentage ==
    MOONLIGHT_PROTOCOL_V1_VIDEO_INITIAL_FEC_MAX
  );

  ready.complete_datagram = MOONLIGHT_PROTOCOL_V1_SESSION_DATAGRAM_MIN;
  ready.video_shard = MOONLIGHT_PROTOCOL_V1_SESSION_VIDEO_SHARD_MIN;
  ready.maximum_video_access_unit_bytes =
    MOONLIGHT_PROTOCOL_V1_VIDEO_ACCESS_UNIT_MIN;
  TEST_CHECK(encode_ready_request(&ready, ready_encoded, &encoded_size));
  TEST_RESULT(
    MoonlightProtocolV1DecodeSessionReadyRequest(
      ready_encoded,
      encoded_size,
      &decoded_ready
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(
    decoded_ready.complete_datagram ==
    MOONLIGHT_PROTOCOL_V1_SESSION_DATAGRAM_MIN
  );
  TEST_CHECK(
    decoded_ready.video_shard ==
    MOONLIGHT_PROTOCOL_V1_SESSION_VIDEO_SHARD_MIN
  );
  TEST_CHECK(
    decoded_ready.maximum_video_access_unit_bytes ==
    MOONLIGHT_PROTOCOL_V1_VIDEO_ACCESS_UNIT_MIN
  );
  return true;
}

/**
 * @brief Verifies START_SESSION request encode validation and atomicity.
 *
 * @return True on success.
 */
static bool test_start_request_encode_rejections(void) {
  MoonlightProtocolV1StartSessionRequest request = valid_start_request();
  uint8_t output[MOONLIGHT_PROTOCOL_V1_START_SESSION_REQUEST_PAYLOAD_MAX];
  uint8_t unchanged[sizeof(output)];
  size_t encoded_size = 777;

  memset(output, 0xa5, sizeof(output));
  memcpy(unchanged, output, sizeof(output));
  TEST_RESULT(
    MoonlightProtocolV1EncodeStartSessionRequest(
      NULL,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeStartSessionRequest(
      &request,
      NULL,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeStartSessionRequest(
      &request,
      output,
      sizeof(output),
      NULL
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeStartSessionRequest(
      &request,
      output,
      sizeof(start_request_golden) - 1u,
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL
  );
  TEST_CHECK(encoded_size == 777u);
  TEST_CHECK(memcmp(output, unchanged, sizeof(output)) == 0);

#define REJECT_START_REQUEST(member, value, expected) \
  do { \
    request = valid_start_request(); \
    request.member = (value); \
    TEST_RESULT( \
      MoonlightProtocolV1EncodeStartSessionRequest( \
        &request, \
        output, \
        sizeof(output), \
        &encoded_size \
      ), \
      (expected) \
    ); \
    TEST_CHECK(encoded_size == 777u); \
    TEST_CHECK(memcmp(output, unchanged, sizeof(output)) == 0); \
  } while (0)

  memset(request.display_id, 0, sizeof(request.display_id));
  TEST_RESULT(
    MoonlightProtocolV1EncodeStartSessionRequest(
      &request,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  TEST_CHECK(memcmp(output, unchanged, sizeof(output)) == 0);
  REJECT_START_REQUEST(
    catalog_revision,
    0,
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  REJECT_START_REQUEST(width, 319, MOONLIGHT_PROTOCOL_RESULT_MALFORMED);
  REJECT_START_REQUEST(width, 16385, MOONLIGHT_PROTOCOL_RESULT_MALFORMED);
  REJECT_START_REQUEST(height, 239, MOONLIGHT_PROTOCOL_RESULT_MALFORMED);
  REJECT_START_REQUEST(height, 16385, MOONLIGHT_PROTOCOL_RESULT_MALFORMED);
  REJECT_START_REQUEST(
    frame_rate_numerator,
    0,
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  REJECT_START_REQUEST(
    frame_rate_denominator,
    0,
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  request = valid_start_request();
  request.frame_rate_numerator = 120;
  request.frame_rate_denominator = 2;
  TEST_RESULT(
    MoonlightProtocolV1EncodeStartSessionRequest(
      &request,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  REJECT_START_REQUEST(bitrate_kbps, 0, MOONLIGHT_PROTOCOL_RESULT_MALFORMED);
  REJECT_START_REQUEST(
    codec_preference_count,
    0,
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  REJECT_START_REQUEST(
    codec_preference_count,
    4,
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  request = valid_start_request();
  request.codec_preferences[0] = (MoonlightProtocolV1VideoCodec) 0;
  TEST_RESULT(
    MoonlightProtocolV1EncodeStartSessionRequest(
      &request,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  request.codec_preferences[0] = (MoonlightProtocolV1VideoCodec) 4;
  TEST_RESULT(
    MoonlightProtocolV1EncodeStartSessionRequest(
      &request,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  request = valid_start_request();
  request.codec_preferences[1] = request.codec_preferences[0];
  request.codec_preference_count = 2;
  TEST_RESULT(
    MoonlightProtocolV1EncodeStartSessionRequest(
      &request,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  REJECT_START_REQUEST(
    dynamic_range,
    (MoonlightProtocolV1VideoDynamicRange) 2,
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  REJECT_START_REQUEST(
    chroma,
    (MoonlightProtocolV1VideoChroma) 2,
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );

#undef REJECT_START_REQUEST

  TEST_CHECK(encoded_size == 777u);
  TEST_CHECK(memcmp(output, unchanged, sizeof(output)) == 0);
  return true;
}

/**
 * @brief Verifies START_SESSION response encode validation and atomicity.
 *
 * @return True on success.
 */
static bool test_start_response_encode_rejections(void) {
  MoonlightProtocolV1StartSessionResponse response = valid_start_response();
  uint8_t output[MOONLIGHT_PROTOCOL_V1_START_SESSION_RESPONSE_PAYLOAD_SIZE];
  uint8_t unchanged[sizeof(output)];
  size_t encoded_size = 777;

  memset(output, 0xa5, sizeof(output));
  memcpy(unchanged, output, sizeof(output));
  TEST_RESULT(
    MoonlightProtocolV1EncodeStartSessionResponse(
      NULL,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeStartSessionResponse(
      &response,
      NULL,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeStartSessionResponse(
      &response,
      output,
      sizeof(output),
      NULL
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeStartSessionResponse(
      &response,
      output,
      sizeof(output) - 1u,
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL
  );
  TEST_CHECK(encoded_size == 777u);
  TEST_CHECK(memcmp(output, unchanged, sizeof(output)) == 0);

#define REJECT_START_RESPONSE(member, value, expected) \
  do { \
    response = valid_start_response(); \
    response.member = (value); \
    TEST_RESULT( \
      MoonlightProtocolV1EncodeStartSessionResponse( \
        &response, \
        output, \
        sizeof(output), \
        &encoded_size \
      ), \
      (expected) \
    ); \
    TEST_CHECK(encoded_size == 777u); \
    TEST_CHECK(memcmp(output, unchanged, sizeof(output)) == 0); \
  } while (0)

  memset(response.session_id, 0, sizeof(response.session_id));
  TEST_RESULT(
    MoonlightProtocolV1EncodeStartSessionResponse(
      &response,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  REJECT_START_RESPONSE(
    session_wire_id,
    0,
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  REJECT_START_RESPONSE(media_epoch, 0, MOONLIGHT_PROTOCOL_RESULT_MALFORMED);
  REJECT_START_RESPONSE(media_epoch, 2, MOONLIGHT_PROTOCOL_RESULT_MALFORMED);
  REJECT_START_RESPONSE(
    maximum_complete_datagram,
    551,
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  REJECT_START_RESPONSE(
    maximum_video_shard,
    496,
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  REJECT_START_RESPONSE(
    maximum_video_shard,
    65520,
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  REJECT_START_RESPONSE(
    maximum_video_shard,
    513,
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  response = valid_start_response();
  response.maximum_complete_datagram = 552;
  response.maximum_video_shard = 528;
  TEST_RESULT(
    MoonlightProtocolV1EncodeStartSessionResponse(
      &response,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );

#define REJECT_VIDEO(member, value, expected) \
  do { \
    response = valid_start_response(); \
    response.video.member = (value); \
    TEST_RESULT( \
      MoonlightProtocolV1EncodeStartSessionResponse( \
        &response, \
        output, \
        sizeof(output), \
        &encoded_size \
      ), \
      (expected) \
    ); \
  } while (0)

  REJECT_VIDEO(
    codec,
    (MoonlightProtocolV1VideoCodec) 0,
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  REJECT_VIDEO(
    codec,
    (MoonlightProtocolV1VideoCodec) 4,
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  REJECT_VIDEO(bit_depth, 9, MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED);
  REJECT_VIDEO(
    chroma,
    (MoonlightProtocolV1VideoChroma) 2,
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  REJECT_VIDEO(
    dynamic_range,
    (MoonlightProtocolV1VideoDynamicRange) 2,
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  REJECT_VIDEO(width, 319, MOONLIGHT_PROTOCOL_RESULT_MALFORMED);
  REJECT_VIDEO(width, 16385, MOONLIGHT_PROTOCOL_RESULT_MALFORMED);
  REJECT_VIDEO(height, 239, MOONLIGHT_PROTOCOL_RESULT_MALFORMED);
  REJECT_VIDEO(height, 16385, MOONLIGHT_PROTOCOL_RESULT_MALFORMED);
  REJECT_VIDEO(
    frame_rate_numerator,
    0,
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  REJECT_VIDEO(
    frame_rate_denominator,
    0,
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  response = valid_start_response();
  response.video.frame_rate_numerator = 120;
  response.video.frame_rate_denominator = 2;
  TEST_RESULT(
    MoonlightProtocolV1EncodeStartSessionResponse(
      &response,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  REJECT_VIDEO(bitrate_kbps, 0, MOONLIGHT_PROTOCOL_RESULT_MALFORMED);
  REJECT_VIDEO(
    codec_configuration_generation,
    0,
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  REJECT_VIDEO(
    maximum_fec_percentage,
    41,
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  response = valid_start_response();
  response.video.minimum_fec_percentage = 6;
  response.video.initial_fec_percentage = 5;
  TEST_RESULT(
    MoonlightProtocolV1EncodeStartSessionResponse(
      &response,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  response = valid_start_response();
  response.video.initial_fec_percentage = 6;
  response.video.maximum_fec_percentage = 5;
  TEST_RESULT(
    MoonlightProtocolV1EncodeStartSessionResponse(
      &response,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  response = valid_start_response();
  response.video.initial_fec_percentage = 26;
  response.video.maximum_fec_percentage = 40;
  TEST_RESULT(
    MoonlightProtocolV1EncodeStartSessionResponse(
      &response,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );

#undef REJECT_VIDEO
#undef REJECT_START_RESPONSE

  TEST_CHECK(encoded_size == 777u);
  TEST_CHECK(memcmp(output, unchanged, sizeof(output)) == 0);
  return true;
}

/**
 * @brief Verifies SESSION_READY encode validation and atomicity.
 *
 * @return True on success.
 */
static bool test_ready_encode_rejections(void) {
  MoonlightProtocolV1SessionReadyRequest request = valid_ready_request();
  uint8_t output[MOONLIGHT_PROTOCOL_V1_SESSION_READY_REQUEST_PAYLOAD_SIZE];
  uint8_t unchanged[sizeof(output)];
  size_t encoded_size = 777;

  memset(output, 0xa5, sizeof(output));
  memcpy(unchanged, output, sizeof(output));
  TEST_RESULT(
    MoonlightProtocolV1EncodeSessionReadyRequest(
      NULL,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeSessionReadyRequest(
      &request,
      NULL,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeSessionReadyRequest(
      &request,
      output,
      sizeof(output),
      NULL
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeSessionReadyRequest(
      &request,
      output,
      sizeof(output) - 1u,
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL
  );
  TEST_CHECK(encoded_size == 777u);
  TEST_CHECK(memcmp(output, unchanged, sizeof(output)) == 0);

  memset(request.session_id, 0, sizeof(request.session_id));
  TEST_RESULT(
    MoonlightProtocolV1EncodeSessionReadyRequest(
      &request,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  request = valid_ready_request();
  request.session_wire_id = 0;
  TEST_RESULT(
    MoonlightProtocolV1EncodeSessionReadyRequest(
      &request,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  request = valid_ready_request();
  request.media_epoch = 2;
  TEST_RESULT(
    MoonlightProtocolV1EncodeSessionReadyRequest(
      &request,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  request = valid_ready_request();
  request.complete_datagram = 552;
  request.video_shard = 528;
  TEST_RESULT(
    MoonlightProtocolV1EncodeSessionReadyRequest(
      &request,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  request = valid_ready_request();
  request.maximum_video_access_unit_bytes =
    MOONLIGHT_PROTOCOL_V1_VIDEO_ACCESS_UNIT_MIN - 1u;
  TEST_RESULT(
    MoonlightProtocolV1EncodeSessionReadyRequest(
      &request,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  request.maximum_video_access_unit_bytes =
    MOONLIGHT_PROTOCOL_V1_VIDEO_ACCESS_UNIT_MAX + 1u;
  TEST_RESULT(
    MoonlightProtocolV1EncodeSessionReadyRequest(
      &request,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  TEST_CHECK(encoded_size == 777u);
  TEST_CHECK(memcmp(output, unchanged, sizeof(output)) == 0);
  return true;
}

/**
 * @brief Verifies strict START_SESSION request decoding.
 *
 * @return True on success.
 */
static bool test_start_request_decode_rejections(void) {
  MoonlightProtocolV1StartSessionRequest request = valid_start_request();
  MoonlightProtocolV1StartSessionRequest output;
  uint8_t encoded[MOONLIGHT_PROTOCOL_V1_START_SESSION_REQUEST_PAYLOAD_MAX];
  uint8_t mutated[MOONLIGHT_PROTOCOL_V1_START_SESSION_REQUEST_PAYLOAD_MAX];
  size_t field_offsets[12];
  size_t value_offset;
  size_t value_size;
  size_t encoded_size = 0;
  size_t field_count = 0;
  size_t offset = 0;
  size_t index;

  TEST_RESULT(
    MoonlightProtocolV1DecodeStartSessionRequest(NULL, 124u, &output),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeStartSessionRequest(
      start_request_golden,
      sizeof(start_request_golden),
      NULL
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_CHECK(
    start_request_decode_rejects(
      start_request_golden,
      sizeof(start_request_golden) - 1u,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  TEST_CHECK(
    start_request_decode_rejects(
      start_request_golden,
      MOONLIGHT_PROTOCOL_V1_START_SESSION_REQUEST_PAYLOAD_MAX + 1u,
      MOONLIGHT_PROTOCOL_RESULT_LIMIT_EXCEEDED
    )
  );

  request.codec_preferences[1] = MOONLIGHT_PROTOCOL_V1_VIDEO_CODEC_HEVC;
  request.codec_preferences[2] = MOONLIGHT_PROTOCOL_V1_VIDEO_CODEC_AV1;
  request.codec_preference_count = 3;
  TEST_CHECK(encode_start_request(&request, encoded, &encoded_size));
  while (offset < encoded_size) {
    const size_t field_size =
      MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE +
      test_load_u32(encoded + offset + 4u);

    field_offsets[field_count++] = offset;
    offset += field_size;
  }
  TEST_CHECK(field_count == 12u);

  for (index = 0; index < field_count; ++index) {
    memcpy(mutated, encoded, encoded_size);
    test_store_u16(mutated + field_offsets[index], 1u);
    if (index == 0) {
      test_store_u16(mutated + field_offsets[index], 2u);
    }
    TEST_CHECK(
      start_request_decode_rejects(
        mutated,
        encoded_size,
        MOONLIGHT_PROTOCOL_RESULT_MALFORMED
      )
    );
  }
  memcpy(mutated, encoded, encoded_size);
  test_store_u16(mutated, 11u);
  TEST_CHECK(
    start_request_decode_rejects(
      mutated,
      encoded_size,
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );
  memcpy(mutated, encoded, encoded_size);
  test_store_u16(mutated, 0u);
  TEST_CHECK(
    start_request_decode_rejects(
      mutated,
      encoded_size,
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );
  memcpy(mutated, encoded, encoded_size);
  test_store_u16(mutated + 2u, MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED);
  TEST_CHECK(
    start_request_decode_rejects(
      mutated,
      encoded_size,
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );
  memcpy(mutated, encoded, encoded_size);
  test_store_u16(mutated + 2u, 2u);
  TEST_CHECK(
    start_request_decode_rejects(
      mutated,
      encoded_size,
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );
  memcpy(mutated, encoded, encoded_size);
  test_store_u32(mutated + 4u, 15u);
  TEST_CHECK(
    start_request_decode_rejects(
      mutated,
      encoded_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  memcpy(mutated, encoded, encoded_size);
  test_store_u32(mutated + 4u, UINT32_MAX);
  TEST_CHECK(
    start_request_decode_rejects(
      mutated,
      encoded_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );

  TEST_CHECK(find_field(encoded, encoded_size, 8, 0, &offset, &value_offset, &value_size));
  memcpy(mutated, encoded, encoded_size);
  test_store_u16(mutated + offset + 2u, 2u);
  TEST_CHECK(
    start_request_decode_rejects(
      mutated,
      encoded_size,
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );
  memcpy(mutated, encoded, encoded_size);
  test_store_u16(mutated + offset + 2u, 0);
  TEST_CHECK(
    start_request_decode_rejects(
      mutated,
      encoded_size,
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );
  memcpy(mutated, encoded, encoded_size);
  test_store_u32(mutated + offset + 4u, 1u);
  TEST_CHECK(
    start_request_decode_rejects(
      mutated,
      encoded_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  TEST_CHECK(find_field(encoded, encoded_size, 8, 1, &offset, &value_offset, &value_size));
  memcpy(mutated, encoded, encoded_size);
  memcpy(mutated + value_offset, mutated + value_offset - 10u, 2u);
  TEST_CHECK(
    start_request_decode_rejects(
      mutated,
      encoded_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  memcpy(mutated, encoded, encoded_size);
  test_store_u16(mutated + value_offset, 4u);
  TEST_CHECK(
    start_request_decode_rejects(
      mutated,
      encoded_size,
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );

  TEST_CHECK(find_field(encoded, encoded_size, 9, 0, &offset, &value_offset, &value_size));
  memcpy(mutated, encoded, encoded_size);
  test_store_u16(mutated + offset, 8u);
  test_store_u16(
    mutated + offset + 2u,
    MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED
  );
  test_store_u32(mutated + offset + 4u, 2u);
  test_store_u16(mutated + offset + 8u, MOONLIGHT_PROTOCOL_V1_VIDEO_CODEC_H264);
  TEST_CHECK(
    start_request_decode_rejects(
      mutated,
      offset + 10u,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  TEST_CHECK(
    start_request_decode_rejects(
      encoded,
      offset,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  memcpy(mutated, encoded, encoded_size);
  test_store_u16(mutated + offset, 9u);
  test_store_u16(mutated + offset + 2u, 2u);
  test_store_u32(mutated + offset + 4u, 0u);
  TEST_CHECK(
    start_request_decode_rejects(
      mutated,
      offset + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );

  TEST_CHECK(find_field(encoded, encoded_size, 9, 0, &offset, &value_offset, &value_size));
  memcpy(mutated, encoded, encoded_size);
  test_store_u16(mutated + offset + 2u, MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED);
  TEST_CHECK(
    start_request_decode_rejects(
      mutated,
      encoded_size,
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );
  memcpy(mutated, encoded, encoded_size);
  test_store_u32(mutated + offset + 4u, 2u);
  TEST_CHECK(
    start_request_decode_rejects(
      mutated,
      encoded_size,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  memcpy(mutated, encoded, encoded_size);
  mutated[value_offset] = 2u;
  TEST_CHECK(
    start_request_decode_rejects(
      mutated,
      encoded_size,
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );
  TEST_CHECK(find_field(encoded, encoded_size, 10, 0, &offset, &value_offset, &value_size));
  memcpy(mutated, encoded, encoded_size);
  mutated[value_offset] = 2u;
  TEST_CHECK(
    start_request_decode_rejects(
      mutated,
      encoded_size,
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );

  request = valid_start_request();
  TEST_CHECK(encode_start_request(&request, encoded, &encoded_size));
  memcpy(mutated, encoded, encoded_size);
  test_store_u16(mutated + encoded_size, 11u);
  test_store_u16(mutated + encoded_size + 2u, 0);
  test_store_u32(mutated + encoded_size + 4u, 0);
  TEST_CHECK(
    start_request_decode_rejects(
      mutated,
      encoded_size + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );
  memcpy(mutated, encoded, encoded_size);
  test_store_u16(mutated + encoded_size, 0u);
  test_store_u16(mutated + encoded_size + 2u, 0);
  test_store_u32(mutated + encoded_size + 4u, 0);
  TEST_CHECK(
    start_request_decode_rejects(
      mutated,
      encoded_size + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );
  memcpy(mutated, encoded, encoded_size);
  test_store_u16(mutated + encoded_size, 11u);
  test_store_u16(mutated + encoded_size + 2u, 2u);
  test_store_u32(mutated + encoded_size + 4u, 0);
  TEST_CHECK(
    start_request_decode_rejects(
      mutated,
      encoded_size + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );
  memcpy(mutated, encoded, encoded_size);
  test_store_u16(mutated + encoded_size, 1u);
  test_store_u16(mutated + encoded_size + 2u, 0);
  test_store_u32(mutated + encoded_size + 4u, 0);
  TEST_CHECK(
    start_request_decode_rejects(
      mutated,
      encoded_size + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  mutated[encoded_size] = 0;
  TEST_CHECK(
    start_request_decode_rejects(
      mutated,
      encoded_size + 1u,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );

  TEST_CHECK(find_field(encoded, encoded_size, 8, 0, &offset, &value_offset, &value_size));
  TEST_CHECK(
    start_request_decode_rejects(
      encoded,
      offset + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + 1u,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  return true;
}

/**
 * @brief Verifies strict successful START_SESSION response decoding.
 *
 * @return True on success.
 */
static bool test_start_response_decode_rejections(void) {
  MoonlightProtocolV1StartSessionResponse output;
  uint8_t mutated[sizeof(start_response_golden)];
  size_t outer_offsets[6];
  size_t nested_offset;
  size_t nested_size;
  size_t value_offset;
  size_t value_size;
  size_t offset = 0;
  size_t index;

  TEST_RESULT(
    MoonlightProtocolV1DecodeStartSessionResponse(
      NULL,
      sizeof(start_response_golden),
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeStartSessionResponse(
      start_response_golden,
      sizeof(start_response_golden),
      NULL
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_CHECK(
    start_response_decode_rejects(
      start_response_golden,
      sizeof(start_response_golden) - 1u,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  TEST_CHECK(
    start_response_decode_rejects(
      start_response_golden,
      sizeof(start_response_golden) + 1u,
      MOONLIGHT_PROTOCOL_RESULT_LIMIT_EXCEEDED
    )
  );

  for (index = 0; index < 6u; ++index) {
    outer_offsets[index] = offset;
    offset +=
      MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE +
      test_load_u32(start_response_golden + offset + 4u);
  }
  TEST_CHECK(offset == sizeof(start_response_golden));
  for (index = 0; index < 6u; ++index) {
    memcpy(mutated, start_response_golden, sizeof(mutated));
    test_store_u16(mutated + outer_offsets[index], 1u);
    if (index == 0) {
      test_store_u16(mutated + outer_offsets[index], 2u);
    }
    TEST_CHECK(
      start_response_decode_rejects(
        mutated,
        sizeof(mutated),
        MOONLIGHT_PROTOCOL_RESULT_MALFORMED
      )
    );
  }
  memcpy(mutated, start_response_golden, sizeof(mutated));
  test_store_u16(mutated, 7u);
  TEST_CHECK(
    start_response_decode_rejects(
      mutated,
      sizeof(mutated),
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );
  memcpy(mutated, start_response_golden, sizeof(mutated));
  test_store_u16(mutated + 2u, MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED);
  TEST_CHECK(
    start_response_decode_rejects(
      mutated,
      sizeof(mutated),
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );
  memcpy(mutated, start_response_golden, sizeof(mutated));
  test_store_u32(mutated + 4u, UINT32_MAX);
  TEST_CHECK(
    start_response_decode_rejects(
      mutated,
      sizeof(mutated),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );

  TEST_CHECK(
    find_field(
      start_response_golden,
      sizeof(start_response_golden),
      6,
      0,
      &offset,
      &nested_offset,
      &nested_size
    )
  );
  TEST_CHECK(
    nested_size == MOONLIGHT_PROTOCOL_V1_ACCEPTED_VIDEO_CONFIG_PAYLOAD_SIZE
  );
  offset = 0;
  for (index = 0; index < 14u; ++index) {
    memcpy(mutated, start_response_golden, sizeof(mutated));
    test_store_u16(mutated + nested_offset + offset, 1u);
    if (index == 0) {
      test_store_u16(mutated + nested_offset + offset, 2u);
    }
    TEST_CHECK(
      start_response_decode_rejects(
        mutated,
        sizeof(mutated),
        MOONLIGHT_PROTOCOL_RESULT_MALFORMED
      )
    );
    offset +=
      MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE +
      test_load_u32(start_response_golden + nested_offset + offset + 4u);
  }
  TEST_CHECK(offset == nested_size);

  memcpy(mutated, start_response_golden, sizeof(mutated));
  test_store_u16(mutated + nested_offset, 15u);
  TEST_CHECK(
    start_response_decode_rejects(
      mutated,
      sizeof(mutated),
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );
  memcpy(mutated, start_response_golden, sizeof(mutated));
  test_store_u16(
    mutated + nested_offset + 2u,
    MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED
  );
  TEST_CHECK(
    start_response_decode_rejects(
      mutated,
      sizeof(mutated),
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );
  memcpy(mutated, start_response_golden, sizeof(mutated));
  test_store_u32(mutated + nested_offset + 4u, UINT32_MAX);
  TEST_CHECK(
    start_response_decode_rejects(
      mutated,
      sizeof(mutated),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );

#define MUTATE_NESTED_VALUE(field_id, value) \
  do { \
    TEST_CHECK( \
      find_field( \
        start_response_golden + nested_offset, \
        nested_size, \
        (field_id), \
        0, \
        &offset, \
        &value_offset, \
        &value_size \
      ) \
    ); \
    memcpy(mutated, start_response_golden, sizeof(mutated)); \
    if (value_size == 1u) { \
      mutated[nested_offset + value_offset] = (uint8_t) (value); \
    } else if (value_size == 2u) { \
      test_store_u16(mutated + nested_offset + value_offset, (uint16_t) (value)); \
    } else { \
      test_store_u32(mutated + nested_offset + value_offset, (uint32_t) (value)); \
    } \
  } while (0)

  MUTATE_NESTED_VALUE(1, 4);
  TEST_CHECK(
    start_response_decode_rejects(
      mutated,
      sizeof(mutated),
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );
  MUTATE_NESTED_VALUE(3, 9);
  TEST_CHECK(
    start_response_decode_rejects(
      mutated,
      sizeof(mutated),
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );
  MUTATE_NESTED_VALUE(4, 2);
  TEST_CHECK(
    start_response_decode_rejects(
      mutated,
      sizeof(mutated),
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );
  MUTATE_NESTED_VALUE(5, 2);
  TEST_CHECK(
    start_response_decode_rejects(
      mutated,
      sizeof(mutated),
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );
  MUTATE_NESTED_VALUE(8, 120);
  TEST_CHECK(
    find_field(
      start_response_golden + nested_offset,
      nested_size,
      9,
      0,
      &offset,
      &value_offset,
      &value_size
    )
  );
  test_store_u32(mutated + nested_offset + value_offset, 2u);
  TEST_CHECK(
    start_response_decode_rejects(
      mutated,
      sizeof(mutated),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  MUTATE_NESTED_VALUE(10, 0);
  TEST_CHECK(
    start_response_decode_rejects(
      mutated,
      sizeof(mutated),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  MUTATE_NESTED_VALUE(12, 0);
  TEST_CHECK(
    start_response_decode_rejects(
      mutated,
      sizeof(mutated),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  MUTATE_NESTED_VALUE(14, 41);
  TEST_CHECK(
    start_response_decode_rejects(
      mutated,
      sizeof(mutated),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );

#undef MUTATE_NESTED_VALUE

  memcpy(mutated, start_response_golden, sizeof(mutated));
  memset(mutated + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE, 0, 16u);
  TEST_CHECK(
    start_response_decode_rejects(
      mutated,
      sizeof(mutated),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  memcpy(mutated, start_response_golden, sizeof(mutated));
  memset(mutated + outer_offsets[1] + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE, 0, 4u);
  TEST_CHECK(
    start_response_decode_rejects(
      mutated,
      sizeof(mutated),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  memcpy(mutated, start_response_golden, sizeof(mutated));
  test_store_u32(
    mutated + outer_offsets[2] + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
    2u
  );
  TEST_CHECK(
    start_response_decode_rejects(
      mutated,
      sizeof(mutated),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  return true;
}

/**
 * @brief Verifies strict SESSION_READY request decoding.
 *
 * @return True on success.
 */
static bool test_ready_decode_rejections(void) {
  MoonlightProtocolV1SessionReadyRequest output;
  uint8_t mutated[sizeof(ready_request_golden)];
  size_t offsets[6];
  size_t offset = 0;
  size_t index;

  TEST_RESULT(
    MoonlightProtocolV1DecodeSessionReadyRequest(
      NULL,
      sizeof(ready_request_golden),
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeSessionReadyRequest(
      ready_request_golden,
      sizeof(ready_request_golden),
      NULL
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_CHECK(
    ready_request_decode_rejects(
      ready_request_golden,
      sizeof(ready_request_golden) - 1u,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  TEST_CHECK(
    ready_request_decode_rejects(
      ready_request_golden,
      sizeof(ready_request_golden) + 1u,
      MOONLIGHT_PROTOCOL_RESULT_LIMIT_EXCEEDED
    )
  );

  for (index = 0; index < 6u; ++index) {
    offsets[index] = offset;
    offset +=
      MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE +
      test_load_u32(ready_request_golden + offset + 4u);
  }
  for (index = 0; index < 6u; ++index) {
    memcpy(mutated, ready_request_golden, sizeof(mutated));
    test_store_u16(mutated + offsets[index], 1u);
    if (index == 0) {
      test_store_u16(mutated + offsets[index], 2u);
    }
    TEST_CHECK(
      ready_request_decode_rejects(
        mutated,
        sizeof(mutated),
        MOONLIGHT_PROTOCOL_RESULT_MALFORMED
      )
    );
  }
  memcpy(mutated, ready_request_golden, sizeof(mutated));
  test_store_u16(mutated, 7u);
  TEST_CHECK(
    ready_request_decode_rejects(
      mutated,
      sizeof(mutated),
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );
  memcpy(mutated, ready_request_golden, sizeof(mutated));
  test_store_u16(mutated + 2u, MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED);
  TEST_CHECK(
    ready_request_decode_rejects(
      mutated,
      sizeof(mutated),
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    )
  );
  memcpy(mutated, ready_request_golden, sizeof(mutated));
  test_store_u32(mutated + 4u, UINT32_MAX);
  TEST_CHECK(
    ready_request_decode_rejects(
      mutated,
      sizeof(mutated),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  memcpy(mutated, ready_request_golden, sizeof(mutated));
  memset(mutated + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE, 0, 16u);
  TEST_CHECK(
    ready_request_decode_rejects(
      mutated,
      sizeof(mutated),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  memcpy(mutated, ready_request_golden, sizeof(mutated));
  memset(mutated + offsets[1] + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE, 0, 4u);
  TEST_CHECK(
    ready_request_decode_rejects(
      mutated,
      sizeof(mutated),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  memcpy(mutated, ready_request_golden, sizeof(mutated));
  test_store_u32(
    mutated + offsets[2] + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
    2u
  );
  TEST_CHECK(
    ready_request_decode_rejects(
      mutated,
      sizeof(mutated),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  memcpy(mutated, ready_request_golden, sizeof(mutated));
  test_store_u16(
    mutated + offsets[3] + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
    552u
  );
  test_store_u16(
    mutated + offsets[4] + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
    528u
  );
  TEST_CHECK(
    ready_request_decode_rejects(
      mutated,
      sizeof(mutated),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  memcpy(mutated, ready_request_golden, sizeof(mutated));
  test_store_u32(
    mutated + offsets[5] + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
    MOONLIGHT_PROTOCOL_V1_VIDEO_ACCESS_UNIT_MIN - 1u
  );
  TEST_CHECK(
    ready_request_decode_rejects(
      mutated,
      sizeof(mutated),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  memcpy(mutated, ready_request_golden, sizeof(mutated));
  test_store_u32(
    mutated + offsets[5] + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
    MOONLIGHT_PROTOCOL_V1_VIDEO_ACCESS_UNIT_MAX + 1u
  );
  TEST_CHECK(
    ready_request_decode_rejects(
      mutated,
      sizeof(mutated),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    )
  );
  return true;
}

/**
 * @brief Verifies strict REQUEST_IDR request and response coding.
 *
 * @return True on success.
 */
static bool test_request_idr_codec(void) {
  MoonlightProtocolV1RequestIdrRequest request =
    valid_request_idr_request();
  MoonlightProtocolV1RequestIdrRequest decoded_request;
  MoonlightProtocolV1RequestIdrResponse response = {
    .selected_repair = MOONLIGHT_PROTOCOL_V1_VIDEO_REPAIR_IDR,
  };
  MoonlightProtocolV1RequestIdrResponse decoded_response;
  MoonlightProtocolV1RequestIdrRequest unchanged_request;
  MoonlightProtocolV1RequestIdrResponse unchanged_response;
  uint8_t request_bytes[MOONLIGHT_PROTOCOL_V1_REQUEST_IDR_REQUEST_PAYLOAD_SIZE];
  uint8_t response_bytes[MOONLIGHT_PROTOCOL_V1_REQUEST_IDR_RESPONSE_PAYLOAD_SIZE];
  uint8_t unchanged_request_bytes[sizeof(request_bytes)];
  uint8_t unchanged_response_bytes[sizeof(response_bytes)];
  uint8_t mutated[MOONLIGHT_PROTOCOL_V1_REQUEST_IDR_REQUEST_PAYLOAD_SIZE];
  size_t encoded_size = 777u;

  memset(request_bytes, 0xa5, sizeof(request_bytes));
  memset(response_bytes, 0x5a, sizeof(response_bytes));
  memcpy(unchanged_request_bytes, request_bytes, sizeof(request_bytes));
  memcpy(unchanged_response_bytes, response_bytes, sizeof(response_bytes));
  memset(&decoded_request, 0x3c, sizeof(decoded_request));
  memset(&decoded_response, 0xc3, sizeof(decoded_response));
  unchanged_request = decoded_request;
  unchanged_response = decoded_response;
  TEST_RESULT(
    MoonlightProtocolV1EncodeRequestIdrRequest(
      NULL,
      request_bytes,
      sizeof(request_bytes),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeRequestIdrRequest(
      &request,
      NULL,
      sizeof(request_bytes),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeRequestIdrRequest(
      &request,
      request_bytes,
      sizeof(request_bytes),
      NULL
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeRequestIdrRequest(
      NULL,
      sizeof(request_bytes),
      &decoded_request
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeRequestIdrRequest(
      request_bytes,
      sizeof(request_bytes),
      NULL
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeRequestIdrResponse(
      NULL,
      response_bytes,
      sizeof(response_bytes),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeRequestIdrResponse(
      &response,
      NULL,
      sizeof(response_bytes),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeRequestIdrResponse(
      &response,
      response_bytes,
      sizeof(response_bytes),
      NULL
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeRequestIdrResponse(
      NULL,
      sizeof(response_bytes),
      &decoded_response
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeRequestIdrResponse(
      response_bytes,
      sizeof(response_bytes),
      NULL
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_CHECK(encoded_size == 777u);
  TEST_CHECK(
    memcmp(request_bytes, unchanged_request_bytes, sizeof(request_bytes)) == 0
  );
  TEST_CHECK(
    memcmp(response_bytes, unchanged_response_bytes, sizeof(response_bytes)) == 0
  );
  TEST_CHECK(
    memcmp(&decoded_request, &unchanged_request, sizeof(decoded_request)) == 0
  );
  TEST_CHECK(
    memcmp(&decoded_response, &unchanged_response, sizeof(decoded_response)) ==
    0
  );

  TEST_RESULT(
    MoonlightProtocolV1EncodeRequestIdrRequest(
      &request,
      request_bytes,
      sizeof(request_bytes),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(encoded_size == sizeof(request_bytes));
  TEST_CHECK(test_load_u16(request_bytes) == 1u);
  TEST_CHECK(test_load_u32(request_bytes + 4u) == 16u);
  TEST_CHECK(test_load_u16(request_bytes + 24u) == 2u);
  TEST_CHECK(test_load_u32(request_bytes + 32u) == 17u);
  TEST_CHECK(test_load_u16(request_bytes + 36u) == 3u);
  TEST_CHECK(request_bytes[44] == MOONLIGHT_PROTOCOL_V1_VIDEO_REPAIR_IDR);
  TEST_RESULT(
    MoonlightProtocolV1DecodeRequestIdrRequest(
      request_bytes,
      sizeof(request_bytes),
      &decoded_request
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(
    memcmp(
      decoded_request.session_id,
      request.session_id,
      sizeof(request.session_id)
    ) == 0
  );
  TEST_CHECK(decoded_request.highest_complete_video_frame == 17u);
  TEST_CHECK(
    decoded_request.preferred_repair ==
    MOONLIGHT_PROTOCOL_V1_VIDEO_REPAIR_IDR
  );

  TEST_RESULT(
    MoonlightProtocolV1EncodeRequestIdrResponse(
      &response,
      response_bytes,
      sizeof(response_bytes),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(encoded_size == sizeof(response_bytes));
  TEST_CHECK(test_load_u16(response_bytes) == 1u);
  TEST_CHECK(test_load_u32(response_bytes + 4u) == 1u);
  TEST_CHECK(response_bytes[8] == MOONLIGHT_PROTOCOL_V1_VIDEO_REPAIR_IDR);
  TEST_RESULT(
    MoonlightProtocolV1DecodeRequestIdrResponse(
      response_bytes,
      sizeof(response_bytes),
      &decoded_response
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(
    decoded_response.selected_repair ==
    MOONLIGHT_PROTOCOL_V1_VIDEO_REPAIR_IDR
  );

  request.highest_complete_video_frame = INT32_MAX;
  request.preferred_repair =
    MOONLIGHT_PROTOCOL_V1_VIDEO_REPAIR_REFERENCE_INVALIDATION;
  TEST_RESULT(
    MoonlightProtocolV1EncodeRequestIdrRequest(
      &request,
      request_bytes,
      sizeof(request_bytes),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeRequestIdrRequest(
      request_bytes,
      sizeof(request_bytes),
      &decoded_request
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(decoded_request.highest_complete_video_frame == INT32_MAX);
  TEST_CHECK(
    decoded_request.preferred_repair ==
    MOONLIGHT_PROTOCOL_V1_VIDEO_REPAIR_REFERENCE_INVALIDATION
  );

  request.highest_complete_video_frame = UINT32_MAX;
  TEST_RESULT(
    MoonlightProtocolV1EncodeRequestIdrRequest(
      &request,
      request_bytes,
      sizeof(request_bytes),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  request = valid_request_idr_request();
  request.preferred_repair = (MoonlightProtocolV1VideoRepair) 3;
  TEST_RESULT(
    MoonlightProtocolV1EncodeRequestIdrRequest(
      &request,
      request_bytes,
      sizeof(request_bytes),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  request = valid_request_idr_request();
  memset(request.session_id, 0, sizeof(request.session_id));
  TEST_RESULT(
    MoonlightProtocolV1EncodeRequestIdrRequest(
      &request,
      request_bytes,
      sizeof(request_bytes),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  request = valid_request_idr_request();
  TEST_RESULT(
    MoonlightProtocolV1EncodeRequestIdrRequest(
      &request,
      request_bytes,
      sizeof(request_bytes) - 1u,
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL
  );

  TEST_RESULT(
    MoonlightProtocolV1EncodeRequestIdrRequest(
      &request,
      request_bytes,
      sizeof(request_bytes),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  memcpy(mutated, request_bytes, sizeof(mutated));
  test_store_u32(mutated + 32u, UINT32_MAX);
  TEST_RESULT(
    MoonlightProtocolV1DecodeRequestIdrRequest(
      mutated,
      sizeof(mutated),
      &decoded_request
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, request_bytes, sizeof(mutated));
  mutated[44] = 3u;
  TEST_RESULT(
    MoonlightProtocolV1DecodeRequestIdrRequest(
      mutated,
      sizeof(mutated),
      &decoded_request
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeRequestIdrRequest(
      request_bytes,
      sizeof(request_bytes) - 1u,
      &decoded_request
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeRequestIdrRequest(
      request_bytes,
      sizeof(request_bytes) + 1u,
      &decoded_request
    ),
    MOONLIGHT_PROTOCOL_RESULT_LIMIT_EXCEEDED
  );

  response.selected_repair = (MoonlightProtocolV1VideoRepair) 3;
  TEST_RESULT(
    MoonlightProtocolV1EncodeRequestIdrResponse(
      &response,
      response_bytes,
      sizeof(response_bytes),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  response.selected_repair = MOONLIGHT_PROTOCOL_V1_VIDEO_REPAIR_IDR;
  TEST_RESULT(
    MoonlightProtocolV1EncodeRequestIdrResponse(
      &response,
      response_bytes,
      sizeof(response_bytes) - 1u,
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeRequestIdrResponse(
      &response,
      response_bytes,
      sizeof(response_bytes),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  response_bytes[8] = 3u;
  TEST_RESULT(
    MoonlightProtocolV1DecodeRequestIdrResponse(
      response_bytes,
      sizeof(response_bytes),
      &decoded_response
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeRequestIdrResponse(
      response_bytes,
      sizeof(response_bytes) - 1u,
      &decoded_response
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeRequestIdrResponse(
      response_bytes,
      sizeof(response_bytes) + 1u,
      &decoded_response
    ),
    MOONLIGHT_PROTOCOL_RESULT_LIMIT_EXCEEDED
  );
  return true;
}

/**
 * @brief Runs one named boolean test.
 *
 * @param name Test name.
 * @param test Test function.
 * @return Zero on success, one on failure.
 */
static int run_test(const char *name, bool (*test)(void)) {
  if (!test()) {
    fprintf(stderr, "FAILED: %s\n", name);
    return 1;
  }
  printf("PASS: %s\n", name);
  return 0;
}

/**
 * @brief Runs all direct-display Stream Session schema tests.
 *
 * @return Zero only when every test passes.
 */
int main(void) {
  int failures = 0;

  failures += run_test("goldens", test_goldens);
  failures += run_test("round_trip_bounds", test_round_trip_bounds);
  failures += run_test(
    "start_request_encode_rejections",
    test_start_request_encode_rejections
  );
  failures += run_test(
    "start_response_encode_rejections",
    test_start_response_encode_rejections
  );
  failures += run_test(
    "ready_encode_rejections",
    test_ready_encode_rejections
  );
  failures += run_test(
    "start_request_decode_rejections",
    test_start_request_decode_rejections
  );
  failures += run_test(
    "start_response_decode_rejections",
    test_start_response_decode_rejections
  );
  failures += run_test(
    "ready_decode_rejections",
    test_ready_decode_rejections
  );
  failures += run_test("request_idr_codec", test_request_idr_codec);
  return failures == 0 ? 0 : 1;
}
