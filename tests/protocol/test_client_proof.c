/**
 * @file test_client_proof.c
 * @brief Native tests for protocol version 1 exporter-bound Client Proof schemas.
 */

#include <moonlight/protocol/client_proof.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef MOONLIGHT_PROTOCOL_V1_TESTDATA_DIR
  #error MOONLIGHT_PROTOCOL_V1_TESTDATA_DIR must name the version 1 golden-vector directory
#endif

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
 * @brief Maximum scratch space used by a mutated Pairing payload.
 */
#define TEST_PAIRING_MUTATION_CAPACITY \
  (MOONLIGHT_PROTOCOL_V1_PAIRING_CLIENT_PROOF_REQUEST_PAYLOAD_MAX + \
   MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + 1u)

/**
 * @brief Maximum scratch space used by a mutated Streaming payload.
 */
#define TEST_STREAMING_MUTATION_CAPACITY \
  (MOONLIGHT_PROTOCOL_V1_STREAMING_CLIENT_PROOF_REQUEST_PAYLOAD_MAX + \
   MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + 1u)

/**
 * @brief P-256 group order in big-endian representation.
 */
static const uint8_t TEST_P256_ORDER[32] = {
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
  0x51,
};

/**
 * @brief Largest low-S P-256 scalar in big-endian representation.
 */
static const uint8_t TEST_P256_HALF_ORDER[32] = {
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
  0xa8,
};

/**
 * @brief Stores one big-endian 16-bit value.
 *
 * @param output Two writable bytes.
 * @param value Host-order value.
 */
static void test_store_u16(uint8_t *output, uint16_t value) {
  output[0] = (uint8_t) (value >> 8u);
  output[1] = (uint8_t) value;
}

/**
 * @brief Stores one big-endian 32-bit value.
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
 * @brief Stores one big-endian 64-bit value.
 *
 * @param output Eight writable bytes.
 * @param value Host-order value.
 */
static void test_store_u64(uint8_t *output, uint64_t value) {
  test_store_u32(output, (uint32_t) (value >> 32u));
  test_store_u32(output + 4u, (uint32_t) value);
}

/**
 * @brief Loads one big-endian 16-bit value.
 *
 * @param input Two readable bytes.
 * @return Host-order value.
 */
static uint16_t test_load_u16(const uint8_t *input) {
  return (uint16_t) (((uint16_t) input[0] << 8u) | input[1]);
}

/**
 * @brief Loads one big-endian 32-bit value.
 *
 * @param input Four readable bytes.
 * @return Host-order value.
 */
static uint32_t test_load_u32(const uint8_t *input) {
  return ((uint32_t) input[0] << 24u) |
         ((uint32_t) input[1] << 16u) |
         ((uint32_t) input[2] << 8u) |
         input[3];
}

/**
 * @brief Converts one ASCII hexadecimal digit to its integer value.
 *
 * @param character Character to convert.
 * @return A value in `[0, 15]`, or `-1` for a non-hexadecimal character.
 */
static int hex_value(int character) {
  if (character >= '0' && character <= '9') {
    return character - '0';
  }
  if (character >= 'a' && character <= 'f') {
    return character - 'a' + 10;
  }
  if (character >= 'A' && character <= 'F') {
    return character - 'A' + 10;
  }
  return -1;
}

/**
 * @brief Tests whether a character is permitted ASCII whitespace.
 *
 * @param character Character to test.
 * @return True for one of the six ASCII whitespace characters.
 */
static bool is_ascii_whitespace(int character) {
  return character == ' ' ||
         character == '\t' ||
         character == '\n' ||
         character == '\r' ||
         character == '\f' ||
         character == '\v';
}

/**
 * @brief Loads one strict hexadecimal proof golden vector.
 *
 * @param name File name relative to the version 1 test-data directory.
 * @param output Destination byte buffer.
 * @param output_capacity Available bytes in `output`.
 * @param output_size Receives the decoded byte count.
 * @return True only for complete hexadecimal pairs and ASCII whitespace.
 */
static bool load_golden(
  const char *name,
  uint8_t *output,
  size_t output_capacity,
  size_t *output_size
) {
  char path[1024];
  FILE *file;
  int character;
  int high_nibble = -1;
  int path_length;
  size_t size = 0;
  bool valid = true;

  path_length = snprintf(
    path,
    sizeof(path),
    "%s/%s",
    MOONLIGHT_PROTOCOL_V1_TESTDATA_DIR,
    name
  );
  if (path_length < 0 || (size_t) path_length >= sizeof(path)) {
    return false;
  }
  file = fopen(path, "rb");
  if (file == NULL) {
    return false;
  }

  while ((character = fgetc(file)) != EOF) {
    const int nibble = hex_value(character);

    if (nibble >= 0) {
      if (high_nibble < 0) {
        high_nibble = nibble;
      } else {
        if (size == output_capacity) {
          valid = false;
          break;
        }
        output[size++] = (uint8_t) ((high_nibble << 4u) | nibble);
        high_nibble = -1;
      }
    } else if (!is_ascii_whitespace(character)) {
      valid = false;
      break;
    }
  }
  if (ferror(file) != 0 || high_nibble >= 0) {
    valid = false;
  }
  if (fclose(file) != 0) {
    valid = false;
  }
  if (!valid) {
    return false;
  }
  *output_size = size;
  return true;
}

/**
 * @brief Fills bytes with a deterministic increasing pattern.
 *
 * @param output Writable bytes.
 * @param output_size Number of bytes to fill.
 * @param first First byte in the pattern.
 */
static void fill_pattern(uint8_t *output, size_t output_size, uint8_t first) {
  size_t index;

  for (index = 0; index < output_size; ++index) {
    output[index] = (uint8_t) (first + index);
  }
}

/**
 * @brief Builds the exact canonical P-256 SubjectPublicKeyInfo template.
 *
 * Point membership is deliberately outside the representation codec, so the
 * coordinates use deterministic non-secret test bytes.
 *
 * @param output Exact-size SPKI destination.
 */
