/**
 * @file wire.c
 * @brief Encodes and incrementally parses protocol version 1 reliable framing.
 */

#include <limits.h>
#include <moonlight/protocol/wire.h>
#include <string.h>

/**
 * @brief Canonical lane-preface magic `SSQ1`.
 */
#define LANE_PREFACE_MAGIC 0x53535131u

/**
 * @brief Canonical reliable-message magic `SQM1`.
 */
#define MESSAGE_ENVELOPE_MAGIC 0x53514d31u

/**
 * @brief Mask containing every version 1 envelope flag.
 */
#define ENVELOPE_FLAG_MASK 0x0007u

/**
 * @brief Mask containing every version 1 TLV flag.
 */
#define TLV_FLAG_MASK 0x0001u

/**
 * @brief Initial reliable-stream parser phase.
 */
#define STREAM_STATE_PREFACE 1u

/**
 * @brief Reliable-stream envelope-header phase.
 */
#define STREAM_STATE_ENVELOPE 2u

/**
 * @brief Reliable-stream payload phase.
 */
#define STREAM_STATE_PAYLOAD 3u

/**
 * @brief Reliable-stream pending envelope-end event phase.
 */
#define STREAM_STATE_ENVELOPE_END 4u

/**
 * @brief Terminal reliable-stream parser phase.
 */
#define STREAM_STATE_FAILED 5u

/**
 * @brief Initial and between-field TLV parser phase.
 */
#define TLV_STATE_HEADER 1u

/**
 * @brief TLV field-value phase.
 */
#define TLV_STATE_VALUE 2u

/**
 * @brief Pending TLV field-end event phase.
 */
#define TLV_STATE_FIELD_END 3u

/**
 * @brief Complete TLV container phase.
 */
#define TLV_STATE_COMPLETE 4u

/**
 * @brief Terminal TLV parser phase.
 */
#define TLV_STATE_FAILED 5u

/**
 * @brief Loads one network-order 16-bit integer.
 *
 * @param input Two readable bytes.
 * @return Host-order value.
 */
static uint16_t load_u16(const uint8_t *input) {
  return (uint16_t) (((uint16_t) input[0] << 8u) | input[1]);
}

/**
 * @brief Loads one network-order 32-bit integer.
 *
 * @param input Four readable bytes.
 * @return Host-order value.
 */
static uint32_t load_u32(const uint8_t *input) {
  return ((uint32_t) input[0] << 24u) |
         ((uint32_t) input[1] << 16u) |
         ((uint32_t) input[2] << 8u) |
         input[3];
}

/**
 * @brief Loads one network-order 64-bit integer.
 *
 * @param input Eight readable bytes.
 * @return Host-order value.
 */
static uint64_t load_u64(const uint8_t *input) {
  return ((uint64_t) load_u32(input) << 32u) | load_u32(input + 4u);
}

/**
 * @brief Stores one network-order 16-bit integer.
 *
 * @param output Two writable bytes.
 * @param value Host-order value.
 */
static void store_u16(uint8_t *output, uint16_t value) {
  output[0] = (uint8_t) (value >> 8u);
  output[1] = (uint8_t) value;
}

/**
 * @brief Stores one network-order 32-bit integer.
 *
 * @param output Four writable bytes.
 * @param value Host-order value.
 */
static void store_u32(uint8_t *output, uint32_t value) {
  output[0] = (uint8_t) (value >> 24u);
  output[1] = (uint8_t) (value >> 16u);
  output[2] = (uint8_t) (value >> 8u);
  output[3] = (uint8_t) value;
}

/**
 * @brief Stores one network-order 64-bit integer.
 *
 * @param output Eight writable bytes.
 * @param value Host-order value.
 */
static void store_u64(uint8_t *output, uint64_t value) {
  store_u32(output, (uint32_t) (value >> 32u));
  store_u32(output + 4u, (uint32_t) value);
}

/**
 * @brief Tests whether a message type has request and response forms.
 *
 * @param message_type Candidate message type.
 * @return True for a request/response message.
 */
static bool message_type_is_request_response(uint16_t message_type) {
  switch (message_type) {
    case MOONLIGHT_PROTOCOL_V1_MESSAGE_CLIENT_HELLO:
    case MOONLIGHT_PROTOCOL_V1_MESSAGE_PING:
    case MOONLIGHT_PROTOCOL_V1_MESSAGE_CLIENT_PROOF:
    case MOONLIGHT_PROTOCOL_V1_MESSAGE_PAIR_REQUEST:
    case MOONLIGHT_PROTOCOL_V1_MESSAGE_GET_HOST_INFO:
    case MOONLIGHT_PROTOCOL_V1_MESSAGE_GET_APP_LIST:
    case MOONLIGHT_PROTOCOL_V1_MESSAGE_LIST_APPLICATION_INSTANCES:
    case MOONLIGHT_PROTOCOL_V1_MESSAGE_START_APPLICATION:
    case MOONLIGHT_PROTOCOL_V1_MESSAGE_STOP_APPLICATION:
    case MOONLIGHT_PROTOCOL_V1_MESSAGE_ATTACH_APPLICATION:
    case MOONLIGHT_PROTOCOL_V1_MESSAGE_DETACH_APPLICATION:
    case MOONLIGHT_PROTOCOL_V1_MESSAGE_START_SESSION:
    case MOONLIGHT_PROTOCOL_V1_MESSAGE_SESSION_READY:
    case MOONLIGHT_PROTOCOL_V1_MESSAGE_STOP_SESSION:
    case MOONLIGHT_PROTOCOL_V1_MESSAGE_MEDIA_EPOCH_READY:
    case MOONLIGHT_PROTOCOL_V1_MESSAGE_REQUEST_IDR:
    case MOONLIGHT_PROTOCOL_V1_MESSAGE_GET_ASSET:
      return true;
    default:
      return false;
  }
}

