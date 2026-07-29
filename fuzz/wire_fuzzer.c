/**
 * @file wire_fuzzer.c
 * @brief LibFuzzer entry point for protocol version 1 reliable framing.
 */

#include <moonlight/protocol/client_proof.h>
#include <moonlight/protocol/control.h>
#include <moonlight/protocol/pairing.h>
#include <moonlight/protocol/wire.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Maximum canonical stream assembled by the invariant checks.
 */
#define FUZZ_STREAM_CAPACITY 512u

/**
 * @brief Aborts when a codec invariant is violated.
 *
 * @param condition Invariant to require.
 */
static void require_invariant(int condition) {
  if (!condition) {
    abort();
  }
}

/**
 * @brief Selects one bounded parser limit from arbitrary input.
 *
 * @param selector Arbitrary selector byte.
 * @return A protocol lane or state limit.
 */
static uint32_t select_payload_limit(uint8_t selector) {
  static const uint32_t limits[] = {
    0,
    98,
    4096,
    MOONLIGHT_PROTOCOL_V1_RELIABLE_INPUT_PAYLOAD_MAX,
    MOONLIGHT_PROTOCOL_V1_PAIR_CONTROL_PAYLOAD_MAX,
    MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX,
    MOONLIGHT_PROTOCOL_V1_BULK_PAYLOAD_MAX,
  };

  return limits[selector % (sizeof(limits) / sizeof(limits[0]))];
}

/**
 * @brief Exercises one arbitrary byte sequence through the stream parser.
 *
 * @param data Arbitrary bytes.
 * @param size Number of readable bytes.
 */
static void fuzz_stream_parser(const uint8_t *data, size_t size) {
  MoonlightProtocolV1StreamParser parser;
  MoonlightProtocolResult result;
  size_t offset = 0;
  size_t iterations = 0;
  const uint8_t selector = size == 0 ? 0 : data[0];

  require_invariant(
    MoonlightProtocolV1StreamParserInitialize(
      &parser,
      select_payload_limit(selector)
    ) == MOONLIGHT_PROTOCOL_RESULT_OK
  );

  while (offset < size && iterations < size * 4u + 16u) {
    MoonlightProtocolV1StreamEvent event;
    size_t consumed = 0;
    size_t chunk_size = 1u + data[offset] % 31u;

    if (chunk_size > size - offset) {
      chunk_size = size - offset;
    }
    result = MoonlightProtocolV1StreamParserFeed(
      &parser,
      data + offset,
      chunk_size,
      &consumed,
      &event
    );
    require_invariant(consumed <= chunk_size);
    if (event.type == MOONLIGHT_PROTOCOL_V1_STREAM_EVENT_PAYLOAD) {
      require_invariant(event.payload >= data + offset);
      require_invariant(
        event.payload + event.payload_size <= data + offset + chunk_size
      );
      require_invariant(event.payload_size != 0);
    }
    offset += consumed;
    ++iterations;
    if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
      break;
    }
    if (consumed == 0 && event.type == MOONLIGHT_PROTOCOL_V1_STREAM_EVENT_NONE) {
      break;
    }
  }
  require_invariant(iterations < size * 4u + 17u);
  (void) MoonlightProtocolV1StreamParserFinish(&parser);
}

/**
 * @brief Exercises one arbitrary byte sequence through a bounded TLV parser.
 *
 * @param data Arbitrary bytes.
 * @param size Number of readable bytes.
 */
static void fuzz_tlv_parser(const uint8_t *data, size_t size) {
  MoonlightProtocolV1TlvParser parser;
  MoonlightProtocolResult result;
  const uint32_t payload_size =
    size > MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX ?
      MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX :
      (uint32_t) size;
  size_t offset = 0;
  size_t iterations = 0;

  require_invariant(
    MoonlightProtocolV1TlvParserInitialize(
      &parser,
      payload_size,
      size == 0 ? 0 : data[0] % (MOONLIGHT_PROTOCOL_V1_TLV_DEPTH_MAX + 1u)
    ) == MOONLIGHT_PROTOCOL_RESULT_OK
  );

  while (offset < payload_size && iterations < (size_t) payload_size * 4u + 16u) {
    MoonlightProtocolV1TlvEvent event;
    size_t consumed = 0;
    size_t chunk_size = 1u + data[offset] % 17u;

    if (chunk_size > payload_size - offset) {
      chunk_size = payload_size - offset;
    }
    result = MoonlightProtocolV1TlvParserFeed(
      &parser,
      data + offset,
      chunk_size,
      &consumed,
      &event
    );
    require_invariant(consumed <= chunk_size);
    if (event.type == MOONLIGHT_PROTOCOL_V1_TLV_EVENT_VALUE) {
      require_invariant(event.value >= data + offset);
      require_invariant(
        event.value + event.value_size <= data + offset + chunk_size
      );
      require_invariant(event.value_size != 0);
    }
    offset += consumed;
    ++iterations;
    if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
      break;
    }
    if (consumed == 0 && event.type == MOONLIGHT_PROTOCOL_V1_TLV_EVENT_NONE) {
      break;
    }
  }
  require_invariant(iterations < (size_t) payload_size * 4u + 17u);
  (void) MoonlightProtocolV1TlvParserFinish(&parser);
}

