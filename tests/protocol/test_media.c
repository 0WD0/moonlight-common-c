/**
 * @file test_media.c
 * @brief Native tests for canonical Sunshine protocol version 1 media DATAGRAMs.
 */

#include <moonlight/protocol/media.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef MOONLIGHT_PROTOCOL_V1_TESTDATA_DIR
  #error MOONLIGHT_PROTOCOL_V1_TESTDATA_DIR must name the version 1 golden-vector directory
#endif

/**
 * @brief Largest golden vector accepted by the native test loader.
 */
#define TEST_VECTOR_CAPACITY 2048u

/**
 * @brief Complete byte count of either Video golden DATAGRAM.
 */
#define TEST_VIDEO_DATAGRAM_SIZE 552u

/**
 * @brief Complete byte count of either Audio golden DATAGRAM.
 */
#define TEST_AUDIO_DATAGRAM_SIZE 52u

/**
 * @brief Largest canonical Reed-Solomon coefficient matrix in bytes.
 */
#define TEST_RS_MAX_COEFFICIENTS (127u * 128u)

/**
 * @brief Offset of a Video coding shard within a complete DATAGRAM.
 */
#define TEST_VIDEO_CODING_OFFSET \
  (MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE + MOONLIGHT_PROTOCOL_V1_VIDEO_SHARD_HEADER_SIZE)

/**
 * @brief Offset of an Audio coding shard within a complete DATAGRAM.
 */
#define TEST_AUDIO_CODING_OFFSET \
  (MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE + MOONLIGHT_PROTOCOL_V1_AUDIO_SHARD_HEADER_SIZE)

/**
 * @brief Active Video context shared by canonical golden-vector tests.
 */
static const MoonlightProtocolV1VideoContext TEST_VIDEO_CONTEXT = {
  .session_wire_id = UINT32_C(0x11223344),
  .media_epoch = 1,
  .complete_datagram_limit = TEST_VIDEO_DATAGRAM_SIZE,
  .coding_shard_bytes = 512,
  .enabled_channels = MOONLIGHT_PROTOCOL_V1_MEDIA_CHANNEL_VIDEO_DATA |
                      MOONLIGHT_PROTOCOL_V1_MEDIA_CHANNEL_VIDEO_PARITY,
  .reference_recovery_enabled = true,
};

/**
 * @brief Active Audio context shared by canonical golden-vector tests.
 */
static const MoonlightProtocolV1AudioContext TEST_AUDIO_CONTEXT = {
  .session_wire_id = UINT32_C(0x11223344),
  .complete_datagram_limit = TEST_AUDIO_DATAGRAM_SIZE,
  .maximum_packet_bytes = 14,
  .enabled_channels = MOONLIGHT_PROTOCOL_V1_MEDIA_CHANNEL_AUDIO_DATA |
                      MOONLIGHT_PROTOCOL_V1_MEDIA_CHANNEL_AUDIO_PARITY,
};

/**
 * @brief Four canonical Audio data coding shards underlying the parity golden.
 */
