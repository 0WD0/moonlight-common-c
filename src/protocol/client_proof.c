/**
 * @file client_proof.c
 * @brief Implements protocol version 1 exporter-bound Client Proof schemas.
 */

#include <moonlight/protocol/client_proof.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/**
 * @brief Exact protocol major bound into every Client Proof transcript.
 */
#define CLIENT_PROOF_PROTOCOL_MAJOR 1u

/**
 * @brief Number of required scalar fields in a Pairing CLIENT_PROOF request.
 */
#define PAIRING_CLIENT_PROOF_FIELD_COUNT 5u

/**
 * @brief Number of required scalar fields in a Streaming CLIENT_PROOF request.
 */
#define STREAMING_CLIENT_PROOF_FIELD_COUNT 6u

/**
 * @brief Smallest structurally possible Pairing CLIENT_PROOF payload.
 */
#define PAIRING_CLIENT_PROOF_PAYLOAD_MIN 173u

/**
 * @brief Smallest structurally possible Streaming CLIENT_PROOF payload.
 */
#define STREAMING_CLIENT_PROOF_PAYLOAD_MIN 121u

/**
 * @brief Exact Pairing Proof domain-separation context.
 */
static const uint8_t pairing_proof_context[] =
  "Sunshine-Pairing-Proof-v1";

/**
 * @brief Exact Streaming Proof domain-separation context.
 */
static const uint8_t streaming_proof_context[] =
  "Sunshine-Streaming-Proof-v1";

/**
 * @brief Exact Pairing ALPN bound into a Pairing Proof transcript.
 */
static const uint8_t pairing_alpn[] = "sunshine-pair/1";

/**
 * @brief Exact Streaming ALPN bound into a Streaming Proof transcript.
 */
static const uint8_t streaming_alpn[] = "sunshine-stream/1";

/**
 * @brief P-256 group order as one fixed-width network-order integer.
 */
static const uint8_t p256_order[32] = {
  0xff,
  0xff,
  0xff,
  0xff,
  0x00,
  0x00,
  0x00,
  0x00,
  0xff,
  0xff,
  0xff,
  0xff,
  0xff,
  0xff,
  0xff,
  0xff,
  0xbc,
  0xe6,
  0xfa,
  0xad,
  0xa7,
  0x17,
  0x9e,
  0x84,
  0xf3,
  0xb9,
  0xca,
  0xc2,
  0xfc,
  0x63,
  0x25,
  0x51
};

/**
 * @brief Floor of half the P-256 group order in network byte order.
 */
static const uint8_t p256_half_order[32] = {
  0x7f,
  0xff,
  0xff,
  0xff,
  0x80,
  0x00,
  0x00,
  0x00,
  0x7f,
  0xff,
  0xff,
  0xff,
  0xff,
  0xff,
  0xff,
  0xff,
  0xde,
  0x73,
  0x7d,
  0x56,
  0xd3,
  0x8b,
  0xcf,
  0x42,
  0x79,
  0xdc,
  0xe5,
  0x61,
  0x7e,
  0x31,
  0x92,
  0xa8
};

/**
 * @brief Canonical DER prefix before a P-256 uncompressed affine point.
 */
static const uint8_t p256_spki_prefix[] = {
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
  0x04
};

/**
 * @brief Canonical DER prefix through an RSA-2048 modulus sign octet.
 */
static const uint8_t rsa2048_spki_prefix[] = {
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
  0x00
};

/**
 * @brief Canonical DER encoding of RSA public exponent 65537.
 */
static const uint8_t rsa65537_suffix[] = {
  0x02,
  0x03,
  0x01,
  0x00,
  0x01
};

_Static_assert(
  sizeof(pairing_proof_context) - 1u == 25u,
  "Pairing Proof context must remain 25 bytes"
);
_Static_assert(
  sizeof(streaming_proof_context) - 1u == 27u,
  "Streaming Proof context must remain 27 bytes"
);
_Static_assert(
  sizeof(pairing_alpn) - 1u == 15u,
  "Pairing ALPN must remain 15 bytes"
);
_Static_assert(
  sizeof(streaming_alpn) - 1u == 17u,
  "Streaming ALPN must remain 17 bytes"
);

/**
 * @brief Reliably clears sensitive temporary storage.
 *
 * Volatile byte stores prevent the compiler from removing the wipe after the
 * final non-volatile use of the object.
 *
 * @param data Writable storage to clear.
 * @param data_size Number of bytes to clear.
 */
static void proof_secure_clear(void *data, size_t data_size) {
  volatile uint8_t *cursor = (volatile uint8_t *) data;

  while (data_size != 0) {
    *cursor++ = 0;
    --data_size;
  }
}

/**
 * @brief Stores one network-order 16-bit integer.
 *
 * @param output Two writable bytes.
 * @param value Host-order value.
 */
static void proof_store_u16(uint8_t *output, uint16_t value) {
  output[0] = (uint8_t) (value >> 8u);
  output[1] = (uint8_t) value;
}

/**
 * @brief Stores one network-order 32-bit integer.
 *
 * @param output Four writable bytes.
 * @param value Host-order value.
 */
