/**
 * @file control.h
 * @brief Defines protocol version 1 Control Lane message schemas.
 */
#pragma once

#include <moonlight/protocol/pairing.h>
#include <moonlight/protocol/wire.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Exact protocol minor accepted by version 1.
 */
#define MOONLIGHT_PROTOCOL_V1_MINOR 0u

/**
 * @brief Maximum encoded CLIENT_HELLO request payload.
 */
#define MOONLIGHT_PROTOCOL_V1_CLIENT_HELLO_REQUEST_PAYLOAD_MAX 98u

/**
 * @brief Exact encoded CLIENT_HELLO response payload size.
 */
#define MOONLIGHT_PROTOCOL_V1_CLIENT_HELLO_RESPONSE_PAYLOAD_SIZE 74u

/**
 * @brief Maximum complete stream-0 Early Hello byte count.
 */
#define MOONLIGHT_PROTOCOL_V1_EARLY_HELLO_SIZE_MAX \
  (MOONLIGHT_PROTOCOL_V1_LANE_PREFACE_SIZE + \
   MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE + \
   MOONLIGHT_PROTOCOL_V1_CLIENT_HELLO_REQUEST_PAYLOAD_MAX)

/**
 * @brief Maximum Client software-version UTF-8 byte count.
 */
#define MOONLIGHT_PROTOCOL_V1_SOFTWARE_VERSION_MAX 64u

/**
 * @brief Maximum opaque token byte count in a PING payload.
 */
#define MOONLIGHT_PROTOCOL_V1_PING_TOKEN_MAX 32u

/**
 * @brief Maximum canonical encoded PING request or response payload.
 */
#define MOONLIGHT_PROTOCOL_V1_PING_PAYLOAD_MAX \
  (MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + MOONLIGHT_PROTOCOL_V1_PING_TOKEN_MAX)

/**
 * @brief Size of a protocol version 1 UUID value.
 */
#define MOONLIGHT_PROTOCOL_V1_UUID_SIZE 16u

/**
 * @brief Mask of every defined protocol version 1 capability bit.
 */
#define MOONLIGHT_PROTOCOL_V1_CAPABILITY_MASK UINT64_C(0x3f)

/**
 * @brief Maximum Host display-name UTF-8 byte count.
 */
#define MOONLIGHT_PROTOCOL_V1_HOST_DISPLAY_NAME_MAX 128u

/**
 * @brief Process-wide protocol version 1 active Stream Session ceiling.
 */
#define MOONLIGHT_PROTOCOL_V1_ACTIVE_STREAM_SESSION_MAX 8u

/**
 * @brief Minimum encoded successful GET_HOST_INFO response payload size.
 */
#define MOONLIGHT_PROTOCOL_V1_HOST_INFO_RESPONSE_PAYLOAD_MIN \
  (10u * MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + \
   MOONLIGHT_PROTOCOL_V1_UUID_SIZE + 1u + 1u + 2u + 8u + 4u + 4u + 8u + 8u + 1u)

/**
 * @brief Maximum encoded successful GET_HOST_INFO response payload size.
 */
#define MOONLIGHT_PROTOCOL_V1_HOST_INFO_RESPONSE_PAYLOAD_MAX \
  (MOONLIGHT_PROTOCOL_V1_HOST_INFO_RESPONSE_PAYLOAD_MIN - 2u + \
   MOONLIGHT_PROTOCOL_V1_HOST_DISPLAY_NAME_MAX + \
   MOONLIGHT_PROTOCOL_V1_SOFTWARE_VERSION_MAX)

/**
 * @brief Exact byte count of an opaque GET_APP_LIST cursor.
 */
#define MOONLIGHT_PROTOCOL_V1_APP_LIST_CURSOR_SIZE 16u

/**
 * @brief Default GET_APP_LIST page size when request field 2 is absent.
 */
#define MOONLIGHT_PROTOCOL_V1_APP_LIST_DEFAULT_MAX_ENTRIES 64u

/**
 * @brief Maximum GET_APP_LIST application-record count in one page.
 */
#define MOONLIGHT_PROTOCOL_V1_APP_LIST_MAX_ENTRIES 128u

/**
 * @brief Maximum Application ID UTF-8 byte count.
 */
#define MOONLIGHT_PROTOCOL_V1_APPLICATION_ID_MAX 128u

