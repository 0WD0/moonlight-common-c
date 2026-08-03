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
#define MOONLIGHT_PROTOCOL_V1_SESSION_READY_REQUEST_PAYLOAD_SIZE 80u

/**
 * @brief Minimum canonical PREPARE_SESSION_REPLACEMENT request payload size.
 */
#define MOONLIGHT_PROTOCOL_V1_PREPARE_SESSION_REPLACEMENT_REQUEST_PAYLOAD_MIN 168u

/**
 * @brief Maximum canonical PREPARE_SESSION_REPLACEMENT request payload size.
 */
#define MOONLIGHT_PROTOCOL_V1_PREPARE_SESSION_REPLACEMENT_REQUEST_PAYLOAD_MAX 188u

/**
 * @brief Exact canonical successful PREPARE_SESSION_REPLACEMENT response size.
 */
#define MOONLIGHT_PROTOCOL_V1_PREPARE_SESSION_REPLACEMENT_RESPONSE_PAYLOAD_SIZE 262u

/**
 * @brief Exact canonical COMMIT_SESSION_REPLACEMENT request payload size.
 */
#define MOONLIGHT_PROTOCOL_V1_COMMIT_SESSION_REPLACEMENT_REQUEST_PAYLOAD_SIZE 136u

/**
 * @brief Exact canonical CANCEL_SESSION_REPLACEMENT request payload size.
 */
#define MOONLIGHT_PROTOCOL_V1_CANCEL_SESSION_REPLACEMENT_REQUEST_PAYLOAD_SIZE 72u

/**
 * @brief Smallest Host-advertised replacement commit lifetime in milliseconds.
 */
#define MOONLIGHT_PROTOCOL_V1_SESSION_REPLACEMENT_LIFETIME_MIN_MS 5000u

/**
 * @brief Largest Host-advertised replacement commit lifetime in milliseconds.
 */
#define MOONLIGHT_PROTOCOL_V1_SESSION_REPLACEMENT_LIFETIME_MAX_MS 30000u

/**
 * @brief Smallest accepted complete Video access unit in bytes.
 */
#define MOONLIGHT_PROTOCOL_V1_VIDEO_ACCESS_UNIT_MIN 65536u

/**
 * @brief Largest accepted complete Video access unit in bytes.
 */
#define MOONLIGHT_PROTOCOL_V1_VIDEO_ACCESS_UNIT_MAX 33554432u

/**
 * @brief Exact canonical REQUEST_IDR request payload size.
 */
#define MOONLIGHT_PROTOCOL_V1_REQUEST_IDR_REQUEST_PAYLOAD_SIZE 45u

/**
 * @brief Exact canonical successful REQUEST_IDR response payload size.
 */