static const uint8_t TEST_AUDIO_SOURCE_SHARDS[MOONLIGHT_PROTOCOL_V1_AUDIO_DATA_SHARDS][16] = {
  {0x00, 0x04, 0x11, 0x22, 0x33, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x00, 0x01, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x00, 0x02, 0x66, 0x77, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x00, 0x03, 0x88, 0x99, 0xaa, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
};

/**
 * @brief Canonical Audio shard-index 4 Reed-Solomon matrix row.
 */
static const uint8_t TEST_AUDIO_PARITY_ROW[MOONLIGHT_PROTOCOL_V1_AUDIO_DATA_SHARDS] = {
  0x8e,
  0xf4,
  0x47,
  0xa7,
};

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
    MoonlightProtocolResult test_result_value = (expression); \
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
 * @brief Verifies every fixed low-latency Opus Audio profile constant.
 *
 * @return True on success.
 */
static bool test_audio_profile_constants(void) {
  TEST_CHECK(MOONLIGHT_PROTOCOL_V1_AUDIO_OPUS_SAMPLE_RATE_HZ == 48000u);
  TEST_CHECK(MOONLIGHT_PROTOCOL_V1_AUDIO_OPUS_CHANNEL_COUNT == 2u);
  TEST_CHECK(MOONLIGHT_PROTOCOL_V1_AUDIO_OPUS_STREAM_COUNT == 1u);
  TEST_CHECK(MOONLIGHT_PROTOCOL_V1_AUDIO_OPUS_COUPLED_STREAM_COUNT == 1u);
  TEST_CHECK(MOONLIGHT_PROTOCOL_V1_AUDIO_OPUS_FRAME_SAMPLES == 240u);
  TEST_CHECK(MOONLIGHT_PROTOCOL_V1_AUDIO_OPUS_FRAME_DURATION_US == 5000u);
  TEST_CHECK(MOONLIGHT_PROTOCOL_V1_AUDIO_OPUS_BITRATE_BPS == 96000u);
  TEST_CHECK(
    MOONLIGHT_PROTOCOL_V1_AUDIO_OPUS_FRAME_SAMPLES *
      MOONLIGHT_PROTOCOL_V1_AUDIO_DATA_SHARDS ==
    MOONLIGHT_PROTOCOL_V1_AUDIO_BLOCK_SAMPLES
  );
  return true;
}

/**
 * @brief Converts one ASCII hexadecimal digit to its integer value.
 *
 * @param character Character to convert.
 * @return A value in `[0, 15]`, or `-1` when the character is not hexadecimal.
 */
static int hex_value(int character) {
  if (character >= '0' && character <= '9') {
    return character - '0';
  }
  if (character >= 'a' && character <= 'f') {
    return character - 'a' + 10;
  }
  if (character >= 'A' && character <= 'F') {
    return character - 'A' + 10;
  }
  return -1;
}

/**
 * @brief Tests whether a character is ASCII whitespace permitted in a golden file.
 *
 * @param character Character to test.
 * @return True for the six ASCII whitespace characters.
 */
static bool is_ascii_whitespace(int character) {
  return character == ' ' ||
         character == '\t' ||
         character == '\n' ||
         character == '\r' ||
         character == '\f' ||
         character == '\v';
}

/**
 * @brief Loads a strict hexadecimal golden file without accepting comments or prefixes.
 *
 * @param name File name relative to the version 1 test-data directory.
 * @param output Destination byte buffer.
 * @param output_capacity Available bytes in `output`.
 * @param output_size Receives the decoded byte count.
 * @return True when the file contains only complete hexadecimal byte pairs and whitespace.
 */
static bool load_golden(
  const char *name,
  uint8_t *output,
  size_t output_capacity,
  size_t *output_size
) {
  char path[1024];
  FILE *file;
  int character;
  int high_nibble = -1;
  int path_length;
  size_t size = 0;
  bool valid = true;

  path_length = snprintf(
    path,
    sizeof(path),
    "%s/%s",
    MOONLIGHT_PROTOCOL_V1_TESTDATA_DIR,
    name
  );
  if (path_length < 0 || (size_t) path_length >= sizeof(path)) {
    fprintf(stderr, "golden path is too long: %s\n", name);
    return false;
  }

  file = fopen(path, "rb");
  if (file == NULL) {
    fprintf(stderr, "cannot open golden vector: %s\n", path);
    return false;
  }

  while ((character = fgetc(file)) != EOF) {
    int nibble = hex_value(character);

    if (nibble >= 0) {
      if (high_nibble < 0) {
        high_nibble = nibble;
      } else {
        if (size >= output_capacity) {
          valid = false;
          break;
        }
        output[size++] = (uint8_t) ((high_nibble << 4) | nibble);
        high_nibble = -1;
      }
    } else if (!is_ascii_whitespace(character)) {
      valid = false;
      break;
    }
  }

  if (ferror(file) != 0 || high_nibble >= 0 || fclose(file) != 0) {
    valid = false;
  }
  if (!valid) {
    fprintf(stderr, "invalid strict hexadecimal golden vector: %s\n", path);
    return false;
  }

  *output_size = size;
  return true;
}

/**
 * @brief Stores one big-endian 16-bit integer for a negative-vector mutation.
 *
 * @param output Two writable bytes.
 * @param value Host-order value.
 */
static void store_test_u16(uint8_t *output, uint16_t value) {
  output[0] = (uint8_t) (value >> 8);
  output[1] = (uint8_t) value;
}

/**
 * @brief Stores one big-endian 32-bit integer for a negative-vector mutation.
 *
 * @param output Four writable bytes.
 * @param value Host-order value.
 */
static void store_test_u32(uint8_t *output, uint32_t value) {
  output[0] = (uint8_t) (value >> 24);
  output[1] = (uint8_t) (value >> 16);
  output[2] = (uint8_t) (value >> 8);
  output[3] = (uint8_t) value;
}

/**
 * @brief Multiplies two bytes in the canonical version 1 `GF(256)` field.
 *
 * @param left Left field element.
 * @param right Right field element.
 * @return Product reduced by primitive polynomial `0x11d`.
 */
static uint8_t multiply_test_gf256(uint8_t left, uint8_t right) {
  uint8_t product = 0;

  while (right != 0) {
    if ((right & 1u) != 0) {
      product ^= left;
    }
    left = (uint8_t) ((left << 1u) ^ ((left & 0x80u) != 0 ? 0x1du : 0u));
    right >>= 1u;
  }
  return product;
}

/**
 * @brief Checks common fields shared by all four golden DATAGRAMs.
 *
 * @param datagram Decoded outer header.
 * @param channel Expected channel.
 * @param sequence Expected channel sequence.
 * @return True when the fields match the corpus contract.
 */
static bool check_common_golden_header(
  const MoonlightProtocolV1DatagramHeader *datagram,
  MoonlightProtocolV1DatagramChannel channel,
  uint32_t sequence
) {
  TEST_CHECK(datagram->channel == channel);
  TEST_CHECK(datagram->session_wire_id == UINT32_C(0x11223344));
  TEST_CHECK(datagram->sequence == sequence);
  return true;
}

/**
 * @brief Parses and re-encodes the Video data golden vector.
 *
 * @return True when semantic fields and canonical bytes match.
 */
static bool test_video_data_golden(void) {
  static const uint8_t codec_bytes[] = {0xde, 0xad, 0xbe, 0xef, 0x42};
  uint8_t encoded[TEST_VECTOR_CAPACITY];
  uint8_t vector[TEST_VECTOR_CAPACITY];
  MoonlightProtocolV1VideoDatagramView view;
  size_t encoded_size = 0;
  size_t vector_size = 0;

  TEST_CHECK(load_golden("video-data.hex", vector, sizeof(vector), &vector_size));
  TEST_CHECK(vector_size == TEST_VIDEO_DATAGRAM_SIZE);
  TEST_RESULT(
    MoonlightProtocolV1ParseVideoDatagram(
      &TEST_VIDEO_CONTEXT,
      vector,
      vector_size,
      &view
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(check_common_golden_header(
    &view.datagram,
    MOONLIGHT_PROTOCOL_V1_CHANNEL_VIDEO_DATA,
    UINT32_C(0x01020304)
  ));
  TEST_CHECK(view.datagram.flags == (MOONLIGHT_PROTOCOL_V1_VIDEO_FLAG_KEY_FRAME | MOONLIGHT_PROTOCOL_V1_VIDEO_FLAG_START_OF_FRAME | MOONLIGHT_PROTOCOL_V1_VIDEO_FLAG_END_OF_FRAME));
  TEST_CHECK(view.datagram.media_epoch == 1);
  TEST_CHECK(view.shard.frame_id == UINT32_C(0x01020304));
  TEST_CHECK(view.shard.fec_block_index == 0);
  TEST_CHECK(view.shard.fec_block_count == 1);
  TEST_CHECK(view.shard.shard_index == 0);
  TEST_CHECK(view.shard.data_shard_count == 1);
  TEST_CHECK(view.shard.parity_shard_count == 1);
  TEST_CHECK(view.shard.coding_shard_bytes == 512);
  TEST_CHECK(view.shard.codec_configuration_generation == UINT32_C(0xa1b2c3d4));
  TEST_CHECK(!view.is_parity);
  TEST_CHECK(view.coding_shard == vector + TEST_VIDEO_CODING_OFFSET);
  TEST_CHECK(view.coding_shard_size == 512);
  TEST_CHECK(view.data.meaningful_bytes == sizeof(codec_bytes));
  TEST_CHECK(memcmp(view.data.bytes, codec_bytes, sizeof(codec_bytes)) == 0);

  TEST_RESULT(
    MoonlightProtocolV1EncodeVideoDatagram(
      &TEST_VIDEO_CONTEXT,
      &view.datagram,
      &view.shard,
      view.coding_shard,
      view.coding_shard_size,
      encoded,
      sizeof(encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(encoded_size == vector_size);
  TEST_CHECK(memcmp(encoded, vector, vector_size) == 0);
  return true;
}

/**
 * @brief Parses and re-encodes the Video parity golden vector.
 *
 * @return True when semantic fields and canonical bytes match.
 */
static bool test_video_parity_golden(void) {
  uint8_t data_vector[TEST_VECTOR_CAPACITY];
  uint8_t encoded[TEST_VECTOR_CAPACITY];
  uint8_t vector[TEST_VECTOR_CAPACITY];
  MoonlightProtocolV1VideoDatagramView view;
  size_t data_vector_size = 0;
  size_t encoded_size = 0;
  size_t vector_size = 0;

  TEST_CHECK(load_golden(
    "video-data.hex",
    data_vector,
    sizeof(data_vector),
    &data_vector_size
  ));
  TEST_CHECK(load_golden("video-parity.hex", vector, sizeof(vector), &vector_size));
  TEST_CHECK(data_vector_size == TEST_VIDEO_DATAGRAM_SIZE);
  TEST_CHECK(vector_size == TEST_VIDEO_DATAGRAM_SIZE);
  TEST_RESULT(
    MoonlightProtocolV1ParseVideoDatagram(
      &TEST_VIDEO_CONTEXT,
      vector,
      vector_size,
      &view
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(check_common_golden_header(
    &view.datagram,
    MOONLIGHT_PROTOCOL_V1_CHANNEL_VIDEO_PARITY,
    UINT32_C(0x01020305)
  ));
  TEST_CHECK(view.datagram.flags == MOONLIGHT_PROTOCOL_V1_VIDEO_FLAG_KEY_FRAME);
  TEST_CHECK(view.datagram.media_epoch == 1);
  TEST_CHECK(view.shard.frame_id == UINT32_C(0x01020304));
  TEST_CHECK(view.shard.shard_index == 1);
  TEST_CHECK(view.shard.data_shard_count == 1);
  TEST_CHECK(view.shard.parity_shard_count == 1);
  TEST_CHECK(view.shard.coding_shard_bytes == 512);
  TEST_CHECK(view.is_parity);
  TEST_CHECK(view.data.meaningful_bytes == 0);
  TEST_CHECK(view.data.bytes == NULL);
  TEST_CHECK(memcmp(view.coding_shard, data_vector + TEST_VIDEO_CODING_OFFSET, view.coding_shard_size) == 0);

  TEST_RESULT(
    MoonlightProtocolV1EncodeVideoDatagram(
      &TEST_VIDEO_CONTEXT,
      &view.datagram,
      &view.shard,
      view.coding_shard,
      view.coding_shard_size,
      encoded,
      sizeof(encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(encoded_size == vector_size);
  TEST_CHECK(memcmp(encoded, vector, vector_size) == 0);
  return true;
}

/**
 * @brief Parses and re-encodes the Audio data golden vector.
 *
 * @return True when semantic fields and canonical bytes match.
 */
static bool test_audio_data_golden(void) {
  static const uint8_t codec_bytes[] = {0x11, 0x22, 0x33, 0x44};
  uint8_t encoded[TEST_VECTOR_CAPACITY];
  uint8_t vector[TEST_VECTOR_CAPACITY];
  MoonlightProtocolV1AudioDatagramView view;
  size_t encoded_size = 0;
  size_t vector_size = 0;

  TEST_CHECK(load_golden("audio-data.hex", vector, sizeof(vector), &vector_size));
  TEST_CHECK(vector_size == TEST_AUDIO_DATAGRAM_SIZE);
  TEST_RESULT(
    MoonlightProtocolV1ParseAudioDatagram(
      &TEST_AUDIO_CONTEXT,
      vector,
      vector_size,
      &view
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(check_common_golden_header(
    &view.datagram,
    MOONLIGHT_PROTOCOL_V1_CHANNEL_AUDIO_DATA,
    UINT32_C(0x0a0b0c0d)
  ));
  TEST_CHECK(view.datagram.flags == 0);
  TEST_CHECK(view.datagram.media_epoch == 0);
  TEST_CHECK(view.shard.fec_block_id == 2);
  TEST_CHECK(view.shard.shard_index == 0);
  TEST_CHECK(view.shard.coding_shard_bytes == 16);
  TEST_CHECK(view.shard.first_pcm_sample_index == UINT64_C(1920));
  TEST_CHECK(!view.is_parity);
  TEST_CHECK(view.coding_shard == vector + TEST_AUDIO_CODING_OFFSET);
  TEST_CHECK(view.coding_shard_size == 16);
  TEST_CHECK(memcmp(view.coding_shard, TEST_AUDIO_SOURCE_SHARDS[0], view.coding_shard_size) == 0);
  TEST_CHECK(view.data.meaningful_bytes == sizeof(codec_bytes));
  TEST_CHECK(memcmp(view.data.bytes, codec_bytes, sizeof(codec_bytes)) == 0);

  TEST_RESULT(
    MoonlightProtocolV1EncodeAudioDatagram(
      &TEST_AUDIO_CONTEXT,
      &view.datagram,
      &view.shard,
      view.coding_shard,
      view.coding_shard_size,
      encoded,
      sizeof(encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(encoded_size == vector_size);
  TEST_CHECK(memcmp(encoded, vector, vector_size) == 0);
  return true;
}

/**
 * @brief Parses and re-encodes the Audio parity golden vector.
 *
 * @return True when semantic fields and canonical bytes match.
 */
static bool test_audio_parity_golden(void) {
  uint8_t encoded[TEST_VECTOR_CAPACITY];
  uint8_t vector[TEST_VECTOR_CAPACITY];
  MoonlightProtocolV1AudioDatagramView view;
  size_t encoded_size = 0;
  size_t data_index;
  size_t i;
  size_t vector_size = 0;

  TEST_CHECK(load_golden("audio-parity.hex", vector, sizeof(vector), &vector_size));
  TEST_CHECK(vector_size == TEST_AUDIO_DATAGRAM_SIZE);
  TEST_RESULT(
    MoonlightProtocolV1ParseAudioDatagram(
      &TEST_AUDIO_CONTEXT,
      vector,
      vector_size,
      &view
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(check_common_golden_header(
    &view.datagram,
    MOONLIGHT_PROTOCOL_V1_CHANNEL_AUDIO_PARITY,
    UINT32_C(0x0a0b0c0e)
  ));
  TEST_CHECK(view.shard.fec_block_id == 2);
  TEST_CHECK(view.shard.shard_index == 4);
  TEST_CHECK(view.shard.coding_shard_bytes == 16);
  TEST_CHECK(view.shard.first_pcm_sample_index == UINT64_C(1920));
  TEST_CHECK(view.is_parity);
  TEST_CHECK(view.data.meaningful_bytes == 0);
  TEST_CHECK(view.data.bytes == NULL);
  for (i = 0; i < view.coding_shard_size; ++i) {
    uint8_t expected = 0;

    for (data_index = 0;
         data_index < MOONLIGHT_PROTOCOL_V1_AUDIO_DATA_SHARDS;
         ++data_index) {
      expected ^= multiply_test_gf256(
        TEST_AUDIO_PARITY_ROW[data_index],
        TEST_AUDIO_SOURCE_SHARDS[data_index][i]
      );
    }
    TEST_CHECK(view.coding_shard[i] == expected);
  }

  TEST_RESULT(
    MoonlightProtocolV1EncodeAudioDatagram(
      &TEST_AUDIO_CONTEXT,
      &view.datagram,
      &view.shard,
      view.coding_shard,
      view.coding_shard_size,
      encoded,
      sizeof(encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(encoded_size == vector_size);
  TEST_CHECK(memcmp(encoded, vector, vector_size) == 0);
  return true;
}

/**
 * @brief Exercises canonical Reed-Solomon contexts and parity encoding.
 *
 * @return True when matrix anchors, zero parity, and both golden profiles match.
 */
static bool test_reed_solomon_encoding(void) {
  union {
    MoonlightProtocolV1RsContext context;
    uint8_t bytes[MOONLIGHT_PROTOCOL_V1_CODING_SHARD_MIN];
  } aliased_initialization, guarded_context;

  union {
    uint8_t *pointers[MOONLIGHT_PROTOCOL_V1_AUDIO_PARITY_SHARDS];
    uint8_t bytes[MOONLIGHT_PROTOCOL_V1_CODING_SHARD_MIN];
  } aliased_parity_pointers;

  static const uint8_t expected_audio_coefficients[] = {
    0x8e,
    0xf4,
    0x47,
    0xa7,
    0xf4,
    0x8e,
    0xa7,
    0x47,
  };
  static const uint8_t legacy_audio_coefficients[] = {
    0x77,
    0x40,
    0x38,
    0x0e,
    0xc7,
    0xa7,
    0x0d,
    0x6c,
  };
  uint8_t aliased_initialization_before[sizeof(aliased_initialization)];
  uint8_t aliased_parity_pointers_before[sizeof(aliased_parity_pointers)];
  uint8_t audio_coefficients[sizeof(expected_audio_coefficients)];
  uint8_t audio_data_pointers_before[sizeof(
    const uint8_t *[MOONLIGHT_PROTOCOL_V1_AUDIO_DATA_SHARDS]
  )];
  uint8_t legacy_audio_parity[MOONLIGHT_PROTOCOL_V1_AUDIO_PARITY_SHARDS][16];
  uint8_t maximum_coefficients[TEST_RS_MAX_COEFFICIENTS];
  uint8_t audio_parity[MOONLIGHT_PROTOCOL_V1_AUDIO_PARITY_SHARDS][16];
  uint8_t guarded_coefficients[MOONLIGHT_PROTOCOL_V1_CODING_SHARD_MIN];
  uint8_t guarded_coefficients_before[sizeof(guarded_coefficients)];
  uint8_t guarded_context_before[sizeof(guarded_context)];
  uint8_t video_coefficients[1];
  uint8_t video_data[TEST_VECTOR_CAPACITY];
  uint8_t video_parity[TEST_VECTOR_CAPACITY];
  uint8_t video_output[512];
  const uint8_t *audio_data_pointers[MOONLIGHT_PROTOCOL_V1_AUDIO_DATA_SHARDS];
  uint8_t *audio_parity_pointers[MOONLIGHT_PROTOCOL_V1_AUDIO_PARITY_SHARDS];
  const uint8_t *video_data_pointer;
  uint8_t *video_parity_pointer = video_output;
  MoonlightProtocolV1RsContext audio_context;
  MoonlightProtocolV1RsContext maximum_context;
  MoonlightProtocolV1RsContext video_context;
  MoonlightProtocolV1RsContext zero_parity_context;
  size_t storage_size = 0;
  size_t video_data_size = 0;
  size_t video_parity_size = 0;
  size_t i;

  TEST_CHECK(multiply_test_gf256(0x8e, 0x02) == 1);
  TEST_CHECK(multiply_test_gf256(0xf4, 0x03) == 1);
  TEST_CHECK(multiply_test_gf256(0x47, 0x04) == 1);
  TEST_CHECK(multiply_test_gf256(0xa7, 0x05) == 1);

  TEST_RESULT(
    MoonlightProtocolV1RsCoefficientStorageSize(
      MOONLIGHT_PROTOCOL_V1_AUDIO_DATA_SHARDS,
      MOONLIGHT_PROTOCOL_V1_AUDIO_PARITY_SHARDS,
      &storage_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(storage_size == sizeof(audio_coefficients));
  TEST_RESULT(
    MoonlightProtocolV1RsCoefficientStorageSize(255, 0, &storage_size),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(storage_size == 0);
  TEST_RESULT(
    MoonlightProtocolV1RsCoefficientStorageSize(127, 128, &storage_size),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(storage_size == TEST_RS_MAX_COEFFICIENTS);
  TEST_RESULT(
    MoonlightProtocolV1RsInitialize(
      &maximum_context,
      127,
      128,
      maximum_coefficients,
      sizeof(maximum_coefficients)
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(maximum_context.data_shards == 127);
  TEST_CHECK(maximum_context.parity_shards == 128);
  TEST_CHECK(multiply_test_gf256(maximum_coefficients[0], 0x80) == 1);
  TEST_CHECK(multiply_test_gf256(maximum_coefficients[TEST_RS_MAX_COEFFICIENTS - 1], 0x81) == 1);
  TEST_RESULT(
    MoonlightProtocolV1RsCoefficientStorageSize(0, 1, &storage_size),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1RsCoefficientStorageSize(1, 255, &storage_size),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1RsCoefficientStorageSize(1, 1, NULL),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );

  memset(audio_coefficients, 0, sizeof(audio_coefficients));
  TEST_RESULT(
    MoonlightProtocolV1RsInitialize(
      &audio_context,
      MOONLIGHT_PROTOCOL_V1_AUDIO_DATA_SHARDS,
      MOONLIGHT_PROTOCOL_V1_AUDIO_PARITY_SHARDS,
      audio_coefficients,
      sizeof(audio_coefficients)
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(audio_context.data_shards == MOONLIGHT_PROTOCOL_V1_AUDIO_DATA_SHARDS);
  TEST_CHECK(audio_context.parity_shards == MOONLIGHT_PROTOCOL_V1_AUDIO_PARITY_SHARDS);
  TEST_CHECK(audio_context.coefficients == audio_coefficients);
  TEST_CHECK(memcmp(audio_coefficients, expected_audio_coefficients, sizeof(audio_coefficients)) == 0);
  TEST_RESULT(
    MoonlightProtocolV1RsInitialize(
      &video_context,
      1,
      1,
      video_coefficients,
      0
    ),
    MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL
  );
  TEST_RESULT(
    MoonlightProtocolV1RsInitialize(
      NULL,
      1,
      1,
      video_coefficients,
      sizeof(video_coefficients)
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1RsInitialize(
      &video_context,
      0,
      1,
      video_coefficients,
      sizeof(video_coefficients)
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1RsInitialize(
      &video_context,
      1,
      1,
      NULL,
      sizeof(video_coefficients)
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  memset(&aliased_initialization, 0xa5, sizeof(aliased_initialization));
  memcpy(
    aliased_initialization_before,
    &aliased_initialization,
    sizeof(aliased_initialization)
  );
  TEST_RESULT(
    MoonlightProtocolV1RsInitialize(
      &aliased_initialization.context,
      MOONLIGHT_PROTOCOL_V1_AUDIO_DATA_SHARDS,
      MOONLIGHT_PROTOCOL_V1_AUDIO_PARITY_SHARDS,
      aliased_initialization.bytes,
      sizeof(aliased_initialization.bytes)
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_CHECK(memcmp(&aliased_initialization, aliased_initialization_before, sizeof(aliased_initialization)) == 0);
  TEST_RESULT(
    MoonlightProtocolV1RsInitialize(
      &zero_parity_context,
      4,
      0,
      NULL,
      0
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(zero_parity_context.data_shards == 4);
  TEST_CHECK(zero_parity_context.parity_shards == 0);
  TEST_CHECK(zero_parity_context.coefficients == NULL);

  for (i = 0; i < MOONLIGHT_PROTOCOL_V1_AUDIO_DATA_SHARDS; ++i) {
    audio_data_pointers[i] = TEST_AUDIO_SOURCE_SHARDS[i];
  }
  for (i = 0; i < MOONLIGHT_PROTOCOL_V1_AUDIO_PARITY_SHARDS; ++i) {
    memset(audio_parity[i], 0xa5, sizeof(audio_parity[i]));
    audio_parity_pointers[i] = audio_parity[i];
  }
  memset(&guarded_context, 0, sizeof(guarded_context));
  memset(guarded_coefficients, 0x5a, sizeof(guarded_coefficients));
  TEST_RESULT(
    MoonlightProtocolV1RsInitialize(
      &guarded_context.context,
      MOONLIGHT_PROTOCOL_V1_AUDIO_DATA_SHARDS,
      MOONLIGHT_PROTOCOL_V1_AUDIO_PARITY_SHARDS,
      guarded_coefficients,
      sizeof(guarded_coefficients)
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );

  memcpy(
    guarded_context_before,
    &guarded_context,
    sizeof(guarded_context)
  );
  audio_parity_pointers[0] = guarded_context.bytes;
  TEST_RESULT(
    MoonlightProtocolV1RsEncode(
      &guarded_context.context,
      16,
      audio_data_pointers,
      MOONLIGHT_PROTOCOL_V1_AUDIO_DATA_SHARDS,
      audio_parity_pointers,
      MOONLIGHT_PROTOCOL_V1_AUDIO_PARITY_SHARDS
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_CHECK(memcmp(&guarded_context, guarded_context_before, sizeof(guarded_context)) == 0);

  memcpy(
    guarded_coefficients_before,
    guarded_coefficients,
    sizeof(guarded_coefficients)
  );
  audio_parity_pointers[0] = guarded_coefficients;
  TEST_RESULT(
    MoonlightProtocolV1RsEncode(
      &guarded_context.context,
      16,
      audio_data_pointers,
      MOONLIGHT_PROTOCOL_V1_AUDIO_DATA_SHARDS,
      audio_parity_pointers,
      MOONLIGHT_PROTOCOL_V1_AUDIO_PARITY_SHARDS
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_CHECK(memcmp(guarded_coefficients, guarded_coefficients_before, sizeof(guarded_coefficients)) == 0);

  memcpy(
    audio_data_pointers_before,
    audio_data_pointers,
    sizeof(audio_data_pointers)
  );
  audio_parity_pointers[0] = (uint8_t *) (void *) audio_data_pointers;
  TEST_RESULT(
    MoonlightProtocolV1RsEncode(
      &guarded_context.context,
      16,
      audio_data_pointers,
      MOONLIGHT_PROTOCOL_V1_AUDIO_DATA_SHARDS,
      audio_parity_pointers,
      MOONLIGHT_PROTOCOL_V1_AUDIO_PARITY_SHARDS
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_CHECK(memcmp(audio_data_pointers, audio_data_pointers_before, sizeof(audio_data_pointers)) == 0);

  memset(&aliased_parity_pointers, 0, sizeof(aliased_parity_pointers));
  aliased_parity_pointers.pointers[0] = aliased_parity_pointers.bytes;
  aliased_parity_pointers.pointers[1] = audio_parity[1];
  memcpy(
    aliased_parity_pointers_before,
    &aliased_parity_pointers,
    sizeof(aliased_parity_pointers)
  );
  TEST_RESULT(
    MoonlightProtocolV1RsEncode(
      &guarded_context.context,
      16,
      audio_data_pointers,
      MOONLIGHT_PROTOCOL_V1_AUDIO_DATA_SHARDS,
      aliased_parity_pointers.pointers,
      MOONLIGHT_PROTOCOL_V1_AUDIO_PARITY_SHARDS
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_CHECK(memcmp(&aliased_parity_pointers, aliased_parity_pointers_before, sizeof(aliased_parity_pointers)) == 0);

  for (i = 0; i < MOONLIGHT_PROTOCOL_V1_AUDIO_PARITY_SHARDS; ++i) {
    audio_parity_pointers[i] = audio_parity[i];
  }
  TEST_RESULT(
    MoonlightProtocolV1RsEncode(
      &audio_context,
      16,
      audio_data_pointers,
      MOONLIGHT_PROTOCOL_V1_AUDIO_DATA_SHARDS,
      audio_parity_pointers,
      MOONLIGHT_PROTOCOL_V1_AUDIO_PARITY_SHARDS
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  for (i = 0; i < sizeof(audio_parity[0]); ++i) {
    size_t data_index;
    uint8_t expected_first = 0;
    uint8_t expected_second = 0;

    for (data_index = 0;
         data_index < MOONLIGHT_PROTOCOL_V1_AUDIO_DATA_SHARDS;
         ++data_index) {
      expected_first ^= multiply_test_gf256(
        expected_audio_coefficients[data_index],
        TEST_AUDIO_SOURCE_SHARDS[data_index][i]
      );
      expected_second ^= multiply_test_gf256(
        expected_audio_coefficients[MOONLIGHT_PROTOCOL_V1_AUDIO_DATA_SHARDS + data_index],
        TEST_AUDIO_SOURCE_SHARDS[data_index][i]
      );
    }
    TEST_CHECK(audio_parity[0][i] == expected_first);
    TEST_CHECK(audio_parity[1][i] == expected_second);
  }
  memset(legacy_audio_parity, 0, sizeof(legacy_audio_parity));
  for (i = 0; i < sizeof(legacy_audio_parity[0]); ++i) {
    size_t data_index;

    for (data_index = 0;
         data_index < MOONLIGHT_PROTOCOL_V1_AUDIO_DATA_SHARDS;
         ++data_index) {
      legacy_audio_parity[0][i] ^= multiply_test_gf256(
        legacy_audio_coefficients[data_index],
        TEST_AUDIO_SOURCE_SHARDS[data_index][i]
      );
      legacy_audio_parity[1][i] ^= multiply_test_gf256(
        legacy_audio_coefficients[MOONLIGHT_PROTOCOL_V1_AUDIO_DATA_SHARDS + data_index],
        TEST_AUDIO_SOURCE_SHARDS[data_index][i]
      );
    }
  }
  TEST_CHECK(memcmp(legacy_audio_parity[0], audio_parity[0], sizeof(audio_parity[0])) != 0);
  TEST_CHECK(memcmp(legacy_audio_parity[1], audio_parity[1], sizeof(audio_parity[1])) != 0);
  TEST_RESULT(
    MoonlightProtocolV1RsEncode(
      &zero_parity_context,
      16,
      audio_data_pointers,
      MOONLIGHT_PROTOCOL_V1_AUDIO_DATA_SHARDS,
      NULL,
      0
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  memset(audio_parity, 0xa5, sizeof(audio_parity));
  TEST_RESULT(
    MoonlightProtocolV1RsEncode(
      NULL,
      16,
      audio_data_pointers,
      MOONLIGHT_PROTOCOL_V1_AUDIO_DATA_SHARDS,
      audio_parity_pointers,
      MOONLIGHT_PROTOCOL_V1_AUDIO_PARITY_SHARDS
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  video_context = audio_context;
  video_context.data_shards = 0;
  TEST_RESULT(
    MoonlightProtocolV1RsEncode(
      &video_context,
      16,
      audio_data_pointers,
      MOONLIGHT_PROTOCOL_V1_AUDIO_DATA_SHARDS,
      audio_parity_pointers,
      MOONLIGHT_PROTOCOL_V1_AUDIO_PARITY_SHARDS
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1RsEncode(
      &audio_context,
      15,
      audio_data_pointers,
      MOONLIGHT_PROTOCOL_V1_AUDIO_DATA_SHARDS,
      audio_parity_pointers,
      MOONLIGHT_PROTOCOL_V1_AUDIO_PARITY_SHARDS
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1RsEncode(
      &audio_context,
      16,
      NULL,
      MOONLIGHT_PROTOCOL_V1_AUDIO_DATA_SHARDS,
      audio_parity_pointers,
      MOONLIGHT_PROTOCOL_V1_AUDIO_PARITY_SHARDS
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1RsEncode(
      &audio_context,
      16,
      audio_data_pointers,
      MOONLIGHT_PROTOCOL_V1_AUDIO_DATA_SHARDS - 1,
      audio_parity_pointers,
      MOONLIGHT_PROTOCOL_V1_AUDIO_PARITY_SHARDS
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  audio_data_pointers[0] = NULL;
  TEST_RESULT(
    MoonlightProtocolV1RsEncode(
      &audio_context,
      16,
      audio_data_pointers,
      MOONLIGHT_PROTOCOL_V1_AUDIO_DATA_SHARDS,
      audio_parity_pointers,
      MOONLIGHT_PROTOCOL_V1_AUDIO_PARITY_SHARDS
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  audio_data_pointers[0] = TEST_AUDIO_SOURCE_SHARDS[0];
  audio_data_pointers[1] = audio_data_pointers[0];
  TEST_RESULT(
    MoonlightProtocolV1RsEncode(
      &audio_context,
      16,
      audio_data_pointers,
      MOONLIGHT_PROTOCOL_V1_AUDIO_DATA_SHARDS,
      audio_parity_pointers,
      MOONLIGHT_PROTOCOL_V1_AUDIO_PARITY_SHARDS
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  audio_data_pointers[1] = TEST_AUDIO_SOURCE_SHARDS[1];
  TEST_RESULT(
    MoonlightProtocolV1RsEncode(
      &audio_context,
      16,
      audio_data_pointers,
      MOONLIGHT_PROTOCOL_V1_AUDIO_DATA_SHARDS,
      audio_parity_pointers,
      MOONLIGHT_PROTOCOL_V1_AUDIO_PARITY_SHARDS - 1
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1RsEncode(
      &audio_context,
      16,
      audio_data_pointers,
      MOONLIGHT_PROTOCOL_V1_AUDIO_DATA_SHARDS,
      NULL,
      MOONLIGHT_PROTOCOL_V1_AUDIO_PARITY_SHARDS
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  audio_parity_pointers[0] = NULL;
  TEST_RESULT(
    MoonlightProtocolV1RsEncode(
      &audio_context,
      16,
      audio_data_pointers,
      MOONLIGHT_PROTOCOL_V1_AUDIO_DATA_SHARDS,
      audio_parity_pointers,
      MOONLIGHT_PROTOCOL_V1_AUDIO_PARITY_SHARDS
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  audio_parity_pointers[0] = audio_parity[0];
  audio_parity_pointers[1] = audio_parity_pointers[0];
  TEST_RESULT(
    MoonlightProtocolV1RsEncode(
      &audio_context,
      16,
      audio_data_pointers,
      MOONLIGHT_PROTOCOL_V1_AUDIO_DATA_SHARDS,
      audio_parity_pointers,
      MOONLIGHT_PROTOCOL_V1_AUDIO_PARITY_SHARDS
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  audio_parity_pointers[1] = audio_parity[1];
  audio_data_pointers[0] = audio_parity[0];
  TEST_RESULT(
    MoonlightProtocolV1RsEncode(
      &audio_context,
      16,
      audio_data_pointers,
      MOONLIGHT_PROTOCOL_V1_AUDIO_DATA_SHARDS,
      audio_parity_pointers,
      MOONLIGHT_PROTOCOL_V1_AUDIO_PARITY_SHARDS
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  audio_data_pointers[0] = TEST_AUDIO_SOURCE_SHARDS[0];
  for (i = 0; i < sizeof(audio_parity); ++i) {
    TEST_CHECK(((const uint8_t *) audio_parity)[i] == 0xa5);
  }

  TEST_CHECK(load_golden(
    "video-data.hex",
    video_data,
    sizeof(video_data),
    &video_data_size
  ));
  TEST_CHECK(load_golden(
    "video-parity.hex",
    video_parity,
    sizeof(video_parity),
    &video_parity_size
  ));
  TEST_RESULT(
    MoonlightProtocolV1RsInitialize(
      &video_context,
      1,
      1,
      video_coefficients,
      sizeof(video_coefficients)
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(video_coefficients[0] == 1);
  video_data_pointer = video_data + TEST_VIDEO_CODING_OFFSET;
  memset(video_output, 0xa5, sizeof(video_output));
  TEST_RESULT(
    MoonlightProtocolV1RsEncode(
      &video_context,
      sizeof(video_output),
      &video_data_pointer,
      1,
      &video_parity_pointer,
      1
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(video_data_size == TEST_VIDEO_DATAGRAM_SIZE);
  TEST_CHECK(video_parity_size == TEST_VIDEO_DATAGRAM_SIZE);
  TEST_CHECK(memcmp(video_output, video_parity + TEST_VIDEO_CODING_OFFSET, sizeof(video_output)) == 0);
  return true;
}

/**
 * @brief Exercises deterministic Reed-Solomon reconstruction and workspace rules.
 *
 * @return True when all Audio four-of-six survivor sets recover exact data.
 */
static bool test_reed_solomon_reconstruction(void) {
  uint8_t coefficients[8];
  uint8_t maximum_coefficients[TEST_RS_MAX_COEFFICIENTS];
  uint8_t maximum_marks[MOONLIGHT_PROTOCOL_V1_FEC_SHARD_MAX];
  uint8_t original[6][16];
  uint8_t parity[2][16];
  uint8_t working[6][16];
  uint8_t workspace[32];
  const uint8_t *data_pointers[4];
  uint8_t *parity_pointers[2] = {parity[0], parity[1]};
  uint8_t *row_pointers[6];
  uint8_t marks[6];
  uint8_t original_marks[6];
  MoonlightProtocolV1RsContext context;
  MoonlightProtocolV1RsContext maximum_context;
  MoonlightProtocolV1RsContext singular_context;
  MoonlightProtocolV1RsContext zero_parity_context;
  size_t first_missing;
  size_t second_missing;
  size_t workspace_size = 0;

  TEST_RESULT(
    MoonlightProtocolV1RsInitialize(
      &context,
      4,
      2,
      coefficients,
      sizeof(coefficients)
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  for (first_missing = 0; first_missing < 4; ++first_missing) {
    memcpy(original[first_missing], TEST_AUDIO_SOURCE_SHARDS[first_missing], 16);
    data_pointers[first_missing] = original[first_missing];
  }
  TEST_RESULT(
    MoonlightProtocolV1RsEncode(
      &context,
      16,
      data_pointers,
      4,
      parity_pointers,
      2
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  memcpy(original[4], parity[0], 16);
  memcpy(original[5], parity[1], 16);

  for (first_missing = 0; first_missing < 6; ++first_missing) {
    for (second_missing = first_missing + 1;
         second_missing < 6;
         ++second_missing) {
      size_t data_missing_count = 0;
      size_t row;

      memcpy(working, original, sizeof(working));
      memset(marks, 0, sizeof(marks));
      marks[first_missing] = 1;
      marks[second_missing] = 1;
      memcpy(original_marks, marks, sizeof(marks));
      for (row = 0; row < 6; ++row) {
        if (marks[row] != 0 && row < 4) {
          memset(working[row], 0xa5, sizeof(working[row]));
          ++data_missing_count;
        }
        row_pointers[row] = marks[row] != 0 && row >= 4 ?
                              NULL :
                              working[row];
      }

      workspace_size = SIZE_MAX;
      TEST_RESULT(
        MoonlightProtocolV1RsReconstructWorkspaceSize(
          &context,
          marks,
          sizeof(marks),
          &workspace_size
        ),
        MOONLIGHT_PROTOCOL_RESULT_OK
      );
      TEST_CHECK(
        workspace_size ==
        2u * data_missing_count * data_missing_count +
          2u * data_missing_count
      );
      memset(workspace, 0xa5, sizeof(workspace));
      TEST_RESULT(
        MoonlightProtocolV1RsReconstructData(
          &context,
          16,
          row_pointers,
          6,
          marks,
          sizeof(marks),
          workspace_size == 0 ? NULL : workspace,
          workspace_size
        ),
        MOONLIGHT_PROTOCOL_RESULT_OK
      );
      TEST_CHECK(memcmp(marks, original_marks, sizeof(marks)) == 0);
      for (row = 0; row < 4; ++row) {
        TEST_CHECK(memcmp(working[row], original[row], 16) == 0);
      }
      for (row = 4; row < 6; ++row) {
        if (marks[row] == 0) {
          TEST_CHECK(memcmp(working[row], original[row], 16) == 0);
        }
      }
    }
  }

  memset(marks, 0, sizeof(marks));
  marks[0] = 1;
  marks[1] = 1;
  marks[2] = 1;
  workspace_size = SIZE_MAX;
  TEST_RESULT(
    MoonlightProtocolV1RsReconstructWorkspaceSize(
      &context,
      marks,
      sizeof(marks),
      &workspace_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INSUFFICIENT_SHARDS
  );
  marks[2] = 2;
  TEST_RESULT(
    MoonlightProtocolV1RsReconstructWorkspaceSize(
      &context,
      marks,
      sizeof(marks),
      &workspace_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1RsReconstructWorkspaceSize(
      &context,
      marks,
      sizeof(marks) - 1,
      &workspace_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1RsReconstructWorkspaceSize(
      &context,
      marks,
      sizeof(marks),
      NULL
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1RsReconstructWorkspaceSize(
      NULL,
      marks,
      sizeof(marks),
      &workspace_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  singular_context = context;
  singular_context.coefficients = NULL;
  TEST_RESULT(
    MoonlightProtocolV1RsReconstructWorkspaceSize(
      &singular_context,
      marks,
      sizeof(marks),
      &workspace_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  singular_context = context;
  singular_context.parity_shards = 0;
  TEST_RESULT(
    MoonlightProtocolV1RsReconstructWorkspaceSize(
      &singular_context,
      marks,
      sizeof(marks),
      &workspace_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );

  memcpy(working, original, sizeof(working));
  memset(marks, 0, sizeof(marks));
  marks[0] = 1;
  marks[1] = 1;
  memset(working[0], 0xa5, 16);
  memset(working[1], 0xa5, 16);
  for (first_missing = 0; first_missing < 6; ++first_missing) {
    row_pointers[first_missing] = working[first_missing];
  }
  TEST_RESULT(
    MoonlightProtocolV1RsReconstructWorkspaceSize(
      &context,
      marks,
      sizeof(marks),
      &workspace_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(workspace_size == 12);
  TEST_RESULT(
    MoonlightProtocolV1RsReconstructData(
      NULL,
      16,
      row_pointers,
      6,
      marks,
      sizeof(marks),
      workspace,
      workspace_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1RsReconstructData(
      &context,
      15,
      row_pointers,
      6,
      marks,
      sizeof(marks),
      workspace,
      workspace_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1RsReconstructData(
      &context,
      16,
      NULL,
      6,
      marks,
      sizeof(marks),
      workspace,
      workspace_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1RsReconstructData(
      &context,
      16,
      row_pointers,
      5,
      marks,
      sizeof(marks),
      workspace,
      workspace_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1RsReconstructData(
      &context,
      16,
      row_pointers,
      6,
      marks,
      sizeof(marks) - 1,
      workspace,
      workspace_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  marks[2] = 2;
  TEST_RESULT(
    MoonlightProtocolV1RsReconstructData(
      &context,
      16,
      row_pointers,
      6,
      marks,
      sizeof(marks),
      workspace,
      workspace_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  marks[2] = 1;
  TEST_RESULT(
    MoonlightProtocolV1RsReconstructData(
      &context,
      16,
      row_pointers,
      6,
      marks,
      sizeof(marks),
      workspace,
      workspace_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INSUFFICIENT_SHARDS
  );
  marks[2] = 0;
  row_pointers[0] = NULL;
  TEST_RESULT(
    MoonlightProtocolV1RsReconstructData(
      &context,
      16,
      row_pointers,
      6,
      marks,
      sizeof(marks),
      workspace,
      workspace_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  row_pointers[0] = working[0];
  row_pointers[1] = row_pointers[0];
  TEST_RESULT(
    MoonlightProtocolV1RsReconstructData(
      &context,
      16,
      row_pointers,
      6,
      marks,
      sizeof(marks),
      workspace,
      workspace_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  row_pointers[1] = working[1];
  TEST_RESULT(
    MoonlightProtocolV1RsReconstructData(
      &context,
      16,
      row_pointers,
      6,
      marks,
      sizeof(marks),
      NULL,
      workspace_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1RsReconstructData(
      &context,
      16,
      row_pointers,
      6,
      marks,
      sizeof(marks),
      marks,
      workspace_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  row_pointers[0] = workspace;
  TEST_RESULT(
    MoonlightProtocolV1RsReconstructData(
      &context,
      16,
      row_pointers,
      6,
      marks,
      sizeof(marks),
      workspace,
      workspace_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  row_pointers[0] = working[0];
  TEST_RESULT(
    MoonlightProtocolV1RsReconstructData(
      &context,
      16,
      row_pointers,
      6,
      marks,
      sizeof(marks),
      workspace,
      workspace_size - 1
    ),
    MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL
  );
  for (first_missing = 0; first_missing < 16; ++first_missing) {
    TEST_CHECK(working[0][first_missing] == 0xa5);
    TEST_CHECK(working[1][first_missing] == 0xa5);
  }

  memset(coefficients, 0, sizeof(coefficients));
  singular_context = context;
  singular_context.coefficients = coefficients;
  memset(marks, 0, sizeof(marks));
  marks[0] = 1;
  memset(working[0], 0xa5, 16);
  TEST_RESULT(
    MoonlightProtocolV1RsReconstructWorkspaceSize(
      &singular_context,
      marks,
      sizeof(marks),
      &workspace_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_RESULT(
    MoonlightProtocolV1RsReconstructData(
      &singular_context,
      16,
      row_pointers,
      6,
      marks,
      sizeof(marks),
      workspace,
      workspace_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INTERNAL_ERROR
  );
  for (first_missing = 0; first_missing < 16; ++first_missing) {
    TEST_CHECK(working[0][first_missing] == 0xa5);
  }

  TEST_RESULT(
    MoonlightProtocolV1RsInitialize(
      &zero_parity_context,
      4,
      0,
      NULL,
      0
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  memset(marks, 0, 4);
  TEST_RESULT(
    MoonlightProtocolV1RsReconstructWorkspaceSize(
      &zero_parity_context,
      marks,
      4,
      &workspace_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(workspace_size == 0);
  TEST_RESULT(
    MoonlightProtocolV1RsReconstructData(
      &zero_parity_context,
      16,
      row_pointers,
      4,
      marks,
      4,
      NULL,
      0
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  marks[0] = 1;
  TEST_RESULT(
    MoonlightProtocolV1RsReconstructWorkspaceSize(
      &zero_parity_context,
      marks,
      4,
      &workspace_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INSUFFICIENT_SHARDS
  );

  TEST_RESULT(
    MoonlightProtocolV1RsInitialize(
      &maximum_context,
      127,
      128,
      maximum_coefficients,
      sizeof(maximum_coefficients)
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  memset(maximum_marks, 0, sizeof(maximum_marks));
  memset(maximum_marks, 1, 127);
  TEST_RESULT(
    MoonlightProtocolV1RsReconstructWorkspaceSize(
      &maximum_context,
      maximum_marks,
      sizeof(maximum_marks),
      &workspace_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(workspace_size == 32512);
  maximum_marks[127] = 1;
  TEST_RESULT(
    MoonlightProtocolV1RsReconstructWorkspaceSize(
      &maximum_context,
      maximum_marks,
      sizeof(maximum_marks),
      &workspace_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(workspace_size == 32512);
  maximum_marks[128] = 1;
  TEST_RESULT(
    MoonlightProtocolV1RsReconstructWorkspaceSize(
      &maximum_context,
      maximum_marks,
      sizeof(maximum_marks),
      &workspace_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INSUFFICIENT_SHARDS
  );
  TEST_RESULT(
    MoonlightProtocolV1RsInitialize(
      &zero_parity_context,
      255,
      0,
      NULL,
      0
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  memset(maximum_marks, 0, sizeof(maximum_marks));
  TEST_RESULT(
    MoonlightProtocolV1RsReconstructWorkspaceSize(
      &zero_parity_context,
      maximum_marks,
      sizeof(maximum_marks),
      &workspace_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(workspace_size == 0);
  maximum_marks[254] = 1;
  TEST_RESULT(
    MoonlightProtocolV1RsReconstructWorkspaceSize(
      &zero_parity_context,
      maximum_marks,
      sizeof(maximum_marks),
      &workspace_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INSUFFICIENT_SHARDS
  );
  return true;
}

/**
 * @brief Exercises canonical data-shard construction and padding validation.
 *
 * @return True when valid and malformed coding shards are classified exactly.
 */
static bool test_data_coding_shard_validation(void) {
  static const uint8_t codec_bytes[] = {0x21, 0x43, 0x65};
  uint8_t coding_shard[32];
  MoonlightProtocolV1DataShardView view;
  size_t i;

  memset(coding_shard, 0xa5, sizeof(coding_shard));
  TEST_RESULT(
    MoonlightProtocolV1BuildDataCodingShard(
      codec_bytes,
      sizeof(codec_bytes),
      coding_shard,
      16
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(coding_shard[0] == 0);
  TEST_CHECK(coding_shard[1] == sizeof(codec_bytes));
  TEST_CHECK(memcmp(coding_shard + 2, codec_bytes, sizeof(codec_bytes)) == 0);
  for (i = 2 + sizeof(codec_bytes); i < 16; ++i) {
    TEST_CHECK(coding_shard[i] == 0);
  }
  TEST_RESULT(
    MoonlightProtocolV1ParseDataCodingShard(coding_shard, 16, 14, &view),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(view.meaningful_bytes == sizeof(codec_bytes));
  TEST_CHECK(view.bytes == coding_shard + 2);

  TEST_RESULT(
    MoonlightProtocolV1BuildDataCodingShard(NULL, 1, coding_shard, 16),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1BuildDataCodingShard(codec_bytes, 1, NULL, 16),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1BuildDataCodingShard(codec_bytes, 1, coding_shard, 15),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1BuildDataCodingShard(codec_bytes, 1, coding_shard, 17),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1BuildDataCodingShard(
      codec_bytes,
      1,
      coding_shard,
      (size_t) UINT16_MAX + 1u
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1BuildDataCodingShard(codec_bytes, 0, coding_shard, 16),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1BuildDataCodingShard(codec_bytes, 15, coding_shard, 16),
    MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL
  );
  TEST_RESULT(
    MoonlightProtocolV1BuildDataCodingShard(codec_bytes, UINT32_MAX, coding_shard, 16),
    MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL
  );

  TEST_RESULT(
    MoonlightProtocolV1ParseDataCodingShard(NULL, 16, 14, &view),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1ParseDataCodingShard(coding_shard, 16, 14, NULL),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1ParseDataCodingShard(coding_shard, 15, 13, &view),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1ParseDataCodingShard(coding_shard, 17, 15, &view),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1ParseDataCodingShard(
      coding_shard,
      (size_t) UINT16_MAX + 1u,
      1,
      &view
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1ParseDataCodingShard(coding_shard, 16, 0, &view),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1ParseDataCodingShard(coding_shard, 16, 15, &view),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );

  TEST_RESULT(
    MoonlightProtocolV1BuildDataCodingShard(
      codec_bytes,
      sizeof(codec_bytes),
      coding_shard,
      16
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  coding_shard[0] = 0;
  coding_shard[1] = 0;
  TEST_RESULT(
    MoonlightProtocolV1ParseDataCodingShard(coding_shard, 16, 14, &view),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  coding_shard[1] = 3;
  TEST_RESULT(
    MoonlightProtocolV1ParseDataCodingShard(coding_shard, 16, 2, &view),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  coding_shard[15] = 1;
  TEST_RESULT(
    MoonlightProtocolV1ParseDataCodingShard(coding_shard, 16, 14, &view),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );

  coding_shard[0] = 0x10;
  coding_shard[1] = 0x20;
  coding_shard[2] = 0x30;
  TEST_RESULT(
    MoonlightProtocolV1BuildDataCodingShard(
      coding_shard,
      3,
      coding_shard,
      16
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_RESULT(
    MoonlightProtocolV1ParseDataCodingShard(coding_shard, 16, 14, &view),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(view.meaningful_bytes == 3);
  TEST_CHECK(view.bytes[0] == 0x10);
  TEST_CHECK(view.bytes[1] == 0x20);
  TEST_CHECK(view.bytes[2] == 0x30);
  return true;
}

/**
 * @brief Exercises common-header argument, direction, flag, and reserved-field errors.
 *
 * @return True when every common-header error has the required result.
 */
static bool test_datagram_header_validation(void) {
  uint8_t encoded[MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE];
  uint8_t mutated[MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE];
  MoonlightProtocolV1DatagramHeader decoded;
  MoonlightProtocolV1DatagramHeader header = {
    .channel = MOONLIGHT_PROTOCOL_V1_CHANNEL_VIDEO_DATA,
    .flags = MOONLIGHT_PROTOCOL_V1_VIDEO_FLAG_START_OF_FRAME |
             MOONLIGHT_PROTOCOL_V1_VIDEO_FLAG_END_OF_FRAME,
    .session_wire_id = 1,
    .media_epoch = 1,
    .sequence = 0,
  };

  TEST_RESULT(
    MoonlightProtocolV1EncodeDatagramHeader(
      &header,
      MOONLIGHT_PROTOCOL_DIRECTION_HOST_TO_CLIENT,
      encoded,
      sizeof(encoded)
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeDatagramHeader(
      encoded,
      sizeof(encoded),
      MOONLIGHT_PROTOCOL_DIRECTION_HOST_TO_CLIENT,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(decoded.channel == header.channel);
  TEST_CHECK(decoded.flags == header.flags);
  TEST_CHECK(decoded.session_wire_id == header.session_wire_id);
  TEST_CHECK(decoded.media_epoch == header.media_epoch);

  TEST_RESULT(
    MoonlightProtocolV1EncodeDatagramHeader(
      NULL,
      MOONLIGHT_PROTOCOL_DIRECTION_HOST_TO_CLIENT,
      encoded,
      sizeof(encoded)
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeDatagramHeader(
      &header,
      (MoonlightProtocolDirection) 0,
      encoded,
      sizeof(encoded)
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeDatagramHeader(
      &header,
      MOONLIGHT_PROTOCOL_DIRECTION_HOST_TO_CLIENT,
      NULL,
      sizeof(encoded)
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeDatagramHeader(
      &header,
      MOONLIGHT_PROTOCOL_DIRECTION_HOST_TO_CLIENT,
      encoded,
      sizeof(encoded) - 1
    ),
    MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeDatagramHeader(
      NULL,
      sizeof(encoded),
      MOONLIGHT_PROTOCOL_DIRECTION_HOST_TO_CLIENT,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeDatagramHeader(
      encoded,
      sizeof(encoded),
      MOONLIGHT_PROTOCOL_DIRECTION_HOST_TO_CLIENT,
      NULL
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeDatagramHeader(
      encoded,
      sizeof(encoded),
      (MoonlightProtocolDirection) 0,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeDatagramHeader(
      encoded,
      sizeof(encoded) - 1,
      MOONLIGHT_PROTOCOL_DIRECTION_HOST_TO_CLIENT,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_TRUNCATED
  );

  memcpy(mutated, encoded, sizeof(mutated));
  mutated[2] = 1;
  TEST_RESULT(
    MoonlightProtocolV1DecodeDatagramHeader(
      mutated,
      sizeof(mutated),
      MOONLIGHT_PROTOCOL_DIRECTION_HOST_TO_CLIENT,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, encoded, sizeof(mutated));
  mutated[0] = 0xff;
  TEST_RESULT(
    MoonlightProtocolV1DecodeDatagramHeader(
      mutated,
      sizeof(mutated),
      MOONLIGHT_PROTOCOL_DIRECTION_HOST_TO_CLIENT,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  memcpy(mutated, encoded, sizeof(mutated));
  mutated[0] = 0;
  TEST_RESULT(
    MoonlightProtocolV1DecodeDatagramHeader(
      mutated,
      sizeof(mutated),
      MOONLIGHT_PROTOCOL_DIRECTION_HOST_TO_CLIENT,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeDatagramHeader(
      encoded,
      sizeof(encoded),
      MOONLIGHT_PROTOCOL_DIRECTION_CLIENT_TO_HOST,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
  );

  header.session_wire_id = 0;
  TEST_RESULT(
    MoonlightProtocolV1EncodeDatagramHeader(
      &header,
      MOONLIGHT_PROTOCOL_DIRECTION_HOST_TO_CLIENT,
      encoded,
      sizeof(encoded)
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  header.session_wire_id = 1;
  header.flags = 0x80;
  TEST_RESULT(
    MoonlightProtocolV1EncodeDatagramHeader(
      &header,
      MOONLIGHT_PROTOCOL_DIRECTION_HOST_TO_CLIENT,
      encoded,
      sizeof(encoded)
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  header.flags = MOONLIGHT_PROTOCOL_V1_VIDEO_FLAG_KEY_FRAME |
                 MOONLIGHT_PROTOCOL_V1_VIDEO_FLAG_REFERENCE_RECOVERY;
  TEST_RESULT(
    MoonlightProtocolV1EncodeDatagramHeader(
      &header,
      MOONLIGHT_PROTOCOL_DIRECTION_HOST_TO_CLIENT,
      encoded,
      sizeof(encoded)
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  header.flags = 0;
  header.media_epoch = 0;
  TEST_RESULT(
    MoonlightProtocolV1EncodeDatagramHeader(
      &header,
      MOONLIGHT_PROTOCOL_DIRECTION_HOST_TO_CLIENT,
      encoded,
      sizeof(encoded)
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );

  header.channel = MOONLIGHT_PROTOCOL_V1_CHANNEL_AUDIO_DATA;
  header.flags = 1;
  TEST_RESULT(
    MoonlightProtocolV1EncodeDatagramHeader(
      &header,
      MOONLIGHT_PROTOCOL_DIRECTION_HOST_TO_CLIENT,
      encoded,
      sizeof(encoded)
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  header.flags = 0;
  header.media_epoch = 1;
  TEST_RESULT(
    MoonlightProtocolV1EncodeDatagramHeader(
      &header,
      MOONLIGHT_PROTOCOL_DIRECTION_HOST_TO_CLIENT,
      encoded,
      sizeof(encoded)
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );

  header.channel = MOONLIGHT_PROTOCOL_V1_CHANNEL_MEDIA_FEEDBACK;
  header.media_epoch = 0;
  TEST_RESULT(
    MoonlightProtocolV1EncodeDatagramHeader(
      &header,
      MOONLIGHT_PROTOCOL_DIRECTION_CLIENT_TO_HOST,
      encoded,
      sizeof(encoded)
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  header.media_epoch = 1;
  TEST_RESULT(
    MoonlightProtocolV1EncodeDatagramHeader(
      &header,
      MOONLIGHT_PROTOCOL_DIRECTION_CLIENT_TO_HOST,
      encoded,
      sizeof(encoded)
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );

  header.channel = MOONLIGHT_PROTOCOL_V1_CHANNEL_REALTIME_INPUT;
  header.media_epoch = 0;
  TEST_RESULT(
    MoonlightProtocolV1EncodeDatagramHeader(
      &header,
      MOONLIGHT_PROTOCOL_DIRECTION_CLIENT_TO_HOST,
      encoded,
      sizeof(encoded)
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  header.channel = MOONLIGHT_PROTOCOL_V1_CHANNEL_HAPTICS_STATE;
  TEST_RESULT(
    MoonlightProtocolV1EncodeDatagramHeader(
      &header,
      MOONLIGHT_PROTOCOL_DIRECTION_HOST_TO_CLIENT,
      encoded,
      sizeof(encoded)
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  return true;
}

/**
 * @brief Exercises exact negotiated context binding for complete media DATAGRAMs.
 *
 * @return True when stale, disabled, and out-of-snapshot inputs are distinguished.
 */
static bool test_explicit_context_binding(void) {
  uint8_t audio_data[TEST_VECTOR_CAPACITY];
  uint8_t audio_parity[TEST_VECTOR_CAPACITY];
  uint8_t encoded[TEST_VECTOR_CAPACITY];
  uint8_t mutated[TEST_VECTOR_CAPACITY];
  uint8_t video_data[TEST_VECTOR_CAPACITY];
  uint8_t video_parity[TEST_VECTOR_CAPACITY];
  MoonlightProtocolV1AudioContext audio_context;
  MoonlightProtocolV1AudioDatagramView audio_view;
  MoonlightProtocolV1VideoContext video_context;
  MoonlightProtocolV1VideoDatagramView video_view;
  size_t audio_data_size = 0;
  size_t audio_parity_size = 0;
  size_t encoded_size = 0;
  size_t video_data_size = 0;
  size_t video_parity_size = 0;

  TEST_CHECK(load_golden(
    "video-data.hex",
    video_data,
    sizeof(video_data),
    &video_data_size
  ));
  TEST_CHECK(load_golden(
    "video-parity.hex",
    video_parity,
    sizeof(video_parity),
    &video_parity_size
  ));
  TEST_CHECK(load_golden(
    "audio-data.hex",
    audio_data,
    sizeof(audio_data),
    &audio_data_size
  ));
  TEST_CHECK(load_golden(
    "audio-parity.hex",
    audio_parity,
    sizeof(audio_parity),
    &audio_parity_size
  ));

  TEST_RESULT(
    MoonlightProtocolV1ParseVideoDatagram(
      NULL,
      video_data,
      video_data_size,
      &video_view
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  video_context = TEST_VIDEO_CONTEXT;
  video_context.session_wire_id = 0;
  TEST_RESULT(
    MoonlightProtocolV1ParseVideoDatagram(
      &video_context,
      video_data,
      video_data_size,
      &video_view
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  video_context = TEST_VIDEO_CONTEXT;
  video_context.media_epoch = 0;
  TEST_RESULT(
    MoonlightProtocolV1ParseVideoDatagram(
      &video_context,
      video_data,
      video_data_size,
      &video_view
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  video_context = TEST_VIDEO_CONTEXT;
  video_context.coding_shard_bytes = 496;
  TEST_RESULT(
    MoonlightProtocolV1ParseVideoDatagram(
      &video_context,
      video_data,
      video_data_size,
      &video_view
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  video_context = TEST_VIDEO_CONTEXT;
  video_context.coding_shard_bytes =
    MOONLIGHT_PROTOCOL_V1_VIDEO_CODING_SHARD_MAX + 16;
  TEST_RESULT(
    MoonlightProtocolV1ParseVideoDatagram(
      &video_context,
      video_data,
      video_data_size,
      &video_view
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  video_context = TEST_VIDEO_CONTEXT;
  video_context.coding_shard_bytes = 513;
  TEST_RESULT(
    MoonlightProtocolV1ParseVideoDatagram(
      &video_context,
      video_data,
      video_data_size,
      &video_view
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  video_context = TEST_VIDEO_CONTEXT;
  video_context.enabled_channels |= MOONLIGHT_PROTOCOL_V1_MEDIA_CHANNEL_AUDIO_DATA;
  TEST_RESULT(
    MoonlightProtocolV1ParseVideoDatagram(
      &video_context,
      video_data,
      video_data_size,
      &video_view
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  video_context = TEST_VIDEO_CONTEXT;
  video_context.session_wire_id += 1;
  TEST_RESULT(
    MoonlightProtocolV1ParseVideoDatagram(
      &video_context,
      video_data,
      video_data_size,
      &video_view
    ),
    MOONLIGHT_PROTOCOL_RESULT_STALE_CONTEXT
  );
  video_context = TEST_VIDEO_CONTEXT;
  video_context.media_epoch += 1;
  TEST_RESULT(
    MoonlightProtocolV1ParseVideoDatagram(
      &video_context,
      video_data,
      video_data_size,
      &video_view
    ),
    MOONLIGHT_PROTOCOL_RESULT_STALE_CONTEXT
  );
  video_context = TEST_VIDEO_CONTEXT;
  video_context.coding_shard_bytes = 528;
  video_context.complete_datagram_limit = 568;
  TEST_RESULT(
    MoonlightProtocolV1ParseVideoDatagram(
      &video_context,
      video_data,
      video_data_size,
      &video_view
    ),
    MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
  );
  video_context = TEST_VIDEO_CONTEXT;
  video_context.complete_datagram_limit -= 1;
  TEST_RESULT(
    MoonlightProtocolV1ParseVideoDatagram(
      &video_context,
      video_data,
      video_data_size,
      &video_view
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  video_context = TEST_VIDEO_CONTEXT;
  video_context.enabled_channels = MOONLIGHT_PROTOCOL_V1_MEDIA_CHANNEL_VIDEO_PARITY;
  TEST_RESULT(
    MoonlightProtocolV1ParseVideoDatagram(
      &video_context,
      video_data,
      video_data_size,
      &video_view
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  video_context = TEST_VIDEO_CONTEXT;
  video_context.enabled_channels = MOONLIGHT_PROTOCOL_V1_MEDIA_CHANNEL_VIDEO_DATA;
  TEST_RESULT(
    MoonlightProtocolV1ParseVideoDatagram(
      &video_context,
      video_parity,
      video_parity_size,
      &video_view
    ),
    MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
  );

  memcpy(mutated, video_data, video_data_size);
  mutated[1] = MOONLIGHT_PROTOCOL_V1_VIDEO_FLAG_REFERENCE_RECOVERY |
               MOONLIGHT_PROTOCOL_V1_VIDEO_FLAG_START_OF_FRAME |
               MOONLIGHT_PROTOCOL_V1_VIDEO_FLAG_END_OF_FRAME;
  video_context = TEST_VIDEO_CONTEXT;
  video_context.reference_recovery_enabled = false;
  TEST_RESULT(
    MoonlightProtocolV1ParseVideoDatagram(
      &video_context,
      mutated,
      video_data_size,
      &video_view
    ),
    MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
  );
  TEST_RESULT(
    MoonlightProtocolV1ParseVideoDatagram(
      &TEST_VIDEO_CONTEXT,
      mutated,
      video_data_size,
      &video_view
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  encoded_size = 99;
  TEST_RESULT(
    MoonlightProtocolV1EncodeVideoDatagram(
      NULL,
      &video_view.datagram,
      &video_view.shard,
      video_view.coding_shard,
      video_view.coding_shard_size,
      encoded,
      sizeof(encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_CHECK(encoded_size == 0);

  TEST_RESULT(
    MoonlightProtocolV1ParseAudioDatagram(
      NULL,
      audio_data,
      audio_data_size,
      &audio_view
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  audio_context = TEST_AUDIO_CONTEXT;
  audio_context.session_wire_id = 0;
  TEST_RESULT(
    MoonlightProtocolV1ParseAudioDatagram(
      &audio_context,
      audio_data,
      audio_data_size,
      &audio_view
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  audio_context = TEST_AUDIO_CONTEXT;
  audio_context.maximum_packet_bytes = 0;
  TEST_RESULT(
    MoonlightProtocolV1ParseAudioDatagram(
      &audio_context,
      audio_data,
      audio_data_size,
      &audio_view
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  audio_context = TEST_AUDIO_CONTEXT;
  audio_context.maximum_packet_bytes =
    MOONLIGHT_PROTOCOL_V1_AUDIO_PACKET_MAX + 1;
  TEST_RESULT(
    MoonlightProtocolV1ParseAudioDatagram(
      &audio_context,
      audio_data,
      audio_data_size,
      &audio_view
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  audio_context = TEST_AUDIO_CONTEXT;
  audio_context.enabled_channels = MOONLIGHT_PROTOCOL_V1_MEDIA_CHANNEL_AUDIO_PARITY;
  TEST_RESULT(
    MoonlightProtocolV1ParseAudioDatagram(
      &audio_context,
      audio_data,
      audio_data_size,
      &audio_view
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  audio_context = TEST_AUDIO_CONTEXT;
  audio_context.enabled_channels |= MOONLIGHT_PROTOCOL_V1_MEDIA_CHANNEL_VIDEO_DATA;
  TEST_RESULT(
    MoonlightProtocolV1ParseAudioDatagram(
      &audio_context,
      audio_data,
      audio_data_size,
      &audio_view
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  audio_context = TEST_AUDIO_CONTEXT;
  audio_context.session_wire_id += 1;
  TEST_RESULT(
    MoonlightProtocolV1ParseAudioDatagram(
      &audio_context,
      audio_data,
      audio_data_size,
      &audio_view
    ),
    MOONLIGHT_PROTOCOL_RESULT_STALE_CONTEXT
  );
  audio_context = TEST_AUDIO_CONTEXT;
  audio_context.maximum_packet_bytes = 3;
  TEST_RESULT(
    MoonlightProtocolV1ParseAudioDatagram(
      &audio_context,
      audio_data,
      audio_data_size,
      &audio_view
    ),
    MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
  );
  audio_context = TEST_AUDIO_CONTEXT;
  audio_context.complete_datagram_limit -= 1;
  TEST_RESULT(
    MoonlightProtocolV1ParseAudioDatagram(
      &audio_context,
      audio_data,
      audio_data_size,
      &audio_view
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  audio_context = TEST_AUDIO_CONTEXT;
  audio_context.maximum_packet_bytes = 15;
  TEST_RESULT(
    MoonlightProtocolV1ParseAudioDatagram(
      &audio_context,
      audio_data,
      audio_data_size,
      &audio_view
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  audio_context = TEST_AUDIO_CONTEXT;
  audio_context.enabled_channels = MOONLIGHT_PROTOCOL_V1_MEDIA_CHANNEL_AUDIO_DATA;
  TEST_RESULT(
    MoonlightProtocolV1ParseAudioDatagram(
      &audio_context,
      audio_parity,
      audio_parity_size,
      &audio_view
    ),
    MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
  );

  memcpy(mutated, audio_parity, audio_parity_size);
  store_test_u16(
    mutated + MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE + 10,
    32
  );
  memset(mutated + audio_parity_size, 0, 16);
  TEST_RESULT(
    MoonlightProtocolV1ParseAudioDatagram(
      &TEST_AUDIO_CONTEXT,
      mutated,
      audio_parity_size + 16,
      &audio_view
    ),
    MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
  );
  audio_context = TEST_AUDIO_CONTEXT;
  audio_context.complete_datagram_limit = TEST_AUDIO_DATAGRAM_SIZE + 16;
  TEST_RESULT(
    MoonlightProtocolV1ParseAudioDatagram(
      &audio_context,
      mutated,
      audio_parity_size + 16,
      &audio_view
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  encoded_size = 99;
  TEST_RESULT(
    MoonlightProtocolV1EncodeAudioDatagram(
      NULL,
      &audio_view.datagram,
      &audio_view.shard,
      audio_view.coding_shard,
      audio_view.coding_shard_size,
      encoded,
      sizeof(encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_CHECK(encoded_size == 0);
  return true;
}

/**
 * @brief Exercises receiver policy classification for every codec result.
 *
 * @return True when stale DATAGRAMs are dropped and violations remain fatal.
 */
static bool test_received_datagram_disposition(void) {
  TEST_CHECK(
    MoonlightProtocolV1ClassifyReceivedDatagramResult(
      MOONLIGHT_PROTOCOL_RESULT_OK
    ) == MOONLIGHT_PROTOCOL_V1_DATAGRAM_DISPOSITION_ACCEPT
  );
  TEST_CHECK(
    MoonlightProtocolV1ClassifyReceivedDatagramResult(
      MOONLIGHT_PROTOCOL_RESULT_STALE_CONTEXT
    ) == MOONLIGHT_PROTOCOL_V1_DATAGRAM_DISPOSITION_DROP_STALE
  );
  TEST_CHECK(
    MoonlightProtocolV1ClassifyReceivedDatagramResult(
      MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
    ) == MOONLIGHT_PROTOCOL_V1_DATAGRAM_DISPOSITION_LOCAL_ERROR
  );
  TEST_CHECK(
    MoonlightProtocolV1ClassifyReceivedDatagramResult(
      MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL
    ) == MOONLIGHT_PROTOCOL_V1_DATAGRAM_DISPOSITION_LOCAL_ERROR
  );
  TEST_CHECK(
    MoonlightProtocolV1ClassifyReceivedDatagramResult(
      MOONLIGHT_PROTOCOL_RESULT_INSUFFICIENT_SHARDS
    ) == MOONLIGHT_PROTOCOL_V1_DATAGRAM_DISPOSITION_LOCAL_ERROR
  );
  TEST_CHECK(
    MoonlightProtocolV1ClassifyReceivedDatagramResult(
      MOONLIGHT_PROTOCOL_RESULT_INTERNAL_ERROR
    ) == MOONLIGHT_PROTOCOL_V1_DATAGRAM_DISPOSITION_LOCAL_ERROR
  );
  TEST_CHECK(
    MoonlightProtocolV1ClassifyReceivedDatagramResult(
      MOONLIGHT_PROTOCOL_RESULT_TRUNCATED
    ) == MOONLIGHT_PROTOCOL_V1_DATAGRAM_DISPOSITION_PROTOCOL_VIOLATION
  );
  TEST_CHECK(
    MoonlightProtocolV1ClassifyReceivedDatagramResult(
      MOONLIGHT_PROTOCOL_RESULT_TRAILING_DATA
    ) == MOONLIGHT_PROTOCOL_V1_DATAGRAM_DISPOSITION_PROTOCOL_VIOLATION
  );
  TEST_CHECK(
    MoonlightProtocolV1ClassifyReceivedDatagramResult(
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    ) == MOONLIGHT_PROTOCOL_V1_DATAGRAM_DISPOSITION_PROTOCOL_VIOLATION
  );
  TEST_CHECK(
    MoonlightProtocolV1ClassifyReceivedDatagramResult(
      MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
    ) == MOONLIGHT_PROTOCOL_V1_DATAGRAM_DISPOSITION_PROTOCOL_VIOLATION
  );
  TEST_CHECK(
    MoonlightProtocolV1ClassifyReceivedDatagramResult(
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    ) == MOONLIGHT_PROTOCOL_V1_DATAGRAM_DISPOSITION_PROTOCOL_VIOLATION
  );
  TEST_CHECK(
    MoonlightProtocolV1ClassifyReceivedDatagramResult(
      MOONLIGHT_PROTOCOL_RESULT_LIMIT_EXCEEDED
    ) == MOONLIGHT_PROTOCOL_V1_DATAGRAM_DISPOSITION_PROTOCOL_VIOLATION
  );
  TEST_CHECK(
    MoonlightProtocolV1ClassifyReceivedDatagramResult(
      (MoonlightProtocolResult) -1
    ) == MOONLIGHT_PROTOCOL_V1_DATAGRAM_DISPOSITION_LOCAL_ERROR
  );
  return true;
}

/**
 * @brief Exercises Video truncation, trailing bytes, malformed values, and context errors.
 *
 * @return True when every Video error has the required result.
 */
static bool test_video_error_paths(void) {
  uint8_t encoded[TEST_VECTOR_CAPACITY];
  uint8_t vector[TEST_VECTOR_CAPACITY];
  uint8_t mutated[TEST_VECTOR_CAPACITY];
  MoonlightProtocolV1VideoDatagramView view;
  MoonlightProtocolV1DatagramHeader datagram;
  MoonlightProtocolV1VideoShardHeader shard;
  size_t encoded_size;
  size_t i;
  size_t other_size = 0;
  size_t vector_size = 0;

  TEST_CHECK(load_golden("video-data.hex", vector, sizeof(vector), &vector_size));
  TEST_RESULT(
    MoonlightProtocolV1ParseVideoDatagram(&TEST_VIDEO_CONTEXT, vector, vector_size, &view),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  datagram = view.datagram;
  shard = view.shard;

  TEST_RESULT(
    MoonlightProtocolV1ParseVideoDatagram(&TEST_VIDEO_CONTEXT, NULL, vector_size, &view),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1ParseVideoDatagram(&TEST_VIDEO_CONTEXT, vector, vector_size, NULL),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_CHECK(load_golden("audio-data.hex", mutated, sizeof(mutated), &other_size));
  TEST_RESULT(
    MoonlightProtocolV1ParseVideoDatagram(&TEST_VIDEO_CONTEXT, mutated, other_size, &view),
    MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
  );
  for (i = 0; i < vector_size; ++i) {
    TEST_RESULT(
      MoonlightProtocolV1ParseVideoDatagram(&TEST_VIDEO_CONTEXT, vector, i, &view),
      MOONLIGHT_PROTOCOL_RESULT_TRUNCATED
    );
  }
  memcpy(mutated, vector, vector_size);
  mutated[vector_size] = 0;
  TEST_RESULT(
    MoonlightProtocolV1ParseVideoDatagram(&TEST_VIDEO_CONTEXT, mutated, vector_size + 1, &view),
    MOONLIGHT_PROTOCOL_RESULT_TRAILING_DATA
  );

  memcpy(mutated, vector, vector_size);
  mutated[MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE + 20] = 1;
  TEST_RESULT(
    MoonlightProtocolV1ParseVideoDatagram(&TEST_VIDEO_CONTEXT, mutated, vector_size, &view),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, vector, vector_size);
  store_test_u32(mutated + MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE, 0);
  TEST_RESULT(
    MoonlightProtocolV1ParseVideoDatagram(&TEST_VIDEO_CONTEXT, mutated, vector_size, &view),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, vector, vector_size);
  store_test_u16(mutated + MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE + 6, 0);
  TEST_RESULT(
    MoonlightProtocolV1ParseVideoDatagram(&TEST_VIDEO_CONTEXT, mutated, vector_size, &view),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, vector, vector_size);
  store_test_u16(mutated + MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE + 4, 1);
  TEST_RESULT(
    MoonlightProtocolV1ParseVideoDatagram(&TEST_VIDEO_CONTEXT, mutated, vector_size, &view),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, vector, vector_size);
  store_test_u16(mutated + MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE + 10, 255);
  store_test_u16(mutated + MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE + 12, 1);
  TEST_RESULT(
    MoonlightProtocolV1ParseVideoDatagram(&TEST_VIDEO_CONTEXT, mutated, vector_size, &view),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, vector, vector_size);
  store_test_u16(mutated + MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE + 8, 2);
  TEST_RESULT(
    MoonlightProtocolV1ParseVideoDatagram(&TEST_VIDEO_CONTEXT, mutated, vector_size, &view),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );

  memcpy(mutated, vector, vector_size);
  store_test_u16(mutated + MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE + 14, 496);
  TEST_RESULT(
    MoonlightProtocolV1ParseVideoDatagram(&TEST_VIDEO_CONTEXT, mutated, TEST_VIDEO_CODING_OFFSET + 496, &view),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, vector, vector_size);
  store_test_u16(mutated + MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE + 14, 513);
  mutated[vector_size] = 0;
  TEST_RESULT(
    MoonlightProtocolV1ParseVideoDatagram(&TEST_VIDEO_CONTEXT, mutated, vector_size + 1, &view),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );

  memcpy(mutated, vector, vector_size);
  mutated[0] = MOONLIGHT_PROTOCOL_V1_CHANNEL_VIDEO_PARITY;
  TEST_RESULT(
    MoonlightProtocolV1ParseVideoDatagram(&TEST_VIDEO_CONTEXT, mutated, vector_size, &view),
    MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
  );
  memcpy(mutated, vector, vector_size);
  mutated[1] = MOONLIGHT_PROTOCOL_V1_VIDEO_FLAG_KEY_FRAME |
               MOONLIGHT_PROTOCOL_V1_VIDEO_FLAG_START_OF_FRAME;
  TEST_RESULT(
    MoonlightProtocolV1ParseVideoDatagram(&TEST_VIDEO_CONTEXT, mutated, vector_size, &view),
    MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
  );
  memcpy(mutated, vector, vector_size);
  mutated[1] = MOONLIGHT_PROTOCOL_V1_VIDEO_FLAG_KEY_FRAME |
               MOONLIGHT_PROTOCOL_V1_VIDEO_FLAG_START_OF_FRAME;
  store_test_u16(mutated + MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE + 10, 2);
  TEST_RESULT(
    MoonlightProtocolV1ParseVideoDatagram(&TEST_VIDEO_CONTEXT, mutated, vector_size, &view),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );

  memcpy(mutated, vector, vector_size);
  mutated[TEST_VIDEO_CODING_OFFSET] = 0;
  mutated[TEST_VIDEO_CODING_OFFSET + 1] = 0;
  TEST_RESULT(
    MoonlightProtocolV1ParseVideoDatagram(&TEST_VIDEO_CONTEXT, mutated, vector_size, &view),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, vector, vector_size);
  mutated[vector_size - 1] = 1;
  TEST_RESULT(
    MoonlightProtocolV1ParseVideoDatagram(&TEST_VIDEO_CONTEXT, mutated, vector_size, &view),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, vector, vector_size);
  mutated[0] = 0xff;
  TEST_RESULT(
    MoonlightProtocolV1ParseVideoDatagram(&TEST_VIDEO_CONTEXT, mutated, vector_size, &view),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  memcpy(mutated, vector, vector_size);
  mutated[1] = 0x80;
  TEST_RESULT(
    MoonlightProtocolV1ParseVideoDatagram(&TEST_VIDEO_CONTEXT, mutated, vector_size, &view),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  memcpy(mutated, vector, vector_size);
  mutated[1] = MOONLIGHT_PROTOCOL_V1_VIDEO_FLAG_KEY_FRAME |
               MOONLIGHT_PROTOCOL_V1_VIDEO_FLAG_START_OF_FRAME |
               MOONLIGHT_PROTOCOL_V1_VIDEO_FLAG_END_OF_FRAME |
               MOONLIGHT_PROTOCOL_V1_VIDEO_FLAG_REFERENCE_RECOVERY;
  TEST_RESULT(
    MoonlightProtocolV1ParseVideoDatagram(&TEST_VIDEO_CONTEXT, mutated, vector_size, &view),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );

  encoded_size = 99;
  TEST_RESULT(
    MoonlightProtocolV1EncodeVideoDatagram(&TEST_VIDEO_CONTEXT, &datagram, &shard, vector + TEST_VIDEO_CODING_OFFSET, 512, encoded, vector_size - 1, &encoded_size),
    MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL
  );
  TEST_CHECK(encoded_size == vector_size);
  TEST_RESULT(
    MoonlightProtocolV1EncodeVideoDatagram(&TEST_VIDEO_CONTEXT, &datagram, &shard, vector + TEST_VIDEO_CODING_OFFSET, 512, NULL, sizeof(encoded), &encoded_size),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeVideoDatagram(&TEST_VIDEO_CONTEXT, &datagram, &shard, vector + TEST_VIDEO_CODING_OFFSET, 512, encoded, sizeof(encoded), NULL),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  encoded_size = 99;
  TEST_RESULT(
    MoonlightProtocolV1EncodeVideoDatagram(&TEST_VIDEO_CONTEXT, NULL, &shard, vector + TEST_VIDEO_CODING_OFFSET, 512, encoded, sizeof(encoded), &encoded_size),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_CHECK(encoded_size == 0);
  encoded_size = 99;
  TEST_RESULT(
    MoonlightProtocolV1EncodeVideoDatagram(&TEST_VIDEO_CONTEXT, &datagram, NULL, vector + TEST_VIDEO_CODING_OFFSET, 512, encoded, sizeof(encoded), &encoded_size),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_CHECK(encoded_size == 0);
  encoded_size = 99;
  TEST_RESULT(
    MoonlightProtocolV1EncodeVideoDatagram(&TEST_VIDEO_CONTEXT, &datagram, &shard, NULL, 512, encoded, sizeof(encoded), &encoded_size),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_CHECK(encoded_size == 0);

  datagram.channel = MOONLIGHT_PROTOCOL_V1_CHANNEL_AUDIO_DATA;
  datagram.flags = 0;
  datagram.media_epoch = 0;
  TEST_RESULT(
    MoonlightProtocolV1EncodeVideoDatagram(&TEST_VIDEO_CONTEXT, &datagram, &shard, vector + TEST_VIDEO_CODING_OFFSET, 512, encoded, sizeof(encoded), &encoded_size),
    MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
  );
  datagram = view.datagram;

  memcpy(encoded, vector + TEST_VIDEO_CODING_OFFSET, 512);
  TEST_RESULT(
    MoonlightProtocolV1EncodeVideoDatagram(&TEST_VIDEO_CONTEXT, &datagram, &shard, encoded, 512, encoded, sizeof(encoded), &encoded_size),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(encoded_size == vector_size);
  TEST_CHECK(memcmp(encoded, vector, vector_size) == 0);

  encoded_size = 99;
  shard.frame_id = UINT32_C(0x80000000);
  TEST_RESULT(
    MoonlightProtocolV1EncodeVideoDatagram(&TEST_VIDEO_CONTEXT, &datagram, &shard, vector + TEST_VIDEO_CODING_OFFSET, 512, encoded, sizeof(encoded), &encoded_size),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  TEST_CHECK(encoded_size == 0);
  shard = view.shard;
  shard.coding_shard_bytes = UINT16_MAX;
  TEST_RESULT(
    MoonlightProtocolV1EncodeVideoDatagram(&TEST_VIDEO_CONTEXT, &datagram, &shard, vector + TEST_VIDEO_CODING_OFFSET, 512, encoded, sizeof(encoded), &encoded_size),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  shard = view.shard;
  datagram.channel = MOONLIGHT_PROTOCOL_V1_CHANNEL_VIDEO_PARITY;
  datagram.flags = MOONLIGHT_PROTOCOL_V1_VIDEO_FLAG_KEY_FRAME;
  shard.parity_shard_count = 0;
  TEST_RESULT(
    MoonlightProtocolV1EncodeVideoDatagram(&TEST_VIDEO_CONTEXT, &datagram, &shard, vector + TEST_VIDEO_CODING_OFFSET, 512, encoded, sizeof(encoded), &encoded_size),
    MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
  );
  return true;
}

/**
 * @brief Exercises Audio truncation, trailing bytes, malformed values, and context errors.
 *
 * @return True when every Audio error has the required result.
 */
static bool test_audio_error_paths(void) {
  uint8_t encoded[TEST_VECTOR_CAPACITY];
  uint8_t vector[TEST_VECTOR_CAPACITY];
  uint8_t mutated[TEST_VECTOR_CAPACITY];
  MoonlightProtocolV1AudioDatagramView view;
  MoonlightProtocolV1DatagramHeader datagram;
  MoonlightProtocolV1AudioShardHeader shard;
  size_t encoded_size;
  size_t i;
  size_t other_size = 0;
  size_t vector_size = 0;

  TEST_CHECK(load_golden("audio-data.hex", vector, sizeof(vector), &vector_size));
  TEST_RESULT(
    MoonlightProtocolV1ParseAudioDatagram(&TEST_AUDIO_CONTEXT, vector, vector_size, &view),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  datagram = view.datagram;
  shard = view.shard;

  TEST_RESULT(
    MoonlightProtocolV1ParseAudioDatagram(&TEST_AUDIO_CONTEXT, NULL, vector_size, &view),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1ParseAudioDatagram(&TEST_AUDIO_CONTEXT, vector, vector_size, NULL),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_CHECK(load_golden("video-data.hex", mutated, sizeof(mutated), &other_size));
  TEST_RESULT(
    MoonlightProtocolV1ParseAudioDatagram(&TEST_AUDIO_CONTEXT, mutated, other_size, &view),
    MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
  );
  for (i = 0; i < vector_size; ++i) {
    TEST_RESULT(
      MoonlightProtocolV1ParseAudioDatagram(&TEST_AUDIO_CONTEXT, vector, i, &view),
      MOONLIGHT_PROTOCOL_RESULT_TRUNCATED
    );
  }
  memcpy(mutated, vector, vector_size);
  mutated[vector_size] = 0;
  TEST_RESULT(
    MoonlightProtocolV1ParseAudioDatagram(&TEST_AUDIO_CONTEXT, mutated, vector_size + 1, &view),
    MOONLIGHT_PROTOCOL_RESULT_TRAILING_DATA
  );
  memcpy(mutated, vector, vector_size);
  mutated[2] = 1;
  TEST_RESULT(
    MoonlightProtocolV1ParseAudioDatagram(&TEST_AUDIO_CONTEXT, mutated, vector_size, &view),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );

  memcpy(mutated, vector, vector_size);
  store_test_u16(mutated + MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE + 6, 3);
  TEST_RESULT(
    MoonlightProtocolV1ParseAudioDatagram(&TEST_AUDIO_CONTEXT, mutated, vector_size, &view),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, vector, vector_size);
  store_test_u16(mutated + MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE + 8, 3);
  TEST_RESULT(
    MoonlightProtocolV1ParseAudioDatagram(&TEST_AUDIO_CONTEXT, mutated, vector_size, &view),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, vector, vector_size);
  store_test_u16(mutated + MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE + 4, 6);
  TEST_RESULT(
    MoonlightProtocolV1ParseAudioDatagram(&TEST_AUDIO_CONTEXT, mutated, vector_size, &view),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, vector, vector_size);
  mutated[MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE + 19] = 0x81;
  TEST_RESULT(
    MoonlightProtocolV1ParseAudioDatagram(&TEST_AUDIO_CONTEXT, mutated, vector_size, &view),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, vector, vector_size);
  store_test_u32(mutated + MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE, 3);
  TEST_RESULT(
    MoonlightProtocolV1ParseAudioDatagram(&TEST_AUDIO_CONTEXT, mutated, vector_size, &view),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, vector, vector_size);
  store_test_u16(mutated + MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE + 10, 0);
  TEST_RESULT(
    MoonlightProtocolV1ParseAudioDatagram(&TEST_AUDIO_CONTEXT, mutated, TEST_AUDIO_CODING_OFFSET, &view),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, vector, vector_size);
  store_test_u16(mutated + MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE + 10, 17);
  mutated[vector_size] = 0;
  TEST_RESULT(
    MoonlightProtocolV1ParseAudioDatagram(&TEST_AUDIO_CONTEXT, mutated, vector_size + 1, &view),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );

  memcpy(mutated, vector, vector_size);
  mutated[0] = MOONLIGHT_PROTOCOL_V1_CHANNEL_AUDIO_PARITY;
  TEST_RESULT(
    MoonlightProtocolV1ParseAudioDatagram(&TEST_AUDIO_CONTEXT, mutated, vector_size, &view),
    MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
  );
  memcpy(mutated, vector, vector_size);
  store_test_u16(mutated + MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE + 4, 4);
  TEST_RESULT(
    MoonlightProtocolV1ParseAudioDatagram(&TEST_AUDIO_CONTEXT, mutated, vector_size, &view),
    MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
  );
  memcpy(mutated, vector, vector_size);
  mutated[0] = MOONLIGHT_PROTOCOL_V1_CHANNEL_VIDEO_DATA;
  mutated[1] = MOONLIGHT_PROTOCOL_V1_VIDEO_FLAG_START_OF_FRAME |
               MOONLIGHT_PROTOCOL_V1_VIDEO_FLAG_END_OF_FRAME;
  store_test_u32(mutated + 8, 1);
  TEST_RESULT(
    MoonlightProtocolV1ParseAudioDatagram(&TEST_AUDIO_CONTEXT, mutated, vector_size, &view),
    MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
  );

  memcpy(mutated, vector, vector_size);
  mutated[TEST_AUDIO_CODING_OFFSET] = 0;
  mutated[TEST_AUDIO_CODING_OFFSET + 1] = 0;
  TEST_RESULT(
    MoonlightProtocolV1ParseAudioDatagram(&TEST_AUDIO_CONTEXT, mutated, vector_size, &view),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, vector, vector_size);
  mutated[vector_size - 1] = 1;
  TEST_RESULT(
    MoonlightProtocolV1ParseAudioDatagram(&TEST_AUDIO_CONTEXT, mutated, vector_size, &view),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );

  encoded_size = 99;
  TEST_RESULT(
    MoonlightProtocolV1EncodeAudioDatagram(&TEST_AUDIO_CONTEXT, &datagram, &shard, vector + TEST_AUDIO_CODING_OFFSET, 16, encoded, vector_size - 1, &encoded_size),
    MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL
  );
  TEST_CHECK(encoded_size == vector_size);
  TEST_RESULT(
    MoonlightProtocolV1EncodeAudioDatagram(&TEST_AUDIO_CONTEXT, &datagram, &shard, vector + TEST_AUDIO_CODING_OFFSET, 16, NULL, sizeof(encoded), &encoded_size),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeAudioDatagram(&TEST_AUDIO_CONTEXT, &datagram, &shard, vector + TEST_AUDIO_CODING_OFFSET, 16, encoded, sizeof(encoded), NULL),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  encoded_size = 99;
  TEST_RESULT(
    MoonlightProtocolV1EncodeAudioDatagram(&TEST_AUDIO_CONTEXT, NULL, &shard, vector + TEST_AUDIO_CODING_OFFSET, 16, encoded, sizeof(encoded), &encoded_size),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_CHECK(encoded_size == 0);
  encoded_size = 99;
  TEST_RESULT(
    MoonlightProtocolV1EncodeAudioDatagram(&TEST_AUDIO_CONTEXT, &datagram, NULL, vector + TEST_AUDIO_CODING_OFFSET, 16, encoded, sizeof(encoded), &encoded_size),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_CHECK(encoded_size == 0);
  encoded_size = 99;
  TEST_RESULT(
    MoonlightProtocolV1EncodeAudioDatagram(&TEST_AUDIO_CONTEXT, &datagram, &shard, NULL, 16, encoded, sizeof(encoded), &encoded_size),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_CHECK(encoded_size == 0);

  datagram.channel = MOONLIGHT_PROTOCOL_V1_CHANNEL_VIDEO_DATA;
  datagram.flags = MOONLIGHT_PROTOCOL_V1_VIDEO_FLAG_KEY_FRAME |
                   MOONLIGHT_PROTOCOL_V1_VIDEO_FLAG_START_OF_FRAME |
                   MOONLIGHT_PROTOCOL_V1_VIDEO_FLAG_END_OF_FRAME;
  datagram.media_epoch = 1;
  TEST_RESULT(
    MoonlightProtocolV1EncodeAudioDatagram(&TEST_AUDIO_CONTEXT, &datagram, &shard, vector + TEST_AUDIO_CODING_OFFSET, 16, encoded, sizeof(encoded), &encoded_size),
    MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
  );
  datagram = view.datagram;

  memcpy(encoded, vector + TEST_AUDIO_CODING_OFFSET, 16);
  TEST_RESULT(
    MoonlightProtocolV1EncodeAudioDatagram(&TEST_AUDIO_CONTEXT, &datagram, &shard, encoded, 16, encoded, sizeof(encoded), &encoded_size),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(encoded_size == vector_size);
  TEST_CHECK(memcmp(encoded, vector, vector_size) == 0);

  encoded_size = 99;
  shard.coding_shard_bytes = MOONLIGHT_PROTOCOL_V1_AUDIO_CODING_SHARD_MAX + 16;
  TEST_RESULT(
    MoonlightProtocolV1EncodeAudioDatagram(&TEST_AUDIO_CONTEXT, &datagram, &shard, vector + TEST_AUDIO_CODING_OFFSET, 16, encoded, sizeof(encoded), &encoded_size),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  TEST_CHECK(encoded_size == 0);
  return true;
}

/**
 * @brief Native test entry.
 *
 * @return Zero when all codec and corpus checks pass.
 */
int main(void) {
  static const struct {
    const char *name;  ///< Human-readable test name.
    bool (*function)(void);  ///< Test function.
  } tests[] = {
    {"fixed Audio profile", test_audio_profile_constants},
    {"video data golden", test_video_data_golden},
    {"video parity golden", test_video_parity_golden},
    {"audio data golden", test_audio_data_golden},
    {"audio parity golden", test_audio_parity_golden},
    {"Reed-Solomon encoding", test_reed_solomon_encoding},
    {"Reed-Solomon reconstruction", test_reed_solomon_reconstruction},
    {"data coding shard validation", test_data_coding_shard_validation},
    {"DATAGRAM header validation", test_datagram_header_validation},
    {"explicit context binding", test_explicit_context_binding},
    {"received DATAGRAM disposition", test_received_datagram_disposition},
    {"Video error paths", test_video_error_paths},
    {"Audio error paths", test_audio_error_paths},
  };

  size_t i;

  for (i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
    if (!tests[i].function()) {
      fprintf(stderr, "FAILED: %s\n", tests[i].name);
      return 1;
    }
    printf("PASS: %s\n", tests[i].name);
  }

  return 0;
}