/**
 * @brief Tests whether a message type has a notification form.
 *
 * @param message_type Candidate message type.
 * @return True for a notification message.
 */
static bool message_type_is_notification(uint16_t message_type) {
  switch (message_type) {
    case MOONLIGHT_PROTOCOL_V1_MESSAGE_GOAWAY:
    case MOONLIGHT_PROTOCOL_V1_MESSAGE_APPLICATION_STATE:
    case MOONLIGHT_PROTOCOL_V1_MESSAGE_SESSION_STATE:
    case MOONLIGHT_PROTOCOL_V1_MESSAGE_MEDIA_EPOCH_PREPARE:
    case MOONLIGHT_PROTOCOL_V1_MESSAGE_VIDEO_TARGET:
    case MOONLIGHT_PROTOCOL_V1_MESSAGE_INPUT_RELIABLE:
      return true;
    default:
      return false;
  }
}

bool MoonlightProtocolV1MessageTypeIsKnown(uint16_t message_type) {
  return message_type_is_request_response(message_type) ||
         message_type_is_notification(message_type);
}

/**
 * @brief Validates one host-order lane preface.
 *
 * @param preface Candidate lane preface.
 * @return The codec result.
 */
static MoonlightProtocolResult validate_lane_preface(
  const MoonlightProtocolV1LanePreface *preface
) {
  if (preface == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }

  switch (preface->kind) {
    case MOONLIGHT_PROTOCOL_V1_LANE_PAIR_CONTROL:
    case MOONLIGHT_PROTOCOL_V1_LANE_CONTROL:
    case MOONLIGHT_PROTOCOL_V1_LANE_BULK:
      return preface->session_wire_id == 0 ?
               MOONLIGHT_PROTOCOL_RESULT_OK :
               MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH;
    case MOONLIGHT_PROTOCOL_V1_LANE_RELIABLE_INPUT:
      return preface->session_wire_id != 0 ?
               MOONLIGHT_PROTOCOL_RESULT_OK :
               MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH;
    default:
      return MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED;
  }
}

