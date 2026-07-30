/**
 * @file session.h
 * @brief Defines the direct-display Stream Session schemas for protocol version 1.
 */
#pragma once

#include <moonlight/protocol/wire.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Exact byte count of an opaque Host Display identifier.
 */
#define MOONLIGHT_PROTOCOL_V1_SESSION_DISPLAY_ID_SIZE 16u

/**
 * @brief Exact byte count of a Stream Session UUID.
 */
#define MOONLIGHT_PROTOCOL_V1_SESSION_ID_SIZE 16u

/**
 * @brief Maximum ordered codec preferences in a START_SESSION request.
 */
#define MOONLIGHT_PROTOCOL_V1_VIDEO_CODEC_PREFERENCE_MAX 3u

/**
 * @brief Smallest requested or accepted Video width in pixels.
 */
#define MOONLIGHT_PROTOCOL_V1_VIDEO_WIDTH_MIN 320u

/**
 * @brief Largest requested or accepted Video width in pixels.
 */
#define MOONLIGHT_PROTOCOL_V1_VIDEO_WIDTH_MAX 16384u

/**
 * @brief Smallest requested or accepted Video height in pixels.
 */
#define MOONLIGHT_PROTOCOL_V1_VIDEO_HEIGHT_MIN 240u

/**
 * @brief Largest requested or accepted Video height in pixels.
 */
#define MOONLIGHT_PROTOCOL_V1_VIDEO_HEIGHT_MAX 16384u

/**
 * @brief Initial Media Epoch assigned to a new Stream Session.
 */
#define MOONLIGHT_PROTOCOL_V1_INITIAL_MEDIA_EPOCH 1u

/**
 * @brief Smallest complete DATAGRAM limit that can carry a Video shard.
 */
#define MOONLIGHT_PROTOCOL_V1_SESSION_DATAGRAM_MIN 552u

/**
 * @brief Smallest negotiated Video coding-shard size.
 */
#define MOONLIGHT_PROTOCOL_V1_SESSION_VIDEO_SHARD_MIN 512u

/**
 * @brief Largest negotiated Video coding-shard size representable by v1.
 */
#define MOONLIGHT_PROTOCOL_V1_SESSION_VIDEO_SHARD_MAX 65488u

/**
 * @brief Combined common and Video-shard header size outside a coding shard.
 */
#define MOONLIGHT_PROTOCOL_V1_SESSION_VIDEO_HEADER_SIZE 40u

/**
 * @brief Largest defined predicted-frame Video FEC percentage.
 */
#define MOONLIGHT_PROTOCOL_V1_VIDEO_FEC_MAX 40u

/**
 * @brief Largest permitted initial predicted-frame Video FEC percentage.
 */
#define MOONLIGHT_PROTOCOL_V1_VIDEO_INITIAL_FEC_MAX 25u

/**
 * @brief Minimum canonical START_SESSION request payload size.
 */
#define MOONLIGHT_PROTOCOL_V1_START_SESSION_REQUEST_PAYLOAD_MIN 124u

/**
 * @brief Maximum canonical START_SESSION request payload size.
 */
#define MOONLIGHT_PROTOCOL_V1_START_SESSION_REQUEST_PAYLOAD_MAX 144u

/**
 * @brief Exact canonical accepted Video-configuration record size.
 */
#define MOONLIGHT_PROTOCOL_V1_ACCEPTED_VIDEO_CONFIG_PAYLOAD_SIZE 142u

/**
 * @brief Exact canonical successful START_SESSION response payload size.
 */
#define MOONLIGHT_PROTOCOL_V1_START_SESSION_RESPONSE_PAYLOAD_SIZE 218u

/**
 * @brief Exact canonical SESSION_READY request payload size.
 */