static void proof_store_u32(uint8_t *output, uint32_t value) {
  output[0] = (uint8_t) (value >> 24u);
  output[1] = (uint8_t) (value >> 16u);
  output[2] = (uint8_t) (value >> 8u);
  output[3] = (uint8_t) value;
}

/**
 * @brief Stores one network-order 64-bit integer.
 *
 * @param output Eight writable bytes.
 * @param value Host-order value.
 */
static void proof_store_u64(uint8_t *output, uint64_t value) {
  proof_store_u32(output, (uint32_t) (value >> 32u));
  proof_store_u32(output + 4u, (uint32_t) value);
}

/**
 * @brief Loads one network-order 32-bit integer.
 *
 * @param input Four readable bytes.
 * @return Host-order value.
 */
static uint32_t proof_load_u32(const uint8_t *input) {
  return ((uint32_t) input[0] << 24u) |
         ((uint32_t) input[1] << 16u) |
         ((uint32_t) input[2] << 8u) |
         input[3];
}

/**
 * @brief Loads one network-order 64-bit integer.
 *
 * @param input Eight readable bytes.
 * @return Host-order value.
 */
static uint64_t proof_load_u64(const uint8_t *input) {
  return ((uint64_t) proof_load_u32(input) << 32u) |
         proof_load_u32(input + 4u);
}

/**
 * @brief Validates one exact Client Credential registry value.
 *
 * @param credential_scheme Candidate scheme.
 * @return OK for an exact version 1 value, otherwise UNSUPPORTED.
 */
static MoonlightProtocolResult validate_credential_scheme(
  MoonlightProtocolV1CredentialScheme credential_scheme
) {
  switch (credential_scheme) {
    case MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256:
    case MOONLIGHT_PROTOCOL_V1_CREDENTIAL_RSA2048_PKCS1_SHA256_COMPAT:
      return MOONLIGHT_PROTOCOL_RESULT_OK;
    default:
      return MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED;
  }
}

/**
 * @brief Validates the exact Client Proof format value.
 *
 * @param proof_format Candidate format.
 * @return OK for format one, otherwise UNSUPPORTED.
 */
static MoonlightProtocolResult validate_proof_format(uint8_t proof_format) {
  return proof_format == MOONLIGHT_PROTOCOL_V1_CLIENT_PROOF_FORMAT ?
           MOONLIGHT_PROTOCOL_RESULT_OK :
           MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED;
}

MoonlightProtocolResult MoonlightProtocolV1ValidateCredentialSpki(
  MoonlightProtocolV1CredentialScheme credential_scheme,
  const uint8_t *credential_spki,
  size_t credential_spki_size
) {
  MoonlightProtocolResult result;

  if (credential_spki == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  result = validate_credential_scheme(credential_scheme);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }

  if (credential_scheme == MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256) {
    if (credential_spki_size != MOONLIGHT_PROTOCOL_V1_P256_CREDENTIAL_SPKI_SIZE || memcmp(credential_spki, p256_spki_prefix, sizeof(p256_spki_prefix)) != 0) {
      return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
    }
    return MOONLIGHT_PROTOCOL_RESULT_OK;
  }

  if (credential_spki_size != MOONLIGHT_PROTOCOL_V1_RSA2048_CREDENTIAL_SPKI_SIZE || memcmp(credential_spki, rsa2048_spki_prefix, sizeof(rsa2048_spki_prefix)) != 0 || memcmp(credential_spki + credential_spki_size - sizeof(rsa65537_suffix), rsa65537_suffix, sizeof(rsa65537_suffix)) != 0) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  if ((credential_spki[sizeof(rsa2048_spki_prefix)] & 0x80u) == 0 || (credential_spki[credential_spki_size - sizeof(rsa65537_suffix) - 1u] & 1u) == 0) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Compares a shortest unsigned scalar with one fixed-width maximum.
 *
 * @param scalar Shortest nonzero big-endian scalar bytes.
 * @param scalar_size Number of bytes in `scalar`.
 * @param maximum Fixed 32-byte big-endian maximum.
 * @param maximum_inclusive Whether equality with `maximum` is permitted.
 * @return True only when the scalar is in the selected upper range.
 */
static bool scalar_within_maximum(
  const uint8_t *scalar,
  size_t scalar_size,
  const uint8_t maximum[32],
  bool maximum_inclusive
) {
  int comparison;

  if (scalar_size < 32u) {
    return true;
  }
  if (scalar_size > 32u) {
    return false;
  }
  comparison = memcmp(scalar, maximum, 32u);
  return comparison < 0 || (maximum_inclusive && comparison == 0);
}

/**
 * @brief Validates one strict positive DER INTEGER scalar.
 *
 * @param input Bytes beginning with the INTEGER tag.
 * @param input_size Available containing bytes.
 * @param maximum Fixed-width scalar upper bound.
 * @param maximum_inclusive Whether the upper bound itself is valid.
 * @param consumed Receives the exact DER INTEGER byte count on success.
 * @return The codec result.
 */
static MoonlightProtocolResult validate_der_scalar(
  const uint8_t *input,
  size_t input_size,
  const uint8_t maximum[32],
  bool maximum_inclusive,
  size_t *consumed
) {
  const uint8_t *scalar;
  size_t scalar_size;
  size_t encoded_size;

  if (input_size < 3u || input[0] != 0x02u) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  scalar_size = input[1];
  if (scalar_size == 0 || scalar_size > 33u) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  encoded_size = 2u + scalar_size;
  if (encoded_size > input_size) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }

  scalar = input + 2u;
  if ((scalar[0] & 0x80u) != 0) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  if (scalar_size > 1u && scalar[0] == 0) {
    if ((scalar[1] & 0x80u) == 0) {
      return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
    }
    ++scalar;
    --scalar_size;
  }
  if (scalar_size == 1u && scalar[0] == 0) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  if (!scalar_within_maximum(
        scalar,
        scalar_size,
        maximum,
        maximum_inclusive
      )) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }

  *consumed = encoded_size;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Validates one strict canonical low-S P-256 DER signature.
 *
 * @param signature Complete signature bytes.
 * @param signature_size Number of bytes in `signature`.
 * @return The codec result.
 */