static void make_p256_spki(
  uint8_t output[MOONLIGHT_PROTOCOL_V1_P256_CREDENTIAL_SPKI_SIZE]
) {
  static const uint8_t prefix[] = {
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

  memcpy(output, prefix, sizeof(prefix));
  fill_pattern(output + sizeof(prefix), 64u, 1u);
}

/**
 * @brief Builds the exact canonical RSA-2048 SubjectPublicKeyInfo template.
 *
 * Cryptographic modulus validation is deliberately outside the representation
 * codec. The encoded integer is positive, exactly 2048 bits, and odd.
 *
 * @param output Exact-size SPKI destination.
 */
static void make_rsa_spki(
  uint8_t output[MOONLIGHT_PROTOCOL_V1_RSA2048_CREDENTIAL_SPKI_SIZE]
) {
  static const uint8_t prefix[] = {
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
  static const uint8_t suffix[] = {
    0x02,
    0x03,
    0x01,
    0x00,
    0x01,
  };

  memcpy(output, prefix, sizeof(prefix));
  fill_pattern(output + sizeof(prefix), 256u, 0x80u);
  output[sizeof(prefix)] = 0x80;
  output[sizeof(prefix) + 255u] |= 1u;
  memcpy(output + sizeof(prefix) + 256u, suffix, sizeof(suffix));
}

/**
 * @brief Builds the shortest valid low-S ECDSA signature.
 *
 * @param output Destination with at least eight bytes.
 * @return Encoded signature size.
 */
static size_t make_minimum_p256_signature(uint8_t *output) {
  static const uint8_t signature[] = {
    0x30,
    0x06,
    0x02,
    0x01,
    0x01,
    0x02,
    0x01,
    0x01,
  };

  memcpy(output, signature, sizeof(signature));
  return sizeof(signature);
}

/**
 * @brief Builds the largest canonical low-S P-256 signature.
 *
 * The first scalar is `n - 1` and needs a positive-sign padding byte. The
 * second scalar is `floor(n / 2)` and needs no padding.
 *
 * @param output Destination with at least 71 bytes.
 * @return Encoded signature size.
 */
static size_t make_maximum_low_s_p256_signature(uint8_t *output) {
  output[0] = 0x30;
  output[1] = 0x45;
  output[2] = 0x02;
  output[3] = 0x21;
  output[4] = 0x00;
  memcpy(output + 5u, TEST_P256_ORDER, sizeof(TEST_P256_ORDER));
  --output[36];
  output[37] = 0x02;
  output[38] = 0x20;
  memcpy(output + 39u, TEST_P256_HALF_ORDER, sizeof(TEST_P256_HALF_ORDER));
  return 71u;
}

/**
 * @brief Builds a deterministic exact-size RSA signature representation.
 *
 * @param output Exact-size signature destination.
 */
static void make_rsa_signature(
  uint8_t output[MOONLIGHT_PROTOCOL_V1_RSA2048_SIGNATURE_SIZE]
) {
  fill_pattern(
    output,
    MOONLIGHT_PROTOCOL_V1_RSA2048_SIGNATURE_SIZE,
    0x5au
  );
}

/**
 * @brief Builds one valid Pairing proof request for a selected scheme.
 *
 * @param scheme Candidate Credential scheme.
 * @param largest_p256_signature Whether P-256 should use its largest valid low-S representation.
 * @return Valid request.
 */
static MoonlightProtocolV1PairingClientProofRequest make_pairing_request(
  MoonlightProtocolV1CredentialScheme scheme,
  bool largest_p256_signature
) {
  MoonlightProtocolV1PairingClientProofRequest request;

  memset(&request, 0, sizeof(request));
  request.proof_format = MOONLIGHT_PROTOCOL_V1_CLIENT_PROOF_FORMAT;
  request.credential_scheme = scheme;
  fill_pattern(request.admission_hash, sizeof(request.admission_hash), 0xc0u);
  if (scheme == MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256) {
    make_p256_spki(request.credential_spki);
    request.credential_spki_size =
      MOONLIGHT_PROTOCOL_V1_P256_CREDENTIAL_SPKI_SIZE;
    request.signature_size = largest_p256_signature ?
                               make_maximum_low_s_p256_signature(
                                 request.signature
                               ) :
                               make_minimum_p256_signature(request.signature);
  } else {
    make_rsa_spki(request.credential_spki);
    request.credential_spki_size =
      MOONLIGHT_PROTOCOL_V1_RSA2048_CREDENTIAL_SPKI_SIZE;
    make_rsa_signature(request.signature);
    request.signature_size = MOONLIGHT_PROTOCOL_V1_RSA2048_SIGNATURE_SIZE;
  }
  return request;
}

/**
 * @brief Builds one valid Streaming proof request for a selected stored scheme.
 *
 * @param scheme Stored Client Credential scheme.
 * @param largest_p256_signature Whether P-256 should use its largest valid low-S representation.
 * @return Valid request.
 */
static MoonlightProtocolV1StreamingClientProofRequest make_streaming_request(
  MoonlightProtocolV1CredentialScheme scheme,
  bool largest_p256_signature
) {
  MoonlightProtocolV1StreamingClientProofRequest request;

  memset(&request, 0, sizeof(request));
  request.proof_format = MOONLIGHT_PROTOCOL_V1_CLIENT_PROOF_FORMAT;
  fill_pattern(request.principal_id, sizeof(request.principal_id), 0x10u);
  request.credential_epoch = UINT64_C(0x0102030405060708);
  request.observed_authorization_generation =
    UINT64_C(0x8899aabbccddeeff);
  fill_pattern(request.admission_hash, sizeof(request.admission_hash), 0xe0u);
  if (scheme == MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256) {
    request.signature_size = largest_p256_signature ?
                               make_maximum_low_s_p256_signature(
                                 request.signature
                               ) :
                               make_minimum_p256_signature(request.signature);
  } else {
    make_rsa_signature(request.signature);
    request.signature_size = MOONLIGHT_PROTOCOL_V1_RSA2048_SIGNATURE_SIZE;
  }
  return request;
}

/**
 * @brief Finds one canonical TLV field in a complete test payload.
 *
 * @param payload Complete TLV payload.
 * @param payload_size Number of bytes in `payload`.
 * @param field_id Field identifier to locate.
 * @param header_offset Receives the field-header offset.
 * @param value_offset Receives the field-value offset.
 * @param value_size Receives the field-value size.
 * @return True when exactly one well-delimited matching field is present.
 */
static bool find_tlv_field(
  const uint8_t *payload,
  size_t payload_size,
  uint16_t field_id,
  size_t *header_offset,
  size_t *value_offset,
  size_t *value_size
) {
  size_t offset = 0;
  bool found = false;

  while (offset + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE <= payload_size) {
    const uint32_t field_size = test_load_u32(payload + offset + 4u);
    const size_t next =
      offset + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE + field_size;

    if (next > payload_size) {
      return false;
    }
    if (test_load_u16(payload + offset) == field_id) {
      if (found) {
        return false;
      }
      found = true;
      *header_offset = offset;
      *value_offset = offset + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE;
      *value_size = field_size;
    }
    offset = next;
  }
  return found && offset == payload_size;
}

/**
 * @brief Compares all semantic Pairing proof request fields.
 *
 * @param actual Decoded request.
 * @param expected Original request.
 * @return True when every meaningful field matches.
 */
static bool pairing_requests_equal(
  const MoonlightProtocolV1PairingClientProofRequest *actual,
  const MoonlightProtocolV1PairingClientProofRequest *expected
) {
  return actual->proof_format == expected->proof_format &&
         actual->credential_scheme == expected->credential_scheme &&
         actual->credential_spki_size == expected->credential_spki_size &&
         memcmp(
           actual->credential_spki,
           expected->credential_spki,
           expected->credential_spki_size
         ) == 0 &&
         memcmp(
           actual->admission_hash,
           expected->admission_hash,
           sizeof(actual->admission_hash)
         ) == 0 &&
         actual->signature_size == expected->signature_size &&
         memcmp(
           actual->signature,
           expected->signature,
           expected->signature_size
         ) == 0;
}

/**
 * @brief Compares all semantic Streaming proof request fields.
 *
 * @param actual Decoded request.
 * @param expected Original request.
 * @return True when every meaningful field matches.
 */
static bool streaming_requests_equal(
  const MoonlightProtocolV1StreamingClientProofRequest *actual,
  const MoonlightProtocolV1StreamingClientProofRequest *expected
) {
  return actual->proof_format == expected->proof_format &&
         memcmp(
           actual->principal_id,
           expected->principal_id,
           sizeof(actual->principal_id)
         ) == 0 &&
         actual->credential_epoch == expected->credential_epoch &&
         actual->observed_authorization_generation ==
           expected->observed_authorization_generation &&
         memcmp(
           actual->admission_hash,
           expected->admission_hash,
           sizeof(actual->admission_hash)
         ) == 0 &&
         actual->signature_size == expected->signature_size &&
         memcmp(
           actual->signature,
           expected->signature,
           expected->signature_size
         ) == 0;
}

/**
 * @brief Compares one scheme-neutral candidate with its source request.
 *
 * @param candidate Decoded candidate.
 * @param request Original scheme-bound request.
 * @return True when every wire field matches.
 */
static bool streaming_candidate_matches_request(
  const MoonlightProtocolV1StreamingClientProofCandidate *candidate,
  const MoonlightProtocolV1StreamingClientProofRequest *request
) {
  return candidate->proof_format == request->proof_format &&
         memcmp(
           candidate->principal_id,
           request->principal_id,
           sizeof(candidate->principal_id)
         ) == 0 &&
         candidate->credential_epoch == request->credential_epoch &&
         candidate->observed_authorization_generation ==
           request->observed_authorization_generation &&
         memcmp(
           candidate->admission_hash,
           request->admission_hash,
           sizeof(candidate->admission_hash)
         ) == 0 &&
         candidate->signature_size == request->signature_size &&
         memcmp(
           candidate->signature,
           request->signature,
           request->signature_size
         ) == 0;
}

/**
 * @brief Tests whether a complete object representation is zero.
 *
 * @param value Object bytes to inspect.
 * @param value_size Number of bytes in `value`.
 * @return True only when every byte is zero.
 */
static bool object_is_zero(const void *value, size_t value_size) {
  const uint8_t *bytes = value;
  size_t index;

  for (index = 0; index < value_size; ++index) {
    if (bytes[index] != 0) {
      return false;
    }
  }
  return true;
}

/**
 * @brief Encodes one minimum P-256 Pairing request for mutation tests.
 *
 * @param output Destination with maximum Pairing capacity.
 * @param output_size Receives the encoded size.
 * @return True on success.
 */
static bool encode_pairing_fixture(uint8_t *output, size_t *output_size) {
  const MoonlightProtocolV1PairingClientProofRequest request =
    make_pairing_request(
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      false
    );

  TEST_RESULT(
    MoonlightProtocolV1EncodePairingClientProofRequest(
      &request,
      output,
      MOONLIGHT_PROTOCOL_V1_PAIRING_CLIENT_PROOF_REQUEST_PAYLOAD_MAX,
      output_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  return true;
}

/**
 * @brief Encodes one minimum P-256 Streaming request for mutation tests.
 *
 * @param output Destination with maximum Streaming capacity.
 * @param output_size Receives the encoded size.
 * @return True on success.
 */
static bool encode_streaming_fixture(uint8_t *output, size_t *output_size) {
  const MoonlightProtocolV1StreamingClientProofRequest request =
    make_streaming_request(
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      false
    );

  TEST_RESULT(
    MoonlightProtocolV1EncodeStreamingClientProofRequest(
      &request,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      output,
      MOONLIGHT_PROTOCOL_V1_STREAMING_CLIENT_PROOF_REQUEST_PAYLOAD_MAX,
      output_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  return true;
}

/**
 * @brief Verifies both exact Credential SPKI representation templates.
 *
 * @return True on success.
 */
static bool test_credential_spki_templates(void) {
  uint8_t p256[MOONLIGHT_PROTOCOL_V1_P256_CREDENTIAL_SPKI_SIZE];
  uint8_t rsa[MOONLIGHT_PROTOCOL_V1_RSA2048_CREDENTIAL_SPKI_SIZE];
  uint8_t mutated[MOONLIGHT_PROTOCOL_V1_RSA2048_CREDENTIAL_SPKI_SIZE];

  make_p256_spki(p256);
  make_rsa_spki(rsa);
  TEST_RESULT(
    MoonlightProtocolV1ValidateCredentialSpki(
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      p256,
      sizeof(p256)
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_RESULT(
    MoonlightProtocolV1ValidateCredentialSpki(
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_RSA2048_PKCS1_SHA256_COMPAT,
      rsa,
      sizeof(rsa)
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_RESULT(
    MoonlightProtocolV1ValidateCredentialSpki(
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      NULL,
      sizeof(p256)
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1ValidateCredentialSpki(
      (MoonlightProtocolV1CredentialScheme) 0,
      p256,
      sizeof(p256)
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  TEST_RESULT(
    MoonlightProtocolV1ValidateCredentialSpki(
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      p256,
      sizeof(p256) - 1u
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  TEST_RESULT(
    MoonlightProtocolV1ValidateCredentialSpki(
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      p256,
      sizeof(p256) + 1u
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  TEST_RESULT(
    MoonlightProtocolV1ValidateCredentialSpki(
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_RSA2048_PKCS1_SHA256_COMPAT,
      rsa,
      sizeof(rsa) - 1u
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );

  memcpy(mutated, p256, sizeof(p256));
  mutated[0] = 0x31;
  TEST_RESULT(
    MoonlightProtocolV1ValidateCredentialSpki(
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      mutated,
      sizeof(p256)
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, p256, sizeof(p256));
  mutated[12] ^= 1u;
  TEST_RESULT(
    MoonlightProtocolV1ValidateCredentialSpki(
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      mutated,
      sizeof(p256)
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, p256, sizeof(p256));
  mutated[22] ^= 1u;
  TEST_RESULT(
    MoonlightProtocolV1ValidateCredentialSpki(
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      mutated,
      sizeof(p256)
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, p256, sizeof(p256));
  mutated[25] = 1;
  TEST_RESULT(
    MoonlightProtocolV1ValidateCredentialSpki(
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      mutated,
      sizeof(p256)
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, p256, sizeof(p256));
  mutated[26] = 0x02;
  TEST_RESULT(
    MoonlightProtocolV1ValidateCredentialSpki(
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      mutated,
      sizeof(p256)
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );

  memcpy(mutated, rsa, sizeof(rsa));
  mutated[18] ^= 1u;
  TEST_RESULT(
    MoonlightProtocolV1ValidateCredentialSpki(
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_RSA2048_PKCS1_SHA256_COMPAT,
      mutated,
      sizeof(rsa)
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, rsa, sizeof(rsa));
  mutated[20] = 0x04;
  TEST_RESULT(
    MoonlightProtocolV1ValidateCredentialSpki(
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_RSA2048_PKCS1_SHA256_COMPAT,
      mutated,
      sizeof(rsa)
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, rsa, sizeof(rsa));
  mutated[25] = 1;
  TEST_RESULT(
    MoonlightProtocolV1ValidateCredentialSpki(
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_RSA2048_PKCS1_SHA256_COMPAT,
      mutated,
      sizeof(rsa)
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, rsa, sizeof(rsa));
  mutated[32] = 1;
  TEST_RESULT(
    MoonlightProtocolV1ValidateCredentialSpki(
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_RSA2048_PKCS1_SHA256_COMPAT,
      mutated,
      sizeof(rsa)
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, rsa, sizeof(rsa));
  mutated[290] = 0x02;
  TEST_RESULT(
    MoonlightProtocolV1ValidateCredentialSpki(
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_RSA2048_PKCS1_SHA256_COMPAT,
      mutated,
      sizeof(rsa)
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, rsa, sizeof(rsa));
  mutated[33] = 0x7f;
  TEST_RESULT(
    MoonlightProtocolV1ValidateCredentialSpki(
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_RSA2048_PKCS1_SHA256_COMPAT,
      mutated,
      sizeof(rsa)
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, rsa, sizeof(rsa));
  mutated[288] &= (uint8_t) ~1u;
  TEST_RESULT(
    MoonlightProtocolV1ValidateCredentialSpki(
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_RSA2048_PKCS1_SHA256_COMPAT,
      mutated,
      sizeof(rsa)
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  return true;
}

/**
 * @brief Verifies strict canonical low-S ECDSA and exact-size RSA signatures.
 *
 * @return True on success.
 */
static bool test_signature_representations(void) {
  uint8_t signature[MOONLIGHT_PROTOCOL_V1_RSA2048_SIGNATURE_SIZE];
  uint8_t mutated[MOONLIGHT_PROTOCOL_V1_P256_SIGNATURE_SIZE_MAX + 1u];
  size_t signature_size;

  signature_size = make_minimum_p256_signature(signature);
  TEST_RESULT(
    MoonlightProtocolV1ValidateProofSignature(
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      signature,
      signature_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  signature_size = make_maximum_low_s_p256_signature(signature);
  TEST_RESULT(
    MoonlightProtocolV1ValidateProofSignature(
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      signature,
      signature_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  make_rsa_signature(signature);
  TEST_RESULT(
    MoonlightProtocolV1ValidateProofSignature(
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_RSA2048_PKCS1_SHA256_COMPAT,
      signature,
      sizeof(signature)
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_RESULT(
    MoonlightProtocolV1ValidateProofSignature(
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      NULL,
      8u
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1ValidateProofSignature(
      (MoonlightProtocolV1CredentialScheme) 3,
      signature,
      sizeof(signature)
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  TEST_RESULT(
    MoonlightProtocolV1ValidateProofSignature(
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_RSA2048_PKCS1_SHA256_COMPAT,
      signature,
      sizeof(signature) - 1u
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );

  {
    static const uint8_t malformed[][10] = {
      {0x31, 0x06, 0x02, 0x01, 0x01, 0x02, 0x01, 0x01},
      {0x30, 0x07, 0x02, 0x01, 0x01, 0x02, 0x01, 0x01},
      {0x30, 0x06, 0x03, 0x01, 0x01, 0x02, 0x01, 0x01},
      {0x30, 0x06, 0x02, 0x01, 0x00, 0x02, 0x01, 0x01},
      {0x30, 0x06, 0x02, 0x01, 0x80, 0x02, 0x01, 0x01},
      {0x30, 0x07, 0x02, 0x02, 0x00, 0x01, 0x02, 0x01, 0x01},
      {0x30, 0x06, 0x02, 0x01, 0x01, 0x02, 0x01, 0x00},
      {0x30, 0x81, 0x06, 0x02, 0x01, 0x01, 0x02, 0x01, 0x01},
    };
    static const size_t malformed_sizes[] = {8u, 8u, 8u, 8u, 8u, 9u, 8u, 9u};
    size_t index;

    for (index = 0; index < sizeof(malformed_sizes) / sizeof(malformed_sizes[0]); ++index) {
      TEST_RESULT(
        MoonlightProtocolV1ValidateProofSignature(
          MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
          malformed[index],
          malformed_sizes[index]
        ),
        MOONLIGHT_PROTOCOL_RESULT_MALFORMED
      );
    }
  }
  {
    static const uint8_t scalar_too_wide[40] = {
      0x30,
      0x26,
      0x02,
      0x21,
      0x01,
      [37] = 0x02,
      [38] = 0x01,
      [39] = 0x01,
    };
    static const uint8_t scalar_length_zero[] = {
      0x30,
      0x06,
      0x02,
      0x00,
      0x02,
      0x02,
      0x01,
      0x01,
    };
    static const uint8_t scalar_length_too_large[41] = {
      0x30,
      0x27,
      0x02,
      0x22,
      0x01,
      [38] = 0x02,
      [39] = 0x01,
      [40] = 0x01,
    };
    static const uint8_t scalar_declared_length_overrun[] = {
      0x30,
      0x06,
      0x02,
      0x05,
      0x01,
      0x02,
      0x03,
      0x04,
    };
    static const uint8_t missing_second_scalar_header[] = {
      0x30,
      0x06,
      0x02,
      0x03,
      0x01,
      0x02,
      0x03,
      0x00,
    };
    static const uint8_t third_scalar[] = {
      0x30,
      0x09,
      0x02,
      0x01,
      0x01,
      0x02,
      0x01,
      0x01,
      0x02,
      0x01,
      0x01,
    };

    static const struct {
      const uint8_t *bytes;  ///< Malformed DER signature bytes.
      size_t size;  ///< Number of bytes in `bytes`.
    } malformed_scalars[] = {
      {scalar_too_wide, sizeof(scalar_too_wide)},
      {scalar_length_zero, sizeof(scalar_length_zero)},
      {scalar_length_too_large, sizeof(scalar_length_too_large)},
      {
        scalar_declared_length_overrun,
        sizeof(scalar_declared_length_overrun),
      },
      {
        missing_second_scalar_header,
        sizeof(missing_second_scalar_header),
      },
      {third_scalar, sizeof(third_scalar)},
    };

    size_t index;

    for (index = 0;
         index < sizeof(malformed_scalars) / sizeof(malformed_scalars[0]);
         ++index) {
      TEST_RESULT(
        MoonlightProtocolV1ValidateProofSignature(
          MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
          malformed_scalars[index].bytes,
          malformed_scalars[index].size
        ),
        MOONLIGHT_PROTOCOL_RESULT_MALFORMED
      );
    }
  }

  signature_size = make_maximum_low_s_p256_signature(mutated);
  memcpy(mutated + 39u, TEST_P256_HALF_ORDER, sizeof(TEST_P256_HALF_ORDER));
  ++mutated[signature_size - 1u];
  TEST_RESULT(
    MoonlightProtocolV1ValidateProofSignature(
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      mutated,
      signature_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );

  signature_size = make_maximum_low_s_p256_signature(mutated);
  memcpy(mutated + 5u, TEST_P256_ORDER, sizeof(TEST_P256_ORDER));
  TEST_RESULT(
    MoonlightProtocolV1ValidateProofSignature(
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      mutated,
      signature_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );

  signature_size = make_minimum_p256_signature(mutated);
  mutated[signature_size] = 0;
  TEST_RESULT(
    MoonlightProtocolV1ValidateProofSignature(
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      mutated,
      signature_size + 1u
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  TEST_RESULT(
    MoonlightProtocolV1ValidateProofSignature(
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      mutated,
      MOONLIGHT_PROTOCOL_V1_P256_SIGNATURE_SIZE_MAX
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  return true;
}

/**
 * @brief Verifies Pairing request round trips at both scheme bounds.
 *
 * @return True on success.
 */
static bool test_pairing_round_trip_bounds(void) {
  uint8_t encoded[MOONLIGHT_PROTOCOL_V1_PAIRING_CLIENT_PROOF_REQUEST_PAYLOAD_MAX];
  MoonlightProtocolV1PairingClientProofRequest decoded;
  MoonlightProtocolV1PairingClientProofRequest request;
  size_t encoded_size;
  size_t header_offset;
  size_t value_offset;
  size_t value_size;

  request = make_pairing_request(
    MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
    false
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodePairingClientProofRequest(
      &request,
      encoded,
      sizeof(encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(encoded_size == 173u);
  TEST_RESULT(
    MoonlightProtocolV1DecodePairingClientProofRequest(
      encoded,
      encoded_size,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(pairing_requests_equal(&decoded, &request));
  TEST_CHECK(find_tlv_field(encoded, encoded_size, 1u, &header_offset, &value_offset, &value_size));
  TEST_CHECK(header_offset == 0u && value_size == 1u);
  TEST_CHECK(encoded[value_offset] == MOONLIGHT_PROTOCOL_V1_CLIENT_PROOF_FORMAT);
  TEST_CHECK(find_tlv_field(encoded, encoded_size, 2u, &header_offset, &value_offset, &value_size));
  TEST_CHECK(header_offset == 9u && value_size == 1u);
  TEST_CHECK(encoded[value_offset] == MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256);
  TEST_CHECK(find_tlv_field(encoded, encoded_size, 3u, &header_offset, &value_offset, &value_size));
  TEST_CHECK(value_size == MOONLIGHT_PROTOCOL_V1_P256_CREDENTIAL_SPKI_SIZE);
  TEST_CHECK(memcmp(encoded + value_offset, request.credential_spki, value_size) == 0);
  TEST_CHECK(find_tlv_field(encoded, encoded_size, 4u, &header_offset, &value_offset, &value_size));
  TEST_CHECK(value_size == sizeof(request.admission_hash));
  TEST_CHECK(memcmp(encoded + value_offset, request.admission_hash, value_size) == 0);
  TEST_CHECK(find_tlv_field(encoded, encoded_size, 5u, &header_offset, &value_offset, &value_size));
  TEST_CHECK(value_size == request.signature_size);

  request = make_pairing_request(
    MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
    true
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodePairingClientProofRequest(
      &request,
      encoded,
      sizeof(encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(encoded_size == 236u);
  TEST_RESULT(
    MoonlightProtocolV1DecodePairingClientProofRequest(
      encoded,
      encoded_size,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(pairing_requests_equal(&decoded, &request));

  request = make_pairing_request(
    MOONLIGHT_PROTOCOL_V1_CREDENTIAL_RSA2048_PKCS1_SHA256_COMPAT,
    false
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodePairingClientProofRequest(
      &request,
      encoded,
      sizeof(encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(
    encoded_size ==
    MOONLIGHT_PROTOCOL_V1_PAIRING_CLIENT_PROOF_REQUEST_PAYLOAD_MAX
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodePairingClientProofRequest(
      encoded,
      encoded_size,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(pairing_requests_equal(&decoded, &request));
  return true;
}

/**
 * @brief Verifies Streaming request round trips and stored-scheme binding.
 *
 * @return True on success.
 */
static bool test_streaming_round_trip_and_context(void) {
  uint8_t encoded[MOONLIGHT_PROTOCOL_V1_STREAMING_CLIENT_PROOF_REQUEST_PAYLOAD_MAX];
  MoonlightProtocolV1StreamingClientProofRequest decoded;
  MoonlightProtocolV1StreamingClientProofRequest request;
  size_t encoded_size;
  size_t header_offset;
  size_t value_offset;
  size_t value_size;

  request = make_streaming_request(
    MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
    false
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeStreamingClientProofRequest(
      &request,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      encoded,
      sizeof(encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(encoded_size == 121u);
  TEST_RESULT(
    MoonlightProtocolV1DecodeStreamingClientProofRequest(
      encoded,
      encoded_size,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(streaming_requests_equal(&decoded, &request));
  TEST_CHECK(find_tlv_field(encoded, encoded_size, 2u, &header_offset, &value_offset, &value_size));
  TEST_CHECK(value_size == sizeof(request.principal_id));
  TEST_CHECK(memcmp(encoded + value_offset, request.principal_id, value_size) == 0);
  TEST_CHECK(find_tlv_field(encoded, encoded_size, 3u, &header_offset, &value_offset, &value_size));
  TEST_CHECK(value_size == 8u);
  TEST_CHECK(memcmp(encoded + value_offset, "\x01\x02\x03\x04\x05\x06\x07\x08", 8u) == 0);
  TEST_CHECK(find_tlv_field(encoded, encoded_size, 4u, &header_offset, &value_offset, &value_size));
  TEST_CHECK(value_size == 8u);
  TEST_CHECK(memcmp(encoded + value_offset, "\x88\x99\xaa\xbb\xcc\xdd\xee\xff", 8u) == 0);

  TEST_RESULT(
    MoonlightProtocolV1DecodeStreamingClientProofRequest(
      encoded,
      encoded_size,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_RSA2048_PKCS1_SHA256_COMPAT,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeStreamingClientProofRequest(
      &request,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_RSA2048_PKCS1_SHA256_COMPAT,
      encoded,
      sizeof(encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );

  request = make_streaming_request(
    MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
    true
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeStreamingClientProofRequest(
      &request,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      encoded,
      sizeof(encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(encoded_size == 184u);
  TEST_RESULT(
    MoonlightProtocolV1DecodeStreamingClientProofRequest(
      encoded,
      encoded_size,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(streaming_requests_equal(&decoded, &request));

  request = make_streaming_request(
    MOONLIGHT_PROTOCOL_V1_CREDENTIAL_RSA2048_PKCS1_SHA256_COMPAT,
    false
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeStreamingClientProofRequest(
      &request,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_RSA2048_PKCS1_SHA256_COMPAT,
      encoded,
      sizeof(encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(
    encoded_size ==
    MOONLIGHT_PROTOCOL_V1_STREAMING_CLIENT_PROOF_REQUEST_PAYLOAD_MAX
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeStreamingClientProofRequest(
      encoded,
      encoded_size,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_RSA2048_PKCS1_SHA256_COMPAT,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(streaming_requests_equal(&decoded, &request));
  TEST_RESULT(
    MoonlightProtocolV1DecodeStreamingClientProofRequest(
      encoded,
      encoded_size,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      &decoded
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  return true;
}

/**
 * @brief Verifies scheme-neutral Streaming Proof decode and stored-scheme handoff.
 *
 * @return True on success.
 */
static bool test_streaming_candidate_handoff(void) {
  uint8_t encoded
    [MOONLIGHT_PROTOCOL_V1_STREAMING_CLIENT_PROOF_REQUEST_PAYLOAD_MAX + 1u];
  MoonlightProtocolV1StreamingClientProofCandidate candidate;
  MoonlightProtocolV1StreamingClientProofCandidate valid_candidate;
  MoonlightProtocolV1StreamingClientProofRequest finalized;
  MoonlightProtocolV1StreamingClientProofRequest request;
  size_t encoded_size;
  size_t header_offset;
  size_t value_offset;
  size_t value_size;

  request = make_streaming_request(
    MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
    false
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeStreamingClientProofRequest(
      &request,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      encoded,
      sizeof(encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeStreamingClientProofCandidate(
      encoded,
      encoded_size,
      &candidate
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(streaming_candidate_matches_request(&candidate, &request));
  TEST_RESULT(
    MoonlightProtocolV1FinalizeStreamingClientProofCandidate(
      &candidate,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      &finalized
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(streaming_requests_equal(&finalized, &request));
  valid_candidate = candidate;

  TEST_CHECK(find_tlv_field(encoded, encoded_size, 6u, &header_offset, &value_offset, &value_size));
  TEST_CHECK(value_size == MOONLIGHT_PROTOCOL_V1_P256_SIGNATURE_SIZE_MIN);
  encoded[value_offset] ^= 1u;
  TEST_RESULT(
    MoonlightProtocolV1DecodeStreamingClientProofCandidate(
      encoded,
      encoded_size,
      &candidate
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(candidate.signature[0] == encoded[value_offset]);
  memset(&finalized, 0xa5, sizeof(finalized));
  TEST_RESULT(
    MoonlightProtocolV1FinalizeStreamingClientProofCandidate(
      &candidate,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      &finalized
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  TEST_CHECK(object_is_zero(&finalized, sizeof(finalized)));

  memset(&finalized, 0xa5, sizeof(finalized));
  TEST_RESULT(
    MoonlightProtocolV1FinalizeStreamingClientProofCandidate(
      &valid_candidate,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_RSA2048_PKCS1_SHA256_COMPAT,
      &finalized
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  TEST_CHECK(object_is_zero(&finalized, sizeof(finalized)));

  request = make_streaming_request(
    MOONLIGHT_PROTOCOL_V1_CREDENTIAL_RSA2048_PKCS1_SHA256_COMPAT,
    false
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeStreamingClientProofRequest(
      &request,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_RSA2048_PKCS1_SHA256_COMPAT,
      encoded,
      sizeof(encoded),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeStreamingClientProofCandidate(
      encoded,
      encoded_size,
      &candidate
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(streaming_candidate_matches_request(&candidate, &request));
  TEST_RESULT(
    MoonlightProtocolV1FinalizeStreamingClientProofCandidate(
      &candidate,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_RSA2048_PKCS1_SHA256_COMPAT,
      &finalized
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(streaming_requests_equal(&finalized, &request));
  valid_candidate = candidate;

  TEST_CHECK(find_tlv_field(encoded, encoded_size, 6u, &header_offset, &value_offset, &value_size));
  TEST_CHECK(value_size == MOONLIGHT_PROTOCOL_V1_RSA2048_SIGNATURE_SIZE);
  test_store_u32(encoded + header_offset + 4u, 255u);
  TEST_RESULT(
    MoonlightProtocolV1DecodeStreamingClientProofCandidate(
      encoded,
      encoded_size - 1u,
      &candidate
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(candidate.signature_size == 255u);
  memset(&finalized, 0xa5, sizeof(finalized));
  TEST_RESULT(
    MoonlightProtocolV1FinalizeStreamingClientProofCandidate(
      &candidate,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_RSA2048_PKCS1_SHA256_COMPAT,
      &finalized
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  TEST_CHECK(object_is_zero(&finalized, sizeof(finalized)));

  memset(&finalized, 0xa5, sizeof(finalized));
  TEST_RESULT(
    MoonlightProtocolV1FinalizeStreamingClientProofCandidate(
      &valid_candidate,
      (MoonlightProtocolV1CredentialScheme) 0x7f,
      &finalized
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  TEST_CHECK(object_is_zero(&finalized, sizeof(finalized)));
  memset(&finalized, 0xa5, sizeof(finalized));
  TEST_RESULT(
    MoonlightProtocolV1FinalizeStreamingClientProofCandidate(
      NULL,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_RSA2048_PKCS1_SHA256_COMPAT,
      &finalized
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_CHECK(object_is_zero(&finalized, sizeof(finalized)));
  TEST_RESULT(
    MoonlightProtocolV1FinalizeStreamingClientProofCandidate(
      &valid_candidate,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_RSA2048_PKCS1_SHA256_COMPAT,
      NULL
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );

  candidate = valid_candidate;
  candidate.proof_format = 2u;
  memset(&finalized, 0xa5, sizeof(finalized));
  TEST_RESULT(
    MoonlightProtocolV1FinalizeStreamingClientProofCandidate(
      &candidate,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_RSA2048_PKCS1_SHA256_COMPAT,
      &finalized
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  TEST_CHECK(object_is_zero(&finalized, sizeof(finalized)));
  candidate = valid_candidate;
  candidate.credential_epoch = 0;
  TEST_RESULT(
    MoonlightProtocolV1FinalizeStreamingClientProofCandidate(
      &candidate,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_RSA2048_PKCS1_SHA256_COMPAT,
      &finalized
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  TEST_CHECK(object_is_zero(&finalized, sizeof(finalized)));
  candidate = valid_candidate;
  candidate.observed_authorization_generation = 0;
  TEST_RESULT(
    MoonlightProtocolV1FinalizeStreamingClientProofCandidate(
      &candidate,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_RSA2048_PKCS1_SHA256_COMPAT,
      &finalized
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  TEST_CHECK(object_is_zero(&finalized, sizeof(finalized)));
  candidate = valid_candidate;
  candidate.signature_size =
    MOONLIGHT_PROTOCOL_V1_P256_SIGNATURE_SIZE_MIN - 1u;
  TEST_RESULT(
    MoonlightProtocolV1FinalizeStreamingClientProofCandidate(
      &candidate,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_RSA2048_PKCS1_SHA256_COMPAT,
      &finalized
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  TEST_CHECK(object_is_zero(&finalized, sizeof(finalized)));
  candidate = valid_candidate;
  candidate.signature_size =
    MOONLIGHT_PROTOCOL_V1_RSA2048_SIGNATURE_SIZE + 1u;
  TEST_RESULT(
    MoonlightProtocolV1FinalizeStreamingClientProofCandidate(
      &candidate,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_RSA2048_PKCS1_SHA256_COMPAT,
      &finalized
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  TEST_CHECK(object_is_zero(&finalized, sizeof(finalized)));

  memset(&candidate, 0xa5, sizeof(candidate));
  TEST_RESULT(
    MoonlightProtocolV1DecodeStreamingClientProofCandidate(
      NULL,
      0,
      &candidate
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_CHECK(object_is_zero(&candidate, sizeof(candidate)));
  TEST_RESULT(
    MoonlightProtocolV1DecodeStreamingClientProofCandidate(
      encoded,
      encoded_size,
      NULL
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  memset(&candidate, 0xa5, sizeof(candidate));
  TEST_RESULT(
    MoonlightProtocolV1DecodeStreamingClientProofCandidate(
      encoded,
      0,
      &candidate
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  TEST_CHECK(object_is_zero(&candidate, sizeof(candidate)));
  encoded[encoded_size] = 0;
  memset(&candidate, 0xa5, sizeof(candidate));
  TEST_RESULT(
    MoonlightProtocolV1DecodeStreamingClientProofCandidate(
      encoded,
      encoded_size + 1u,
      &candidate
    ),
    MOONLIGHT_PROTOCOL_RESULT_LIMIT_EXCEEDED
  );
  TEST_CHECK(object_is_zero(&candidate, sizeof(candidate)));
  return true;
}

/**
 * @brief Verifies exact Pairing and Streaming transcript byte layouts.
 *
 * @return True on success.
 */
static bool test_exact_transcripts(void) {
  MoonlightProtocolV1PairingProofTranscript pairing;
  MoonlightProtocolV1StreamingProofTranscript streaming;
  uint8_t actual_pairing[MOONLIGHT_PROTOCOL_V1_PAIRING_PROOF_TRANSCRIPT_SIZE];
  uint8_t expected_pairing[sizeof(actual_pairing)];
  uint8_t actual_streaming[MOONLIGHT_PROTOCOL_V1_STREAMING_PROOF_TRANSCRIPT_SIZE];
  uint8_t expected_streaming[sizeof(actual_streaming)];
  uint8_t pairing_golden[sizeof(actual_pairing)];
  uint8_t streaming_golden[sizeof(actual_streaming)];
  size_t pairing_golden_size;
  size_t streaming_golden_size;
  size_t offset;

  memset(&pairing, 0, sizeof(pairing));
  pairing.proof_format = MOONLIGHT_PROTOCOL_V1_CLIENT_PROOF_FORMAT;
  fill_pattern(pairing.exporter, sizeof(pairing.exporter), 0x00u);
  fill_pattern(pairing.host_id, sizeof(pairing.host_id), 0x20u);
  fill_pattern(pairing.host_identity, sizeof(pairing.host_identity), 0x30u);
  pairing.credential_scheme =
    MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256;
  fill_pattern(
    pairing.credential_digest,
    sizeof(pairing.credential_digest),
    0x50u
  );
  fill_pattern(pairing.admission_hash, sizeof(pairing.admission_hash), 0x70u);

  offset = 0;
  memcpy(expected_pairing + offset, "Sunshine-Pairing-Proof-v1", 25u);
  offset += 25u;
  expected_pairing[offset++] = MOONLIGHT_PROTOCOL_V1_CLIENT_PROOF_FORMAT;
  memcpy(expected_pairing + offset, pairing.exporter, sizeof(pairing.exporter));
  offset += sizeof(pairing.exporter);
  expected_pairing[offset++] = 15u;
  memcpy(expected_pairing + offset, "sunshine-pair/1", 15u);
  offset += 15u;
  expected_pairing[offset++] = 1u;
  expected_pairing[offset++] = 0u;
  expected_pairing[offset++] = 1u;
  memcpy(expected_pairing + offset, pairing.host_id, sizeof(pairing.host_id));
  offset += sizeof(pairing.host_id);
  memcpy(
    expected_pairing + offset,
    pairing.host_identity,
    sizeof(pairing.host_identity)
  );
  offset += sizeof(pairing.host_identity);
  expected_pairing[offset++] = (uint8_t) pairing.credential_scheme;
  memcpy(
    expected_pairing + offset,
    pairing.credential_digest,
    sizeof(pairing.credential_digest)
  );
  offset += sizeof(pairing.credential_digest);
  memcpy(
    expected_pairing + offset,
    pairing.admission_hash,
    sizeof(pairing.admission_hash)
  );
  offset += sizeof(pairing.admission_hash);
  TEST_CHECK(offset == sizeof(expected_pairing));
  TEST_RESULT(
    MoonlightProtocolV1BuildPairingProofTranscript(
      &pairing,
      actual_pairing,
      sizeof(actual_pairing)
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(memcmp(actual_pairing, expected_pairing, sizeof(actual_pairing)) == 0);
  TEST_CHECK(
    load_golden(
      "pairing-proof-transcript.hex",
      pairing_golden,
      sizeof(pairing_golden),
      &pairing_golden_size
    )
  );
  TEST_CHECK(pairing_golden_size == sizeof(actual_pairing));
  TEST_CHECK(
    memcmp(actual_pairing, pairing_golden, sizeof(actual_pairing)) == 0
  );

  memset(&streaming, 0, sizeof(streaming));
  streaming.proof_format = MOONLIGHT_PROTOCOL_V1_CLIENT_PROOF_FORMAT;
  fill_pattern(streaming.exporter, sizeof(streaming.exporter), 0x90u);
  fill_pattern(streaming.host_id, sizeof(streaming.host_id), 0xb0u);
  fill_pattern(streaming.host_identity, sizeof(streaming.host_identity), 0xc0u);
  fill_pattern(streaming.principal_id, sizeof(streaming.principal_id), 0xe0u);
  streaming.credential_epoch = UINT64_C(0x0102030405060708);
  streaming.observed_authorization_generation =
    UINT64_C(0x1112131415161718);
  streaming.credential_scheme =
    MOONLIGHT_PROTOCOL_V1_CREDENTIAL_RSA2048_PKCS1_SHA256_COMPAT;
  fill_pattern(
    streaming.credential_digest,
    sizeof(streaming.credential_digest),
    0x00u
  );
  fill_pattern(
    streaming.admission_hash,
    sizeof(streaming.admission_hash),
    0x20u
  );

  offset = 0;
  memcpy(expected_streaming + offset, "Sunshine-Streaming-Proof-v1", 27u);
  offset += 27u;
  expected_streaming[offset++] = MOONLIGHT_PROTOCOL_V1_CLIENT_PROOF_FORMAT;
  memcpy(expected_streaming + offset, streaming.exporter, sizeof(streaming.exporter));
  offset += sizeof(streaming.exporter);
  expected_streaming[offset++] = 17u;
  memcpy(expected_streaming + offset, "sunshine-stream/1", 17u);
  offset += 17u;
  expected_streaming[offset++] = 1u;
  expected_streaming[offset++] = 0u;
  expected_streaming[offset++] = 1u;
  memcpy(expected_streaming + offset, streaming.host_id, sizeof(streaming.host_id));
  offset += sizeof(streaming.host_id);
  memcpy(
    expected_streaming + offset,
    streaming.host_identity,
    sizeof(streaming.host_identity)
  );
  offset += sizeof(streaming.host_identity);
  memcpy(
    expected_streaming + offset,
    streaming.principal_id,
    sizeof(streaming.principal_id)
  );
  offset += sizeof(streaming.principal_id);
  test_store_u64(expected_streaming + offset, streaming.credential_epoch);
  offset += 8u;
  test_store_u64(
    expected_streaming + offset,
    streaming.observed_authorization_generation
  );
  offset += 8u;
  expected_streaming[offset++] = (uint8_t) streaming.credential_scheme;
  memcpy(
    expected_streaming + offset,
    streaming.credential_digest,
    sizeof(streaming.credential_digest)
  );
  offset += sizeof(streaming.credential_digest);
  memcpy(
    expected_streaming + offset,
    streaming.admission_hash,
    sizeof(streaming.admission_hash)
  );
  offset += sizeof(streaming.admission_hash);
  TEST_CHECK(offset == sizeof(expected_streaming));
  TEST_RESULT(
    MoonlightProtocolV1BuildStreamingProofTranscript(
      &streaming,
      actual_streaming,
      sizeof(actual_streaming)
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(
    memcmp(actual_streaming, expected_streaming, sizeof(actual_streaming)) == 0
  );
  TEST_CHECK(
    load_golden(
      "streaming-proof-transcript.hex",
      streaming_golden,
      sizeof(streaming_golden),
      &streaming_golden_size
    )
  );
  TEST_CHECK(streaming_golden_size == sizeof(actual_streaming));
  TEST_CHECK(
    memcmp(actual_streaming, streaming_golden, sizeof(actual_streaming)) == 0
  );
  return true;
}

/**
 * @brief Verifies transcript argument checks and failure output atomicity.
 *
 * @return True on success.
 */
static bool test_transcript_rejections_are_atomic(void) {
  MoonlightProtocolV1PairingProofTranscript pairing;
  MoonlightProtocolV1StreamingProofTranscript streaming;
  uint8_t pairing_output[MOONLIGHT_PROTOCOL_V1_PAIRING_PROOF_TRANSCRIPT_SIZE];
  uint8_t pairing_unchanged[sizeof(pairing_output)];
  uint8_t streaming_output[MOONLIGHT_PROTOCOL_V1_STREAMING_PROOF_TRANSCRIPT_SIZE];
  uint8_t streaming_unchanged[sizeof(streaming_output)];

  memset(&pairing, 0, sizeof(pairing));
  pairing.proof_format = MOONLIGHT_PROTOCOL_V1_CLIENT_PROOF_FORMAT;
  pairing.credential_scheme =
    MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256;
  memset(pairing_output, 0xa5, sizeof(pairing_output));
  memcpy(pairing_unchanged, pairing_output, sizeof(pairing_output));

  TEST_RESULT(
    MoonlightProtocolV1BuildPairingProofTranscript(
      NULL,
      pairing_output,
      sizeof(pairing_output)
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1BuildPairingProofTranscript(
      &pairing,
      NULL,
      sizeof(pairing_output)
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1BuildPairingProofTranscript(
      &pairing,
      pairing_output,
      sizeof(pairing_output) - 1u
    ),
    MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL
  );
  pairing.proof_format = 2u;
  TEST_RESULT(
    MoonlightProtocolV1BuildPairingProofTranscript(
      &pairing,
      pairing_output,
      sizeof(pairing_output)
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  pairing.proof_format = MOONLIGHT_PROTOCOL_V1_CLIENT_PROOF_FORMAT;
  pairing.credential_scheme = (MoonlightProtocolV1CredentialScheme) 3;
  TEST_RESULT(
    MoonlightProtocolV1BuildPairingProofTranscript(
      &pairing,
      pairing_output,
      sizeof(pairing_output)
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  TEST_CHECK(
    memcmp(pairing_output, pairing_unchanged, sizeof(pairing_output)) == 0
  );

  memset(&streaming, 0, sizeof(streaming));
  streaming.proof_format = MOONLIGHT_PROTOCOL_V1_CLIENT_PROOF_FORMAT;
  streaming.credential_scheme =
    MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256;
  streaming.credential_epoch = 1u;
  streaming.observed_authorization_generation = 1u;
  memset(streaming_output, 0x5a, sizeof(streaming_output));
  memcpy(streaming_unchanged, streaming_output, sizeof(streaming_output));
  TEST_RESULT(
    MoonlightProtocolV1BuildStreamingProofTranscript(
      NULL,
      streaming_output,
      sizeof(streaming_output)
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1BuildStreamingProofTranscript(
      &streaming,
      NULL,
      sizeof(streaming_output)
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1BuildStreamingProofTranscript(
      &streaming,
      streaming_output,
      sizeof(streaming_output) - 1u
    ),
    MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL
  );
  streaming.proof_format = 2u;
  TEST_RESULT(
    MoonlightProtocolV1BuildStreamingProofTranscript(
      &streaming,
      streaming_output,
      sizeof(streaming_output)
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  streaming.proof_format = MOONLIGHT_PROTOCOL_V1_CLIENT_PROOF_FORMAT;
  streaming.credential_epoch = 0;
  TEST_RESULT(
    MoonlightProtocolV1BuildStreamingProofTranscript(
      &streaming,
      streaming_output,
      sizeof(streaming_output)
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  streaming.credential_epoch = 1u;
  streaming.observed_authorization_generation = 0;
  TEST_RESULT(
    MoonlightProtocolV1BuildStreamingProofTranscript(
      &streaming,
      streaming_output,
      sizeof(streaming_output)
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  streaming.observed_authorization_generation = 1u;
  streaming.credential_scheme = (MoonlightProtocolV1CredentialScheme) 0;
  TEST_RESULT(
    MoonlightProtocolV1BuildStreamingProofTranscript(
      &streaming,
      streaming_output,
      sizeof(streaming_output)
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  TEST_CHECK(
    memcmp(streaming_output, streaming_unchanged, sizeof(streaming_output)) == 0
  );
  return true;
}

/**
 * @brief Verifies Pairing encoder arguments, validation, and atomicity.
 *
 * @return True on success.
 */
static bool test_pairing_encode_rejections_are_atomic(void) {
  MoonlightProtocolV1PairingClientProofRequest request =
    make_pairing_request(
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      false
    );
  uint8_t output[MOONLIGHT_PROTOCOL_V1_PAIRING_CLIENT_PROOF_REQUEST_PAYLOAD_MAX];
  uint8_t unchanged[sizeof(output)];
  size_t encoded_size = 777u;

  memset(output, 0xa5, sizeof(output));
  memcpy(unchanged, output, sizeof(output));
  TEST_RESULT(
    MoonlightProtocolV1EncodePairingClientProofRequest(
      NULL,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodePairingClientProofRequest(
      &request,
      NULL,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodePairingClientProofRequest(
      &request,
      output,
      sizeof(output),
      NULL
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodePairingClientProofRequest(
      &request,
      output,
      172u,
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL
  );

  request.proof_format = 2u;
  TEST_RESULT(
    MoonlightProtocolV1EncodePairingClientProofRequest(
      &request,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  request = make_pairing_request(
    MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
    false
  );
  request.credential_scheme = (MoonlightProtocolV1CredentialScheme) 3;
  TEST_RESULT(
    MoonlightProtocolV1EncodePairingClientProofRequest(
      &request,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  request = make_pairing_request(
    MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
    false
  );
  --request.credential_spki_size;
  TEST_RESULT(
    MoonlightProtocolV1EncodePairingClientProofRequest(
      &request,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  request = make_pairing_request(
    MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
    false
  );
  request.credential_spki[26] = 0x03;
  TEST_RESULT(
    MoonlightProtocolV1EncodePairingClientProofRequest(
      &request,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  request = make_pairing_request(
    MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
    false
  );
  request.signature[7] = 0;
  TEST_RESULT(
    MoonlightProtocolV1EncodePairingClientProofRequest(
      &request,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  TEST_CHECK(memcmp(output, unchanged, sizeof(output)) == 0);
  TEST_CHECK(encoded_size == 777u);
  return true;
}

/**
 * @brief Verifies Streaming encoder arguments, epochs, schemes, and atomicity.
 *
 * @return True on success.
 */
static bool test_streaming_encode_rejections_are_atomic(void) {
  MoonlightProtocolV1StreamingClientProofRequest request =
    make_streaming_request(
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      false
    );
  uint8_t output[MOONLIGHT_PROTOCOL_V1_STREAMING_CLIENT_PROOF_REQUEST_PAYLOAD_MAX];
  uint8_t unchanged[sizeof(output)];
  size_t encoded_size = 888u;

  memset(output, 0x5a, sizeof(output));
  memcpy(unchanged, output, sizeof(output));
  TEST_RESULT(
    MoonlightProtocolV1EncodeStreamingClientProofRequest(
      NULL,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeStreamingClientProofRequest(
      &request,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      NULL,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeStreamingClientProofRequest(
      &request,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      output,
      sizeof(output),
      NULL
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeStreamingClientProofRequest(
      &request,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      output,
      120u,
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeStreamingClientProofRequest(
      &request,
      (MoonlightProtocolV1CredentialScheme) 3,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );

  request.proof_format = 0;
  TEST_RESULT(
    MoonlightProtocolV1EncodeStreamingClientProofRequest(
      &request,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  request = make_streaming_request(
    MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
    false
  );
  request.credential_epoch = 0;
  TEST_RESULT(
    MoonlightProtocolV1EncodeStreamingClientProofRequest(
      &request,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  request = make_streaming_request(
    MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
    false
  );
  request.observed_authorization_generation = 0;
  TEST_RESULT(
    MoonlightProtocolV1EncodeStreamingClientProofRequest(
      &request,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  request = make_streaming_request(
    MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
    false
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodeStreamingClientProofRequest(
      &request,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_RSA2048_PKCS1_SHA256_COMPAT,
      output,
      sizeof(output),
      &encoded_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  TEST_CHECK(memcmp(output, unchanged, sizeof(output)) == 0);
  TEST_CHECK(encoded_size == 888u);
  return true;
}

/**
 * @brief Verifies Pairing TLV field, order, flag, length, and trailing checks.
 *
 * @return True on success.
 */
static bool test_pairing_decode_rejections_are_atomic(void) {
  uint8_t valid[MOONLIGHT_PROTOCOL_V1_PAIRING_CLIENT_PROOF_REQUEST_PAYLOAD_MAX];
  uint8_t rsa_valid[MOONLIGHT_PROTOCOL_V1_PAIRING_CLIENT_PROOF_REQUEST_PAYLOAD_MAX];
  uint8_t mutated[TEST_PAIRING_MUTATION_CAPACITY];
  MoonlightProtocolV1PairingClientProofRequest output;
  MoonlightProtocolV1PairingClientProofRequest unchanged;
  MoonlightProtocolV1PairingClientProofRequest rsa_request;
  size_t valid_size;
  size_t rsa_valid_size;
  size_t header_offset;
  size_t value_offset;
  size_t value_size;
  size_t index;

  TEST_CHECK(encode_pairing_fixture(valid, &valid_size));
  rsa_request = make_pairing_request(
    MOONLIGHT_PROTOCOL_V1_CREDENTIAL_RSA2048_PKCS1_SHA256_COMPAT,
    false
  );
  TEST_RESULT(
    MoonlightProtocolV1EncodePairingClientProofRequest(
      &rsa_request,
      rsa_valid,
      sizeof(rsa_valid),
      &rsa_valid_size
    ),
    MOONLIGHT_PROTOCOL_RESULT_OK
  );
  TEST_CHECK(
    rsa_valid_size ==
    MOONLIGHT_PROTOCOL_V1_PAIRING_CLIENT_PROOF_REQUEST_PAYLOAD_MAX
  );
  memset(&output, 0xa5, sizeof(output));
  unchanged = output;
  TEST_RESULT(
    MoonlightProtocolV1DecodePairingClientProofRequest(
      NULL,
      valid_size,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodePairingClientProofRequest(
      valid,
      valid_size,
      NULL
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodePairingClientProofRequest(valid, 0, &output),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  for (index = 0; index < rsa_valid_size; ++index) {
    TEST_RESULT(
      MoonlightProtocolV1DecodePairingClientProofRequest(
        rsa_valid,
        index,
        &output
      ),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    );
  }
  TEST_RESULT(
    MoonlightProtocolV1DecodePairingClientProofRequest(
      mutated,
      MOONLIGHT_PROTOCOL_V1_PAIRING_CLIENT_PROOF_REQUEST_PAYLOAD_MAX + 1u,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_LIMIT_EXCEEDED
  );
  for (index = 1; index <= 5u; ++index) {
    TEST_CHECK(
      find_tlv_field(
        valid,
        valid_size,
        (uint16_t) index,
        &header_offset,
        &value_offset,
        &value_size
      )
    );
    memcpy(mutated, valid, valid_size);
    test_store_u16(
      mutated + header_offset + 2u,
      MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED
    );
    TEST_RESULT(
      MoonlightProtocolV1DecodePairingClientProofRequest(
        mutated,
        valid_size,
        &output
      ),
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    );

    memcpy(mutated, valid, valid_size);
    test_store_u32(
      mutated + header_offset + 4u,
      (uint32_t) (value_size == 1u ? 2u : value_size - 1u)
    );
    TEST_RESULT(
      MoonlightProtocolV1DecodePairingClientProofRequest(
        mutated,
        valid_size,
        &output
      ),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    );
  }

  memcpy(mutated, valid, valid_size);
  mutated[8] = 2u;
  TEST_RESULT(
    MoonlightProtocolV1DecodePairingClientProofRequest(
      mutated,
      valid_size,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  memcpy(mutated, valid, valid_size);
  mutated[17] = 3u;
  TEST_RESULT(
    MoonlightProtocolV1DecodePairingClientProofRequest(
      mutated,
      valid_size,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  memcpy(mutated, valid, valid_size);
  test_store_u32(mutated + 4u, UINT32_MAX);
  TEST_RESULT(
    MoonlightProtocolV1DecodePairingClientProofRequest(
      mutated,
      valid_size,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated, 0);
  TEST_RESULT(
    MoonlightProtocolV1DecodePairingClientProofRequest(
      mutated,
      valid_size,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated, 6u);
  TEST_RESULT(
    MoonlightProtocolV1DecodePairingClientProofRequest(
      mutated,
      valid_size,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated, 2u);
  TEST_RESULT(
    MoonlightProtocolV1DecodePairingClientProofRequest(
      mutated,
      valid_size,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated + 9u, 1u);
  TEST_RESULT(
    MoonlightProtocolV1DecodePairingClientProofRequest(
      mutated,
      valid_size,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated + 2u, 2u);
  TEST_RESULT(
    MoonlightProtocolV1DecodePairingClientProofRequest(
      mutated,
      valid_size,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );

  memcpy(mutated, valid, valid_size);
  mutated[valid_size] = 0;
  TEST_RESULT(
    MoonlightProtocolV1DecodePairingClientProofRequest(
      mutated,
      valid_size + 1u,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, valid, valid_size);
  memset(
    mutated + valid_size,
    0,
    MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE
  );
  test_store_u16(mutated + valid_size, 6u);
  TEST_RESULT(
    MoonlightProtocolV1DecodePairingClientProofRequest(
      mutated,
      valid_size + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  test_store_u16(mutated + valid_size, 0);
  TEST_RESULT(
    MoonlightProtocolV1DecodePairingClientProofRequest(
      mutated,
      valid_size + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  test_store_u16(mutated + valid_size, 6u);
  test_store_u16(mutated + valid_size + 2u, 2u);
  TEST_RESULT(
    MoonlightProtocolV1DecodePairingClientProofRequest(
      mutated,
      valid_size + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  test_store_u16(mutated + valid_size + 2u, 0);
  test_store_u32(mutated + valid_size + 4u, 1u);
  TEST_RESULT(
    MoonlightProtocolV1DecodePairingClientProofRequest(
      mutated,
      valid_size + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, valid, valid_size);
  memset(
    mutated + valid_size,
    0,
    MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE
  );
  test_store_u16(mutated + valid_size, 5u);
  TEST_RESULT(
    MoonlightProtocolV1DecodePairingClientProofRequest(
      mutated,
      valid_size + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodePairingClientProofRequest(
      valid,
      valid_size - 1u,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  TEST_CHECK(memcmp(&output, &unchanged, sizeof(output)) == 0);
  return true;
}

/**
 * @brief Verifies Streaming TLV, epoch, generation, scheme, and trailing checks.
 *
 * @return True on success.
 */
static bool test_streaming_decode_rejections_are_atomic(void) {
  uint8_t valid[MOONLIGHT_PROTOCOL_V1_STREAMING_CLIENT_PROOF_REQUEST_PAYLOAD_MAX];
  uint8_t mutated[TEST_STREAMING_MUTATION_CAPACITY];
  MoonlightProtocolV1StreamingClientProofRequest output;
  MoonlightProtocolV1StreamingClientProofRequest unchanged;
  size_t valid_size;
  size_t header_offset;
  size_t value_offset;
  size_t value_size;
  size_t index;

  TEST_CHECK(encode_streaming_fixture(valid, &valid_size));
  memset(&output, 0x5a, sizeof(output));
  unchanged = output;
  TEST_RESULT(
    MoonlightProtocolV1DecodeStreamingClientProofRequest(
      NULL,
      valid_size,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeStreamingClientProofRequest(
      valid,
      valid_size,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      NULL
    ),
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeStreamingClientProofRequest(
      valid,
      valid_size,
      (MoonlightProtocolV1CredentialScheme) 3,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  for (index = 0; index < valid_size; ++index) {
    TEST_RESULT(
      MoonlightProtocolV1DecodeStreamingClientProofRequest(
        valid,
        index,
        MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
        &output
      ),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    );
  }
  TEST_RESULT(
    MoonlightProtocolV1DecodeStreamingClientProofRequest(
      mutated,
      MOONLIGHT_PROTOCOL_V1_STREAMING_CLIENT_PROOF_REQUEST_PAYLOAD_MAX + 1u,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_LIMIT_EXCEEDED
  );
  for (index = 1; index <= 6u; ++index) {
    TEST_CHECK(
      find_tlv_field(
        valid,
        valid_size,
        (uint16_t) index,
        &header_offset,
        &value_offset,
        &value_size
      )
    );
    memcpy(mutated, valid, valid_size);
    test_store_u16(
      mutated + header_offset + 2u,
      MOONLIGHT_PROTOCOL_V1_TLV_FLAG_REPEATED
    );
    TEST_RESULT(
      MoonlightProtocolV1DecodeStreamingClientProofRequest(
        mutated,
        valid_size,
        MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
        &output
      ),
      MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
    );

    memcpy(mutated, valid, valid_size);
    test_store_u32(
      mutated + header_offset + 4u,
      (uint32_t) (value_size == 1u ? 2u : value_size - 1u)
    );
    TEST_RESULT(
      MoonlightProtocolV1DecodeStreamingClientProofRequest(
        mutated,
        valid_size,
        MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
        &output
      ),
      MOONLIGHT_PROTOCOL_RESULT_MALFORMED
    );
  }

  memcpy(mutated, valid, valid_size);
  mutated[8] = 2u;
  TEST_RESULT(
    MoonlightProtocolV1DecodeStreamingClientProofRequest(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  memcpy(mutated, valid, valid_size);
  test_store_u32(mutated + 4u, UINT32_MAX);
  TEST_RESULT(
    MoonlightProtocolV1DecodeStreamingClientProofRequest(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated, 0);
  TEST_RESULT(
    MoonlightProtocolV1DecodeStreamingClientProofRequest(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated, 7u);
  TEST_RESULT(
    MoonlightProtocolV1DecodeStreamingClientProofRequest(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated, 2u);
  TEST_RESULT(
    MoonlightProtocolV1DecodeStreamingClientProofRequest(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated + 9u, 1u);
  TEST_RESULT(
    MoonlightProtocolV1DecodeStreamingClientProofRequest(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, valid, valid_size);
  test_store_u16(mutated + 2u, 2u);
  TEST_RESULT(
    MoonlightProtocolV1DecodeStreamingClientProofRequest(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );

  TEST_CHECK(find_tlv_field(valid, valid_size, 3u, &header_offset, &value_offset, &value_size));
  memcpy(mutated, valid, valid_size);
  memset(mutated + value_offset, 0, value_size);
  TEST_RESULT(
    MoonlightProtocolV1DecodeStreamingClientProofRequest(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  TEST_CHECK(find_tlv_field(valid, valid_size, 4u, &header_offset, &value_offset, &value_size));
  memcpy(mutated, valid, valid_size);
  memset(mutated + value_offset, 0, value_size);
  TEST_RESULT(
    MoonlightProtocolV1DecodeStreamingClientProofRequest(
      mutated,
      valid_size,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );

  memcpy(mutated, valid, valid_size);
  mutated[valid_size] = 0;
  TEST_RESULT(
    MoonlightProtocolV1DecodeStreamingClientProofRequest(
      mutated,
      valid_size + 1u,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, valid, valid_size);
  memset(
    mutated + valid_size,
    0,
    MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE
  );
  test_store_u16(mutated + valid_size, 7u);
  TEST_RESULT(
    MoonlightProtocolV1DecodeStreamingClientProofRequest(
      mutated,
      valid_size + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  test_store_u16(mutated + valid_size, 0);
  TEST_RESULT(
    MoonlightProtocolV1DecodeStreamingClientProofRequest(
      mutated,
      valid_size + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  test_store_u16(mutated + valid_size, 7u);
  test_store_u16(mutated + valid_size + 2u, 2u);
  TEST_RESULT(
    MoonlightProtocolV1DecodeStreamingClientProofRequest(
      mutated,
      valid_size + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED
  );
  test_store_u16(mutated + valid_size + 2u, 0);
  test_store_u32(mutated + valid_size + 4u, 1u);
  TEST_RESULT(
    MoonlightProtocolV1DecodeStreamingClientProofRequest(
      mutated,
      valid_size + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  memcpy(mutated, valid, valid_size);
  memset(
    mutated + valid_size,
    0,
    MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE
  );
  test_store_u16(mutated + valid_size, 6u);
  TEST_RESULT(
    MoonlightProtocolV1DecodeStreamingClientProofRequest(
      mutated,
      valid_size + MOONLIGHT_PROTOCOL_V1_TLV_HEADER_SIZE,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  TEST_RESULT(
    MoonlightProtocolV1DecodeStreamingClientProofRequest(
      valid,
      valid_size - 1u,
      MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256,
      &output
    ),
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED
  );
  TEST_CHECK(memcmp(&output, &unchanged, sizeof(output)) == 0);
  return true;
}

/**
 * @brief Executes every Client Proof schema test.
 *
 * @return Zero only when every test succeeds.
 */
int main(void) {
  static const struct {
    const char *name;  ///< Human-readable test name.
    bool (*run)(void);  ///< Test implementation.
  } tests[] = {
    {"Credential SPKI templates", test_credential_spki_templates},
    {"signature representations", test_signature_representations},
    {"Pairing round-trip bounds", test_pairing_round_trip_bounds},
    {"Streaming round-trip and context", test_streaming_round_trip_and_context},
    {"Streaming candidate handoff", test_streaming_candidate_handoff},
    {"exact proof transcripts", test_exact_transcripts},
    {"transcript rejection atomicity", test_transcript_rejections_are_atomic},
    {"Pairing encode rejection atomicity", test_pairing_encode_rejections_are_atomic},
    {"Streaming encode rejection atomicity", test_streaming_encode_rejections_are_atomic},
    {"Pairing decode rejection atomicity", test_pairing_decode_rejections_are_atomic},
    {"Streaming decode rejection atomicity", test_streaming_decode_rejections_are_atomic},
  };

  size_t index;

  for (index = 0; index < sizeof(tests) / sizeof(tests[0]); ++index) {
    if (!tests[index].run()) {
      fprintf(stderr, "FAILED: %s\n", tests[index].name);
      return 1;
    }
    printf("PASS: %s\n", tests[index].name);
  }
  return 0;
}
