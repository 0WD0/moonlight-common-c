/**
 * @file wire.h
 * @brief Defines backend-independent Sunshine protocol version 1 reliable framing.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Size of a canonical protocol version 1 lane preface.
 */
#define MOONLIGHT_PROTOCOL_V1_LANE_PREFACE_SIZE 16u

/**
 * @brief Size of a canonical protocol version 1 reliable-message envelope.
 */
#define MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE 24u

/**
 * @brief Size of a canonical protocol version 1 TLV field header.
 */
#define MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE 8u

/**
 * @brief Maximum Pair Control message payload.
 */
#define MOONLIGHT_PROTOCOL_V1_PAIR_CONTROL_PAYLOAD_MAX 12288u

/**
 * @brief Maximum Control message payload.
 */
#define MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX 65536u

/**
 * @brief Maximum Reliable Input message payload.
 */
#define MOONLIGHT_PROTOCOL_V1_RELIABLE_INPUT_PAYLOAD_MAX 4126u

/**
 * @brief Maximum Bulk message payload.
 */
#define MOONLIGHT_PROTOCOL_V1_BULK_PAYLOAD_MAX 16777216u

/**
 * @brief Maximum top-level TLV field occurrences in one message.
 */
#define MOONLIGHT_PROTOCOL_V1_TLV_TOP_LEVEL_FIELD_MAX 129u

/**
 * @brief Maximum field occurrences in one nested TLV record.
 */
#define MOONLIGHT_PROTOCOL_V1_TLV_NESTED_FIELD_MAX 64u

/**
 * @brief Maximum TLV record depth below the top-level message payload.
 */