MoonlightProtocolResult MoonlightProtocolV1EncodeLanePreface(
  const MoonlightProtocolV1LanePreface *preface,
  uint8_t *output,
  size_t output_size
) {
  uint8_t encoded[MOONLIGHT_PROTOCOL_V1_LANE_PREFACE_SIZE] = {0};
  MoonlightProtocolResult result;

  if (output == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  result = validate_lane_preface(preface);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (output_size < sizeof(encoded)) {
    return MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL;
  }

  store_u32(encoded, LANE_PREFACE_MAGIC);
  encoded[4] = (uint8_t) preface->kind;
  store_u16(encoded + 6u, MOONLIGHT_PROTOCOL_V1_LANE_PREFACE_SIZE);
  store_u32(encoded + 8u, preface->session_wire_id);
  memcpy(output, encoded, sizeof(encoded));
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

MoonlightProtocolResult MoonlightProtocolV1DecodeLanePreface(
  const uint8_t *input,
  size_t input_size,
  MoonlightProtocolV1LanePreface *preface
) {
  MoonlightProtocolV1LanePreface decoded;
  MoonlightProtocolResult result;

  if (input == NULL || preface == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (input_size < MOONLIGHT_PROTOCOL_V1_LANE_PREFACE_SIZE) {
    return MOONLIGHT_PROTOCOL_RESULT_TRUNCATED;
  }
  if (load_u32(input) != LANE_PREFACE_MAGIC || load_u16(input + 6u) != MOONLIGHT_PROTOCOL_V1_LANE_PREFACE_SIZE || load_u32(input + 12u) != 0) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  if (input[5] != 0) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }

  decoded.kind = (MoonlightProtocolV1LaneKind) input[4];
  decoded.session_wire_id = load_u32(input + 8u);
  result = validate_lane_preface(&decoded);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }

  *preface = decoded;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Converts one host-order message type without narrowing it.
 *
 * @param message_type Candidate host-order message type.
 * @param wire_type Receives the 16-bit wire value on success.
 * @return The codec result.
 */
static MoonlightProtocolResult message_type_to_wire(
  MoonlightProtocolV1MessageType message_type,
  uint16_t *wire_type
) {
  const int64_t candidate = (int64_t) message_type;

  if (candidate < 0 || candidate > UINT16_MAX) {
    return MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED;
  }

  *wire_type = (uint16_t) candidate;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Validates the type-independent reliable-envelope tuple.
 *
 * @param envelope Candidate envelope.
 * @param payload_limit Explicit active payload limit.
 * @return The codec result.
 */
static MoonlightProtocolResult validate_envelope_tuple(
  const MoonlightProtocolV1MessageEnvelope *envelope,
  uint32_t payload_limit
) {
  if (envelope == NULL || payload_limit > MOONLIGHT_PROTOCOL_V1_BULK_PAYLOAD_MAX) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (envelope->payload_length > payload_limit) {
    return MOONLIGHT_PROTOCOL_RESULT_LIMIT_EXCEEDED;
  }
  if ((envelope->flags & (uint16_t) ~ENVELOPE_FLAG_MASK) != 0) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }

  switch (envelope->flags) {
    case 0:
      return envelope->status == MOONLIGHT_PROTOCOL_V1_STATUS_OK &&
                 envelope->correlation_id != 0 ?
               MOONLIGHT_PROTOCOL_RESULT_OK :
               MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
    case MOONLIGHT_PROTOCOL_V1_ENVELOPE_FLAG_RESPONSE:
      return envelope->status == MOONLIGHT_PROTOCOL_V1_STATUS_OK &&
                 envelope->correlation_id != 0 ?
               MOONLIGHT_PROTOCOL_RESULT_OK :
               MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
    case MOONLIGHT_PROTOCOL_V1_ENVELOPE_FLAG_RESPONSE |
      MOONLIGHT_PROTOCOL_V1_ENVELOPE_FLAG_ERROR:
      if (envelope->status <= MOONLIGHT_PROTOCOL_V1_STATUS_OK) {
        return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
      }
      if (envelope->status > MOONLIGHT_PROTOCOL_V1_STATUS_SHUTTING_DOWN) {
        return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
      }
      return envelope->correlation_id != 0 ?
               MOONLIGHT_PROTOCOL_RESULT_OK :
               MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
    case MOONLIGHT_PROTOCOL_V1_ENVELOPE_FLAG_NOTIFICATION:
      return envelope->status == MOONLIGHT_PROTOCOL_V1_STATUS_OK &&
                 envelope->correlation_id == 0 ?
               MOONLIGHT_PROTOCOL_RESULT_OK :
               MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
    default:
      return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
}

/**
 * @brief Validates one known host-order message envelope.
 *
 * @param envelope Candidate envelope.
 * @param payload_limit Explicit active payload limit.
 * @return The codec result.
 */
static MoonlightProtocolResult validate_message_envelope(
  const MoonlightProtocolV1MessageEnvelope *envelope,
  uint32_t payload_limit
) {
  MoonlightProtocolResult result;
  uint16_t message_type;

  result = validate_envelope_tuple(envelope, payload_limit);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  result = message_type_to_wire(envelope->message_type, &message_type);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (!MoonlightProtocolV1MessageTypeIsKnown(message_type)) {
    return MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED;
  }

  if (envelope->flags == MOONLIGHT_PROTOCOL_V1_ENVELOPE_FLAG_NOTIFICATION) {
    return message_type_is_notification(message_type) ?
             MOONLIGHT_PROTOCOL_RESULT_OK :
             MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH;
  }
  return message_type_is_request_response(message_type) ?
           MOONLIGHT_PROTOCOL_RESULT_OK :
           MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH;
}

MoonlightProtocolResult MoonlightProtocolV1EncodeMessageEnvelope(
  const MoonlightProtocolV1MessageEnvelope *envelope,
  uint32_t payload_limit,
  uint8_t *output,
  size_t output_size
) {
  uint8_t encoded[MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE];
  MoonlightProtocolResult result;

  if (output == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  result = validate_message_envelope(envelope, payload_limit);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (output_size < sizeof(encoded)) {
    return MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL;
  }

  store_u32(encoded, MESSAGE_ENVELOPE_MAGIC);
  store_u16(encoded + 4u, (uint16_t) envelope->message_type);
  store_u16(encoded + 6u, envelope->flags);
  store_u32(encoded + 8u, envelope->payload_length);
  store_u32(encoded + 12u, (uint32_t) envelope->status);
  store_u64(encoded + 16u, envelope->correlation_id);
  memcpy(output, encoded, sizeof(encoded));
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Decodes an envelope while accepting an unknown message registry type.
 *
 * This internal form still validates the magic, payload limit, and complete
 * flags/status/correlation tuple. It lets the stream parser safely drain an
 * unsupported but length-bounded request before the caller responds.
 *
 * @param input Bytes beginning with a complete envelope.
 * @param input_size Available bytes in `input`.
 * @param payload_limit Explicit legal payload maximum.
 * @param envelope Receives host-order values only on success.
 * @return The codec result.
 */
static MoonlightProtocolResult decode_message_envelope_structural(
  const uint8_t *input,
  size_t input_size,
  uint32_t payload_limit,
  MoonlightProtocolV1MessageEnvelope *envelope
) {
  MoonlightProtocolV1MessageEnvelope decoded;
  MoonlightProtocolResult result;

  if (input == NULL || envelope == NULL || payload_limit > MOONLIGHT_PROTOCOL_V1_BULK_PAYLOAD_MAX) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (input_size < MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE) {
    return MOONLIGHT_PROTOCOL_RESULT_TRUNCATED;
  }
  if (load_u32(input) != MESSAGE_ENVELOPE_MAGIC) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }

  decoded.message_type = (MoonlightProtocolV1MessageType) load_u16(input + 4u);
  decoded.flags = load_u16(input + 6u);
  decoded.payload_length = load_u32(input + 8u);
  decoded.status = (MoonlightProtocolV1MessageStatus) load_u32(input + 12u);
  decoded.correlation_id = load_u64(input + 16u);
  result = validate_envelope_tuple(&decoded, payload_limit);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }

  *envelope = decoded;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

MoonlightProtocolResult MoonlightProtocolV1DecodeMessageEnvelope(
  const uint8_t *input,
  size_t input_size,
  uint32_t payload_limit,
  MoonlightProtocolV1MessageEnvelope *envelope
) {
  MoonlightProtocolV1MessageEnvelope decoded;
  MoonlightProtocolResult result;

  if (envelope == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  result = decode_message_envelope_structural(
    input,
    input_size,
    payload_limit,
    &decoded
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  result = validate_message_envelope(&decoded, payload_limit);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }

  *envelope = decoded;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Validates one host-order TLV field header.
 *
 * @param field Candidate field header.
 * @return The codec result.
 */
static MoonlightProtocolResult validate_tlv_field(
  const MoonlightProtocolV1TlvField *field
) {
  if (field == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if ((field->flags & (uint16_t) ~TLV_FLAG_MASK) != 0) {
    return MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED;
  }
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

MoonlightProtocolResult MoonlightProtocolV1EncodeTlvFieldHeader(
  const MoonlightProtocolV1TlvField *field,
  uint8_t *output,
  size_t output_size
) {
  uint8_t encoded[MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE];
  MoonlightProtocolResult result;

  if (output == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  result = validate_tlv_field(field);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (output_size < sizeof(encoded)) {
    return MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL;
  }

  store_u16(encoded, field->field_id);
  store_u16(encoded + 2u, field->flags);
  store_u32(encoded + 4u, field->field_length);
  memcpy(output, encoded, sizeof(encoded));
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

MoonlightProtocolResult MoonlightProtocolV1DecodeTlvFieldHeader(
  const uint8_t *input,
  size_t input_size,
  MoonlightProtocolV1TlvField *field
) {
  MoonlightProtocolV1TlvField decoded;
  MoonlightProtocolResult result;

  if (input == NULL || field == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (input_size < MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE) {
    return MOONLIGHT_PROTOCOL_RESULT_TRUNCATED;
  }

  decoded.field_id = load_u16(input);
  decoded.flags = load_u16(input + 2u);
  decoded.field_length = load_u32(input + 4u);
  result = validate_tlv_field(&decoded);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }

  *field = decoded;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Tests whether an envelope is a request.
 *
 * @param envelope Structurally valid envelope.
 * @return True for the request tuple.
 */
static bool envelope_is_request(
  const MoonlightProtocolV1MessageEnvelope *envelope
) {
  return envelope->flags == 0;
}

/**
 * @brief Tests whether an envelope is a successful or error response.
 *
 * @param envelope Structurally valid envelope.
 * @return True for either response tuple.
 */
static bool envelope_is_response(
  const MoonlightProtocolV1MessageEnvelope *envelope
) {
  return envelope->flags == MOONLIGHT_PROTOCOL_V1_ENVELOPE_FLAG_RESPONSE ||
         envelope->flags == (MOONLIGHT_PROTOCOL_V1_ENVELOPE_FLAG_RESPONSE |
                             MOONLIGHT_PROTOCOL_V1_ENVELOPE_FLAG_ERROR);
}

/**
 * @brief Tests whether an unknown request can be answered on one stream half.
 *
 * @param lane Accepted lane kind.
 * @param reverse Whether this is the acceptor-to-initiator half.
 * @return True when a request tuple is legal on that half.
 */
static bool lane_accepts_unknown_request(
  MoonlightProtocolV1LaneKind lane,
  bool reverse
) {
  switch (lane) {
    case MOONLIGHT_PROTOCOL_V1_LANE_PAIR_CONTROL:
    case MOONLIGHT_PROTOCOL_V1_LANE_BULK:
      return !reverse;
    case MOONLIGHT_PROTOCOL_V1_LANE_CONTROL:
      return true;
    case MOONLIGHT_PROTOCOL_V1_LANE_RELIABLE_INPUT:
    default:
      return false;
  }
}

/**
 * @brief Tests whether a known message form is legal on one stream half.
 *
 * @param lane Accepted lane kind.
 * @param reverse Whether this is the acceptor-to-initiator half.
 * @param envelope Known and structurally valid envelope.
 * @return The codec result.
 */
static MoonlightProtocolResult validate_message_lane(
  MoonlightProtocolV1LaneKind lane,
  bool reverse,
  const MoonlightProtocolV1MessageEnvelope *envelope
) {
  switch (lane) {
    case MOONLIGHT_PROTOCOL_V1_LANE_PAIR_CONTROL:
      if (envelope->message_type != MOONLIGHT_PROTOCOL_V1_MESSAGE_CLIENT_PROOF && envelope->message_type != MOONLIGHT_PROTOCOL_V1_MESSAGE_PAIR_REQUEST) {
        return MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH;
      }
      break;
    case MOONLIGHT_PROTOCOL_V1_LANE_CONTROL:
      if (envelope->message_type == MOONLIGHT_PROTOCOL_V1_MESSAGE_PAIR_REQUEST || envelope->message_type == MOONLIGHT_PROTOCOL_V1_MESSAGE_INPUT_RELIABLE || envelope->message_type == MOONLIGHT_PROTOCOL_V1_MESSAGE_GET_ASSET) {
        return MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH;
      }
      if (envelope->message_type == MOONLIGHT_PROTOCOL_V1_MESSAGE_PING) {
        return MOONLIGHT_PROTOCOL_RESULT_OK;
      }
      if (envelope->flags == MOONLIGHT_PROTOCOL_V1_ENVELOPE_FLAG_NOTIFICATION) {
        return reverse ?
                 MOONLIGHT_PROTOCOL_RESULT_OK :
                 MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH;
      }
      break;
    case MOONLIGHT_PROTOCOL_V1_LANE_RELIABLE_INPUT:
      return !reverse &&
                 envelope->message_type == MOONLIGHT_PROTOCOL_V1_MESSAGE_INPUT_RELIABLE &&
                 envelope->flags == MOONLIGHT_PROTOCOL_V1_ENVELOPE_FLAG_NOTIFICATION ?
               MOONLIGHT_PROTOCOL_RESULT_OK :
               MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH;
    case MOONLIGHT_PROTOCOL_V1_LANE_BULK:
      if (envelope->message_type != MOONLIGHT_PROTOCOL_V1_MESSAGE_GET_ASSET) {
        return MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH;
      }
      break;
    default:
      return MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED;
  }

  return (reverse && envelope_is_response(envelope)) ||
             (!reverse && envelope_is_request(envelope)) ?
           MOONLIGHT_PROTOCOL_RESULT_OK :
           MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH;
}

/**
 * @brief Returns the immutable protocol payload ceiling for one lane.
 *
 * @param lane Known lane kind.
 * @return Maximum envelope payload in bytes.
 */
static uint32_t lane_payload_limit(MoonlightProtocolV1LaneKind lane) {
  switch (lane) {
    case MOONLIGHT_PROTOCOL_V1_LANE_PAIR_CONTROL:
      return MOONLIGHT_PROTOCOL_V1_PAIR_CONTROL_PAYLOAD_MAX;
    case MOONLIGHT_PROTOCOL_V1_LANE_CONTROL:
      return MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX;
    case MOONLIGHT_PROTOCOL_V1_LANE_RELIABLE_INPUT:
      return MOONLIGHT_PROTOCOL_V1_RELIABLE_INPUT_PAYLOAD_MAX;
    case MOONLIGHT_PROTOCOL_V1_LANE_BULK:
      return MOONLIGHT_PROTOCOL_V1_BULK_PAYLOAD_MAX;
    default:
      return 0;
  }
}

/**
 * @brief Combines the immutable lane ceiling with the active state limit.
 *
 * @param parser Parser holding both limits.
 * @return Effective maximum envelope payload.
 */
static uint32_t stream_payload_limit(
  const MoonlightProtocolV1StreamParser *parser
) {
  const uint32_t lane_limit = lane_payload_limit(parser->lane.kind);

  return parser->payload_limit < lane_limit ?
           parser->payload_limit :
           lane_limit;
}

/**
 * @brief Makes one empty stream event.
 *
 * @return Zeroed event with type NONE.
 */
static MoonlightProtocolV1StreamEvent empty_stream_event(void) {
  MoonlightProtocolV1StreamEvent event;

  memset(&event, 0, sizeof(event));
  event.type = MOONLIGHT_PROTOCOL_V1_STREAM_EVENT_NONE;
  return event;
}

MoonlightProtocolResult MoonlightProtocolV1StreamParserInitialize(
  MoonlightProtocolV1StreamParser *parser,
  uint32_t payload_limit
) {
  MoonlightProtocolV1StreamParser initialized;

  if (parser == NULL || payload_limit > MOONLIGHT_PROTOCOL_V1_BULK_PAYLOAD_MAX) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }

  memset(&initialized, 0, sizeof(initialized));
  initialized.payload_limit = payload_limit;
  initialized.terminal_result = MOONLIGHT_PROTOCOL_RESULT_OK;
  initialized.state = STREAM_STATE_PREFACE;
  *parser = initialized;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

MoonlightProtocolResult MoonlightProtocolV1StreamParserInitializeReverse(
  MoonlightProtocolV1StreamParser *parser,
  const MoonlightProtocolV1LanePreface *lane,
  uint32_t payload_limit
) {
  MoonlightProtocolV1StreamParser initialized;
  MoonlightProtocolResult result;

  if (parser == NULL || payload_limit > MOONLIGHT_PROTOCOL_V1_BULK_PAYLOAD_MAX) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  result = validate_lane_preface(lane);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (lane->kind == MOONLIGHT_PROTOCOL_V1_LANE_RELIABLE_INPUT) {
    return MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH;
  }

  memset(&initialized, 0, sizeof(initialized));
  initialized.payload_limit = payload_limit;
  initialized.lane = *lane;
  initialized.terminal_result = MOONLIGHT_PROTOCOL_RESULT_OK;
  initialized.state = STREAM_STATE_ENVELOPE;
  initialized.reverse = true;
  *parser = initialized;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Copies available fixed-header bytes into a stream parser.
 *
 * @param parser Parser receiving header bytes.
 * @param required_size Complete header size.
 * @param input Next stream bytes.
 * @param input_size Available bytes.
 * @return Number of bytes copied.
 */
static size_t stream_copy_header(
  MoonlightProtocolV1StreamParser *parser,
  size_t required_size,
  const uint8_t *input,
  size_t input_size
) {
  const size_t needed = required_size - parser->header_size;
  const size_t copied = input_size < needed ? input_size : needed;

  if (copied != 0) {
    memcpy(parser->header_bytes + parser->header_size, input, copied);
    parser->header_size += copied;
  }
  return copied;
}

/**
 * @brief Atomically makes a stream parse failure sticky.
 *
 * @param parser Parser entering its failed phase.
 * @param result Non-OK wire result.
 * @return The supplied result.
 */
static MoonlightProtocolResult fail_stream_parser(
  MoonlightProtocolV1StreamParser *parser,
  MoonlightProtocolResult result
) {
  parser->terminal_result = result;
  parser->state = STREAM_STATE_FAILED;
  return result;
}

MoonlightProtocolResult MoonlightProtocolV1StreamParserFeed(
  MoonlightProtocolV1StreamParser *parser,
  const uint8_t *input,
  size_t input_size,
  size_t *consumed,
  MoonlightProtocolV1StreamEvent *event
) {
  MoonlightProtocolV1StreamEvent produced;
  MoonlightProtocolResult result;
  size_t used = 0;

  if (parser == NULL || consumed == NULL || event == NULL || (input == NULL && input_size != 0) || parser->state < STREAM_STATE_PREFACE || parser->state > STREAM_STATE_FAILED) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }

  produced = empty_stream_event();
  if (parser->terminal_result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    *consumed = 0;
    *event = produced;
    return parser->terminal_result;
  }

  if (parser->state == STREAM_STATE_PREFACE) {
    used = stream_copy_header(
      parser,
      MOONLIGHT_PROTOCOL_V1_LANE_PREFACE_SIZE,
      input,
      input_size
    );
    if (parser->header_size == MOONLIGHT_PROTOCOL_V1_LANE_PREFACE_SIZE) {
      MoonlightProtocolV1LanePreface decoded;

      result = MoonlightProtocolV1DecodeLanePreface(
        parser->header_bytes,
        parser->header_size,
        &decoded
      );
      if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
        *consumed = used;
        *event = produced;
        return fail_stream_parser(parser, result);
      }
      parser->lane = decoded;
      parser->header_size = 0;
      parser->state = STREAM_STATE_ENVELOPE;
      produced.type = MOONLIGHT_PROTOCOL_V1_STREAM_EVENT_LANE_PREFACE;
      produced.lane = decoded;
    }
  } else if (parser->state == STREAM_STATE_ENVELOPE) {
    used = stream_copy_header(
      parser,
      MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE,
      input,
      input_size
    );
    if (parser->header_size == MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE) {
      MoonlightProtocolV1MessageEnvelope decoded;

      result = decode_message_envelope_structural(
        parser->header_bytes,
        parser->header_size,
        stream_payload_limit(parser),
        &decoded
      );
      if (result == MOONLIGHT_PROTOCOL_RESULT_OK) {
        if (MoonlightProtocolV1MessageTypeIsKnown((uint16_t) decoded.message_type)) {
          result = validate_message_envelope(
            &decoded,
            stream_payload_limit(parser)
          );
          if (result == MOONLIGHT_PROTOCOL_RESULT_OK) {
            result = validate_message_lane(
              parser->lane.kind,
              parser->reverse,
              &decoded
            );
          }
          parser->message_result = MOONLIGHT_PROTOCOL_RESULT_OK;
        } else if (envelope_is_request(&decoded) && lane_accepts_unknown_request(parser->lane.kind, parser->reverse)) {
          parser->message_result = MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED;
        } else {
          result = MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED;
        }
      }
      if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
        *consumed = used;
        *event = produced;
        return fail_stream_parser(parser, result);
      }
      parser->envelope = decoded;
      parser->payload_remaining = decoded.payload_length;
      parser->header_size = 0;
      parser->state = decoded.payload_length == 0 ?
                        STREAM_STATE_ENVELOPE_END :
                        STREAM_STATE_PAYLOAD;
      produced.type = MOONLIGHT_PROTOCOL_V1_STREAM_EVENT_ENVELOPE_BEGIN;
      produced.lane = parser->lane;
      produced.envelope = decoded;
      produced.message_result = parser->message_result;
    }
  } else if (parser->state == STREAM_STATE_PAYLOAD) {
    const size_t emitted =
      input_size < parser->payload_remaining ?
        input_size :
        parser->payload_remaining;

    if (emitted != 0) {
      parser->payload_remaining -= (uint32_t) emitted;
      if (parser->payload_remaining == 0) {
        parser->state = STREAM_STATE_ENVELOPE_END;
      }
      used = emitted;
      produced.type = MOONLIGHT_PROTOCOL_V1_STREAM_EVENT_PAYLOAD;
      produced.lane = parser->lane;
      produced.envelope = parser->envelope;
      produced.message_result = parser->message_result;
      produced.payload = input;
      produced.payload_size = emitted;
    }
  } else if (parser->state == STREAM_STATE_ENVELOPE_END) {
    parser->state = STREAM_STATE_ENVELOPE;
    produced.type = MOONLIGHT_PROTOCOL_V1_STREAM_EVENT_ENVELOPE_END;
    produced.lane = parser->lane;
    produced.envelope = parser->envelope;
    produced.message_result = parser->message_result;
  }

  *consumed = used;
  *event = produced;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

MoonlightProtocolResult MoonlightProtocolV1StreamParserFinish(
  const MoonlightProtocolV1StreamParser *parser
) {
  if (parser == NULL || parser->state < STREAM_STATE_PREFACE || parser->state > STREAM_STATE_FAILED) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (parser->terminal_result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return parser->terminal_result;
  }
  if ((parser->state == STREAM_STATE_ENVELOPE && parser->header_size == 0) || parser->state == STREAM_STATE_ENVELOPE_END) {
    return MOONLIGHT_PROTOCOL_RESULT_OK;
  }
  return MOONLIGHT_PROTOCOL_RESULT_TRUNCATED;
}

/**
 * @brief Makes one empty TLV event.
 *
 * @return Zeroed event with type NONE.
 */
static MoonlightProtocolV1TlvEvent empty_tlv_event(void) {
  MoonlightProtocolV1TlvEvent event;

  memset(&event, 0, sizeof(event));
  event.type = MOONLIGHT_PROTOCOL_V1_TLV_EVENT_NONE;
  return event;
}

MoonlightProtocolResult MoonlightProtocolV1TlvParserInitialize(
  MoonlightProtocolV1TlvParser *parser,
  uint32_t payload_length,
  uint8_t depth
) {
  MoonlightProtocolV1TlvParser initialized;

  if (parser == NULL || depth > MOONLIGHT_PROTOCOL_V1_TLV_DEPTH_MAX) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (payload_length > MOONLIGHT_PROTOCOL_V1_BULK_PAYLOAD_MAX) {
    return MOONLIGHT_PROTOCOL_RESULT_LIMIT_EXCEEDED;
  }

  memset(&initialized, 0, sizeof(initialized));
  initialized.container_remaining = payload_length;
  initialized.terminal_result = MOONLIGHT_PROTOCOL_RESULT_OK;
  initialized.depth = depth;
  initialized.state = payload_length == 0 ?
                        TLV_STATE_COMPLETE :
                        TLV_STATE_HEADER;
  *parser = initialized;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Atomically makes a TLV parse failure sticky.
 *
 * @param parser Parser entering its failed phase.
 * @param result Non-OK validation result.
 * @return The supplied result.
 */
static MoonlightProtocolResult fail_tlv_parser(
  MoonlightProtocolV1TlvParser *parser,
  MoonlightProtocolResult result
) {
  parser->terminal_result = result;
  parser->state = TLV_STATE_FAILED;
  return result;
}

/**
 * @brief Returns the occurrence limit for one TLV depth.
 *
 * @param depth Top-level zero or nested depth.
 * @return Maximum field occurrences.
 */
static size_t tlv_field_limit(uint8_t depth) {
  return depth == 0 ?
           MOONLIGHT_PROTOCOL_V1_TLV_TOP_LEVEL_FIELD_MAX :
           MOONLIGHT_PROTOCOL_V1_TLV_NESTED_FIELD_MAX;
}

MoonlightProtocolResult MoonlightProtocolV1TlvParserFeed(
  MoonlightProtocolV1TlvParser *parser,
  const uint8_t *input,
  size_t input_size,
  size_t *consumed,
  MoonlightProtocolV1TlvEvent *event
) {
  MoonlightProtocolV1TlvEvent produced;
  size_t used = 0;

  if (parser == NULL || consumed == NULL || event == NULL || (input == NULL && input_size != 0) || parser->state < TLV_STATE_HEADER || parser->state > TLV_STATE_FAILED) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }

  produced = empty_tlv_event();
  if (parser->terminal_result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    *consumed = 0;
    *event = produced;
    return parser->terminal_result;
  }

  if (parser->state == TLV_STATE_COMPLETE) {
    *consumed = 0;
    *event = produced;
    return MOONLIGHT_PROTOCOL_RESULT_OK;
  }

  if (parser->state == TLV_STATE_HEADER) {
    const size_t needed =
      MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE - parser->header_size;
    const size_t available =
      input_size < parser->container_remaining ?
        input_size :
        parser->container_remaining;
    const size_t copied = available < needed ? available : needed;

    if (parser->header_size + parser->container_remaining < MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE) {
      *consumed = 0;
      *event = produced;
      return fail_tlv_parser(parser, MOONLIGHT_PROTOCOL_RESULT_MALFORMED);
    }
    if (copied != 0) {
      memcpy(parser->header_bytes + parser->header_size, input, copied);
      parser->header_size += copied;
      parser->container_remaining -= (uint32_t) copied;
      used = copied;
    }

    if (parser->header_size == MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE) {
      MoonlightProtocolV1TlvField decoded;
      MoonlightProtocolResult result =
        MoonlightProtocolV1DecodeTlvFieldHeader(
          parser->header_bytes,
          parser->header_size,
          &decoded
        );

      if (result == MOONLIGHT_PROTOCOL_RESULT_OK && decoded.field_length > parser->container_remaining) {
        result = MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
      }
      if (result == MOONLIGHT_PROTOCOL_RESULT_OK && parser->field_count >= tlv_field_limit(parser->depth)) {
        result = MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
      }
      if (result == MOONLIGHT_PROTOCOL_RESULT_OK && parser->has_last_field && decoded.field_id < parser->last_field_id) {
        result = MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
      }
      if (result == MOONLIGHT_PROTOCOL_RESULT_OK && parser->has_last_field && decoded.field_id == parser->last_field_id && ((decoded.flags & MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED) == 0 || (parser->last_field_flags & MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED) == 0)) {
        result = MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
      }
      if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
        *consumed = used;
        *event = produced;
        return fail_tlv_parser(parser, result);
      }

      parser->field = decoded;
      parser->value_remaining = decoded.field_length;
      parser->last_field_id = decoded.field_id;
      parser->last_field_flags = decoded.flags;
      parser->has_last_field = true;
      ++parser->field_count;
      parser->header_size = 0;
      parser->state = decoded.field_length == 0 ?
                        TLV_STATE_FIELD_END :
                        TLV_STATE_VALUE;
      produced.type = MOONLIGHT_PROTOCOL_V1_TLV_EVENT_FIELD_BEGIN;
      produced.field = decoded;
    }
  } else if (parser->state == TLV_STATE_VALUE) {
    const size_t emitted =
      input_size < parser->value_remaining ?
        input_size :
        parser->value_remaining;

    if (emitted != 0) {
      parser->container_remaining -= (uint32_t) emitted;
      parser->value_remaining -= (uint32_t) emitted;
      if (parser->value_remaining == 0) {
        parser->state = TLV_STATE_FIELD_END;
      }
      used = emitted;
      produced.type = MOONLIGHT_PROTOCOL_V1_TLV_EVENT_VALUE;
      produced.field = parser->field;
      produced.value = input;
      produced.value_size = emitted;
    }
  } else if (parser->state == TLV_STATE_FIELD_END) {
    parser->state = parser->container_remaining == 0 ?
                      TLV_STATE_COMPLETE :
                      TLV_STATE_HEADER;
    produced.type = MOONLIGHT_PROTOCOL_V1_TLV_EVENT_FIELD_END;
    produced.field = parser->field;
  }

  *consumed = used;
  *event = produced;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

bool MoonlightProtocolV1TlvParserIsComplete(
  const MoonlightProtocolV1TlvParser *parser
) {
  if (parser == NULL || parser->terminal_result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return false;
  }
  return parser->state == TLV_STATE_COMPLETE ||
         (parser->state == TLV_STATE_FIELD_END &&
          parser->container_remaining == 0);
}

MoonlightProtocolResult MoonlightProtocolV1TlvParserFinish(
  const MoonlightProtocolV1TlvParser *parser
) {
  if (parser == NULL || parser->state < TLV_STATE_HEADER || parser->state > TLV_STATE_FAILED) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (parser->terminal_result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return parser->terminal_result;
  }
  return MoonlightProtocolV1TlvParserIsComplete(parser) ?
           MOONLIGHT_PROTOCOL_RESULT_OK :
           MOONLIGHT_PROTOCOL_RESULT_TRUNCATED;
}

MoonlightProtocolResult MoonlightProtocolV1TlvWriterInitialize(
  MoonlightProtocolV1TlvWriter *writer,
  uint8_t *output,
  size_t output_capacity,
  uint8_t depth
) {
  MoonlightProtocolV1TlvWriter initialized;

  if (writer == NULL || (output == NULL && output_capacity != 0) || output_capacity > MOONLIGHT_PROTOCOL_V1_BULK_PAYLOAD_MAX || depth > MOONLIGHT_PROTOCOL_V1_TLV_DEPTH_MAX) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }

  memset(&initialized, 0, sizeof(initialized));
  initialized.output = output;
  initialized.output_capacity = output_capacity;
  initialized.depth = depth;
  *writer = initialized;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

MoonlightProtocolResult MoonlightProtocolV1TlvWriterAppend(
  MoonlightProtocolV1TlvWriter *writer,
  uint16_t field_id,
  uint16_t flags,
  const uint8_t *value,
  size_t value_size
) {
  uint8_t encoded_header[MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE];
  MoonlightProtocolV1TlvField field;
  MoonlightProtocolResult result;
  size_t required;

  if (writer == NULL || (value == NULL && value_size != 0) || writer->depth > MOONLIGHT_PROTOCOL_V1_TLV_DEPTH_MAX || writer->output_capacity > MOONLIGHT_PROTOCOL_V1_BULK_PAYLOAD_MAX || (writer->output == NULL && writer->output_capacity != 0) || writer->output_size > writer->output_capacity) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (value_size > UINT32_MAX || value_size > MOONLIGHT_PROTOCOL_V1_BULK_PAYLOAD_MAX) {
    return MOONLIGHT_PROTOCOL_RESULT_LIMIT_EXCEEDED;
  }

  field.field_id = field_id;
  field.flags = flags;
  field.field_length = (uint32_t) value_size;
  result = MoonlightProtocolV1EncodeTlvFieldHeader(
    &field,
    encoded_header,
    sizeof(encoded_header)
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (writer->field_count >= tlv_field_limit(writer->depth) || (writer->has_last_field && field_id < writer->last_field_id) || (writer->has_last_field && field_id == writer->last_field_id && ((flags & MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED) == 0 || (writer->last_field_flags & MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED) == 0))) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }

  required = MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + value_size;
  if (required > writer->output_capacity - writer->output_size) {
    return MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL;
  }

  if (value_size != 0) {
    memmove(
      writer->output + writer->output_size + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
      value,
      value_size
    );
  }
  memcpy(
    writer->output + writer->output_size,
    encoded_header,
    sizeof(encoded_header)
  );
  writer->output_size += required;
  writer->last_field_id = field_id;
  writer->last_field_flags = flags;
  writer->has_last_field = true;
  ++writer->field_count;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

size_t MoonlightProtocolV1TlvWriterSize(
  const MoonlightProtocolV1TlvWriter *writer
) {
  return writer == NULL ? 0 : writer->output_size;
}
