/**
 * @file input.c
 * @brief Implements canonical protocol version 1 typed input encoding.
 */

#include <limits.h>
#include <moonlight/protocol/input.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/**
 * @brief Number of required fields in one reliable input payload.
 */
#define RELIABLE_INPUT_FIELD_COUNT 3u

/**
 * @brief Stores one network-order 16-bit integer.
 *
 * @param output Two writable bytes.
 * @param value Host-order value.
 */
static void input_store_u16(uint8_t *output, uint16_t value) {
  output[0] = (uint8_t) (value >> 8u);
  output[1] = (uint8_t) value;
}

/**
 * @brief Stores one network-order 32-bit integer.
 *
 * @param output Four writable bytes.
 * @param value Host-order value.
 */
static void input_store_u32(uint8_t *output, uint32_t value) {
  output[0] = (uint8_t) (value >> 24u);
  output[1] = (uint8_t) (value >> 16u);
  output[2] = (uint8_t) (value >> 8u);
  output[3] = (uint8_t) value;
}

/**
 * @brief Loads one network-order 16-bit integer.
 *
 * @param input Two readable bytes.
 * @return Host-order value.
 */
static uint16_t input_load_u16(const uint8_t *input) {
  return (uint16_t) (((uint16_t) input[0] << 8u) | input[1]);
}

/**
 * @brief Loads one network-order signed 16-bit integer.
 *
 * @param input Two readable bytes.
 * @return Host-order two's-complement value.
 */
static int16_t input_load_i16(const uint8_t *input) {
  const uint16_t encoded = input_load_u16(input);

  if (encoded <= INT16_MAX) {
    return (int16_t) encoded;
  }
  return (int16_t) (-1 - (int16_t) (UINT16_MAX - encoded));
}

/**
 * @brief Loads one network-order 32-bit integer.
 *
 * @param input Four readable bytes.
 * @return Host-order value.
 */
static uint32_t input_load_u32(const uint8_t *input) {
  return ((uint32_t) input[0] << 24u) |
         ((uint32_t) input[1] << 16u) |
         ((uint32_t) input[2] << 8u) |
         input[3];
}

/**
 * @brief Writes one fixed scalar TLV field.
 *
 * @param output Destination positioned at the field header.
 * @param field_id Canonical field identifier.
 * @param value Scalar field bytes.
 * @param value_size Exact scalar byte count.
 * @return Number of bytes written.
 */
static size_t input_encode_field(
  uint8_t *output,
  uint16_t field_id,
  const uint8_t *value,
  size_t value_size
) {
  input_store_u16(output, field_id);
  input_store_u16(output + 2u, 0);
  input_store_u32(output + 4u, (uint32_t) value_size);
  memcpy(output + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE, value, value_size);
  return MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + value_size;
}

/**
 * @brief Tests whether a touch event is in the canonical registry.
 *
 * @param event Event to test.
 * @return True for down, up, or cancel.
 */
static bool input_touch_event_is_known(MoonlightProtocolV1TouchEvent event) {
  return event == MOONLIGHT_PROTOCOL_V1_TOUCH_EVENT_DOWN ||
         event == MOONLIGHT_PROTOCOL_V1_TOUCH_EVENT_UP ||
         event == MOONLIGHT_PROTOCOL_V1_TOUCH_EVENT_CANCEL;
}

/**
 * @brief Tests whether a cancel edge carries only its contact identifier.
 *
 * @param edge Edge to inspect.
 * @return True for a canonical cancel or any non-cancel event.
 */
static bool input_touch_cancel_is_canonical(
  const MoonlightProtocolV1TouchEdge *edge
) {
  return edge->event != MOONLIGHT_PROTOCOL_V1_TOUCH_EVENT_CANCEL ||
         (uint16_t) (edge->x |
                     edge->y |
                     edge->pressure |
                     edge->major |
                     edge->minor |
                     (uint16_t) edge->rotation_centidegrees) == 0;
}

/**
 * @brief Validates one caller-supplied reliable touch edge.
 *
 * @param edge Edge to validate.
 * @return The codec result.
 */