#define MOONLIGHT_PROTOCOL_V1_TLV_DEPTH_MAX 2u

  /**
   * @brief Describes the result of a Sunshine protocol version 1 codec operation.
   */
  typedef enum MoonlightProtocolResult {
    MOONLIGHT_PROTOCOL_RESULT_OK = 0,  ///< The complete value was encoded or decoded.
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT,  ///< A pointer, size, or caller-supplied limit is invalid.
    MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL,  ///< The output buffer cannot hold the complete canonical value.
    MOONLIGHT_PROTOCOL_RESULT_INSUFFICIENT_SHARDS,  ///< Too few valid FEC rows remain to recover missing data.
    MOONLIGHT_PROTOCOL_RESULT_INTERNAL_ERROR,  ///< A local invariant failed despite structurally valid inputs.
    MOONLIGHT_PROTOCOL_RESULT_TRUNCATED,  ///< The input ends before the complete declared value.
    MOONLIGHT_PROTOCOL_RESULT_TRAILING_DATA,  ///< Bytes remain after the one complete value.
    MOONLIGHT_PROTOCOL_RESULT_STALE_CONTEXT,  ///< A delayed value belongs to another valid session or epoch.
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED,  ///< A known value violates a structural version 1 invariant.
    MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH,  ///< A known value is invalid for the supplied direction or channel role.
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED,  ///< The value uses an unknown version 1 registry entry or flag.
    MOONLIGHT_PROTOCOL_RESULT_LIMIT_EXCEEDED  ///< A peer-declared value exceeds an explicit receive limit.
  } MoonlightProtocolResult;

  /**
   * @brief Identifies the direction of an application payload.
   */
  typedef enum MoonlightProtocolDirection {
    MOONLIGHT_PROTOCOL_DIRECTION_CLIENT_TO_HOST = 1,  ///< The payload travels from Artemis to Sunshine.
    MOONLIGHT_PROTOCOL_DIRECTION_HOST_TO_CLIENT = 2  ///< The payload travels from Sunshine to Artemis.
  } MoonlightProtocolDirection;

  /**
   * @brief Identifies a reliable QUIC stream lane.
   */
  typedef enum MoonlightProtocolV1LaneKind {
    MOONLIGHT_PROTOCOL_V1_LANE_PAIR_CONTROL = 0x01,  ///< Pairing ALPN bidirectional stream zero.
    MOONLIGHT_PROTOCOL_V1_LANE_CONTROL = 0x10,  ///< Streaming ALPN bidirectional stream zero.
    MOONLIGHT_PROTOCOL_V1_LANE_RELIABLE_INPUT = 0x11,  ///< Stream-Session reliable-input lane.
    MOONLIGHT_PROTOCOL_V1_LANE_BULK = 0x20  ///< Per-transfer bounded asset lane.
  } MoonlightProtocolV1LaneKind;

  /**
   * @brief Defines reliable-message envelope flag bits.
   */
  typedef enum MoonlightProtocolV1EnvelopeFlag {
    MOONLIGHT_PROTOCOL_V1_ENVELOPE_FLAG_RESPONSE = 0x0001,  ///< The envelope answers one request.
    MOONLIGHT_PROTOCOL_V1_ENVELOPE_FLAG_ERROR = 0x0002,  ///< The response carries a nonzero status.
    MOONLIGHT_PROTOCOL_V1_ENVELOPE_FLAG_NOTIFICATION = 0x0004  ///< No response or correlation identifier is used.
  } MoonlightProtocolV1EnvelopeFlag;

  /**
   * @brief Defines the complete protocol version 1 message status registry.
   */
  typedef enum MoonlightProtocolV1MessageStatus {
    MOONLIGHT_PROTOCOL_V1_STATUS_OK = 0,  ///< Successful request or response.
    MOONLIGHT_PROTOCOL_V1_STATUS_INVALID_MESSAGE = 1,  ///< A complete request failed field validation.
    MOONLIGHT_PROTOCOL_V1_STATUS_UNSUPPORTED_MESSAGE = 2,  ///< A message, field, or required feature is unknown.
    MOONLIGHT_PROTOCOL_V1_STATUS_INVALID_STATE = 3,  ///< The operation is invalid in the current state.
    MOONLIGHT_PROTOCOL_V1_STATUS_UNAUTHENTICATED = 4,  ///< No enabled Principal authorizes the request.
    MOONLIGHT_PROTOCOL_V1_STATUS_PERMISSION_DENIED = 5,  ///< The Principal ACL lacks permission.
    MOONLIGHT_PROTOCOL_V1_STATUS_NOT_FOUND = 6,  ///< The requested object does not exist.
    MOONLIGHT_PROTOCOL_V1_STATUS_CONFLICT = 7,  ///< Current application or session state conflicts.
    MOONLIGHT_PROTOCOL_V1_STATUS_RESOURCE_EXHAUSTED = 8,  ///< A bounded Host resource is unavailable.
    MOONLIGHT_PROTOCOL_V1_STATUS_RATE_LIMITED = 9,  ///< A pairing or request rate limit was exceeded.
    MOONLIGHT_PROTOCOL_V1_STATUS_UNSUPPORTED_CONFIG = 10,  ///< A media or input configuration is unsupported.
    MOONLIGHT_PROTOCOL_V1_STATUS_STALE_EPOCH = 11,  ///< The request names an inactive Media Epoch.
    MOONLIGHT_PROTOCOL_V1_STATUS_INTERNAL = 12,  ///< The Host failed without a safe public detail.
    MOONLIGHT_PROTOCOL_V1_STATUS_SHUTTING_DOWN = 13  ///< The Host or connection is draining.
  } MoonlightProtocolV1MessageStatus;

  /**
   * @brief Defines the exact protocol version 1 reliable-message registry.
   */
  typedef enum MoonlightProtocolV1MessageType {
    MOONLIGHT_PROTOCOL_V1_MESSAGE_CLIENT_HELLO = 0x0001,  ///< Streaming admission request and response.
    MOONLIGHT_PROTOCOL_V1_MESSAGE_PING = 0x0002,  ///< Streaming Control echo request and response.
    MOONLIGHT_PROTOCOL_V1_MESSAGE_GOAWAY = 0x0003,  ///< Streaming Control drain notification.
    MOONLIGHT_PROTOCOL_V1_MESSAGE_CLIENT_PROOF = 0x0004,  ///< Pairing or Streaming client proof.
    MOONLIGHT_PROTOCOL_V1_MESSAGE_PAIR_REQUEST = 0x0100,  ///< Pairing transaction request and response.
    MOONLIGHT_PROTOCOL_V1_MESSAGE_GET_HOST_INFO = 0x0200,  ///< Host information request and response.
    MOONLIGHT_PROTOCOL_V1_MESSAGE_GET_APP_LIST = 0x0201,  ///< Visible application catalog request and response.
    MOONLIGHT_PROTOCOL_V1_MESSAGE_LIST_APPLICATION_INSTANCES = 0x0202,  ///< Visible live-instance request and response.
    MOONLIGHT_PROTOCOL_V1_MESSAGE_START_APPLICATION = 0x0300,  ///< Application start request and response.
    MOONLIGHT_PROTOCOL_V1_MESSAGE_STOP_APPLICATION = 0x0301,  ///< Application stop request and response.
    MOONLIGHT_PROTOCOL_V1_MESSAGE_APPLICATION_STATE = 0x0302,  ///< Application lifecycle notification.
    MOONLIGHT_PROTOCOL_V1_MESSAGE_ATTACH_APPLICATION = 0x0310,  ///< Attachment creation request and response.
    MOONLIGHT_PROTOCOL_V1_MESSAGE_DETACH_APPLICATION = 0x0311,  ///< Attachment removal request and response.
    MOONLIGHT_PROTOCOL_V1_MESSAGE_START_SESSION = 0x0320,  ///< Stream Session start request and response.
    MOONLIGHT_PROTOCOL_V1_MESSAGE_SESSION_READY = 0x0321,  ///< Stream Session readiness request and response.
    MOONLIGHT_PROTOCOL_V1_MESSAGE_STOP_SESSION = 0x0322,  ///< Stream Session stop request and response.
    MOONLIGHT_PROTOCOL_V1_MESSAGE_SESSION_STATE = 0x0323,  ///< Stream Session lifecycle notification.
    MOONLIGHT_PROTOCOL_V1_MESSAGE_MEDIA_EPOCH_PREPARE = 0x0330,  ///< Video Media Epoch proposal notification.
    MOONLIGHT_PROTOCOL_V1_MESSAGE_MEDIA_EPOCH_READY = 0x0331,  ///< Video Media Epoch readiness request and response.
    MOONLIGHT_PROTOCOL_V1_MESSAGE_REQUEST_IDR = 0x0332,  ///< Key-frame request and response.
    MOONLIGHT_PROTOCOL_V1_MESSAGE_VIDEO_TARGET = 0x0333,  ///< Encoder target notification.
    MOONLIGHT_PROTOCOL_V1_MESSAGE_INPUT_RELIABLE = 0x0400,  ///< Reliable input notification.
    MOONLIGHT_PROTOCOL_V1_MESSAGE_GET_ASSET = 0x0500  ///< Bulk asset request and response.
  } MoonlightProtocolV1MessageType;

  /**
   * @brief Defines TLV field flag bits.
   */
  typedef enum MoonlightProtocolV1TlvFlag {
    MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED = 0x0001  ///< The schema declares this field repeated.
  } MoonlightProtocolV1TlvFlag;

  /**
   * @brief Identifies one event emitted by the incremental reliable-stream parser.
   */
  typedef enum MoonlightProtocolV1StreamEventType {
    MOONLIGHT_PROTOCOL_V1_STREAM_EVENT_NONE = 0,  ///< More input is needed or the parser is at a boundary.
    MOONLIGHT_PROTOCOL_V1_STREAM_EVENT_LANE_PREFACE,  ///< One complete lane preface was accepted.
    MOONLIGHT_PROTOCOL_V1_STREAM_EVENT_ENVELOPE_BEGIN,  ///< One complete envelope header was accepted.
    MOONLIGHT_PROTOCOL_V1_STREAM_EVENT_PAYLOAD,  ///< A borrowed envelope-payload chunk is available.
    MOONLIGHT_PROTOCOL_V1_STREAM_EVENT_ENVELOPE_END  ///< The current envelope payload is complete.
  } MoonlightProtocolV1StreamEventType;

  /**
   * @brief Identifies one event emitted by the incremental TLV parser.
   */
  typedef enum MoonlightProtocolV1TlvEventType {
    MOONLIGHT_PROTOCOL_V1_TLV_EVENT_NONE = 0,  ///< More input is needed or the containing payload is complete.
    MOONLIGHT_PROTOCOL_V1_TLV_EVENT_FIELD_BEGIN,  ///< One complete field header was accepted.
    MOONLIGHT_PROTOCOL_V1_TLV_EVENT_VALUE,  ///< A borrowed field-value chunk is available.
    MOONLIGHT_PROTOCOL_V1_TLV_EVENT_FIELD_END  ///< The current field value is complete.
  } MoonlightProtocolV1TlvEventType;

  /**
   * @brief Holds one canonical reliable-stream lane preface in host byte order.
   */
  typedef struct MoonlightProtocolV1LanePreface {
    MoonlightProtocolV1LaneKind kind;  ///< Known lane kind.
    uint32_t session_wire_id;  ///< Zero for connection lanes or the active Stream Session ID.
  } MoonlightProtocolV1LanePreface;

  /**
   * @brief Holds one canonical reliable-message envelope in host byte order.
   */
  typedef struct MoonlightProtocolV1MessageEnvelope {
    MoonlightProtocolV1MessageType message_type;  ///< Exact version 1 registry type.
    uint16_t flags;  ///< Canonical request, response, error, or notification flags.
    uint32_t payload_length;  ///< Bytes immediately following the envelope.
    MoonlightProtocolV1MessageStatus status;  ///< Zero except for an error response.
    uint64_t correlation_id;  ///< Nonzero request/response ID or zero for a notification.
  } MoonlightProtocolV1MessageEnvelope;

  /**
   * @brief Holds one canonical TLV field header in host byte order.
   */
  typedef struct MoonlightProtocolV1TlvField {
    uint16_t field_id;  ///< Message-form and nesting-path-local field number.
    uint16_t flags;  ///< Zero or `MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED`.
    uint32_t field_length;  ///< Number of value bytes following the field header.
  } MoonlightProtocolV1TlvField;

  /**
   * @brief Holds one event from `MoonlightProtocolV1StreamParserFeed`.
   */
  typedef struct MoonlightProtocolV1StreamEvent {
    MoonlightProtocolV1StreamEventType type;  ///< Event kind.
    MoonlightProtocolV1LanePreface lane;  ///< Accepted lane for a preface event.
    MoonlightProtocolV1MessageEnvelope envelope;  ///< Current envelope for envelope and payload events.
    MoonlightProtocolResult message_result;  ///< OK, or UNSUPPORTED while draining a bounded unknown request.
    const uint8_t *payload;  ///< Borrowed payload bytes for a payload event.
    size_t payload_size;  ///< Number of borrowed bytes in `payload`.
  } MoonlightProtocolV1StreamEvent;

  /**
   * @brief Caller-owned state for one allocation-free reliable-stream parser.
   *
   * Initialize this structure with `MoonlightProtocolV1StreamParserInitialize`
   * and do not modify its fields directly while parsing. Payload limits may be
   * changed only through
   * `MoonlightProtocolV1StreamParserSetPayloadLimitAtBoundary`. This parser
   * enforces fixed framing, payload ceilings, and message-form direction on an
   * already selected lane. The caller still enforces ALPN and QUIC stream
   * identity, lane cardinality, proof and admission ordering, correlation
   * replay rules, message schemas, authorization, and connection state.
   */
  typedef struct MoonlightProtocolV1StreamParser {
    uint8_t header_bytes[MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE];  ///< Partial fixed header.
    size_t header_size;  ///< Bytes currently retained in `header_bytes`.
    uint32_t payload_remaining;  ///< Bytes not yet emitted for the active envelope.
    uint32_t payload_limit;  ///< Explicit per-envelope receive limit.
    MoonlightProtocolV1LanePreface lane;  ///< Accepted lane preface.
    MoonlightProtocolV1MessageEnvelope envelope;  ///< Active envelope.
    MoonlightProtocolResult message_result;  ///< Active message-level validation result.
    MoonlightProtocolResult terminal_result;  ///< Sticky parse failure, or OK.
    uint8_t state;  ///< Internal parser phase.
    bool reverse;  ///< Whether this is the acceptor-to-initiator stream half.
  } MoonlightProtocolV1StreamParser;

  /**
   * @brief Holds one event from `MoonlightProtocolV1TlvParserFeed`.
   */
  typedef struct MoonlightProtocolV1TlvEvent {
    MoonlightProtocolV1TlvEventType type;  ///< Event kind.
    MoonlightProtocolV1TlvField field;  ///< Current field for field and value events.
    const uint8_t *value;  ///< Borrowed field-value bytes for a value event.
    size_t value_size;  ///< Number of borrowed bytes in `value`.
  } MoonlightProtocolV1TlvEvent;

  /**
   * @brief Caller-owned state for one allocation-free TLV payload parser.
   *
   * Initialize this structure with `MoonlightProtocolV1TlvParserInitialize`
   * and do not modify its fields while parsing. This structural parser does
   * not know a message schema: the caller still validates whether a field ID,
   * repeated declaration, empty value, and value representation are legal for
   * the selected message form. A structural TLV failure is message-level; the
   * surrounding stream parser can continue draining the declared envelope.
   */
  typedef struct MoonlightProtocolV1TlvParser {
    uint8_t header_bytes[MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE];  ///< Partial TLV header.
    size_t header_size;  ///< Bytes currently retained in `header_bytes`.
    uint32_t container_remaining;  ///< Unconsumed bytes in the containing payload.
    uint32_t value_remaining;  ///< Unemitted bytes in the active field value.
    size_t field_count;  ///< Accepted field occurrences.
    MoonlightProtocolV1TlvField field;  ///< Active field.
    MoonlightProtocolResult terminal_result;  ///< Sticky parse failure, or OK.
    uint16_t last_field_id;  ///< Most recently accepted field ID.
    uint16_t last_field_flags;  ///< Flags of the most recently accepted field.
    uint8_t depth;  ///< Top-level zero or nested depth in `[1, 2]`.
    uint8_t state;  ///< Internal parser phase.
    bool has_last_field;  ///< Whether ordering has a previous field.
  } MoonlightProtocolV1TlvParser;

  /**
   * @brief Caller-owned state for one canonical TLV payload writer.
   *
   * Initialize this structure with `MoonlightProtocolV1TlvWriterInitialize`
   * and do not modify its fields while writing.
   */
  typedef struct MoonlightProtocolV1TlvWriter {
    uint8_t *output;  ///< Caller-owned complete containing-payload buffer.
    size_t output_capacity;  ///< Writable bytes in `output`.
    size_t output_size;  ///< Bytes committed by successful appends.
    size_t field_count;  ///< Committed field occurrences.
    uint16_t last_field_id;  ///< Most recently committed field ID.
    uint16_t last_field_flags;  ///< Flags of the most recently committed field.
    uint8_t depth;  ///< Top-level zero or nested depth in `[1, 2]`.
    bool has_last_field;  ///< Whether ordering has a previous field.
  } MoonlightProtocolV1TlvWriter;

  /**
   * @brief Tests whether a message type is in the exact version 1 registry.
   *
   * @param message_type Candidate host-order message type.
   * @return True only for a known protocol version 1 message type.
   */
  bool MoonlightProtocolV1MessageTypeIsKnown(uint16_t message_type);

  /**
   * @brief Encodes one canonical lane preface.
   *
   * The output is not modified when validation fails.
   *
   * @param preface Host-order lane values.
   * @param output Destination buffer.
   * @param output_size Available bytes in `output`.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1EncodeLanePreface(
    const MoonlightProtocolV1LanePreface *preface,
    uint8_t *output,
    size_t output_size
  );

  /**
   * @brief Decodes one canonical lane preface.
   *
   * @param input Bytes beginning with a complete lane preface.
   * @param input_size Available bytes in `input`.
   * @param preface Receives host-order values only on success.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1DecodeLanePreface(
    const uint8_t *input,
    size_t input_size,
    MoonlightProtocolV1LanePreface *preface
  );

  /**
   * @brief Encodes one canonical reliable-message envelope.
   *
   * The output is not modified when validation fails.
   *
   * @param envelope Host-order envelope values.
   * @param payload_limit Explicit legal payload maximum for the active lane and state.
   * @param output Destination buffer.
   * @param output_size Available bytes in `output`.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1EncodeMessageEnvelope(
    const MoonlightProtocolV1MessageEnvelope *envelope,
    uint32_t payload_limit,
    uint8_t *output,
    size_t output_size
  );

  /**
   * @brief Decodes one canonical reliable-message envelope.
   *
   * @param input Bytes beginning with a complete envelope.
   * @param input_size Available bytes in `input`.
   * @param payload_limit Explicit legal payload maximum for the active lane and state.
   * @param envelope Receives host-order values only on success.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1DecodeMessageEnvelope(
    const uint8_t *input,
    size_t input_size,
    uint32_t payload_limit,
    MoonlightProtocolV1MessageEnvelope *envelope
  );

  /**
   * @brief Encodes one canonical TLV field header.
   *
   * @param field Host-order field values.
   * @param output Destination buffer.
   * @param output_size Available bytes in `output`.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1EncodeTlvFieldHeader(
    const MoonlightProtocolV1TlvField *field,
    uint8_t *output,
    size_t output_size
  );

  /**
   * @brief Decodes one canonical TLV field header.
   *
   * @param input Bytes beginning with a complete field header.
   * @param input_size Available bytes in `input`.
   * @param field Receives host-order values only on success.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1DecodeTlvFieldHeader(
    const uint8_t *input,
    size_t input_size,
    MoonlightProtocolV1TlvField *field
  );

  /**
   * @brief Initializes one incremental reliable-stream parser.
   *
   * @param parser Parser state to initialize.
   * @param payload_limit Explicit maximum payload for every accepted envelope.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1StreamParserInitialize(
    MoonlightProtocolV1StreamParser *parser,
    uint32_t payload_limit
  );

  /**
   * @brief Initializes a parser for the preface-free acceptor-to-initiator half.
   *
   * A bidirectional stream carries its lane preface only in the initiating
   * direction. The reverse direction begins with an envelope and uses the
   * already accepted lane context supplied here. A Reliable Input lane is
   * unidirectional and is rejected.
   *
   * @param parser Parser state to initialize.
   * @param lane Lane accepted from the initiating direction's preface.
   * @param payload_limit Explicit maximum payload for every accepted envelope.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1StreamParserInitializeReverse(
    MoonlightProtocolV1StreamParser *parser,
    const MoonlightProtocolV1LanePreface *lane,
    uint32_t payload_limit
  );

  /**
   * @brief Changes the receive limit at a complete envelope boundary.
   *
   * The parser must be healthy and either ready for the next envelope or
   * holding a complete envelope whose end event is pending. It must not retain
   * a partial header or payload. The immutable lane ceiling remains in force
   * when the next envelope is decoded. The parser is unchanged on failure.
   *
   * @param parser Initialized parser at a complete envelope boundary.
   * @param payload_limit New explicit per-envelope payload maximum.
   * @return OK on success, INVALID_ARGUMENT for a null parser or an absolute
   * limit violation, and CONTEXT_MISMATCH outside a clean boundary.
   */
  MoonlightProtocolResult MoonlightProtocolV1StreamParserSetPayloadLimitAtBoundary(
    MoonlightProtocolV1StreamParser *parser,
    uint32_t payload_limit
  );

  /**
   * @brief Consumes stream bytes until one parser event is available.
   *
   * Payload events borrow a range from `input`; the range remains valid only
   * as long as the caller keeps those input bytes alive. An envelope begin or
   * end event may consume zero bytes, so callers continue while an event is
   * returned even when `consumed` is zero. A structurally valid request with an
   * unknown message type is fully drained with `event.message_result` set to
   * `MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED`, after which the caller can send
   * `UNSUPPORTED_MESSAGE` if its upper-layer state permits a response. This
   * disposition never authorizes a response, side effect, or continued lane
   * use by itself. An unknown response or notification, malformed framing, or
   * wrong-lane form is a sticky failure.
   *
   * @param parser Initialized parser state.
   * @param input Next stream bytes, or `NULL` when `input_size` is zero.
   * @param input_size Number of readable bytes in `input`.
   * @param consumed Receives the number of bytes consumed by this call.
   * @param event Receives one event or `STREAM_EVENT_NONE`.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1StreamParserFeed(
    MoonlightProtocolV1StreamParser *parser,
    const uint8_t *input,
    size_t input_size,
    size_t *consumed,
    MoonlightProtocolV1StreamEvent *event
  );

  /**
   * @brief Validates that a reliable stream ended at a complete message boundary.
   *
   * @param parser Initialized parser state.
   * @return OK at a complete post-preface boundary, TRUNCATED otherwise, or
   * the parser's sticky failure.
   */
  MoonlightProtocolResult MoonlightProtocolV1StreamParserFinish(
    const MoonlightProtocolV1StreamParser *parser
  );

  /**
   * @brief Initializes one incremental parser for a complete TLV container.
   *
   * @param parser Parser state to initialize.
   * @param payload_length Exact byte length of the containing TLV payload.
   * @param depth Zero for a message payload or `[1, 2]` for a nested record.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1TlvParserInitialize(
    MoonlightProtocolV1TlvParser *parser,
    uint32_t payload_length,
    uint8_t depth
  );

  /**
   * @brief Consumes TLV bytes until one field event is available.
   *
   * Value events borrow a range from `input`. A field begin or end event may
   * consume zero bytes, so callers continue while an event is returned even
   * when `consumed` is zero. A parse failure is sticky.
   *
   * @param parser Initialized parser state.
   * @param input Next containing-payload bytes, or `NULL` when `input_size` is zero.
   * @param input_size Number of readable bytes in `input`.
   * @param consumed Receives the number of bytes consumed by this call.
   * @param event Receives one event or `TLV_EVENT_NONE`.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1TlvParserFeed(
    MoonlightProtocolV1TlvParser *parser,
    const uint8_t *input,
    size_t input_size,
    size_t *consumed,
    MoonlightProtocolV1TlvEvent *event
  );

  /**
   * @brief Tests whether the complete declared TLV container was consumed.
   *
   * @param parser Initialized parser state.
   * @return True only at a complete field boundary with no sticky failure.
   */
  bool MoonlightProtocolV1TlvParserIsComplete(
    const MoonlightProtocolV1TlvParser *parser
  );

  /**
   * @brief Validates that a TLV container ended at a complete field boundary.
   *
   * @param parser Initialized parser state.
   * @return OK at a complete boundary, TRUNCATED otherwise, or the parser's
   * sticky failure.
   */
  MoonlightProtocolResult MoonlightProtocolV1TlvParserFinish(
    const MoonlightProtocolV1TlvParser *parser
  );

  /**
   * @brief Initializes one canonical TLV payload writer.
   *
   * @param writer Writer state to initialize.
   * @param output Caller-owned containing-payload buffer.
   * @param output_capacity Writable bytes in `output`.
   * @param depth Zero for a message payload or `[1, 2]` for a nested record.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1TlvWriterInitialize(
    MoonlightProtocolV1TlvWriter *writer,
    uint8_t *output,
    size_t output_capacity,
    uint8_t depth
  );

  /**
   * @brief Appends one complete canonical TLV field.
   *
   * `value` may overlap `writer->output`; the value is moved before its header
   * is written. Writer state and output are unchanged when validation fails.
   *
   * @param writer Initialized writer state.
   * @param field_id Message-form and nesting-path-local field number.
   * @param flags Zero or `MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED`.
   * @param value Field value, or `NULL` when `value_size` is zero.
   * @param value_size Number of field-value bytes.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1TlvWriterAppend(
    MoonlightProtocolV1TlvWriter *writer,
    uint16_t field_id,
    uint16_t flags,
    const uint8_t *value,
    size_t value_size
  );

  /**
   * @brief Returns the committed canonical TLV payload size.
   *
   * @param writer Initialized writer state.
   * @return Number of bytes committed by successful appends.
   */
  size_t MoonlightProtocolV1TlvWriterSize(
    const MoonlightProtocolV1TlvWriter *writer
  );

#ifdef __cplusplus
}
#endif