/**
 * @brief Maximum Application display-name UTF-8 byte count.
 */
#define MOONLIGHT_PROTOCOL_V1_APPLICATION_DISPLAY_NAME_MAX 256u

/**
 * @brief Exact byte count of an optional Application icon SHA-256 digest.
 */
#define MOONLIGHT_PROTOCOL_V1_APPLICATION_ICON_DIGEST_SIZE 32u

/**
 * @brief Minimum canonical nested Application-record payload size.
 */
#define MOONLIGHT_PROTOCOL_V1_APPLICATION_RECORD_PAYLOAD_MIN \
  (2u * MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + 2u)

/**
 * @brief Maximum canonical nested Application-record payload size.
 */
#define MOONLIGHT_PROTOCOL_V1_APPLICATION_RECORD_PAYLOAD_MAX \
  (3u * MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + \
   MOONLIGHT_PROTOCOL_V1_APPLICATION_ID_MAX + \
   MOONLIGHT_PROTOCOL_V1_APPLICATION_DISPLAY_NAME_MAX + \
   MOONLIGHT_PROTOCOL_V1_APPLICATION_ICON_DIGEST_SIZE)

/**
 * @brief Maximum canonical GET_APP_LIST request payload size.
 */
#define MOONLIGHT_PROTOCOL_V1_GET_APP_LIST_REQUEST_PAYLOAD_MAX \
  (2u * MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + \
   MOONLIGHT_PROTOCOL_V1_APP_LIST_CURSOR_SIZE + 2u)

/**
 * @brief Maximum canonical successful GET_APP_LIST response payload size.
 */
#define MOONLIGHT_PROTOCOL_V1_GET_APP_LIST_RESPONSE_PAYLOAD_MAX \
  (MOONLIGHT_PROTOCOL_V1_APP_LIST_MAX_ENTRIES * \
     (MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + \
      MOONLIGHT_PROTOCOL_V1_APPLICATION_RECORD_PAYLOAD_MAX) + \
   MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + \
   MOONLIGHT_PROTOCOL_V1_APP_LIST_CURSOR_SIZE)

/**
 * @brief Exact byte count of an opaque Host Display ID.
 */
#define MOONLIGHT_PROTOCOL_V1_DISPLAY_ID_SIZE 16u

/**
 * @brief Maximum Host Display label UTF-8 byte count.
 */
#define MOONLIGHT_PROTOCOL_V1_DISPLAY_LABEL_MAX 256u

/**
 * @brief Maximum Host Display count in one catalog snapshot.
 */
#define MOONLIGHT_PROTOCOL_V1_DISPLAY_LIST_MAX_ENTRIES 32u

/**
 * @brief Mask of every defined Host Display flag.
 */
#define MOONLIGHT_PROTOCOL_V1_DISPLAY_FLAG_MASK UINT32_C(0x07)

/**
 * @brief Minimum canonical nested Host Display record payload size.
 */
#define MOONLIGHT_PROTOCOL_V1_DISPLAY_RECORD_PAYLOAD_MIN \
  (9u * MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + \
   MOONLIGHT_PROTOCOL_V1_DISPLAY_ID_SIZE + 1u + 7u * 4u)

/**
 * @brief Maximum canonical nested Host Display record payload size.
 */
#define MOONLIGHT_PROTOCOL_V1_DISPLAY_RECORD_PAYLOAD_MAX \
  (MOONLIGHT_PROTOCOL_V1_DISPLAY_RECORD_PAYLOAD_MIN - 1u + \
   MOONLIGHT_PROTOCOL_V1_DISPLAY_LABEL_MAX)

/**
 * @brief Maximum canonical successful GET_DISPLAY_LIST response payload size.
 */
