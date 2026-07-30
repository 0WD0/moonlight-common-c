/**
 * @file session.c
 * @brief Implements protocol version 1 direct-display Stream Session schemas.
 */

#include <moonlight/protocol/session.h>
#include <moonlight/protocol/wire.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/**
 * @brief Largest field ID in a START_SESSION request.
 */
#define START_SESSION_REQUEST_FIELD_MAX 10u

/**
 * @brief Largest field ID in a successful START_SESSION response.
 */
#define START_SESSION_RESPONSE_FIELD_MAX 6u

/**
 * @brief Largest field ID in an accepted Video-configuration record.
 */
#define ACCEPTED_VIDEO_CONFIG_FIELD_MAX 14u

/**
 * @brief Largest field ID in a SESSION_READY request.
 */
#define SESSION_READY_REQUEST_FIELD_MAX 5u

/**
 * @brief Stores one network-order 16-bit integer.
 *
 * @param output Two writable bytes.
 * @param value Host-order value.
 */
static void session_store_u16(uint8_t *output, uint16_t value) {
  output[0] = (uint8_t) (value >> 8u);
  output[1] = (uint8_t) value;
}

/**
 * @brief Stores one network-order 32-bit integer.
 *
 * @param output Four writable bytes.
 * @param value Host-order value.
 */
