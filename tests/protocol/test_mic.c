/**
 * @file test_mic.c
 * @brief Native tests for canonical Sunshine protocol version 1 microphone DATAGRAMs.
 */

#include <moonlight/protocol/mic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief Asserts that an expression is true, printing the failed expression.
 */
#define ASSERT_TRUE(expr) \
  do { \
    if (!(expr)) { \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
      return false; \
    } \
  } while (0)

/**
 * @brief Asserts that a codec call returned the expected result.
 */
#define ASSERT_RESULT(call, expected) \
  do { \
    MoonlightProtocolResult actual = (call); \
    if (actual != (expected)) { \
      fprintf(stderr, "FAIL %s:%d: %s -> %d, expected %d\n", \
              __FILE__, __LINE__, #call, (int) actual, (int) (expected)); \
      return false; \
    } \
  } while (0)

/**
 * @brief Exercises the round-trip encode/decode of a canonical MIC DATAGRAM.
 */
static bool test_round_trip(void) {
  const uint8_t opus[64] = {0x01, 0x02, 0x03, 0x04};
  uint8_t datagram[MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE +
                    MOONLIGHT_PROTOCOL_V1_MIC_MAX_PAYLOAD_SIZE];
  uint8_t decoded[MOONLIGHT_PROTOCOL_V1_MIC_MAX_PAYLOAD_SIZE];
  size_t encoded_size = 0;
  size_t decoded_size = 0;

  ASSERT_RESULT(
    MoonlightProtocolV1EncodeMicDatagram(
      0x2a,
      0x11,
      opus,
      sizeof(opus),
      datagram,
      sizeof(datagram),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  ASSERT_TRUE(encoded_size == MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE + sizeof(opus));

  // The header must name the microphone channel with the exact session id.
  ASSERT_TRUE(datagram[0] == MOONLIGHT_PROTOCOL_V1_CHANNEL_MIC);
  ASSERT_TRUE(datagram[1] == 0);

  ASSERT_RESULT(
    MoonlightProtocolV1DecodeMicDatagram(
      0x2a,
      datagram,
      encoded_size,
      decoded,
      sizeof(decoded),
      &decoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  ASSERT_TRUE(decoded_size == sizeof(opus));
  ASSERT_TRUE(memcmp(decoded, opus, sizeof(opus)) == 0);

  return true;
}

/**
 * @brief Exercises rejection of foreign, malformed, and truncated datagrams.
 */
static bool test_rejections(void) {
  const uint8_t opus[8] = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17};
  uint8_t datagram[MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE +
                    MOONLIGHT_PROTOCOL_V1_MIC_MAX_PAYLOAD_SIZE];
  uint8_t decoded[MOONLIGHT_PROTOCOL_V1_MIC_MAX_PAYLOAD_SIZE];
  size_t encoded_size = 0;
  size_t decoded_size = 0;

  ASSERT_RESULT(
    MoonlightProtocolV1EncodeMicDatagram(
      0x2a,
      0x11,
      opus,
      sizeof(opus),
      datagram,
      sizeof(datagram),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );

  // Wrong session: stale context.
  ASSERT_RESULT(
    MoonlightProtocolV1DecodeMicDatagram(
      0x2b,
      datagram,
      encoded_size,
      decoded,
      sizeof(decoded),
      &decoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_STALE_CONTEXT
  );

  // Truncated datagram.
  ASSERT_RESULT(
    MoonlightProtocolV1DecodeMicDatagram(
      0x2a,
      datagram,
      MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE - 1u,
      decoded,
      sizeof(decoded),
      &decoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_TRUNCATED
  );

  // Zero sequence is malformed.
  ASSERT_RESULT(
    MoonlightProtocolV1EncodeMicDatagram(
      0x2a,
      0,
      opus,
      sizeof(opus),
      datagram,
      sizeof(datagram),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );

  // Zero session id is invalid.
  ASSERT_RESULT(
    MoonlightProtocolV1EncodeMicDatagram(
      0,
      0x11,
      opus,
      sizeof(opus),
      datagram,
      sizeof(datagram),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );

  // Empty opus payload is invalid.
  ASSERT_RESULT(
    MoonlightProtocolV1EncodeMicDatagram(
      0x2a,
      0x11,
      opus,
      0,
      datagram,
      sizeof(datagram),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );

  // Destination buffer too small.
  ASSERT_RESULT(
    MoonlightProtocolV1EncodeMicDatagram(
      0x2a,
      0x11,
      opus,
      sizeof(opus),
      datagram,
      MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE,
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL
  );

  return true;
}

int main(void) {
  if (!test_round_trip()) {
    return 1;
  }
  if (!test_rejections()) {
    return 1;
  }
  printf("mic protocol tests passed\n");
  return 0;
}
