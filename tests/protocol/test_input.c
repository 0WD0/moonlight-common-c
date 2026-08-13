/**
 * @file test_input.c
 * @brief Native tests for canonical protocol version 1 typed input.
 */

#include <moonlight/protocol/control.h>
#include <moonlight/protocol/input.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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
 * @brief Canonical reliable KEY_EDGE golden bytes.
 */
static const uint8_t TEST_KEY_EDGE_GOLDEN[MOONLIGHT_PROTOCOL_V1_KEY_EDGE_PAYLOAD_SIZE] = {
  0x00,
  0x01,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x04,
  0x01,
  0x02,
  0x03,
  0x04,
  0x00,
  0x02,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x02,
  0x01,
  0x01,
  0x00,
  0x03,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x08,
  0x00,
  0x07,
  0x00,
  0x04,
  0x01,
  0x03,
  0x00,
  0x00,
};

/**
 * @brief Canonical reliable POINTER_SCROLL golden bytes.
 */
static const uint8_t TEST_POINTER_SCROLL_GOLDEN[MOONLIGHT_PROTOCOL_V1_POINTER_SCROLL_PAYLOAD_SIZE] = {
  0x00,
  0x01,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x04,
  0x01,
  0x02,
  0x03,
  0x04,
  0x00,
  0x02,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x02,
  0x01,
  0x05,
  0x00,
  0x03,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x04,
  0x00,
  0x78,
  0xff,
  0x10,
};

/**
 * @brief Canonical reliable POINTER_BUTTON golden bytes.
 */
static const uint8_t TEST_POINTER_BUTTON_GOLDEN[MOONLIGHT_PROTOCOL_V1_POINTER_BUTTON_PAYLOAD_SIZE] = {
  0x00,
  0x01,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x04,
  0x01,
  0x02,
  0x03,
  0x04,
  0x00,
  0x02,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x02,
  0x01,
  0x06,
  0x00,
  0x03,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x06,
  0x11,
  0x22,
  0x33,
  0x44,
  0x03,
  0x01,
};

/**
 * @brief Canonical reliable POINTER_SCROLL_AT golden bytes.
 */
static const uint8_t TEST_POINTER_SCROLL_AT_GOLDEN[MOONLIGHT_PROTOCOL_V1_POINTER_SCROLL_AT_PAYLOAD_SIZE] = {
  0x00,
  0x01,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x04,
  0x01,
  0x02,
  0x03,
  0x04,
  0x00,
  0x02,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x02,
  0x01,
  0x07,
  0x00,
  0x03,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x08,
  0x11,
  0x22,
  0x33,
  0x44,
  0x00,
  0x78,
  0xff,
  0x10,
};

/**
 * @brief Canonical reliable TOUCH_EDGE golden bytes.
 */
static const uint8_t TEST_TOUCH_EDGE_GOLDEN[MOONLIGHT_PROTOCOL_V1_TOUCH_EDGE_PAYLOAD_SIZE] = {
  0x00,
  0x01,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x04,
  0x01,
  0x02,
  0x03,
  0x04,
  0x00,
  0x02,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x02,
  0x01,
  0x04,
  0x00,
  0x03,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x12,
  0xa1,
  0xb2,
  0xc3,
  0xd4,
  0x01,
  0x00,
  0x11,
  0x22,
  0x33,
  0x44,
  0x55,
  0x66,
  0x77,
  0x88,
  0x99,
  0xaa,
  0xfb,
  0x2e,
};

/**
 * @brief Canonical real-time TOUCH_MOVE DATAGRAM golden bytes.
 */
static const uint8_t TEST_TOUCH_MOVE_GOLDEN[MOONLIGHT_PROTOCOL_V1_TOUCH_MOVE_DATAGRAM_SIZE] = {
  0x05,
  0x00,
  0x00,
  0x00,
  0x11,
  0x22,
  0x33,
  0x44,
  0x00,
  0x00,
  0x00,
  0x00,
  0x01,
  0x02,
  0x03,
  0x04,
  0x01,
  0x02,
  0x03,
  0x04,
  0x00,
  0x03,
  0x00,
  0x10,
  0xa1,
  0xb2,
  0xc3,
  0xd4,
  0x11,
  0x22,
  0x33,
  0x44,
  0x55,
  0x66,
  0x77,
  0x88,
  0x99,
  0xaa,
  0xfb,
  0x2e,
};

/**
 * @brief Canonical real-time POINTER_ABSOLUTE DATAGRAM golden bytes.
 */
static const uint8_t TEST_POINTER_ABSOLUTE_GOLDEN[MOONLIGHT_PROTOCOL_V1_POINTER_ABSOLUTE_DATAGRAM_SIZE] = {
  0x05,
  0x00,
  0x00,
  0x00,
  0x11,
  0x22,
  0x33,
  0x44,
  0x00,
  0x00,
  0x00,
  0x00,
  0x01,
  0x02,
  0x03,
  0x04,
  0x01,
  0x02,
  0x03,
  0x04,
  0x00,
  0x02,
  0x00,
  0x04,
  0x11,
  0x22,
  0x33,
  0x44,
};

/**
 * @brief Canonical host-order keyboard edge matching the golden bytes.
 */
static const MoonlightProtocolV1KeyEdge TEST_KEY_EDGE = {
  .input_sequence = UINT32_C(0x01020304),
  .usage_page = UINT16_C(0x0007),
  .usage = UINT16_C(0x0004),
  .pressed = 1,
  .modifiers =
    MOONLIGHT_PROTOCOL_V1_KEY_MODIFIER_LEFT_CONTROL |
    MOONLIGHT_PROTOCOL_V1_KEY_MODIFIER_LEFT_SHIFT,
};

/**
 * @brief Canonical host-order pointer scroll matching the golden bytes.
 */
static const MoonlightProtocolV1PointerScroll TEST_POINTER_SCROLL = {
  .input_sequence = UINT32_C(0x01020304),
  .vertical = 120,
  .horizontal = -240,
};

/**
 * @brief Canonical host-order positioned scroll matching the golden bytes.
 */
static const MoonlightProtocolV1PointerScrollAt TEST_POINTER_SCROLL_AT = {
  .input_sequence = UINT32_C(0x01020304),
  .x = UINT16_C(0x1122),
  .y = UINT16_C(0x3344),
  .vertical = 120,
  .horizontal = -240,
};

/**
 * @brief Canonical host-order positioned button edge matching the golden bytes.
 */
static const MoonlightProtocolV1PointerButtonEdge TEST_POINTER_BUTTON = {
  .input_sequence = UINT32_C(0x01020304),
  .x = UINT16_C(0x1122),
  .y = UINT16_C(0x3344),
  .button = MOONLIGHT_PROTOCOL_V1_POINTER_BUTTON_RIGHT,
  .pressed = 1,
};