static MoonlightProtocolResult validate_p256_signature(
  const uint8_t *signature,
  size_t signature_size
) {
  MoonlightProtocolResult result;
  size_t consumed;
  size_t offset = 2u;

  if (signature_size < MOONLIGHT_PROTOCOL_V1_P256_SIGNATURE_SIZE_MIN || signature_size > MOONLIGHT_PROTOCOL_V1_P256_SIGNATURE_SIZE_MAX || signature[0] != 0x30u || signature[1] != signature_size - 2u) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }

  result = validate_der_scalar(
    signature + offset,
    signature_size - offset,
    p256_order,
    false,
    &consumed
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  offset += consumed;

  result = validate_der_scalar(
    signature + offset,
    signature_size - offset,
    p256_half_order,
    true,
    &consumed
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  offset += consumed;
  return offset == signature_size ?
           MOONLIGHT_PROTOCOL_RESULT_OK :
           MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
}

MoonlightProtocolResult MoonlightProtocolV1ValidateProofSignature(
  MoonlightProtocolV1CredentialScheme credential_scheme,
  const uint8_t *signature,
  size_t signature_size
) {
  MoonlightProtocolResult result;

  if (signature == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  result = validate_credential_scheme(credential_scheme);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }

  if (credential_scheme == MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256) {
    return validate_p256_signature(signature, signature_size);
  }
  return signature_size == MOONLIGHT_PROTOCOL_V1_RSA2048_SIGNATURE_SIZE ?
           MOONLIGHT_PROTOCOL_RESULT_OK :
           MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
}

/**
 * @brief Validates one complete Pairing CLIENT_PROOF request.
 *
 * @param request Candidate request.
 * @return The codec result.
 */
static MoonlightProtocolResult validate_pairing_request(
  const MoonlightProtocolV1PairingClientProofRequest *request
) {
  MoonlightProtocolResult result;

  if (request == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  result = validate_proof_format(request->proof_format);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  result = MoonlightProtocolV1ValidateCredentialSpki(
    request->credential_scheme,
    request->credential_spki,
    request->credential_spki_size
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  return MoonlightProtocolV1ValidateProofSignature(
    request->credential_scheme,
    request->signature,
    request->signature_size
  );
}

/**
 * @brief Validates one complete Streaming CLIENT_PROOF request.
 *
 * @param request Candidate request.
 * @param credential_scheme Immutable stored Credential scheme.
 * @return The codec result.
 */
static MoonlightProtocolResult validate_streaming_request(
  const MoonlightProtocolV1StreamingClientProofRequest *request,
  MoonlightProtocolV1CredentialScheme credential_scheme
) {
  MoonlightProtocolResult result;

  if (request == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  result = validate_proof_format(request->proof_format);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (request->credential_epoch == 0 || request->observed_authorization_generation == 0) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  return MoonlightProtocolV1ValidateProofSignature(
    credential_scheme,
    request->signature,
    request->signature_size
  );
}

/**
 * @brief Validates scheme-neutral Streaming CLIENT_PROOF fields.
 *
 * @param candidate Candidate decoded from the exact Streaming schema.
 * @return The codec result.
 */
static MoonlightProtocolResult validate_streaming_candidate(
  const MoonlightProtocolV1StreamingClientProofCandidate *candidate
) {
  MoonlightProtocolResult result;

  if (candidate == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  result = validate_proof_format(candidate->proof_format);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (candidate->credential_epoch == 0 || candidate->observed_authorization_generation == 0 || candidate->signature_size < MOONLIGHT_PROTOCOL_V1_P256_SIGNATURE_SIZE_MIN || candidate->signature_size > MOONLIGHT_PROTOCOL_V1_RSA2048_SIGNATURE_SIZE) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Clears a failed Streaming CLIENT_PROOF candidate output.
 *
 * @param candidate Candidate output to clear.
 * @param result Failure result to return.
 * @return The supplied failure result.
 */
static MoonlightProtocolResult fail_streaming_candidate(
  MoonlightProtocolV1StreamingClientProofCandidate *candidate,
  MoonlightProtocolResult result
) {
  memset(candidate, 0, sizeof(*candidate));
  return result;
}

/**
 * @brief Clears a failed scheme-bound Streaming CLIENT_PROOF output.
 *
 * @param request Scheme-bound request output to clear.
 * @param result Failure result to return.
 * @return The supplied failure result.
 */
static MoonlightProtocolResult fail_streaming_request(
  MoonlightProtocolV1StreamingClientProofRequest *request,
  MoonlightProtocolResult result
) {
  memset(request, 0, sizeof(*request));
  return result;
}

/**
 * @brief Appends one already validated scalar field to a local TLV writer.
 *
 * @param writer Initialized local writer.
 * @param field_id Exact selected-schema field identifier.
 * @param value Complete scalar value bytes.
 * @param value_size Number of bytes in `value`.
 * @return The codec result.
 */
static MoonlightProtocolResult append_scalar(
  MoonlightProtocolV1TlvWriter *writer,
  uint16_t field_id,
  const uint8_t *value,
  size_t value_size
) {
  return MoonlightProtocolV1TlvWriterAppend(
    writer,
    field_id,
    0,
    value,
    value_size
  );
}

MoonlightProtocolResult MoonlightProtocolV1EncodePairingClientProofRequest(
  const MoonlightProtocolV1PairingClientProofRequest *request,
  uint8_t *output,
  size_t output_size,
  size_t *encoded_size
) {
  uint8_t encoded[MOONLIGHT_PROTOCOL_V1_PAIRING_CLIENT_PROOF_REQUEST_PAYLOAD_MAX];
  uint8_t credential_scheme;
  MoonlightProtocolV1TlvWriter writer;
  MoonlightProtocolResult result;
  size_t size;

  if (output == NULL || encoded_size == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  result = validate_pairing_request(request);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  credential_scheme = (uint8_t) request->credential_scheme;

  result = MoonlightProtocolV1TlvWriterInitialize(
    &writer,
    encoded,
    sizeof(encoded),
    0
  );
  if (result == MOONLIGHT_PROTOCOL_RESULT_OK) {  // GCOVR_EXCL_BR_LINE: local writer initialization cannot fail.
    result = append_scalar(&writer, 1, &request->proof_format, 1u);
  }
  if (result == MOONLIGHT_PROTOCOL_RESULT_OK) {  // GCOVR_EXCL_BR_LINE: validated fields fit the exact schema maximum.
    result = append_scalar(&writer, 2, &credential_scheme, 1u);
  }
  if (result == MOONLIGHT_PROTOCOL_RESULT_OK) {  // GCOVR_EXCL_BR_LINE: validated fields fit the exact schema maximum.
    result = append_scalar(
      &writer,
      3,
      request->credential_spki,
      request->credential_spki_size
    );
  }
  if (result == MOONLIGHT_PROTOCOL_RESULT_OK) {  // GCOVR_EXCL_BR_LINE: validated fields fit the exact schema maximum.
    result = append_scalar(
      &writer,
      4,
      request->admission_hash,
      sizeof(request->admission_hash)
    );
  }
  if (result == MOONLIGHT_PROTOCOL_RESULT_OK) {  // GCOVR_EXCL_BR_LINE: validated fields fit the exact schema maximum.
    result = append_scalar(
      &writer,
      5,
      request->signature,
      request->signature_size
    );
  }
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {  // GCOVR_EXCL_BR_LINE: guarded by exact local capacity.
    return MOONLIGHT_PROTOCOL_RESULT_INTERNAL_ERROR;  // GCOVR_EXCL_LINE
  }

  size = MoonlightProtocolV1TlvWriterSize(&writer);
  if (output_size < size) {
    return MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL;
  }
  memcpy(output, encoded, size);
  *encoded_size = size;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Decodes one expected scalar TLV from contiguous input.
 *
 * @param input Complete containing payload.
 * @param input_size Number of containing bytes.
 * @param offset Current field offset, advanced on success.
 * @param expected_id Exact expected field identifier.
 * @param maximum_known_id Largest field identifier in the selected schema.
 * @param value Receives a borrowed field value.
 * @param value_size Receives the borrowed value size.
 * @return The codec result.
 */
static MoonlightProtocolResult decode_expected_scalar(
  const uint8_t *input,
  size_t input_size,
  size_t *offset,
  uint16_t expected_id,
  uint16_t maximum_known_id,
  const uint8_t **value,
  size_t *value_size
) {
  MoonlightProtocolV1TlvField field;
  MoonlightProtocolResult result;
  const size_t remaining = input_size - *offset;

  result = MoonlightProtocolV1DecodeTlvFieldHeader(
    input + *offset,
    remaining,
    &field
  );
  if (result == MOONLIGHT_PROTOCOL_RESULT_TRUNCATED) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (field.field_length > remaining - MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  if (field.field_id != expected_id) {
    return field.field_id >= 1u && field.field_id <= maximum_known_id ?
             MOONLIGHT_PROTOCOL_RESULT_MALFORMED :
             MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED;
  }
  if (field.flags != 0) {
    return MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED;
  }

  *value = input + *offset + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE;
  *value_size = field.field_length;
  *offset += MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + field.field_length;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Classifies bytes after every required scalar field.
 *
 * @param input Complete containing payload.
 * @param input_size Number of containing bytes.
 * @param offset First byte after the required schema.
 * @param maximum_known_id Largest field identifier in the selected schema.
 * @return OK for exact consumption, MALFORMED for known or broken trailing
 * data, and UNSUPPORTED for a complete unknown field or flag.
 */
static MoonlightProtocolResult classify_trailing_field(
  const uint8_t *input,
  size_t input_size,
  size_t offset,
  uint16_t maximum_known_id
) {
  MoonlightProtocolV1TlvField field;
  MoonlightProtocolResult result;
  const size_t remaining = input_size - offset;

  if (remaining == 0) {
    return MOONLIGHT_PROTOCOL_RESULT_OK;
  }
  result = MoonlightProtocolV1DecodeTlvFieldHeader(
    input + offset,
    remaining,
    &field
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result == MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED ?
             result :
             MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  if (field.field_length > remaining - MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  return field.field_id >= 1u && field.field_id <= maximum_known_id ?
           MOONLIGHT_PROTOCOL_RESULT_MALFORMED :
           MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED;
}

MoonlightProtocolResult MoonlightProtocolV1DecodePairingClientProofRequest(
  const uint8_t *input,
  size_t input_size,
  MoonlightProtocolV1PairingClientProofRequest *request
) {
  MoonlightProtocolV1PairingClientProofRequest decoded;
  const uint8_t *value;
  size_t value_size;
  size_t offset = 0;
  MoonlightProtocolResult result;

  if (input == NULL || request == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (input_size > MOONLIGHT_PROTOCOL_V1_PAIRING_CLIENT_PROOF_REQUEST_PAYLOAD_MAX) {
    return MOONLIGHT_PROTOCOL_RESULT_LIMIT_EXCEEDED;
  }
  if (input_size < PAIRING_CLIENT_PROOF_PAYLOAD_MIN) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }

  memset(&decoded, 0, sizeof(decoded));
  result = decode_expected_scalar(
    input,
    input_size,
    &offset,
    1,
    PAIRING_CLIENT_PROOF_FIELD_COUNT,
    &value,
    &value_size
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (value_size != 1u) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  decoded.proof_format = value[0];
  result = validate_proof_format(decoded.proof_format);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }

  result = decode_expected_scalar(
    input,
    input_size,
    &offset,
    2,
    PAIRING_CLIENT_PROOF_FIELD_COUNT,
    &value,
    &value_size
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (value_size != 1u) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  decoded.credential_scheme = (MoonlightProtocolV1CredentialScheme) value[0];
  result = validate_credential_scheme(decoded.credential_scheme);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }

  result = decode_expected_scalar(
    input,
    input_size,
    &offset,
    3,
    PAIRING_CLIENT_PROOF_FIELD_COUNT,
    &value,
    &value_size
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  result = MoonlightProtocolV1ValidateCredentialSpki(
    decoded.credential_scheme,
    value,
    value_size
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  memcpy(decoded.credential_spki, value, value_size);
  decoded.credential_spki_size = value_size;

  result = decode_expected_scalar(
    input,
    input_size,
    &offset,
    4,
    PAIRING_CLIENT_PROOF_FIELD_COUNT,
    &value,
    &value_size
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (value_size != sizeof(decoded.admission_hash)) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  memcpy(decoded.admission_hash, value, value_size);

  result = decode_expected_scalar(
    input,
    input_size,
    &offset,
    5,
    PAIRING_CLIENT_PROOF_FIELD_COUNT,
    &value,
    &value_size
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  result = MoonlightProtocolV1ValidateProofSignature(
    decoded.credential_scheme,
    value,
    value_size
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  result = classify_trailing_field(
    input,
    input_size,
    offset,
    PAIRING_CLIENT_PROOF_FIELD_COUNT
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  memcpy(decoded.signature, value, value_size);
  decoded.signature_size = value_size;

  result = validate_pairing_request(&decoded);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {  // GCOVR_EXCL_BR_LINE: every decoded field was already validated.
    return result;  // GCOVR_EXCL_LINE
  }
  *request = decoded;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

MoonlightProtocolResult MoonlightProtocolV1EncodeStreamingClientProofRequest(
  const MoonlightProtocolV1StreamingClientProofRequest *request,
  MoonlightProtocolV1CredentialScheme credential_scheme,
  uint8_t *output,
  size_t output_size,
  size_t *encoded_size
) {
  uint8_t encoded[MOONLIGHT_PROTOCOL_V1_STREAMING_CLIENT_PROOF_REQUEST_PAYLOAD_MAX];
  uint8_t credential_epoch[8];
  uint8_t authorization_generation[8];
  MoonlightProtocolV1TlvWriter writer;
  MoonlightProtocolResult result;
  size_t size;

  if (output == NULL || encoded_size == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  result = validate_streaming_request(request, credential_scheme);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }

  proof_store_u64(credential_epoch, request->credential_epoch);
  proof_store_u64(
    authorization_generation,
    request->observed_authorization_generation
  );
  result = MoonlightProtocolV1TlvWriterInitialize(
    &writer,
    encoded,
    sizeof(encoded),
    0
  );
  if (result == MOONLIGHT_PROTOCOL_RESULT_OK) {  // GCOVR_EXCL_BR_LINE: local writer initialization cannot fail.
    result = append_scalar(&writer, 1, &request->proof_format, 1u);
  }
  if (result == MOONLIGHT_PROTOCOL_RESULT_OK) {  // GCOVR_EXCL_BR_LINE: validated fields fit the exact schema maximum.
    result = append_scalar(
      &writer,
      2,
      request->principal_id,
      sizeof(request->principal_id)
    );
  }
  if (result == MOONLIGHT_PROTOCOL_RESULT_OK) {  // GCOVR_EXCL_BR_LINE: validated fields fit the exact schema maximum.
    result = append_scalar(
      &writer,
      3,
      credential_epoch,
      sizeof(credential_epoch)
    );
  }
  if (result == MOONLIGHT_PROTOCOL_RESULT_OK) {  // GCOVR_EXCL_BR_LINE: validated fields fit the exact schema maximum.
    result = append_scalar(
      &writer,
      4,
      authorization_generation,
      sizeof(authorization_generation)
    );
  }
  if (result == MOONLIGHT_PROTOCOL_RESULT_OK) {  // GCOVR_EXCL_BR_LINE: validated fields fit the exact schema maximum.
    result = append_scalar(
      &writer,
      5,
      request->admission_hash,
      sizeof(request->admission_hash)
    );
  }
  if (result == MOONLIGHT_PROTOCOL_RESULT_OK) {  // GCOVR_EXCL_BR_LINE: validated fields fit the exact schema maximum.
    result = append_scalar(
      &writer,
      6,
      request->signature,
      request->signature_size
    );
  }
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {  // GCOVR_EXCL_BR_LINE: guarded by exact local capacity.
    return MOONLIGHT_PROTOCOL_RESULT_INTERNAL_ERROR;  // GCOVR_EXCL_LINE
  }

  size = MoonlightProtocolV1TlvWriterSize(&writer);
  if (output_size < size) {
    return MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL;
  }
  memcpy(output, encoded, size);
  *encoded_size = size;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

MoonlightProtocolResult MoonlightProtocolV1DecodeStreamingClientProofCandidate(
  const uint8_t *input,
  size_t input_size,
  MoonlightProtocolV1StreamingClientProofCandidate *candidate
) {
  MoonlightProtocolV1StreamingClientProofCandidate decoded;
  const uint8_t *value;
  size_t value_size;
  size_t offset = 0;
  MoonlightProtocolResult result;

  if (candidate == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  if (input == NULL) {
    return fail_streaming_candidate(
      candidate,
      MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
    );
  }
  if (input_size > MOONLIGHT_PROTOCOL_V1_STREAMING_CLIENT_PROOF_REQUEST_PAYLOAD_MAX) {
    return fail_streaming_candidate(
      candidate,
      MOONLIGHT_PROTOCOL_RESULT_LIMIT_EXCEEDED
    );
  }
  if (input_size < STREAMING_CLIENT_PROOF_PAYLOAD_MIN) {
    return fail_streaming_candidate(
      candidate,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    );
  }

  memset(&decoded, 0, sizeof(decoded));
  result = decode_expected_scalar(
    input,
    input_size,
    &offset,
    1,
    STREAMING_CLIENT_PROOF_FIELD_COUNT,
    &value,
    &value_size
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return fail_streaming_candidate(candidate, result);
  }
  if (value_size != 1u) {
    return fail_streaming_candidate(
      candidate,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    );
  }
  decoded.proof_format = value[0];
  result = validate_proof_format(decoded.proof_format);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return fail_streaming_candidate(candidate, result);
  }

  result = decode_expected_scalar(
    input,
    input_size,
    &offset,
    2,
    STREAMING_CLIENT_PROOF_FIELD_COUNT,
    &value,
    &value_size
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return fail_streaming_candidate(candidate, result);
  }
  if (value_size != sizeof(decoded.principal_id)) {
    return fail_streaming_candidate(
      candidate,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    );
  }
  memcpy(decoded.principal_id, value, value_size);

  result = decode_expected_scalar(
    input,
    input_size,
    &offset,
    3,
    STREAMING_CLIENT_PROOF_FIELD_COUNT,
    &value,
    &value_size
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return fail_streaming_candidate(candidate, result);
  }
  if (value_size != 8u) {
    return fail_streaming_candidate(
      candidate,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    );
  }
  decoded.credential_epoch = proof_load_u64(value);

  result = decode_expected_scalar(
    input,
    input_size,
    &offset,
    4,
    STREAMING_CLIENT_PROOF_FIELD_COUNT,
    &value,
    &value_size
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return fail_streaming_candidate(candidate, result);
  }
  if (value_size != 8u) {
    return fail_streaming_candidate(
      candidate,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    );
  }
  decoded.observed_authorization_generation = proof_load_u64(value);

  result = decode_expected_scalar(
    input,
    input_size,
    &offset,
    5,
    STREAMING_CLIENT_PROOF_FIELD_COUNT,
    &value,
    &value_size
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return fail_streaming_candidate(candidate, result);
  }
  if (value_size != sizeof(decoded.admission_hash)) {
    return fail_streaming_candidate(
      candidate,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    );
  }
  memcpy(decoded.admission_hash, value, value_size);

  result = decode_expected_scalar(
    input,
    input_size,
    &offset,
    6,
    STREAMING_CLIENT_PROOF_FIELD_COUNT,
    &value,
    &value_size
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return fail_streaming_candidate(candidate, result);
  }
  if (value_size < MOONLIGHT_PROTOCOL_V1_P256_SIGNATURE_SIZE_MIN) {
    return fail_streaming_candidate(
      candidate,
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    );
  }
  if (value_size > MOONLIGHT_PROTOCOL_V1_RSA2048_SIGNATURE_SIZE) {  // GCOVR_EXCL_BR_LINE: exact preceding fields and the payload maximum already imply this bound.
    return fail_streaming_candidate(  // GCOVR_EXCL_LINE
      candidate,  // GCOVR_EXCL_LINE
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED  // GCOVR_EXCL_LINE
    );  // GCOVR_EXCL_LINE
  }
  result = classify_trailing_field(
    input,
    input_size,
    offset,
    STREAMING_CLIENT_PROOF_FIELD_COUNT
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return fail_streaming_candidate(candidate, result);
  }
  memcpy(decoded.signature, value, value_size);
  decoded.signature_size = value_size;

  result = validate_streaming_candidate(&decoded);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return fail_streaming_candidate(candidate, result);
  }
  *candidate = decoded;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

MoonlightProtocolResult MoonlightProtocolV1FinalizeStreamingClientProofCandidate(
  const MoonlightProtocolV1StreamingClientProofCandidate *candidate,
  MoonlightProtocolV1CredentialScheme credential_scheme,
  MoonlightProtocolV1StreamingClientProofRequest *request
) {
  MoonlightProtocolV1StreamingClientProofRequest finalized;
  MoonlightProtocolResult result;

  if (request == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  result = validate_credential_scheme(credential_scheme);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return fail_streaming_request(request, result);
  }
  result = validate_streaming_candidate(candidate);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return fail_streaming_request(request, result);
  }
  result = MoonlightProtocolV1ValidateProofSignature(
    credential_scheme,
    candidate->signature,
    candidate->signature_size
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return fail_streaming_request(request, result);
  }

  memset(&finalized, 0, sizeof(finalized));
  finalized.proof_format = candidate->proof_format;
  memcpy(
    finalized.principal_id,
    candidate->principal_id,
    sizeof(finalized.principal_id)
  );
  finalized.credential_epoch = candidate->credential_epoch;
  finalized.observed_authorization_generation =
    candidate->observed_authorization_generation;
  memcpy(
    finalized.admission_hash,
    candidate->admission_hash,
    sizeof(finalized.admission_hash)
  );
  memcpy(
    finalized.signature,
    candidate->signature,
    candidate->signature_size
  );
  finalized.signature_size = candidate->signature_size;

  result = validate_streaming_request(&finalized, credential_scheme);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {  // GCOVR_EXCL_BR_LINE: all fields were validated above.
    return fail_streaming_request(request, result);  // GCOVR_EXCL_LINE
  }
  *request = finalized;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

MoonlightProtocolResult MoonlightProtocolV1DecodeStreamingClientProofRequest(
  const uint8_t *input,
  size_t input_size,
  MoonlightProtocolV1CredentialScheme credential_scheme,
  MoonlightProtocolV1StreamingClientProofRequest *request
) {
  MoonlightProtocolV1StreamingClientProofCandidate candidate;
  MoonlightProtocolV1StreamingClientProofRequest decoded;
  MoonlightProtocolResult result;

  if (input == NULL || request == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  result = validate_credential_scheme(credential_scheme);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  result = MoonlightProtocolV1DecodeStreamingClientProofCandidate(
    input,
    input_size,
    &candidate
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  result = MoonlightProtocolV1FinalizeStreamingClientProofCandidate(
    &candidate,
    credential_scheme,
    &decoded
  );
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  *request = decoded;
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

/**
 * @brief Appends bytes to one fixed local transcript.
 *
 * @param output Fixed transcript buffer.
 * @param offset Current byte offset, advanced after the copy.
 * @param value Complete bytes to append.
 * @param value_size Number of bytes in `value`.
 */
static void append_transcript_bytes(
  uint8_t *output,
  size_t *offset,
  const uint8_t *value,
  size_t value_size
) {
  memcpy(output + *offset, value, value_size);
  *offset += value_size;
}

MoonlightProtocolResult MoonlightProtocolV1BuildPairingProofTranscript(
  const MoonlightProtocolV1PairingProofTranscript *transcript,
  uint8_t *output,
  size_t output_size
) {
  uint8_t encoded[MOONLIGHT_PROTOCOL_V1_PAIRING_PROOF_TRANSCRIPT_SIZE];
  uint8_t protocol_minor[2];
  uint8_t credential_scheme;
  MoonlightProtocolResult result;
  size_t offset = 0;

  if (transcript == NULL || output == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  result = validate_proof_format(transcript->proof_format);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  result = validate_credential_scheme(transcript->credential_scheme);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (output_size < sizeof(encoded)) {
    return MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL;
  }

  proof_store_u16(protocol_minor, MOONLIGHT_PROTOCOL_V1_MINOR);
  credential_scheme = (uint8_t) transcript->credential_scheme;
  append_transcript_bytes(
    encoded,
    &offset,
    pairing_proof_context,
    sizeof(pairing_proof_context) - 1u
  );
  encoded[offset++] = transcript->proof_format;
  append_transcript_bytes(
    encoded,
    &offset,
    transcript->exporter,
    sizeof(transcript->exporter)
  );
  encoded[offset++] = (uint8_t) (sizeof(pairing_alpn) - 1u);
  append_transcript_bytes(
    encoded,
    &offset,
    pairing_alpn,
    sizeof(pairing_alpn) - 1u
  );
  encoded[offset++] = CLIENT_PROOF_PROTOCOL_MAJOR;
  append_transcript_bytes(
    encoded,
    &offset,
    protocol_minor,
    sizeof(protocol_minor)
  );
  append_transcript_bytes(
    encoded,
    &offset,
    transcript->host_id,
    sizeof(transcript->host_id)
  );
  append_transcript_bytes(
    encoded,
    &offset,
    transcript->host_identity,
    sizeof(transcript->host_identity)
  );
  encoded[offset++] = credential_scheme;
  append_transcript_bytes(
    encoded,
    &offset,
    transcript->credential_digest,
    sizeof(transcript->credential_digest)
  );
  append_transcript_bytes(
    encoded,
    &offset,
    transcript->admission_hash,
    sizeof(transcript->admission_hash)
  );
  if (offset != sizeof(encoded)) {  // GCOVR_EXCL_BR_LINE: fixed field widths sum to the destination size.
    proof_secure_clear(encoded, sizeof(encoded));  // GCOVR_EXCL_LINE
    return MOONLIGHT_PROTOCOL_RESULT_INTERNAL_ERROR;  // GCOVR_EXCL_LINE
  }

  memcpy(output, encoded, sizeof(encoded));
  proof_secure_clear(encoded, sizeof(encoded));
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}

MoonlightProtocolResult MoonlightProtocolV1BuildStreamingProofTranscript(
  const MoonlightProtocolV1StreamingProofTranscript *transcript,
  uint8_t *output,
  size_t output_size
) {
  uint8_t encoded[MOONLIGHT_PROTOCOL_V1_STREAMING_PROOF_TRANSCRIPT_SIZE];
  uint8_t protocol_minor[2];
  uint8_t credential_epoch[8];
  uint8_t authorization_generation[8];
  uint8_t credential_scheme;
  MoonlightProtocolResult result;
  size_t offset = 0;

  if (transcript == NULL || output == NULL) {
    return MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT;
  }
  result = validate_proof_format(transcript->proof_format);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  result = validate_credential_scheme(transcript->credential_scheme);
  if (result != MOONLIGHT_PROTOCOL_RESULT_OK) {
    return result;
  }
  if (transcript->credential_epoch == 0 || transcript->observed_authorization_generation == 0) {
    return MOONLIGHT_PROTOCOL_RESULT_MALFORMED;
  }
  if (output_size < sizeof(encoded)) {
    return MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL;
  }

  proof_store_u16(protocol_minor, MOONLIGHT_PROTOCOL_V1_MINOR);
  proof_store_u64(credential_epoch, transcript->credential_epoch);
  proof_store_u64(
    authorization_generation,
    transcript->observed_authorization_generation
  );
  credential_scheme = (uint8_t) transcript->credential_scheme;
  append_transcript_bytes(
    encoded,
    &offset,
    streaming_proof_context,
    sizeof(streaming_proof_context) - 1u
  );
  encoded[offset++] = transcript->proof_format;
  append_transcript_bytes(
    encoded,
    &offset,
    transcript->exporter,
    sizeof(transcript->exporter)
  );
  encoded[offset++] = (uint8_t) (sizeof(streaming_alpn) - 1u);
  append_transcript_bytes(
    encoded,
    &offset,
    streaming_alpn,
    sizeof(streaming_alpn) - 1u
  );
  encoded[offset++] = CLIENT_PROOF_PROTOCOL_MAJOR;
  append_transcript_bytes(
    encoded,
    &offset,
    protocol_minor,
    sizeof(protocol_minor)
  );
  append_transcript_bytes(
    encoded,
    &offset,
    transcript->host_id,
    sizeof(transcript->host_id)
  );
  append_transcript_bytes(
    encoded,
    &offset,
    transcript->host_identity,
    sizeof(transcript->host_identity)
  );
  append_transcript_bytes(
    encoded,
    &offset,
    transcript->principal_id,
    sizeof(transcript->principal_id)
  );
  append_transcript_bytes(
    encoded,
    &offset,
    credential_epoch,
    sizeof(credential_epoch)
  );
  append_transcript_bytes(
    encoded,
    &offset,
    authorization_generation,
    sizeof(authorization_generation)
  );
  encoded[offset++] = credential_scheme;
  append_transcript_bytes(
    encoded,
    &offset,
    transcript->credential_digest,
    sizeof(transcript->credential_digest)
  );
  append_transcript_bytes(
    encoded,
    &offset,
    transcript->admission_hash,
    sizeof(transcript->admission_hash)
  );
  if (offset != sizeof(encoded)) {  // GCOVR_EXCL_BR_LINE: fixed field widths sum to the destination size.
    proof_secure_clear(encoded, sizeof(encoded));  // GCOVR_EXCL_LINE
    return MOONLIGHT_PROTOCOL_RESULT_INTERNAL_ERROR;  // GCOVR_EXCL_LINE
  }

  memcpy(output, encoded, sizeof(encoded));
  proof_secure_clear(encoded, sizeof(encoded));
  return MOONLIGHT_PROTOCOL_RESULT_OK;
}
