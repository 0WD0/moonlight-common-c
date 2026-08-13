/**
 * @file input.h
 * @brief Encodes and decodes canonical protocol version 1 typed input.
 */
#pragma once

#include <moonlight/protocol/media.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Exact byte count of a canonical reliable KEY_EDGE body.
 */
#define MOONLIGHT_PROTOCOL_V1_KEY_EDGE_BODY_SIZE 8u

/**
 * @brief Exact byte count of a canonical reliable KEY_EDGE TLV payload.
 */
#define MOONLIGHT_PROTOCOL_V1_KEY_EDGE_PAYLOAD_SIZE \
  (3u * MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + 4u + 2u + \
   MOONLIGHT_PROTOCOL_V1_KEY_EDGE_BODY_SIZE)

/**
 * @brief Exact byte count of a canonical reliable TOUCH_EDGE body.
 */
#define MOONLIGHT_PROTOCOL_V1_TOUCH_EDGE_BODY_SIZE 18u

/**
 * @brief Exact byte count of a canonical reliable TOUCH_EDGE TLV payload.
 */
#define MOONLIGHT_PROTOCOL_V1_TOUCH_EDGE_PAYLOAD_SIZE \
  (3u * MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + 4u + 2u + \
   MOONLIGHT_PROTOCOL_V1_TOUCH_EDGE_BODY_SIZE)

/**
 * @brief Exact byte count of the common real-time input state header.
 */
#define MOONLIGHT_PROTOCOL_V1_INPUT_STATE_HEADER_SIZE 8u

/**
 * @brief Exact byte count of a canonical real-time TOUCH_MOVE body.
 */
#define MOONLIGHT_PROTOCOL_V1_TOUCH_MOVE_BODY_SIZE 16u

/**
 * @brief Exact byte count of a complete canonical TOUCH_MOVE DATAGRAM.
 */
