/**
 * @file media_fuzzer.c
 * @brief LibFuzzer entry point for Sunshine protocol version 1 media DATAGRAMs.
 */

#include <moonlight/protocol/media.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Capacity covering every representable complete Video DATAGRAM.
 */
#define FUZZ_ENCODE_CAPACITY 65536u

/**
 * @brief Maximum Audio coding-shard size used by the protocol profile.
 */
#define FUZZ_CODING_CAPACITY MOONLIGHT_PROTOCOL_V1_AUDIO_CODING_SHARD_MAX

/**
 * @brief Aborts when a successful decode cannot reproduce canonical input bytes.
 *
 * @param condition Invariant to require.
 */
static void require_invariant(int condition) {
  if (!condition) {
    abort();
  }
}

/**
 * @brief Loads one big-endian 16-bit integer from fuzzer-owned bytes.
 *
 * @param input Two readable bytes.
 * @return Host-order value.
 */
static uint16_t load_fuzz_u16(const uint8_t *input) {
  return (uint16_t) (((uint16_t) input[0] << 8u) | input[1]);
}

/**
 * @brief Multiplies two canonical version 1 field elements independently.
 *
 * @param left Left field element.
 * @param right Right field element.
 * @return Product reduced modulo primitive polynomial `0x11d`.
 */