static void session_store_u32(uint8_t *output, uint32_t value) {
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
static void session_store_u64(uint8_t *output, uint64_t value) {
  session_store_u32(output, (uint32_t) (value >> 32u));
  session_store_u32(output + 4u, (uint32_t) value);
}

/**
 * @brief Loads one network-order 16-bit integer.
 *
 * @param input Two readable bytes.
 * @return Host-order value.
 */
static uint16_t session_load_u16(const uint8_t *input) {
  return (uint16_t) (((uint16_t) input[0] << 8u) | input[1]);
}

/**
 * @brief Loads one network-order 32-bit integer.
 *
 * @param input Four readable bytes.
 * @return Host-order value.
 */
static uint32_t session_load_u32(const uint8_t *input) {
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
static uint64_t session_load_u64(const uint8_t *input) {
  return ((uint64_t) session_load_u32(input) << 32u) |
         session_load_u32(input + 4u);
}

/**
 * @brief Tests whether fixed opaque bytes are not the reserved all-zero value.
 *
 * @param input Fixed identifier bytes.
 * @param input_size Number of bytes to inspect.
 * @return True when at least one byte is nonzero.
 */
static bool session_identifier_is_nonzero(
  const uint8_t *input,
  size_t input_size
) {
  uint8_t combined = 0;
  size_t index;

  for (index = 0; index < input_size; ++index) {
    combined |= input[index];
  }
  return combined != 0;
}

/**
 * @brief Computes the greatest common divisor of two positive 32-bit values.
 *
 * @param left First positive value.
 * @param right Second positive value.
 * @return Greatest common divisor.
 */
static uint32_t session_gcd(uint32_t left, uint32_t right) {
  while (right != 0) {
    const uint32_t remainder = left % right;

    left = right;
    right = remainder;
  }
  return left;
}

/**
 * @brief Validates one Video codec registry value.
 *
 * @param codec Candidate codec.
 * @return OK for a defined codec, otherwise UNSUPPORTED.
 */
static MoonlightProtocolResult session_validate_codec(
  MoonlightProtocolV1VideoCodec codec
) {
  if (
    codec < MOONLIGHT_PROTOCOL_V1_VIDEO_CODEC_H264 ||
    codec > MOONLIGHT_PROTOCOL_V1_VIDEO_CODEC_AV1
  ) {
    return MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED;
  }
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Validates one dynamic-range registry value.
 *
 * @param dynamic_range Candidate dynamic range.
 * @return OK for SDR or HDR10, otherwise UNSUPPORTED.
 */
static MoonlightProtocolResult session_validate_dynamic_range(
  MoonlightProtocolV1VideoDynamicRange dynamic_range
) {
  if (
    dynamic_range != MOONLIGHT_PROTOCOL_V1_VIDEO_DYNAMIC_RANGE_SDR &&
    dynamic_range != MOONLIGHT_PROTOCOL_V1_VIDEO_DYNAMIC_RANGE_HDR10
  ) {
    return MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED;
  }
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Validates one chroma-sampling registry value.
 *
 * @param chroma Candidate chroma sampling.
 * @return OK for 4:2:0 or 4:4:4, otherwise UNSUPPORTED.
 */
static MoonlightProtocolResult session_validate_chroma(
  MoonlightProtocolV1VideoChroma chroma
) {
  if (
    chroma != MOONLIGHT_PROTOCOL_V1_VIDEO_CHROMA_420 &&
    chroma != MOONLIGHT_PROTOCOL_V1_VIDEO_CHROMA_444
  ) {
    return MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED;
  }
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Validates one canonical positive frame-rate rational.
 *
 * @param numerator Frame-rate numerator.
 * @param denominator Frame-rate denominator.
 * @return The codec result.
 */
static MoonlightProtocolResult session_validate_frame_rate(
  uint32_t numerator,
  uint32_t denominator
) {
  if (numerator == 0 || denominator == 0) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  if (session_gcd(numerator, denominator) != 1u) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Validates a width and height against the direct Video profile.
 *
 * @param width Video width.
 * @param height Video height.
 * @return The codec result.
 */
static MoonlightProtocolResult session_validate_dimensions(
  uint16_t width,
  uint16_t height
) {
  if (
    width < MOONLIGHT_PROTOCOL_V1_VIDEO_WIDTH_MIN ||
    width > MOONLIGHT_PROTOCOL_V1_VIDEO_WIDTH_MAX ||
    height < MOONLIGHT_PROTOCOL_V1_VIDEO_HEIGHT_MIN ||
    height > MOONLIGHT_PROTOCOL_V1_VIDEO_HEIGHT_MAX
  ) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Validates complete-DATAGRAM and Video-shard limits.
 *
 * @param complete_datagram Complete-DATAGRAM payload limit.
 * @param video_shard Exact or maximum Video coding-shard size.
 * @return The codec result.
 */
static MoonlightProtocolResult session_validate_packet_limits(
  uint16_t complete_datagram,
  uint16_t video_shard
) {
  if (complete_datagram < MOONLIGHT_PROTOCOL_V1_SESSION_DATAGRAM_MIN) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  if (
    video_shard < MOONLIGHT_PROTOCOL_V1_SESSION_VIDEO_SHARD_MIN ||
    video_shard > MOONLIGHT_PROTOCOL_V1_SESSION_VIDEO_SHARD_MAX ||
    video_shard % 16u != 0
  ) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  if (
    (uint32_t) video_shard +
      MOONLIGHT_PROTOCOL_V1_SESSION_VIDEO_HEADER_SIZE >
    complete_datagram
  ) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Validates one host-order START_SESSION request.
 *
 * @param request Candidate request.
 * @return The codec result.
 */
static MoonlightProtocolResult session_validate_start_request(
  const MoonlightProtocolV1StartSessionRequest *request
) {
  MoonlightProtocolResult result;
  size_t index;
  size_t previous;

  if (request == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (
    !session_identifier_is_nonzero(
      request->display_id,
      sizeof(request->display_id)
    ) ||
    request->catalog_revision == 0 ||
    request->bitrate_kbps == 0
  ) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  result = session_validate_dimensions(request->width, request->height);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  result = session_validate_frame_rate(
    request->frame_rate_numerator,
    request->frame_rate_denominator
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (
    request->codec_preference_count == 0 ||
    request->codec_preference_count >
      MOONLIGHT_PROTOCOL_V1_VIDEO_CODEC_PREFERENCE_MAX
  ) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  for (index = 0; index < request->codec_preference_count; ++index) {
    result = session_validate_codec(request->codec_preferences[index]);
    if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
      return result;
    }
    for (previous = 0; previous < index; ++previous) {
      if (
        request->codec_preferences[previous] ==
        request->codec_preferences[index]
      ) {
        return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
      }
    }
  }
  result = session_validate_dynamic_range(request->dynamic_range);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  return session_validate_chroma(request->chroma);
}

/**
 * @brief Validates one accepted Video configuration.
 *
 * @param video Candidate Video configuration.
 * @return The codec result.
 */
static MoonlightProtocolResult session_validate_video(
  const MoonlightProtocolV1AcceptedVideoConfig *video
) {
  MoonlightProtocolResult result;

  result = session_validate_codec(video->codec);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (video->bit_depth != 8u && video->bit_depth != 10u) {
    return MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED;
  }
  result = session_validate_chroma(video->chroma);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  result = session_validate_dynamic_range(video->dynamic_range);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  result = session_validate_dimensions(video->width, video->height);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  result = session_validate_frame_rate(
    video->frame_rate_numerator,
    video->frame_rate_denominator
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (
    video->bitrate_kbps == 0 ||
    video->codec_configuration_generation == 0
  ) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  if (
    video->maximum_fec_percentage > MOONLIGHT_PROTOCOL_V1_VIDEO_FEC_MAX ||
    video->minimum_fec_percentage > video->initial_fec_percentage ||
    video->initial_fec_percentage > video->maximum_fec_percentage ||
    video->initial_fec_percentage >
      MOONLIGHT_PROTOCOL_V1_VIDEO_INITIAL_FEC_MAX
  ) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Validates one successful START_SESSION response.
 *
 * @param response Candidate response.
 * @return The codec result.
 */
static MoonlightProtocolResult session_validate_start_response(
  const MoonlightProtocolV1StartSessionResponse *response
) {
  MoonlightProtocolResult result;

  if (response == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (
    !session_identifier_is_nonzero(
      response->session_id,
      sizeof(response->session_id)
    ) ||
    response->session_wire_id == 0 ||
    response->media_epoch != MOONLIGHT_PROTOCOL_V1_INITIAL_MEDIA_EPOCH
  ) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  result = session_validate_packet_limits(
    response->maximum_complete_datagram,
    response->maximum_video_shard
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  return session_validate_video(&response->video);
}

/**
 * @brief Validates one SESSION_READY request.
 *
 * @param request Candidate request.
 * @return The codec result.
 */
static MoonlightProtocolResult session_validate_ready_request(
  const MoonlightProtocolV1SessionReadyRequest *request
) {
  if (request == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (
    !session_identifier_is_nonzero(
      request->session_id,
      sizeof(request->session_id)
    ) ||
    request->session_wire_id == 0 ||
    request->media_epoch != MOONLIGHT_PROTOCOL_V1_INITIAL_MEDIA_EPOCH
  ) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  return session_validate_packet_limits(
    request->complete_datagram,
    request->video_shard
  );
}

/**
 * @brief Appends one canonical field to fixed local storage.
 *
 * @param output Destination positioned at the field header.
 * @param field_id Field ID.
 * @param flags Canonical field flags.
 * @param value Field bytes.
 * @param value_size Number of field bytes.
 * @return Number of encoded bytes.
 */
static size_t session_encode_field(
  uint8_t *output,
  uint16_t field_id,
  uint16_t flags,
  const uint8_t *value,
  size_t value_size
) {
  session_store_u16(output, field_id);
  session_store_u16(output + 2u, flags);
  session_store_u32(output + 4u, (uint32_t) value_size);
  memcpy(output + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE, value, value_size);
  return MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + value_size;
}

/**
 * @brief Decodes one complete contiguous field from a bounded payload.
 *
 * @param input Complete containing payload.
 * @param input_size Number of containing bytes.
 * @param offset Current field offset, advanced on success.
 * @param field Receives the decoded field header.
 * @param value Receives the borrowed complete field value.
 * @return The codec result.
 */
static MoonlightProtocolResult session_decode_field(
  const uint8_t *input,
  size_t input_size,
  size_t *offset,
  MoonlightProtocolV1TlvField *field,
  const uint8_t **value
) {
  const size_t remaining = input_size - *offset;
  MoonlightProtocolResult result;

  result = MoonlightProtocolV1DecodeTlvFieldHeader(
    input + *offset,
    remaining,
    field
  );
  if (result == MOONLIGHT_PROTOCOL_RESULT_TRUNCATED) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (
    field->field_length >
    remaining - MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE
  ) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  *value = input + *offset + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE;
  *offset += MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + field->field_length;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Classifies a field ID against one exact canonical position.
 *
 * @param field_id Actual field ID.
 * @param expected_id Required field ID at this position.
 * @param maximum_known_id Largest defined field ID in the selected schema.
 * @return OK for the exact field, MALFORMED for known reordering or
 * duplication, and UNSUPPORTED for an unknown field.
 */
static MoonlightProtocolResult session_classify_field_id(
  uint16_t field_id,
  uint16_t expected_id,
  uint16_t maximum_known_id
) {
  if (field_id == expected_id) {
    return MOONLIGHT_PROTOCOL_RESULT_OK;
  }
  if (field_id >= 1u && field_id <= maximum_known_id) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  return MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED;
}

/**
 * @brief Decodes one exact scalar field.
 *
 * @param input Complete containing payload.
 * @param input_size Number of containing bytes.
 * @param offset Current field offset, advanced on success.
 * @param expected_id Required field ID.
 * @param maximum_known_id Largest defined field ID in the selected schema.
 * @param expected_flags Required schema flags.
 * @param expected_size Exact field-value size.
 * @param value Receives the borrowed field value.
 * @return The codec result.
 */
static MoonlightProtocolResult session_decode_scalar(
  const uint8_t *input,
  size_t input_size,
  size_t *offset,
  uint16_t expected_id,
  uint16_t maximum_known_id,
  uint16_t expected_flags,
  size_t expected_size,
  const uint8_t **value
) {
  MoonlightProtocolV1TlvField field;
  MoonlightProtocolResult result;

  result = session_decode_field(input, input_size, offset, &field, value);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  result = session_classify_field_id(
    field.field_id,
    expected_id,
    maximum_known_id
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (field.flags != expected_flags) {
    return MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED;
  }
  if (field.field_length != expected_size) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Classifies bytes trailing a complete required schema.
 *
 * @param input Complete containing payload.
 * @param input_size Number of containing bytes.
 * @param offset First trailing byte.
 * @param maximum_known_id Largest defined field ID in the selected schema.
 * @return OK for exact consumption, MALFORMED for a broken or known field, and
 * UNSUPPORTED for an unknown field.
 */
static MoonlightProtocolResult session_classify_trailing(
  const uint8_t *input,
  size_t input_size,
  size_t offset,
  uint16_t maximum_known_id
) {
  MoonlightProtocolV1TlvField field;
  const uint8_t *value;
  MoonlightProtocolResult result;

  if (offset == input_size) {
    return MOONLIGHT_PROTOCOL_RESULT_OK;
  }
  result = session_decode_field(
    input,
    input_size,
    &offset,
    &field,
    &value
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result == MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED ?
             result :
             MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  return field.field_id >= 1u && field.field_id <= maximum_known_id ?
           MOONLIGHT_PROTOCOL_RESULT_MALFORMED :
           MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED;
}

MoonlightProtocolResult MoonlightProtocolV1EncodeStartSessionRequest(
  const MoonlightProtocolV1StartSessionRequest *request,
  uint8_t *output,
  size_t output_size,
  size_t *encoded_size
) {
  uint8_t encoded[MOONLIGHT_PROTOCOL_V1_START_SESSION_REQUEST_PAYLOAD_MAX];
  uint8_t scalar[8];
  MoonlightProtocolResult result;
  size_t size = 0;
  size_t index;

  if (output == NULL || encoded_size == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  result = session_validate_start_request(request);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }

  size += session_encode_field(
    encoded + size,
    1,
    0,
    request->display_id,
    sizeof(request->display_id)
  );
  session_store_u64(scalar, request->catalog_revision);
  size += session_encode_field(encoded + size, 2, 0, scalar, 8u);
  session_store_u16(scalar, request->width);
  size += session_encode_field(encoded + size, 3, 0, scalar, 2u);
  session_store_u16(scalar, request->height);
  size += session_encode_field(encoded + size, 4, 0, scalar, 2u);
  session_store_u32(scalar, request->frame_rate_numerator);
  size += session_encode_field(encoded + size, 5, 0, scalar, 4u);
  session_store_u32(scalar, request->frame_rate_denominator);
  size += session_encode_field(encoded + size, 6, 0, scalar, 4u);
  session_store_u32(scalar, request->bitrate_kbps);
  size += session_encode_field(encoded + size, 7, 0, scalar, 4u);
  for (index = 0; index < request->codec_preference_count; ++index) {
    session_store_u16(scalar, (uint16_t) request->codec_preferences[index]);
    size += session_encode_field(
      encoded + size,
      8,
      MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED,
      scalar,
      2u
    );
  }
  scalar[0] = (uint8_t) request->dynamic_range;
  size += session_encode_field(encoded + size, 9, 0, scalar, 1u);
  scalar[0] = (uint8_t) request->chroma;
  size += session_encode_field(encoded + size, 10, 0, scalar, 1u);

  if (output_size < size) {
    return MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL;
  }
  memcpy(output, encoded, size);
  *encoded_size = size;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

MoonlightProtocolResult MoonlightProtocolV1DecodeStartSessionRequest(
  const uint8_t *input,
  size_t input_size,
  MoonlightProtocolV1StartSessionRequest *request
) {
  MoonlightProtocolV1StartSessionRequest decoded;
  MoonlightProtocolV1TlvField field;
  const uint8_t *value;
  size_t offset = 0;
  size_t previous;
  MoonlightProtocolResult result;

  if (input == NULL || request == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (input_size > MOONLIGHT_PROTOCOL_V1_START_SESSION_REQUEST_PAYLOAD_MAX) {
    return MOONLIGHT_PROTOCOL_RESULT_LIMIT_EXCEEDED;
  }
  if (input_size < MOONLIGHT_PROTOCOL_V1_START_SESSION_REQUEST_PAYLOAD_MIN) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }

  memset(&decoded, 0, sizeof(decoded));
  result = session_decode_scalar(
    input,
    input_size,
    &offset,
    1,
    START_SESSION_REQUEST_FIELD_MAX,
    0,
    sizeof(decoded.display_id),
    &value
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  memcpy(decoded.display_id, value, sizeof(decoded.display_id));
  result = session_decode_scalar(
    input,
    input_size,
    &offset,
    2,
    START_SESSION_REQUEST_FIELD_MAX,
    0,
    8u,
    &value
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  decoded.catalog_revision = session_load_u64(value);
  result = session_decode_scalar(
    input,
    input_size,
    &offset,
    3,
    START_SESSION_REQUEST_FIELD_MAX,
    0,
    2u,
    &value
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  decoded.width = session_load_u16(value);
  result = session_decode_scalar(
    input,
    input_size,
    &offset,
    4,
    START_SESSION_REQUEST_FIELD_MAX,
    0,
    2u,
    &value
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  decoded.height = session_load_u16(value);
  result = session_decode_scalar(
    input,
    input_size,
    &offset,
    5,
    START_SESSION_REQUEST_FIELD_MAX,
    0,
    4u,
    &value
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  decoded.frame_rate_numerator = session_load_u32(value);
  result = session_decode_scalar(
    input,
    input_size,
    &offset,
    6,
    START_SESSION_REQUEST_FIELD_MAX,
    0,
    4u,
    &value
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  decoded.frame_rate_denominator = session_load_u32(value);
  result = session_decode_scalar(
    input,
    input_size,
    &offset,
    7,
    START_SESSION_REQUEST_FIELD_MAX,
    0,
    4u,
    &value
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  decoded.bitrate_kbps = session_load_u32(value);

  result = session_decode_field(input, input_size, &offset, &field, &value);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  result = session_classify_field_id(
    field.field_id,
    8,
    START_SESSION_REQUEST_FIELD_MAX
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  for (;;) {
    MoonlightProtocolV1VideoCodec codec;

    if (field.flags != MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED) {
      return MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED;
    }
    if (field.field_length != 2u) {
      return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
    }
    /*
     * Four complete codec fields cannot fit below the public request payload
     * ceiling together with the required suffix fields. Retain this explicit
     * guard so that the fixed preference array stays safe when a malformed
     * request ends immediately after a fourth codec field.
     */
    if (decoded.codec_preference_count >= MOONLIGHT_PROTOCOL_V1_VIDEO_CODEC_PREFERENCE_MAX) {
      return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
    }
    codec = (MoonlightProtocolV1VideoCodec) session_load_u16(value);
    result = session_validate_codec(codec);
    if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
      return result;
    }
    for (previous = 0; previous < decoded.codec_preference_count; ++previous) {
      if (decoded.codec_preferences[previous] == codec) {
        return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
      }
    }
    decoded.codec_preferences[decoded.codec_preference_count++] = codec;

    if (offset == input_size) {
      return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
    }
    result = session_decode_field(
      input,
      input_size,
      &offset,
      &field,
      &value
    );
    if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
      return result;
    }
    if (field.field_id != 8u) {
      break;
    }
  }
  result = session_classify_field_id(
    field.field_id,
    9,
    START_SESSION_REQUEST_FIELD_MAX
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (field.flags != 0) {
    return MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED;
  }
  if (field.field_length != 1u) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  decoded.dynamic_range = (MoonlightProtocolV1VideoDynamicRange) value[0];

  result = session_decode_scalar(
    input,
    input_size,
    &offset,
    10,
    START_SESSION_REQUEST_FIELD_MAX,
    0,
    1u,
    &value
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  decoded.chroma = (MoonlightProtocolV1VideoChroma) value[0];
  result = session_classify_trailing(
    input,
    input_size,
    offset,
    START_SESSION_REQUEST_FIELD_MAX
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  result = session_validate_start_request(&decoded);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  *request = decoded;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Encodes one validated accepted Video configuration.
 *
 * @param video Validated configuration.
 * @param output Exact-size nested-record destination.
 * @return Number of encoded bytes.
 */
static size_t session_encode_video(
  const MoonlightProtocolV1AcceptedVideoConfig *video,
  uint8_t *output
) {
  uint8_t scalar[4];
  size_t size = 0;

  session_store_u16(scalar, (uint16_t) video->codec);
  size += session_encode_field(output + size, 1, 0, scalar, 2u);
  session_store_u16(scalar, video->profile);
  size += session_encode_field(output + size, 2, 0, scalar, 2u);
  scalar[0] = video->bit_depth;
  size += session_encode_field(output + size, 3, 0, scalar, 1u);
  scalar[0] = (uint8_t) video->chroma;
  size += session_encode_field(output + size, 4, 0, scalar, 1u);
  scalar[0] = (uint8_t) video->dynamic_range;
  size += session_encode_field(output + size, 5, 0, scalar, 1u);
  session_store_u16(scalar, video->width);
  size += session_encode_field(output + size, 6, 0, scalar, 2u);
  session_store_u16(scalar, video->height);
  size += session_encode_field(output + size, 7, 0, scalar, 2u);
  session_store_u32(scalar, video->frame_rate_numerator);
  size += session_encode_field(output + size, 8, 0, scalar, 4u);
  session_store_u32(scalar, video->frame_rate_denominator);
  size += session_encode_field(output + size, 9, 0, scalar, 4u);
  session_store_u32(scalar, video->bitrate_kbps);
  size += session_encode_field(output + size, 10, 0, scalar, 4u);
  scalar[0] = video->initial_fec_percentage;
  size += session_encode_field(output + size, 11, 0, scalar, 1u);
  session_store_u32(scalar, video->codec_configuration_generation);
  size += session_encode_field(output + size, 12, 0, scalar, 4u);
  scalar[0] = video->minimum_fec_percentage;
  size += session_encode_field(output + size, 13, 0, scalar, 1u);
  scalar[0] = video->maximum_fec_percentage;
  size += session_encode_field(output + size, 14, 0, scalar, 1u);
  return size;
}

/**
 * @brief Decodes one exact accepted Video-configuration record.
 *
 * @param input Exact nested-record bytes.
 * @param input_size Number of nested bytes.
 * @param video Receives validated host-order values only on success.
 * @return The codec result.
 */
static MoonlightProtocolResult session_decode_video(
  const uint8_t *input,
  size_t input_size,
  MoonlightProtocolV1AcceptedVideoConfig *video
) {
  MoonlightProtocolV1AcceptedVideoConfig decoded;
  const uint8_t *value;
  size_t offset = 0;
  MoonlightProtocolResult result;

  memset(&decoded, 0, sizeof(decoded));

#define DECODE_VIDEO_SCALAR(field_id, field_size) \
  do { \
    result = session_decode_scalar( \
      input, \
      input_size, \
      &offset, \
      (field_id), \
      ACCEPTED_VIDEO_CONFIG_FIELD_MAX, \
      0, \
      (field_size), \
      &value \
    ); \
    if (result != MOONLIGHT_PROTOCOL_RESULT_OK) { \
      return result; \
    } \
  } while (0)

  DECODE_VIDEO_SCALAR(1, 2u);
  decoded.codec = (MoonlightProtocolV1VideoCodec) session_load_u16(value);
  DECODE_VIDEO_SCALAR(2, 2u);
  decoded.profile = session_load_u16(value);
  DECODE_VIDEO_SCALAR(3, 1u);
  decoded.bit_depth = value[0];
  DECODE_VIDEO_SCALAR(4, 1u);
  decoded.chroma = (MoonlightProtocolV1VideoChroma) value[0];
  DECODE_VIDEO_SCALAR(5, 1u);
  decoded.dynamic_range = (MoonlightProtocolV1VideoDynamicRange) value[0];
  DECODE_VIDEO_SCALAR(6, 2u);
  decoded.width = session_load_u16(value);
  DECODE_VIDEO_SCALAR(7, 2u);
  decoded.height = session_load_u16(value);
  DECODE_VIDEO_SCALAR(8, 4u);
  decoded.frame_rate_numerator = session_load_u32(value);
  DECODE_VIDEO_SCALAR(9, 4u);
  decoded.frame_rate_denominator = session_load_u32(value);
  DECODE_VIDEO_SCALAR(10, 4u);
  decoded.bitrate_kbps = session_load_u32(value);
  DECODE_VIDEO_SCALAR(11, 1u);
  decoded.initial_fec_percentage = value[0];
  DECODE_VIDEO_SCALAR(12, 4u);
  decoded.codec_configuration_generation = session_load_u32(value);
  DECODE_VIDEO_SCALAR(13, 1u);
  decoded.minimum_fec_percentage = value[0];
  DECODE_VIDEO_SCALAR(14, 1u);
  decoded.maximum_fec_percentage = value[0];

#undef DECODE_VIDEO_SCALAR

  result = session_validate_video(&decoded);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  *video = decoded;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

MoonlightProtocolResult MoonlightProtocolV1EncodeStartSessionResponse(
  const MoonlightProtocolV1StartSessionResponse *response,
  uint8_t *output,
  size_t output_size,
  size_t *encoded_size
) {
  uint8_t encoded[MOONLIGHT_PROTOCOL_V1_START_SESSION_RESPONSE_PAYLOAD_SIZE];
  uint8_t video[MOONLIGHT_PROTOCOL_V1_ACCEPTED_VIDEO_CONFIG_PAYLOAD_SIZE];
  uint8_t scalar[4];
  MoonlightProtocolResult result;
  size_t size = 0;
  size_t video_size;

  if (output == NULL || encoded_size == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  result = session_validate_start_response(response);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }

  size += session_encode_field(
    encoded + size,
    1,
    0,
    response->session_id,
    sizeof(response->session_id)
  );
  session_store_u32(scalar, response->session_wire_id);
  size += session_encode_field(encoded + size, 2, 0, scalar, 4u);
  session_store_u32(scalar, response->media_epoch);
  size += session_encode_field(encoded + size, 3, 0, scalar, 4u);
  session_store_u16(scalar, response->maximum_complete_datagram);
  size += session_encode_field(encoded + size, 4, 0, scalar, 2u);
  session_store_u16(scalar, response->maximum_video_shard);
  size += session_encode_field(encoded + size, 5, 0, scalar, 2u);
  video_size = session_encode_video(&response->video, video);
  size += session_encode_field(encoded + size, 6, 0, video, video_size);

  if (output_size < size) {
    return MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL;
  }
  memcpy(output, encoded, size);
  *encoded_size = size;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

MoonlightProtocolResult MoonlightProtocolV1DecodeStartSessionResponse(
  const uint8_t *input,
  size_t input_size,
  MoonlightProtocolV1StartSessionResponse *response
) {
  MoonlightProtocolV1StartSessionResponse decoded;
  const uint8_t *value;
  size_t offset = 0;
  MoonlightProtocolResult result;

  if (input == NULL || response == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (input_size > MOONLIGHT_PROTOCOL_V1_START_SESSION_RESPONSE_PAYLOAD_SIZE) {
    return MOONLIGHT_PROTOCOL_RESULT_LIMIT_EXCEEDED;
  }
  if (input_size < MOONLIGHT_PROTOCOL_V1_START_SESSION_RESPONSE_PAYLOAD_SIZE) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }

  memset(&decoded, 0, sizeof(decoded));
  result = session_decode_scalar(
    input,
    input_size,
    &offset,
    1,
    START_SESSION_RESPONSE_FIELD_MAX,
    0,
    sizeof(decoded.session_id),
    &value
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  memcpy(decoded.session_id, value, sizeof(decoded.session_id));
  result = session_decode_scalar(
    input,
    input_size,
    &offset,
    2,
    START_SESSION_RESPONSE_FIELD_MAX,
    0,
    4u,
    &value
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  decoded.session_wire_id = session_load_u32(value);
  result = session_decode_scalar(
    input,
    input_size,
    &offset,
    3,
    START_SESSION_RESPONSE_FIELD_MAX,
    0,
    4u,
    &value
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  decoded.media_epoch = session_load_u32(value);
  result = session_decode_scalar(
    input,
    input_size,
    &offset,
    4,
    START_SESSION_RESPONSE_FIELD_MAX,
    0,
    2u,
    &value
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  decoded.maximum_complete_datagram = session_load_u16(value);
  result = session_decode_scalar(
    input,
    input_size,
    &offset,
    5,
    START_SESSION_RESPONSE_FIELD_MAX,
    0,
    2u,
    &value
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  decoded.maximum_video_shard = session_load_u16(value);
  result = session_decode_scalar(
    input,
    input_size,
    &offset,
    6,
    START_SESSION_RESPONSE_FIELD_MAX,
    0,
    MOONLIGHT_PROTOCOL_V1_ACCEPTED_VIDEO_CONFIG_PAYLOAD_SIZE,
    &value
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  result = session_decode_video(
    value,
    MOONLIGHT_PROTOCOL_V1_ACCEPTED_VIDEO_CONFIG_PAYLOAD_SIZE,
    &decoded.video
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  result = session_validate_start_response(&decoded);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  *response = decoded;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

MoonlightProtocolResult MoonlightProtocolV1EncodeSessionReadyRequest(
  const MoonlightProtocolV1SessionReadyRequest *request,
  uint8_t *output,
  size_t output_size,
  size_t *encoded_size
) {
  uint8_t encoded[MOONLIGHT_PROTOCOL_V1_SESSION_READY_REQUEST_PAYLOAD_SIZE];
  uint8_t scalar[4];
  MoonlightProtocolResult result;
  size_t size = 0;

  if (output == NULL || encoded_size == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  result = session_validate_ready_request(request);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }

  size += session_encode_field(
    encoded + size,
    1,
    0,
    request->session_id,
    sizeof(request->session_id)
  );
  session_store_u32(scalar, request->session_wire_id);
  size += session_encode_field(encoded + size, 2, 0, scalar, 4u);
  session_store_u32(scalar, request->media_epoch);
  size += session_encode_field(encoded + size, 3, 0, scalar, 4u);
  session_store_u16(scalar, request->complete_datagram);
  size += session_encode_field(encoded + size, 4, 0, scalar, 2u);
  session_store_u16(scalar, request->video_shard);
  size += session_encode_field(encoded + size, 5, 0, scalar, 2u);

  if (output_size < size) {
    return MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL;
  }
  memcpy(output, encoded, size);
  *encoded_size = size;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

MoonlightProtocolResult MoonlightProtocolV1DecodeSessionReadyRequest(
  const uint8_t *input,
  size_t input_size,
  MoonlightProtocolV1SessionReadyRequest *request
) {
  MoonlightProtocolV1SessionReadyRequest decoded;
  const uint8_t *value;
  size_t offset = 0;
  MoonlightProtocolResult result;

  if (input == NULL || request == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (input_size > MOONLIGHT_PROTOCOL_V1_SESSION_READY_REQUEST_PAYLOAD_SIZE) {
    return MOONLIGHT_PROTOCOL_RESULT_LIMIT_EXCEEDED;
  }
  if (input_size < MOONLIGHT_PROTOCOL_V1_SESSION_READY_REQUEST_PAYLOAD_SIZE) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }

  memset(&decoded, 0, sizeof(decoded));
  result = session_decode_scalar(
    input,
    input_size,
    &offset,
    1,
    SESSION_READY_REQUEST_FIELD_MAX,
    0,
    sizeof(decoded.session_id),
    &value
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  memcpy(decoded.session_id, value, sizeof(decoded.session_id));
  result = session_decode_scalar(
    input,
    input_size,
    &offset,
    2,
    SESSION_READY_REQUEST_FIELD_MAX,
    0,
    4u,
    &value
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  decoded.session_wire_id = session_load_u32(value);
  result = session_decode_scalar(
    input,
    input_size,
    &offset,
    3,
    SESSION_READY_REQUEST_FIELD_MAX,
    0,
    4u,
    &value
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  decoded.media_epoch = session_load_u32(value);
  result = session_decode_scalar(
    input,
    input_size,
    &offset,
    4,
    SESSION_READY_REQUEST_FIELD_MAX,
    0,
    2u,
    &value
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  decoded.complete_datagram = session_load_u16(value);
  result = session_decode_scalar(
    input,
    input_size,
    &offset,
    5,
    SESSION_READY_REQUEST_FIELD_MAX,
    0,
    2u,
    &value
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  decoded.video_shard = session_load_u16(value);
  result = session_validate_ready_request(&decoded);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  *request = decoded;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}
