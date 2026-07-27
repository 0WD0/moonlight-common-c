/**
 * @file media.h
 * @brief Encodes and decodes canonical Sunshine protocol version 1 media DATAGRAMs.
 */
#pragma once

#include <moonlight/protocol/wire.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Size of the common Sunshine DATAGRAM header in bytes.
 */
#define MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE 16u

/**
 * @brief Size of a canonical Video Shard header in bytes.
 */
#define MOONLIGHT_PROTOCOL_V1_VIDEO_SHARD_HEADER_SIZE 24u

/**
 * @brief Size of a canonical Audio Shard header in bytes.
 */
#define MOONLIGHT_PROTOCOL_V1_AUDIO_SHARD_HEADER_SIZE 20u

/**
 * @brief Smallest permitted coding-shard allocation in bytes.
 */
#define MOONLIGHT_PROTOCOL_V1_CODING_SHARD_MIN 16u

/**
 * @brief Smallest active Video coding-shard size in bytes.
 */
#define MOONLIGHT_PROTOCOL_V1_VIDEO_CODING_SHARD_MIN 512u

/**
 * @brief Largest Video coding-shard size representable in a 65,535-byte DATAGRAM.
 */
#define MOONLIGHT_PROTOCOL_V1_VIDEO_CODING_SHARD_MAX 65488u

/**
 * @brief Largest Audio coding-shard size in bytes.
 */
#define MOONLIGHT_PROTOCOL_V1_AUDIO_CODING_SHARD_MAX 1152u

/**
 * @brief Largest negotiated meaningful Opus packet in bytes.
 */
#define MOONLIGHT_PROTOCOL_V1_AUDIO_PACKET_MAX 1150u

/**
 * @brief Maximum number of FEC blocks in one Video frame.
 */
#define MOONLIGHT_PROTOCOL_V1_VIDEO_FEC_BLOCK_MAX 64u

/**
 * @brief Maximum combined data and parity shards in one FEC block.
 */
#define MOONLIGHT_PROTOCOL_V1_FEC_SHARD_MAX 255u

/**
 * @brief Number of data shards in the fixed Audio FEC profile.
 */
#define MOONLIGHT_PROTOCOL_V1_AUDIO_DATA_SHARDS 4u

/**
 * @brief Number of parity shards in the fixed Audio FEC profile.
 */
#define MOONLIGHT_PROTOCOL_V1_AUDIO_PARITY_SHARDS 2u

/**
 * @brief Number of PCM samples advanced by one 20 ms Audio FEC block.
 */
