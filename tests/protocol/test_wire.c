/**
 * @file test_wire.c
 * @brief Native tests for protocol version 1 reliable framing and TLV codecs.
 */

#include <limits.h>
#include <moonlight/protocol/client_proof.h>
#include <moonlight/protocol/wire.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief Maximum stream payload retained by one test observation.
 */
#define TEST_OBSERVED_PAYLOAD_MAX 64u

/**
 * @brief Maximum envelopes retained by one test observation.
 */
#define TEST_OBSERVED_ENVELOPE_MAX 8u

/**
 * @brief Storage for one more than the top-level TLV field limit.
 */
#define TEST_MANY_TLV_BYTES \
  ((MOONLIGHT_PROTOCOL_V1_TLV_TOP_LEVEL_FIELD_MAX + 1u) * \
   MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE)

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
    const MoonlightProtocolResult test_result_value = (expression); \
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
 * @brief Summarizes semantic events emitted by a stream parser.
 */
typedef struct StreamObservation {
  size_t lane_count;  ///< Number of lane-preface events.
  size_t envelope_begin_count;  ///< Number of envelope-begin events.
  size_t payload_event_count;  ///< Number of nonempty payload events.
  size_t envelope_end_count;  ///< Number of envelope-end events.
  MoonlightProtocolV1LanePreface lane;  ///< Last observed lane.
  MoonlightProtocolV1MessageEnvelope envelopes[TEST_OBSERVED_ENVELOPE_MAX];  ///< Begun envelopes.
  MoonlightProtocolResult message_results[TEST_OBSERVED_ENVELOPE_MAX];  ///< Per-envelope validation results.
  MoonlightProtocolResult active_message_result;  ///< Result expected on payload and end events.
  uint8_t payload[TEST_OBSERVED_PAYLOAD_MAX];  ///< Concatenated payload bytes.
  size_t payload_size;  ///< Number of concatenated payload bytes.
} StreamObservation;

/**
 * @brief Summarizes semantic events emitted by a TLV parser.
 */
typedef struct TlvObservation {
  MoonlightProtocolV1TlvField fields[TEST_OBSERVED_ENVELOPE_MAX];  ///< Begun fields.
  size_t field_begin_count;  ///< Number of field-begin events.
  size_t value_event_count;  ///< Number of nonempty value events.
  size_t field_end_count;  ///< Number of field-end events.
  uint8_t values[TEST_OBSERVED_PAYLOAD_MAX];  ///< Concatenated value bytes.
  size_t value_size;  ///< Number of concatenated value bytes.
} TlvObservation;

/**
 * @brief Encodes one Control lane preface.
 *
 * @param output Destination with room for one lane preface.
 * @return True on success.
 */