static uint8_t multiply_fuzz_gf256(uint8_t left, uint8_t right) {
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
 * @brief Fuzzes fixed headers, coding shards, and complete media DATAGRAM parsers.
 *
 * @param data Arbitrary fuzzer-owned bytes.
 * @param size Number of readable bytes in `data`.
 * @return Zero after processing the input.
 */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  uint8_t coding_shard[FUZZ_CODING_CAPACITY];
  uint8_t encoded[FUZZ_ENCODE_CAPACITY];
  MoonlightProtocolV1AudioContext audio_context = {
    .session_wire_id = 1,
    .complete_datagram_limit = UINT16_MAX,
    .maximum_packet_bytes = MOONLIGHT_PROTOCOL_V1_AUDIO_PACKET_MAX,
    .enabled_channels = MOONLIGHT_PROTOCOL_V1_MEDIA_CHANNEL_AUDIO_DATA |
                        MOONLIGHT_PROTOCOL_V1_MEDIA_CHANNEL_AUDIO_PARITY,
  };
  MoonlightProtocolV1AudioDatagramView audio;
  MoonlightProtocolV1DataShardView data_shard;
  MoonlightProtocolV1DatagramHeader header;
  MoonlightProtocolV1VideoContext video_context = {
    .session_wire_id = 1,
    .media_epoch = 1,
    .complete_datagram_limit = UINT16_MAX,
    .coding_shard_bytes = MOONLIGHT_PROTOCOL_V1_VIDEO_CODING_SHARD_MIN,
    .enabled_channels = MOONLIGHT_PROTOCOL_V1_MEDIA_CHANNEL_VIDEO_DATA |
                        MOONLIGHT_PROTOCOL_V1_MEDIA_CHANNEL_VIDEO_PARITY,
    .reference_recovery_enabled = true,
  };
  MoonlightProtocolV1VideoDatagramView video;
  MoonlightProtocolResult result;
  size_t coding_size;
  size_t encoded_size;
  size_t payload_size;

  result = MoonlightProtocolV1DecodeDatagramHeader(
    data,
    size,
    MOONLIGHT_PROTOCOL_DIRECTION_HOST_TO_CLIENT,
    &header
  );
  if (result == MOONLIGHT_PROTOCOL_RESULT_OK) {
    require_invariant(
      MoonlightProtocolV1EncodeDatagramHeader(
        &header,
        MOONLIGHT_PROTOCOL_DIRECTION_HOST_TO_CLIENT,
        encoded,
        sizeof(encoded)
      ) == MOONLIGHT_PROTOCOL_RESULT_OK
    );
    require_invariant(size >= MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE);
    require_invariant(
      memcmp(encoded, data, MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE) == 0
    );
    if (header.channel == MOONLIGHT_PROTOCOL_V1_CHANNEL_VIDEO_DATA || header.channel == MOONLIGHT_PROTOCOL_V1_CHANNEL_VIDEO_PARITY) {
      uint16_t declared_coding_size = 0;

      video_context.session_wire_id = header.session_wire_id;
      video_context.media_epoch = header.media_epoch;
      if (size >= MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE + MOONLIGHT_PROTOCOL_V1_VIDEO_SHARD_HEADER_SIZE) {
        declared_coding_size = load_fuzz_u16(
          data + MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE + 14u
        );
      }
      if (declared_coding_size >= MOONLIGHT_PROTOCOL_V1_VIDEO_CODING_SHARD_MIN && declared_coding_size <= MOONLIGHT_PROTOCOL_V1_VIDEO_CODING_SHARD_MAX && declared_coding_size % 16u == 0) {
        video_context.coding_shard_bytes = declared_coding_size;
      }
    } else if (header.channel == MOONLIGHT_PROTOCOL_V1_CHANNEL_AUDIO_DATA || header.channel == MOONLIGHT_PROTOCOL_V1_CHANNEL_AUDIO_PARITY) {
      audio_context.session_wire_id = header.session_wire_id;
    }
  }
  (void) MoonlightProtocolV1DecodeDatagramHeader(
    data,
    size,
    MOONLIGHT_PROTOCOL_DIRECTION_CLIENT_TO_HOST,
    &header
  );

  if (size > 0) {
    coding_size = MOONLIGHT_PROTOCOL_V1_CODING_SHARD_MIN +
                  ((size_t) data[0] % 72u) * 16u;
    payload_size = size;
    if (payload_size > coding_size - 2u) {
      payload_size = coding_size - 2u;
    }
    require_invariant(payload_size > 0);
    require_invariant(
      MoonlightProtocolV1BuildDataCodingShard(
        data,
        payload_size,
        coding_shard,
        coding_size
      ) == MOONLIGHT_PROTOCOL_RESULT_OK
    );
    require_invariant(
      MoonlightProtocolV1ParseDataCodingShard(
        coding_shard,
        coding_size,
        coding_size - 2u,
        &data_shard
      ) == MOONLIGHT_PROTOCOL_RESULT_OK
    );
    require_invariant(data_shard.meaningful_bytes == payload_size);
    require_invariant(memcmp(data_shard.bytes, data, payload_size) == 0);
  }

  if (size >= MOONLIGHT_PROTOCOL_V1_CODING_SHARD_MIN && size <= UINT16_MAX && size % 16u == 0) {
    (void) MoonlightProtocolV1ParseDataCodingShard(
      data,
      size,
      size - 2u,
      &data_shard
    );
  }

  {
    uint8_t coefficients[8];
    uint8_t marks[6] = {0};
    uint8_t original_marks[6];
    uint8_t parity[2][16];
    uint8_t rows[6][16];
    uint8_t sources[4][16];
    uint8_t workspace[12];
    const uint8_t *source_pointers[4];
    uint8_t *parity_pointers[2] = {parity[0], parity[1]};
    uint8_t *row_pointers[6];
    MoonlightProtocolV1RsContext rs_context;
    size_t byte_index;
    size_t missing_data = 0;
    size_t row_index;
    size_t workspace_size = 0;

    for (row_index = 0; row_index < 4; ++row_index) {
      for (byte_index = 0; byte_index < 16; ++byte_index) {
        const size_t input_index = row_index * 16u + byte_index;

        sources[row_index][byte_index] =
          size == 0 ?
            (uint8_t) input_index :
            data[input_index % size];
      }
      source_pointers[row_index] = sources[row_index];
    }
    require_invariant(
      MoonlightProtocolV1RsInitialize(
        &rs_context,
        4,
        2,
        coefficients,
        sizeof(coefficients)
      ) == MOONLIGHT_PROTOCOL_RESULT_OK
    );
    require_invariant(
      MoonlightProtocolV1RsEncode(
        &rs_context,
        16,
        source_pointers,
        4,
        parity_pointers,
        2
      ) == MOONLIGHT_PROTOCOL_RESULT_OK
    );
    for (row_index = 0; row_index < 2; ++row_index) {
      for (byte_index = 0; byte_index < 16; ++byte_index) {
        uint8_t expected = 0;
        size_t data_index;

        for (data_index = 0; data_index < 4; ++data_index) {
          expected ^= multiply_fuzz_gf256(
            coefficients[row_index * 4u + data_index],
            sources[data_index][byte_index]
          );
        }
        require_invariant(parity[row_index][byte_index] == expected);
      }
    }

    for (row_index = 0; row_index < 4; ++row_index) {
      memcpy(rows[row_index], sources[row_index], 16);
    }
    memcpy(rows[4], parity[0], 16);
    memcpy(rows[5], parity[1], 16);
    marks[size == 0 ? 0 : data[0] % 4u] = 1;
    if (size > 1 && (data[1] & 1u) != 0) {
      marks[data[1] % 4u] = 1;
    }
    for (row_index = 0; row_index < 4; ++row_index) {
      if (marks[row_index] != 0) {
        ++missing_data;
      }
    }
    if (missing_data == 1 && size > 2 && (data[2] & 1u) != 0) {
      marks[4u + data[2] % 2u] = 1;
    }
    memcpy(original_marks, marks, sizeof(marks));
    for (row_index = 0; row_index < 6; ++row_index) {
      if (marks[row_index] != 0 && row_index < 4) {
        memset(rows[row_index], 0xa5, 16);
      }
      row_pointers[row_index] = marks[row_index] != 0 && row_index >= 4 ?
                                  NULL :
                                  rows[row_index];
    }
    require_invariant(
      MoonlightProtocolV1RsReconstructWorkspaceSize(
        &rs_context,
        marks,
        sizeof(marks),
        &workspace_size
      ) == MOONLIGHT_PROTOCOL_RESULT_OK
    );
    require_invariant(
      workspace_size ==
      2u * missing_data * missing_data + 2u * missing_data
    );
    require_invariant(
      MoonlightProtocolV1RsReconstructData(
        &rs_context,
        16,
        row_pointers,
        6,
        marks,
        sizeof(marks),
        workspace,
        workspace_size
      ) == MOONLIGHT_PROTOCOL_RESULT_OK
    );
    require_invariant(memcmp(marks, original_marks, sizeof(marks)) == 0);
    for (row_index = 0; row_index < 4; ++row_index) {
      require_invariant(memcmp(rows[row_index], sources[row_index], 16) == 0);
    }
  }

  result = MoonlightProtocolV1ParseVideoDatagram(
    &video_context,
    data,
    size,
    &video
  );
  (void) MoonlightProtocolV1ClassifyReceivedDatagramResult(result);
  if (result == MOONLIGHT_PROTOCOL_RESULT_OK) {
    encoded_size = 0;
    require_invariant(
      MoonlightProtocolV1EncodeVideoDatagram(
        &video_context,
        &video.datagram,
        &video.shard,
        video.coding_shard,
        video.coding_shard_size,
        encoded,
        sizeof(encoded),
        &encoded_size
      ) == MOONLIGHT_PROTOCOL_RESULT_OK
    );
    require_invariant(encoded_size == size);
    require_invariant(memcmp(encoded, data, size) == 0);
  }

  result = MoonlightProtocolV1ParseAudioDatagram(
    &audio_context,
    data,
    size,
    &audio
  );
  (void) MoonlightProtocolV1ClassifyReceivedDatagramResult(result);
  if (result == MOONLIGHT_PROTOCOL_RESULT_OK) {
    encoded_size = 0;
    require_invariant(
      MoonlightProtocolV1EncodeAudioDatagram(
        &audio_context,
        &audio.datagram,
        &audio.shard,
        audio.coding_shard,
        audio.coding_shard_size,
        encoded,
        sizeof(encoded),
        &encoded_size
      ) == MOONLIGHT_PROTOCOL_RESULT_OK
    );
    require_invariant(encoded_size == size);
    require_invariant(memcmp(encoded, data, size) == 0);
  }

  return 0;
}
