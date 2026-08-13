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
 * @brief Exact byte count of a canonical reliable POINTER_SCROLL body.
 */
#define MOONLIGHT_PROTOCOL_V1_POINTER_SCROLL_BODY_SIZE 4u

/**
 * @brief Exact byte count of a canonical reliable POINTER_SCROLL TLV payload.
 */
#define MOONLIGHT_PROTOCOL_V1_POINTER_SCROLL_PAYLOAD_SIZE \
  (3u * MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + 4u + 2u + \
   MOONLIGHT_PROTOCOL_V1_POINTER_SCROLL_BODY_SIZE)

/**
 * @brief Exact byte count of a canonical reliable POINTER_BUTTON body.
 */
#define MOONLIGHT_PROTOCOL_V1_POINTER_BUTTON_BODY_SIZE 6u

/**
 * @brief Exact byte count of a canonical reliable POINTER_BUTTON TLV payload.
 */
#define MOONLIGHT_PROTOCOL_V1_POINTER_BUTTON_PAYLOAD_SIZE \
  (3u * MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + 4u + 2u + \
   MOONLIGHT_PROTOCOL_V1_POINTER_BUTTON_BODY_SIZE)

/**
 * @brief Exact byte count of a canonical reliable POINTER_SCROLL_AT body.
 */
#define MOONLIGHT_PROTOCOL_V1_POINTER_SCROLL_AT_BODY_SIZE 8u

/**
 * @brief Exact byte count of a canonical reliable POINTER_SCROLL_AT TLV payload.
 */
#define MOONLIGHT_PROTOCOL_V1_POINTER_SCROLL_AT_PAYLOAD_SIZE \
  (3u * MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + 4u + 2u + \
   MOONLIGHT_PROTOCOL_V1_POINTER_SCROLL_AT_BODY_SIZE)

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
 * @brief Exact byte count of a canonical real-time POINTER_ABSOLUTE body.
 */
#define MOONLIGHT_PROTOCOL_V1_POINTER_ABSOLUTE_BODY_SIZE 4u

/**
 * @brief Exact byte count of a complete canonical POINTER_ABSOLUTE DATAGRAM.
 */