#define MOONLIGHT_PROTOCOL_V1_REQUEST_IDR_RESPONSE_PAYLOAD_SIZE 9u

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
    uint32_t maximum_video_access_unit_bytes;  ///< Client-owned complete Video access-unit ceiling.
  } MoonlightProtocolV1SessionReadyRequest;

  /**
   * @brief Holds one PREPARE_SESSION_REPLACEMENT request.
   *
   * The successor record is the exact canonical START_SESSION request schema.
   * The transaction layer additionally requires the predecessor identifiers to
   * name its active Stream Session and the target display to differ from it.
   */
  typedef struct MoonlightProtocolV1PrepareSessionReplacementRequest {
    uint8_t predecessor_session_id[MOONLIGHT_PROTOCOL_V1_SESSION_ID_SIZE];  ///< Exact active predecessor UUID.
    uint32_t predecessor_session_wire_id;  ///< Exact active predecessor wire identifier.
    MoonlightProtocolV1StartSessionRequest successor;  ///< Explicit target display and requested media profile.
  } MoonlightProtocolV1PrepareSessionReplacementRequest;

  /**
   * @brief Holds one successful PREPARE_SESSION_REPLACEMENT response.
   *
   * The successor record is the exact canonical START_SESSION successful
   * response schema and therefore owns fresh session identifiers and epoch 1.
   */
  typedef struct MoonlightProtocolV1PrepareSessionReplacementResponse {
    uint8_t replacement_id[MOONLIGHT_PROTOCOL_V1_SESSION_ID_SIZE];  ///< Nonzero connection-scoped replacement UUID.
    MoonlightProtocolV1StartSessionResponse successor;  ///< Dormant successor proposal.
    uint32_t commit_lifetime_milliseconds;  ///< Bounded relative commit lifetime.
  } MoonlightProtocolV1PrepareSessionReplacementResponse;

  /**
   * @brief Holds one COMMIT_SESSION_REPLACEMENT request.
   *
   * The successor record is the exact canonical SESSION_READY request schema.
   * The transaction layer matches all three identities to the prepared
   * replacement before entering its irreversible commit point.
   */
  typedef struct MoonlightProtocolV1CommitSessionReplacementRequest {
    uint8_t replacement_id[MOONLIGHT_PROTOCOL_V1_SESSION_ID_SIZE];  ///< Exact prepared replacement UUID.
    uint8_t predecessor_session_id[MOONLIGHT_PROTOCOL_V1_SESSION_ID_SIZE];  ///< Exact active predecessor UUID.
    MoonlightProtocolV1SessionReadyRequest successor;  ///< Exact successor identities and Client limits.
  } MoonlightProtocolV1CommitSessionReplacementRequest;

  /**
   * @brief Holds one CANCEL_SESSION_REPLACEMENT request.
   */
  typedef struct MoonlightProtocolV1CancelSessionReplacementRequest {
    uint8_t replacement_id[MOONLIGHT_PROTOCOL_V1_SESSION_ID_SIZE];  ///< Exact prepared replacement UUID.
    uint8_t predecessor_session_id[MOONLIGHT_PROTOCOL_V1_SESSION_ID_SIZE];  ///< Exact active predecessor UUID.
    uint8_t successor_session_id[MOONLIGHT_PROTOCOL_V1_SESSION_ID_SIZE];  ///< Exact dormant successor UUID.
  } MoonlightProtocolV1CancelSessionReplacementRequest;

  /**
   * @brief Defines protocol version 1 Video repair selection values.
   */
  typedef enum MoonlightProtocolV1VideoRepair {
    MOONLIGHT_PROTOCOL_V1_VIDEO_REPAIR_IDR = 1,  ///< Independent decoder refresh frame.
    MOONLIGHT_PROTOCOL_V1_VIDEO_REPAIR_REFERENCE_INVALIDATION = 2  ///< Reference-frame invalidation.
  } MoonlightProtocolV1VideoRepair;

  /**
   * @brief Holds one REQUEST_IDR request for an active Stream Session.
   *
   * A zero highest-complete frame means that no frame in the current Media
   * Epoch has been accepted by the decoder. Nonzero values are bounded to the
   * protocol's positive 31-bit Video frame-ID range.
   */
  typedef struct MoonlightProtocolV1RequestIdrRequest {
    uint8_t session_id[MOONLIGHT_PROTOCOL_V1_SESSION_ID_SIZE];  ///< Nonzero active Stream Session UUID.
    uint32_t highest_complete_video_frame;  ///< Zero or latest decoder-accepted frame ID.
    MoonlightProtocolV1VideoRepair preferred_repair;  ///< Client-preferred repair operation.
  } MoonlightProtocolV1RequestIdrRequest;

  /**
   * @brief Holds one successful REQUEST_IDR response.
   */
  typedef struct MoonlightProtocolV1RequestIdrResponse {
    MoonlightProtocolV1VideoRepair selected_repair;  ///< Host-selected repair operation.
  } MoonlightProtocolV1RequestIdrResponse;

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

  /**
   * @brief Encodes one canonical PREPARE_SESSION_REPLACEMENT request.
   *
   * The output buffer and `encoded_size` remain unchanged on failure.
   *
   * @param request Validated host-order request.
   * @param output Destination buffer.
   * @param output_size Available bytes in `output`.
   * @param encoded_size Receives the encoded payload size.
   * @return The codec result.
   */
  MoonlightProtocolResult
    MoonlightProtocolV1EncodePrepareSessionReplacementRequest(
      const MoonlightProtocolV1PrepareSessionReplacementRequest *request,
      uint8_t *output,
      size_t output_size,
      size_t *encoded_size
    );

  /**
   * @brief Decodes one canonical PREPARE_SESSION_REPLACEMENT request.
   *
   * `request` remains unchanged on failure.
   *
   * @param input Complete request payload.
   * @param input_size Number of bytes in `input`.
   * @param request Receives validated host-order values only on success.
   * @return The codec result.
   */
  MoonlightProtocolResult
    MoonlightProtocolV1DecodePrepareSessionReplacementRequest(
      const uint8_t *input,
      size_t input_size,
      MoonlightProtocolV1PrepareSessionReplacementRequest *request
    );

  /**
   * @brief Encodes one canonical successful replacement preparation response.
   *
   * The output buffer and `encoded_size` remain unchanged on failure.
   *
   * @param response Validated host-order response.
   * @param output Destination buffer.
   * @param output_size Available bytes in `output`.
   * @param encoded_size Receives the encoded payload size.
   * @return The codec result.
   */
  MoonlightProtocolResult
    MoonlightProtocolV1EncodePrepareSessionReplacementResponse(
      const MoonlightProtocolV1PrepareSessionReplacementResponse *response,
      uint8_t *output,
      size_t output_size,
      size_t *encoded_size
    );

  /**
   * @brief Decodes one canonical successful replacement preparation response.
   *
   * `response` remains unchanged on failure.
   *
   * @param input Complete response payload.
   * @param input_size Number of bytes in `input`.
   * @param response Receives validated host-order values only on success.
   * @return The codec result.
   */
  MoonlightProtocolResult
    MoonlightProtocolV1DecodePrepareSessionReplacementResponse(
      const uint8_t *input,
      size_t input_size,
      MoonlightProtocolV1PrepareSessionReplacementResponse *response
    );

  /**
   * @brief Encodes one canonical COMMIT_SESSION_REPLACEMENT request.
   *
   * The output buffer and `encoded_size` remain unchanged on failure.
   *
   * @param request Validated host-order request.
   * @param output Destination buffer.
   * @param output_size Available bytes in `output`.
   * @param encoded_size Receives the encoded payload size.
   * @return The codec result.
   */
  MoonlightProtocolResult
    MoonlightProtocolV1EncodeCommitSessionReplacementRequest(
      const MoonlightProtocolV1CommitSessionReplacementRequest *request,
      uint8_t *output,
      size_t output_size,
      size_t *encoded_size
    );

  /**
   * @brief Decodes one canonical COMMIT_SESSION_REPLACEMENT request.
   *
   * `request` remains unchanged on failure.
   *
   * @param input Complete request payload.
   * @param input_size Number of bytes in `input`.
   * @param request Receives validated host-order values only on success.
   * @return The codec result.
   */
  MoonlightProtocolResult
    MoonlightProtocolV1DecodeCommitSessionReplacementRequest(
      const uint8_t *input,
      size_t input_size,
      MoonlightProtocolV1CommitSessionReplacementRequest *request
    );

  /**
   * @brief Encodes one canonical CANCEL_SESSION_REPLACEMENT request.
   *
   * The output buffer and `encoded_size` remain unchanged on failure.
   *
   * @param request Validated host-order request.
   * @param output Destination buffer.
   * @param output_size Available bytes in `output`.
   * @param encoded_size Receives the encoded payload size.
   * @return The codec result.
   */
  MoonlightProtocolResult
    MoonlightProtocolV1EncodeCancelSessionReplacementRequest(
      const MoonlightProtocolV1CancelSessionReplacementRequest *request,
      uint8_t *output,
      size_t output_size,
      size_t *encoded_size
    );

  /**
   * @brief Decodes one canonical CANCEL_SESSION_REPLACEMENT request.
   *
   * `request` remains unchanged on failure.
   *
   * @param input Complete request payload.
   * @param input_size Number of bytes in `input`.
   * @param request Receives validated host-order values only on success.
   * @return The codec result.
   */
  MoonlightProtocolResult
    MoonlightProtocolV1DecodeCancelSessionReplacementRequest(
      const uint8_t *input,
      size_t input_size,
      MoonlightProtocolV1CancelSessionReplacementRequest *request
    );

  /**
   * @brief Encodes one canonical REQUEST_IDR request.
   *
   * The output buffer and `encoded_size` remain unchanged on failure.
   *
   * @param request Validated host-order request.
   * @param output Destination buffer.
   * @param output_size Available bytes in `output`.
   * @param encoded_size Receives the encoded payload size.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1EncodeRequestIdrRequest(
    const MoonlightProtocolV1RequestIdrRequest *request,
    uint8_t *output,
    size_t output_size,
    size_t *encoded_size
  );

  /**
   * @brief Decodes one complete canonical REQUEST_IDR request.
   *
   * `request` remains unchanged on failure. The transaction layer validates
   * the Stream Session identifier against its exact active runtime and decides
   * which defined repair operation is currently supported.
   *
   * @param input Complete request payload.
   * @param input_size Number of bytes in `input`.
   * @param request Receives validated host-order values only on success.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1DecodeRequestIdrRequest(
    const uint8_t *input,
    size_t input_size,
    MoonlightProtocolV1RequestIdrRequest *request
  );

  /**
   * @brief Encodes one canonical successful REQUEST_IDR response.
   *
   * The output buffer and `encoded_size` remain unchanged on failure.
   *
   * @param response Validated host-order response.
   * @param output Destination buffer.
   * @param output_size Available bytes in `output`.
   * @param encoded_size Receives the encoded payload size.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1EncodeRequestIdrResponse(
    const MoonlightProtocolV1RequestIdrResponse *response,
    uint8_t *output,
    size_t output_size,
    size_t *encoded_size
  );

  /**
   * @brief Decodes one complete canonical successful REQUEST_IDR response.
   *
   * `response` remains unchanged on failure.
   *
   * @param input Complete response payload.
   * @param input_size Number of bytes in `input`.
   * @param response Receives the validated Host selection only on success.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1DecodeRequestIdrResponse(
    const uint8_t *input,
    size_t input_size,
    MoonlightProtocolV1RequestIdrResponse *response
  );

#ifdef __cplusplus
}
#endif
