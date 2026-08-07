/**
 * @file mic.c
 * @brief Encodes and decodes canonical protocol version 1 microphone datagrams.
 */
#include <moonlight/protocol/mic.h>

#include <string.h>

/**
 * @brief Writes one network-order 32-bit value.
 */
static void mic_store_u32(uint8_t *output, uint32_t value) {
  output[0] = (uint8_t) (value >> 24u);
  output[1] = (uint8_t) (value >> 16u);
  output[2] = (uint8_t) (value >> 8u);
  output[3] = (uint8_t) value;
}

MoonlightProtocolResult MoonlightProtocolV1EncodeMicDatagram(
  uint32_t session_wire_id,
  uint32_t sequence,
  const uint8_t *opus,
  size_t opus_size,
  uint8_t *output,
  size_t output_size,
  size_t *encoded_size
) {
  uint8_t encoded[MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE +
                   MOONLIGHT_PROTOCOL_V1_MIC_MAX_PAYLOAD_SIZE];
  const size_t encoded_size_value =
    MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE + opus_size;

  if (
    session_wire_id == 0 ||
    sequence == 0 ||
    opus == NULL ||
    opus_size < MOONLIGHT_PROTOCOL_V1_MIC_MIN_PAYLOAD_SIZE ||
    opus_size > MOONLIGHT_PROTOCOL_V1_MIC_MAX_PAYLOAD_SIZE ||
    output == NULL ||
    encoded_size == NULL
  ) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (output_size < sizeof(encoded)) {
    return MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL;
  }

  encoded[0] = MOONLIGHT_PROTOCOL_V1_CHANNEL_MIC;
  encoded[1] = 0;
  encoded[2] = 0;
  encoded[3] = 0;
  mic_store_u32(encoded + 4u, session_wire_id);
  mic_store_u32(encoded + 8u, 0);
  mic_store_u32(encoded + 12u, sequence);
  memcpy(encoded + MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE, opus, opus_size);

  memcpy(output, encoded, encoded_size_value);
  *encoded_size = encoded_size_value;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

MoonlightProtocolResult MoonlightProtocolV1DecodeMicDatagram(
  uint32_t expected_session_wire_id,
  const uint8_t *input,
  size_t input_size,
  uint8_t *opus,
  size_t opus_capacity,
  size_t *opus_size
) {
  MoonlightProtocolV1DatagramHeader header;
  size_t payload_size;
  MoonlightProtocolResult result;

  if (
    expected_session_wire_id == 0 ||
    input == NULL ||
    opus == NULL ||
    opus_size == NULL
  ) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (input_size < MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE) {
    return MOONLIGHT_PROTOCOL_RESULT_TRUNCATED;
  }

  result = MoonlightProtocolV1DecodeDatagramHeader(
    input,
    input_size,
    MOONLIGHT_PROTOCOL_DIRECTION_CLIENT_TO_HOST,
    &header
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (header.channel != MOONLIGHT_PROTOCOL_V1_CHANNEL_MIC) {
    return MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH;
  }
  if (header.session_wire_id != expected_session_wire_id) {
    return MOONLIGHT_PROTOCOL_RESULT_STALE_CONTEXT;
  }
  if (header.sequence == 0) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }

  payload_size = input_size - MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE;
  if (payload_size < MOONLIGHT_PROTOCOL_V1_MIC_MIN_PAYLOAD_SIZE) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  if (payload_size > MOONLIGHT_PROTOCOL_V1_MIC_MAX_PAYLOAD_SIZE) {
    return MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED;
  }
  if (payload_size > opus_capacity) {
    return MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL;
  }

  memcpy(
    opus,
    input + MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE,
    payload_size
  );
  *opus_size = payload_size;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}