#define MOONLIGHT_PROTOCOL_V1_AUDIO_BLOCK_SAMPLES 960u

  /**
   * @brief Identifies a Sunshine protocol version 1 DATAGRAM channel.
   */
  typedef enum MoonlightProtocolV1DatagramChannel {
    MOONLIGHT_PROTOCOL_V1_CHANNEL_VIDEO_DATA = 0x01,  ///< Host-to-Client Video data shard.
    MOONLIGHT_PROTOCOL_V1_CHANNEL_VIDEO_PARITY = 0x02,  ///< Host-to-Client Video parity shard.
    MOONLIGHT_PROTOCOL_V1_CHANNEL_AUDIO_DATA = 0x03,  ///< Host-to-Client Audio data shard.
    MOONLIGHT_PROTOCOL_V1_CHANNEL_AUDIO_PARITY = 0x04,  ///< Host-to-Client Audio parity shard.
    MOONLIGHT_PROTOCOL_V1_CHANNEL_REALTIME_INPUT = 0x05,  ///< Client-to-Host real-time input.
    MOONLIGHT_PROTOCOL_V1_CHANNEL_HAPTICS_STATE = 0x06,  ///< Host-to-Client haptics or state.
    MOONLIGHT_PROTOCOL_V1_CHANNEL_MEDIA_FEEDBACK = 0x07  ///< Client-to-Host media feedback.
  } MoonlightProtocolV1DatagramChannel;

  /**
   * @brief Defines the known Video DATAGRAM flag bits.
   */
  typedef enum MoonlightProtocolV1VideoFlag {
    MOONLIGHT_PROTOCOL_V1_VIDEO_FLAG_KEY_FRAME = 0x01,  ///< The shard belongs to an independently decodable key frame.
    MOONLIGHT_PROTOCOL_V1_VIDEO_FLAG_START_OF_FRAME = 0x02,  ///< The shard is the first data shard of the frame.
    MOONLIGHT_PROTOCOL_V1_VIDEO_FLAG_END_OF_FRAME = 0x04,  ///< The shard is the final data shard of the frame.
    MOONLIGHT_PROTOCOL_V1_VIDEO_FLAG_REFERENCE_RECOVERY = 0x08  ///< The shard belongs to a reference-recovery frame.
  } MoonlightProtocolV1VideoFlag;

  /**
   * @brief Defines negotiated media-channel bits used by explicit codec contexts.
   */
  typedef enum MoonlightProtocolV1MediaChannelMask {
    MOONLIGHT_PROTOCOL_V1_MEDIA_CHANNEL_VIDEO_DATA = 0x01,  ///< Permit Video data DATAGRAMs.
    MOONLIGHT_PROTOCOL_V1_MEDIA_CHANNEL_VIDEO_PARITY = 0x02,  ///< Permit Video parity DATAGRAMs.
    MOONLIGHT_PROTOCOL_V1_MEDIA_CHANNEL_AUDIO_DATA = 0x04,  ///< Permit Audio data DATAGRAMs.
    MOONLIGHT_PROTOCOL_V1_MEDIA_CHANNEL_AUDIO_PARITY = 0x08  ///< Permit Audio parity DATAGRAMs.
  } MoonlightProtocolV1MediaChannelMask;

  /**
   * @brief Classifies the required response to a complete received DATAGRAM parse result.
   */
  typedef enum MoonlightProtocolV1ReceivedDatagramDisposition {
    MOONLIGHT_PROTOCOL_V1_DATAGRAM_DISPOSITION_ACCEPT = 0,  ///< Continue with the successfully parsed DATAGRAM.
    MOONLIGHT_PROTOCOL_V1_DATAGRAM_DISPOSITION_DROP_STALE,  ///< Silently discard a delayed session or epoch DATAGRAM.
    MOONLIGHT_PROTOCOL_V1_DATAGRAM_DISPOSITION_LOCAL_ERROR,  ///< Correct a local API call or resource error.
    MOONLIGHT_PROTOCOL_V1_DATAGRAM_DISPOSITION_PROTOCOL_VIOLATION  ///< Close the peer connection for malformed wire input.
  } MoonlightProtocolV1ReceivedDatagramDisposition;

  /**
   * @brief Binds Video DATAGRAM validation to one active Stream Session and Media Epoch.
   */
  typedef struct MoonlightProtocolV1VideoContext {
    uint32_t session_wire_id;  ///< Exact nonzero wire ID of the active Stream Session.
    uint32_t media_epoch;  ///< Exact nonzero active Video Media Epoch.
    uint16_t complete_datagram_limit;  ///< Active maximum complete DATAGRAM payload in bytes.
    uint16_t coding_shard_bytes;  ///< Exact active `V_ready` coding-shard size.
    uint8_t enabled_channels;  ///< Negotiated Video bits from `MoonlightProtocolV1MediaChannelMask`.
    bool reference_recovery_enabled;  ///< Whether reference-frame invalidation was negotiated.
  } MoonlightProtocolV1VideoContext;

  /**
   * @brief Binds Audio DATAGRAM validation to one Stream Session and block limit snapshot.
   */
  typedef struct MoonlightProtocolV1AudioContext {
    uint32_t session_wire_id;  ///< Exact nonzero wire ID of the active Stream Session.
    uint16_t complete_datagram_limit;  ///< Applicable complete-DATAGRAM limit for this Audio block.
    uint16_t maximum_packet_bytes;  ///< Negotiated `P_sel` maximum meaningful Opus bytes.
    uint8_t enabled_channels;  ///< Negotiated Audio bits from `MoonlightProtocolV1MediaChannelMask`.
  } MoonlightProtocolV1AudioContext;

  /**
   * @brief Holds one immutable canonical Reed-Solomon coefficient matrix.
   *
   * The caller owns `coefficients` for the complete context lifetime and must
   * not modify it after initialization. The same initialized context may be
   * read concurrently. Concurrent calls are safe only when every byte range
   * written by one call is disjoint from every range accessed by another call
   * and all nonempty reconstruction workspaces are distinct.
   */
  typedef struct MoonlightProtocolV1RsContext {
    uint16_t data_shards;  ///< Number of systematic data rows in `[1, 255]`.
    uint16_t parity_shards;  ///< Number of parity rows with total rows at most 255.
    const uint8_t *coefficients;  ///< Borrowed row-major `parity_shards * data_shards` matrix.
  } MoonlightProtocolV1RsContext;

  /**
   * @brief Holds a common DATAGRAM header in host byte order.
   */
  typedef struct MoonlightProtocolV1DatagramHeader {
    MoonlightProtocolV1DatagramChannel channel;  ///< Registry channel carried by the DATAGRAM.
    uint8_t flags;  ///< Channel-specific flags.
    uint32_t session_wire_id;  ///< Nonzero short identifier of the active Stream Session.
    uint32_t media_epoch;  ///< Nonzero Video epoch, zero for Audio/input/haptics, or feedback Video epoch.
    uint32_t sequence;  ///< Per-channel and per-epoch serial number.
  } MoonlightProtocolV1DatagramHeader;

  /**
   * @brief Holds a Video Shard header in host byte order.
   */
  typedef struct MoonlightProtocolV1VideoShardHeader {
    uint32_t frame_id;  ///< Stream-Session-wide encoder-submission identifier in `[1, 0x7fffffff]`.
    uint16_t fec_block_index;  ///< Zero-based FEC block index in this frame.
    uint16_t fec_block_count;  ///< Total FEC blocks in this frame in `[1, 64]`.
    uint16_t shard_index;  ///< Wire row within the block.
    uint16_t data_shard_count;  ///< Number of data rows in the block.
    uint16_t parity_shard_count;  ///< Number of parity rows in the block.
    uint16_t coding_shard_bytes;  ///< Exact fixed coding-shard size for the active Media Epoch.
    uint32_t codec_configuration_generation;  ///< Opaque codec configuration generation checked by protocol state.
  } MoonlightProtocolV1VideoShardHeader;

  /**
   * @brief Holds an Audio Shard header in host byte order.
   */
  typedef struct MoonlightProtocolV1AudioShardHeader {
    uint32_t fec_block_id;  ///< Audio FEC block serial derived from the first PCM sample index.
    uint16_t shard_index;  ///< Fixed 4+2 Audio row in `[0, 5]`.
    uint16_t coding_shard_bytes;  ///< Per-block coding-shard size in `[16, 1152]`.
    uint64_t first_pcm_sample_index;  ///< First PCM sample index of the 20 ms block.
  } MoonlightProtocolV1AudioShardHeader;

  /**
   * @brief Borrows the meaningful codec bytes inside one canonical data coding shard.
   */
  typedef struct MoonlightProtocolV1DataShardView {
    uint16_t meaningful_bytes;  ///< Number of codec bytes before canonical zero padding.
    const uint8_t *bytes;  ///< Borrowed codec bytes immediately after the network-order length.
  } MoonlightProtocolV1DataShardView;

  /**
   * @brief Borrows one validated complete Video DATAGRAM.
   */
  typedef struct MoonlightProtocolV1VideoDatagramView {
    MoonlightProtocolV1DatagramHeader datagram;  ///< Decoded outer DATAGRAM header.
    MoonlightProtocolV1VideoShardHeader shard;  ///< Decoded Video Shard header.
    const uint8_t *coding_shard;  ///< Borrowed complete coding shard.
    size_t coding_shard_size;  ///< Size of `coding_shard`, equal to `shard.coding_shard_bytes`.
    bool is_parity;  ///< True when the channel and row identify a parity shard.
    MoonlightProtocolV1DataShardView data;  ///< Parsed data view, empty when `is_parity` is true.
  } MoonlightProtocolV1VideoDatagramView;

  /**
   * @brief Borrows one validated complete Audio DATAGRAM.
   */
  typedef struct MoonlightProtocolV1AudioDatagramView {
    MoonlightProtocolV1DatagramHeader datagram;  ///< Decoded outer DATAGRAM header.
    MoonlightProtocolV1AudioShardHeader shard;  ///< Decoded Audio Shard header.
    const uint8_t *coding_shard;  ///< Borrowed complete coding shard.
    size_t coding_shard_size;  ///< Size of `coding_shard`, equal to `shard.coding_shard_bytes`.
    bool is_parity;  ///< True when the channel and row identify a parity shard.
    MoonlightProtocolV1DataShardView data;  ///< Parsed data view, empty when `is_parity` is true.
  } MoonlightProtocolV1AudioDatagramView;

  /**
   * @brief Encodes one canonical common DATAGRAM header.
   *
   * The output buffer is not modified when validation fails.
   *
   * @param header Host-order header values.
   * @param direction Direction in which the encoded payload will travel.
   * @param output Destination buffer.
   * @param output_size Available bytes in `output`.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1EncodeDatagramHeader(
    const MoonlightProtocolV1DatagramHeader *header,
    MoonlightProtocolDirection direction,
    uint8_t *output,
    size_t output_size
  );

  /**
   * @brief Decodes one canonical common DATAGRAM header.
   *
   * @param input Source bytes containing at least one complete header.
   * @param input_size Available bytes in `input`.
   * @param direction Direction in which the payload was received.
   * @param header Receives host-order header values only on success.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1DecodeDatagramHeader(
    const uint8_t *input,
    size_t input_size,
    MoonlightProtocolDirection direction,
    MoonlightProtocolV1DatagramHeader *header
  );

  /**
   * @brief Builds one canonical data coding shard.
   *
   * `codec_bytes` may overlap `output`; the codec bytes are moved before the
   * canonical prefix and padding are written.
   *
   * @param codec_bytes Nonempty encoded codec bytes.
   * @param codec_size Number of encoded codec bytes.
   * @param output Destination coding-shard buffer; its size selects `S`.
   * @param output_size Exact coding-shard size in `[16, 65488]`, a multiple of 16.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1BuildDataCodingShard(
    const uint8_t *codec_bytes,
    size_t codec_size,
    uint8_t *output,
    size_t output_size
  );

  /**
   * @brief Validates and borrows one canonical data coding shard.
   *
   * @param input Complete coding-shard bytes.
   * @param input_size Exact coding-shard size.
   * @param maximum_meaningful_bytes Negotiated maximum codec bytes accepted from the shard.
   * @param view Receives a borrowed view only on success.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1ParseDataCodingShard(
    const uint8_t *input,
    size_t input_size,
    size_t maximum_meaningful_bytes,
    MoonlightProtocolV1DataShardView *view
  );

  /**
   * @brief Gets caller-owned storage required by a canonical Reed-Solomon context.
   *
   * @param data_shards Number of systematic data rows.
   * @param parity_shards Number of parity rows, which may be zero.
   * @param storage_size Receives `data_shards * parity_shards` on success.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1RsCoefficientStorageSize(
    uint16_t data_shards,
    uint16_t parity_shards,
    size_t *storage_size
  );

  /**
   * @brief Initializes one immutable canonical Reed-Solomon context.
   *
   * `coefficient_storage` must remain readable and unchanged while `context`
   * is used. When coefficient storage is required, its complete range and
   * `context` must be disjoint. For `parity_shards == 0`, it may be `NULL`
   * with size zero.
   *
   * @param context Context to initialize only on success.
   * @param data_shards Number of systematic data rows.
   * @param parity_shards Number of parity rows, which may be zero.
   * @param coefficient_storage Caller-owned coefficient matrix storage.
   * @param coefficient_storage_size Available coefficient storage in bytes.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1RsInitialize(
    MoonlightProtocolV1RsContext *context,
    uint16_t data_shards,
    uint16_t parity_shards,
    uint8_t *coefficient_storage,
    size_t coefficient_storage_size
  );

  /**
   * @brief Encodes every canonical parity coding shard.
   *
   * All readable and writable `shard_size` byte ranges must be pairwise
   * disjoint. Every writable parity range must also be disjoint from `context`,
   * its coefficient storage, and both shard pointer arrays. Every parity output
   * is fully overwritten. With zero parity, `parity_shards` may be `NULL` and
   * `parity_shard_count` must be zero.
   *
   * @param context Initialized immutable Reed-Solomon context.
   * @param shard_size Exact coding-shard size in `[16, 65488]`, a multiple of 16.
   * @param data_shards Exact array of readable systematic shard buffers.
   * @param data_shard_count Number of entries in `data_shards`.
   * @param parity_shards Exact array of writable parity shard buffers.
   * @param parity_shard_count Number of entries in `parity_shards`.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1RsEncode(
    const MoonlightProtocolV1RsContext *context,
    size_t shard_size,
    const uint8_t *const *data_shards,
    size_t data_shard_count,
    uint8_t *const *parity_shards,
    size_t parity_shard_count
  );

  /**
   * @brief Gets workspace required to reconstruct the marked missing data rows.
   *
   * `marks` must contain exactly `data_shards + parity_shards` entries. Zero
   * means present and one means missing; no other value is accepted.
   *
   * @param context Initialized immutable Reed-Solomon context.
   * @param marks Exact received-row erasure map.
   * @param mark_count Number of entries in `marks`.
   * @param workspace_size Receives the exact required byte count on success.
   * @return `MOONLIGHT_PROTOCOL_RESULT_INSUFFICIENT_SHARDS` when too few
   * parity rows remain, otherwise the codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1RsReconstructWorkspaceSize(
    const MoonlightProtocolV1RsContext *context,
    const uint8_t *marks,
    size_t mark_count,
    size_t *workspace_size
  );

  /**
   * @brief Reconstructs only marked missing systematic data rows.
   *
   * A present row points to a readable `shard_size` byte buffer. A missing
   * data row points to a writable destination of the same size. A missing
   * parity row may be `NULL`. All active shard ranges and the workspace must
   * be disjoint. `workspace`, `marks`, the `shards` pointer array, `context`,
   * and its coefficient storage must not overlap one another or any active
   * shard range. The function never changes `marks`, present rows, or parity
   * rows. Validation and matrix inversion complete before any destination is
   * modified.
   *
   * @param context Initialized immutable Reed-Solomon context.
   * @param shard_size Exact coding-shard size in `[16, 65488]`, a multiple of 16.
   * @param shards Exact wire-row-order array of data then parity buffers.
   * @param shard_count Number of entries in `shards`.
   * @param marks Exact received-row erasure map.
   * @param mark_count Number of entries in `marks`.
   * @param workspace Caller-owned reconstruction workspace.
   * @param workspace_size Available workspace in bytes.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1RsReconstructData(
    const MoonlightProtocolV1RsContext *context,
    size_t shard_size,
    uint8_t *const *shards,
    size_t shard_count,
    const uint8_t *marks,
    size_t mark_count,
    uint8_t *workspace,
    size_t workspace_size
  );

  /**
   * @brief Encodes one complete canonical Video DATAGRAM.
   *
   * The output buffer is not modified when validation fails.
   * `coding_shard` may overlap `output`.
   *
   * @param context Exact active Video context.
   * @param datagram Host-order outer header with a Video data or parity channel.
   * @param shard Host-order Video Shard header.
   * @param coding_shard Complete prebuilt data or parity coding shard.
   * @param coding_shard_size Size of `coding_shard`.
   * @param output Destination DATAGRAM buffer.
   * @param output_size Available bytes in `output`.
   * @param encoded_size Receives the required complete DATAGRAM size when arguments are structurally valid.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1EncodeVideoDatagram(
    const MoonlightProtocolV1VideoContext *context,
    const MoonlightProtocolV1DatagramHeader *datagram,
    const MoonlightProtocolV1VideoShardHeader *shard,
    const uint8_t *coding_shard,
    size_t coding_shard_size,
    uint8_t *output,
    size_t output_size,
    size_t *encoded_size
  );

  /**
   * @brief Decodes one complete canonical Video DATAGRAM without allocation.
   *
   * @param context Exact active Video context.
   * @param input Complete DATAGRAM payload.
   * @param input_size Exact DATAGRAM payload size.
   * @param view Receives decoded headers and borrowed coding-shard views only on success.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1ParseVideoDatagram(
    const MoonlightProtocolV1VideoContext *context,
    const uint8_t *input,
    size_t input_size,
    MoonlightProtocolV1VideoDatagramView *view
  );

  /**
   * @brief Encodes one complete canonical Audio DATAGRAM.
   *
   * The output buffer is not modified when validation fails.
   * `coding_shard` may overlap `output`.
   *
   * @param context Exact Stream Session and Audio block limit context.
   * @param datagram Host-order outer header with an Audio data or parity channel.
   * @param shard Host-order Audio Shard header.
   * @param coding_shard Complete prebuilt data or parity coding shard.
   * @param coding_shard_size Size of `coding_shard`.
   * @param output Destination DATAGRAM buffer.
   * @param output_size Available bytes in `output`.
   * @param encoded_size Receives the required complete DATAGRAM size when arguments are structurally valid.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1EncodeAudioDatagram(
    const MoonlightProtocolV1AudioContext *context,
    const MoonlightProtocolV1DatagramHeader *datagram,
    const MoonlightProtocolV1AudioShardHeader *shard,
    const uint8_t *coding_shard,
    size_t coding_shard_size,
    uint8_t *output,
    size_t output_size,
    size_t *encoded_size
  );

  /**
   * @brief Decodes one complete canonical Audio DATAGRAM without allocation.
   *
   * @param context Exact Stream Session and Audio block limit context.
   * @param input Complete DATAGRAM payload.
   * @param input_size Exact DATAGRAM payload size.
   * @param view Receives decoded headers and borrowed coding-shard views only on success.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1ParseAudioDatagram(
    const MoonlightProtocolV1AudioContext *context,
    const uint8_t *input,
    size_t input_size,
    MoonlightProtocolV1AudioDatagramView *view
  );

  /**
   * @brief Classifies a complete received DATAGRAM parser result.
   *
   * This helper is only for a directly received atomic DATAGRAM. A malformed
   * reconstructed coding shard invalidates its FEC block instead and must not
   * be passed to this helper.
   *
   * @param result Result returned by a complete DATAGRAM parser.
   * @return The required receiver disposition.
   */
  MoonlightProtocolV1ReceivedDatagramDisposition
    MoonlightProtocolV1ClassifyReceivedDatagramResult(MoonlightProtocolResult result);

#ifdef __cplusplus
}
#endif