#define MOONLIGHT_PROTOCOL_V1_TOUCH_MOVE_DATAGRAM_SIZE \
  (MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE + \
   MOONLIGHT_PROTOCOL_V1_INPUT_STATE_HEADER_SIZE + \
   MOONLIGHT_PROTOCOL_V1_TOUCH_MOVE_BODY_SIZE)

  /**
   * @brief Identifies a reliable protocol version 1 input subtype.
   */
  typedef enum MoonlightProtocolV1ReliableInputSubtype {
    MOONLIGHT_PROTOCOL_V1_RELIABLE_INPUT_KEY_EDGE = 0x0101,  ///< Reliable physical-keyboard edge.
    MOONLIGHT_PROTOCOL_V1_RELIABLE_INPUT_TOUCH_EDGE = 0x0104  ///< Reliable touch lifecycle edge.
  } MoonlightProtocolV1ReliableInputSubtype;

  /**
   * @brief USB HID boot-keyboard modifier bitmap carried by KEY_EDGE.
   */
  typedef enum MoonlightProtocolV1KeyModifier {
    MOONLIGHT_PROTOCOL_V1_KEY_MODIFIER_LEFT_CONTROL = 1u << 0u,
    MOONLIGHT_PROTOCOL_V1_KEY_MODIFIER_LEFT_SHIFT = 1u << 1u,
    MOONLIGHT_PROTOCOL_V1_KEY_MODIFIER_LEFT_ALT = 1u << 2u,
    MOONLIGHT_PROTOCOL_V1_KEY_MODIFIER_LEFT_GUI = 1u << 3u,
    MOONLIGHT_PROTOCOL_V1_KEY_MODIFIER_RIGHT_CONTROL = 1u << 4u,
    MOONLIGHT_PROTOCOL_V1_KEY_MODIFIER_RIGHT_SHIFT = 1u << 5u,
    MOONLIGHT_PROTOCOL_V1_KEY_MODIFIER_RIGHT_ALT = 1u << 6u,
    MOONLIGHT_PROTOCOL_V1_KEY_MODIFIER_RIGHT_GUI = 1u << 7u
  } MoonlightProtocolV1KeyModifier;

  /**
   * @brief Identifies a real-time protocol version 1 input subtype.
   */
  typedef enum MoonlightProtocolV1RealtimeInputSubtype {
    MOONLIGHT_PROTOCOL_V1_REALTIME_INPUT_TOUCH_MOVE = 0x0003  ///< Coalescible current touch position.
  } MoonlightProtocolV1RealtimeInputSubtype;

  /**
   * @brief Identifies one reliable touch lifecycle event.
   */
  typedef enum MoonlightProtocolV1TouchEvent {
    MOONLIGHT_PROTOCOL_V1_TOUCH_EVENT_DOWN = 1,  ///< Begins one contact.
    MOONLIGHT_PROTOCOL_V1_TOUCH_EVENT_UP = 2,  ///< Ends one contact at the supplied position.
    MOONLIGHT_PROTOCOL_V1_TOUCH_EVENT_CANCEL = 3  ///< Cancels one contact without position state.
  } MoonlightProtocolV1TouchEvent;

  /**
   * @brief Holds one canonical reliable physical-keyboard edge.
   */
  typedef struct MoonlightProtocolV1KeyEdge {
    uint32_t input_sequence;  ///< Nonzero serial shared with other direct input.
    uint16_t usage_page;  ///< USB HID usage page.
    uint16_t usage;  ///< USB HID usage within `usage_page`.
    uint8_t pressed;  ///< Exactly one for press or zero for release.
    uint8_t modifiers;  ///< USB HID boot-keyboard modifier bitmap after this edge.
  } MoonlightProtocolV1KeyEdge;

  /**
   * @brief Holds one canonical reliable TOUCH_EDGE notification.
   */
  typedef struct MoonlightProtocolV1TouchEdge {
    uint32_t input_sequence;  ///< Nonzero serial shared with real-time input state.
    uint32_t contact;  ///< Client-scoped logical contact identifier.
    MoonlightProtocolV1TouchEvent event;  ///< Defined touch lifecycle event.
    uint16_t x;  ///< Horizontal position normalized over `[0, 65535]`.
    uint16_t y;  ///< Vertical position normalized over `[0, 65535]`.
    uint16_t pressure;  ///< Contact pressure normalized over `[0, 65535]`.
    uint16_t major;  ///< Major contact-axis length normalized over `[0, 65535]`.
    uint16_t minor;  ///< Minor contact-axis length normalized over `[0, 65535]`.
    int16_t rotation_centidegrees;  ///< Contact-axis rotation in hundredths of a degree.
  } MoonlightProtocolV1TouchEdge;

  /**
   * @brief Holds one canonical real-time TOUCH_MOVE state.
   */
  typedef struct MoonlightProtocolV1TouchMove {
    uint32_t input_sequence;  ///< Nonzero serial shared with reliable input edges.
    uint32_t contact;  ///< Client-scoped logical contact identifier.
    uint16_t x;  ///< Horizontal position normalized over `[0, 65535]`.
    uint16_t y;  ///< Vertical position normalized over `[0, 65535]`.
    uint16_t pressure;  ///< Contact pressure normalized over `[0, 65535]`.
    uint16_t major;  ///< Major contact-axis length normalized over `[0, 65535]`.
    uint16_t minor;  ///< Minor contact-axis length normalized over `[0, 65535]`.
    int16_t rotation_centidegrees;  ///< Contact-axis rotation in hundredths of a degree.
  } MoonlightProtocolV1TouchMove;

  /**
   * @brief Decode the reliable input subtype without accepting its body.
   *
   * The caller must subsequently invoke the strict subtype decoder. `subtype`
   * remains unchanged on failure.
   *
   * @param input Complete reliable-message payload.
   * @param input_size Number of bytes in `input`.
   * @param subtype Receives the host-order subtype.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1DecodeReliableInputSubtype(
    const uint8_t *input,
    size_t input_size,
    MoonlightProtocolV1ReliableInputSubtype *subtype
  );

  /**
   * @brief Encodes one complete canonical reliable KEY_EDGE TLV payload.
   *
   * The output buffer and `encoded_size` remain unchanged on failure.
   *
   * @param edge Validated host-order edge.
   * @param output Destination buffer.
   * @param output_size Available bytes in `output`.
   * @param encoded_size Receives the exact encoded payload size.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1EncodeKeyEdgePayload(
    const MoonlightProtocolV1KeyEdge *edge,
    uint8_t *output,
    size_t output_size,
    size_t *encoded_size
  );

  /**
   * @brief Decodes one complete canonical reliable KEY_EDGE TLV payload.
   *
   * `edge` remains unchanged on failure. The caller validates the containing
   * notification envelope, Reliable Input Lane, negotiated capability,
   * authorization, and active Stream Session.
   *
   * @param input Complete reliable-message payload.
   * @param input_size Number of bytes in `input`.
   * @param edge Receives validated host-order values only on success.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1DecodeKeyEdgePayload(
    const uint8_t *input,
    size_t input_size,
    MoonlightProtocolV1KeyEdge *edge
  );

  /**
   * @brief Encodes one complete canonical reliable TOUCH_EDGE TLV payload.
   *
   * The output buffer and `encoded_size` remain unchanged on failure. The
   * containing reliable envelope must use
   * `MOONLIGHT_PROTOCOL_V1_MESSAGE_INPUT_RELIABLE`.
   *
   * @param edge Validated host-order edge.
   * @param output Destination buffer.
   * @param output_size Available bytes in `output`.
   * @param encoded_size Receives the exact encoded payload size.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1EncodeTouchEdgePayload(
    const MoonlightProtocolV1TouchEdge *edge,
    uint8_t *output,
    size_t output_size,
    size_t *encoded_size
  );

  /**
   * @brief Decodes one complete canonical reliable TOUCH_EDGE TLV payload.
   *
   * `edge` remains unchanged on failure. The caller validates the containing
   * notification envelope, Reliable Input Lane, negotiated capability,
   * authorization, and active Stream Session.
   *
   * @param input Complete reliable-message payload.
   * @param input_size Number of bytes in `input`.
   * @param edge Receives validated host-order values only on success.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1DecodeTouchEdgePayload(
    const uint8_t *input,
    size_t input_size,
    MoonlightProtocolV1TouchEdge *edge
  );

  /**
   * @brief Encodes one complete canonical real-time TOUCH_MOVE DATAGRAM.
   *
   * The common DATAGRAM sequence and inner input-state sequence are both
   * written from `move->input_sequence`. The output buffer and `encoded_size`
   * remain unchanged on failure.
   *
   * @param session_wire_id Exact nonzero active Stream Session wire ID.
   * @param move Validated host-order touch state.
   * @param output Destination buffer.
   * @param output_size Available bytes in `output`.
   * @param encoded_size Receives the exact complete DATAGRAM size.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1EncodeTouchMoveDatagram(
    uint32_t session_wire_id,
    const MoonlightProtocolV1TouchMove *move,
    uint8_t *output,
    size_t output_size,
    size_t *encoded_size
  );

  /**
   * @brief Decodes one complete canonical real-time TOUCH_MOVE DATAGRAM.
   *
   * The decoder requires the exact active session, real-time input channel,
   * zero flags and Media Epoch, defined subtype, exact body size, and equal
   * nonzero outer and inner input sequences. `move` remains unchanged on
   * failure.
   *
   * @param expected_session_wire_id Exact nonzero active Stream Session wire ID.
   * @param input Complete DATAGRAM bytes.
   * @param input_size Number of bytes in `input`.
   * @param move Receives validated host-order values only on success.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1DecodeTouchMoveDatagram(
    uint32_t expected_session_wire_id,
    const uint8_t *input,
    size_t input_size,
    MoonlightProtocolV1TouchMove *move
  );

#ifdef __cplusplus
}
#endif