#define MOONLIGHT_PROTOCOL_V1_SESSION_READY_REQUEST_PAYLOAD_SIZE 68u

  /**
   * @brief Defines the protocol version 1 Video codec registry.
   */
  typedef enum MoonlightProtocolV1VideoCodec {
    MOONLIGHT_PROTOCOL_V1_VIDEO_CODEC_H264 = 1,  ///< H.264/AVC.
    MOONLIGHT_PROTOCOL_V1_VIDEO_CODEC_HEVC = 2,  ///< H.265/HEVC.
    MOONLIGHT_PROTOCOL_V1_VIDEO_CODEC_AV1 = 3  ///< AV1.
  } MoonlightProtocolV1VideoCodec;

  /**
   * @brief Defines negotiated Video dynamic-range modes.
   */
  typedef enum MoonlightProtocolV1VideoDynamicRange {
    MOONLIGHT_PROTOCOL_V1_VIDEO_DYNAMIC_RANGE_SDR = 0,  ///< Standard dynamic range.
    MOONLIGHT_PROTOCOL_V1_VIDEO_DYNAMIC_RANGE_HDR10 = 1  ///< HDR10 dynamic range.
  } MoonlightProtocolV1VideoDynamicRange;

  /**
   * @brief Defines negotiated Video chroma-sampling modes.
   */
  typedef enum MoonlightProtocolV1VideoChroma {
    MOONLIGHT_PROTOCOL_V1_VIDEO_CHROMA_420 = 0,  ///< 4:2:0 chroma sampling.
    MOONLIGHT_PROTOCOL_V1_VIDEO_CHROMA_444 = 1  ///< 4:4:4 chroma sampling.
  } MoonlightProtocolV1VideoChroma;

  /**
   * @brief Holds one direct-display START_SESSION request.
   *
   * The display ID is an opaque selector from the exact Display Catalog
   * revision. Codec preferences remain in Client preference order and contain
   * no duplicate registry values. Frame rate is a reduced positive rational.
   */
  typedef struct MoonlightProtocolV1StartSessionRequest {
    uint8_t display_id[MOONLIGHT_PROTOCOL_V1_SESSION_DISPLAY_ID_SIZE];  ///< Nonzero opaque Host Display ID.
    uint64_t catalog_revision;  ///< Exact nonzero Display Catalog revision.
    uint16_t width;  ///< Requested Video width in `[320, 16384]`.
    uint16_t height;  ///< Requested Video height in `[240, 16384]`.
    uint32_t frame_rate_numerator;  ///< Nonzero reduced frame-rate numerator.
    uint32_t frame_rate_denominator;  ///< Nonzero reduced frame-rate denominator.
    uint32_t bitrate_kbps;  ///< Nonzero Client Video bitrate ceiling in kbit/s.
    MoonlightProtocolV1VideoCodec codec_preferences[MOONLIGHT_PROTOCOL_V1_VIDEO_CODEC_PREFERENCE_MAX];  ///< Ordered unique codec preferences.
    size_t codec_preference_count;  ///< Number of preferences in `[1, 3]`.
    MoonlightProtocolV1VideoDynamicRange dynamic_range;  ///< Requested SDR or HDR10 mode.
    MoonlightProtocolV1VideoChroma chroma;  ///< Requested 4:2:0 or 4:4:4 sampling.
  } MoonlightProtocolV1StartSessionRequest;

  /**
   * @brief Holds the accepted Video configuration nested in START_SESSION.
   */
  typedef struct MoonlightProtocolV1AcceptedVideoConfig {
    MoonlightProtocolV1VideoCodec codec;  ///< Selected Video codec.
    uint16_t profile;  ///< Codec profile registry value.
    uint8_t bit_depth;  ///< Accepted bit depth, exactly 8 or 10.
    MoonlightProtocolV1VideoChroma chroma;  ///< Accepted chroma sampling.
    MoonlightProtocolV1VideoDynamicRange dynamic_range;  ///< Accepted dynamic range.
    uint16_t width;  ///< Accepted Video width in `[320, 16384]`.
    uint16_t height;  ///< Accepted Video height in `[240, 16384]`.
    uint32_t frame_rate_numerator;  ///< Nonzero reduced frame-rate numerator.
    uint32_t frame_rate_denominator;  ///< Nonzero reduced frame-rate denominator.
    uint32_t bitrate_kbps;  ///< Nonzero initial Video target in kbit/s.
    uint8_t initial_fec_percentage;  ///< Initial predicted-frame FEC percentage.
    uint32_t codec_configuration_generation;  ///< Nonzero codec configuration generation.
    uint8_t minimum_fec_percentage;  ///< Minimum predicted-frame FEC percentage.
    uint8_t maximum_fec_percentage;  ///< Maximum predicted-frame FEC percentage.
  } MoonlightProtocolV1AcceptedVideoConfig;

  /**
   * @brief Holds one successful START_SESSION response.
   *
   * The Host proposal independently satisfies `D >= 552`,
   * `512 <= V <= 65488`, `V mod 16 == 0`, and `V + 40 <= D`.
   */
  typedef struct MoonlightProtocolV1StartSessionResponse {
    uint8_t session_id[MOONLIGHT_PROTOCOL_V1_SESSION_ID_SIZE];  ///< Nonzero Stream Session UUID.
    uint32_t session_wire_id;  ///< Nonzero short Stream Session identifier.
    uint32_t media_epoch;  ///< Initial Media Epoch, exactly one.
    uint16_t maximum_complete_datagram;  ///< Host complete-DATAGRAM upper bound.
    uint16_t maximum_video_shard;  ///< Host Video coding-shard upper bound.
    MoonlightProtocolV1AcceptedVideoConfig video;  ///< Accepted Video configuration.
  } MoonlightProtocolV1StartSessionResponse;

  /**
   * @brief Holds the SESSION_READY request for initial Media Epoch one.
   *
   * Session identifiers must equal the successful START_SESSION response.
   * The Client may lower the proposed DATAGRAM and Video-shard limits before
   * sending this request. This codec validates the request's standalone
   * `D`/`V` fit. The transaction layer MUST additionally reject either value
   * when it exceeds the corresponding Host proposal.
   */
  typedef struct MoonlightProtocolV1SessionReadyRequest {
    uint8_t session_id[MOONLIGHT_PROTOCOL_V1_SESSION_ID_SIZE];  ///< Nonzero Stream Session UUID.
    uint32_t session_wire_id;  ///< Nonzero short Stream Session identifier.
    uint32_t media_epoch;  ///< Initial Media Epoch, exactly one.
    uint16_t complete_datagram;  ///< Client-selected complete-DATAGRAM limit.
    uint16_t video_shard;  ///< Client-selected exact Video coding-shard size.
  } MoonlightProtocolV1SessionReadyRequest;

  /**
   * @brief Encodes one canonical direct-display START_SESSION request.
   *
   * The output buffer and `encoded_size` remain unchanged on failure.
   *
   * @param request Validated host-order request.
   * @param output Destination buffer.
   * @param output_size Available bytes in `output`.
   * @param encoded_size Receives the encoded payload size.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1EncodeStartSessionRequest(
    const MoonlightProtocolV1StartSessionRequest *request,
    uint8_t *output,
    size_t output_size,
    size_t *encoded_size
  );

  /**
   * @brief Decodes one complete canonical direct-display START_SESSION request.
   *
   * Unknown fields, schema flags, duplicate codec preferences, missing fields,
   * and noncanonical field ordering are rejected. `request` remains unchanged
   * on failure.
   *
   * @param input Complete request payload.
   * @param input_size Number of bytes in `input`.
   * @param request Receives validated host-order values only on success.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1DecodeStartSessionRequest(
    const uint8_t *input,
    size_t input_size,
    MoonlightProtocolV1StartSessionRequest *request
  );

  /**
   * @brief Encodes one canonical successful START_SESSION response.
   *
   * The output buffer and `encoded_size` remain unchanged on failure.
   *
   * @param response Validated host-order response.
   * @param output Destination buffer.
   * @param output_size Available bytes in `output`.
   * @param encoded_size Receives the encoded payload size.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1EncodeStartSessionResponse(
    const MoonlightProtocolV1StartSessionResponse *response,
    uint8_t *output,
    size_t output_size,
    size_t *encoded_size
  );

  /**
   * @brief Decodes one complete canonical successful START_SESSION response.
   *
   * The nested Video record is required, exact, and self-contained.
   * `response` remains unchanged on failure.
   *
   * @param input Complete response payload.
   * @param input_size Number of bytes in `input`.
   * @param response Receives validated host-order values only on success.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1DecodeStartSessionResponse(
    const uint8_t *input,
    size_t input_size,
    MoonlightProtocolV1StartSessionResponse *response
  );

  /**
   * @brief Encodes one canonical SESSION_READY request.
   *
   * The output buffer and `encoded_size` remain unchanged on failure.
   *
   * @param request Validated host-order request.
   * @param output Destination buffer.
   * @param output_size Available bytes in `output`.
   * @param encoded_size Receives the encoded payload size.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1EncodeSessionReadyRequest(
    const MoonlightProtocolV1SessionReadyRequest *request,
    uint8_t *output,
    size_t output_size,
    size_t *encoded_size
  );

  /**
   * @brief Decodes one complete canonical SESSION_READY request.
   *
   * `request` remains unchanged on failure. The surrounding transaction
   * validates identifier equality with the START_SESSION response and
   * requires both selected limits to be no greater than the Host proposals.
   *
   * @param input Complete request payload.
   * @param input_size Number of bytes in `input`.
   * @param request Receives validated host-order values only on success.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1DecodeSessionReadyRequest(
    const uint8_t *input,
    size_t input_size,
    MoonlightProtocolV1SessionReadyRequest *request
  );

#ifdef __cplusplus
}
#endif
