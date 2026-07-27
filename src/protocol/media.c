/**
 * @file media.c
 * @brief Implements canonical Sunshine protocol version 1 media DATAGRAM encoding.
 */

#include <limits.h>
#include <moonlight/protocol/media.h>
#include <string.h>

#define VIDEO_FLAG_MASK ((uint8_t) (MOONLIGHT_PROTOCOL_V1_VIDEO_FLAG_KEY_FRAME | \
                                    MOONLIGHT_PROTOCOL_V1_VIDEO_FLAG_START_OF_FRAME | \
                                    MOONLIGHT_PROTOCOL_V1_VIDEO_FLAG_END_OF_FRAME | \
                                    MOONLIGHT_PROTOCOL_V1_VIDEO_FLAG_REFERENCE_RECOVERY))

/**
 * @brief Loads one network-order 16-bit integer.
 *
 * @param input Two readable bytes.
 * @return The host-order value.
 */
static uint16_t load_u16_be(const uint8_t *input) {
  return (uint16_t) (((uint16_t) input[0] << 8) | input[1]);
}

/**
 * @brief Loads one network-order 32-bit integer.
 *
 * @param input Four readable bytes.
 * @return The host-order value.
 */
static uint32_t load_u32_be(const uint8_t *input) {
  return ((uint32_t) input[0] << 24) |
         ((uint32_t) input[1] << 16) |
         ((uint32_t) input[2] << 8) |
         (uint32_t) input[3];
}

/**
 * @brief Loads one network-order 64-bit integer.
 *
 * @param input Eight readable bytes.
 * @return The host-order value.
 */
static uint64_t load_u64_be(const uint8_t *input) {
  return ((uint64_t) load_u32_be(input) << 32) | load_u32_be(input + 4);
}

/**
 * @brief Stores one network-order 16-bit integer.
 *
 * @param output Two writable bytes.
 * @param value Host-order value.
 */
static void store_u16_be(uint8_t *output, uint16_t value) {
  output[0] = (uint8_t) (value >> 8);
  output[1] = (uint8_t) value;
}

/**
 * @brief Stores one network-order 32-bit integer.
 *
 * @param output Four writable bytes.
 * @param value Host-order value.
 */
static void store_u32_be(uint8_t *output, uint32_t value) {
  output[0] = (uint8_t) (value >> 24);
  output[1] = (uint8_t) (value >> 16);
  output[2] = (uint8_t) (value >> 8);
  output[3] = (uint8_t) value;
}

/**
 * @brief Stores one network-order 64-bit integer.
 *
 * @param output Eight writable bytes.
 * @param value Host-order value.
 */
static void store_u64_be(uint8_t *output, uint64_t value) {
  store_u32_be(output, (uint32_t) (value >> 32));
  store_u32_be(output + 4, (uint32_t) value);
}

/**
 * @brief Multiplies two bytes in the canonical `GF(256)` field.
 *
 * @param left Left field element.
 * @param right Right field element.
 * @return The field product reduced by primitive polynomial `0x11d`.
 */
static uint8_t gf_multiply(uint8_t left, uint8_t right) {
  uint8_t result = 0;

  while (right != 0) {
    if ((right & 1u) != 0) {
      result ^= left;
    }
    left = (uint8_t) ((uint8_t) (left << 1u) ^ ((left & 0x80u) != 0 ? 0x1du : 0u));
    right >>= 1u;
  }

  return result;
}

/**
 * @brief Raises a field element to an unsigned integer power.
 *
 * @param value Field element.
 * @param exponent Nonnegative exponent.
 * @return The field power.
 */
static uint8_t gf_power(uint8_t value, uint8_t exponent) {
  uint8_t result = 1;

  while (exponent != 0) {
    if ((exponent & 1u) != 0) {
      result = gf_multiply(result, value);
    }
    value = gf_multiply(value, value);
    exponent >>= 1u;
  }

  return result;
}

/**
 * @brief Inverts one known nonzero canonical field element.
 *
 * @param value Nonzero field element.
 * @return Its multiplicative inverse.
 */
static uint8_t gf_inverse(uint8_t value) {
  return gf_power(value, 254u);
}

/**
 * @brief XOR-accumulates one scaled shard into an output shard.
 *
 * @param output Writable accumulator.
 * @param input Readable shard bytes.
 * @param coefficient Field multiplier.
 * @param size Number of bytes in both shards.
 */
static void add_scaled_shard(
  uint8_t *output,
  const uint8_t *input,
  uint8_t coefficient,
  size_t size
) {
  size_t index;

  if (coefficient == 1) {
    for (index = 0; index < size; ++index) {
      output[index] ^= input[index];
    }
    return;
  }
  for (index = 0; index < size; ++index) {
    output[index] ^= gf_multiply(coefficient, input[index]);
  }
}

/**
 * @brief Tests whether a direction enumerator is known.
 *
 * @param direction Direction to test.
 * @return True when the direction is a version 1 value.
 */
static bool is_known_direction(MoonlightProtocolDirection direction) {
  return direction == MOONLIGHT_PROTOCOL_DIRECTION_CLIENT_TO_HOST ||
         direction == MOONLIGHT_PROTOCOL_DIRECTION_HOST_TO_CLIENT;
}

/**
 * @brief Tests whether a channel enumerator is known.
 *
 * @param channel Channel to test.
 * @return True when the channel is in the version 1 registry.
 */
static bool is_known_channel(MoonlightProtocolV1DatagramChannel channel) {
  return channel >= MOONLIGHT_PROTOCOL_V1_CHANNEL_VIDEO_DATA &&
         channel <= MOONLIGHT_PROTOCOL_V1_CHANNEL_MEDIA_FEEDBACK;
}

/**
 * @brief Tests whether a channel carries Video shards.
 *
 * @param channel Channel to test.
 * @return True for the Video data and parity channels.
 */