#define MOONLIGHT_PROTOCOL_V1_GET_DISPLAY_LIST_RESPONSE_PAYLOAD_MAX \
  (MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + 8u + \
   MOONLIGHT_PROTOCOL_V1_DISPLAY_LIST_MAX_ENTRIES * \
     (MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + \
      MOONLIGHT_PROTOCOL_V1_DISPLAY_RECORD_PAYLOAD_MAX))

  /**
   * @brief Defines Host Display metadata flags.
   */
  typedef enum MoonlightProtocolV1DisplayFlag {
    MOONLIGHT_PROTOCOL_V1_DISPLAY_FLAG_PRIMARY = 0x01,  ///< The operating system reports this as the primary display.
    MOONLIGHT_PROTOCOL_V1_DISPLAY_FLAG_HDR_ENABLED = 0x02,  ///< HDR output is currently enabled.
    MOONLIGHT_PROTOCOL_V1_DISPLAY_FLAG_METADATA_KNOWN = 0x04  ///< Geometry and refresh metadata are authoritative.
  } MoonlightProtocolV1DisplayFlag;

  /**
   * @brief Holds one validated CLIENT_HELLO request payload.
   */
  typedef struct MoonlightProtocolV1ClientHelloRequest {
    uint16_t protocol_minor;  ///< Exact protocol minor, currently zero.
    uint64_t capability_bits;  ///< Defined Client feature bits.
    uint8_t software_version[MOONLIGHT_PROTOCOL_V1_SOFTWARE_VERSION_MAX];  ///< Canonical UTF-8 bytes.
    size_t software_version_size;  ///< Number of bytes in `software_version`.
  } MoonlightProtocolV1ClientHelloRequest;

  /**
   * @brief Holds one validated CLIENT_HELLO successful-response payload.
   */
  typedef struct MoonlightProtocolV1ClientHelloResponse {
    uint16_t protocol_minor;  ///< Exact protocol minor, currently zero.
    uint64_t capability_bits;  ///< Listener-frozen Host feature bits.
    uint8_t host_id[MOONLIGHT_PROTOCOL_V1_UUID_SIZE];  ///< Host UUID in canonical wire bytes.
    uint32_t maximum_control_payload;  ///< Listener-frozen Control payload maximum.
    uint32_t maximum_bulk_payload;  ///< Listener-frozen Bulk payload maximum.
  } MoonlightProtocolV1ClientHelloResponse;

  /**
   * @brief Holds one canonical PING request or successful-response payload.
   *
   * A zero `token_size` encodes as an empty payload with field 1 absent.
   */
  typedef struct MoonlightProtocolV1PingPayload {
    uint8_t token[MOONLIGHT_PROTOCOL_V1_PING_TOKEN_MAX];  ///< Opaque echo token bytes.
    size_t token_size;  ///< Token bytes in `[0, 32]`; zero means no field.
  } MoonlightProtocolV1PingPayload;

  /**
   * @brief Holds one validated successful GET_HOST_INFO response payload.
   */
  typedef struct MoonlightProtocolV1HostInfoResponse {
    uint8_t host_id[MOONLIGHT_PROTOCOL_V1_UUID_SIZE];  ///< Authenticated Host UUID in canonical wire bytes.
    uint8_t display_name[MOONLIGHT_PROTOCOL_V1_HOST_DISPLAY_NAME_MAX];  ///< Canonical Host display-name UTF-8 bytes.
    size_t display_name_size;  ///< Number of bytes in `display_name`.
    uint8_t software_version[MOONLIGHT_PROTOCOL_V1_SOFTWARE_VERSION_MAX];  ///< Canonical Host software-version UTF-8 bytes.
    size_t software_version_size;  ///< Number of bytes in `software_version`.
    uint16_t configured_quic_port;  ///< Nonzero configured QUIC UDP port.
    uint64_t capability_bits;  ///< Defined negotiated Host capability bits.
    uint32_t maximum_active_stream_sessions;  ///< Nonzero process-wide active-session limit.
    uint32_t available_stream_session_slots;  ///< Currently available slots, no greater than the maximum.
    uint64_t acl_permission_bits;  ///< Current Actor ACL permission bits.
    uint64_t authorization_generation;  ///< Current nonzero authorization generation.
    MoonlightProtocolV1InstanceVisibility instance_visibility;  ///< Current Principal visibility policy.
  } MoonlightProtocolV1HostInfoResponse;

  /**
   * @brief Holds one validated GET_APP_LIST request payload.
   *
   * A zero `cursor_size` omits field 1. A zero `maximum_entries` omits field
   * 2 and selects `MOONLIGHT_PROTOCOL_V1_APP_LIST_DEFAULT_MAX_ENTRIES`.
   */
  typedef struct MoonlightProtocolV1GetAppListRequest {
    uint8_t cursor[MOONLIGHT_PROTOCOL_V1_APP_LIST_CURSOR_SIZE];  ///< Opaque prior-page cursor bytes.
    size_t cursor_size;  ///< Cursor bytes, exactly zero or 16.
    uint16_t maximum_entries;  ///< Zero for the default, otherwise a value in `[1, 128]`.
  } MoonlightProtocolV1GetAppListRequest;

  /**
   * @brief Holds one validated nested GET_APP_LIST Application record.
   */
  typedef struct MoonlightProtocolV1ApplicationRecord {
    uint8_t application_id[MOONLIGHT_PROTOCOL_V1_APPLICATION_ID_MAX];  ///< Canonical Application ID UTF-8 bytes.
    size_t application_id_size;  ///< Application ID bytes in `[1, 128]`.
    uint8_t display_name[MOONLIGHT_PROTOCOL_V1_APPLICATION_DISPLAY_NAME_MAX];  ///< Canonical display-name UTF-8 bytes.
    size_t display_name_size;  ///< Display-name bytes in `[1, 256]`.
    uint8_t icon_asset_sha256[MOONLIGHT_PROTOCOL_V1_APPLICATION_ICON_DIGEST_SIZE];  ///< Optional icon digest bytes.
    size_t icon_asset_sha256_size;  ///< Icon digest bytes, exactly zero or 32.
  } MoonlightProtocolV1ApplicationRecord;

  /**
   * @brief Holds one validated successful GET_APP_LIST response payload.
   *
   * The fixed arrays are wholly caller-owned. No codec call allocates,
   * retains, or borrows storage from this structure.
   */
  typedef struct MoonlightProtocolV1GetAppListResponse {
    MoonlightProtocolV1ApplicationRecord entries[MOONLIGHT_PROTOCOL_V1_APP_LIST_MAX_ENTRIES];  ///< Strictly ID-sorted records.
    size_t entry_count;  ///< Number of records in `[0, 128]`.
    uint8_t next_cursor[MOONLIGHT_PROTOCOL_V1_APP_LIST_CURSOR_SIZE];  ///< Opaque next-page cursor bytes.
    size_t next_cursor_size;  ///< Cursor bytes, exactly zero or 16.
  } MoonlightProtocolV1GetAppListResponse;

  /**
   * @brief Holds one validated Host Display catalog record.
   *
   * `display_id` is the sole value a Client returns when selecting a display.
   * `label` is presentation-only and MUST NOT be used as a capture selector.
   */
  typedef struct MoonlightProtocolV1DisplayRecord {
    uint8_t display_id[MOONLIGHT_PROTOCOL_V1_DISPLAY_ID_SIZE];  ///< Opaque Host-scoped stable Display ID.
    uint8_t label[MOONLIGHT_PROTOCOL_V1_DISPLAY_LABEL_MAX];  ///< Canonical human-readable UTF-8 label.
    size_t label_size;  ///< Label bytes in `[1, 256]`.
    uint32_t width;  ///< Current physical width in pixels, or zero when metadata is unknown.
    uint32_t height;  ///< Current physical height in pixels, or zero when metadata is unknown.
    uint32_t refresh_rate_numerator;  ///< Current refresh numerator, or zero when metadata is unknown.
    uint32_t refresh_rate_denominator;  ///< Current refresh denominator, or zero when metadata is unknown.
    int32_t origin_x;  ///< Virtual-desktop horizontal origin, or zero when metadata is unknown.
    int32_t origin_y;  ///< Virtual-desktop vertical origin, or zero when metadata is unknown.
    uint32_t flags;  ///< Defined `MoonlightProtocolV1DisplayFlag` bits only.
  } MoonlightProtocolV1DisplayRecord;

  /**
   * @brief Holds one validated successful GET_DISPLAY_LIST response payload.
   *
   * Records are strictly ordered by raw `display_id` bytes. The revision is an
   * opaque nonzero fingerprint of the complete snapshot and is revalidated
   * before a selected display is used to create a Stream Session.
   */
  typedef struct MoonlightProtocolV1GetDisplayListResponse {
    uint64_t catalog_revision;  ///< Nonzero opaque snapshot revision.
    MoonlightProtocolV1DisplayRecord entries[MOONLIGHT_PROTOCOL_V1_DISPLAY_LIST_MAX_ENTRIES];  ///< Strictly ID-sorted records.
    size_t entry_count;  ///< Number of records in `[0, 32]`.
  } MoonlightProtocolV1GetDisplayListResponse;

  /**
   * @brief Encodes one canonical CLIENT_HELLO request payload.
   *
   * The output and `encoded_size` are unchanged when validation fails.
   *
   * @param request Validated host-order request values.
   * @param output Destination buffer.
   * @param output_size Available bytes in `output`.
   * @param encoded_size Receives the encoded payload size.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1EncodeClientHelloRequest(
    const MoonlightProtocolV1ClientHelloRequest *request,
    uint8_t *output,
    size_t output_size,
    size_t *encoded_size
  );

  /**
   * @brief Decodes one complete canonical CLIENT_HELLO request payload.
   *
   * @param input Complete payload bytes.
   * @param input_size Number of bytes in `input`.
   * @param request Receives validated host-order values only on success.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1DecodeClientHelloRequest(
    const uint8_t *input,
    size_t input_size,
    MoonlightProtocolV1ClientHelloRequest *request
  );

  /**
   * @brief Encodes one canonical successful CLIENT_HELLO response payload.
   *
   * The output and `encoded_size` are unchanged when validation fails.
   *
   * @param response Validated host-order response values.
   * @param output Destination buffer.
   * @param output_size Available bytes in `output`.
   * @param encoded_size Receives the encoded payload size.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1EncodeClientHelloResponse(
    const MoonlightProtocolV1ClientHelloResponse *response,
    uint8_t *output,
    size_t output_size,
    size_t *encoded_size
  );

  /**
   * @brief Decodes one complete canonical successful CLIENT_HELLO response payload.
   *
   * @param input Complete payload bytes.
   * @param input_size Number of bytes in `input`.
   * @param response Receives validated host-order values only on success.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1DecodeClientHelloResponse(
    const uint8_t *input,
    size_t input_size,
    MoonlightProtocolV1ClientHelloResponse *response
  );

  /**
   * @brief Encodes one canonical PING request or successful-response payload.
   *
   * Field 1 is absent when `payload->token_size` is zero. Otherwise the sole
   * field has flags zero and an opaque value of 1 through 32 bytes. The output
   * and `encoded_size` are unchanged when validation fails.
   *
   * @param payload Validated token value.
   * @param output Destination buffer, or null only for an empty payload.
   * @param output_size Available bytes in `output`.
   * @param encoded_size Receives zero or the complete encoded payload size.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1EncodePingPayload(
    const MoonlightProtocolV1PingPayload *payload,
    uint8_t *output,
    size_t output_size,
    size_t *encoded_size
  );

  /**
   * @brief Decodes one complete canonical PING request or response payload.
   *
   * Empty input decodes to a zero-length token. A present payload contains
   * exactly scalar field 1 with flags zero and 1 through 32 opaque bytes.
   *
   * @param input Complete payload bytes, or null only when `input_size` is zero.
   * @param input_size Number of bytes in `input`.
   * @param payload Receives the validated token only on success.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1DecodePingPayload(
    const uint8_t *input,
    size_t input_size,
    MoonlightProtocolV1PingPayload *payload
  );

  /**
   * @brief Encodes one canonical successful GET_HOST_INFO response payload.
   *
   * All ten required scalar fields are emitted in ascending field order with
   * flags zero. The output and `encoded_size` are unchanged when validation
   * fails.
   *
   * @param response Validated host-order response values.
   * @param output Destination buffer.
   * @param output_size Available bytes in `output`.
   * @param encoded_size Receives the encoded payload size.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1EncodeHostInfoResponse(
    const MoonlightProtocolV1HostInfoResponse *response,
    uint8_t *output,
    size_t output_size,
    size_t *encoded_size
  );

  /**
   * @brief Decodes one complete canonical successful GET_HOST_INFO response payload.
   *
   * The decoder requires fields 1 through 10 exactly once in ascending order,
   * with scalar flags zero, exact integer widths, canonical bounded UTF-8,
   * defined capability and ACL bits, bounded session counts, a nonzero port
   * and authorization generation, and a defined visibility policy.
   * Structural incompleteness and repeated known fields are malformed;
   * unknown fields or flags are unsupported.
   *
   * @param input Complete payload bytes.
   * @param input_size Number of bytes in `input`.
   * @param response Receives validated host-order values only on success.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1DecodeHostInfoResponse(
    const uint8_t *input,
    size_t input_size,
    MoonlightProtocolV1HostInfoResponse *response
  );

  /**
   * @brief Encodes one canonical GET_APP_LIST request payload.
   *
   * Optional field 1 is emitted before optional field 2. A zero
   * `maximum_entries` preserves the canonical absent-field form rather than
   * emitting the default value. The output and `encoded_size` are unchanged
   * when validation fails.
   *
   * @param request Validated host-order request values.
   * @param output Destination buffer, or null only for an empty request.
   * @param output_size Available bytes in `output`.
   * @param encoded_size Receives the encoded payload size.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1EncodeGetAppListRequest(
    const MoonlightProtocolV1GetAppListRequest *request,
    uint8_t *output,
    size_t output_size,
    size_t *encoded_size
  );

  /**
   * @brief Decodes one complete canonical GET_APP_LIST request payload.
   *
   * Empty input is valid. Known fields must be unique, ordered, carry flags
   * zero, and use their exact widths. The output is replaced only on success.
   *
   * @param input Complete payload bytes, or null only when `input_size` is zero.
   * @param input_size Number of bytes in `input`.
   * @param request Receives validated host-order values only on success.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1DecodeGetAppListRequest(
    const uint8_t *input,
    size_t input_size,
    MoonlightProtocolV1GetAppListRequest *request
  );

  /**
   * @brief Encodes one canonical successful GET_APP_LIST response payload.
   *
   * Application IDs must already be in strict lexicographic raw-byte order.
   * Outer record fields use `REPEATED`; all nested fields and the optional
   * next cursor use flags zero. The output and `encoded_size` are unchanged
   * when validation fails.
   *
   * @param response Validated caller-owned response values.
   * @param output Destination buffer, or null only for an empty response.
   * @param output_size Available bytes in `output`.
   * @param encoded_size Receives the encoded payload size.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1EncodeGetAppListResponse(
    const MoonlightProtocolV1GetAppListResponse *response,
    uint8_t *output,
    size_t output_size,
    size_t *encoded_size
  );

  /**
   * @brief Decodes one complete canonical successful GET_APP_LIST response.
   *
   * The decoder owns no memory: it copies at most 128 bounded records and one
   * cursor into `response`. It rejects noncanonical nesting, flags, UTF-8,
   * lengths, record counts, duplicate IDs, and nonascending raw-byte ID order.
   * The output is replaced only on success.
   *
   * @param input Complete payload bytes, or null only when `input_size` is zero.
   * @param input_size Number of bytes in `input`.
   * @param response Receives validated caller-owned values only on success.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1DecodeGetAppListResponse(
    const uint8_t *input,
    size_t input_size,
    MoonlightProtocolV1GetAppListResponse *response
  );

  /**
   * @brief Encodes one canonical successful GET_DISPLAY_LIST response.
   *
   * Field 1 contains the required nonzero revision. Repeated field 2 contains
   * records in strict raw Display-ID order. Every nested scalar is required,
   * uses flags zero, and owns its bytes. The output and `encoded_size` are
   * unchanged when validation fails.
   *
   * @param response Validated caller-owned response values.
   * @param output Destination buffer.
   * @param output_size Available bytes in `output`.
   * @param encoded_size Receives the encoded payload size.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1EncodeGetDisplayListResponse(
    const MoonlightProtocolV1GetDisplayListResponse *response,
    uint8_t *output,
    size_t output_size,
    size_t *encoded_size
  );

  /**
   * @brief Decodes one complete canonical successful GET_DISPLAY_LIST response.
   *
   * The decoder copies at most 32 bounded records and rejects zero revisions,
   * invalid UTF-8, undefined metadata combinations, duplicate or unordered
   * Display IDs, noncanonical nesting, unknown fields, and unsupported flags.
   * The output is replaced only on success.
   *
   * @param input Complete payload bytes.
   * @param input_size Number of bytes in `input`.
   * @param response Receives validated caller-owned values only on success.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1DecodeGetDisplayListResponse(
    const uint8_t *input,
    size_t input_size,
    MoonlightProtocolV1GetDisplayListResponse *response
  );

#ifdef __cplusplus
}
#endif