static MoonlightProtocolResult input_validate_touch_edge(
  const MoonlightProtocolV1TouchEdge *edge
) {
  if (edge == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (edge->input_sequence == 0) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  if (!input_touch_event_is_known(edge->event)) {
    return MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED;
  }
  if (!input_touch_cancel_is_canonical(edge)) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Writes a canonical reliable TOUCH_EDGE body.
 *
 * @param edge Validated edge.
 * @param output Eighteen writable bytes.
 */
static void input_write_touch_edge_body(
  const MoonlightProtocolV1TouchEdge *edge,
  uint8_t *output
) {
  input_store_u32(output, edge->contact);
  output[4] = (uint8_t) edge->event;
  output[5] = 0;
  input_store_u16(output + 6u, edge->x);
  input_store_u16(output + 8u, edge->y);
  input_store_u16(output + 10u, edge->pressure);
  input_store_u16(output + 12u, edge->major);
  input_store_u16(output + 14u, edge->minor);
  input_store_u16(output + 16u, (uint16_t) edge->rotation_centidegrees);
}

/**
 * @brief Decodes one exact scalar field from a reliable input payload.
 *
 * @param input Complete containing payload.
 * @param input_size Number of containing bytes.
 * @param offset Current offset, advanced on success.
 * @param expected_id Required field ID.
 * @param expected_size Required field-value size.
 * @param wrong_size_result Result for a complete field of the wrong size.
 * @param value Receives the borrowed field value.
 * @return The codec result.
 */
static MoonlightProtocolResult input_decode_scalar(
  const uint8_t *input,
  size_t input_size,
  size_t *offset,
  uint16_t expected_id,
  size_t expected_size,
  MoonlightProtocolResult wrong_size_result,
  const uint8_t **value
) {
  MoonlightProtocolV1TlvField field;
  MoonlightProtocolResult result;
  size_t remaining;

  remaining = input_size - *offset;
  result = MoonlightProtocolV1DecodeTlvFieldHeader(
    input + *offset,
    remaining,
    &field
  );
  if (result == MOONLIGHT_PROTOCOL_RESULT_TRUNCATED) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (field.field_id != expected_id) {
    return field.field_id >= 1u &&
               field.field_id <= RELIABLE_INPUT_FIELD_COUNT ?
             MOONLIGHT_PROTOCOL_RESULT_MALFORMED :
             MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED;
  }
  if (field.flags != 0) {
    return MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED;
  }
  if (field.field_length > remaining - MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  if (field.field_length != expected_size) {
    return wrong_size_result;
  }

  *value = input + *offset + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE;
  *offset += MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + field.field_length;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Validates one caller-supplied reliable keyboard edge.
 *
 * @param edge Edge to validate.
 * @return The codec result.
 */
static MoonlightProtocolResult input_validate_key_edge(
  const MoonlightProtocolV1KeyEdge *edge
) {
  if (edge == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (
    edge->input_sequence == 0 ||
    edge->usage_page == 0 ||
    edge->usage == 0
  ) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  if (edge->pressed > 1u) {
    return MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED;
  }
  if (
    edge->usage_page == 0x0007u &&
    edge->usage >= 0x00e0u &&
    edge->usage <= 0x00e7u
  ) {
    const uint8_t modifier =
      (uint8_t)(1u << (edge->usage - 0x00e0u));
    if (
      ((edge->modifiers & modifier) != 0u) !=
      (edge->pressed != 0u)
    ) {
      return MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED;
    }
  }
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

MoonlightProtocolResult MoonlightProtocolV1DecodeReliableInputSubtype(
  const uint8_t *input,
  size_t input_size,
  MoonlightProtocolV1ReliableInputSubtype *subtype
) {
  const uint8_t *value;
  MoonlightProtocolResult result;
  MoonlightProtocolV1ReliableInputSubtype decoded;
  size_t offset = 0;

  if (input == NULL || subtype == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (input_size > MOONLIGHT_PROTOCOL_V1_RELIABLE_INPUT_PAYLOAD_MAX) {
    return MOONLIGHT_PROTOCOL_RESULT_LIMIT_EXCEEDED;
  }
  if (input_size < 2u * MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + 6u) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  result = input_decode_scalar(
    input,
    input_size,
    &offset,
    1,
    4u,
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED,
    &value
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (input_load_u32(value) == 0) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  result = input_decode_scalar(
    input,
    input_size,
    &offset,
    2,
    2u,
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED,
    &value
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  decoded = (MoonlightProtocolV1ReliableInputSubtype) input_load_u16(value);
  *subtype = decoded;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

MoonlightProtocolResult MoonlightProtocolV1EncodeKeyEdgePayload(
  const MoonlightProtocolV1KeyEdge *edge,
  uint8_t *output,
  size_t output_size,
  size_t *encoded_size
) {
  uint8_t encoded[MOONLIGHT_PROTOCOL_V1_KEY_EDGE_PAYLOAD_SIZE];
  uint8_t scalar[MOONLIGHT_PROTOCOL_V1_KEY_EDGE_BODY_SIZE];
  MoonlightProtocolResult result;
  size_t size = 0;

  if (output == NULL || encoded_size == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  result = input_validate_key_edge(edge);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }

  input_store_u32(scalar, edge->input_sequence);
  size += input_encode_field(encoded + size, 1, scalar, 4u);
  input_store_u16(
    scalar,
    (uint16_t) MOONLIGHT_PROTOCOL_V1_RELIABLE_INPUT_KEY_EDGE
  );
  size += input_encode_field(encoded + size, 2, scalar, 2u);
  input_store_u16(scalar, edge->usage_page);
  input_store_u16(scalar + 2u, edge->usage);
  scalar[4] = edge->pressed;
  scalar[5] = edge->modifiers;
  scalar[6] = 0;
  scalar[7] = 0;
  size += input_encode_field(
    encoded + size,
    3,
    scalar,
    MOONLIGHT_PROTOCOL_V1_KEY_EDGE_BODY_SIZE
  );

  if (output_size < size) {
    return MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL;
  }
  memcpy(output, encoded, size);
  *encoded_size = size;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

MoonlightProtocolResult MoonlightProtocolV1DecodeKeyEdgePayload(
  const uint8_t *input,
  size_t input_size,
  MoonlightProtocolV1KeyEdge *edge
) {
  MoonlightProtocolV1KeyEdge decoded;
  const uint8_t *value;
  MoonlightProtocolResult result;
  size_t offset = 0;

  if (input == NULL || edge == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (input_size > MOONLIGHT_PROTOCOL_V1_KEY_EDGE_PAYLOAD_SIZE) {
    return MOONLIGHT_PROTOCOL_RESULT_LIMIT_EXCEEDED;
  }
  if (input_size < 3u * MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }

  memset(&decoded, 0, sizeof(decoded));
  result = input_decode_scalar(
    input,
    input_size,
    &offset,
    1,
    4u,
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED,
    &value
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  decoded.input_sequence = input_load_u32(value);
  result = input_decode_scalar(
    input,
    input_size,
    &offset,
    2,
    2u,
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED,
    &value
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (
    input_load_u16(value) !=
    MOONLIGHT_PROTOCOL_V1_RELIABLE_INPUT_KEY_EDGE
  ) {
    return MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED;
  }
  result = input_decode_scalar(
    input,
    input_size,
    &offset,
    3,
    MOONLIGHT_PROTOCOL_V1_KEY_EDGE_BODY_SIZE,
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED,
    &value
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (value[6] != 0 || value[7] != 0) {
    return MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED;
  }

  decoded.usage_page = input_load_u16(value);
  decoded.usage = input_load_u16(value + 2u);
  decoded.pressed = value[4];
  decoded.modifiers = value[5];
  result = input_validate_key_edge(&decoded);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }

  *edge = decoded;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}


MoonlightProtocolResult MoonlightProtocolV1EncodeTouchEdgePayload(
  const MoonlightProtocolV1TouchEdge *edge,
  uint8_t *output,
  size_t output_size,
  size_t *encoded_size
) {
  uint8_t encoded[MOONLIGHT_PROTOCOL_V1_TOUCH_EDGE_PAYLOAD_SIZE];
  uint8_t scalar[MOONLIGHT_PROTOCOL_V1_TOUCH_EDGE_BODY_SIZE];
  MoonlightProtocolResult result;
  size_t size = 0;

  if (output == NULL || encoded_size == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  result = input_validate_touch_edge(edge);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }

  input_store_u32(scalar, edge->input_sequence);
  size += input_encode_field(encoded + size, 1, scalar, 4u);
  input_store_u16(
    scalar,
    (uint16_t) MOONLIGHT_PROTOCOL_V1_RELIABLE_INPUT_TOUCH_EDGE
  );
  size += input_encode_field(encoded + size, 2, scalar, 2u);
  input_write_touch_edge_body(edge, scalar);
  size += input_encode_field(
    encoded + size,
    3,
    scalar,
    MOONLIGHT_PROTOCOL_V1_TOUCH_EDGE_BODY_SIZE
  );

  if (output_size < size) {
    return MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL;
  }
  memcpy(output, encoded, size);
  *encoded_size = size;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

MoonlightProtocolResult MoonlightProtocolV1DecodeTouchEdgePayload(
  const uint8_t *input,
  size_t input_size,
  MoonlightProtocolV1TouchEdge *edge
) {
  MoonlightProtocolV1TouchEdge decoded;
  const uint8_t *value;
  MoonlightProtocolResult result;
  size_t offset = 0;

  if (input == NULL || edge == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (input_size > MOONLIGHT_PROTOCOL_V1_TOUCH_EDGE_PAYLOAD_SIZE) {
    return MOONLIGHT_PROTOCOL_RESULT_LIMIT_EXCEEDED;
  }
  if (input_size < 3u * MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }

  memset(&decoded, 0, sizeof(decoded));
  result = input_decode_scalar(
    input,
    input_size,
    &offset,
    1,
    4u,
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED,
    &value
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  decoded.input_sequence = input_load_u32(value);
  result = input_decode_scalar(
    input,
    input_size,
    &offset,
    2,
    2u,
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED,
    &value
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (
    input_load_u16(value) !=
    MOONLIGHT_PROTOCOL_V1_RELIABLE_INPUT_TOUCH_EDGE
  ) {
    return MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED;
  }
  result = input_decode_scalar(
    input,
    input_size,
    &offset,
    3,
    MOONLIGHT_PROTOCOL_V1_TOUCH_EDGE_BODY_SIZE,
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED,
    &value
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (value[5] != 0) {
    return MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED;
  }

  decoded.contact = input_load_u32(value);
  decoded.event = (MoonlightProtocolV1TouchEvent) value[4];
  decoded.x = input_load_u16(value + 6u);
  decoded.y = input_load_u16(value + 8u);
  decoded.pressure = input_load_u16(value + 10u);
  decoded.major = input_load_u16(value + 12u);
  decoded.minor = input_load_u16(value + 14u);
  decoded.rotation_centidegrees = input_load_i16(value + 16u);
  result = input_validate_touch_edge(&decoded);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }

  *edge = decoded;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Validates one caller-supplied real-time touch move.
 *
 * @param move Move to validate.
 * @return The codec result.
 */
static MoonlightProtocolResult input_validate_touch_move(
  const MoonlightProtocolV1TouchMove *move
) {
  if (move == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (move->input_sequence == 0) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Writes a canonical real-time TOUCH_MOVE body.
 *
 * @param move Validated move.
 * @param output Sixteen writable bytes.
 */
static void input_write_touch_move_body(
  const MoonlightProtocolV1TouchMove *move,
  uint8_t *output
) {
  input_store_u32(output, move->contact);
  input_store_u16(output + 4u, move->x);
  input_store_u16(output + 6u, move->y);
  input_store_u16(output + 8u, move->pressure);
  input_store_u16(output + 10u, move->major);
  input_store_u16(output + 12u, move->minor);
  input_store_u16(output + 14u, (uint16_t) move->rotation_centidegrees);
}

MoonlightProtocolResult MoonlightProtocolV1EncodeTouchMoveDatagram(
  uint32_t session_wire_id,
  const MoonlightProtocolV1TouchMove *move,
  uint8_t *output,
  size_t output_size,
  size_t *encoded_size
) {
  uint8_t encoded[MOONLIGHT_PROTOCOL_V1_TOUCH_MOVE_DATAGRAM_SIZE];
  MoonlightProtocolResult result;

  if (session_wire_id == 0 || output == NULL || encoded_size == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  result = input_validate_touch_move(move);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (output_size < sizeof(encoded)) {
    return MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL;
  }

  encoded[0] = MOONLIGHT_PROTOCOL_V1_CHANNEL_REALTIME_INPUT;
  encoded[1] = 0;
  input_store_u16(encoded + 2u, 0);
  input_store_u32(encoded + 4u, session_wire_id);
  input_store_u32(encoded + 8u, 0);
  input_store_u32(encoded + 12u, move->input_sequence);
  input_store_u32(
    encoded + MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE,
    move->input_sequence
  );
  input_store_u16(
    encoded + MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE + 4u,
    (uint16_t) MOONLIGHT_PROTOCOL_V1_REALTIME_INPUT_TOUCH_MOVE
  );
  input_store_u16(
    encoded + MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE + 6u,
    MOONLIGHT_PROTOCOL_V1_TOUCH_MOVE_BODY_SIZE
  );
  input_write_touch_move_body(
    move,
    encoded + MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE +
      MOONLIGHT_PROTOCOL_V1_INPUT_STATE_HEADER_SIZE
  );

  memcpy(output, encoded, sizeof(encoded));
  *encoded_size = sizeof(encoded);
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

MoonlightProtocolResult MoonlightProtocolV1DecodeTouchMoveDatagram(
  uint32_t expected_session_wire_id,
  const uint8_t *input,
  size_t input_size,
  MoonlightProtocolV1TouchMove *move
) {
  MoonlightProtocolV1DatagramHeader header;
  MoonlightProtocolV1TouchMove decoded;
  const uint8_t *state;
  const uint8_t *body;
  MoonlightProtocolResult result;
  uint32_t state_sequence;

  if (expected_session_wire_id == 0 || input == NULL || move == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (input_size < MOONLIGHT_PROTOCOL_V1_TOUCH_MOVE_DATAGRAM_SIZE) {
    return MOONLIGHT_PROTOCOL_RESULT_TRUNCATED;
  }
  if (input_size > MOONLIGHT_PROTOCOL_V1_TOUCH_MOVE_DATAGRAM_SIZE) {
    return MOONLIGHT_PROTOCOL_RESULT_TRAILING_DATA;
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
  if (header.channel != MOONLIGHT_PROTOCOL_V1_CHANNEL_REALTIME_INPUT) {
    return MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH;
  }
  if (header.session_wire_id != expected_session_wire_id) {
    return MOONLIGHT_PROTOCOL_RESULT_STALE_CONTEXT;
  }
  if (header.sequence == 0) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }

  state = input + MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE;
  state_sequence = input_load_u32(state);
  if (state_sequence == 0 || state_sequence != header.sequence) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  if (
    input_load_u16(state + 4u) !=
    MOONLIGHT_PROTOCOL_V1_REALTIME_INPUT_TOUCH_MOVE
  ) {
    return MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED;
  }
  if (input_load_u16(state + 6u) != MOONLIGHT_PROTOCOL_V1_TOUCH_MOVE_BODY_SIZE) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }

  body = state + MOONLIGHT_PROTOCOL_V1_INPUT_STATE_HEADER_SIZE;
  decoded.input_sequence = state_sequence;
  decoded.contact = input_load_u32(body);
  decoded.x = input_load_u16(body + 4u);
  decoded.y = input_load_u16(body + 6u);
  decoded.pressure = input_load_u16(body + 8u);
  decoded.major = input_load_u16(body + 10u);
  decoded.minor = input_load_u16(body + 12u);
  decoded.rotation_centidegrees = input_load_i16(body + 14u);
  *move = decoded;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}