static bool is_video_channel(MoonlightProtocolV1DatagramChannel channel) {
  return channel == MOONLIGHT_PROTOCOL_V1_CHANNEL_VIDEO_DATA ||
         channel == MOONLIGHT_PROTOCOL_V1_CHANNEL_VIDEO_PARITY;
}

/**
 * @brief Tests whether a channel carries Audio shards.
 *
 * @param channel Channel to test.
 * @return True for the Audio data and parity channels.
 */
static bool is_audio_channel(MoonlightProtocolV1DatagramChannel channel) {
  return channel == MOONLIGHT_PROTOCOL_V1_CHANNEL_AUDIO_DATA ||
         channel == MOONLIGHT_PROTOCOL_V1_CHANNEL_AUDIO_PARITY;
}

/**
 * @brief Maps one media channel to its explicit context bit.
 *
 * @param channel Known Video or Audio channel to map.
 * @return The corresponding media-channel bit.
 */
static uint8_t media_channel_mask(MoonlightProtocolV1DatagramChannel channel) {
  if (channel == MOONLIGHT_PROTOCOL_V1_CHANNEL_VIDEO_DATA) {
    return MOONLIGHT_PROTOCOL_V1_MEDIA_CHANNEL_VIDEO_DATA;
  }
  if (channel == MOONLIGHT_PROTOCOL_V1_CHANNEL_VIDEO_PARITY) {
    return MOONLIGHT_PROTOCOL_V1_MEDIA_CHANNEL_VIDEO_PARITY;
  }
  if (channel == MOONLIGHT_PROTOCOL_V1_CHANNEL_AUDIO_DATA) {
    return MOONLIGHT_PROTOCOL_V1_MEDIA_CHANNEL_AUDIO_DATA;
  }
  return MOONLIGHT_PROTOCOL_V1_MEDIA_CHANNEL_AUDIO_PARITY;
}

/**
 * @brief Validates one caller-supplied active Video context.
 *
 * @param context Context to validate.
 * @return The codec result.
 */
static MoonlightProtocolResult validate_video_context(
  const MoonlightProtocolV1VideoContext *context
) {
  const uint8_t video_mask =
    MOONLIGHT_PROTOCOL_V1_MEDIA_CHANNEL_VIDEO_DATA |
    MOONLIGHT_PROTOCOL_V1_MEDIA_CHANNEL_VIDEO_PARITY;

  if (context == NULL || context->session_wire_id == 0 || context->media_epoch == 0 || context->coding_shard_bytes < MOONLIGHT_PROTOCOL_V1_VIDEO_CODING_SHARD_MIN || context->coding_shard_bytes > MOONLIGHT_PROTOCOL_V1_VIDEO_CODING_SHARD_MAX || context->coding_shard_bytes % 16u != 0 || (uint32_t) context->coding_shard_bytes + MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE + MOONLIGHT_PROTOCOL_V1_VIDEO_SHARD_HEADER_SIZE > context->complete_datagram_limit || (context->enabled_channels & MOONLIGHT_PROTOCOL_V1_MEDIA_CHANNEL_VIDEO_DATA) == 0 || (context->enabled_channels & (uint8_t) ~video_mask) != 0) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }

  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Validates one caller-supplied Audio block context.
 *
 * @param context Context to validate.
 * @return The codec result.
 */
static MoonlightProtocolResult validate_audio_context(
  const MoonlightProtocolV1AudioContext *context
) {
  const uint8_t audio_mask =
    MOONLIGHT_PROTOCOL_V1_MEDIA_CHANNEL_AUDIO_DATA |
    MOONLIGHT_PROTOCOL_V1_MEDIA_CHANNEL_AUDIO_PARITY;
  uint16_t coding_capacity;

  if (context == NULL || context->session_wire_id == 0 || context->complete_datagram_limit < MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE + MOONLIGHT_PROTOCOL_V1_AUDIO_SHARD_HEADER_SIZE + MOONLIGHT_PROTOCOL_V1_CODING_SHARD_MIN || context->maximum_packet_bytes == 0 || context->maximum_packet_bytes > MOONLIGHT_PROTOCOL_V1_AUDIO_PACKET_MAX || (context->enabled_channels & MOONLIGHT_PROTOCOL_V1_MEDIA_CHANNEL_AUDIO_DATA) == 0 || (context->enabled_channels & (uint8_t) ~audio_mask) != 0) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }

  coding_capacity = (uint16_t) (((context->complete_datagram_limit -
                                  MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE -
                                  MOONLIGHT_PROTOCOL_V1_AUDIO_SHARD_HEADER_SIZE) /
                                 16u) *
                                16u);
  if (context->maximum_packet_bytes > coding_capacity - 2u) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }

  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Tests canonical Reed-Solomon row counts.
 *
 * @param data_shards Systematic row count.
 * @param parity_shards Parity row count.
 * @return True when the combined context is representable.
 */
static bool are_valid_rs_counts(
  uint16_t data_shards,
  uint16_t parity_shards
) {
  return data_shards != 0 &&
         (uint32_t) data_shards + parity_shards <=
           MOONLIGHT_PROTOCOL_V1_FEC_SHARD_MAX;
}

/**
 * @brief Validates one initialized immutable Reed-Solomon context.
 *
 * @param context Context to validate.
 * @return The codec result.
 */
static MoonlightProtocolResult validate_rs_context(
  const MoonlightProtocolV1RsContext *context
) {
  if (context == NULL || !are_valid_rs_counts(context->data_shards, context->parity_shards) || (context->parity_shards == 0) != (context->coefficients == NULL)) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }

  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Gets the required direction for a known channel.
 *
 * @param channel Known channel.
 * @return The required payload direction.
 */
