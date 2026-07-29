/**
 * @file pairing.c
 * @brief Implements protocol version 1 Pair Control Lane pairing schemas.
 */

#include <moonlight/protocol/pairing.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/**
 * @brief Encoded length of the shortest PAIR_REQUEST request payload.
 */
#define PAIR_REQUEST_PAYLOAD_MIN 97u

/**
 * @brief Number of fields in a PAIR_REQUEST request.
 */
#define PAIR_REQUEST_FIELD_COUNT 4u

/**
 * @brief Number of fields in a successful PAIR_REQUEST response.
 */
#define PAIR_RESPONSE_FIELD_COUNT 6u

/**
 * @brief Reliably clears sensitive temporary storage.
 *
 * Volatile byte stores prevent the compiler from removing the wipe after the
 * final non-volatile use of the object.
 *
 * @param data Writable storage to clear.
 * @param data_size Number of bytes to clear.
 */
static void pairing_secure_clear(void *data, size_t data_size) {
  volatile uint8_t *cursor = (volatile uint8_t *) data;

  while (data_size != 0) {
    *cursor++ = 0;
    --data_size;
  }
}

/**
 * @brief Stores one network-order 16-bit integer.
 *
 * @param output Two writable bytes.
 * @param value Host-order value.
 */
static void pairing_store_u16(uint8_t *output, uint16_t value) {
  output[0] = (uint8_t) (value >> 8u);
  output[1] = (uint8_t) value;
}

/**
 * @brief Stores one network-order 32-bit integer.
 *
 * @param output Four writable bytes.
 * @param value Host-order value.
 */
