/**
 * @file client_proof.h
 * @brief Defines protocol version 1 exporter-bound Client Proof schemas.
 */
#pragma once

#include <moonlight/protocol/control.h>
#include <moonlight/protocol/wire.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Exact Client Proof format accepted by protocol version 1.
 */
#define MOONLIGHT_PROTOCOL_V1_CLIENT_PROOF_FORMAT 1u

/**
 * @brief Size of a SHA-256 digest used by Client Proof.
 */
#define MOONLIGHT_PROTOCOL_V1_CLIENT_PROOF_SHA256_SIZE 32u

/**
 * @brief Exact canonical DER SPKI size for a P-256 Client Credential.
 */
#define MOONLIGHT_PROTOCOL_V1_P256_CREDENTIAL_SPKI_SIZE 91u

/**
 * @brief Exact canonical DER SPKI size for an RSA-2048 Client Credential.
 */
#define MOONLIGHT_PROTOCOL_V1_RSA2048_CREDENTIAL_SPKI_SIZE 294u

/**
 * @brief Minimum strict DER size of a P-256 ECDSA signature.
 */
#define MOONLIGHT_PROTOCOL_V1_P256_SIGNATURE_SIZE_MIN 8u

/**
 * @brief Maximum permitted DER field size of a P-256 ECDSA signature.
 */
#define MOONLIGHT_PROTOCOL_V1_P256_SIGNATURE_SIZE_MAX 72u

/**
 * @brief Exact RSA-2048 PKCS #1 version 1.5 signature size.
 */
#define MOONLIGHT_PROTOCOL_V1_RSA2048_SIGNATURE_SIZE 256u

/**
 * @brief Maximum encoded Pairing CLIENT_PROOF request payload.
 */
#define MOONLIGHT_PROTOCOL_V1_PAIRING_CLIENT_PROOF_REQUEST_PAYLOAD_MAX 624u

/**
 * @brief Maximum encoded Streaming CLIENT_PROOF request payload.
 */
#define MOONLIGHT_PROTOCOL_V1_STREAMING_CLIENT_PROOF_REQUEST_PAYLOAD_MAX 369u

/**
 * @brief Exact Pairing Proof signature-transcript size.
 */
#define MOONLIGHT_PROTOCOL_V1_PAIRING_PROOF_TRANSCRIPT_SIZE 190u

/**
 * @brief Exact Streaming Proof signature-transcript size.
 */