static MoonlightProtocolDirection direction_for_channel(MoonlightProtocolV1DatagramChannel channel) {
  if (channel == MOONLIGHT_PROTOCOL_V1_CHANNEL_REALTIME_INPUT || channel == MOONLIGHT_PROTOCOL_V1_CHANNEL_MEDIA_FEEDBACK) {
    return MOONLIGHT_PROTOCOL_DIRECTION_CLIENT_TO_HOST;
  }

  return MOONLIGHT_PROTOCOL_DIRECTION_HOST_TO_CLIENT;
}

/**
 * @brief Validates one host-order common DATAGRAM header.
 *
 * @param header Header to validate.
 * @param direction Direction in which the payload travels.
 * @return The codec result.
 */
static MoonlightProtocolResult validate_datagram_header(
  const MoonlightProtocolV1DatagramHeader *header,
  MoonlightProtocolDirection direction
) {
  if (header == NULL || !is_known_direction(direction)) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (!is_known_channel(header->channel)) {
    return MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED;
  }
  if (direction_for_channel(header->channel) != direction) {
    return MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH;
  }
  if (header->session_wire_id == 0) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }

  if (is_video_channel(header->channel)) {
    if ((header->flags & (uint8_t) ~VIDEO_FLAG_MASK) != 0) {
      return MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED;
    }
    if ((header->flags & MOONLIGHT_PROTOCOL_V1_VIDEO_FLAG_KEY_FRAME) != 0 && (header->flags & MOONLIGHT_PROTOCOL_V1_VIDEO_FLAG_REFERENCE_RECOVERY) != 0) {
      return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
    }
    if (header->media_epoch == 0) {
      return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
    }
  } else {
    if (header->flags != 0) {
      return MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED;
    }
    if (header->channel == MOONLIGHT_PROTOCOL_V1_CHANNEL_MEDIA_FEEDBACK) {
      if (header->media_epoch == 0) {
        return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
      }
    } else if (header->media_epoch != 0) {
      return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
    }
  }

  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Writes a validated common DATAGRAM header.
 *
 * @param header Valid host-order header.
 * @param output Sixteen writable bytes.
 */
static void write_datagram_header(
  const MoonlightProtocolV1DatagramHeader *header,
  uint8_t *output
) {
  output[0] = (uint8_t) header->channel;
  output[1] = header->flags;
  store_u16_be(output + 2, 0);
  store_u32_be(output + 4, header->session_wire_id);
  store_u32_be(output + 8, header->media_epoch);
  store_u32_be(output + 12, header->sequence);
}

MoonlightProtocolResult MoonlightProtocolV1EncodeDatagramHeader(
  const MoonlightProtocolV1DatagramHeader *header,
  MoonlightProtocolDirection direction,
  uint8_t *output,
  size_t output_size
) {
  MoonlightProtocolResult result;

  if (output == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }

  result = validate_datagram_header(header, direction);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (output_size < MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE) {
    return MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL;
  }

  write_datagram_header(header, output);
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

MoonlightProtocolResult MoonlightProtocolV1DecodeDatagramHeader(
  const uint8_t *input,
  size_t input_size,
  MoonlightProtocolDirection direction,
  MoonlightProtocolV1DatagramHeader *header
) {
  MoonlightProtocolV1DatagramHeader decoded;
  MoonlightProtocolResult result;

  if (input == NULL || header == NULL || !is_known_direction(direction)) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (input_size < MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE) {
    return MOONLIGHT_PROTOCOL_RESULT_TRUNCATED;
  }
  if (load_u16_be(input + 2) != 0) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }

  decoded.channel = (MoonlightProtocolV1DatagramChannel) input[0];
  decoded.flags = input[1];
  decoded.session_wire_id = load_u32_be(input + 4);
  decoded.media_epoch = load_u32_be(input + 8);
  decoded.sequence = load_u32_be(input + 12);

  result = validate_datagram_header(&decoded, direction);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }

  *header = decoded;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Tests whether a coding-shard size is structurally valid.
 *
 * @param size Coding-shard size.
 * @return True when the size is a canonical nonzero multiple of 16.
 */
static bool is_valid_coding_shard_size(size_t size) {
  return size >= MOONLIGHT_PROTOCOL_V1_CODING_SHARD_MIN &&
         size <= MOONLIGHT_PROTOCOL_V1_VIDEO_CODING_SHARD_MAX &&
         size % 16u == 0;
}

