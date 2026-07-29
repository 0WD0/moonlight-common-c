/**
 * @file control.c
 * @brief Implements protocol version 1 Control Lane message schemas.
 */

#include <moonlight/protocol/control.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/**
 * @brief Encoded length of the shortest CLIENT_HELLO request payload.
 */
#define CLIENT_HELLO_REQUEST_PAYLOAD_MIN 35u

/**
 * @brief Number of fields in a CLIENT_HELLO request.
 */
#define CLIENT_HELLO_REQUEST_FIELD_COUNT 3u

/**
 * @brief Number of fields in a CLIENT_HELLO response.
 */
#define CLIENT_HELLO_RESPONSE_FIELD_COUNT 5u

/**
 * @brief Number of required fields in a successful GET_HOST_INFO response.
 */
#define HOST_INFO_RESPONSE_FIELD_COUNT 10u

/**
 * @brief Stores one network-order 16-bit integer.
 *
 * @param output Two writable bytes.
 * @param value Host-order value.
 */
static void control_store_u16(uint8_t *output, uint16_t value) {
  output[0] = (uint8_t) (value >> 8u);
  output[1] = (uint8_t) value;
}

/**
 * @brief Stores one network-order 32-bit integer.
 *
 * @param output Four writable bytes.
 * @param value Host-order value.
 */