#define MOONLIGHT_PROTOCOL_V1_STREAMING_PROOF_TRANSCRIPT_SIZE 226u

  /**
   * @brief Identifies an exact protocol version 1 Client Credential scheme.
   */
  typedef enum MoonlightProtocolV1CredentialScheme {
    MOONLIGHT_PROTOCOL_V1_CREDENTIAL_ECDSA_P256_SHA256 = 1,  ///< P-256 ECDSA with SHA-256 and strict low-S DER.
    MOONLIGHT_PROTOCOL_V1_CREDENTIAL_RSA2048_PKCS1_SHA256_COMPAT = 2  ///< RSA-2048 PKCS #1 version 1.5 with SHA-256.
  } MoonlightProtocolV1CredentialScheme;

  /**
   * @brief Holds one validated Pairing CLIENT_PROOF request payload.
   */
  typedef struct MoonlightProtocolV1PairingClientProofRequest {
    uint8_t proof_format;  ///< Exact Client Proof format, currently one.
    MoonlightProtocolV1CredentialScheme credential_scheme;  ///< Candidate Credential scheme.
    uint8_t credential_spki[MOONLIGHT_PROTOCOL_V1_RSA2048_CREDENTIAL_SPKI_SIZE];  ///< Exact canonical candidate SPKI.
    size_t credential_spki_size;  ///< Scheme-bound bytes in `credential_spki`.
    uint8_t admission_hash[MOONLIGHT_PROTOCOL_V1_CLIENT_PROOF_SHA256_SIZE];  ///< Bound PAIR_REQUEST digest.
    uint8_t signature[MOONLIGHT_PROTOCOL_V1_RSA2048_SIGNATURE_SIZE];  ///< Scheme-bound Pairing Proof signature.
    size_t signature_size;  ///< Scheme-bound bytes in `signature`.
  } MoonlightProtocolV1PairingClientProofRequest;

  /**
   * @brief Holds one validated Streaming CLIENT_PROOF request payload.
   */
  typedef struct MoonlightProtocolV1StreamingClientProofRequest {
    uint8_t proof_format;  ///< Exact Client Proof format, currently one.
    uint8_t principal_id[MOONLIGHT_PROTOCOL_V1_UUID_SIZE];  ///< Claimed Host-local Principal selector.
    uint64_t credential_epoch;  ///< Nonzero Client Credential epoch.
    uint64_t observed_authorization_generation;  ///< Nonzero Client-observed authorization generation.
    uint8_t admission_hash[MOONLIGHT_PROTOCOL_V1_CLIENT_PROOF_SHA256_SIZE];  ///< Bound CLIENT_HELLO digest.
    uint8_t signature[MOONLIGHT_PROTOCOL_V1_RSA2048_SIGNATURE_SIZE];  ///< Stored-scheme-bound Streaming Proof signature.
    size_t signature_size;  ///< Stored-scheme-bound bytes in `signature`.
  } MoonlightProtocolV1StreamingClientProofRequest;

  /**
   * @brief Holds one schema-valid Streaming CLIENT_PROOF before Credential lookup.
   *
   * The signature is deliberately opaque at this stage. No algorithm selector
   * is present on the Streaming wire; callers use `principal_id` and
   * `credential_epoch` to obtain the immutable stored Credential scheme, then
   * call `MoonlightProtocolV1FinalizeStreamingClientProofCandidate`.
   */
  typedef struct MoonlightProtocolV1StreamingClientProofCandidate {
    uint8_t proof_format;  ///< Exact Client Proof format, currently one.
    uint8_t principal_id[MOONLIGHT_PROTOCOL_V1_UUID_SIZE];  ///< Claimed Host-local Principal selector.
    uint64_t credential_epoch;  ///< Nonzero Client Credential epoch.
    uint64_t observed_authorization_generation;  ///< Nonzero Client-observed authorization generation.
    uint8_t admission_hash[MOONLIGHT_PROTOCOL_V1_CLIENT_PROOF_SHA256_SIZE];  ///< Bound CLIENT_HELLO digest.
    uint8_t signature[MOONLIGHT_PROTOCOL_V1_RSA2048_SIGNATURE_SIZE];  ///< Opaque bounded signature bytes.
    size_t signature_size;  ///< Opaque signature size in `[8, 256]`.
  } MoonlightProtocolV1StreamingClientProofCandidate;

  /**
   * @brief Holds every variable field in one Pairing Proof transcript.
   */
  typedef struct MoonlightProtocolV1PairingProofTranscript {
    uint8_t proof_format;  ///< Exact Client Proof format, currently one.
    uint8_t exporter[MOONLIGHT_PROTOCOL_V1_CLIENT_PROOF_SHA256_SIZE];  ///< Regular TLS exporter.
    uint8_t host_id[MOONLIGHT_PROTOCOL_V1_UUID_SIZE];  ///< Host UUID from the invitation.
    uint8_t host_identity[MOONLIGHT_PROTOCOL_V1_CLIENT_PROOF_SHA256_SIZE];  ///< Expected Host SPKI digest.
    MoonlightProtocolV1CredentialScheme credential_scheme;  ///< Candidate Credential scheme.
    uint8_t credential_digest[MOONLIGHT_PROTOCOL_V1_CLIENT_PROOF_SHA256_SIZE];  ///< Candidate SPKI digest.
    uint8_t admission_hash[MOONLIGHT_PROTOCOL_V1_CLIENT_PROOF_SHA256_SIZE];  ///< Bound PAIR_REQUEST digest.
  } MoonlightProtocolV1PairingProofTranscript;

  /**
   * @brief Holds every variable field in one Streaming Proof transcript.
   */
  typedef struct MoonlightProtocolV1StreamingProofTranscript {
    uint8_t proof_format;  ///< Exact Client Proof format, currently one.
    uint8_t exporter[MOONLIGHT_PROTOCOL_V1_CLIENT_PROOF_SHA256_SIZE];  ///< Regular TLS exporter.
    uint8_t host_id[MOONLIGHT_PROTOCOL_V1_UUID_SIZE];  ///< Host UUID from the durable Host record.
    uint8_t host_identity[MOONLIGHT_PROTOCOL_V1_CLIENT_PROOF_SHA256_SIZE];  ///< Expected Host SPKI digest.
    uint8_t principal_id[MOONLIGHT_PROTOCOL_V1_UUID_SIZE];  ///< Host-local Client Principal UUID.
    uint64_t credential_epoch;  ///< Nonzero stored Client Credential epoch.
    uint64_t observed_authorization_generation;  ///< Nonzero Client-observed authorization generation.
    MoonlightProtocolV1CredentialScheme credential_scheme;  ///< Stored Client Credential scheme.
    uint8_t credential_digest[MOONLIGHT_PROTOCOL_V1_CLIENT_PROOF_SHA256_SIZE];  ///< Stored Credential SPKI digest.
    uint8_t admission_hash[MOONLIGHT_PROTOCOL_V1_CLIENT_PROOF_SHA256_SIZE];  ///< Bound CLIENT_HELLO digest.
  } MoonlightProtocolV1StreamingProofTranscript;

  /**
   * @brief Validates a canonical Client Credential SPKI.
   *
   * This performs the protocol's bounded DER structure and exact key-size
   * policy. Cryptographic public-key import and point or modulus validation
   * remain the caller's responsibility.
   *
   * @param credential_scheme Exact paired or candidate Credential scheme.
   * @param credential_spki Complete canonical DER SubjectPublicKeyInfo.
   * @param credential_spki_size Number of bytes in `credential_spki`.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1ValidateCredentialSpki(
    MoonlightProtocolV1CredentialScheme credential_scheme,
    const uint8_t *credential_spki,
    size_t credential_spki_size
  );

  /**
   * @brief Validates a scheme-bound Client Proof signature representation.
   *
   * P-256 signatures must be strict shortest-form DER with both scalars in
   * range and a low `s`. RSA signatures must have the exact RSA-2048 size.
   * This function validates representation only and does not verify the
   * signature against a public key or transcript.
   *
   * @param credential_scheme Exact paired or candidate Credential scheme.
   * @param signature Complete signature bytes.
   * @param signature_size Number of bytes in `signature`.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1ValidateProofSignature(
    MoonlightProtocolV1CredentialScheme credential_scheme,
    const uint8_t *signature,
    size_t signature_size
  );

  /**
   * @brief Encodes one canonical Pairing CLIENT_PROOF request payload.
   *
   * The output and `encoded_size` are unchanged when validation fails.
   *
   * @param request Validated host-order request values.
   * @param output Destination buffer.
   * @param output_size Available bytes in `output`.
   * @param encoded_size Receives the encoded payload size.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1EncodePairingClientProofRequest(
    const MoonlightProtocolV1PairingClientProofRequest *request,
    uint8_t *output,
    size_t output_size,
    size_t *encoded_size
  );

  /**
   * @brief Decodes one complete canonical Pairing CLIENT_PROOF request payload.
   *
   * Structural incompleteness is `MOONLIGHT_PROTOCOL_RESULT_MALFORMED` because
   * this API consumes an already delimited complete payload.
   *
   * @param input Complete payload bytes.
   * @param input_size Number of bytes in `input`.
   * @param request Receives validated host-order values only on success.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1DecodePairingClientProofRequest(
    const uint8_t *input,
    size_t input_size,
    MoonlightProtocolV1PairingClientProofRequest *request
  );

  /**
   * @brief Encodes one canonical Streaming CLIENT_PROOF request payload.
   *
   * The stored Credential scheme is immutable connection context and is not
   * placed on the Streaming wire. The output and `encoded_size` are unchanged
   * when validation fails.
   *
   * @param request Validated host-order request values.
   * @param credential_scheme Stored Credential scheme used for signature policy.
   * @param output Destination buffer.
   * @param output_size Available bytes in `output`.
   * @param encoded_size Receives the encoded payload size.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1EncodeStreamingClientProofRequest(
    const MoonlightProtocolV1StreamingClientProofRequest *request,
    MoonlightProtocolV1CredentialScheme credential_scheme,
    uint8_t *output,
    size_t output_size,
    size_t *encoded_size
  );

  /**
   * @brief Decodes a Streaming CLIENT_PROOF without selecting a signature scheme.
   *
   * This accepts exactly six ordered scalar TLVs, validates fields one through
   * five, and retains field six only as an opaque signature of 8 through 256
   * bytes. The candidate is zeroed on every failure. No unauthenticated wire
   * value can select the signature algorithm.
   *
   * Structural incompleteness is `MOONLIGHT_PROTOCOL_RESULT_MALFORMED` because
   * this API consumes an already delimited complete payload.
   *
   * @param input Complete payload bytes.
   * @param input_size Number of bytes in `input`.
   * @param candidate Receives schema-valid values on success and zeros on failure.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1DecodeStreamingClientProofCandidate(
    const uint8_t *input,
    size_t input_size,
    MoonlightProtocolV1StreamingClientProofCandidate *candidate
  );

  /**
   * @brief Applies a stored Credential scheme to a Streaming Proof candidate.
   *
   * The immutable stored scheme selects the existing strict signature
   * representation policy. The request is zeroed on every failure.
   *
   * @param candidate Schema-valid candidate decoded from the wire.
   * @param credential_scheme Immutable stored Credential scheme.
   * @param request Receives a scheme-bound request on success and zeros on failure.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1FinalizeStreamingClientProofCandidate(
    const MoonlightProtocolV1StreamingClientProofCandidate *candidate,
    MoonlightProtocolV1CredentialScheme credential_scheme,
    MoonlightProtocolV1StreamingClientProofRequest *request
  );

  /**
   * @brief Decodes one complete canonical Streaming CLIENT_PROOF request payload.
   *
   * The supplied stored Credential scheme selects the only permitted signature
   * representation. It is not negotiated from unauthenticated wire data.
   * Structural incompleteness is `MOONLIGHT_PROTOCOL_RESULT_MALFORMED` because
   * this API consumes an already delimited complete payload.
   *
   * @param input Complete payload bytes.
   * @param input_size Number of bytes in `input`.
   * @param credential_scheme Stored Credential scheme used for signature policy.
   * @param request Receives validated host-order values only on success.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1DecodeStreamingClientProofRequest(
    const uint8_t *input,
    size_t input_size,
    MoonlightProtocolV1CredentialScheme credential_scheme,
    MoonlightProtocolV1StreamingClientProofRequest *request
  );

  /**
   * @brief Builds the exact fixed-size Pairing Proof signature transcript.
   *
   * The output is unchanged when validation fails. The codec clears its
   * internal exporter copy; the caller clears `output` immediately after
   * signing or verification.
   *
   * @param transcript Validated Pairing Proof fields.
   * @param output Destination buffer.
   * @param output_size Available bytes in `output`.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1BuildPairingProofTranscript(
    const MoonlightProtocolV1PairingProofTranscript *transcript,
    uint8_t *output,
    size_t output_size
  );

  /**
   * @brief Builds the exact fixed-size Streaming Proof signature transcript.
   *
   * The output is unchanged when validation fails. The codec clears its
   * internal exporter copy; the caller clears `output` immediately after
   * signing or verification.
   *
   * @param transcript Validated Streaming Proof fields.
   * @param output Destination buffer.
   * @param output_size Available bytes in `output`.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1BuildStreamingProofTranscript(
    const MoonlightProtocolV1StreamingProofTranscript *transcript,
    uint8_t *output,
    size_t output_size
  );

#ifdef __cplusplus
}
#endif