MoonlightProtocolResult MoonlightProtocolV1BuildDataCodingShard(
  const uint8_t *codec_bytes,
  size_t codec_size,
  uint8_t *output,
  size_t output_size
) {
  if (codec_bytes == NULL || output == NULL || !is_valid_coding_shard_size(output_size)) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (codec_size == 0) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (codec_size > output_size - 2u || codec_size > UINT16_MAX) {
    return MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL;
  }

  memmove(output + 2, codec_bytes, codec_size);
  store_u16_be(output, (uint16_t) codec_size);
  memset(output + 2 + codec_size, 0, output_size - 2u - codec_size);
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

MoonlightProtocolResult MoonlightProtocolV1ParseDataCodingShard(
  const uint8_t *input,
  size_t input_size,
  size_t maximum_meaningful_bytes,
  MoonlightProtocolV1DataShardView *view
) {
  MoonlightProtocolV1DataShardView decoded;
  uint16_t meaningful_bytes;
  size_t i;

  if (input == NULL || view == NULL || !is_valid_coding_shard_size(input_size)) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (maximum_meaningful_bytes == 0 || maximum_meaningful_bytes > input_size - 2u) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }

  meaningful_bytes = load_u16_be(input);
  if (meaningful_bytes == 0 || meaningful_bytes > maximum_meaningful_bytes || meaningful_bytes > input_size - 2u) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  for (i = 2u + meaningful_bytes; i < input_size; ++i) {
    if (input[i] != 0) {
      return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
    }
  }

  decoded.meaningful_bytes = meaningful_bytes;
  decoded.bytes = input + 2;
  *view = decoded;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Inverts one square canonical field matrix with Gauss-Jordan elimination.
 *
 * @param matrix Writable row-major matrix destroyed by elimination.
 * @param inverse Writable row-major destination initialized by this function.
 * @param dimension Matrix row and column count.
 * @return True when the matrix is nonsingular.
 */
static bool invert_field_matrix(
  uint8_t *matrix,
  uint8_t *inverse,
  size_t dimension
) {
  size_t column;
  size_t row;

  memset(inverse, 0, dimension * dimension);
  for (row = 0; row < dimension; ++row) {
    inverse[row * dimension + row] = 1;
  }

  for (column = 0; column < dimension; ++column) {
    uint8_t pivot_inverse;

    if (matrix[column * dimension + column] == 0) {
      return false;
    }
    pivot_inverse = gf_inverse(matrix[column * dimension + column]);
    for (row = 0; row < dimension; ++row) {
      matrix[column * dimension + row] =
        gf_multiply(matrix[column * dimension + row], pivot_inverse);
      inverse[column * dimension + row] =
        gf_multiply(inverse[column * dimension + row], pivot_inverse);
    }

    for (row = 0; row < dimension; ++row) {
      uint8_t factor;
      size_t index;

      if (row == column) {
        continue;
      }
      factor = matrix[row * dimension + column];
      for (index = 0; index < dimension; ++index) {
        matrix[row * dimension + index] ^=
          gf_multiply(factor, matrix[column * dimension + index]);
        inverse[row * dimension + index] ^=
          gf_multiply(factor, inverse[column * dimension + index]);
      }
    }
  }

  return true;
}

MoonlightProtocolResult MoonlightProtocolV1RsCoefficientStorageSize(
  uint16_t data_shards,
  uint16_t parity_shards,
  size_t *storage_size
) {
  if (storage_size == NULL || !are_valid_rs_counts(data_shards, parity_shards)) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }

  *storage_size = (size_t) data_shards * parity_shards;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

MoonlightProtocolResult MoonlightProtocolV1RsInitialize(
  MoonlightProtocolV1RsContext *context,
  uint16_t data_shards,
  uint16_t parity_shards,
  uint8_t *coefficient_storage,
  size_t coefficient_storage_size
) {
  MoonlightProtocolV1RsContext initialized;
  MoonlightProtocolResult result;
  size_t required_size;
  uint16_t parity_index;

  if (context == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  result = MoonlightProtocolV1RsCoefficientStorageSize(
    data_shards,
    parity_shards,
    &required_size
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (required_size != 0 && coefficient_storage == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (coefficient_storage_size < required_size) {
    return MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL;
  }
  if (required_size != 0 && (const void *) context == (const void *) coefficient_storage) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }

  for (parity_index = 0; parity_index < parity_shards; ++parity_index) {
    uint16_t data_index;

    for (data_index = 0; data_index < data_shards; ++data_index) {
      const uint8_t denominator =
        (uint8_t) ((parity_shards + data_index) ^ parity_index);
      coefficient_storage[(size_t) parity_index * data_shards + data_index] =
        gf_inverse(denominator);
    }
  }

  initialized.data_shards = data_shards;
  initialized.parity_shards = parity_shards;
  initialized.coefficients =
    parity_shards == 0 ? NULL : coefficient_storage;
  *context = initialized;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

MoonlightProtocolResult MoonlightProtocolV1RsEncode(
  const MoonlightProtocolV1RsContext *context,
  size_t shard_size,
  const uint8_t *const *data_shards,
  size_t data_shard_count,
  uint8_t *const *parity_shards,
  size_t parity_shard_count
) {
  MoonlightProtocolResult result;
  size_t left;
  uint16_t parity_index;

  result = validate_rs_context(context);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (!is_valid_coding_shard_size(shard_size) || shard_size > MOONLIGHT_PROTOCOL_V1_VIDEO_CODING_SHARD_MAX || data_shards == NULL || data_shard_count != context->data_shards || parity_shard_count != context->parity_shards || (context->parity_shards != 0 && parity_shards == NULL)) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }

  for (left = 0; left < data_shard_count; ++left) {
    size_t right;

    if (data_shards[left] == NULL) {
      return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
    }
    for (right = left + 1u; right < data_shard_count; ++right) {
      if (data_shards[left] == data_shards[right]) {
        return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
      }
    }
  }
  for (left = 0; left < parity_shard_count; ++left) {
    size_t right;
    size_t data_index;

    if (parity_shards[left] == NULL) {
      return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
    }
    if ((const void *) parity_shards[left] == (const void *) context || (const void *) parity_shards[left] == (const void *) context->coefficients || (const void *) parity_shards[left] == (const void *) data_shards || (const void *) parity_shards[left] == (const void *) parity_shards) {
      return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
    }
    for (right = left + 1u; right < parity_shard_count; ++right) {
      if (parity_shards[left] == parity_shards[right]) {
        return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
      }
    }
    for (data_index = 0; data_index < data_shard_count; ++data_index) {
      if (parity_shards[left] == data_shards[data_index]) {
        return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
      }
    }
  }

  for (parity_index = 0;
       parity_index < context->parity_shards;
       ++parity_index) {
    uint16_t data_index;

    memset(parity_shards[parity_index], 0, shard_size);
    for (data_index = 0;
         data_index < context->data_shards;
         ++data_index) {
      add_scaled_shard(
        parity_shards[parity_index],
        data_shards[data_index],
        context->coefficients[(size_t) parity_index * context->data_shards + data_index],
        shard_size
      );
    }
  }

  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

MoonlightProtocolResult MoonlightProtocolV1RsReconstructWorkspaceSize(
  const MoonlightProtocolV1RsContext *context,
  const uint8_t *marks,
  size_t mark_count,
  size_t *workspace_size
) {
  MoonlightProtocolResult result;
  size_t available_parity = 0;
  size_t missing_data = 0;
  size_t index;
  size_t total_shards;

  result = validate_rs_context(context);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  total_shards =
    (size_t) context->data_shards + context->parity_shards;
  if (marks == NULL || workspace_size == NULL || mark_count != total_shards) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }

  for (index = 0; index < total_shards; ++index) {
    if (marks[index] > 1u) {
      return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
    }
    if (index < context->data_shards && marks[index] != 0) {
      ++missing_data;
    }
    if (index >= context->data_shards && marks[index] == 0) {
      ++available_parity;
    }
  }
  if (available_parity < missing_data) {
    return MOONLIGHT_PROTOCOL_RESULT_INSUFFICIENT_SHARDS;
  }

  *workspace_size =
    2u * missing_data * missing_data + 2u * missing_data;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

MoonlightProtocolResult MoonlightProtocolV1RsReconstructData(
  const MoonlightProtocolV1RsContext *context,
  size_t shard_size,
  uint8_t *const *shards,
  size_t shard_count,
  const uint8_t *marks,
  size_t mark_count,
  uint8_t *workspace,
  size_t workspace_size
) {
  MoonlightProtocolResult result;
  size_t missing_count = 0;
  size_t required_workspace;
  size_t total_shards;
  size_t index;
  uint8_t *missing_data;
  uint8_t *selected_parity;
  uint8_t *matrix;
  uint8_t *inverse;

  result = validate_rs_context(context);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  total_shards =
    (size_t) context->data_shards + context->parity_shards;
  if (!is_valid_coding_shard_size(shard_size) || shard_size > MOONLIGHT_PROTOCOL_V1_VIDEO_CODING_SHARD_MAX || shards == NULL || shard_count != total_shards || mark_count != total_shards) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  result = MoonlightProtocolV1RsReconstructWorkspaceSize(
    context,
    marks,
    mark_count,
    &required_workspace
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (required_workspace != 0 && workspace == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (workspace_size < required_workspace) {
    return MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL;
  }
  if (required_workspace != 0 && ((const void *) workspace == (const void *) marks || (const void *) workspace == (const void *) shards || (const void *) workspace == (const void *) context || (const void *) workspace == (const void *) context->coefficients)) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }

  for (index = 0; index < total_shards; ++index) {
    size_t right;
    const bool active = marks[index] == 0 ||
                        index < context->data_shards;

    if (active && shards[index] == NULL) {
      return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
    }
    if (!active || shards[index] == NULL) {
      continue;
    }
    if (required_workspace != 0 && ((const void *) shards[index] == (const void *) workspace || (const void *) shards[index] == (const void *) marks || (const void *) shards[index] == (const void *) shards || (const void *) shards[index] == (const void *) context || (const void *) shards[index] == (const void *) context->coefficients)) {
      return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
    }
    for (right = index + 1u; right < total_shards; ++right) {
      const bool right_active =
        marks[right] == 0 || right < context->data_shards;

      if (right_active && shards[right] != NULL && shards[index] == shards[right]) {
        return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
      }
    }
  }
  if (required_workspace == 0) {
    return MOONLIGHT_PROTOCOL_RESULT_OK;
  }

  for (index = 0; index < context->data_shards; ++index) {
    if (marks[index] != 0) {
      ++missing_count;
    }
  }
  missing_data = workspace;
  selected_parity = missing_data + missing_count;
  matrix = selected_parity + missing_count;
  inverse = matrix + missing_count * missing_count;

  missing_count = 0;
  for (index = 0; index < context->data_shards; ++index) {
    if (marks[index] != 0) {
      missing_data[missing_count++] = (uint8_t) index;
    }
  }
  {
    size_t selected_count = 0;

    for (index = 0;
         index < context->parity_shards &&
         selected_count < missing_count;
         ++index) {
      if (marks[context->data_shards + index] == 0) {
        selected_parity[selected_count++] = (uint8_t) index;
      }
    }
  }

  for (index = 0; index < missing_count; ++index) {
    size_t column;

    for (column = 0; column < missing_count; ++column) {
      matrix[index * missing_count + column] =
        context->coefficients[(size_t) selected_parity[index] * context->data_shards + missing_data[column]];
    }
  }
  if (!invert_field_matrix(matrix, inverse, missing_count)) {
    return MOONLIGHT_PROTOCOL_RESULT_INTERNAL_ERROR;
  }

  for (index = 0; index < missing_count; ++index) {
    uint8_t *destination = shards[missing_data[index]];
    size_t equation;
    uint16_t data_index;

    memset(destination, 0, shard_size);
    for (equation = 0; equation < missing_count; ++equation) {
      add_scaled_shard(
        destination,
        shards[context->data_shards + selected_parity[equation]],
        inverse[index * missing_count + equation],
        shard_size
      );
    }
    for (data_index = 0;
         data_index < context->data_shards;
         ++data_index) {
      uint8_t coefficient = 0;

      if (marks[data_index] != 0) {
        continue;
      }
      for (equation = 0;
           equation < missing_count;
           ++equation) {
        coefficient ^= gf_multiply(
          inverse[index * missing_count + equation],
          context->coefficients[(size_t) selected_parity[equation] * context->data_shards + data_index]
        );
      }
      add_scaled_shard(
        destination,
        shards[data_index],
        coefficient,
        shard_size
      );
    }
  }

  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Validates one Video header and its complete coding shard.
 *
 * @param context Exact active Video context.
 * @param datagram Outer header.
 * @param shard Video Shard header.
 * @param coding_shard Complete coding shard.
 * @param coding_shard_size Size of the coding shard.
 * @param data_view Receives the data view for a data channel.
 * @return The codec result.
 */
static MoonlightProtocolResult validate_video_datagram(
  const MoonlightProtocolV1VideoContext *context,
  const MoonlightProtocolV1DatagramHeader *datagram,
  const MoonlightProtocolV1VideoShardHeader *shard,
  const uint8_t *coding_shard,
  size_t coding_shard_size,
  MoonlightProtocolV1DataShardView *data_view
) {
  MoonlightProtocolResult result;
  uint32_t total_shards;
  bool is_parity;
  bool should_start;
  bool should_end;

  result = validate_video_context(context);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (shard == NULL || coding_shard == NULL || data_view == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }

  result = validate_datagram_header(datagram, MOONLIGHT_PROTOCOL_DIRECTION_HOST_TO_CLIENT);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (!is_video_channel(datagram->channel)) {
    return MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH;
  }
  if (datagram->session_wire_id != context->session_wire_id || datagram->media_epoch != context->media_epoch) {
    return MOONLIGHT_PROTOCOL_RESULT_STALE_CONTEXT;
  }
  if ((context->enabled_channels & media_channel_mask(datagram->channel)) == 0 || ((datagram->flags & MOONLIGHT_PROTOCOL_V1_VIDEO_FLAG_REFERENCE_RECOVERY) != 0 && !context->reference_recovery_enabled)) {
    return MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH;
  }
  if (shard->frame_id == 0 || shard->frame_id > INT32_MAX || shard->fec_block_count == 0 || shard->fec_block_count > MOONLIGHT_PROTOCOL_V1_VIDEO_FEC_BLOCK_MAX || shard->fec_block_index >= shard->fec_block_count || shard->data_shard_count == 0) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }

  total_shards = (uint32_t) shard->data_shard_count + shard->parity_shard_count;
  if (total_shards > MOONLIGHT_PROTOCOL_V1_FEC_SHARD_MAX || shard->shard_index >= total_shards) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  if (shard->coding_shard_bytes < MOONLIGHT_PROTOCOL_V1_VIDEO_CODING_SHARD_MIN || shard->coding_shard_bytes > MOONLIGHT_PROTOCOL_V1_VIDEO_CODING_SHARD_MAX || shard->coding_shard_bytes % 16u != 0 || coding_shard_size != shard->coding_shard_bytes) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  if (shard->coding_shard_bytes != context->coding_shard_bytes) {
    return MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH;
  }

  is_parity = datagram->channel == MOONLIGHT_PROTOCOL_V1_CHANNEL_VIDEO_PARITY;
  if ((!is_parity && shard->shard_index >= shard->data_shard_count) || (is_parity && (shard->parity_shard_count == 0 || shard->shard_index < shard->data_shard_count))) {
    return MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH;
  }

  should_start = !is_parity &&
                 shard->fec_block_index == 0 &&
                 shard->shard_index == 0;
  should_end = !is_parity &&
               shard->fec_block_index + 1u == shard->fec_block_count &&
               shard->shard_index + 1u == shard->data_shard_count;
  if (((datagram->flags & MOONLIGHT_PROTOCOL_V1_VIDEO_FLAG_START_OF_FRAME) != 0) != should_start || ((datagram->flags & MOONLIGHT_PROTOCOL_V1_VIDEO_FLAG_END_OF_FRAME) != 0) != should_end) {
    return MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH;
  }

  data_view->meaningful_bytes = 0;
  data_view->bytes = NULL;
  if (!is_parity) {
    result = MoonlightProtocolV1ParseDataCodingShard(
      coding_shard,
      coding_shard_size,
      coding_shard_size - 2u,
      data_view
    );
    if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
      return result;
    }
    if (!should_end && data_view->meaningful_bytes != coding_shard_size - 2u) {
      return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
    }
  }

  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Writes one validated Video Shard header.
 *
 * @param shard Valid Video Shard header.
 * @param output Twenty-four writable bytes.
 */
static void write_video_shard_header(
  const MoonlightProtocolV1VideoShardHeader *shard,
  uint8_t *output
) {
  store_u32_be(output, shard->frame_id);
  store_u16_be(output + 4, shard->fec_block_index);
  store_u16_be(output + 6, shard->fec_block_count);
  store_u16_be(output + 8, shard->shard_index);
  store_u16_be(output + 10, shard->data_shard_count);
  store_u16_be(output + 12, shard->parity_shard_count);
  store_u16_be(output + 14, shard->coding_shard_bytes);
  store_u32_be(output + 16, shard->codec_configuration_generation);
  store_u32_be(output + 20, 0);
}

MoonlightProtocolResult MoonlightProtocolV1EncodeVideoDatagram(
  const MoonlightProtocolV1VideoContext *context,
  const MoonlightProtocolV1DatagramHeader *datagram,
  const MoonlightProtocolV1VideoShardHeader *shard,
  const uint8_t *coding_shard,
  size_t coding_shard_size,
  uint8_t *output,
  size_t output_size,
  size_t *encoded_size
) {
  MoonlightProtocolV1DataShardView data_view;
  MoonlightProtocolResult result;
  size_t required_size;

  if (output == NULL || encoded_size == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  *encoded_size = 0;

  result = validate_video_datagram(context, datagram, shard, coding_shard, coding_shard_size, &data_view);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }

  required_size = MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE +
                  MOONLIGHT_PROTOCOL_V1_VIDEO_SHARD_HEADER_SIZE +
                  coding_shard_size;
  *encoded_size = required_size;
  if (output_size < required_size) {
    return MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL;
  }

  memmove(
    output + MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE +
      MOONLIGHT_PROTOCOL_V1_VIDEO_SHARD_HEADER_SIZE,
    coding_shard,
    coding_shard_size
  );
  write_datagram_header(datagram, output);
  write_video_shard_header(shard, output + MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE);
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

MoonlightProtocolResult MoonlightProtocolV1ParseVideoDatagram(
  const MoonlightProtocolV1VideoContext *context,
  const uint8_t *input,
  size_t input_size,
  MoonlightProtocolV1VideoDatagramView *view
) {
  MoonlightProtocolV1VideoDatagramView decoded;
  MoonlightProtocolV1VideoShardHeader shard;
  MoonlightProtocolResult result;
  const uint8_t *shard_bytes;
  size_t required_size;

  result = validate_video_context(context);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (input == NULL || view == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (input_size < MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE + MOONLIGHT_PROTOCOL_V1_VIDEO_SHARD_HEADER_SIZE) {
    return MOONLIGHT_PROTOCOL_RESULT_TRUNCATED;
  }

  result = MoonlightProtocolV1DecodeDatagramHeader(
    input,
    input_size,
    MOONLIGHT_PROTOCOL_DIRECTION_HOST_TO_CLIENT,
    &decoded.datagram
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (!is_video_channel(decoded.datagram.channel)) {
    return MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH;
  }

  shard_bytes = input + MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE;
  if (load_u32_be(shard_bytes + 20) != 0) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }

  shard.frame_id = load_u32_be(shard_bytes);
  shard.fec_block_index = load_u16_be(shard_bytes + 4);
  shard.fec_block_count = load_u16_be(shard_bytes + 6);
  shard.shard_index = load_u16_be(shard_bytes + 8);
  shard.data_shard_count = load_u16_be(shard_bytes + 10);
  shard.parity_shard_count = load_u16_be(shard_bytes + 12);
  shard.coding_shard_bytes = load_u16_be(shard_bytes + 14);
  shard.codec_configuration_generation = load_u32_be(shard_bytes + 16);

  required_size = MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE +
                  MOONLIGHT_PROTOCOL_V1_VIDEO_SHARD_HEADER_SIZE +
                  shard.coding_shard_bytes;
  if (input_size < required_size) {
    return MOONLIGHT_PROTOCOL_RESULT_TRUNCATED;
  }
  if (input_size > required_size) {
    return MOONLIGHT_PROTOCOL_RESULT_TRAILING_DATA;
  }

  decoded.shard = shard;
  decoded.coding_shard = shard_bytes + MOONLIGHT_PROTOCOL_V1_VIDEO_SHARD_HEADER_SIZE;
  decoded.coding_shard_size = shard.coding_shard_bytes;
  result = validate_video_datagram(
    context,
    &decoded.datagram,
    &decoded.shard,
    decoded.coding_shard,
    decoded.coding_shard_size,
    &decoded.data
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }

  decoded.is_parity = decoded.datagram.channel == MOONLIGHT_PROTOCOL_V1_CHANNEL_VIDEO_PARITY;
  *view = decoded;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Validates one Audio header and its complete coding shard.
 *
 * @param context Exact Stream Session and Audio block limit context.
 * @param datagram Outer header.
 * @param shard Audio Shard header.
 * @param coding_shard Complete coding shard.
 * @param coding_shard_size Size of the coding shard.
 * @param data_view Receives the data view for a data channel.
 * @return The codec result.
 */
static MoonlightProtocolResult validate_audio_datagram(
  const MoonlightProtocolV1AudioContext *context,
  const MoonlightProtocolV1DatagramHeader *datagram,
  const MoonlightProtocolV1AudioShardHeader *shard,
  const uint8_t *coding_shard,
  size_t coding_shard_size,
  MoonlightProtocolV1DataShardView *data_view
) {
  MoonlightProtocolResult result;
  bool is_parity;

  result = validate_audio_context(context);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (shard == NULL || coding_shard == NULL || data_view == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }

  result = validate_datagram_header(datagram, MOONLIGHT_PROTOCOL_DIRECTION_HOST_TO_CLIENT);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (!is_audio_channel(datagram->channel)) {
    return MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH;
  }
  if (datagram->session_wire_id != context->session_wire_id) {
    return MOONLIGHT_PROTOCOL_RESULT_STALE_CONTEXT;
  }
  if ((context->enabled_channels & media_channel_mask(datagram->channel)) == 0) {
    return MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH;
  }
  if (shard->shard_index >= MOONLIGHT_PROTOCOL_V1_AUDIO_DATA_SHARDS + MOONLIGHT_PROTOCOL_V1_AUDIO_PARITY_SHARDS || shard->coding_shard_bytes < MOONLIGHT_PROTOCOL_V1_CODING_SHARD_MIN || shard->coding_shard_bytes > MOONLIGHT_PROTOCOL_V1_AUDIO_CODING_SHARD_MAX || shard->coding_shard_bytes % 16u != 0 || coding_shard_size != shard->coding_shard_bytes || shard->first_pcm_sample_index % MOONLIGHT_PROTOCOL_V1_AUDIO_BLOCK_SAMPLES != 0 || (uint32_t) (shard->first_pcm_sample_index / MOONLIGHT_PROTOCOL_V1_AUDIO_BLOCK_SAMPLES) != shard->fec_block_id) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  if ((uint32_t) shard->coding_shard_bytes + MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE + MOONLIGHT_PROTOCOL_V1_AUDIO_SHARD_HEADER_SIZE > context->complete_datagram_limit) {
    return MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH;
  }

  is_parity = datagram->channel == MOONLIGHT_PROTOCOL_V1_CHANNEL_AUDIO_PARITY;
  if ((!is_parity && shard->shard_index >= MOONLIGHT_PROTOCOL_V1_AUDIO_DATA_SHARDS) || (is_parity && shard->shard_index < MOONLIGHT_PROTOCOL_V1_AUDIO_DATA_SHARDS)) {
    return MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH;
  }

  data_view->meaningful_bytes = 0;
  data_view->bytes = NULL;
  if (!is_parity) {
    result = MoonlightProtocolV1ParseDataCodingShard(
      coding_shard,
      coding_shard_size,
      coding_shard_size - 2u,
      data_view
    );
    if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
      return result;
    }
    if (data_view->meaningful_bytes > context->maximum_packet_bytes) {
      return MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH;
    }
  }

  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Writes one validated Audio Shard header.
 *
 * @param shard Valid Audio Shard header.
 * @param output Twenty writable bytes.
 */
static void write_audio_shard_header(
  const MoonlightProtocolV1AudioShardHeader *shard,
  uint8_t *output
) {
  store_u32_be(output, shard->fec_block_id);
  store_u16_be(output + 4, shard->shard_index);
  store_u16_be(output + 6, MOONLIGHT_PROTOCOL_V1_AUDIO_DATA_SHARDS);
  store_u16_be(output + 8, MOONLIGHT_PROTOCOL_V1_AUDIO_PARITY_SHARDS);
  store_u16_be(output + 10, shard->coding_shard_bytes);
  store_u64_be(output + 12, shard->first_pcm_sample_index);
}

MoonlightProtocolResult MoonlightProtocolV1EncodeAudioDatagram(
  const MoonlightProtocolV1AudioContext *context,
  const MoonlightProtocolV1DatagramHeader *datagram,
  const MoonlightProtocolV1AudioShardHeader *shard,
  const uint8_t *coding_shard,
  size_t coding_shard_size,
  uint8_t *output,
  size_t output_size,
  size_t *encoded_size
) {
  MoonlightProtocolV1DataShardView data_view;
  MoonlightProtocolResult result;
  size_t required_size;

  if (output == NULL || encoded_size == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  *encoded_size = 0;

  result = validate_audio_datagram(context, datagram, shard, coding_shard, coding_shard_size, &data_view);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }

  required_size = MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE +
                  MOONLIGHT_PROTOCOL_V1_AUDIO_SHARD_HEADER_SIZE +
                  coding_shard_size;
  *encoded_size = required_size;
  if (output_size < required_size) {
    return MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL;
  }

  memmove(
    output + MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE +
      MOONLIGHT_PROTOCOL_V1_AUDIO_SHARD_HEADER_SIZE,
    coding_shard,
    coding_shard_size
  );
  write_datagram_header(datagram, output);
  write_audio_shard_header(shard, output + MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE);
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

MoonlightProtocolResult MoonlightProtocolV1ParseAudioDatagram(
  const MoonlightProtocolV1AudioContext *context,
  const uint8_t *input,
  size_t input_size,
  MoonlightProtocolV1AudioDatagramView *view
) {
  MoonlightProtocolV1AudioDatagramView decoded;
  MoonlightProtocolV1AudioShardHeader shard;
  MoonlightProtocolResult result;
  const uint8_t *shard_bytes;
  size_t required_size;

  result = validate_audio_context(context);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (input == NULL || view == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (input_size < MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE + MOONLIGHT_PROTOCOL_V1_AUDIO_SHARD_HEADER_SIZE) {
    return MOONLIGHT_PROTOCOL_RESULT_TRUNCATED;
  }

  result = MoonlightProtocolV1DecodeDatagramHeader(
    input,
    input_size,
    MOONLIGHT_PROTOCOL_DIRECTION_HOST_TO_CLIENT,
    &decoded.datagram
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (!is_audio_channel(decoded.datagram.channel)) {
    return MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH;
  }

  shard_bytes = input + MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE;
  if (load_u16_be(shard_bytes + 6) != MOONLIGHT_PROTOCOL_V1_AUDIO_DATA_SHARDS || load_u16_be(shard_bytes + 8) != MOONLIGHT_PROTOCOL_V1_AUDIO_PARITY_SHARDS) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }

  shard.fec_block_id = load_u32_be(shard_bytes);
  shard.shard_index = load_u16_be(shard_bytes + 4);
  shard.coding_shard_bytes = load_u16_be(shard_bytes + 10);
  shard.first_pcm_sample_index = load_u64_be(shard_bytes + 12);

  required_size = MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE +
                  MOONLIGHT_PROTOCOL_V1_AUDIO_SHARD_HEADER_SIZE +
                  shard.coding_shard_bytes;
  if (input_size < required_size) {
    return MOONLIGHT_PROTOCOL_RESULT_TRUNCATED;
  }
  if (input_size > required_size) {
    return MOONLIGHT_PROTOCOL_RESULT_TRAILING_DATA;
  }

  decoded.shard = shard;
  decoded.coding_shard = shard_bytes + MOONLIGHT_PROTOCOL_V1_AUDIO_SHARD_HEADER_SIZE;
  decoded.coding_shard_size = shard.coding_shard_bytes;
  result = validate_audio_datagram(
    context,
    &decoded.datagram,
    &decoded.shard,
    decoded.coding_shard,
    decoded.coding_shard_size,
    &decoded.data
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }

  decoded.is_parity = decoded.datagram.channel == MOONLIGHT_PROTOCOL_V1_CHANNEL_AUDIO_PARITY;
  *view = decoded;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

MoonlightProtocolV1ReceivedDatagramDisposition
  MoonlightProtocolV1ClassifyReceivedDatagramResult(MoonlightProtocolResult result) {
  switch (result) {
    case MOONLIGHT_PROTOCOL_RESULT_OK:
      return MOONLIGHT_PROTOCOL_V1_DATAGRAM_DISPOSITION_ACCEPT;
    case MOONLIGHT_PROTOCOL_RESULT_STALE_CONTEXT:
      return MOONLIGHT_PROTOCOL_V1_DATAGRAM_DISPOSITION_DROP_STALE;
    case MOONLIGHT_PROTOCOL_RESULT_INSUFFICIENT_SHARDS:
    case MOONLIGHT_PROTOCOL_RESULT_INTERNAL_ERROR:
      return MOONLIGHT_PROTOCOL_V1_DATAGRAM_DISPOSITION_LOCAL_ERROR;
    case MOONLIGHT_PROTOCOL_RESULT_TRUNCATED:
    case MOONLIGHT_PROTOCOL_RESULT_TRAILING_DATA:
    case MOONLIGHT_PROTOCOL_RESULT_MALFORMED:
    case MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH:
    case MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED:
      return MOONLIGHT_PROTOCOL_V1_DATAGRAM_DISPOSITION_PROTOCOL_VIOLATION;
    case MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT:
    case MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL:
    default:
      return MOONLIGHT_PROTOCOL_V1_DATAGRAM_DISPOSITION_LOCAL_ERROR;
  }
}