static void pairing_store_u32(uint8_t *output, uint32_t value) {
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
static void pairing_store_u64(uint8_t *output, uint64_t value) {
  pairing_store_u32(output, (uint32_t) (value >> 32u));
  pairing_store_u32(output + 4u, (uint32_t) value);
}

/**
 * @brief Loads one network-order 32-bit integer.
 *
 * @param input Four readable bytes.
 * @return Host-order value.
 */
static uint32_t pairing_load_u32(const uint8_t *input) {
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
static uint64_t pairing_load_u64(const uint8_t *input) {
  return ((uint64_t) pairing_load_u32(input) << 32u) |
         pairing_load_u32(input + 4u);
}

/**
 * @brief Tests whether bytes are a shortest-form Unicode scalar sequence.
 *
 * @param input Candidate UTF-8 bytes.
 * @param input_size Number of candidate bytes.
 * @return True only for canonical UTF-8 without NUL or surrogate code points.
 */
static bool pairing_utf8_is_valid(
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
 * @brief Validates one PAIR_REQUEST request.
 *
 * @param request Candidate request.
 * @return The codec result.
 */
static MoonlightProtocolResult validate_pair_request(
  const MoonlightProtocolV1PairRequest *request
) {
  if (request == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (request->client_name_size == 0 || request->client_name_size > MOONLIGHT_PROTOCOL_V1_PAIR_CLIENT_NAME_MAX || !pairing_utf8_is_valid(request->client_name, request->client_name_size)) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Validates one successful PAIR_REQUEST response.
 *
 * @param response Candidate response.
 * @return The codec result.
 */
static MoonlightProtocolResult validate_pair_response(
  const MoonlightProtocolV1PairResponse *response
) {
  if (response == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if ((response->granted_acl_permission_bits & (uint64_t) ~MOONLIGHT_PROTOCOL_V1_ACL_PERMISSION_MASK) != 0 || response->authorization_generation == 0 || response->credential_epoch != UINT64_C(1) || (response->instance_visibility != MOONLIGHT_PROTOCOL_V1_INSTANCE_VISIBILITY_OWNER_ONLY && response->instance_visibility != MOONLIGHT_PROTOCOL_V1_INSTANCE_VISIBILITY_SHARED)) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  return MOONLIGHT_PROTOCOL_RESULT_OK;
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
  pairing_store_u16(output, field_id);
  pairing_store_u16(output + 2u, 0);
  pairing_store_u32(output + 4u, (uint32_t) value_size);
  memcpy(output + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE, value, value_size);
  return MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + value_size;
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
  if (result == MOONLIGHT_PROTOCOL_RESULT_TRUNCATED) {  // GCOVR_EXCL_BR_LINE: public payload minima include every header.
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;  // GCOVR_EXCL_LINE
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

MoonlightProtocolResult MoonlightProtocolV1EncodePairRequest(
  const MoonlightProtocolV1PairRequest *request,
  uint8_t *output,
  size_t output_size,
  size_t *encoded_size
) {
  uint8_t encoded[MOONLIGHT_PROTOCOL_V1_PAIR_REQUEST_PAYLOAD_MAX];
  MoonlightProtocolResult result;
  size_t size = 0;

  if (output == NULL || encoded_size == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  result = validate_pair_request(request);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }

  size += encode_scalar(
    encoded + size,
    1,
    request->host_id,
    sizeof(request->host_id)
  );
  size += encode_scalar(
    encoded + size,
    2,
    request->token_id,
    sizeof(request->token_id)
  );
  size += encode_scalar(
    encoded + size,
    3,
    request->invitation_secret,
    sizeof(request->invitation_secret)
  );
  size += encode_scalar(
    encoded + size,
    4,
    request->client_name,
    request->client_name_size
  );
  if (output_size < size) {
    pairing_secure_clear(encoded, sizeof(encoded));
    return MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL;
  }
  memcpy(output, encoded, size);
  *encoded_size = size;
  pairing_secure_clear(encoded, sizeof(encoded));
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

MoonlightProtocolResult MoonlightProtocolV1DecodePairRequest(
  const uint8_t *input,
  size_t input_size,
  MoonlightProtocolV1PairRequest *request
) {
  MoonlightProtocolV1PairRequest decoded;
  const uint8_t *values[PAIR_REQUEST_FIELD_COUNT];
  size_t value_sizes[PAIR_REQUEST_FIELD_COUNT];
  static const size_t expected_sizes[PAIR_REQUEST_FIELD_COUNT - 1u] = {
    MOONLIGHT_PROTOCOL_V1_PAIR_UUID_SIZE,
    MOONLIGHT_PROTOCOL_V1_PAIR_TOKEN_ID_SIZE,
    MOONLIGHT_PROTOCOL_V1_PAIR_INVITATION_SECRET_SIZE,
  };
  size_t offset = 0;
  size_t field_index;
  MoonlightProtocolResult result;

  if (input == NULL || request == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (input_size > MOONLIGHT_PROTOCOL_V1_PAIR_REQUEST_PAYLOAD_MAX) {
    return MOONLIGHT_PROTOCOL_RESULT_LIMIT_EXCEEDED;
  }
  if (input_size < PAIR_REQUEST_PAYLOAD_MIN) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }

  memset(&decoded, 0, sizeof(decoded));
  for (field_index = 0; field_index < PAIR_REQUEST_FIELD_COUNT; ++field_index) {
    result = decode_expected_scalar(
      input,
      input_size,
      &offset,
      (uint16_t) (field_index + 1u),
      PAIR_REQUEST_FIELD_COUNT,
      &values[field_index],
      &value_sizes[field_index]
    );
    if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
      return result;
    }
    if (field_index < PAIR_REQUEST_FIELD_COUNT - 1u && value_sizes[field_index] != expected_sizes[field_index]) {
      return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
    }
  }
  result = classify_trailing_field(
    input,
    input_size,
    offset,
    PAIR_REQUEST_FIELD_COUNT
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }

  memcpy(decoded.host_id, values[0], sizeof(decoded.host_id));
  memcpy(decoded.token_id, values[1], sizeof(decoded.token_id));
  memcpy(
    decoded.invitation_secret,
    values[2],
    sizeof(decoded.invitation_secret)
  );
  memcpy(decoded.client_name, values[3], value_sizes[3]);
  decoded.client_name_size = value_sizes[3];
  result = validate_pair_request(&decoded);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    pairing_secure_clear(&decoded, sizeof(decoded));
    return result;
  }
  *request = decoded;
  pairing_secure_clear(&decoded, sizeof(decoded));
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

MoonlightProtocolResult MoonlightProtocolV1EncodePairResponse(
  const MoonlightProtocolV1PairResponse *response,
  uint8_t *output,
  size_t output_size,
  size_t *encoded_size
) {
  uint8_t encoded[MOONLIGHT_PROTOCOL_V1_PAIR_RESPONSE_PAYLOAD_SIZE];
  uint8_t granted_acl_permission_bits[8];
  uint8_t authorization_generation[8];
  uint8_t credential_epoch[8];
  uint8_t instance_visibility;
  MoonlightProtocolResult result;
  size_t size = 0;

  if (output == NULL || encoded_size == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  result = validate_pair_response(response);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (output_size < sizeof(encoded)) {
    return MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL;
  }

  pairing_store_u64(
    granted_acl_permission_bits,
    response->granted_acl_permission_bits
  );
  pairing_store_u64(
    authorization_generation,
    response->authorization_generation
  );
  pairing_store_u64(credential_epoch, response->credential_epoch);
  instance_visibility = (uint8_t) response->instance_visibility;
  size += encode_scalar(
    encoded + size,
    1,
    response->client_principal_id,
    sizeof(response->client_principal_id)
  );
  size += encode_scalar(
    encoded + size,
    2,
    response->host_id,
    sizeof(response->host_id)
  );
  size += encode_scalar(
    encoded + size,
    3,
    granted_acl_permission_bits,
    sizeof(granted_acl_permission_bits)
  );
  size += encode_scalar(
    encoded + size,
    4,
    authorization_generation,
    sizeof(authorization_generation)
  );
  size += encode_scalar(
    encoded + size,
    5,
    credential_epoch,
    sizeof(credential_epoch)
  );
  size += encode_scalar(
    encoded + size,
    6,
    &instance_visibility,
    sizeof(instance_visibility)
  );
  memcpy(output, encoded, sizeof(encoded));
  *encoded_size = size;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

MoonlightProtocolResult MoonlightProtocolV1DecodePairResponse(
  const uint8_t *input,
  size_t input_size,
  MoonlightProtocolV1PairResponse *response
) {
  MoonlightProtocolV1PairResponse decoded;
  const uint8_t *values[PAIR_RESPONSE_FIELD_COUNT];
  size_t value_sizes[PAIR_RESPONSE_FIELD_COUNT];
  static const size_t expected_sizes[PAIR_RESPONSE_FIELD_COUNT] = {
    MOONLIGHT_PROTOCOL_V1_PAIR_UUID_SIZE,
    MOONLIGHT_PROTOCOL_V1_PAIR_UUID_SIZE,
    8u,
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
  if (input_size < MOONLIGHT_PROTOCOL_V1_PAIR_RESPONSE_PAYLOAD_SIZE) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }

  memset(&decoded, 0, sizeof(decoded));
  for (
    field_index = 0;
    field_index < PAIR_RESPONSE_FIELD_COUNT;
    ++field_index) {
    result = decode_expected_scalar(
      input,
      input_size,
      &offset,
      (uint16_t) (field_index + 1u),
      PAIR_RESPONSE_FIELD_COUNT,
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
  result = classify_trailing_field(
    input,
    input_size,
    offset,
    PAIR_RESPONSE_FIELD_COUNT
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }

  memcpy(
    decoded.client_principal_id,
    values[0],
    sizeof(decoded.client_principal_id)
  );
  memcpy(decoded.host_id, values[1], sizeof(decoded.host_id));
  decoded.granted_acl_permission_bits = pairing_load_u64(values[2]);
  decoded.authorization_generation = pairing_load_u64(values[3]);
  decoded.credential_epoch = pairing_load_u64(values[4]);
  decoded.instance_visibility =
    (MoonlightProtocolV1InstanceVisibility) values[5][0];
  result = validate_pair_response(&decoded);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  *response = decoded;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}