/**
 * @brief Canonical host-order absolute pointer position matching the golden DATAGRAM.
 */
static const MoonlightProtocolV1PointerAbsolute TEST_POINTER_ABSOLUTE = {
  .input_sequence = UINT32_C(0x01020304),
  .x = UINT16_C(0x1122),
  .y = UINT16_C(0x3344),
};

/**
 * @brief Canonical host-order reliable edge matching the golden bytes.
 */
static const MoonlightProtocolV1TouchEdge TEST_TOUCH_EDGE = {
  .input_sequence = UINT32_C(0x01020304),
  .contact = UINT32_C(0xa1b2c3d4),
  .event = MOONLIGHT_PROTOCOL_V1_TOUCH_EVENT_DOWN,
  .x = UINT16_C(0x1122),
  .y = UINT16_C(0x3344),
  .pressure = UINT16_C(0x5566),
  .major = UINT16_C(0x7788),
  .minor = UINT16_C(0x99aa),
  .rotation_centidegrees = -1234,
};

/**
 * @brief Canonical host-order move matching the golden DATAGRAM.
 */
static const MoonlightProtocolV1TouchMove TEST_TOUCH_MOVE = {
  .input_sequence = UINT32_C(0x01020304),
  .contact = UINT32_C(0xa1b2c3d4),
  .x = UINT16_C(0x1122),
  .y = UINT16_C(0x3344),
  .pressure = UINT16_C(0x5566),
  .major = UINT16_C(0x7788),
  .minor = UINT16_C(0x99aa),
  .rotation_centidegrees = -1234,
};

/**
 * @brief Stores one network-order 16-bit integer for a negative mutation.
 *
 * @param output Two writable bytes.
 * @param value Host-order value.
 */
static void test_store_u16(uint8_t *output, uint16_t value) {
  output[0] = (uint8_t) (value >> 8u);
  output[1] = (uint8_t) value;
}

/**
 * @brief Stores one network-order 32-bit integer for a negative mutation.
 *
 * @param output Four writable bytes.
 * @param value Host-order value.
 */
static void test_store_u32(uint8_t *output, uint32_t value) {
  output[0] = (uint8_t) (value >> 24u);
  output[1] = (uint8_t) (value >> 16u);
  output[2] = (uint8_t) (value >> 8u);
  output[3] = (uint8_t) value;
}

/**
 * @brief Compares every semantic field in two keyboard edges.
 *
 * @param left First edge.
 * @param right Second edge.
 * @return True only when every field matches.
 */
static bool test_key_edges_equal(
  const MoonlightProtocolV1KeyEdge *left,
  const MoonlightProtocolV1KeyEdge *right
) {
  return left->input_sequence == right->input_sequence &&
         left->usage_page == right->usage_page &&
         left->usage == right->usage &&
         left->pressed == right->pressed &&
         left->modifiers == right->modifiers;
}

/**
 * @brief Compares every semantic field in two pointer scroll updates.
 */
static bool test_pointer_scrolls_equal(
  const MoonlightProtocolV1PointerScroll *left,
  const MoonlightProtocolV1PointerScroll *right
) {
  return left->input_sequence == right->input_sequence &&
         left->vertical == right->vertical &&
         left->horizontal == right->horizontal;
}

/**
 * @brief Compares every semantic field in two reliable edges.
 *
 * @param left First edge.
 * @param right Second edge.
 * @return True only when every field matches.
 */
static bool test_touch_edges_equal(
  const MoonlightProtocolV1TouchEdge *left,
  const MoonlightProtocolV1TouchEdge *right
) {
  return left->input_sequence == right->input_sequence &&
         left->contact == right->contact &&
         left->event == right->event &&
         left->x == right->x &&
         left->y == right->y &&
         left->pressure == right->pressure &&
         left->major == right->major &&
         left->minor == right->minor &&
         left->rotation_centidegrees == right->rotation_centidegrees;
}

/**
 * @brief Compares every semantic field in two real-time moves.
 *
 * @param left First move.
 * @param right Second move.
 * @return True only when every field matches.
 */
static bool test_touch_moves_equal(
  const MoonlightProtocolV1TouchMove *left,
  const MoonlightProtocolV1TouchMove *right
) {
  return left->input_sequence == right->input_sequence &&
         left->contact == right->contact &&
         left->x == right->x &&
         left->y == right->y &&
         left->pressure == right->pressure &&
         left->major == right->major &&
         left->minor == right->minor &&
         left->rotation_centidegrees == right->rotation_centidegrees;
}

/**
 * @brief Exercises canonical keyboard bytes and reliable-subtype dispatch.
 *
 * @return True when encoding, decoding, and subtype inspection agree.
 */