static void control_store_u32(uint8_t *output, uint32_t value) {
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
static void control_store_u64(uint8_t *output, uint64_t value) {
  control_store_u32(output, (uint32_t) (value >> 32u));
  control_store_u32(output + 4u, (uint32_t) value);
}

/**
 * @brief Loads one network-order 16-bit integer.
 *
 * @param input Two readable bytes.
 * @return Host-order value.
 */
static uint16_t control_load_u16(const uint8_t *input) {
  return (uint16_t) (((uint16_t) input[0] << 8u) | input[1]);
}

/**
 * @brief Loads one network-order 32-bit integer.
 *
 * @param input Four readable bytes.
 * @return Host-order value.
 */
static uint32_t control_load_u32(const uint8_t *input) {
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
static uint64_t control_load_u64(const uint8_t *input) {
  return ((uint64_t) control_load_u32(input) << 32u) |
         control_load_u32(input + 4u);
}

/**
 * @brief Tests whether bytes are a shortest-form Unicode scalar sequence.
 *
 * @param input Candidate UTF-8 bytes.
 * @param input_size Number of candidate bytes.
 * @return True only for canonical UTF-8 without NUL or surrogate code points.
 */
static bool control_utf8_is_valid(
  const uint8_t *input,
  size_t input_size
) {
  size_t offset = 0;

  while (offset < input_size) {
    const uint8_t first = input[offset];
    size_t continuation_count;
    uint32_t code_point;
    uint32_t minimum;
    size_t index;

    if (first <= 0x7fu) {
      if (first == 0) {
        return false;
      }
      ++offset;
      continue;
    }
    if (first >= 0xc2u && first <= 0xdfu) {
      continuation_count = 1;
      code_point = first & 0x1fu;
      minimum = 0x80u;
    } else if (first >= 0xe0u && first <= 0xefu) {
      continuation_count = 2;
      code_point = first & 0x0fu;
      minimum = 0x800u;
    } else if (first >= 0xf0u && first <= 0xf4u) {
      continuation_count = 3;
      code_point = first & 0x07u;
      minimum = 0x10000u;
    } else {
      return false;
    }
    if (continuation_count > input_size - offset - 1u) {
      return false;
    }
    for (index = 1; index <= continuation_count; ++index) {
      const uint8_t continuation = input[offset + index];

      if ((continuation & 0xc0u) != 0x80u) {
        return false;
      }
      code_point = (code_point << 6u) | (continuation & 0x3fu);
    }
    if (code_point < minimum || code_point > 0x10ffffu || (code_point >= 0xd800u && code_point <= 0xdfffu)) {
      return false;
    }
    offset += continuation_count + 1u;
  }
  return true;
}

/**
 * @brief Tests whether a fixed identifier contains at least one nonzero byte.
 *
 * @param input Candidate identifier bytes.
 * @param input_size Number of bytes to inspect.
 * @return True when the identifier is not the reserved all-zero value.
 */
static bool control_identifier_is_nonzero(
  const uint8_t *input,
  size_t input_size
) {
  size_t index;
  uint8_t combined = 0;

  for (index = 0; index < input_size; ++index) {
    combined |= input[index];
  }
  return combined != 0;
}

/**
 * @brief Validates one CLIENT_HELLO request.
 *
 * @param request Candidate request.
 * @return The codec result.
 */
static MoonlightProtocolResult validate_client_hello_request(
  const MoonlightProtocolV1ClientHelloRequest *request
) {
  if (request == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (request->protocol_minor != MOONLIGHT_PROTOCOL_V1_MINOR || (request->capability_bits & (uint64_t) ~MOONLIGHT_PROTOCOL_V1_CAPABILITY_MASK) != 0) {
    return MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH;
  }
  if (request->software_version_size == 0 || request->software_version_size > MOONLIGHT_PROTOCOL_V1_SOFTWARE_VERSION_MAX || !control_utf8_is_valid(request->software_version, request->software_version_size)) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Validates one successful CLIENT_HELLO response.
 *
 * @param response Candidate response.
 * @return The codec result.
 */
static MoonlightProtocolResult validate_client_hello_response(
  const MoonlightProtocolV1ClientHelloResponse *response
) {
  if (response == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (response->protocol_minor != MOONLIGHT_PROTOCOL_V1_MINOR || (response->capability_bits & (uint64_t) ~MOONLIGHT_PROTOCOL_V1_CAPABILITY_MASK) != 0) {
    return MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH;
  }
  if (response->maximum_control_payload == 0 || response->maximum_control_payload > MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX || response->maximum_bulk_payload == 0 || response->maximum_bulk_payload > MOONLIGHT_PROTOCOL_V1_BULK_PAYLOAD_MAX) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Validates one successful GET_HOST_INFO response.
 *
 * @param response Candidate response.
 * @return The codec result.
 */
static MoonlightProtocolResult validate_host_info_response(
  const MoonlightProtocolV1HostInfoResponse *response
) {
  uint64_t unknown_bits;

  if (response == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  unknown_bits =
    response->capability_bits &
    (uint64_t) ~MOONLIGHT_PROTOCOL_V1_CAPABILITY_MASK;
  if (unknown_bits != 0) {
    return MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH;
  }
  if (!control_identifier_is_nonzero(
        response->host_id,
        sizeof(response->host_id)
      )) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  if (response->display_name_size == 0) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  if (response->display_name_size > MOONLIGHT_PROTOCOL_V1_HOST_DISPLAY_NAME_MAX) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  if (!control_utf8_is_valid(
        response->display_name,
        response->display_name_size
      )) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  if (response->software_version_size == 0) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  if (response->software_version_size > MOONLIGHT_PROTOCOL_V1_SOFTWARE_VERSION_MAX) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  if (!control_utf8_is_valid(
        response->software_version,
        response->software_version_size
      )) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  if (response->configured_quic_port == 0) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  if (response->maximum_active_stream_sessions == 0) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  if (response->maximum_active_stream_sessions > MOONLIGHT_PROTOCOL_V1_ACTIVE_STREAM_SESSION_MAX) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  if (response->available_stream_session_slots > response->maximum_active_stream_sessions) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  unknown_bits =
    response->acl_permission_bits &
    (uint64_t) ~MOONLIGHT_PROTOCOL_V1_ACL_PERMISSION_MASK;
  if (unknown_bits != 0) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  if (response->authorization_generation == 0) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  switch (response->instance_visibility) {
    case MOONLIGHT_PROTOCOL_V1_INSTANCE_VISIBILITY_OWNER_ONLY:
    case MOONLIGHT_PROTOCOL_V1_INSTANCE_VISIBILITY_SHARED:
      return MOONLIGHT_PROTOCOL_RESULT_OK;
    default:
      return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
}

/**
 * @brief Appends one canonical scalar field to a fixed local buffer.
 *
 * @param output Destination positioned at the field header.
 * @param field_id Expected scalar field ID.
 * @param value Field bytes.
 * @param value_size Number of field bytes.
 * @return Number of encoded bytes.
 */
static size_t encode_scalar(
  uint8_t *output,
  uint16_t field_id,
  const uint8_t *value,
  size_t value_size
) {
  control_store_u16(output, field_id);
  control_store_u16(output + 2u, 0);
  control_store_u32(output + 4u, (uint32_t) value_size);
  memcpy(output + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE, value, value_size);
  return MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + value_size;
}

MoonlightProtocolResult MoonlightProtocolV1EncodeClientHelloRequest(
  const MoonlightProtocolV1ClientHelloRequest *request,
  uint8_t *output,
  size_t output_size,
  size_t *encoded_size
) {
  uint8_t encoded[MOONLIGHT_PROTOCOL_V1_CLIENT_HELLO_REQUEST_PAYLOAD_MAX];
  uint8_t protocol_minor[2];
  uint8_t capability_bits[8];
  MoonlightProtocolResult result;
  size_t size = 0;

  if (output == NULL || encoded_size == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  result = validate_client_hello_request(request);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }

  control_store_u16(protocol_minor, request->protocol_minor);
  control_store_u64(capability_bits, request->capability_bits);
  size += encode_scalar(
    encoded + size,
    1,
    protocol_minor,
    sizeof(protocol_minor)
  );
  size += encode_scalar(
    encoded + size,
    2,
    capability_bits,
    sizeof(capability_bits)
  );
  size += encode_scalar(
    encoded + size,
    3,
    request->software_version,
    request->software_version_size
  );
  if (output_size < size) {
    return MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL;
  }
  memcpy(output, encoded, size);
  *encoded_size = size;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Decodes one expected scalar TLV field from contiguous input.
 *
 * @param input Complete containing payload.
 * @param input_size Number of containing bytes.
 * @param offset Current field offset, advanced on success.
 * @param expected_id Exact expected field ID.
 * @param maximum_known_id Largest field ID in the selected schema.
 * @param value Receives a borrowed field value.
 * @param value_size Receives the field value size.
 * @return The codec result.
 */
static MoonlightProtocolResult decode_expected_scalar(
  const uint8_t *input,
  size_t input_size,
  size_t *offset,
  uint16_t expected_id,
  uint16_t maximum_known_id,
  const uint8_t **value,
  size_t *value_size
) {
  MoonlightProtocolV1TlvField field;
  MoonlightProtocolResult result;
  const size_t remaining = input_size - *offset;

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
  if (field.field_length > remaining - MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  if (field.field_id != expected_id) {
    return field.field_id >= 1u && field.field_id <= maximum_known_id ?
             MOONLIGHT_PROTOCOL_RESULT_MALFORMED :
             MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED;
  }
  if (field.flags != 0) {
    return MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED;
  }

  *value = input + *offset + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE;
  *value_size = field.field_length;
  *offset += MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + field.field_length;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Classifies bytes after every required scalar field.
 *
 * @param input Complete containing payload.
 * @param input_size Number of containing bytes.
 * @param offset First byte after the required schema.
 * @param maximum_known_id Largest field ID in the selected schema.
 * @return OK for exact consumption, MALFORMED for a known duplicate or broken
 * field, and UNSUPPORTED for a complete unknown field or flag.
 */
static MoonlightProtocolResult classify_trailing_field(
  const uint8_t *input,
  size_t input_size,
  size_t offset,
  uint16_t maximum_known_id
) {
  MoonlightProtocolV1TlvField field;
  MoonlightProtocolResult result;
  const size_t remaining = input_size - offset;

  if (remaining == 0) {
    return MOONLIGHT_PROTOCOL_RESULT_OK;
  }
  result = MoonlightProtocolV1DecodeTlvFieldHeader(
    input + offset,
    remaining,
    &field
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result == MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED ?
             result :
             MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  if (field.field_length > remaining - MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  return field.field_id >= 1u && field.field_id <= maximum_known_id ?
           MOONLIGHT_PROTOCOL_RESULT_MALFORMED :
           MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED;
}

MoonlightProtocolResult MoonlightProtocolV1DecodeClientHelloRequest(
  const uint8_t *input,
  size_t input_size,
  MoonlightProtocolV1ClientHelloRequest *request
) {
  MoonlightProtocolV1ClientHelloRequest decoded;
  const uint8_t *value;
  size_t value_size;
  size_t offset = 0;
  MoonlightProtocolResult result;

  if (input == NULL || request == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (input_size > MOONLIGHT_PROTOCOL_V1_CLIENT_HELLO_REQUEST_PAYLOAD_MAX) {
    return MOONLIGHT_PROTOCOL_RESULT_LIMIT_EXCEEDED;
  }
  if (input_size < CLIENT_HELLO_REQUEST_PAYLOAD_MIN) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }

  memset(&decoded, 0, sizeof(decoded));
  result = decode_expected_scalar(
    input,
    input_size,
    &offset,
    1,
    CLIENT_HELLO_REQUEST_FIELD_COUNT,
    &value,
    &value_size
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (value_size != 2u) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  decoded.protocol_minor = control_load_u16(value);

  result = decode_expected_scalar(
    input,
    input_size,
    &offset,
    2,
    CLIENT_HELLO_REQUEST_FIELD_COUNT,
    &value,
    &value_size
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (value_size != 8u) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  decoded.capability_bits = control_load_u64(value);

  result = decode_expected_scalar(
    input,
    input_size,
    &offset,
    3,
    CLIENT_HELLO_REQUEST_FIELD_COUNT,
    &value,
    &value_size
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  result = classify_trailing_field(
    input,
    input_size,
    offset,
    CLIENT_HELLO_REQUEST_FIELD_COUNT
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  memcpy(decoded.software_version, value, value_size);
  decoded.software_version_size = value_size;

  result = validate_client_hello_request(&decoded);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  *request = decoded;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

MoonlightProtocolResult MoonlightProtocolV1EncodeClientHelloResponse(
  const MoonlightProtocolV1ClientHelloResponse *response,
  uint8_t *output,
  size_t output_size,
  size_t *encoded_size
) {
  uint8_t encoded[MOONLIGHT_PROTOCOL_V1_CLIENT_HELLO_RESPONSE_PAYLOAD_SIZE];
  uint8_t protocol_minor[2];
  uint8_t capability_bits[8];
  uint8_t maximum_control_payload[4];
  uint8_t maximum_bulk_payload[4];
  MoonlightProtocolResult result;
  size_t size = 0;

  if (output == NULL || encoded_size == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  result = validate_client_hello_response(response);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (output_size < sizeof(encoded)) {
    return MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL;
  }

  control_store_u16(protocol_minor, response->protocol_minor);
  control_store_u64(capability_bits, response->capability_bits);
  control_store_u32(
    maximum_control_payload,
    response->maximum_control_payload
  );
  control_store_u32(maximum_bulk_payload, response->maximum_bulk_payload);
  size += encode_scalar(
    encoded + size,
    1,
    protocol_minor,
    sizeof(protocol_minor)
  );
  size += encode_scalar(
    encoded + size,
    2,
    capability_bits,
    sizeof(capability_bits)
  );
  size += encode_scalar(
    encoded + size,
    3,
    response->host_id,
    sizeof(response->host_id)
  );
  size += encode_scalar(
    encoded + size,
    4,
    maximum_control_payload,
    sizeof(maximum_control_payload)
  );
  size += encode_scalar(
    encoded + size,
    5,
    maximum_bulk_payload,
    sizeof(maximum_bulk_payload)
  );
  memcpy(output, encoded, sizeof(encoded));
  *encoded_size = size;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

MoonlightProtocolResult MoonlightProtocolV1DecodeClientHelloResponse(
  const uint8_t *input,
  size_t input_size,
  MoonlightProtocolV1ClientHelloResponse *response
) {
  MoonlightProtocolV1ClientHelloResponse decoded;
  const uint8_t *values[CLIENT_HELLO_RESPONSE_FIELD_COUNT];
  size_t value_sizes[CLIENT_HELLO_RESPONSE_FIELD_COUNT];
  static const size_t expected_sizes[CLIENT_HELLO_RESPONSE_FIELD_COUNT] = {
    2u,
    8u,
    MOONLIGHT_PROTOCOL_V1_UUID_SIZE,
    4u,
    4u,
  };
  size_t offset = 0;
  size_t field_index;
  MoonlightProtocolResult result;

  if (input == NULL || response == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (input_size < MOONLIGHT_PROTOCOL_V1_CLIENT_HELLO_RESPONSE_PAYLOAD_SIZE) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }

  memset(&decoded, 0, sizeof(decoded));
  for (
    field_index = 0;
    field_index < CLIENT_HELLO_RESPONSE_FIELD_COUNT;
    ++field_index) {
    result = decode_expected_scalar(
      input,
      input_size,
      &offset,
      (uint16_t) (field_index + 1u),
      CLIENT_HELLO_RESPONSE_FIELD_COUNT,
      &values[field_index],
      &value_sizes[field_index]
    );
    if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
      return result;
    }
    if (value_sizes[field_index] != expected_sizes[field_index]) {
      return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
    }
  }
  decoded.protocol_minor = control_load_u16(values[0]);
  decoded.capability_bits = control_load_u64(values[1]);
  memcpy(decoded.host_id, values[2], sizeof(decoded.host_id));
  decoded.maximum_control_payload = control_load_u32(values[3]);
  decoded.maximum_bulk_payload = control_load_u32(values[4]);
  result = classify_trailing_field(
    input,
    input_size,
    offset,
    CLIENT_HELLO_RESPONSE_FIELD_COUNT
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  result = validate_client_hello_response(&decoded);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  *response = decoded;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

MoonlightProtocolResult MoonlightProtocolV1EncodePingPayload(
  const MoonlightProtocolV1PingPayload *payload,
  uint8_t *output,
  size_t output_size,
  size_t *encoded_size
) {
  uint8_t encoded[MOONLIGHT_PROTOCOL_V1_PING_PAYLOAD_MAX];
  size_t size;

  if (payload == NULL || encoded_size == NULL || (output == NULL && payload->token_size != 0)) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (payload->token_size > MOONLIGHT_PROTOCOL_V1_PING_TOKEN_MAX) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  if (payload->token_size == 0) {
    *encoded_size = 0;
    return MOONLIGHT_PROTOCOL_RESULT_OK;
  }

  size = encode_scalar(
    encoded,
    1,
    payload->token,
    payload->token_size
  );
  if (output_size < size) {
    return MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL;
  }
  memcpy(output, encoded, size);
  *encoded_size = size;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

MoonlightProtocolResult MoonlightProtocolV1DecodePingPayload(
  const uint8_t *input,
  size_t input_size,
  MoonlightProtocolV1PingPayload *payload
) {
  MoonlightProtocolV1PingPayload decoded;
  const uint8_t *value;
  size_t value_size;
  size_t offset = 0;
  MoonlightProtocolResult result;

  if (payload == NULL || (input == NULL && input_size != 0)) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (input_size > MOONLIGHT_PROTOCOL_V1_PING_PAYLOAD_MAX) {
    return MOONLIGHT_PROTOCOL_RESULT_LIMIT_EXCEEDED;
  }

  memset(&decoded, 0, sizeof(decoded));
  if (input_size == 0) {
    *payload = decoded;
    return MOONLIGHT_PROTOCOL_RESULT_OK;
  }
  if (input_size < MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + 1u) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  result = decode_expected_scalar(
    input,
    input_size,
    &offset,
    1,
    1,
    &value,
    &value_size
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  result = classify_trailing_field(input, input_size, offset, 1);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }

  memcpy(decoded.token, value, value_size);
  decoded.token_size = value_size;
  *payload = decoded;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

MoonlightProtocolResult MoonlightProtocolV1EncodeHostInfoResponse(
  const MoonlightProtocolV1HostInfoResponse *response,
  uint8_t *output,
  size_t output_size,
  size_t *encoded_size
) {
  uint8_t encoded[MOONLIGHT_PROTOCOL_V1_HOST_INFO_RESPONSE_PAYLOAD_MAX];
  uint8_t configured_quic_port[2];
  uint8_t capability_bits[8];
  uint8_t maximum_active_stream_sessions[4];
  uint8_t available_stream_session_slots[4];
  uint8_t acl_permission_bits[8];
  uint8_t authorization_generation[8];
  uint8_t instance_visibility;
  MoonlightProtocolResult result;
  size_t size = 0;

  if (output == NULL || encoded_size == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  result = validate_host_info_response(response);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }

  control_store_u16(
    configured_quic_port,
    response->configured_quic_port
  );
  control_store_u64(capability_bits, response->capability_bits);
  control_store_u32(
    maximum_active_stream_sessions,
    response->maximum_active_stream_sessions
  );
  control_store_u32(
    available_stream_session_slots,
    response->available_stream_session_slots
  );
  control_store_u64(acl_permission_bits, response->acl_permission_bits);
  control_store_u64(
    authorization_generation,
    response->authorization_generation
  );
  instance_visibility = (uint8_t) response->instance_visibility;

  size += encode_scalar(
    encoded + size,
    1,
    response->host_id,
    sizeof(response->host_id)
  );
  size += encode_scalar(
    encoded + size,
    2,
    response->display_name,
    response->display_name_size
  );
  size += encode_scalar(
    encoded + size,
    3,
    response->software_version,
    response->software_version_size
  );
  size += encode_scalar(
    encoded + size,
    4,
    configured_quic_port,
    sizeof(configured_quic_port)
  );
  size += encode_scalar(
    encoded + size,
    5,
    capability_bits,
    sizeof(capability_bits)
  );
  size += encode_scalar(
    encoded + size,
    6,
    maximum_active_stream_sessions,
    sizeof(maximum_active_stream_sessions)
  );
  size += encode_scalar(
    encoded + size,
    7,
    available_stream_session_slots,
    sizeof(available_stream_session_slots)
  );
  size += encode_scalar(
    encoded + size,
    8,
    acl_permission_bits,
    sizeof(acl_permission_bits)
  );
  size += encode_scalar(
    encoded + size,
    9,
    authorization_generation,
    sizeof(authorization_generation)
  );
  size += encode_scalar(
    encoded + size,
    10,
    &instance_visibility,
    sizeof(instance_visibility)
  );
  if (output_size < size) {
    return MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL;
  }
  memcpy(output, encoded, size);
  *encoded_size = size;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

MoonlightProtocolResult MoonlightProtocolV1DecodeHostInfoResponse(
  const uint8_t *input,
  size_t input_size,
  MoonlightProtocolV1HostInfoResponse *response
) {
  MoonlightProtocolV1HostInfoResponse decoded;
  const uint8_t *values[HOST_INFO_RESPONSE_FIELD_COUNT];
  size_t value_sizes[HOST_INFO_RESPONSE_FIELD_COUNT];
  static const size_t minimum_sizes[HOST_INFO_RESPONSE_FIELD_COUNT] = {
    MOONLIGHT_PROTOCOL_V1_UUID_SIZE,
    1u,
    1u,
    2u,
    8u,
    4u,
    4u,
    8u,
    8u,
    1u,
  };
  static const size_t maximum_sizes[HOST_INFO_RESPONSE_FIELD_COUNT] = {
    MOONLIGHT_PROTOCOL_V1_UUID_SIZE,
    MOONLIGHT_PROTOCOL_V1_HOST_DISPLAY_NAME_MAX,
    MOONLIGHT_PROTOCOL_V1_SOFTWARE_VERSION_MAX,
    2u,
    8u,
    4u,
    4u,
    8u,
    8u,
    1u,
  };
  size_t offset = 0;
  size_t field_index;
  MoonlightProtocolResult result;

  if (input == NULL || response == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (input_size > MOONLIGHT_PROTOCOL_V1_HOST_INFO_RESPONSE_PAYLOAD_MAX) {
    return MOONLIGHT_PROTOCOL_RESULT_LIMIT_EXCEEDED;
  }
  if (input_size < MOONLIGHT_PROTOCOL_V1_HOST_INFO_RESPONSE_PAYLOAD_MIN) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }

  memset(&decoded, 0, sizeof(decoded));
  for (
    field_index = 0;
    field_index < HOST_INFO_RESPONSE_FIELD_COUNT;
    ++field_index) {
    result = decode_expected_scalar(
      input,
      input_size,
      &offset,
      (uint16_t) (field_index + 1u),
      HOST_INFO_RESPONSE_FIELD_COUNT,
      &values[field_index],
      &value_sizes[field_index]
    );
    if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
      return result;
    }
    if (value_sizes[field_index] < minimum_sizes[field_index] || value_sizes[field_index] > maximum_sizes[field_index]) {
      return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
    }
  }
  result = classify_trailing_field(
    input,
    input_size,
    offset,
    HOST_INFO_RESPONSE_FIELD_COUNT
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }

  memcpy(decoded.host_id, values[0], sizeof(decoded.host_id));
  memcpy(decoded.display_name, values[1], value_sizes[1]);
  decoded.display_name_size = value_sizes[1];
  memcpy(decoded.software_version, values[2], value_sizes[2]);
  decoded.software_version_size = value_sizes[2];
  decoded.configured_quic_port = control_load_u16(values[3]);
  decoded.capability_bits = control_load_u64(values[4]);
  decoded.maximum_active_stream_sessions = control_load_u32(values[5]);
  decoded.available_stream_session_slots = control_load_u32(values[6]);
  decoded.acl_permission_bits = control_load_u64(values[7]);
  decoded.authorization_generation = control_load_u64(values[8]);
  decoded.instance_visibility =
    (MoonlightProtocolV1InstanceVisibility) values[9][0];

  result = validate_host_info_response(&decoded);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  *response = decoded;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}