static bool encode_control_preface(uint8_t *output) {
  const MoonlightProtocolV1LanePreface preface = {
    .kind = MOONLIGHT_PROTOCOL_V1_LANE_CONTROL,
    .session_wire_id = 0,
  };

  TEST_RESULT(
    MoonlightProtocolV1EncodeLanePreface(
      &preface,
      output,
      MOONLIGHT_PROTOCOL_V1_LANE_PREFACE_SIZE
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  return true;
}

/**
 * @brief Encodes one envelope followed by its payload.
 *
 * @param envelope Host-order envelope whose length equals `payload_size`.
 * @param payload Payload bytes, or NULL for an empty payload.
 * @param payload_size Number of payload bytes.
 * @param output Destination buffer.
 * @param output_capacity Available output bytes.
 * @param encoded_size Receives the complete encoded size.
 * @return True on success.
 */
static bool encode_message(
  const MoonlightProtocolV1MessageEnvelope *envelope,
  const uint8_t *payload,
  size_t payload_size,
  uint8_t *output,
  size_t output_capacity,
  size_t *encoded_size
) {
  const size_t required =
    MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE + payload_size;

  TEST_CHECK(envelope != NULL);
  TEST_CHECK(envelope->payload_length == payload_size);
  TEST_CHECK(payload != NULL || payload_size == 0);
  TEST_CHECK(output != NULL);
  TEST_CHECK(encoded_size != NULL);
  TEST_CHECK(required <= output_capacity);
  TEST_RESULT(
    MoonlightProtocolV1EncodeMessageEnvelope(
      envelope,
      MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX,
      output,
      output_capacity
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  if (payload_size != 0) {
    memcpy(
      output + MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE,
      payload,
      payload_size
    );
  }
  *encoded_size = required;
  return true;
}

/**
 * @brief Records one stream parser event.
 *
 * @param observation Observation to update.
 * @param event Event to record.
 * @return True when the event is internally consistent and fits.
 */
static bool observe_stream_event(
  StreamObservation *observation,
  const MoonlightProtocolV1StreamEvent *event
) {
  TEST_CHECK(observation != NULL);
  TEST_CHECK(event != NULL);

  switch (event->type) {
    case MOONLIGHT_PROTOCOL_V1_STREAM_EVENT_NONE:
      TEST_CHECK(event->payload == NULL);
      TEST_CHECK(event->payload_size == 0);
      return true;
    case MOONLIGHT_PROTOCOL_V1_STREAM_EVENT_LANE_PREFACE:
      ++observation->lane_count;
      observation->lane = event->lane;
      TEST_CHECK(event->payload == NULL);
      TEST_CHECK(event->payload_size == 0);
      return true;
    case MOONLIGHT_PROTOCOL_V1_STREAM_EVENT_ENVELOPE_BEGIN:
      TEST_CHECK(
        observation->envelope_begin_count <
        TEST_OBSERVED_ENVELOPE_MAX
      );
      observation->envelopes[observation->envelope_begin_count] =
        event->envelope;
      observation->message_results[observation->envelope_begin_count] =
        event->message_result;
      observation->active_message_result = event->message_result;
      ++observation->envelope_begin_count;
      TEST_CHECK(event->payload == NULL);
      TEST_CHECK(event->payload_size == 0);
      return true;
    case MOONLIGHT_PROTOCOL_V1_STREAM_EVENT_PAYLOAD:
      TEST_CHECK(event->payload != NULL);
      TEST_CHECK(event->payload_size != 0);
      TEST_CHECK(
        event->message_result == observation->active_message_result
      );
      TEST_CHECK(
        event->payload_size <=
        TEST_OBSERVED_PAYLOAD_MAX - observation->payload_size
      );
      memcpy(
        observation->payload + observation->payload_size,
        event->payload,
        event->payload_size
      );
      observation->payload_size += event->payload_size;
      ++observation->payload_event_count;
      return true;
    case MOONLIGHT_PROTOCOL_V1_STREAM_EVENT_ENVELOPE_END:
      ++observation->envelope_end_count;
      TEST_CHECK(
        event->message_result == observation->active_message_result
      );
      TEST_CHECK(event->payload == NULL);
      TEST_CHECK(event->payload_size == 0);
      return true;
    default:
      TEST_CHECK(false);
      return false;
  }
}

/**
 * @brief Feeds one bounded slice and drains all immediately available events.
 *
 * @param parser Initialized stream parser.
 * @param input Slice bytes, or NULL for an empty slice.
 * @param input_size Number of bytes in the slice.
 * @param observation Event observation to update.
 * @return True when the entire slice is consumed without parser failure.
 */
static bool feed_stream_slice(
  MoonlightProtocolV1StreamParser *parser,
  const uint8_t *input,
  size_t input_size,
  StreamObservation *observation
) {
  size_t offset = 0;
  size_t iterations = 0;

  TEST_CHECK(parser != NULL);
  TEST_CHECK(input != NULL || input_size == 0);
  TEST_CHECK(observation != NULL);

  for (;;) {
    MoonlightProtocolV1StreamEvent event;
    const size_t available = input_size - offset;
    const uint8_t *next = available == 0 ? NULL : input + offset;
    size_t consumed = SIZE_MAX;

    TEST_RESULT(
      MoonlightProtocolV1StreamParserFeed(
        parser,
        next,
        available,
        &consumed,
        &event
      ),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    TEST_CHECK(consumed <= available);
    TEST_CHECK(observe_stream_event(observation, &event));
    offset += consumed;
    ++iterations;
    TEST_CHECK(iterations <= input_size + 16u);

    if (consumed == 0 && event.type == MOONLIGHT_PROTOCOL_V1_STREAM_EVENT_NONE) {
      break;
    }
  }

  TEST_CHECK(offset == input_size);
  return true;
}

/**
 * @brief Exercises one known envelope on an initiating or reverse lane half.
 *
 * @param lane Exact lane context.
 * @param reverse True for the preface-free acceptor-to-initiator half.
 * @param envelope Known empty-payload envelope.
 * @param expected Expected stream result while accepting the envelope.
 * @return True on success.
 */
static bool run_stream_lane_case(
  const MoonlightProtocolV1LanePreface *lane,
  bool reverse,
  const MoonlightProtocolV1MessageEnvelope *envelope,
  MoonlightProtocolResult expected
) {
  uint8_t preface_bytes[MOONLIGHT_PROTOCOL_V1_LANE_PREFACE_SIZE];
  uint8_t envelope_bytes[MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE];
  MoonlightProtocolV1StreamParser parser;
  StreamObservation observation = {0};

  TEST_CHECK(lane != NULL);
  TEST_CHECK(envelope != NULL);
  TEST_CHECK(envelope->payload_length == 0);
  TEST_RESULT(
    MoonlightProtocolV1EncodeLanePreface(
      lane,
      preface_bytes,
      sizeof(preface_bytes)
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeMessageEnvelope(
      envelope,
      MOONLIGHT_PROTOCOL_V1_BULK_PAYLOAD_MAX,
      envelope_bytes,
      sizeof(envelope_bytes)
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );

  if (reverse) {
    TEST_RESULT(
      MoonlightProtocolV1StreamParserInitializeReverse(
        &parser,
        lane,
        MOONLIGHT_PROTOCOL_V1_BULK_PAYLOAD_MAX
      ),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
  } else {
    TEST_RESULT(
      MoonlightProtocolV1StreamParserInitialize(
        &parser,
        MOONLIGHT_PROTOCOL_V1_BULK_PAYLOAD_MAX
      ),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    TEST_CHECK(
      feed_stream_slice(
        &parser,
        preface_bytes,
        sizeof(preface_bytes),
        &observation
      )
    );
    TEST_CHECK(observation.lane_count == 1);
  }

  if (expected == MOONLIGHT_PROTOCOL_RESULT_OK) {
    TEST_CHECK(
      feed_stream_slice(
        &parser,
        envelope_bytes,
        sizeof(envelope_bytes),
        &observation
      )
    );
    TEST_CHECK(observation.envelope_begin_count == 1);
    TEST_CHECK(observation.envelope_end_count == 1);
    TEST_CHECK(
      observation.message_results[0] ==
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    TEST_RESULT(
      MoonlightProtocolV1StreamParserFinish(&parser),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
  } else {
    MoonlightProtocolV1StreamEvent event;
    size_t consumed = SIZE_MAX;

    TEST_RESULT(
      MoonlightProtocolV1StreamParserFeed(
        &parser,
        envelope_bytes,
        sizeof(envelope_bytes),
        &consumed,
        &event
      ),
      expected
    );
    TEST_CHECK(consumed == sizeof(envelope_bytes));
    TEST_CHECK(event.type == MOONLIGHT_PROTOCOL_V1_STREAM_EVENT_NONE);
    TEST_RESULT(MoonlightProtocolV1StreamParserFinish(&parser), expected);
  }
  return true;
}

/**
 * @brief Exercises an unknown request on one initiating or reverse lane half.
 *
 * @param lane Exact lane context.
 * @param reverse True for the preface-free acceptor-to-initiator half.
 * @param accepted Whether the request must drain as an unsupported message.
 * @return True on success.
 */
static bool run_unknown_request_case(
  const MoonlightProtocolV1LanePreface *lane,
  bool reverse,
  bool accepted
) {
  const MoonlightProtocolV1MessageEnvelope request = {
    .message_type = MOONLIGHT_PROTOCOL_V1_MESSAGE_PING,
    .flags = 0,
    .payload_length = 0,
    .status = MOONLIGHT_PROTOCOL_V1_STATUS_OK,
    .correlation_id = 99,
  };
  uint8_t preface_bytes[MOONLIGHT_PROTOCOL_V1_LANE_PREFACE_SIZE];
  uint8_t envelope_bytes[MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE];
  MoonlightProtocolV1StreamParser parser;
  StreamObservation observation = {0};

  TEST_RESULT(
    MoonlightProtocolV1EncodeLanePreface(
      lane,
      preface_bytes,
      sizeof(preface_bytes)
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeMessageEnvelope(
      &request,
      MOONLIGHT_PROTOCOL_V1_BULK_PAYLOAD_MAX,
      envelope_bytes,
      sizeof(envelope_bytes)
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  envelope_bytes[4] = 0x7f;
  envelope_bytes[5] = 0xfb;

  if (reverse) {
    TEST_RESULT(
      MoonlightProtocolV1StreamParserInitializeReverse(
        &parser,
        lane,
        MOONLIGHT_PROTOCOL_V1_BULK_PAYLOAD_MAX
      ),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
  } else {
    TEST_RESULT(
      MoonlightProtocolV1StreamParserInitialize(
        &parser,
        MOONLIGHT_PROTOCOL_V1_BULK_PAYLOAD_MAX
      ),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    TEST_CHECK(
      feed_stream_slice(
        &parser,
        preface_bytes,
        sizeof(preface_bytes),
        &observation
      )
    );
  }

  if (accepted) {
    TEST_CHECK(
      feed_stream_slice(
        &parser,
        envelope_bytes,
        sizeof(envelope_bytes),
        &observation
      )
    );
    TEST_CHECK(observation.envelope_begin_count == 1);
    TEST_CHECK(observation.envelope_end_count == 1);
    TEST_CHECK(
      observation.message_results[0] ==
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    );
    TEST_RESULT(
      MoonlightProtocolV1StreamParserFinish(&parser),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
  } else {
    MoonlightProtocolV1StreamEvent event;
    size_t consumed;

    TEST_RESULT(
      MoonlightProtocolV1StreamParserFeed(
        &parser,
        envelope_bytes,
        sizeof(envelope_bytes),
        &consumed,
        &event
      ),
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    );
    TEST_CHECK(consumed == sizeof(envelope_bytes));
    TEST_RESULT(
      MoonlightProtocolV1StreamParserFinish(&parser),
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    );
  }
  return true;
}

/**
 * @brief Records one TLV parser event.
 *
 * @param observation Observation to update.
 * @param event Event to record.
 * @return True when the event is internally consistent and fits.
 */
static bool observe_tlv_event(
  TlvObservation *observation,
  const MoonlightProtocolV1TlvEvent *event
) {
  TEST_CHECK(observation != NULL);
  TEST_CHECK(event != NULL);

  switch (event->type) {
    case MOONLIGHT_PROTOCOL_V1_TLV_EVENT_NONE:
      TEST_CHECK(event->value == NULL);
      TEST_CHECK(event->value_size == 0);
      return true;
    case MOONLIGHT_PROTOCOL_V1_TLV_EVENT_FIELD_BEGIN:
      TEST_CHECK(
        observation->field_begin_count <
        TEST_OBSERVED_ENVELOPE_MAX
      );
      observation->fields[observation->field_begin_count++] = event->field;
      TEST_CHECK(event->value == NULL);
      TEST_CHECK(event->value_size == 0);
      return true;
    case MOONLIGHT_PROTOCOL_V1_TLV_EVENT_VALUE:
      TEST_CHECK(event->value != NULL);
      TEST_CHECK(event->value_size != 0);
      TEST_CHECK(
        event->value_size <=
        TEST_OBSERVED_PAYLOAD_MAX - observation->value_size
      );
      memcpy(
        observation->values + observation->value_size,
        event->value,
        event->value_size
      );
      observation->value_size += event->value_size;
      ++observation->value_event_count;
      return true;
    case MOONLIGHT_PROTOCOL_V1_TLV_EVENT_FIELD_END:
      ++observation->field_end_count;
      TEST_CHECK(event->value == NULL);
      TEST_CHECK(event->value_size == 0);
      return true;
    default:
      TEST_CHECK(false);
      return false;
  }
}

/**
 * @brief Feeds one bounded TLV slice and drains boundary events.
 *
 * @param parser Initialized TLV parser.
 * @param input Slice bytes, or NULL for an empty slice.
 * @param input_size Number of bytes in the slice.
 * @param observation Event observation to update.
 * @return True when the entire slice is consumed without parser failure.
 */
static bool feed_tlv_slice(
  MoonlightProtocolV1TlvParser *parser,
  const uint8_t *input,
  size_t input_size,
  TlvObservation *observation
) {
  size_t offset = 0;
  size_t iterations = 0;

  TEST_CHECK(parser != NULL);
  TEST_CHECK(input != NULL || input_size == 0);
  TEST_CHECK(observation != NULL);

  for (;;) {
    MoonlightProtocolV1TlvEvent event;
    const size_t available = input_size - offset;
    const uint8_t *next = available == 0 ? NULL : input + offset;
    size_t consumed = SIZE_MAX;

    TEST_RESULT(
      MoonlightProtocolV1TlvParserFeed(
        parser,
        next,
        available,
        &consumed,
        &event
      ),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    TEST_CHECK(consumed <= available);
    TEST_CHECK(observe_tlv_event(observation, &event));
    offset += consumed;
    ++iterations;
    TEST_CHECK(iterations <= input_size + 16u);

    if (consumed == 0 && event.type == MOONLIGHT_PROTOCOL_V1_TLV_EVENT_NONE) {
      break;
    }
  }

  TEST_CHECK(offset == input_size);
  return true;
}

/**
 * @brief Appends one manually encoded TLV header to a byte buffer.
 *
 * @param output Destination containing enough space.
 * @param offset Current write offset, updated on success.
 * @param field_id Field identifier.
 * @param flags Field flags.
 * @param field_length Declared value length.
 * @return True on success.
 */
static bool append_tlv_header(
  uint8_t *output,
  size_t *offset,
  uint16_t field_id,
  uint16_t flags,
  uint32_t field_length
) {
  const MoonlightProtocolV1TlvField field = {
    .field_id = field_id,
    .flags = flags,
    .field_length = field_length,
  };

  TEST_CHECK(output != NULL);
  TEST_CHECK(offset != NULL);
  TEST_RESULT(
    MoonlightProtocolV1EncodeTlvFieldHeader(
      &field,
      output + *offset,
      MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  *offset += MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE;
  return true;
}

/**
 * @brief Tests fixed header golden bytes and every envelope form.
 *
 * @return True on success.
 */
static bool test_fixed_headers_and_forms(void) {
  static const uint8_t expected_preface[MOONLIGHT_PROTOCOL_V1_LANE_PREFACE_SIZE] = {
    0x53,
    0x53,
    0x51,
    0x31,
    0x10,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
  };
  static const MoonlightProtocolV1MessageEnvelope forms[] = {
    {
      .message_type = MOONLIGHT_PROTOCOL_V1_MESSAGE_PING,
      .flags = 0,
      .payload_length = 3,
      .status = MOONLIGHT_PROTOCOL_V1_STATUS_OK,
      .correlation_id = UINT64_C(0x0102030405060708),
    },
    {
      .message_type = MOONLIGHT_PROTOCOL_V1_MESSAGE_PING,
      .flags = MOONLIGHT_PROTOCOL_V1_ENVELOPE_FLAG_RESPONSE,
      .payload_length = 3,
      .status = MOONLIGHT_PROTOCOL_V1_STATUS_OK,
      .correlation_id = UINT64_C(0x1112131415161718),
    },
    {
      .message_type = MOONLIGHT_PROTOCOL_V1_MESSAGE_PING,
      .flags = MOONLIGHT_PROTOCOL_V1_ENVELOPE_FLAG_RESPONSE |
               MOONLIGHT_PROTOCOL_V1_ENVELOPE_FLAG_ERROR,
      .payload_length = 3,
      .status = MOONLIGHT_PROTOCOL_V1_STATUS_RATE_LIMITED,
      .correlation_id = UINT64_C(0x2122232425262728),
    },
    {
      .message_type = MOONLIGHT_PROTOCOL_V1_MESSAGE_GOAWAY,
      .flags = MOONLIGHT_PROTOCOL_V1_ENVELOPE_FLAG_NOTIFICATION,
      .payload_length = 0,
      .status = MOONLIGHT_PROTOCOL_V1_STATUS_OK,
      .correlation_id = 0,
    },
  };
  uint8_t bytes[MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE];
  uint8_t preface_bytes[MOONLIGHT_PROTOCOL_V1_LANE_PREFACE_SIZE];
  MoonlightProtocolV1LanePreface decoded_preface;
  size_t index;

  TEST_CHECK(encode_control_preface(preface_bytes));
  TEST_CHECK(memcmp(preface_bytes, expected_preface, sizeof(preface_bytes)) == 0);
  memset(&decoded_preface, 0xa5, sizeof(decoded_preface));
  TEST_RESULT(
    MoonlightProtocolV1DecodeLanePreface(
      preface_bytes,
      sizeof(preface_bytes),
      &decoded_preface
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(decoded_preface.kind == MOONLIGHT_PROTOCOL_V1_LANE_CONTROL);
  TEST_CHECK(decoded_preface.session_wire_id == 0);

  for (index = 0; index < sizeof(forms) / sizeof(forms[0]); ++index) {
    MoonlightProtocolV1MessageEnvelope decoded;

    memset(bytes, 0xa5, sizeof(bytes));
    TEST_RESULT(
      MoonlightProtocolV1EncodeMessageEnvelope(
        &forms[index],
        MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX,
        bytes,
        sizeof(bytes)
      ),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    TEST_CHECK(bytes[0] == 0x53);
    TEST_CHECK(bytes[1] == 0x51);
    TEST_CHECK(bytes[2] == 0x4d);
    TEST_CHECK(bytes[3] == 0x31);
    TEST_CHECK(bytes[4] == (uint8_t) ((uint16_t) forms[index].message_type >> 8u));
    TEST_CHECK(bytes[5] == (uint8_t) forms[index].message_type);
    TEST_CHECK(bytes[6] == (uint8_t) (forms[index].flags >> 8u));
    TEST_CHECK(bytes[7] == (uint8_t) forms[index].flags);
    TEST_CHECK(bytes[8] == 0);
    TEST_CHECK(bytes[9] == 0);
    TEST_CHECK(bytes[10] == 0);
    TEST_CHECK(bytes[11] == (uint8_t) forms[index].payload_length);

    memset(&decoded, 0xa5, sizeof(decoded));
    TEST_RESULT(
      MoonlightProtocolV1DecodeMessageEnvelope(
        bytes,
        sizeof(bytes),
        MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX,
        &decoded
      ),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    TEST_CHECK(decoded.message_type == forms[index].message_type);
    TEST_CHECK(decoded.flags == forms[index].flags);
    TEST_CHECK(decoded.payload_length == forms[index].payload_length);
    TEST_CHECK(decoded.status == forms[index].status);
    TEST_CHECK(decoded.correlation_id == forms[index].correlation_id);
  }

  TEST_CHECK(MoonlightProtocolV1MessageTypeIsKnown(MOONLIGHT_PROTOCOL_V1_MESSAGE_CLIENT_HELLO));
  TEST_CHECK(MoonlightProtocolV1MessageTypeIsKnown(MOONLIGHT_PROTOCOL_V1_MESSAGE_GET_ASSET));
  TEST_CHECK(!MoonlightProtocolV1MessageTypeIsKnown(0));
  TEST_CHECK(!MoonlightProtocolV1MessageTypeIsKnown(0x00ff));
  return true;
}

/**
 * @brief Verifies the narrow unknown-request error-response encoder.
 *
 * @return True on success.
 */
static bool test_unknown_request_response_encoder(void) {
  const uint16_t unknown_type = 0x7ffb;
  const uint64_t correlation_id = UINT64_C(0x0102030405060708);
  uint8_t output[MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE];
  uint8_t unchanged[sizeof(output)];
  MoonlightProtocolV1MessageEnvelope decoded;
  MoonlightProtocolV1MessageEnvelope decoded_before;

  memset(output, 0xa5, sizeof(output));
  memcpy(unchanged, output, sizeof(output));
  TEST_RESULT(
    MoonlightProtocolV1EncodeUnknownRequestUnsupportedResponse(
      unknown_type,
      correlation_id,
      NULL,
      sizeof(output)
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeUnknownRequestUnsupportedResponse(
      MOONLIGHT_PROTOCOL_V1_MESSAGE_PING,
      correlation_id,
      output,
      sizeof(output)
    ),
    MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeUnknownRequestUnsupportedResponse(
      unknown_type,
      0,
      output,
      sizeof(output)
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeUnknownRequestUnsupportedResponse(
      unknown_type,
      correlation_id,
      output,
      sizeof(output) - 1u
    ),
    MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL
  );
  TEST_CHECK(memcmp(output, unchanged, sizeof(output)) == 0);

  TEST_RESULT(
    MoonlightProtocolV1EncodeUnknownRequestUnsupportedResponse(
      unknown_type,
      correlation_id,
      output,
      sizeof(output)
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(output[0] == 0x53 && output[1] == 0x51);
  TEST_CHECK(output[2] == 0x4d && output[3] == 0x31);
  TEST_CHECK(output[4] == 0x7f && output[5] == 0xfb);
  TEST_CHECK(
    output[6] == 0 &&
    output[7] ==
      (MOONLIGHT_PROTOCOL_V1_ENVELOPE_FLAG_RESPONSE |
       MOONLIGHT_PROTOCOL_V1_ENVELOPE_FLAG_ERROR)
  );
  TEST_CHECK(output[8] == 0 && output[11] == 0);
  TEST_CHECK(
    output[12] == 0 &&
    output[15] == MOONLIGHT_PROTOCOL_V1_STATUS_UNSUPPORTED_MESSAGE
  );
  TEST_CHECK(
    output[16] == 0x01 &&
    output[17] == 0x02 &&
    output[22] == 0x07 &&
    output[23] == 0x08
  );

  memset(&decoded, 0xa5, sizeof(decoded));
  memcpy(&decoded_before, &decoded, sizeof(decoded));
  TEST_RESULT(
    MoonlightProtocolV1DecodeMessageEnvelope(
      output,
      sizeof(output),
      MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  TEST_CHECK(
    memcmp(&decoded, &decoded_before, sizeof(decoded)) == 0
  );

  TEST_RESULT(
    MoonlightProtocolV1EncodeUnknownRequestUnsupportedResponse(
      0,
      1,
      output,
      sizeof(output)
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(output[4] == 0 && output[5] == 0);
  return true;
}

/**
 * @brief Tests fixed-header validation and failure atomicity.
 *
 * @return True on success.
 */
static bool test_fixed_header_errors_are_atomic(void) {
  const MoonlightProtocolV1LanePreface control = {
    .kind = MOONLIGHT_PROTOCOL_V1_LANE_CONTROL,
    .session_wire_id = 0,
  };
  const MoonlightProtocolV1LanePreface reliable_input = {
    .kind = MOONLIGHT_PROTOCOL_V1_LANE_RELIABLE_INPUT,
    .session_wire_id = UINT32_C(0x11223344),
  };
  MoonlightProtocolV1MessageEnvelope envelope = {
    .message_type = MOONLIGHT_PROTOCOL_V1_MESSAGE_PING,
    .flags = 0,
    .payload_length = 1,
    .status = MOONLIGHT_PROTOCOL_V1_STATUS_OK,
    .correlation_id = 1,
  };
  MoonlightProtocolV1TlvField field = {
    .field_id = 1,
    .flags = 0,
    .field_length = 2,
  };
  uint8_t bytes[MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE];
  uint8_t before[sizeof(bytes)];
  MoonlightProtocolV1LanePreface decoded_lane;
  MoonlightProtocolV1LanePreface decoded_lane_before;
  MoonlightProtocolV1MessageEnvelope decoded_envelope;
  MoonlightProtocolV1MessageEnvelope decoded_envelope_before;
  MoonlightProtocolV1TlvField decoded_field;
  MoonlightProtocolV1TlvField decoded_field_before;

  memset(bytes, 0xa5, sizeof(bytes));
  memcpy(before, bytes, sizeof(bytes));
  TEST_RESULT(
    MoonlightProtocolV1EncodeLanePreface(
      NULL,
      bytes,
      MOONLIGHT_PROTOCOL_V1_LANE_PREFACE_SIZE
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_CHECK(memcmp(bytes, before, sizeof(bytes)) == 0);
  TEST_RESULT(
    MoonlightProtocolV1EncodeLanePreface(
      &control,
      bytes,
      MOONLIGHT_PROTOCOL_V1_LANE_PREFACE_SIZE - 1u
    ),
    MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL
  );
  TEST_CHECK(memcmp(bytes, before, sizeof(bytes)) == 0);
  TEST_RESULT(
    MoonlightProtocolV1EncodeLanePreface(
      &reliable_input,
      bytes,
      MOONLIGHT_PROTOCOL_V1_LANE_PREFACE_SIZE
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );

  memset(&decoded_lane, 0xa5, sizeof(decoded_lane));
  memcpy(&decoded_lane_before, &decoded_lane, sizeof(decoded_lane));
  TEST_RESULT(
    MoonlightProtocolV1DecodeLanePreface(
      bytes,
      MOONLIGHT_PROTOCOL_V1_LANE_PREFACE_SIZE - 1u,
      &decoded_lane
    ),
    MOONLIGHT_PROTOCOL_RESULT_TRUNCATED
  );
  TEST_CHECK(memcmp(&decoded_lane, &decoded_lane_before, sizeof(decoded_lane)) == 0);
  bytes[5] = 1;
  TEST_RESULT(
    MoonlightProtocolV1DecodeLanePreface(
      bytes,
      MOONLIGHT_PROTOCOL_V1_LANE_PREFACE_SIZE,
      &decoded_lane
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  TEST_CHECK(memcmp(&decoded_lane, &decoded_lane_before, sizeof(decoded_lane)) == 0);

  memset(bytes, 0xa5, sizeof(bytes));
  memcpy(before, bytes, sizeof(bytes));
  envelope.flags = MOONLIGHT_PROTOCOL_V1_ENVELOPE_FLAG_ERROR;
  TEST_RESULT(
    MoonlightProtocolV1EncodeMessageEnvelope(
      &envelope,
      MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX,
      bytes,
      sizeof(bytes)
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  TEST_CHECK(memcmp(bytes, before, sizeof(bytes)) == 0);
  envelope.flags = 0;
  envelope.payload_length = 2;
  TEST_RESULT(
    MoonlightProtocolV1EncodeMessageEnvelope(
      &envelope,
      1,
      bytes,
      sizeof(bytes)
    ),
    MOONLIGHT_PROTOCOL_RESULT_LIMIT_EXCEEDED
  );
  TEST_CHECK(memcmp(bytes, before, sizeof(bytes)) == 0);
  envelope.payload_length = 1;
  TEST_RESULT(
    MoonlightProtocolV1EncodeMessageEnvelope(
      &envelope,
      MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX,
      bytes,
      sizeof(bytes)
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );

  memset(&decoded_envelope, 0xa5, sizeof(decoded_envelope));
  memcpy(
    &decoded_envelope_before,
    &decoded_envelope,
    sizeof(decoded_envelope)
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeMessageEnvelope(
      bytes,
      MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE - 1u,
      MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX,
      &decoded_envelope
    ),
    MOONLIGHT_PROTOCOL_RESULT_TRUNCATED
  );
  TEST_CHECK(
    memcmp(
      &decoded_envelope,
      &decoded_envelope_before,
      sizeof(decoded_envelope)
    ) == 0
  );
  bytes[0] ^= 1u;
  TEST_RESULT(
    MoonlightProtocolV1DecodeMessageEnvelope(
      bytes,
      MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE,
      MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX,
      &decoded_envelope
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  TEST_CHECK(
    memcmp(
      &decoded_envelope,
      &decoded_envelope_before,
      sizeof(decoded_envelope)
    ) == 0
  );

  memset(bytes, 0xa5, sizeof(bytes));
  memcpy(before, bytes, sizeof(bytes));
  field.flags = 2;
  TEST_RESULT(
    MoonlightProtocolV1EncodeTlvFieldHeader(
      &field,
      bytes,
      MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  TEST_CHECK(memcmp(bytes, before, sizeof(bytes)) == 0);
  field.flags = 0;
  TEST_RESULT(
    MoonlightProtocolV1EncodeTlvFieldHeader(
      &field,
      bytes,
      MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  memset(&decoded_field, 0xa5, sizeof(decoded_field));
  memcpy(&decoded_field_before, &decoded_field, sizeof(decoded_field));
  TEST_RESULT(
    MoonlightProtocolV1DecodeTlvFieldHeader(
      bytes,
      MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE - 1u,
      &decoded_field
    ),
    MOONLIGHT_PROTOCOL_RESULT_TRUNCATED
  );
  TEST_CHECK(memcmp(&decoded_field, &decoded_field_before, sizeof(decoded_field)) == 0);
  bytes[3] = 2;
  TEST_RESULT(
    MoonlightProtocolV1DecodeTlvFieldHeader(
      bytes,
      MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
      &decoded_field
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  TEST_CHECK(memcmp(&decoded_field, &decoded_field_before, sizeof(decoded_field)) == 0);
  return true;
}

/**
 * @brief Tests the complete fixed-header validation matrix.
 *
 * @return True on success.
 */
static bool test_fixed_header_validation_matrix(void) {
  MoonlightProtocolV1LanePreface lane = {
    .kind = MOONLIGHT_PROTOCOL_V1_LANE_CONTROL,
    .session_wire_id = 0,
  };
  MoonlightProtocolV1MessageEnvelope envelope = {
    .message_type = MOONLIGHT_PROTOCOL_V1_MESSAGE_PING,
    .flags = 0,
    .payload_length = 0,
    .status = MOONLIGHT_PROTOCOL_V1_STATUS_OK,
    .correlation_id = 1,
  };
  MoonlightProtocolV1TlvField field = {
    .field_id = 1,
    .flags = 0,
    .field_length = 0,
  };
  uint8_t lane_bytes[MOONLIGHT_PROTOCOL_V1_LANE_PREFACE_SIZE];
  uint8_t envelope_bytes[MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE];
  uint8_t tlv_bytes[MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE];
  MoonlightProtocolV1LanePreface decoded_lane;
  MoonlightProtocolV1MessageEnvelope decoded_envelope;
  MoonlightProtocolV1TlvField decoded_field;

  TEST_RESULT(
    MoonlightProtocolV1EncodeLanePreface(
      &lane,
      NULL,
      sizeof(lane_bytes)
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  lane.kind = (MoonlightProtocolV1LaneKind) 0x7f;
  TEST_RESULT(
    MoonlightProtocolV1EncodeLanePreface(
      &lane,
      lane_bytes,
      sizeof(lane_bytes)
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  lane.kind = MOONLIGHT_PROTOCOL_V1_LANE_BULK;
  lane.session_wire_id = 1;
  TEST_RESULT(
    MoonlightProtocolV1EncodeLanePreface(
      &lane,
      lane_bytes,
      sizeof(lane_bytes)
    ),
    MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
  );
  lane.kind = MOONLIGHT_PROTOCOL_V1_LANE_RELIABLE_INPUT;
  lane.session_wire_id = 0;
  TEST_RESULT(
    MoonlightProtocolV1EncodeLanePreface(
      &lane,
      lane_bytes,
      sizeof(lane_bytes)
    ),
    MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
  );
  lane.kind = MOONLIGHT_PROTOCOL_V1_LANE_CONTROL;
  TEST_CHECK(encode_control_preface(lane_bytes));
  TEST_RESULT(
    MoonlightProtocolV1DecodeLanePreface(
      NULL,
      sizeof(lane_bytes),
      &decoded_lane
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeLanePreface(
      lane_bytes,
      sizeof(lane_bytes),
      NULL
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  lane_bytes[6] = 0;
  lane_bytes[7] = 15;
  TEST_RESULT(
    MoonlightProtocolV1DecodeLanePreface(
      lane_bytes,
      sizeof(lane_bytes),
      &decoded_lane
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  TEST_CHECK(encode_control_preface(lane_bytes));
  lane_bytes[12] = 1;
  TEST_RESULT(
    MoonlightProtocolV1DecodeLanePreface(
      lane_bytes,
      sizeof(lane_bytes),
      &decoded_lane
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  TEST_CHECK(encode_control_preface(lane_bytes));
  lane_bytes[4] = 0x7f;
  TEST_RESULT(
    MoonlightProtocolV1DecodeLanePreface(
      lane_bytes,
      sizeof(lane_bytes),
      &decoded_lane
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  TEST_CHECK(encode_control_preface(lane_bytes));
  lane_bytes[11] = 1;
  TEST_RESULT(
    MoonlightProtocolV1DecodeLanePreface(
      lane_bytes,
      sizeof(lane_bytes),
      &decoded_lane
    ),
    MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
  );

  TEST_RESULT(
    MoonlightProtocolV1EncodeMessageEnvelope(
      NULL,
      MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX,
      envelope_bytes,
      sizeof(envelope_bytes)
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeMessageEnvelope(
      &envelope,
      MOONLIGHT_PROTOCOL_V1_BULK_PAYLOAD_MAX + 1u,
      envelope_bytes,
      sizeof(envelope_bytes)
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeMessageEnvelope(
      &envelope,
      MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX,
      NULL,
      sizeof(envelope_bytes)
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeMessageEnvelope(
      &envelope,
      MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX,
      envelope_bytes,
      sizeof(envelope_bytes) - 1u
    ),
    MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL
  );

  envelope.message_type = (MoonlightProtocolV1MessageType) 0x00ff;
  TEST_RESULT(
    MoonlightProtocolV1EncodeMessageEnvelope(
      &envelope,
      MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX,
      envelope_bytes,
      sizeof(envelope_bytes)
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  envelope.message_type = (MoonlightProtocolV1MessageType) UINT32_C(0x10001);
  TEST_RESULT(
    MoonlightProtocolV1EncodeMessageEnvelope(
      &envelope,
      MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX,
      envelope_bytes,
      sizeof(envelope_bytes)
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  envelope.message_type = (MoonlightProtocolV1MessageType) -1;
  TEST_RESULT(
    MoonlightProtocolV1EncodeMessageEnvelope(
      &envelope,
      MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX,
      envelope_bytes,
      sizeof(envelope_bytes)
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  envelope.message_type = MOONLIGHT_PROTOCOL_V1_MESSAGE_PING;
  envelope.flags = 0x8000;
  TEST_RESULT(
    MoonlightProtocolV1EncodeMessageEnvelope(
      &envelope,
      MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX,
      envelope_bytes,
      sizeof(envelope_bytes)
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  envelope.flags = 0;
  envelope.correlation_id = 0;
  TEST_RESULT(
    MoonlightProtocolV1EncodeMessageEnvelope(
      &envelope,
      MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX,
      envelope_bytes,
      sizeof(envelope_bytes)
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  envelope.flags = MOONLIGHT_PROTOCOL_V1_ENVELOPE_FLAG_RESPONSE;
  envelope.status = MOONLIGHT_PROTOCOL_V1_STATUS_INVALID_MESSAGE;
  envelope.correlation_id = 1;
  TEST_RESULT(
    MoonlightProtocolV1EncodeMessageEnvelope(
      &envelope,
      MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX,
      envelope_bytes,
      sizeof(envelope_bytes)
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  envelope.flags = MOONLIGHT_PROTOCOL_V1_ENVELOPE_FLAG_RESPONSE |
                   MOONLIGHT_PROTOCOL_V1_ENVELOPE_FLAG_ERROR;
  envelope.status = MOONLIGHT_PROTOCOL_V1_STATUS_OK;
  TEST_RESULT(
    MoonlightProtocolV1EncodeMessageEnvelope(
      &envelope,
      MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX,
      envelope_bytes,
      sizeof(envelope_bytes)
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  envelope.status = (MoonlightProtocolV1MessageStatus) 14;
  TEST_RESULT(
    MoonlightProtocolV1EncodeMessageEnvelope(
      &envelope,
      MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX,
      envelope_bytes,
      sizeof(envelope_bytes)
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  envelope.status = MOONLIGHT_PROTOCOL_V1_STATUS_INTERNAL;
  envelope.correlation_id = 0;
  TEST_RESULT(
    MoonlightProtocolV1EncodeMessageEnvelope(
      &envelope,
      MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX,
      envelope_bytes,
      sizeof(envelope_bytes)
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  envelope.message_type = MOONLIGHT_PROTOCOL_V1_MESSAGE_GOAWAY;
  envelope.flags = MOONLIGHT_PROTOCOL_V1_ENVELOPE_FLAG_NOTIFICATION;
  envelope.status = MOONLIGHT_PROTOCOL_V1_STATUS_OK;
  envelope.correlation_id = 1;
  TEST_RESULT(
    MoonlightProtocolV1EncodeMessageEnvelope(
      &envelope,
      MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX,
      envelope_bytes,
      sizeof(envelope_bytes)
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  envelope.correlation_id = 0;
  envelope.message_type = MOONLIGHT_PROTOCOL_V1_MESSAGE_PING;
  TEST_RESULT(
    MoonlightProtocolV1EncodeMessageEnvelope(
      &envelope,
      MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX,
      envelope_bytes,
      sizeof(envelope_bytes)
    ),
    MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
  );
  envelope.message_type = MOONLIGHT_PROTOCOL_V1_MESSAGE_GOAWAY;
  envelope.flags = 0;
  envelope.correlation_id = 1;
  TEST_RESULT(
    MoonlightProtocolV1EncodeMessageEnvelope(
      &envelope,
      MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX,
      envelope_bytes,
      sizeof(envelope_bytes)
    ),
    MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
  );

  envelope.message_type = MOONLIGHT_PROTOCOL_V1_MESSAGE_PING;
  envelope.flags = 0;
  envelope.correlation_id = 1;
  TEST_RESULT(
    MoonlightProtocolV1EncodeMessageEnvelope(
      &envelope,
      MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX,
      envelope_bytes,
      sizeof(envelope_bytes)
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeMessageEnvelope(
      NULL,
      sizeof(envelope_bytes),
      MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX,
      &decoded_envelope
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeMessageEnvelope(
      envelope_bytes,
      sizeof(envelope_bytes),
      MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX,
      NULL
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeMessageEnvelope(
      envelope_bytes,
      sizeof(envelope_bytes),
      MOONLIGHT_PROTOCOL_V1_BULK_PAYLOAD_MAX + 1u,
      &decoded_envelope
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );

  TEST_RESULT(
    MoonlightProtocolV1EncodeTlvFieldHeader(
      NULL,
      tlv_bytes,
      sizeof(tlv_bytes)
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeTlvFieldHeader(
      &field,
      NULL,
      sizeof(tlv_bytes)
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeTlvFieldHeader(
      &field,
      tlv_bytes,
      sizeof(tlv_bytes) - 1u
    ),
    MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeTlvFieldHeader(
      &field,
      tlv_bytes,
      sizeof(tlv_bytes)
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeTlvFieldHeader(
      NULL,
      sizeof(tlv_bytes),
      &decoded_field
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeTlvFieldHeader(
      tlv_bytes,
      sizeof(tlv_bytes),
      NULL
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  return true;
}

/**
 * @brief Tests every two-slice split and one-byte stream delivery.
 *
 * @return True on success.
 */
static bool test_stream_all_splits_and_bytes(void) {
  static const uint8_t payload[] = {0x10, 0x20, 0x30, 0x40, 0x50};
  const MoonlightProtocolV1MessageEnvelope envelope = {
    .message_type = MOONLIGHT_PROTOCOL_V1_MESSAGE_PING,
    .flags = 0,
    .payload_length = sizeof(payload),
    .status = MOONLIGHT_PROTOCOL_V1_STATUS_OK,
    .correlation_id = UINT64_C(0x0102030405060708),
  };
  uint8_t stream[MOONLIGHT_PROTOCOL_V1_LANE_PREFACE_SIZE + MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE + sizeof(payload)];
  size_t message_size;
  size_t split;

  TEST_CHECK(encode_control_preface(stream));
  TEST_CHECK(
    encode_message(
      &envelope,
      payload,
      sizeof(payload),
      stream + MOONLIGHT_PROTOCOL_V1_LANE_PREFACE_SIZE,
      sizeof(stream) - MOONLIGHT_PROTOCOL_V1_LANE_PREFACE_SIZE,
      &message_size
    )
  );
  TEST_CHECK(
    message_size ==
    MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE + sizeof(payload)
  );

  for (split = 0; split <= sizeof(stream); ++split) {
    MoonlightProtocolV1StreamParser parser;
    StreamObservation observation = {0};

    TEST_RESULT(
      MoonlightProtocolV1StreamParserInitialize(
        &parser,
        MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX
      ),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    TEST_CHECK(feed_stream_slice(&parser, stream, split, &observation));
    TEST_CHECK(
      feed_stream_slice(
        &parser,
        stream + split,
        sizeof(stream) - split,
        &observation
      )
    );
    TEST_RESULT(
      MoonlightProtocolV1StreamParserFinish(&parser),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    TEST_CHECK(observation.lane_count == 1);
    TEST_CHECK(observation.lane.kind == MOONLIGHT_PROTOCOL_V1_LANE_CONTROL);
    TEST_CHECK(observation.envelope_begin_count == 1);
    TEST_CHECK(observation.envelope_end_count == 1);
    TEST_CHECK(observation.envelopes[0].message_type == envelope.message_type);
    TEST_CHECK(observation.payload_size == sizeof(payload));
    TEST_CHECK(memcmp(observation.payload, payload, sizeof(payload)) == 0);
  }

  {
    MoonlightProtocolV1StreamParser parser;
    StreamObservation observation = {0};
    size_t index;

    TEST_RESULT(
      MoonlightProtocolV1StreamParserInitialize(
        &parser,
        MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX
      ),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    for (index = 0; index < sizeof(stream); ++index) {
      TEST_CHECK(feed_stream_slice(&parser, stream + index, 1, &observation));
    }
    TEST_RESULT(
      MoonlightProtocolV1StreamParserFinish(&parser),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    TEST_CHECK(observation.lane_count == 1);
    TEST_CHECK(observation.envelope_begin_count == 1);
    TEST_CHECK(observation.envelope_end_count == 1);
    TEST_CHECK(observation.payload_size == sizeof(payload));
    TEST_CHECK(memcmp(observation.payload, payload, sizeof(payload)) == 0);
  }

  return true;
}

/**
 * @brief Tests coalesced envelopes and the preface-free reverse half.
 *
 * @return True on success.
 */
static bool test_stream_coalesced_and_reverse(void) {
  static const uint8_t request_payload[] = {0xaa, 0xbb, 0xcc};
  const MoonlightProtocolV1MessageEnvelope request = {
    .message_type = MOONLIGHT_PROTOCOL_V1_MESSAGE_PING,
    .flags = 0,
    .payload_length = sizeof(request_payload),
    .status = MOONLIGHT_PROTOCOL_V1_STATUS_OK,
    .correlation_id = 7,
  };
  const MoonlightProtocolV1MessageEnvelope response = {
    .message_type = MOONLIGHT_PROTOCOL_V1_MESSAGE_PING,
    .flags = MOONLIGHT_PROTOCOL_V1_ENVELOPE_FLAG_RESPONSE,
    .payload_length = 0,
    .status = MOONLIGHT_PROTOCOL_V1_STATUS_OK,
    .correlation_id = 7,
  };
  const MoonlightProtocolV1LanePreface lane = {
    .kind = MOONLIGHT_PROTOCOL_V1_LANE_CONTROL,
    .session_wire_id = 0,
  };
  uint8_t stream[128];
  size_t offset = 0;
  size_t encoded_size;

  TEST_CHECK(encode_control_preface(stream));
  offset += MOONLIGHT_PROTOCOL_V1_LANE_PREFACE_SIZE;
  TEST_CHECK(
    encode_message(
      &request,
      request_payload,
      sizeof(request_payload),
      stream + offset,
      sizeof(stream) - offset,
      &encoded_size
    )
  );
  offset += encoded_size;
  TEST_CHECK(
    encode_message(
      &response,
      NULL,
      0,
      stream + offset,
      sizeof(stream) - offset,
      &encoded_size
    )
  );
  offset += encoded_size;

  {
    MoonlightProtocolV1StreamParser parser;
    StreamObservation observation = {0};

    TEST_RESULT(
      MoonlightProtocolV1StreamParserInitialize(
        &parser,
        MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX
      ),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    TEST_CHECK(feed_stream_slice(&parser, stream, offset, &observation));
    TEST_RESULT(
      MoonlightProtocolV1StreamParserFinish(&parser),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    TEST_CHECK(observation.lane_count == 1);
    TEST_CHECK(observation.envelope_begin_count == 2);
    TEST_CHECK(observation.envelope_end_count == 2);
    TEST_CHECK(observation.envelopes[0].message_type == request.message_type);
    TEST_CHECK(
      observation.envelopes[1].message_type ==
      response.message_type
    );
    TEST_CHECK(observation.payload_size == sizeof(request_payload));
    TEST_CHECK(
      memcmp(
        observation.payload,
        request_payload,
        sizeof(request_payload)
      ) == 0
    );
  }

  {
    MoonlightProtocolV1StreamParser parser;
    StreamObservation observation = {0};
    const uint8_t *reverse = stream + MOONLIGHT_PROTOCOL_V1_LANE_PREFACE_SIZE;
    const size_t reverse_size =
      offset - MOONLIGHT_PROTOCOL_V1_LANE_PREFACE_SIZE;

    TEST_RESULT(
      MoonlightProtocolV1StreamParserInitializeReverse(
        &parser,
        &lane,
        MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX
      ),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    TEST_CHECK(feed_stream_slice(&parser, reverse, reverse_size, &observation));
    TEST_RESULT(
      MoonlightProtocolV1StreamParserFinish(&parser),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    TEST_CHECK(observation.lane_count == 0);
    TEST_CHECK(observation.envelope_begin_count == 2);
    TEST_CHECK(observation.envelope_end_count == 2);
    TEST_CHECK(observation.payload_size == sizeof(request_payload));
  }

  return true;
}

/**
 * @brief Tests the lane and initiating/reverse message-form matrix.
 *
 * @return True on success.
 */
static bool test_stream_half_role_matrix(void) {
  const MoonlightProtocolV1LanePreface pair_lane = {
    .kind = MOONLIGHT_PROTOCOL_V1_LANE_PAIR_CONTROL,
    .session_wire_id = 0,
  };
  const MoonlightProtocolV1LanePreface control_lane = {
    .kind = MOONLIGHT_PROTOCOL_V1_LANE_CONTROL,
    .session_wire_id = 0,
  };
  const MoonlightProtocolV1LanePreface reliable_lane = {
    .kind = MOONLIGHT_PROTOCOL_V1_LANE_RELIABLE_INPUT,
    .session_wire_id = UINT32_C(0x11223344),
  };
  const MoonlightProtocolV1LanePreface bulk_lane = {
    .kind = MOONLIGHT_PROTOCOL_V1_LANE_BULK,
    .session_wire_id = 0,
  };
  const MoonlightProtocolV1MessageEnvelope pair_request = {
    .message_type = MOONLIGHT_PROTOCOL_V1_MESSAGE_CLIENT_PROOF,
    .flags = 0,
    .payload_length = 0,
    .status = MOONLIGHT_PROTOCOL_V1_STATUS_OK,
    .correlation_id = 1,
  };
  const MoonlightProtocolV1MessageEnvelope pair_response = {
    .message_type = MOONLIGHT_PROTOCOL_V1_MESSAGE_CLIENT_PROOF,
    .flags = MOONLIGHT_PROTOCOL_V1_ENVELOPE_FLAG_RESPONSE,
    .payload_length = 0,
    .status = MOONLIGHT_PROTOCOL_V1_STATUS_OK,
    .correlation_id = 1,
  };
  const MoonlightProtocolV1MessageEnvelope control_request = {
    .message_type = MOONLIGHT_PROTOCOL_V1_MESSAGE_GET_HOST_INFO,
    .flags = 0,
    .payload_length = 0,
    .status = MOONLIGHT_PROTOCOL_V1_STATUS_OK,
    .correlation_id = 2,
  };
  const MoonlightProtocolV1MessageEnvelope control_response = {
    .message_type = MOONLIGHT_PROTOCOL_V1_MESSAGE_GET_HOST_INFO,
    .flags = MOONLIGHT_PROTOCOL_V1_ENVELOPE_FLAG_RESPONSE,
    .payload_length = 0,
    .status = MOONLIGHT_PROTOCOL_V1_STATUS_OK,
    .correlation_id = 2,
  };
  const MoonlightProtocolV1MessageEnvelope ping_request = {
    .message_type = MOONLIGHT_PROTOCOL_V1_MESSAGE_PING,
    .flags = 0,
    .payload_length = 0,
    .status = MOONLIGHT_PROTOCOL_V1_STATUS_OK,
    .correlation_id = 3,
  };
  const MoonlightProtocolV1MessageEnvelope ping_response = {
    .message_type = MOONLIGHT_PROTOCOL_V1_MESSAGE_PING,
    .flags = MOONLIGHT_PROTOCOL_V1_ENVELOPE_FLAG_RESPONSE,
    .payload_length = 0,
    .status = MOONLIGHT_PROTOCOL_V1_STATUS_OK,
    .correlation_id = 3,
  };
  const MoonlightProtocolV1MessageEnvelope notification = {
    .message_type = MOONLIGHT_PROTOCOL_V1_MESSAGE_GOAWAY,
    .flags = MOONLIGHT_PROTOCOL_V1_ENVELOPE_FLAG_NOTIFICATION,
    .payload_length = 0,
    .status = MOONLIGHT_PROTOCOL_V1_STATUS_OK,
    .correlation_id = 0,
  };
  const MoonlightProtocolV1MessageEnvelope reliable_notification = {
    .message_type = MOONLIGHT_PROTOCOL_V1_MESSAGE_INPUT_RELIABLE,
    .flags = MOONLIGHT_PROTOCOL_V1_ENVELOPE_FLAG_NOTIFICATION,
    .payload_length = 0,
    .status = MOONLIGHT_PROTOCOL_V1_STATUS_OK,
    .correlation_id = 0,
  };
  const MoonlightProtocolV1MessageEnvelope bulk_request = {
    .message_type = MOONLIGHT_PROTOCOL_V1_MESSAGE_GET_ASSET,
    .flags = 0,
    .payload_length = 0,
    .status = MOONLIGHT_PROTOCOL_V1_STATUS_OK,
    .correlation_id = 4,
  };
  const MoonlightProtocolV1MessageEnvelope bulk_response = {
    .message_type = MOONLIGHT_PROTOCOL_V1_MESSAGE_GET_ASSET,
    .flags = MOONLIGHT_PROTOCOL_V1_ENVELOPE_FLAG_RESPONSE,
    .payload_length = 0,
    .status = MOONLIGHT_PROTOCOL_V1_STATUS_OK,
    .correlation_id = 4,
  };
  MoonlightProtocolV1StreamParser parser;

  TEST_CHECK(
    run_stream_lane_case(
      &pair_lane,
      false,
      &pair_request,
      MOONLIGHT_PROTOCOL_RESULT_OK
    )
  );
  TEST_CHECK(
    run_stream_lane_case(
      &pair_lane,
      true,
      &pair_response,
      MOONLIGHT_PROTOCOL_RESULT_OK
    )
  );
  TEST_CHECK(
    run_stream_lane_case(
      &pair_lane,
      false,
      &pair_response,
      MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
    )
  );
  TEST_CHECK(
    run_stream_lane_case(
      &pair_lane,
      true,
      &pair_request,
      MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
    )
  );
  TEST_CHECK(
    run_stream_lane_case(
      &pair_lane,
      false,
      &ping_request,
      MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
    )
  );
  TEST_CHECK(
    run_stream_lane_case(
      &control_lane,
      false,
      &control_request,
      MOONLIGHT_PROTOCOL_RESULT_OK
    )
  );
  TEST_CHECK(
    run_stream_lane_case(
      &control_lane,
      true,
      &control_response,
      MOONLIGHT_PROTOCOL_RESULT_OK
    )
  );
  TEST_CHECK(
    run_stream_lane_case(
      &control_lane,
      true,
      &control_request,
      MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
    )
  );
  TEST_CHECK(
    run_stream_lane_case(
      &control_lane,
      false,
      &control_response,
      MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
    )
  );
  TEST_CHECK(
    run_stream_lane_case(
      &control_lane,
      false,
      &notification,
      MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
    )
  );
  TEST_CHECK(
    run_stream_lane_case(
      &control_lane,
      true,
      &notification,
      MOONLIGHT_PROTOCOL_RESULT_OK
    )
  );
  TEST_CHECK(
    run_stream_lane_case(
      &control_lane,
      false,
      &ping_request,
      MOONLIGHT_PROTOCOL_RESULT_OK
    )
  );
  TEST_CHECK(
    run_stream_lane_case(
      &control_lane,
      true,
      &ping_request,
      MOONLIGHT_PROTOCOL_RESULT_OK
    )
  );
  TEST_CHECK(
    run_stream_lane_case(
      &control_lane,
      false,
      &ping_response,
      MOONLIGHT_PROTOCOL_RESULT_OK
    )
  );
  TEST_CHECK(
    run_stream_lane_case(
      &control_lane,
      true,
      &ping_response,
      MOONLIGHT_PROTOCOL_RESULT_OK
    )
  );
  TEST_CHECK(
    run_stream_lane_case(
      &reliable_lane,
      false,
      &reliable_notification,
      MOONLIGHT_PROTOCOL_RESULT_OK
    )
  );
  TEST_CHECK(
    run_stream_lane_case(
      &reliable_lane,
      false,
      &ping_request,
      MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
    )
  );
  TEST_RESULT(
    MoonlightProtocolV1StreamParserInitializeReverse(
      &parser,
      &reliable_lane,
      MOONLIGHT_PROTOCOL_V1_RELIABLE_INPUT_PAYLOAD_MAX
    ),
    MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
  );
  TEST_CHECK(
    run_stream_lane_case(
      &bulk_lane,
      false,
      &bulk_request,
      MOONLIGHT_PROTOCOL_RESULT_OK
    )
  );
  TEST_CHECK(
    run_stream_lane_case(
      &bulk_lane,
      true,
      &bulk_response,
      MOONLIGHT_PROTOCOL_RESULT_OK
    )
  );
  TEST_CHECK(
    run_stream_lane_case(
      &bulk_lane,
      false,
      &bulk_response,
      MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
    )
  );
  TEST_CHECK(
    run_stream_lane_case(
      &bulk_lane,
      true,
      &bulk_request,
      MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
    )
  );
  TEST_CHECK(
    run_stream_lane_case(
      &bulk_lane,
      false,
      &ping_request,
      MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
    )
  );
  return true;
}

/**
 * @brief Tests bounded unknown-request draining and subsequent lane reuse.
 *
 * @return True on success.
 */
static bool test_stream_unknown_request_drain(void) {
  static const uint8_t unknown_payload[] = {0xde, 0xad, 0xbe};
  const MoonlightProtocolV1LanePreface lane = {
    .kind = MOONLIGHT_PROTOCOL_V1_LANE_CONTROL,
    .session_wire_id = 0,
  };
  const MoonlightProtocolV1LanePreface pair_lane = {
    .kind = MOONLIGHT_PROTOCOL_V1_LANE_PAIR_CONTROL,
    .session_wire_id = 0,
  };
  const MoonlightProtocolV1LanePreface reliable_lane = {
    .kind = MOONLIGHT_PROTOCOL_V1_LANE_RELIABLE_INPUT,
    .session_wire_id = UINT32_C(0x11223344),
  };
  const MoonlightProtocolV1LanePreface bulk_lane = {
    .kind = MOONLIGHT_PROTOCOL_V1_LANE_BULK,
    .session_wire_id = 0,
  };
  const MoonlightProtocolV1MessageEnvelope unknown_template = {
    .message_type = MOONLIGHT_PROTOCOL_V1_MESSAGE_PING,
    .flags = 0,
    .payload_length = sizeof(unknown_payload),
    .status = MOONLIGHT_PROTOCOL_V1_STATUS_OK,
    .correlation_id = 10,
  };
  const MoonlightProtocolV1MessageEnvelope ping = {
    .message_type = MOONLIGHT_PROTOCOL_V1_MESSAGE_PING,
    .flags = 0,
    .payload_length = 0,
    .status = MOONLIGHT_PROTOCOL_V1_STATUS_OK,
    .correlation_id = 11,
  };
  uint8_t stream[96];
  size_t offset = 0;
  size_t encoded_size;

  TEST_CHECK(encode_control_preface(stream));
  offset += MOONLIGHT_PROTOCOL_V1_LANE_PREFACE_SIZE;
  TEST_CHECK(
    encode_message(
      &unknown_template,
      unknown_payload,
      sizeof(unknown_payload),
      stream + offset,
      sizeof(stream) - offset,
      &encoded_size
    )
  );
  stream[offset + 4u] = 0x7f;
  stream[offset + 5u] = 0xfe;
  offset += encoded_size;
  TEST_CHECK(
    encode_message(
      &ping,
      NULL,
      0,
      stream + offset,
      sizeof(stream) - offset,
      &encoded_size
    )
  );
  offset += encoded_size;

  {
    MoonlightProtocolV1StreamParser parser;
    StreamObservation observation = {0};

    TEST_RESULT(
      MoonlightProtocolV1StreamParserInitialize(
        &parser,
        MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX
      ),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    TEST_CHECK(feed_stream_slice(&parser, stream, offset, &observation));
    TEST_RESULT(
      MoonlightProtocolV1StreamParserFinish(&parser),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    TEST_CHECK(observation.envelope_begin_count == 2);
    TEST_CHECK(observation.envelope_end_count == 2);
    TEST_CHECK(
      observation.message_results[0] ==
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    );
    TEST_CHECK(
      observation.message_results[1] ==
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    TEST_CHECK(observation.envelopes[0].message_type == 0x7ffe);
    TEST_CHECK(observation.payload_size == sizeof(unknown_payload));
    TEST_CHECK(
      memcmp(
        observation.payload,
        unknown_payload,
        sizeof(unknown_payload)
      ) == 0
    );
  }

  {
    MoonlightProtocolV1MessageEnvelope decoded;
    MoonlightProtocolV1MessageEnvelope before;
    const uint8_t *unknown_header =
      stream + MOONLIGHT_PROTOCOL_V1_LANE_PREFACE_SIZE;

    memset(&decoded, 0xa5, sizeof(decoded));
    memcpy(&before, &decoded, sizeof(decoded));
    TEST_RESULT(
      MoonlightProtocolV1DecodeMessageEnvelope(
        unknown_header,
        MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE,
        MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX,
        &decoded
      ),
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    );
    TEST_CHECK(memcmp(&decoded, &before, sizeof(decoded)) == 0);
  }

  {
    MoonlightProtocolV1MessageEnvelope response = {
      .message_type = MOONLIGHT_PROTOCOL_V1_MESSAGE_PING,
      .flags = MOONLIGHT_PROTOCOL_V1_ENVELOPE_FLAG_RESPONSE,
      .payload_length = 0,
      .status = MOONLIGHT_PROTOCOL_V1_STATUS_OK,
      .correlation_id = 12,
    };
    MoonlightProtocolV1StreamParser parser;
    MoonlightProtocolV1StreamEvent event;
    uint8_t header[MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE];
    size_t consumed;

    TEST_RESULT(
      MoonlightProtocolV1EncodeMessageEnvelope(
        &response,
        MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX,
        header,
        sizeof(header)
      ),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    header[4] = 0x7f;
    header[5] = 0xfd;
    TEST_RESULT(
      MoonlightProtocolV1StreamParserInitializeReverse(
        &parser,
        &lane,
        MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX
      ),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    TEST_RESULT(
      MoonlightProtocolV1StreamParserFeed(
        &parser,
        header,
        sizeof(header),
        &consumed,
        &event
      ),
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    );
    TEST_CHECK(consumed == sizeof(header));
    TEST_RESULT(
      MoonlightProtocolV1StreamParserFinish(&parser),
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    );
  }

  {
    MoonlightProtocolV1MessageEnvelope notification = {
      .message_type = MOONLIGHT_PROTOCOL_V1_MESSAGE_GOAWAY,
      .flags = MOONLIGHT_PROTOCOL_V1_ENVELOPE_FLAG_NOTIFICATION,
      .payload_length = 0,
      .status = MOONLIGHT_PROTOCOL_V1_STATUS_OK,
      .correlation_id = 0,
    };
    MoonlightProtocolV1StreamParser parser;
    MoonlightProtocolV1StreamEvent event;
    uint8_t header[MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE];
    size_t consumed;

    TEST_RESULT(
      MoonlightProtocolV1EncodeMessageEnvelope(
        &notification,
        MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX,
        header,
        sizeof(header)
      ),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    header[4] = 0x7f;
    header[5] = 0xfc;
    TEST_RESULT(
      MoonlightProtocolV1StreamParserInitializeReverse(
        &parser,
        &lane,
        MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX
      ),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    TEST_RESULT(
      MoonlightProtocolV1StreamParserFeed(
        &parser,
        header,
        sizeof(header),
        &consumed,
        &event
      ),
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    );
    TEST_CHECK(consumed == sizeof(header));
    TEST_RESULT(
      MoonlightProtocolV1StreamParserFinish(&parser),
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    );
  }
  TEST_CHECK(run_unknown_request_case(&pair_lane, false, true));
  TEST_CHECK(run_unknown_request_case(&pair_lane, true, false));
  TEST_CHECK(run_unknown_request_case(&lane, true, true));
  TEST_CHECK(run_unknown_request_case(&reliable_lane, false, false));
  TEST_CHECK(run_unknown_request_case(&bulk_lane, false, true));
  TEST_CHECK(run_unknown_request_case(&bulk_lane, true, false));
  return true;
}

/**
 * @brief Tests atomic payload-limit updates at both clean boundary states.
 *
 * @return True on success.
 */
static bool test_stream_payload_limit_boundary_updates(void) {
  static const uint8_t payload[] = {0xaa, 0xbb};
  const MoonlightProtocolV1LanePreface lane = {
    .kind = MOONLIGHT_PROTOCOL_V1_LANE_CONTROL,
    .session_wire_id = 0,
  };
  MoonlightProtocolV1MessageEnvelope envelope = {
    .message_type = MOONLIGHT_PROTOCOL_V1_MESSAGE_PING,
    .flags = 0,
    .payload_length = sizeof(payload),
    .status = MOONLIGHT_PROTOCOL_V1_STATUS_OK,
    .correlation_id = 1,
  };
  uint8_t preface[MOONLIGHT_PROTOCOL_V1_LANE_PREFACE_SIZE];
  uint8_t header[MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE];
  MoonlightProtocolV1StreamParser parser;
  MoonlightProtocolV1StreamParser before;
  MoonlightProtocolV1StreamEvent event;
  size_t consumed;

  TEST_CHECK(encode_control_preface(preface));
  TEST_RESULT(
    MoonlightProtocolV1EncodeMessageEnvelope(
      &envelope,
      MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX,
      header,
      sizeof(header)
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_RESULT(
    MoonlightProtocolV1StreamParserSetPayloadLimitAtBoundary(NULL, 1),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );

  TEST_RESULT(
    MoonlightProtocolV1StreamParserInitialize(
      &parser,
      MOONLIGHT_PROTOCOL_V1_CLIENT_HELLO_REQUEST_PAYLOAD_MAX
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  memcpy(&before, &parser, sizeof(parser));
  TEST_RESULT(
    MoonlightProtocolV1StreamParserSetPayloadLimitAtBoundary(
      &parser,
      MOONLIGHT_PROTOCOL_V1_STREAMING_CLIENT_PROOF_REQUEST_PAYLOAD_MAX
    ),
    MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
  );
  TEST_CHECK(memcmp(&parser, &before, sizeof(parser)) == 0);

  TEST_RESULT(
    MoonlightProtocolV1StreamParserFeed(
      &parser,
      preface,
      1,
      &consumed,
      &event
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(consumed == 1);
  TEST_CHECK(event.type == MOONLIGHT_PROTOCOL_V1_STREAM_EVENT_NONE);
  memcpy(&before, &parser, sizeof(parser));
  TEST_RESULT(
    MoonlightProtocolV1StreamParserSetPayloadLimitAtBoundary(
      &parser,
      MOONLIGHT_PROTOCOL_V1_STREAMING_CLIENT_PROOF_REQUEST_PAYLOAD_MAX
    ),
    MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
  );
  TEST_CHECK(memcmp(&parser, &before, sizeof(parser)) == 0);

  TEST_RESULT(
    MoonlightProtocolV1StreamParserInitialize(
      &parser,
      MOONLIGHT_PROTOCOL_V1_CLIENT_HELLO_REQUEST_PAYLOAD_MAX
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_RESULT(
    MoonlightProtocolV1StreamParserFeed(
      &parser,
      preface,
      sizeof(preface),
      &consumed,
      &event
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(consumed == sizeof(preface));
  TEST_CHECK(event.type == MOONLIGHT_PROTOCOL_V1_STREAM_EVENT_LANE_PREFACE);
  memcpy(&before, &parser, sizeof(parser));
  before.payload_limit =
    MOONLIGHT_PROTOCOL_V1_STREAMING_CLIENT_PROOF_REQUEST_PAYLOAD_MAX;
  TEST_RESULT(
    MoonlightProtocolV1StreamParserSetPayloadLimitAtBoundary(
      &parser,
      MOONLIGHT_PROTOCOL_V1_STREAMING_CLIENT_PROOF_REQUEST_PAYLOAD_MAX
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(memcmp(&parser, &before, sizeof(parser)) == 0);

  memcpy(&before, &parser, sizeof(parser));
  TEST_RESULT(
    MoonlightProtocolV1StreamParserSetPayloadLimitAtBoundary(
      &parser,
      MOONLIGHT_PROTOCOL_V1_BULK_PAYLOAD_MAX + 1u
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_CHECK(memcmp(&parser, &before, sizeof(parser)) == 0);

  TEST_RESULT(
    MoonlightProtocolV1StreamParserFeed(
      &parser,
      header,
      1,
      &consumed,
      &event
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(consumed == 1);
  TEST_CHECK(event.type == MOONLIGHT_PROTOCOL_V1_STREAM_EVENT_NONE);
  memcpy(&before, &parser, sizeof(parser));
  TEST_RESULT(
    MoonlightProtocolV1StreamParserSetPayloadLimitAtBoundary(&parser, 1),
    MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
  );
  TEST_CHECK(memcmp(&parser, &before, sizeof(parser)) == 0);

  TEST_RESULT(
    MoonlightProtocolV1StreamParserInitializeReverse(
      &parser,
      &lane,
      MOONLIGHT_PROTOCOL_V1_CLIENT_HELLO_REQUEST_PAYLOAD_MAX
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  memcpy(&before, &parser, sizeof(parser));
  before.payload_limit =
    MOONLIGHT_PROTOCOL_V1_STREAMING_CLIENT_PROOF_REQUEST_PAYLOAD_MAX;
  TEST_RESULT(
    MoonlightProtocolV1StreamParserSetPayloadLimitAtBoundary(
      &parser,
      MOONLIGHT_PROTOCOL_V1_STREAMING_CLIENT_PROOF_REQUEST_PAYLOAD_MAX
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(memcmp(&parser, &before, sizeof(parser)) == 0);

  TEST_RESULT(
    MoonlightProtocolV1StreamParserFeed(
      &parser,
      header,
      sizeof(header),
      &consumed,
      &event
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(consumed == sizeof(header));
  TEST_CHECK(event.type == MOONLIGHT_PROTOCOL_V1_STREAM_EVENT_ENVELOPE_BEGIN);
  memcpy(&before, &parser, sizeof(parser));
  TEST_RESULT(
    MoonlightProtocolV1StreamParserSetPayloadLimitAtBoundary(&parser, 1),
    MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
  );
  TEST_CHECK(memcmp(&parser, &before, sizeof(parser)) == 0);

  TEST_RESULT(
    MoonlightProtocolV1StreamParserFeed(
      &parser,
      payload,
      1,
      &consumed,
      &event
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(consumed == 1);
  TEST_CHECK(event.type == MOONLIGHT_PROTOCOL_V1_STREAM_EVENT_PAYLOAD);
  memcpy(&before, &parser, sizeof(parser));
  TEST_RESULT(
    MoonlightProtocolV1StreamParserSetPayloadLimitAtBoundary(&parser, 1),
    MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
  );
  TEST_CHECK(memcmp(&parser, &before, sizeof(parser)) == 0);

  TEST_RESULT(
    MoonlightProtocolV1StreamParserFeed(
      &parser,
      payload + 1u,
      1,
      &consumed,
      &event
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(consumed == 1);
  TEST_CHECK(event.type == MOONLIGHT_PROTOCOL_V1_STREAM_EVENT_PAYLOAD);
  memcpy(&before, &parser, sizeof(parser));
  before.payload_limit = 400u;
  TEST_RESULT(
    MoonlightProtocolV1StreamParserSetPayloadLimitAtBoundary(&parser, 400u),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(memcmp(&parser, &before, sizeof(parser)) == 0);

  TEST_RESULT(
    MoonlightProtocolV1StreamParserFeed(
      &parser,
      NULL,
      0,
      &consumed,
      &event
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(consumed == 0);
  TEST_CHECK(event.type == MOONLIGHT_PROTOCOL_V1_STREAM_EVENT_ENVELOPE_END);
  memcpy(&before, &parser, sizeof(parser));
  before.payload_limit = 500u;
  TEST_RESULT(
    MoonlightProtocolV1StreamParserSetPayloadLimitAtBoundary(&parser, 500u),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(memcmp(&parser, &before, sizeof(parser)) == 0);

  TEST_RESULT(
    MoonlightProtocolV1StreamParserSetPayloadLimitAtBoundary(
      &parser,
      MOONLIGHT_PROTOCOL_V1_BULK_PAYLOAD_MAX
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  envelope.payload_length = MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX + 1u;
  TEST_RESULT(
    MoonlightProtocolV1EncodeMessageEnvelope(
      &envelope,
      MOONLIGHT_PROTOCOL_V1_BULK_PAYLOAD_MAX,
      header,
      sizeof(header)
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_RESULT(
    MoonlightProtocolV1StreamParserFeed(
      &parser,
      header,
      sizeof(header),
      &consumed,
      &event
    ),
    MOONLIGHT_PROTOCOL_RESULT_LIMIT_EXCEEDED
  );
  TEST_CHECK(consumed == sizeof(header));
  TEST_CHECK(event.type == MOONLIGHT_PROTOCOL_V1_STREAM_EVENT_NONE);
  memcpy(&before, &parser, sizeof(parser));
  TEST_RESULT(
    MoonlightProtocolV1StreamParserSetPayloadLimitAtBoundary(&parser, 1),
    MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
  );
  TEST_CHECK(memcmp(&parser, &before, sizeof(parser)) == 0);
  return true;
}

/**
 * @brief Tests FIN at every byte and every complete framing boundary.
 *
 * @return True on success.
 */
static bool test_stream_finish_boundaries(void) {
  static const uint8_t payload[] = {1, 2, 3, 4, 5};
  const MoonlightProtocolV1MessageEnvelope envelope = {
    .message_type = MOONLIGHT_PROTOCOL_V1_MESSAGE_PING,
    .flags = 0,
    .payload_length = sizeof(payload),
    .status = MOONLIGHT_PROTOCOL_V1_STATUS_OK,
    .correlation_id = 1,
  };
  uint8_t stream[MOONLIGHT_PROTOCOL_V1_LANE_PREFACE_SIZE + MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE + sizeof(payload)];
  size_t encoded_size;
  size_t prefix;

  TEST_CHECK(encode_control_preface(stream));
  TEST_CHECK(
    encode_message(
      &envelope,
      payload,
      sizeof(payload),
      stream + MOONLIGHT_PROTOCOL_V1_LANE_PREFACE_SIZE,
      sizeof(stream) - MOONLIGHT_PROTOCOL_V1_LANE_PREFACE_SIZE,
      &encoded_size
    )
  );
  TEST_CHECK(
    encoded_size ==
    MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE + sizeof(payload)
  );

  for (prefix = 0; prefix <= sizeof(stream); ++prefix) {
    MoonlightProtocolV1StreamParser parser;
    StreamObservation observation = {0};
    const MoonlightProtocolResult expected =
      prefix == MOONLIGHT_PROTOCOL_V1_LANE_PREFACE_SIZE ||
          prefix == sizeof(stream) ?
        MOONLIGHT_PROTOCOL_RESULT_OK :
        MOONLIGHT_PROTOCOL_RESULT_TRUNCATED;

    TEST_RESULT(
      MoonlightProtocolV1StreamParserInitialize(
        &parser,
        MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX
      ),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    TEST_CHECK(feed_stream_slice(&parser, stream, prefix, &observation));
    TEST_RESULT(MoonlightProtocolV1StreamParserFinish(&parser), expected);
  }

  {
    MoonlightProtocolV1StreamParser parser;
    const MoonlightProtocolV1LanePreface lane = {
      .kind = MOONLIGHT_PROTOCOL_V1_LANE_CONTROL,
      .session_wire_id = 0,
    };

    TEST_RESULT(
      MoonlightProtocolV1StreamParserInitializeReverse(
        &parser,
        &lane,
        MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX
      ),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    TEST_RESULT(
      MoonlightProtocolV1StreamParserFinish(&parser),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
  }
  return true;
}

/**
 * @brief Tests stream limits, lane checks, stickiness, and local atomicity.
 *
 * @return True on success.
 */
static bool test_stream_failures(void) {
  const MoonlightProtocolV1LanePreface lane = {
    .kind = MOONLIGHT_PROTOCOL_V1_LANE_CONTROL,
    .session_wire_id = 0,
  };
  MoonlightProtocolV1MessageEnvelope envelope = {
    .message_type = MOONLIGHT_PROTOCOL_V1_MESSAGE_PING,
    .flags = 0,
    .payload_length = 2,
    .status = MOONLIGHT_PROTOCOL_V1_STATUS_OK,
    .correlation_id = 1,
  };
  uint8_t bytes[MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE];

  TEST_RESULT(
    MoonlightProtocolV1StreamParserInitialize(
      NULL,
      MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1StreamParserInitializeReverse(
      NULL,
      &lane,
      MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1StreamParserFinish(NULL),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );

  {
    MoonlightProtocolV1StreamParser parser;
    MoonlightProtocolV1StreamParser before;

    memset(&parser, 0xa5, sizeof(parser));
    memcpy(&before, &parser, sizeof(parser));
    TEST_RESULT(
      MoonlightProtocolV1StreamParserInitialize(
        &parser,
        MOONLIGHT_PROTOCOL_V1_BULK_PAYLOAD_MAX + 1u
      ),
      MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
    );
    TEST_CHECK(memcmp(&parser, &before, sizeof(parser)) == 0);
    TEST_RESULT(
      MoonlightProtocolV1StreamParserInitializeReverse(
        &parser,
        NULL,
        MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX
      ),
      MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
    );
    TEST_CHECK(memcmp(&parser, &before, sizeof(parser)) == 0);
    TEST_RESULT(
      MoonlightProtocolV1StreamParserInitializeReverse(
        &parser,
        &lane,
        MOONLIGHT_PROTOCOL_V1_BULK_PAYLOAD_MAX + 1u
      ),
      MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
    );
    TEST_CHECK(memcmp(&parser, &before, sizeof(parser)) == 0);
  }

  {
    uint8_t preface[MOONLIGHT_PROTOCOL_V1_LANE_PREFACE_SIZE];
    MoonlightProtocolV1StreamParser parser;
    MoonlightProtocolV1StreamEvent event;
    size_t consumed;

    TEST_CHECK(encode_control_preface(preface));
    preface[0] ^= 1u;
    TEST_RESULT(
      MoonlightProtocolV1StreamParserInitialize(
        &parser,
        MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX
      ),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    TEST_RESULT(
      MoonlightProtocolV1StreamParserFeed(
        &parser,
        preface,
        sizeof(preface),
        &consumed,
        &event
      ),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    );
    TEST_CHECK(consumed == sizeof(preface));
    TEST_CHECK(event.type == MOONLIGHT_PROTOCOL_V1_STREAM_EVENT_NONE);
  }

  {
    MoonlightProtocolV1StreamParser parser;
    MoonlightProtocolV1StreamParser parser_before;
    MoonlightProtocolV1StreamEvent event;
    MoonlightProtocolV1StreamEvent event_before;
    size_t consumed = 123;

    TEST_RESULT(
      MoonlightProtocolV1StreamParserInitializeReverse(
        &parser,
        &lane,
        MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX
      ),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    memcpy(&parser_before, &parser, sizeof(parser));
    memset(&event, 0xa5, sizeof(event));
    memcpy(&event_before, &event, sizeof(event));
    TEST_RESULT(
      MoonlightProtocolV1StreamParserFeed(
        &parser,
        bytes,
        sizeof(bytes),
        NULL,
        &event
      ),
      MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
    );
    TEST_CHECK(memcmp(&parser, &parser_before, sizeof(parser)) == 0);
    TEST_CHECK(memcmp(&event, &event_before, sizeof(event)) == 0);
    TEST_CHECK(consumed == 123);
  }

  TEST_RESULT(
    MoonlightProtocolV1EncodeMessageEnvelope(
      &envelope,
      MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX,
      bytes,
      sizeof(bytes)
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  bytes[0] ^= 1u;
  {
    MoonlightProtocolV1StreamParser parser;
    MoonlightProtocolV1StreamEvent event;
    size_t consumed;

    TEST_RESULT(
      MoonlightProtocolV1StreamParserInitializeReverse(
        &parser,
        &lane,
        MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX
      ),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    TEST_RESULT(
      MoonlightProtocolV1StreamParserFeed(
        &parser,
        bytes,
        sizeof(bytes),
        &consumed,
        &event
      ),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    );
    TEST_CHECK(consumed == sizeof(bytes));
    TEST_CHECK(event.type == MOONLIGHT_PROTOCOL_V1_STREAM_EVENT_NONE);
    consumed = SIZE_MAX;
    memset(&event, 0xa5, sizeof(event));
    TEST_RESULT(
      MoonlightProtocolV1StreamParserFeed(
        &parser,
        NULL,
        0,
        &consumed,
        &event
      ),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    );
    TEST_CHECK(consumed == 0);
    TEST_CHECK(event.type == MOONLIGHT_PROTOCOL_V1_STREAM_EVENT_NONE);
    TEST_RESULT(
      MoonlightProtocolV1StreamParserFinish(&parser),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    );
  }

  envelope.payload_length = 2;
  TEST_RESULT(
    MoonlightProtocolV1EncodeMessageEnvelope(
      &envelope,
      MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX,
      bytes,
      sizeof(bytes)
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  {
    MoonlightProtocolV1StreamParser parser;
    MoonlightProtocolV1StreamEvent event;
    size_t consumed;

    TEST_RESULT(
      MoonlightProtocolV1StreamParserInitializeReverse(&parser, &lane, 1),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    TEST_RESULT(
      MoonlightProtocolV1StreamParserFeed(
        &parser,
        bytes,
        sizeof(bytes),
        &consumed,
        &event
      ),
      MOONLIGHT_PROTOCOL_RESULT_LIMIT_EXCEEDED
    );
    TEST_CHECK(consumed == sizeof(bytes));
  }

  envelope.message_type = MOONLIGHT_PROTOCOL_V1_MESSAGE_GET_ASSET;
  envelope.payload_length = 0;
  TEST_RESULT(
    MoonlightProtocolV1EncodeMessageEnvelope(
      &envelope,
      MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX,
      bytes,
      sizeof(bytes)
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  {
    MoonlightProtocolV1StreamParser parser;
    MoonlightProtocolV1StreamEvent event;
    size_t consumed;

    TEST_RESULT(
      MoonlightProtocolV1StreamParserInitializeReverse(
        &parser,
        &lane,
        MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX
      ),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    TEST_RESULT(
      MoonlightProtocolV1StreamParserFeed(
        &parser,
        bytes,
        sizeof(bytes),
        &consumed,
        &event
      ),
      MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH
    );
  }

  envelope.message_type = MOONLIGHT_PROTOCOL_V1_MESSAGE_PING;
  TEST_RESULT(
    MoonlightProtocolV1EncodeMessageEnvelope(
      &envelope,
      MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX,
      bytes,
      sizeof(bytes)
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  {
    MoonlightProtocolV1StreamParser parser;
    MoonlightProtocolV1StreamEvent event;
    size_t consumed;

    TEST_RESULT(
      MoonlightProtocolV1StreamParserInitializeReverse(
        &parser,
        &lane,
        MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX
      ),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );

    /*
     * Exercise the defensive unknown-lane paths after simulated corruption of
     * caller-owned parser state. Normal API inputs reject this lane earlier.
     */
    parser.lane.kind = (MoonlightProtocolV1LaneKind) 0x7f;
    TEST_RESULT(
      MoonlightProtocolV1StreamParserFeed(
        &parser,
        bytes,
        sizeof(bytes),
        &consumed,
        &event
      ),
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    );
    TEST_CHECK(consumed == sizeof(bytes));
    TEST_CHECK(event.type == MOONLIGHT_PROTOCOL_V1_STREAM_EVENT_NONE);
    TEST_RESULT(
      MoonlightProtocolV1StreamParserFinish(&parser),
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    );
  }
  return true;
}

/**
 * @brief Builds the canonical multi-field TLV payload used by split tests.
 *
 * @param output Destination buffer.
 * @param output_capacity Available bytes.
 * @param output_size Receives the encoded payload size.
 * @return True on success.
 */
static bool build_test_tlv_payload(
  uint8_t *output,
  size_t output_capacity,
  size_t *output_size
) {
  static const uint8_t one[] = {0xaa};
  static const uint8_t two[] = {0xbb, 0xcc};
  static const uint8_t three[] = {0xdd};
  MoonlightProtocolV1TlvWriter writer;

  TEST_RESULT(
    MoonlightProtocolV1TlvWriterInitialize(
      &writer,
      output,
      output_capacity,
      0
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_RESULT(
    MoonlightProtocolV1TlvWriterAppend(&writer, 1, 0, one, sizeof(one)),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_RESULT(
    MoonlightProtocolV1TlvWriterAppend(
      &writer,
      2,
      MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED,
      two,
      sizeof(two)
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_RESULT(
    MoonlightProtocolV1TlvWriterAppend(
      &writer,
      2,
      MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED,
      three,
      sizeof(three)
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_RESULT(
    MoonlightProtocolV1TlvWriterAppend(&writer, 3, 0, NULL, 0),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  *output_size = MoonlightProtocolV1TlvWriterSize(&writer);
  return true;
}

/**
 * @brief Tests TLV writer/parser round trips at every split and one byte at a time.
 *
 * @return True on success.
 */
static bool test_tlv_round_trip_splits(void) {
  static const uint8_t expected_values[] = {0xaa, 0xbb, 0xcc, 0xdd};
  uint8_t payload[64];
  size_t payload_size;
  size_t split;

  TEST_CHECK(build_test_tlv_payload(payload, sizeof(payload), &payload_size));
  TEST_CHECK(payload_size == 4u * MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + 4u);

  for (split = 0; split <= payload_size; ++split) {
    MoonlightProtocolV1TlvParser parser;
    TlvObservation observation = {0};

    TEST_RESULT(
      MoonlightProtocolV1TlvParserInitialize(
        &parser,
        (uint32_t) payload_size,
        0
      ),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    TEST_CHECK(feed_tlv_slice(&parser, payload, split, &observation));
    TEST_CHECK(
      feed_tlv_slice(
        &parser,
        payload + split,
        payload_size - split,
        &observation
      )
    );
    TEST_CHECK(MoonlightProtocolV1TlvParserIsComplete(&parser));
    TEST_RESULT(
      MoonlightProtocolV1TlvParserFinish(&parser),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    TEST_CHECK(observation.field_begin_count == 4);
    TEST_CHECK(observation.field_end_count == 4);
    TEST_CHECK(observation.fields[0].field_id == 1);
    TEST_CHECK(observation.fields[1].field_id == 2);
    TEST_CHECK(observation.fields[2].field_id == 2);
    TEST_CHECK(observation.fields[3].field_id == 3);
    TEST_CHECK(observation.value_size == sizeof(expected_values));
    TEST_CHECK(
      memcmp(
        observation.values,
        expected_values,
        sizeof(expected_values)
      ) == 0
    );
  }

  {
    MoonlightProtocolV1TlvParser parser;
    TlvObservation observation = {0};
    size_t index;

    TEST_RESULT(
      MoonlightProtocolV1TlvParserInitialize(
        &parser,
        (uint32_t) payload_size,
        0
      ),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    for (index = 0; index < payload_size; ++index) {
      TEST_CHECK(feed_tlv_slice(&parser, payload + index, 1, &observation));
    }
    TEST_CHECK(MoonlightProtocolV1TlvParserIsComplete(&parser));
    TEST_RESULT(
      MoonlightProtocolV1TlvParserFinish(&parser),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    TEST_CHECK(observation.field_begin_count == 4);
    TEST_CHECK(observation.field_end_count == 4);
    TEST_CHECK(observation.value_size == sizeof(expected_values));
  }

  return true;
}

/**
 * @brief Tests TLV ordering and repeated-field rules in writer and parser.
 *
 * @return True on success.
 */
static bool test_tlv_order_and_repeat(void) {
  static const uint8_t value = 0x5a;
  uint8_t output[64];
  uint8_t output_before[sizeof(output)];
  MoonlightProtocolV1TlvWriter writer;
  MoonlightProtocolV1TlvWriter writer_before;
  uint8_t malformed[3u * MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE];
  size_t offset;

  memset(output, 0xa5, sizeof(output));
  TEST_RESULT(
    MoonlightProtocolV1TlvWriterInitialize(
      &writer,
      output,
      sizeof(output),
      0
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_RESULT(
    MoonlightProtocolV1TlvWriterAppend(&writer, 2, 0, &value, 1),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  memcpy(&writer_before, &writer, sizeof(writer));
  memcpy(output_before, output, sizeof(output));
  TEST_RESULT(
    MoonlightProtocolV1TlvWriterAppend(&writer, 1, 0, &value, 1),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  TEST_CHECK(memcmp(&writer, &writer_before, sizeof(writer)) == 0);
  TEST_CHECK(memcmp(output, output_before, sizeof(output)) == 0);
  TEST_RESULT(
    MoonlightProtocolV1TlvWriterAppend(&writer, 2, 0, &value, 1),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  TEST_CHECK(memcmp(&writer, &writer_before, sizeof(writer)) == 0);
  TEST_CHECK(memcmp(output, output_before, sizeof(output)) == 0);

  TEST_RESULT(
    MoonlightProtocolV1TlvWriterInitialize(
      &writer,
      output,
      sizeof(output),
      0
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_RESULT(
    MoonlightProtocolV1TlvWriterAppend(
      &writer,
      1,
      MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED,
      NULL,
      0
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_RESULT(
    MoonlightProtocolV1TlvWriterAppend(
      &writer,
      1,
      MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED,
      NULL,
      0
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  memcpy(&writer_before, &writer, sizeof(writer));
  memcpy(output_before, output, sizeof(output));
  TEST_RESULT(
    MoonlightProtocolV1TlvWriterAppend(&writer, 1, 0, NULL, 0),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  TEST_CHECK(memcmp(&writer, &writer_before, sizeof(writer)) == 0);
  TEST_CHECK(memcmp(output, output_before, sizeof(output)) == 0);

  offset = 0;
  TEST_CHECK(append_tlv_header(malformed, &offset, 2, 0, 0));
  TEST_CHECK(append_tlv_header(malformed, &offset, 1, 0, 0));
  {
    MoonlightProtocolV1TlvParser parser;
    MoonlightProtocolV1TlvEvent event;
    size_t consumed;

    TEST_RESULT(
      MoonlightProtocolV1TlvParserInitialize(
        &parser,
        (uint32_t) offset,
        0
      ),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    TEST_RESULT(
      MoonlightProtocolV1TlvParserFeed(
        &parser,
        malformed,
        offset,
        &consumed,
        &event
      ),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    TEST_CHECK(event.type == MOONLIGHT_PROTOCOL_V1_TLV_EVENT_FIELD_BEGIN);
    TEST_RESULT(
      MoonlightProtocolV1TlvParserFeed(
        &parser,
        malformed + consumed,
        offset - consumed,
        &consumed,
        &event
      ),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    TEST_CHECK(event.type == MOONLIGHT_PROTOCOL_V1_TLV_EVENT_FIELD_END);
    TEST_RESULT(
      MoonlightProtocolV1TlvParserFeed(
        &parser,
        malformed + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
        MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
        &consumed,
        &event
      ),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    );
  }

  offset = 0;
  TEST_CHECK(append_tlv_header(malformed, &offset, 1, 0, 0));
  TEST_CHECK(append_tlv_header(malformed, &offset, 1, 0, 0));
  {
    MoonlightProtocolV1TlvParser parser;
    TlvObservation observation = {0};
    size_t consumed;
    MoonlightProtocolV1TlvEvent event;

    TEST_RESULT(
      MoonlightProtocolV1TlvParserInitialize(
        &parser,
        (uint32_t) offset,
        0
      ),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    TEST_CHECK(
      feed_tlv_slice(
        &parser,
        malformed,
        MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
        &observation
      )
    );
    TEST_RESULT(
      MoonlightProtocolV1TlvParserFeed(
        &parser,
        malformed + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
        MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
        &consumed,
        &event
      ),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    );
  }

  return true;
}

/**
 * @brief Tests TLV occurrence limits at every permitted nesting depth.
 *
 * @return True on success.
 */
static bool test_tlv_count_and_depth_limits(void) {
  uint8_t many[TEST_MANY_TLV_BYTES];
  uint8_t output[TEST_MANY_TLV_BYTES];
  size_t offset = 0;
  size_t index;

  for (index = 0;
       index <= MOONLIGHT_PROTOCOL_V1_TLV_TOP_LEVEL_FIELD_MAX;
       ++index) {
    TEST_CHECK(
      append_tlv_header(
        many,
        &offset,
        (uint16_t) (index + 1u),
        0,
        0
      )
    );
  }
  TEST_CHECK(offset == sizeof(many));

  {
    MoonlightProtocolV1TlvWriter writer;

    TEST_RESULT(
      MoonlightProtocolV1TlvWriterInitialize(
        &writer,
        output,
        sizeof(output),
        0
      ),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    for (index = 0;
         index < MOONLIGHT_PROTOCOL_V1_TLV_TOP_LEVEL_FIELD_MAX;
         ++index) {
      TEST_RESULT(
        MoonlightProtocolV1TlvWriterAppend(
          &writer,
          (uint16_t) (index + 1u),
          0,
          NULL,
          0
        ),
        MOONLIGHT_PROTOCOL_RESULT_OK
      );
    }
    TEST_CHECK(
      MoonlightProtocolV1TlvWriterSize(&writer) ==
      MOONLIGHT_PROTOCOL_V1_TLV_TOP_LEVEL_FIELD_MAX *
        MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE
    );
    TEST_RESULT(
      MoonlightProtocolV1TlvWriterAppend(
        &writer,
        (uint16_t) (MOONLIGHT_PROTOCOL_V1_TLV_TOP_LEVEL_FIELD_MAX + 1u),
        0,
        NULL,
        0
      ),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    );
  }

  {
    MoonlightProtocolV1TlvParser parser;
    size_t position = 0;

    TEST_RESULT(
      MoonlightProtocolV1TlvParserInitialize(
        &parser,
        (uint32_t) sizeof(many),
        0
      ),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    while (position <
           MOONLIGHT_PROTOCOL_V1_TLV_TOP_LEVEL_FIELD_MAX *
             MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE) {
      MoonlightProtocolV1TlvEvent event;
      size_t consumed;

      TEST_RESULT(
        MoonlightProtocolV1TlvParserFeed(
          &parser,
          many + position,
          sizeof(many) - position,
          &consumed,
          &event
        ),
        MOONLIGHT_PROTOCOL_RESULT_OK
      );
      TEST_CHECK(event.type != MOONLIGHT_PROTOCOL_V1_TLV_EVENT_NONE);
      position += consumed;
    }
    for (;;) {
      MoonlightProtocolV1TlvEvent event;
      size_t consumed;
      MoonlightProtocolResult result =
        MoonlightProtocolV1TlvParserFeed(
          &parser,
          many + position,
          sizeof(many) - position,
          &consumed,
          &event
        );

      position += consumed;
      if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
        TEST_CHECK(result == MOONLIGHT_PROTOCOL_RESULT_MALFORMED);
        break;
      }
      TEST_CHECK(event.type != MOONLIGHT_PROTOCOL_V1_TLV_EVENT_NONE);
    }
    TEST_RESULT(
      MoonlightProtocolV1TlvParserFinish(&parser),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    );
  }

  for (index = 1; index <= MOONLIGHT_PROTOCOL_V1_TLV_DEPTH_MAX; ++index) {
    MoonlightProtocolV1TlvWriter writer;
    MoonlightProtocolV1TlvParser parser;
    const size_t nested_size =
      (MOONLIGHT_PROTOCOL_V1_TLV_NESTED_FIELD_MAX + 1u) *
      MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE;

    TEST_RESULT(
      MoonlightProtocolV1TlvWriterInitialize(
        &writer,
        output,
        sizeof(output),
        (uint8_t) index
      ),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    {
      size_t field_index;

      for (field_index = 0;
           field_index < MOONLIGHT_PROTOCOL_V1_TLV_NESTED_FIELD_MAX;
           ++field_index) {
        TEST_RESULT(
          MoonlightProtocolV1TlvWriterAppend(
            &writer,
            (uint16_t) (field_index + 1u),
            0,
            NULL,
            0
          ),
          MOONLIGHT_PROTOCOL_RESULT_OK
        );
      }
    }
    TEST_RESULT(
      MoonlightProtocolV1TlvWriterAppend(
        &writer,
        (uint16_t) (MOONLIGHT_PROTOCOL_V1_TLV_NESTED_FIELD_MAX + 1u),
        0,
        NULL,
        0
      ),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    );

    TEST_RESULT(
      MoonlightProtocolV1TlvParserInitialize(
        &parser,
        (uint32_t) nested_size,
        (uint8_t) index
      ),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    {
      size_t position = 0;
      MoonlightProtocolResult result = MOONLIGHT_PROTOCOL_RESULT_OK;

      while (result == MOONLIGHT_PROTOCOL_RESULT_OK) {
        MoonlightProtocolV1TlvEvent event;
        size_t consumed;

        result = MoonlightProtocolV1TlvParserFeed(
          &parser,
          many + position,
          nested_size - position,
          &consumed,
          &event
        );
        position += consumed;
        if (result == MOONLIGHT_PROTOCOL_RESULT_OK) {
          TEST_CHECK(
            consumed != 0 ||
            event.type != MOONLIGHT_PROTOCOL_V1_TLV_EVENT_NONE
          );
        }
      }
      TEST_CHECK(result == MOONLIGHT_PROTOCOL_RESULT_MALFORMED);
    }
  }

  {
    MoonlightProtocolV1TlvParser parser;
    MoonlightProtocolV1TlvParser parser_before;
    MoonlightProtocolV1TlvWriter writer;
    MoonlightProtocolV1TlvWriter writer_before;

    memset(&parser, 0xa5, sizeof(parser));
    memcpy(&parser_before, &parser, sizeof(parser));
    TEST_RESULT(
      MoonlightProtocolV1TlvParserInitialize(
        &parser,
        0,
        MOONLIGHT_PROTOCOL_V1_TLV_DEPTH_MAX + 1u
      ),
      MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
    );
    TEST_CHECK(memcmp(&parser, &parser_before, sizeof(parser)) == 0);

    memset(&writer, 0xa5, sizeof(writer));
    memcpy(&writer_before, &writer, sizeof(writer));
    TEST_RESULT(
      MoonlightProtocolV1TlvWriterInitialize(
        &writer,
        output,
        sizeof(output),
        MOONLIGHT_PROTOCOL_V1_TLV_DEPTH_MAX + 1u
      ),
      MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
    );
    TEST_CHECK(memcmp(&writer, &writer_before, sizeof(writer)) == 0);
  }

  return true;
}

/**
 * @brief Tests TLV declared-size limits and sticky parser failures.
 *
 * @return True on success.
 */
static bool test_tlv_limits_sticky_and_finish(void) {
  uint8_t bytes[MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE];
  size_t offset = 0;

  TEST_RESULT(
    MoonlightProtocolV1TlvParserInitialize(NULL, 0, 0),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_CHECK(!MoonlightProtocolV1TlvParserIsComplete(NULL));
  TEST_RESULT(
    MoonlightProtocolV1TlvParserFinish(NULL),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );

  TEST_CHECK(append_tlv_header(bytes, &offset, 1, 0, 1));
  {
    MoonlightProtocolV1TlvParser parser;
    MoonlightProtocolV1TlvEvent event;
    size_t consumed;

    TEST_RESULT(
      MoonlightProtocolV1TlvParserInitialize(
        &parser,
        MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
        0
      ),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    TEST_RESULT(
      MoonlightProtocolV1TlvParserFeed(
        &parser,
        bytes,
        sizeof(bytes),
        &consumed,
        &event
      ),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    );
    TEST_CHECK(consumed == sizeof(bytes));
    consumed = SIZE_MAX;
    memset(&event, 0xa5, sizeof(event));
    TEST_RESULT(
      MoonlightProtocolV1TlvParserFeed(
        &parser,
        NULL,
        0,
        &consumed,
        &event
      ),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    );
    TEST_CHECK(consumed == 0);
    TEST_CHECK(event.type == MOONLIGHT_PROTOCOL_V1_TLV_EVENT_NONE);
    TEST_CHECK(!MoonlightProtocolV1TlvParserIsComplete(&parser));
    TEST_RESULT(
      MoonlightProtocolV1TlvParserFinish(&parser),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    );
  }

  {
    MoonlightProtocolV1TlvParser parser;
    MoonlightProtocolV1TlvEvent event;
    size_t consumed;

    bytes[3] = 2;
    TEST_RESULT(
      MoonlightProtocolV1TlvParserInitialize(
        &parser,
        sizeof(bytes),
        0
      ),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    TEST_RESULT(
      MoonlightProtocolV1TlvParserFeed(
        &parser,
        bytes,
        sizeof(bytes),
        &consumed,
        &event
      ),
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    );
    TEST_RESULT(
      MoonlightProtocolV1TlvParserFinish(&parser),
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    );
  }

  {
    MoonlightProtocolV1TlvParser parser;
    TlvObservation observation = {0};

    TEST_RESULT(
      MoonlightProtocolV1TlvParserInitialize(&parser, 7, 0),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    TEST_RESULT(
      MoonlightProtocolV1TlvParserFinish(&parser),
      MOONLIGHT_PROTOCOL_RESULT_TRUNCATED
    );
    TEST_RESULT(
      MoonlightProtocolV1TlvParserFeed(
        &parser,
        bytes,
        7,
        &(size_t) {0},
        &(MoonlightProtocolV1TlvEvent) {0}
      ),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    );
    (void) observation;
  }

  {
    uint8_t complete_field[MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + 1u];
    MoonlightProtocolV1TlvParser parser;
    MoonlightProtocolV1TlvEvent event;
    size_t complete_offset = 0;
    size_t consumed;

    TEST_CHECK(
      append_tlv_header(
        complete_field,
        &complete_offset,
        1,
        0,
        1
      )
    );
    complete_field[complete_offset] = 0x5a;
    TEST_RESULT(
      MoonlightProtocolV1TlvParserInitialize(
        &parser,
        sizeof(complete_field),
        0
      ),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    TEST_RESULT(
      MoonlightProtocolV1TlvParserFeed(
        &parser,
        complete_field,
        MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
        &consumed,
        &event
      ),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    TEST_CHECK(event.type == MOONLIGHT_PROTOCOL_V1_TLV_EVENT_FIELD_BEGIN);
    TEST_RESULT(
      MoonlightProtocolV1TlvParserFeed(
        &parser,
        complete_field + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
        1,
        &consumed,
        &event
      ),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    TEST_CHECK(event.type == MOONLIGHT_PROTOCOL_V1_TLV_EVENT_VALUE);
    TEST_CHECK(MoonlightProtocolV1TlvParserIsComplete(&parser));
    TEST_RESULT(
      MoonlightProtocolV1TlvParserFinish(&parser),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
  }

  {
    MoonlightProtocolV1TlvParser parser;
    MoonlightProtocolV1TlvWriter writer;
    uint8_t byte = 0;

    TEST_RESULT(
      MoonlightProtocolV1TlvParserInitialize(
        &parser,
        MOONLIGHT_PROTOCOL_V1_BULK_PAYLOAD_MAX,
        0
      ),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    TEST_RESULT(
      MoonlightProtocolV1TlvParserFinish(&parser),
      MOONLIGHT_PROTOCOL_RESULT_TRUNCATED
    );
    TEST_RESULT(
      MoonlightProtocolV1TlvParserInitialize(
        &parser,
        MOONLIGHT_PROTOCOL_V1_BULK_PAYLOAD_MAX + 1u,
        0
      ),
      MOONLIGHT_PROTOCOL_RESULT_LIMIT_EXCEEDED
    );
    TEST_RESULT(
      MoonlightProtocolV1TlvWriterInitialize(
        &writer,
        &byte,
        MOONLIGHT_PROTOCOL_V1_BULK_PAYLOAD_MAX,
        0
      ),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    TEST_RESULT(
      MoonlightProtocolV1TlvWriterAppend(
        &writer,
        1,
        0,
        &byte,
        (size_t) MOONLIGHT_PROTOCOL_V1_BULK_PAYLOAD_MAX + 1u
      ),
      MOONLIGHT_PROTOCOL_RESULT_LIMIT_EXCEEDED
    );
  }
  return true;
}

/**
 * @brief Tests TLV writer overlap support and failure atomicity.
 *
 * @return True on success.
 */
static bool test_tlv_writer_atomicity_and_overlap(void) {
  uint8_t output[32];
  uint8_t output_before[sizeof(output)];
  MoonlightProtocolV1TlvWriter writer;
  MoonlightProtocolV1TlvWriter writer_before;

  TEST_RESULT(
    MoonlightProtocolV1TlvWriterInitialize(NULL, output, sizeof(output), 0),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1TlvWriterAppend(NULL, 1, 0, NULL, 0),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1TlvWriterInitialize(&writer, NULL, 1, 0),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );

  output[0] = 0x11;
  output[1] = 0x22;
  output[2] = 0x33;
  output[3] = 0x44;
  TEST_RESULT(
    MoonlightProtocolV1TlvWriterInitialize(
      &writer,
      output,
      sizeof(output),
      0
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_RESULT(
    MoonlightProtocolV1TlvWriterAppend(
      &writer,
      1,
      0,
      output,
      4
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(MoonlightProtocolV1TlvWriterSize(&writer) == 12);
  TEST_CHECK(output[0] == 0);
  TEST_CHECK(output[1] == 1);
  TEST_CHECK(output[2] == 0);
  TEST_CHECK(output[3] == 0);
  TEST_CHECK(output[4] == 0);
  TEST_CHECK(output[5] == 0);
  TEST_CHECK(output[6] == 0);
  TEST_CHECK(output[7] == 4);
  TEST_CHECK(output[8] == 0x11);
  TEST_CHECK(output[9] == 0x22);
  TEST_CHECK(output[10] == 0x33);
  TEST_CHECK(output[11] == 0x44);

  memcpy(&writer_before, &writer, sizeof(writer));
  memcpy(output_before, output, sizeof(output));
  TEST_RESULT(
    MoonlightProtocolV1TlvWriterAppend(
      &writer,
      2,
      0,
      output,
      sizeof(output)
    ),
    MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL
  );
  TEST_CHECK(memcmp(&writer, &writer_before, sizeof(writer)) == 0);
  TEST_CHECK(memcmp(output, output_before, sizeof(output)) == 0);

  TEST_RESULT(
    MoonlightProtocolV1TlvWriterAppend(
      &writer,
      2,
      2,
      NULL,
      0
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  TEST_CHECK(memcmp(&writer, &writer_before, sizeof(writer)) == 0);
  TEST_CHECK(memcmp(output, output_before, sizeof(output)) == 0);

  {
    MoonlightProtocolV1TlvParser parser;
    MoonlightProtocolV1TlvParser parser_before;
    MoonlightProtocolV1TlvEvent event;
    MoonlightProtocolV1TlvEvent event_before;
    size_t consumed = 91;

    TEST_RESULT(
      MoonlightProtocolV1TlvParserInitialize(&parser, 0, 0),
      MOONLIGHT_PROTOCOL_RESULT_OK
    );
    memcpy(&parser_before, &parser, sizeof(parser));
    memset(&event, 0xa5, sizeof(event));
    memcpy(&event_before, &event, sizeof(event));
    TEST_RESULT(
      MoonlightProtocolV1TlvParserFeed(
        &parser,
        output,
        sizeof(output),
        NULL,
        &event
      ),
      MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
    );
    TEST_CHECK(memcmp(&parser, &parser_before, sizeof(parser)) == 0);
    TEST_CHECK(memcmp(&event, &event_before, sizeof(event)) == 0);
    TEST_CHECK(consumed == 91);
  }

  TEST_CHECK(MoonlightProtocolV1TlvWriterSize(NULL) == 0);
  return true;
}

/**
 * @brief Runs one named native test.
 *
 * @param name Human-readable test name.
 * @param test Test function.
 * @return True on success.
 */
static bool run_test(const char *name, bool (*test)(void)) {
  if (!test()) {
    fprintf(stderr, "FAILED: %s\n", name);
    return false;
  }
  printf("PASS: %s\n", name);
  return true;
}

/**
 * @brief Runs the protocol version 1 reliable-wire native tests.
 *
 * @return Zero on success.
 */
int main(void) {
  bool passed = true;

  passed = run_test("fixed headers and forms", test_fixed_headers_and_forms) && passed;
  passed = run_test("unknown request response encoder", test_unknown_request_response_encoder) && passed;
  passed = run_test("fixed header atomic errors", test_fixed_header_errors_are_atomic) && passed;
  passed = run_test("fixed header validation matrix", test_fixed_header_validation_matrix) && passed;
  passed = run_test("stream all splits and bytes", test_stream_all_splits_and_bytes) && passed;
  passed = run_test("stream coalesced and reverse", test_stream_coalesced_and_reverse) && passed;
  passed = run_test("stream half-role matrix", test_stream_half_role_matrix) && passed;
  passed = run_test("stream unknown request drain", test_stream_unknown_request_drain) && passed;
  passed = run_test("stream payload-limit boundary updates", test_stream_payload_limit_boundary_updates) && passed;
  passed = run_test("stream FIN boundaries", test_stream_finish_boundaries) && passed;
  passed = run_test("stream failures", test_stream_failures) && passed;
  passed = run_test("TLV round-trip splits", test_tlv_round_trip_splits) && passed;
  passed = run_test("TLV order and repeat", test_tlv_order_and_repeat) && passed;
  passed = run_test("TLV count and depth", test_tlv_count_and_depth_limits) && passed;
  passed = run_test("TLV limits and sticky failures", test_tlv_limits_sticky_and_finish) && passed;
  passed = run_test("TLV writer atomicity and overlap", test_tlv_writer_atomicity_and_overlap) && passed;
  return passed ? 0 : 1;
}