static bool test_key_edge_golden(void) {
  uint8_t encoded[MOONLIGHT_PROTOCOL_V1_KEY_EDGE_PAYLOAD_SIZE];
  MoonlightProtocolV1KeyEdge decoded;
  MoonlightProtocolV1ReliableInputSubtype subtype =
    MOONLIGHT_PROTOCOL_V1_RELIABLE_INPUT_TOUCH_EDGE;
  size_t encoded_size = 0;

  TEST_CHECK(
    MOONLIGHT_PROTOCOL_V1_CAPABILITY_PHYSICAL_KEYBOARD ==
    UINT64_C(0x100)
  );
  TEST_CHECK(MOONLIGHT_PROTOCOL_V1_KEY_EDGE_BODY_SIZE == 8u);
  TEST_CHECK(MOONLIGHT_PROTOCOL_V1_KEY_EDGE_PAYLOAD_SIZE == 38u);
  TEST_RESULT(
    MoonlightProtocolV1EncodeKeyEdgePayload(
      &TEST_KEY_EDGE,
      encoded,
      sizeof(encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(encoded_size == sizeof(encoded));
  TEST_CHECK(memcmp(encoded, TEST_KEY_EDGE_GOLDEN, sizeof(encoded)) == 0);
  TEST_RESULT(
    MoonlightProtocolV1DecodeReliableInputSubtype(
      TEST_KEY_EDGE_GOLDEN,
      sizeof(TEST_KEY_EDGE_GOLDEN),
      &subtype
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(subtype == MOONLIGHT_PROTOCOL_V1_RELIABLE_INPUT_KEY_EDGE);
  TEST_RESULT(
    MoonlightProtocolV1DecodeKeyEdgePayload(
      TEST_KEY_EDGE_GOLDEN,
      sizeof(TEST_KEY_EDGE_GOLDEN),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(test_key_edges_equal(&decoded, &TEST_KEY_EDGE));
  TEST_RESULT(
    MoonlightProtocolV1DecodeReliableInputSubtype(
      TEST_TOUCH_EDGE_GOLDEN,
      sizeof(TEST_TOUCH_EDGE_GOLDEN),
      &subtype
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(subtype == MOONLIGHT_PROTOCOL_V1_RELIABLE_INPUT_TOUCH_EDGE);
  return true;
}

/**
 * @brief Exercises keyboard codec argument and semantic rejections.
 *
 * @return True when failures preserve every caller-owned output.
 */
static bool test_key_edge_rejections(void) {
  uint8_t output[MOONLIGHT_PROTOCOL_V1_KEY_EDGE_PAYLOAD_SIZE];
  uint8_t output_before[sizeof(output)];
  uint8_t mutated[sizeof(TEST_KEY_EDGE_GOLDEN)];
  MoonlightProtocolV1KeyEdge edge = TEST_KEY_EDGE;
  MoonlightProtocolV1KeyEdge decoded;
  MoonlightProtocolV1KeyEdge decoded_before;
  MoonlightProtocolV1ReliableInputSubtype subtype =
    MOONLIGHT_PROTOCOL_V1_RELIABLE_INPUT_TOUCH_EDGE;
  size_t encoded_size = 17;

  memset(output, 0xa5, sizeof(output));
  memcpy(output_before, output, sizeof(output));
  memset(&decoded, 0x5a, sizeof(decoded));
  decoded_before = decoded;
  TEST_RESULT(
    MoonlightProtocolV1EncodeKeyEdgePayload(
      NULL,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeKeyEdgePayload(
      &edge,
      NULL,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeKeyEdgePayload(
      &edge,
      output,
      sizeof(output),
      NULL
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeKeyEdgePayload(
      &edge,
      output,
      sizeof(output) - 1u,
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL
  );
  TEST_CHECK(encoded_size == 17u);
  TEST_CHECK(memcmp(output, output_before, sizeof(output)) == 0);

  edge.input_sequence = 0;
  TEST_RESULT(
    MoonlightProtocolV1EncodeKeyEdgePayload(
      &edge,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  edge = TEST_KEY_EDGE;
  edge.usage_page = 0;
  TEST_RESULT(
    MoonlightProtocolV1EncodeKeyEdgePayload(
      &edge,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  edge = TEST_KEY_EDGE;
  edge.usage = 0;
  TEST_RESULT(
    MoonlightProtocolV1EncodeKeyEdgePayload(
      &edge,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  edge = TEST_KEY_EDGE;
  edge.pressed = 2;
  TEST_RESULT(
    MoonlightProtocolV1EncodeKeyEdgePayload(
      &edge,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );

  TEST_RESULT(
    MoonlightProtocolV1DecodeKeyEdgePayload(
      NULL,
      sizeof(TEST_KEY_EDGE_GOLDEN),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeKeyEdgePayload(
      TEST_KEY_EDGE_GOLDEN,
      sizeof(TEST_KEY_EDGE_GOLDEN),
      NULL
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeKeyEdgePayload(
      TEST_KEY_EDGE_GOLDEN,
      sizeof(TEST_KEY_EDGE_GOLDEN) + 1u,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_LIMIT_EXCEEDED
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeKeyEdgePayload(
      TEST_KEY_EDGE_GOLDEN,
      sizeof(TEST_KEY_EDGE_GOLDEN) - 1u,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );

  memcpy(mutated, TEST_KEY_EDGE_GOLDEN, sizeof(mutated));
  test_store_u16(mutated + 20u, 0x0104);
  TEST_RESULT(
    MoonlightProtocolV1DecodeKeyEdgePayload(
      mutated,
      sizeof(mutated),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  memcpy(mutated, TEST_KEY_EDGE_GOLDEN, sizeof(mutated));
  mutated[34] = 2;
  TEST_RESULT(
    MoonlightProtocolV1DecodeKeyEdgePayload(
      mutated,
      sizeof(mutated),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  memcpy(mutated, TEST_KEY_EDGE_GOLDEN, sizeof(mutated));
  mutated[36] = 1;
  TEST_RESULT(
    MoonlightProtocolV1DecodeKeyEdgePayload(
      mutated,
      sizeof(mutated),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  memcpy(mutated, TEST_KEY_EDGE_GOLDEN, sizeof(mutated));
  test_store_u32(mutated + 8u, 0);
  TEST_RESULT(
    MoonlightProtocolV1DecodeKeyEdgePayload(
      mutated,
      sizeof(mutated),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, TEST_KEY_EDGE_GOLDEN, sizeof(mutated));
  test_store_u16(mutated + 30u, 0);
  TEST_RESULT(
    MoonlightProtocolV1DecodeKeyEdgePayload(
      mutated,
      sizeof(mutated),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  TEST_CHECK(memcmp(&decoded, &decoded_before, sizeof(decoded)) == 0);

  TEST_RESULT(
    MoonlightProtocolV1DecodeReliableInputSubtype(
      NULL,
      sizeof(TEST_KEY_EDGE_GOLDEN),
      &subtype
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeReliableInputSubtype(
      TEST_KEY_EDGE_GOLDEN,
      sizeof(TEST_KEY_EDGE_GOLDEN),
      NULL
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeReliableInputSubtype(
      TEST_KEY_EDGE_GOLDEN,
      MOONLIGHT_PROTOCOL_V1_RELIABLE_INPUT_PAYLOAD_MAX + 1u,
      &subtype
    ),
    MOONLIGHT_PROTOCOL_RESULT_LIMIT_EXCEEDED
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeReliableInputSubtype(
      TEST_KEY_EDGE_GOLDEN,
      1u,
      &subtype
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, TEST_KEY_EDGE_GOLDEN, sizeof(mutated));
  test_store_u32(mutated + 8u, 0);
  TEST_RESULT(
    MoonlightProtocolV1DecodeReliableInputSubtype(
      mutated,
      sizeof(mutated),
      &subtype
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  TEST_CHECK(subtype == MOONLIGHT_PROTOCOL_V1_RELIABLE_INPUT_TOUCH_EDGE);
  return true;
}

/**
 * @brief Exercises canonical pointer-scroll bytes, signed deltas, and rejections.
 */
static bool test_pointer_scroll(void) {
  uint8_t encoded[MOONLIGHT_PROTOCOL_V1_POINTER_SCROLL_PAYLOAD_SIZE];
  uint8_t before[sizeof(encoded)];
  uint8_t mutated[sizeof(encoded)];
  MoonlightProtocolV1PointerScroll scroll = TEST_POINTER_SCROLL;
  MoonlightProtocolV1PointerScroll decoded;
  MoonlightProtocolV1PointerScroll decoded_before;
  MoonlightProtocolV1ReliableInputSubtype subtype =
    MOONLIGHT_PROTOCOL_V1_RELIABLE_INPUT_TOUCH_EDGE;
  size_t encoded_size = 17;

  TEST_CHECK(
    MOONLIGHT_PROTOCOL_V1_CAPABILITY_POINTER_SCROLL ==
    UINT64_C(0x200)
  );
  TEST_CHECK(MOONLIGHT_PROTOCOL_V1_CAPABILITY_MASK == UINT64_C(0x7ff));
  TEST_CHECK(MOONLIGHT_PROTOCOL_V1_POINTER_SCROLL_BODY_SIZE == 4u);
  TEST_CHECK(MOONLIGHT_PROTOCOL_V1_POINTER_SCROLL_PAYLOAD_SIZE == 34u);
  TEST_RESULT(
    MoonlightProtocolV1EncodePointerScrollPayload(
      &scroll,
      encoded,
      sizeof(encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(encoded_size == sizeof(encoded));
  TEST_CHECK(
    memcmp(encoded, TEST_POINTER_SCROLL_GOLDEN, sizeof(encoded)) == 0
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeReliableInputSubtype(
      encoded,
      sizeof(encoded),
      &subtype
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(
    subtype == MOONLIGHT_PROTOCOL_V1_RELIABLE_INPUT_POINTER_SCROLL
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodePointerScrollPayload(
      encoded,
      sizeof(encoded),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(test_pointer_scrolls_equal(&decoded, &scroll));

  scroll.vertical = INT16_MIN;
  scroll.horizontal = INT16_MAX;
  TEST_RESULT(
    MoonlightProtocolV1EncodePointerScrollPayload(
      &scroll,
      encoded,
      sizeof(encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodePointerScrollPayload(
      encoded,
      sizeof(encoded),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(test_pointer_scrolls_equal(&decoded, &scroll));

  memset(encoded, 0xa5, sizeof(encoded));
  memcpy(before, encoded, sizeof(before));
  scroll = TEST_POINTER_SCROLL;
  encoded_size = 17;
  TEST_RESULT(
    MoonlightProtocolV1EncodePointerScrollPayload(
      NULL,
      encoded,
      sizeof(encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodePointerScrollPayload(
      &scroll,
      NULL,
      sizeof(encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodePointerScrollPayload(
      &scroll,
      encoded,
      sizeof(encoded),
      NULL
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodePointerScrollPayload(
      &scroll,
      encoded,
      sizeof(encoded) - 1u,
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL
  );
  TEST_CHECK(encoded_size == 17u);
  TEST_CHECK(memcmp(encoded, before, sizeof(encoded)) == 0);

  scroll.input_sequence = 0;
  TEST_RESULT(
    MoonlightProtocolV1EncodePointerScrollPayload(
      &scroll,
      encoded,
      sizeof(encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  scroll = TEST_POINTER_SCROLL;
  scroll.vertical = 0;
  scroll.horizontal = 0;
  TEST_RESULT(
    MoonlightProtocolV1EncodePointerScrollPayload(
      &scroll,
      encoded,
      sizeof(encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );

  memset(&decoded, 0x5a, sizeof(decoded));
  decoded_before = decoded;
  TEST_RESULT(
    MoonlightProtocolV1DecodePointerScrollPayload(
      NULL,
      sizeof(TEST_POINTER_SCROLL_GOLDEN),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodePointerScrollPayload(
      TEST_POINTER_SCROLL_GOLDEN,
      sizeof(TEST_POINTER_SCROLL_GOLDEN),
      NULL
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodePointerScrollPayload(
      TEST_POINTER_SCROLL_GOLDEN,
      sizeof(TEST_POINTER_SCROLL_GOLDEN) + 1u,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_LIMIT_EXCEEDED
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodePointerScrollPayload(
      TEST_POINTER_SCROLL_GOLDEN,
      sizeof(TEST_POINTER_SCROLL_GOLDEN) - 1u,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, TEST_POINTER_SCROLL_GOLDEN, sizeof(mutated));
  test_store_u16(mutated + 20u, 0x0106);
  TEST_RESULT(
    MoonlightProtocolV1DecodePointerScrollPayload(
      mutated,
      sizeof(mutated),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  memcpy(mutated, TEST_POINTER_SCROLL_GOLDEN, sizeof(mutated));
  memset(mutated + 30u, 0, 4u);
  TEST_RESULT(
    MoonlightProtocolV1DecodePointerScrollPayload(
      mutated,
      sizeof(mutated),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  TEST_CHECK(
    test_pointer_scrolls_equal(&decoded, &decoded_before)
  );
  return true;
}

/**
 * @brief Exercises the complete absolute pointer codec family.
 */
static bool test_pointer_input(void) {
  uint8_t button_encoded[MOONLIGHT_PROTOCOL_V1_POINTER_BUTTON_PAYLOAD_SIZE];
  uint8_t scroll_encoded[MOONLIGHT_PROTOCOL_V1_POINTER_SCROLL_AT_PAYLOAD_SIZE];
  uint8_t position_encoded[MOONLIGHT_PROTOCOL_V1_POINTER_ABSOLUTE_DATAGRAM_SIZE];
  MoonlightProtocolV1PointerButtonEdge button = TEST_POINTER_BUTTON;
  MoonlightProtocolV1PointerButtonEdge decoded_button;
  MoonlightProtocolV1PointerScrollAt scroll = TEST_POINTER_SCROLL_AT;
  MoonlightProtocolV1PointerScrollAt decoded_scroll;
  MoonlightProtocolV1PointerAbsolute position = TEST_POINTER_ABSOLUTE;
  MoonlightProtocolV1PointerAbsolute decoded_position;
  MoonlightProtocolV1ReliableInputSubtype reliable_subtype =
    MOONLIGHT_PROTOCOL_V1_RELIABLE_INPUT_TOUCH_EDGE;
  MoonlightProtocolV1RealtimeInputSubtype realtime_subtype =
    MOONLIGHT_PROTOCOL_V1_REALTIME_INPUT_TOUCH_MOVE;
  size_t encoded_size = 0;

  TEST_CHECK(
    MOONLIGHT_PROTOCOL_V1_CAPABILITY_POINTER_INPUT == UINT64_C(0x400)
  );
  TEST_CHECK(MOONLIGHT_PROTOCOL_V1_POINTER_BUTTON_BODY_SIZE == 6u);
  TEST_CHECK(MOONLIGHT_PROTOCOL_V1_POINTER_BUTTON_PAYLOAD_SIZE == 36u);
  TEST_CHECK(MOONLIGHT_PROTOCOL_V1_POINTER_SCROLL_AT_BODY_SIZE == 8u);
  TEST_CHECK(MOONLIGHT_PROTOCOL_V1_POINTER_SCROLL_AT_PAYLOAD_SIZE == 38u);
  TEST_CHECK(MOONLIGHT_PROTOCOL_V1_POINTER_ABSOLUTE_BODY_SIZE == 4u);
  TEST_CHECK(MOONLIGHT_PROTOCOL_V1_POINTER_ABSOLUTE_DATAGRAM_SIZE == 28u);

  TEST_RESULT(
    MoonlightProtocolV1EncodePointerButtonPayload(
      &button,
      button_encoded,
      sizeof(button_encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(encoded_size == sizeof(button_encoded));
  TEST_CHECK(
    memcmp(
      button_encoded,
      TEST_POINTER_BUTTON_GOLDEN,
      sizeof(button_encoded)
    ) == 0
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeReliableInputSubtype(
      button_encoded,
      sizeof(button_encoded),
      &reliable_subtype
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(
    reliable_subtype ==
    MOONLIGHT_PROTOCOL_V1_RELIABLE_INPUT_POINTER_BUTTON
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodePointerButtonPayload(
      button_encoded,
      sizeof(button_encoded),
      &decoded_button
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(
    decoded_button.input_sequence == button.input_sequence &&
    decoded_button.x == button.x &&
    decoded_button.y == button.y &&
    decoded_button.button == button.button &&
    decoded_button.pressed == button.pressed
  );
  button.pressed = 2;
  TEST_RESULT(
    MoonlightProtocolV1EncodePointerButtonPayload(
      &button,
      button_encoded,
      sizeof(button_encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  button = TEST_POINTER_BUTTON;
  button.button = (MoonlightProtocolV1PointerButton) 0;
  TEST_RESULT(
    MoonlightProtocolV1EncodePointerButtonPayload(
      &button,
      button_encoded,
      sizeof(button_encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );

  TEST_RESULT(
    MoonlightProtocolV1EncodePointerScrollAtPayload(
      &scroll,
      scroll_encoded,
      sizeof(scroll_encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(encoded_size == sizeof(scroll_encoded));
  TEST_CHECK(
    memcmp(
      scroll_encoded,
      TEST_POINTER_SCROLL_AT_GOLDEN,
      sizeof(scroll_encoded)
    ) == 0
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeReliableInputSubtype(
      scroll_encoded,
      sizeof(scroll_encoded),
      &reliable_subtype
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(
    reliable_subtype ==
    MOONLIGHT_PROTOCOL_V1_RELIABLE_INPUT_POINTER_SCROLL_AT
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodePointerScrollAtPayload(
      scroll_encoded,
      sizeof(scroll_encoded),
      &decoded_scroll
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(
    decoded_scroll.input_sequence == scroll.input_sequence &&
    decoded_scroll.x == scroll.x &&
    decoded_scroll.y == scroll.y &&
    decoded_scroll.vertical == scroll.vertical &&
    decoded_scroll.horizontal == scroll.horizontal
  );
  scroll.vertical = 0;
  scroll.horizontal = 0;
  TEST_RESULT(
    MoonlightProtocolV1EncodePointerScrollAtPayload(
      &scroll,
      scroll_encoded,
      sizeof(scroll_encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );

  TEST_RESULT(
    MoonlightProtocolV1EncodePointerAbsoluteDatagram(
      UINT32_C(0x11223344),
      &position,
      position_encoded,
      sizeof(position_encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(encoded_size == sizeof(position_encoded));
  TEST_CHECK(
    memcmp(
      position_encoded,
      TEST_POINTER_ABSOLUTE_GOLDEN,
      sizeof(position_encoded)
    ) == 0
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeRealtimeInputSubtype(
      UINT32_C(0x11223344),
      position_encoded,
      sizeof(position_encoded),
      &realtime_subtype
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(
    realtime_subtype ==
    MOONLIGHT_PROTOCOL_V1_REALTIME_INPUT_POINTER_ABSOLUTE
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodePointerAbsoluteDatagram(
      UINT32_C(0x11223344),
      position_encoded,
      sizeof(position_encoded),
      &decoded_position
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(
    decoded_position.input_sequence == position.input_sequence &&
    decoded_position.x == position.x &&
    decoded_position.y == position.y
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodePointerAbsoluteDatagram(
      UINT32_C(0x55667788),
      position_encoded,
      sizeof(position_encoded),
      &decoded_position
    ),
    MOONLIGHT_PROTOCOL_RESULT_STALE_CONTEXT
  );
  position.input_sequence = 0;
  TEST_RESULT(
    MoonlightProtocolV1EncodePointerAbsoluteDatagram(
      UINT32_C(0x11223344),
      &position,
      position_encoded,
      sizeof(position_encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  return true;
}

/**
 * @brief Exercises both canonical touch golden vectors and signed extremes.
 *
 * @return True when encoding and decoding preserve all canonical bytes.
 */
static bool test_touch_goldens(void) {
  uint8_t encoded[MOONLIGHT_PROTOCOL_V1_TOUCH_EDGE_PAYLOAD_SIZE];
  uint8_t move_encoded[MOONLIGHT_PROTOCOL_V1_TOUCH_MOVE_DATAGRAM_SIZE];
  MoonlightProtocolV1TouchEdge decoded_edge;
  MoonlightProtocolV1TouchMove decoded_move;
  MoonlightProtocolV1TouchMove extreme_move = TEST_TOUCH_MOVE;
  size_t encoded_size = 0;

  TEST_CHECK(
    MOONLIGHT_PROTOCOL_V1_CAPABILITY_REALTIME_INPUT_DATAGRAM ==
    UINT64_C(0x02)
  );
  TEST_CHECK(MOONLIGHT_PROTOCOL_V1_TOUCH_EDGE_BODY_SIZE == 18u);
  TEST_CHECK(MOONLIGHT_PROTOCOL_V1_TOUCH_EDGE_PAYLOAD_SIZE == 48u);
  TEST_CHECK(MOONLIGHT_PROTOCOL_V1_TOUCH_MOVE_BODY_SIZE == 16u);
  TEST_CHECK(MOONLIGHT_PROTOCOL_V1_TOUCH_MOVE_DATAGRAM_SIZE == 40u);

  TEST_RESULT(
    MoonlightProtocolV1EncodeTouchEdgePayload(
      &TEST_TOUCH_EDGE,
      encoded,
      sizeof(encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(encoded_size == sizeof(encoded));
  TEST_CHECK(memcmp(encoded, TEST_TOUCH_EDGE_GOLDEN, sizeof(encoded)) == 0);
  TEST_RESULT(
    MoonlightProtocolV1DecodeTouchEdgePayload(
      TEST_TOUCH_EDGE_GOLDEN,
      sizeof(TEST_TOUCH_EDGE_GOLDEN),
      &decoded_edge
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(test_touch_edges_equal(&decoded_edge, &TEST_TOUCH_EDGE));

  TEST_RESULT(
    MoonlightProtocolV1EncodeTouchMoveDatagram(
      UINT32_C(0x11223344),
      &TEST_TOUCH_MOVE,
      move_encoded,
      sizeof(move_encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(encoded_size == sizeof(move_encoded));
  TEST_CHECK(
    memcmp(move_encoded, TEST_TOUCH_MOVE_GOLDEN, sizeof(move_encoded)) == 0
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeTouchMoveDatagram(
      UINT32_C(0x11223344),
      TEST_TOUCH_MOVE_GOLDEN,
      sizeof(TEST_TOUCH_MOVE_GOLDEN),
      &decoded_move
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(test_touch_moves_equal(&decoded_move, &TEST_TOUCH_MOVE));

  extreme_move.rotation_centidegrees = INT16_MIN;
  TEST_RESULT(
    MoonlightProtocolV1EncodeTouchMoveDatagram(
      UINT32_C(0x11223344),
      &extreme_move,
      move_encoded,
      sizeof(move_encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeTouchMoveDatagram(
      UINT32_C(0x11223344),
      move_encoded,
      sizeof(move_encoded),
      &decoded_move
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(test_touch_moves_equal(&decoded_move, &extreme_move));
  extreme_move.rotation_centidegrees = INT16_MAX;
  TEST_RESULT(
    MoonlightProtocolV1EncodeTouchMoveDatagram(
      UINT32_C(0x11223344),
      &extreme_move,
      move_encoded,
      sizeof(move_encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeTouchMoveDatagram(
      UINT32_C(0x11223344),
      move_encoded,
      sizeof(move_encoded),
      &decoded_move
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(test_touch_moves_equal(&decoded_move, &extreme_move));
  return true;
}

/**
 * @brief Exercises reliable TOUCH_EDGE encoder argument and semantic errors.
 *
 * @return True when invalid edges fail without mutating outputs.
 */
static bool test_touch_edge_encode_rejections(void) {
  uint8_t output[MOONLIGHT_PROTOCOL_V1_TOUCH_EDGE_PAYLOAD_SIZE];
  uint8_t before[sizeof(output)];
  MoonlightProtocolV1TouchEdge edge = TEST_TOUCH_EDGE;
  size_t encoded_size = 17;

  memset(output, 0xa5, sizeof(output));
  memcpy(before, output, sizeof(before));
  TEST_RESULT(
    MoonlightProtocolV1EncodeTouchEdgePayload(
      NULL,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeTouchEdgePayload(
      &edge,
      NULL,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeTouchEdgePayload(
      &edge,
      output,
      sizeof(output),
      NULL
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_CHECK(memcmp(output, before, sizeof(output)) == 0);
  TEST_CHECK(encoded_size == 17);

  edge.input_sequence = 0;
  TEST_RESULT(
    MoonlightProtocolV1EncodeTouchEdgePayload(
      &edge,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  edge.input_sequence = TEST_TOUCH_EDGE.input_sequence;
  edge.event = (MoonlightProtocolV1TouchEvent) 0;
  TEST_RESULT(
    MoonlightProtocolV1EncodeTouchEdgePayload(
      &edge,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  edge.event = (MoonlightProtocolV1TouchEvent) 4;
  TEST_RESULT(
    MoonlightProtocolV1EncodeTouchEdgePayload(
      &edge,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  edge.event = MOONLIGHT_PROTOCOL_V1_TOUCH_EVENT_UP;
  TEST_RESULT(
    MoonlightProtocolV1EncodeTouchEdgePayload(
      &edge,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  edge.event = MOONLIGHT_PROTOCOL_V1_TOUCH_EVENT_CANCEL;
  TEST_RESULT(
    MoonlightProtocolV1EncodeTouchEdgePayload(
      &edge,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  edge.x = 0;
  edge.y = 0;
  edge.pressure = 0;
  edge.major = 0;
  edge.minor = 0;
  edge.rotation_centidegrees = 0;
  TEST_RESULT(
    MoonlightProtocolV1EncodeTouchEdgePayload(
      &edge,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeTouchEdgePayload(
      &TEST_TOUCH_EDGE,
      output,
      sizeof(output) - 1u,
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL
  );
  TEST_CHECK(encoded_size == sizeof(output));
  return true;
}

/**
 * @brief Exercises reliable TOUCH_EDGE decoder structure and value errors.
 *
 * @return True when every malformed payload is classified strictly.
 */
static bool test_touch_edge_decode_rejections(void) {
  uint8_t mutated[MOONLIGHT_PROTOCOL_V1_TOUCH_EDGE_PAYLOAD_SIZE + 1u];
  MoonlightProtocolV1TouchEdge decoded;
  MoonlightProtocolV1TouchEdge before;

  memset(&decoded, 0xa5, sizeof(decoded));
  memcpy(&before, &decoded, sizeof(before));
  TEST_RESULT(
    MoonlightProtocolV1DecodeTouchEdgePayload(
      NULL,
      sizeof(TEST_TOUCH_EDGE_GOLDEN),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeTouchEdgePayload(
      TEST_TOUCH_EDGE_GOLDEN,
      sizeof(TEST_TOUCH_EDGE_GOLDEN),
      NULL
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_CHECK(memcmp(&decoded, &before, sizeof(decoded)) == 0);

  memcpy(mutated, TEST_TOUCH_EDGE_GOLDEN, sizeof(TEST_TOUCH_EDGE_GOLDEN));
  mutated[sizeof(TEST_TOUCH_EDGE_GOLDEN)] = 0;
  TEST_RESULT(
    MoonlightProtocolV1DecodeTouchEdgePayload(
      mutated,
      sizeof(mutated),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_LIMIT_EXCEEDED
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeTouchEdgePayload(mutated, 23u, &decoded),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeTouchEdgePayload(mutated, 24u, &decoded),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );

  memcpy(mutated, TEST_TOUCH_EDGE_GOLDEN, sizeof(TEST_TOUCH_EDGE_GOLDEN));
  mutated[1] = 2;
  TEST_RESULT(
    MoonlightProtocolV1DecodeTouchEdgePayload(
      mutated,
      sizeof(TEST_TOUCH_EDGE_GOLDEN),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  mutated[1] = 4;
  TEST_RESULT(
    MoonlightProtocolV1DecodeTouchEdgePayload(
      mutated,
      sizeof(TEST_TOUCH_EDGE_GOLDEN),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  mutated[1] = 0;
  TEST_RESULT(
    MoonlightProtocolV1DecodeTouchEdgePayload(
      mutated,
      sizeof(TEST_TOUCH_EDGE_GOLDEN),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );

  memcpy(mutated, TEST_TOUCH_EDGE_GOLDEN, sizeof(TEST_TOUCH_EDGE_GOLDEN));
  mutated[3] = 1;
  TEST_RESULT(
    MoonlightProtocolV1DecodeTouchEdgePayload(
      mutated,
      sizeof(TEST_TOUCH_EDGE_GOLDEN),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  mutated[3] = 2;
  TEST_RESULT(
    MoonlightProtocolV1DecodeTouchEdgePayload(
      mutated,
      sizeof(TEST_TOUCH_EDGE_GOLDEN),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );

  memcpy(mutated, TEST_TOUCH_EDGE_GOLDEN, sizeof(TEST_TOUCH_EDGE_GOLDEN));
  test_store_u32(mutated + 4u, 3);
  TEST_RESULT(
    MoonlightProtocolV1DecodeTouchEdgePayload(
      mutated,
      sizeof(TEST_TOUCH_EDGE_GOLDEN),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  test_store_u32(mutated + 4u, UINT32_MAX);
  TEST_RESULT(
    MoonlightProtocolV1DecodeTouchEdgePayload(
      mutated,
      sizeof(TEST_TOUCH_EDGE_GOLDEN),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, TEST_TOUCH_EDGE_GOLDEN, sizeof(TEST_TOUCH_EDGE_GOLDEN));
  test_store_u32(mutated + 8u, 0);
  TEST_RESULT(
    MoonlightProtocolV1DecodeTouchEdgePayload(
      mutated,
      sizeof(TEST_TOUCH_EDGE_GOLDEN),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );

  memcpy(mutated, TEST_TOUCH_EDGE_GOLDEN, sizeof(TEST_TOUCH_EDGE_GOLDEN));
  mutated[13] = 1;
  TEST_RESULT(
    MoonlightProtocolV1DecodeTouchEdgePayload(
      mutated,
      sizeof(TEST_TOUCH_EDGE_GOLDEN),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  mutated[13] = 4;
  TEST_RESULT(
    MoonlightProtocolV1DecodeTouchEdgePayload(
      mutated,
      sizeof(TEST_TOUCH_EDGE_GOLDEN),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  memcpy(mutated, TEST_TOUCH_EDGE_GOLDEN, sizeof(TEST_TOUCH_EDGE_GOLDEN));
  test_store_u32(mutated + 16u, 1);
  TEST_RESULT(
    MoonlightProtocolV1DecodeTouchEdgePayload(
      mutated,
      sizeof(TEST_TOUCH_EDGE_GOLDEN),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, TEST_TOUCH_EDGE_GOLDEN, sizeof(TEST_TOUCH_EDGE_GOLDEN));
  test_store_u16(mutated + 20u, 0x0106);
  TEST_RESULT(
    MoonlightProtocolV1DecodeTouchEdgePayload(
      mutated,
      sizeof(TEST_TOUCH_EDGE_GOLDEN),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );

  memcpy(mutated, TEST_TOUCH_EDGE_GOLDEN, sizeof(TEST_TOUCH_EDGE_GOLDEN));
  mutated[23] = 2;
  TEST_RESULT(
    MoonlightProtocolV1DecodeTouchEdgePayload(
      mutated,
      sizeof(TEST_TOUCH_EDGE_GOLDEN),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  mutated[23] = 4;
  TEST_RESULT(
    MoonlightProtocolV1DecodeTouchEdgePayload(
      mutated,
      sizeof(TEST_TOUCH_EDGE_GOLDEN),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  memcpy(mutated, TEST_TOUCH_EDGE_GOLDEN, sizeof(TEST_TOUCH_EDGE_GOLDEN));
  test_store_u32(mutated + 26u, 17);
  TEST_RESULT(
    MoonlightProtocolV1DecodeTouchEdgePayload(
      mutated,
      sizeof(TEST_TOUCH_EDGE_GOLDEN),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  test_store_u32(mutated + 26u, UINT32_MAX);
  TEST_RESULT(
    MoonlightProtocolV1DecodeTouchEdgePayload(
      mutated,
      sizeof(TEST_TOUCH_EDGE_GOLDEN),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );

  memcpy(mutated, TEST_TOUCH_EDGE_GOLDEN, sizeof(TEST_TOUCH_EDGE_GOLDEN));
  mutated[35] = 1;
  TEST_RESULT(
    MoonlightProtocolV1DecodeTouchEdgePayload(
      mutated,
      sizeof(TEST_TOUCH_EDGE_GOLDEN),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  memcpy(mutated, TEST_TOUCH_EDGE_GOLDEN, sizeof(TEST_TOUCH_EDGE_GOLDEN));
  mutated[34] = 4;
  TEST_RESULT(
    MoonlightProtocolV1DecodeTouchEdgePayload(
      mutated,
      sizeof(TEST_TOUCH_EDGE_GOLDEN),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  mutated[34] = MOONLIGHT_PROTOCOL_V1_TOUCH_EVENT_CANCEL;
  TEST_RESULT(
    MoonlightProtocolV1DecodeTouchEdgePayload(
      mutated,
      sizeof(TEST_TOUCH_EDGE_GOLDEN),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  TEST_CHECK(memcmp(&decoded, &before, sizeof(decoded)) == 0);
  return true;
}

/**
 * @brief Exercises real-time TOUCH_MOVE encoder argument and sequence errors.
 *
 * @return True when failures preserve caller outputs.
 */
static bool test_touch_move_encode_rejections(void) {
  uint8_t output[MOONLIGHT_PROTOCOL_V1_TOUCH_MOVE_DATAGRAM_SIZE];
  uint8_t before[sizeof(output)];
  MoonlightProtocolV1TouchMove move = TEST_TOUCH_MOVE;
  size_t encoded_size = 19;

  memset(output, 0xa5, sizeof(output));
  memcpy(before, output, sizeof(before));
  TEST_RESULT(
    MoonlightProtocolV1EncodeTouchMoveDatagram(
      0,
      &move,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeTouchMoveDatagram(
      1,
      NULL,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeTouchMoveDatagram(
      1,
      &move,
      NULL,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeTouchMoveDatagram(
      1,
      &move,
      output,
      sizeof(output),
      NULL
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_CHECK(memcmp(output, before, sizeof(output)) == 0);
  TEST_CHECK(encoded_size == 19);

  move.input_sequence = 0;
  TEST_RESULT(
    MoonlightProtocolV1EncodeTouchMoveDatagram(
      1,
      &move,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeTouchMoveDatagram(
      1,
      &TEST_TOUCH_MOVE,
      output,
      sizeof(output) - 1u,
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL
  );
  TEST_CHECK(memcmp(output, before, sizeof(output)) == 0);
  TEST_CHECK(encoded_size == 19);
  return true;
}

/**
 * @brief Exercises real-time TOUCH_MOVE header and body rejection paths.
 *
 * @return True when every malformed DATAGRAM has the required result.
 */
static bool test_touch_move_decode_rejections(void) {
  uint8_t mutated[MOONLIGHT_PROTOCOL_V1_TOUCH_MOVE_DATAGRAM_SIZE + 1u];
  MoonlightProtocolV1TouchMove decoded;
  MoonlightProtocolV1TouchMove before;

  memset(&decoded, 0xa5, sizeof(decoded));
  memcpy(&before, &decoded, sizeof(before));
  TEST_RESULT(
    MoonlightProtocolV1DecodeTouchMoveDatagram(
      0,
      TEST_TOUCH_MOVE_GOLDEN,
      sizeof(TEST_TOUCH_MOVE_GOLDEN),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeTouchMoveDatagram(
      UINT32_C(0x11223344),
      NULL,
      sizeof(TEST_TOUCH_MOVE_GOLDEN),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeTouchMoveDatagram(
      UINT32_C(0x11223344),
      TEST_TOUCH_MOVE_GOLDEN,
      sizeof(TEST_TOUCH_MOVE_GOLDEN),
      NULL
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeTouchMoveDatagram(
      UINT32_C(0x11223344),
      TEST_TOUCH_MOVE_GOLDEN,
      sizeof(TEST_TOUCH_MOVE_GOLDEN) - 1u,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_TRUNCATED
  );
  memcpy(mutated, TEST_TOUCH_MOVE_GOLDEN, sizeof(TEST_TOUCH_MOVE_GOLDEN));
  mutated[sizeof(TEST_TOUCH_MOVE_GOLDEN)] = 0;
  TEST_RESULT(
    MoonlightProtocolV1DecodeTouchMoveDatagram(
      UINT32_C(0x11223344),
      mutated,
      sizeof(mutated),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_TRAILING_DATA
  );

  memcpy(mutated, TEST_TOUCH_MOVE_GOLDEN, sizeof(TEST_TOUCH_MOVE_GOLDEN));
  mutated[2] = 1;
  TEST_RESULT(
    MoonlightProtocolV1DecodeTouchMoveDatagram(
      UINT32_C(0x11223344),
      mutated,
      sizeof(TEST_TOUCH_MOVE_GOLDEN),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, TEST_TOUCH_MOVE_GOLDEN, sizeof(TEST_TOUCH_MOVE_GOLDEN));
  mutated[1] = 1;
  TEST_RESULT(
    MoonlightProtocolV1DecodeTouchMoveDatagram(
      UINT32_C(0x11223344),
      mutated,
      sizeof(TEST_TOUCH_MOVE_GOLDEN),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  memcpy(mutated, TEST_TOUCH_MOVE_GOLDEN, sizeof(TEST_TOUCH_MOVE_GOLDEN));
  test_store_u32(mutated + 8u, 1);
  TEST_RESULT(
    MoonlightProtocolV1DecodeTouchMoveDatagram(
      UINT32_C(0x11223344),
      mutated,
      sizeof(TEST_TOUCH_MOVE_GOLDEN),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, TEST_TOUCH_MOVE_GOLDEN, sizeof(TEST_TOUCH_MOVE_GOLDEN));
  mutated[0] = MOONLIGHT_PROTOCOL_V1_CHANNEL_MEDIA_FEEDBACK;
  test_store_u32(mutated + 8u, 1);
  TEST_RESULT(
    MoonlightProtocolV1DecodeTouchMoveDatagram(
      UINT32_C(0x11223344),
      mutated,
      sizeof(TEST_TOUCH_MOVE_GOLDEN),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
  );

  TEST_RESULT(
    MoonlightProtocolV1DecodeTouchMoveDatagram(
      UINT32_C(0x11223345),
      TEST_TOUCH_MOVE_GOLDEN,
      sizeof(TEST_TOUCH_MOVE_GOLDEN),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_STALE_CONTEXT
  );
  memcpy(mutated, TEST_TOUCH_MOVE_GOLDEN, sizeof(TEST_TOUCH_MOVE_GOLDEN));
  test_store_u32(mutated + 12u, 0);
  TEST_RESULT(
    MoonlightProtocolV1DecodeTouchMoveDatagram(
      UINT32_C(0x11223344),
      mutated,
      sizeof(TEST_TOUCH_MOVE_GOLDEN),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, TEST_TOUCH_MOVE_GOLDEN, sizeof(TEST_TOUCH_MOVE_GOLDEN));
  test_store_u32(mutated + 16u, 0);
  TEST_RESULT(
    MoonlightProtocolV1DecodeTouchMoveDatagram(
      UINT32_C(0x11223344),
      mutated,
      sizeof(TEST_TOUCH_MOVE_GOLDEN),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  test_store_u32(mutated + 16u, UINT32_C(0x01020305));
  TEST_RESULT(
    MoonlightProtocolV1DecodeTouchMoveDatagram(
      UINT32_C(0x11223344),
      mutated,
      sizeof(TEST_TOUCH_MOVE_GOLDEN),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, TEST_TOUCH_MOVE_GOLDEN, sizeof(TEST_TOUCH_MOVE_GOLDEN));
  test_store_u16(mutated + 20u, 4);
  TEST_RESULT(
    MoonlightProtocolV1DecodeTouchMoveDatagram(
      UINT32_C(0x11223344),
      mutated,
      sizeof(TEST_TOUCH_MOVE_GOLDEN),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  memcpy(mutated, TEST_TOUCH_MOVE_GOLDEN, sizeof(TEST_TOUCH_MOVE_GOLDEN));
  test_store_u16(mutated + 22u, 15);
  TEST_RESULT(
    MoonlightProtocolV1DecodeTouchMoveDatagram(
      UINT32_C(0x11223344),
      mutated,
      sizeof(TEST_TOUCH_MOVE_GOLDEN),
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  TEST_CHECK(memcmp(&decoded, &before, sizeof(decoded)) == 0);
  return true;
}

/**
 * @brief Runs one named boolean test.
 *
 * @param name Test name.
 * @param test Test function.
 * @return Zero on success, one on failure.
 */
static int run_test(const char *name, bool (*test)(void)) {
  if (!test()) {
    fprintf(stderr, "FAILED: %s\n", name);
    return 1;
  }
  printf("PASS: %s\n", name);
  return 0;
}

/**
 * @brief Runs all protocol version 1 typed input tests.
 *
 * @return Zero only when every test passes.
 */
int main(void) {
  int failures = 0;

  failures += run_test("key_edge_golden", test_key_edge_golden);
  failures += run_test("key_edge_rejections", test_key_edge_rejections);
  failures += run_test("pointer_scroll", test_pointer_scroll);
  failures += run_test("pointer_input", test_pointer_input);
  failures += run_test("touch_goldens", test_touch_goldens);
  failures += run_test(
    "touch_edge_encode_rejections",
    test_touch_edge_encode_rejections
  );
  failures += run_test(
    "touch_edge_decode_rejections",
    test_touch_edge_decode_rejections
  );
  failures += run_test(
    "touch_move_encode_rejections",
    test_touch_move_encode_rejections
  );
  failures += run_test(
    "touch_move_decode_rejections",
    test_touch_move_decode_rejections
  );
  return failures == 0 ? 0 : 1;
}