#define MOONLIGHT_PROTOCOL_V1_POINTER_ABSOLUTE_DATAGRAM_SIZE \
  (MOONLIGHT_PROTOCOL_V1_DATAGRAM_HEADER_SIZE + \
   MOONLIGHT_PROTOCOL_V1_INPUT_STATE_HEADER_SIZE + \
   MOONLIGHT_PROTOCOL_V1_POINTER_ABSOLUTE_BODY_SIZE)

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
    MOONLIGHT_PROTOCOL_V1_RELIABLE_INPUT_TOUCH_EDGE = 0x0104,  ///< Reliable touch lifecycle edge.
    MOONLIGHT_PROTOCOL_V1_RELIABLE_INPUT_POINTER_SCROLL = 0x0105,  ///< Reliable high-resolution pointer scroll.
    MOONLIGHT_PROTOCOL_V1_RELIABLE_INPUT_POINTER_BUTTON = 0x0106,  ///< Reliable positioned pointer-button edge.
    MOONLIGHT_PROTOCOL_V1_RELIABLE_INPUT_POINTER_SCROLL_AT = 0x0107  ///< Reliable positioned pointer scroll.
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
    MOONLIGHT_PROTOCOL_V1_REALTIME_INPUT_POINTER_ABSOLUTE = 0x0002,  ///< Coalescible absolute pointer position.
    MOONLIGHT_PROTOCOL_V1_REALTIME_INPUT_TOUCH_MOVE = 0x0003  ///< Coalescible current touch position.
  } MoonlightProtocolV1RealtimeInputSubtype;

  /**
   * @brief Identifies one canonical pointer button.
   */
  typedef enum MoonlightProtocolV1PointerButton {
    MOONLIGHT_PROTOCOL_V1_POINTER_BUTTON_LEFT = 1,
    MOONLIGHT_PROTOCOL_V1_POINTER_BUTTON_MIDDLE = 2,
    MOONLIGHT_PROTOCOL_V1_POINTER_BUTTON_RIGHT = 3,
    MOONLIGHT_PROTOCOL_V1_POINTER_BUTTON_X1 = 4,
    MOONLIGHT_PROTOCOL_V1_POINTER_BUTTON_X2 = 5
  } MoonlightProtocolV1PointerButton;

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
   * @brief Holds one canonical reliable high-resolution pointer scroll update.
   *
   * One wheel detent is 120 units. Signed deltas preserve the Client platform's
   * vertical and horizontal direction conventions without coalescing.
   */
  typedef struct MoonlightProtocolV1PointerScroll {
    uint32_t input_sequence;  ///< Nonzero serial shared with other direct input.
    int16_t vertical;  ///< Signed high-resolution vertical wheel units.
    int16_t horizontal;  ///< Signed high-resolution horizontal wheel units.
  } MoonlightProtocolV1PointerScroll;

  /**
   * @brief Holds one canonical reliable positioned pointer-button edge.
   */
  typedef struct MoonlightProtocolV1PointerButtonEdge {
    uint32_t input_sequence;  ///< Nonzero serial shared with other direct input.
    uint16_t x;  ///< Horizontal pointer position normalized over `[0, 65535]`.
    uint16_t y;  ///< Vertical pointer position normalized over `[0, 65535]`.
    MoonlightProtocolV1PointerButton button;  ///< Defined pointer button.
    uint8_t pressed;  ///< Exactly one for press or zero for release.
  } MoonlightProtocolV1PointerButtonEdge;

  /**
   * @brief Holds one canonical reliable positioned pointer scroll update.
   */
  typedef struct MoonlightProtocolV1PointerScrollAt {
    uint32_t input_sequence;  ///< Nonzero serial shared with other direct input.
    uint16_t x;  ///< Horizontal pointer position normalized over `[0, 65535]`.
    uint16_t y;  ///< Vertical pointer position normalized over `[0, 65535]`.
    int16_t vertical;  ///< Signed high-resolution vertical wheel units.
    int16_t horizontal;  ///< Signed high-resolution horizontal wheel units.
  } MoonlightProtocolV1PointerScrollAt;

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
   * @brief Holds one canonical real-time absolute pointer position.
   */
  typedef struct MoonlightProtocolV1PointerAbsolute {
    uint32_t input_sequence;  ///< Nonzero serial shared with reliable input.
    uint16_t x;  ///< Horizontal position normalized over `[0, 65535]`.
    uint16_t y;  ///< Vertical position normalized over `[0, 65535]`.
  } MoonlightProtocolV1PointerAbsolute;

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
   * @brief Decode the real-time input subtype without accepting its body.
   *
   * The caller must subsequently invoke the strict subtype decoder. The common
   * DATAGRAM header, session binding, sequence equality, and body extent are
   * validated. `subtype` remains unchanged on failure.
   *
   * @param expected_session_wire_id Exact nonzero active Stream Session wire ID.
   * @param input Complete DATAGRAM bytes.
   * @param input_size Number of bytes in `input`.
   * @param subtype Receives the host-order subtype.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1DecodeRealtimeInputSubtype(
    uint32_t expected_session_wire_id,
    const uint8_t *input,
    size_t input_size,
    MoonlightProtocolV1RealtimeInputSubtype *subtype
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
   * @brief Encodes one complete canonical reliable POINTER_SCROLL TLV payload.
   *
   * The output buffer and `encoded_size` remain unchanged on failure.
   *
   * @param scroll Nonzero vertical and/or horizontal host-order delta.
   * @param output Destination buffer.
   * @param output_size Available bytes in `output`.
   * @param encoded_size Receives the exact encoded payload size.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1EncodePointerScrollPayload(
    const MoonlightProtocolV1PointerScroll *scroll,
    uint8_t *output,
    size_t output_size,
    size_t *encoded_size
  );

  /**
   * @brief Decodes one complete canonical reliable POINTER_SCROLL TLV payload.
   *
   * `scroll` remains unchanged on failure. The caller validates the containing
   * notification envelope, Reliable Input Lane, negotiated capability,
   * authorization, and active Stream Session.
   *
   * @param input Complete reliable-message payload.
   * @param input_size Number of bytes in `input`.
   * @param scroll Receives validated host-order values only on success.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1DecodePointerScrollPayload(
    const uint8_t *input,
    size_t input_size,
    MoonlightProtocolV1PointerScroll *scroll
  );

  /**
   * @brief Encodes one complete canonical reliable POINTER_BUTTON TLV payload.
   *
   * The output buffer and `encoded_size` remain unchanged on failure.
   *
   * @param edge Positioned host-order pointer-button edge.
   * @param output Destination buffer.
   * @param output_size Available bytes in `output`.
   * @param encoded_size Receives the exact encoded payload size.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1EncodePointerButtonPayload(
    const MoonlightProtocolV1PointerButtonEdge *edge,
    uint8_t *output,
    size_t output_size,
    size_t *encoded_size
  );

  /**
   * @brief Decodes one complete canonical reliable POINTER_BUTTON TLV payload.
   *
   * `edge` remains unchanged on failure. The caller validates the containing
   * notification envelope, Reliable Input Lane, negotiated capabilities,
   * authorization, and active Stream Session.
   *
   * @param input Complete reliable-message payload.
   * @param input_size Number of bytes in `input`.
   * @param edge Receives validated host-order values only on success.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1DecodePointerButtonPayload(
    const uint8_t *input,
    size_t input_size,
    MoonlightProtocolV1PointerButtonEdge *edge
  );

  /**
   * @brief Encodes one complete canonical reliable POINTER_SCROLL_AT TLV payload.
   *
   * The output buffer and `encoded_size` remain unchanged on failure.
   *
   * @param scroll Positioned nonzero vertical and/or horizontal host-order delta.
   * @param output Destination buffer.
   * @param output_size Available bytes in `output`.
   * @param encoded_size Receives the exact encoded payload size.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1EncodePointerScrollAtPayload(
    const MoonlightProtocolV1PointerScrollAt *scroll,
    uint8_t *output,
    size_t output_size,
    size_t *encoded_size
  );

  /**
   * @brief Decodes one complete canonical reliable POINTER_SCROLL_AT TLV payload.
   *
   * `scroll` remains unchanged on failure. The caller validates the containing
   * notification envelope, Reliable Input Lane, negotiated capabilities,
   * authorization, and active Stream Session.
   *
   * @param input Complete reliable-message payload.
   * @param input_size Number of bytes in `input`.
   * @param scroll Receives validated host-order values only on success.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1DecodePointerScrollAtPayload(
    const uint8_t *input,
    size_t input_size,
    MoonlightProtocolV1PointerScrollAt *scroll
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
   * @brief Encodes one complete canonical real-time POINTER_ABSOLUTE DATAGRAM.
   *
   * The common DATAGRAM sequence and inner input-state sequence are both
   * written from `position->input_sequence`. The output buffer and
   * `encoded_size` remain unchanged on failure.
   *
   * @param session_wire_id Exact nonzero active Stream Session wire ID.
   * @param position Validated host-order pointer position.
   * @param output Destination buffer.
   * @param output_size Available bytes in `output`.
   * @param encoded_size Receives the exact complete DATAGRAM size.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1EncodePointerAbsoluteDatagram(
    uint32_t session_wire_id,
    const MoonlightProtocolV1PointerAbsolute *position,
    uint8_t *output,
    size_t output_size,
    size_t *encoded_size
  );

  /**
   * @brief Decodes one complete canonical real-time POINTER_ABSOLUTE DATAGRAM.
   *
   * The decoder requires the exact active session, real-time input channel,
   * zero flags and Media Epoch, exact body size, and equal nonzero outer and
   * inner input sequences. `position` remains unchanged on failure.
   *
   * @param expected_session_wire_id Exact nonzero active Stream Session wire ID.
   * @param input Complete DATAGRAM bytes.
   * @param input_size Number of bytes in `input`.
   * @param position Receives validated host-order values only on success.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1DecodePointerAbsoluteDatagram(
    uint32_t expected_session_wire_id,
    const uint8_t *input,
    size_t input_size,
    MoonlightProtocolV1PointerAbsolute *position
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