/**
 * @brief Builds and incrementally parses one canonical Control request.
 *
 * @param data Arbitrary field-value bytes.
 * @param size Number of readable bytes.
 */
static void fuzz_canonical_roundtrip(const uint8_t *data, size_t size) {
  uint8_t payload[96];
  uint8_t stream[FUZZ_STREAM_CAPACITY];
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
  MoonlightProtocolV1StreamParser parser;
  MoonlightProtocolV1TlvWriter writer;
  size_t payload_value_size = size > 32u ? 32u : size;
  size_t stream_size;
  size_t offset = 0;
  size_t copied_payload = 0;
  size_t iterations = 0;

  require_invariant(
    MoonlightProtocolV1TlvWriterInitialize(
      &writer,
      payload,
      sizeof(payload),
      0
    ) == MOONLIGHT_PROTOCOL_RESULT_OK
  );
  if (payload_value_size != 0) {
    require_invariant(
      MoonlightProtocolV1TlvWriterAppend(
        &writer,
        1,
        0,
        data,
        payload_value_size
      ) == MOONLIGHT_PROTOCOL_RESULT_OK
    );
  }
  envelope.payload_length = (uint32_t) MoonlightProtocolV1TlvWriterSize(&writer);
  require_invariant(
    MoonlightProtocolV1EncodeLanePreface(
      &lane,
      stream,
      sizeof(stream)
    ) == MOONLIGHT_PROTOCOL_RESULT_OK
  );
  require_invariant(
    MoonlightProtocolV1EncodeMessageEnvelope(
      &envelope,
      MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX,
      stream + MOONLIGHT_PROTOCOL_V1_LANE_PREFACE_SIZE,
      sizeof(stream) - MOONLIGHT_PROTOCOL_V1_LANE_PREFACE_SIZE
    ) == MOONLIGHT_PROTOCOL_RESULT_OK
  );
  memcpy(
    stream + MOONLIGHT_PROTOCOL_V1_LANE_PREFACE_SIZE +
      MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE,
    payload,
    envelope.payload_length
  );
  stream_size =
    MOONLIGHT_PROTOCOL_V1_LANE_PREFACE_SIZE +
    MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE +
    envelope.payload_length;

  require_invariant(
    MoonlightProtocolV1StreamParserInitialize(
      &parser,
      MOONLIGHT_PROTOCOL_V1_CONTROL_PAYLOAD_MAX
    ) == MOONLIGHT_PROTOCOL_RESULT_OK
  );
  while ((offset < stream_size || iterations < 3u) &&
         iterations < stream_size * 4u + 16u) {
    MoonlightProtocolV1StreamEvent event;
    size_t consumed = 0;
    const size_t remaining = stream_size - offset;
    const size_t chunk_size = remaining == 0 ?
                                0 :
                                1u + stream[offset] % remaining;
    MoonlightProtocolResult result =
      MoonlightProtocolV1StreamParserFeed(
        &parser,
        remaining == 0 ? NULL : stream + offset,
        chunk_size,
        &consumed,
        &event
      );

    require_invariant(result == MOONLIGHT_PROTOCOL_RESULT_OK);
    require_invariant(consumed <= chunk_size);
    if (event.type == MOONLIGHT_PROTOCOL_V1_STREAM_EVENT_PAYLOAD) {
      require_invariant(
        memcmp(
          event.payload,
          payload + copied_payload,
          event.payload_size
        ) == 0
      );
      copied_payload += event.payload_size;
    }
    offset += consumed;
    ++iterations;
    if (offset == stream_size && consumed == 0 && event.type == MOONLIGHT_PROTOCOL_V1_STREAM_EVENT_NONE) {
      break;
    }
  }
  require_invariant(offset == stream_size);
  require_invariant(copied_payload == envelope.payload_length);
  require_invariant(
    MoonlightProtocolV1StreamParserFinish(&parser) ==
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
}

/**
 * @brief Exercises strict CLIENT_HELLO schema decoders and canonical re-encoding.
 *
 * @param data Arbitrary payload bytes.
 * @param size Number of readable bytes.
 */
static void fuzz_control_schema(const uint8_t *data, size_t size) {
  uint8_t encoded[MOONLIGHT_PROTOCOL_V1_CLIENT_HELLO_REQUEST_PAYLOAD_MAX];
  MoonlightProtocolV1ClientHelloRequest request;
  MoonlightProtocolV1ClientHelloResponse response;
  MoonlightProtocolResult result;
  size_t encoded_size = 0;

  result = MoonlightProtocolV1DecodeClientHelloRequest(
    data,
    size,
    &request
  );
  if (result == MOONLIGHT_PROTOCOL_RESULT_OK) {
    require_invariant(
      MoonlightProtocolV1EncodeClientHelloRequest(
        &request,
        encoded,
        sizeof(encoded),
        &encoded_size
      ) == MOONLIGHT_PROTOCOL_RESULT_OK
    );
    require_invariant(encoded_size == size);
    require_invariant(memcmp(encoded, data, size) == 0);
  }

  result = MoonlightProtocolV1DecodeClientHelloResponse(
    data,
    size,
    &response
  );
  if (result == MOONLIGHT_PROTOCOL_RESULT_OK) {
    require_invariant(
      MoonlightProtocolV1EncodeClientHelloResponse(
        &response,
        encoded,
        sizeof(encoded),
        &encoded_size
      ) == MOONLIGHT_PROTOCOL_RESULT_OK
    );
    require_invariant(encoded_size == size);
    require_invariant(memcmp(encoded, data, size) == 0);
  }
}

/**
 * @brief Exercises Client Credential and signature validators directly.
 *
 * Raw input reaches both scheme validators without first having to satisfy the
 * surrounding TLV schema. Near-valid templates are also mutated one byte at a
 * time so short fuzz runs can enter the strict DER and SPKI checks.
 *
 * @param data Arbitrary representation or mutation selector bytes.
 * @param size Number of readable bytes.
 */
static void fuzz_credential_representations(
  const uint8_t *data,
  size_t size
) {
  uint8_t p256_spki[MOONLIGHT_PROTOCOL_V1_P256_CREDENTIAL_SPKI_SIZE] = {
    0x30,
    0x59,
    0x30,
    0x13,
    0x06,
    0x07,
    0x2a,
    0x86,
    0x48,
    0xce,
    0x3d,
    0x02,
    0x01,
    0x06,
    0x08,
    0x2a,
    0x86,
    0x48,
    0xce,
    0x3d,
    0x03,
    0x01,
    0x07,
    0x03,
    0x42,
    0x00,
    0x04,
  };
  uint8_t rsa_spki[MOONLIGHT_PROTOCOL_V1_RSA2048_CREDENTIAL_SPKI_SIZE] = {
    0x30,
    0x82,
    0x01,
    0x22,
    0x30,
    0x0d,
    0x06,
    0x09,
    0x2a,
    0x86,
    0x48,
    0x86,
    0xf7,
    0x0d,
    0x01,
    0x01,
    0x01,
    0x05,
    0x00,
    0x03,
    0x82,
    0x01,
    0x0f,
    0x00,
    0x30,
    0x82,
    0x01,
    0x0a,
    0x02,
    0x82,
    0x01,
    0x01,
    0x00,
  };
  uint8_t p256_signature[] = {
    0x30,
    0x06,
    0x02,
    0x01,
    0x01,
    0x02,
    0x01,
    0x01,
  };
  uint8_t rsa_signature[MOONLIGHT_PROTOCOL_V1_RSA2048_SIGNATURE_SIZE];
  uint8_t mutation;

  (void) MoonlightProtocolV1ValidateCredentialSpki(
    MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
    data,
    size
  );
  (void) MoonlightProtocolV1ValidateCredentialSpki(
    MOONLIGHT_PROTOCOL_V1_CREDENTIAL_RSA2048_PKCS1_SHA256_COMPAT,
    data,
    size
  );
  (void) MoonlightProtocolV1ValidateProofSignature(
    MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
    data,
    size
  );
  (void) MoonlightProtocolV1ValidateProofSignature(
    MOONLIGHT_PROTOCOL_V1_CREDENTIAL_RSA2048_PKCS1_SHA256_COMPAT,
    data,
    size
  );

  memset(p256_spki + 27u, 0xa5, 64u);
  memset(rsa_spki + 33u, 0xa5, 256u);
  rsa_spki[33] = 0x80;
  rsa_spki[288] |= 1u;
  rsa_spki[289] = 0x02;
  rsa_spki[290] = 0x03;
  rsa_spki[291] = 0x01;
  rsa_spki[292] = 0x00;
  rsa_spki[293] = 0x01;
  memset(rsa_signature, 0xa5, sizeof(rsa_signature));

  require_invariant(
    MoonlightProtocolV1ValidateCredentialSpki(
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      p256_spki,
      sizeof(p256_spki)
    ) == MOONLIGHT_PROTOCOL_RESULT_OK
  );
  require_invariant(
    MoonlightProtocolV1ValidateCredentialSpki(
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_RSA2048_PKCS1_SHA256_COMPAT,
      rsa_spki,
      sizeof(rsa_spki)
    ) == MOONLIGHT_PROTOCOL_RESULT_OK
  );
  require_invariant(
    MoonlightProtocolV1ValidateProofSignature(
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      p256_signature,
      sizeof(p256_signature)
    ) == MOONLIGHT_PROTOCOL_RESULT_OK
  );
  require_invariant(
    MoonlightProtocolV1ValidateProofSignature(
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_RSA2048_PKCS1_SHA256_COMPAT,
      rsa_signature,
      sizeof(rsa_signature)
    ) == MOONLIGHT_PROTOCOL_RESULT_OK
  );

  if (size == 0) {
    return;
  }
  mutation = size > 1u ? data[1] : 0xffu;
  p256_spki[data[0] % sizeof(p256_spki)] ^= mutation;
  rsa_spki[data[0] % sizeof(rsa_spki)] ^= mutation;
  p256_signature[data[0] % sizeof(p256_signature)] ^= mutation;
  rsa_signature[data[0] % sizeof(rsa_signature)] ^= mutation;
  (void) MoonlightProtocolV1ValidateCredentialSpki(
    MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
    p256_spki,
    sizeof(p256_spki)
  );
  (void) MoonlightProtocolV1ValidateCredentialSpki(
    MOONLIGHT_PROTOCOL_V1_CREDENTIAL_RSA2048_PKCS1_SHA256_COMPAT,
    rsa_spki,
    sizeof(rsa_spki)
  );
  (void) MoonlightProtocolV1ValidateProofSignature(
    MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
    p256_signature,
    sizeof(p256_signature)
  );
  (void) MoonlightProtocolV1ValidateProofSignature(
    MOONLIGHT_PROTOCOL_V1_CREDENTIAL_RSA2048_PKCS1_SHA256_COMPAT,
    rsa_signature,
    sizeof(rsa_signature)
  );
}

/**
 * @brief Exercises strict pairing and Client Proof schema decoders.
 *
 * Every accepted payload must re-encode byte-for-byte. Streaming Proof is
 * attempted under both immutable stored schemes because that context is
 * deliberately absent from its wire form.
 *
 * @param data Arbitrary payload bytes.
 * @param size Number of readable bytes.
 */
static void fuzz_admission_schemas(const uint8_t *data, size_t size) {
  uint8_t encoded[MOONLIGHT_PROTOCOL_V1_PAIRING_CLIENT_PROOF_REQUEST_PAYLOAD_MAX];
  MoonlightProtocolV1PairingClientProofRequest pairing_proof;
  MoonlightProtocolV1StreamingClientProofRequest streaming_proof;
  MoonlightProtocolV1PairRequest pair_request;
  MoonlightProtocolV1PairResponse pair_response;
  MoonlightProtocolResult result;
  size_t encoded_size = 0;
  size_t scheme_index;
  static const MoonlightProtocolV1CredentialScheme schemes[] = {
    MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
    MOONLIGHT_PROTOCOL_V1_CREDENTIAL_RSA2048_PKCS1_SHA256_COMPAT,
  };

  result = MoonlightProtocolV1DecodePairingClientProofRequest(
    data,
    size,
    &pairing_proof
  );
  if (result == MOONLIGHT_PROTOCOL_RESULT_OK) {
    require_invariant(
      MoonlightProtocolV1EncodePairingClientProofRequest(
        &pairing_proof,
        encoded,
        sizeof(encoded),
        &encoded_size
      ) == MOONLIGHT_PROTOCOL_RESULT_OK
    );
    require_invariant(encoded_size == size);
    require_invariant(memcmp(encoded, data, size) == 0);
  }

  for (scheme_index = 0;
       scheme_index < sizeof(schemes) / sizeof(schemes[0]);
       ++scheme_index) {
    result = MoonlightProtocolV1DecodeStreamingClientProofRequest(
      data,
      size,
      schemes[scheme_index],
      &streaming_proof
    );
    if (result == MOONLIGHT_PROTOCOL_RESULT_OK) {
      require_invariant(
        MoonlightProtocolV1EncodeStreamingClientProofRequest(
          &streaming_proof,
          schemes[scheme_index],
          encoded,
          sizeof(encoded),
          &encoded_size
        ) == MOONLIGHT_PROTOCOL_RESULT_OK
      );
      require_invariant(encoded_size == size);
      require_invariant(memcmp(encoded, data, size) == 0);
    }
  }

  result = MoonlightProtocolV1DecodePairRequest(data, size, &pair_request);
  if (result == MOONLIGHT_PROTOCOL_RESULT_OK) {
    require_invariant(
      MoonlightProtocolV1EncodePairRequest(
        &pair_request,
        encoded,
        sizeof(encoded),
        &encoded_size
      ) == MOONLIGHT_PROTOCOL_RESULT_OK
    );
    require_invariant(encoded_size == size);
    require_invariant(memcmp(encoded, data, size) == 0);
  }

  result = MoonlightProtocolV1DecodePairResponse(data, size, &pair_response);
  if (result == MOONLIGHT_PROTOCOL_RESULT_OK) {
    require_invariant(
      MoonlightProtocolV1EncodePairResponse(
        &pair_response,
        encoded,
        sizeof(encoded),
        &encoded_size
      ) == MOONLIGHT_PROTOCOL_RESULT_OK
    );
    require_invariant(encoded_size == size);
    require_invariant(memcmp(encoded, data, size) == 0);
  }
}

/**
 * @brief Fuzzes framing, schemas, Credential representations, and canonical round trips.
 *
 * @param data Arbitrary fuzzer-owned bytes.
 * @param size Number of readable bytes in `data`.
 * @return Zero after processing the input.
 */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  uint8_t encoded[MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE];
  MoonlightProtocolV1LanePreface lane;
  MoonlightProtocolV1MessageEnvelope envelope;
  MoonlightProtocolV1TlvField field;
  MoonlightProtocolResult result;

  result = MoonlightProtocolV1DecodeLanePreface(data, size, &lane);
  if (result == MOONLIGHT_PROTOCOL_RESULT_OK) {
    require_invariant(
      MoonlightProtocolV1EncodeLanePreface(
        &lane,
        encoded,
        sizeof(encoded)
      ) == MOONLIGHT_PROTOCOL_RESULT_OK
    );
    require_invariant(
      memcmp(encoded, data, MOONLIGHT_PROTOCOL_V1_LANE_PREFACE_SIZE) == 0
    );
  }

  result = MoonlightProtocolV1DecodeMessageEnvelope(
    data,
    size,
    MOONLIGHT_PROTOCOL_V1_BULK_PAYLOAD_MAX,
    &envelope
  );
  if (result == MOONLIGHT_PROTOCOL_RESULT_OK) {
    require_invariant(
      MoonlightProtocolV1EncodeMessageEnvelope(
        &envelope,
        MOONLIGHT_PROTOCOL_V1_BULK_PAYLOAD_MAX,
        encoded,
        sizeof(encoded)
      ) == MOONLIGHT_PROTOCOL_RESULT_OK
    );
    require_invariant(
      memcmp(encoded, data, MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE) == 0
    );
    if (size >= MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE && (size_t) envelope.payload_length <= size - MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE) {
      fuzz_admission_schemas(
        data + MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE,
        envelope.payload_length
      );
    }
  }

  result = MoonlightProtocolV1DecodeTlvFieldHeader(data, size, &field);
  if (result == MOONLIGHT_PROTOCOL_RESULT_OK) {
    require_invariant(
      MoonlightProtocolV1EncodeTlvFieldHeader(
        &field,
        encoded,
        sizeof(encoded)
      ) == MOONLIGHT_PROTOCOL_RESULT_OK
    );
    require_invariant(
      memcmp(encoded, data, MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE) == 0
    );
  }

  fuzz_stream_parser(data, size);
  fuzz_tlv_parser(data, size);
  fuzz_canonical_roundtrip(data, size);
  fuzz_control_schema(data, size);
  fuzz_credential_representations(data, size);
  fuzz_admission_schemas(data, size);
  return 0;
}
