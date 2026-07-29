/**
 * @file pairing.h
 * @brief Defines protocol version 1 Pair Control Lane pairing schemas.
 */
#pragma once

#include <moonlight/protocol/wire.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Maximum encoded PAIR_REQUEST request payload.
 */
#define MOONLIGHT_PROTOCOL_V1_PAIR_REQUEST_PAYLOAD_MAX 160u

/**
 * @brief Exact encoded successful PAIR_REQUEST response payload size.
 */
#define MOONLIGHT_PROTOCOL_V1_PAIR_RESPONSE_PAYLOAD_SIZE 105u

/**
 * @brief Size of a pairing UUID in canonical wire byte order.
 */
#define MOONLIGHT_PROTOCOL_V1_PAIR_UUID_SIZE 16u

/**
 * @brief Size of a pairing invitation token identifier.
 */
#define MOONLIGHT_PROTOCOL_V1_PAIR_TOKEN_ID_SIZE 16u

/**
 * @brief Size of a pairing invitation secret.
 */
#define MOONLIGHT_PROTOCOL_V1_PAIR_INVITATION_SECRET_SIZE 32u

/**
 * @brief Maximum Client display-name UTF-8 byte count.
 */
#define MOONLIGHT_PROTOCOL_V1_PAIR_CLIENT_NAME_MAX 64u

/**
 * @brief Mask of every defined protocol version 1 ACL permission bit.
 */
#define MOONLIGHT_PROTOCOL_V1_ACL_PERMISSION_MASK UINT64_C(0x7f)

  /**
   * @brief Defines protocol version 1 Principal ACL permission bits.
   */
  typedef enum MoonlightProtocolV1AclPermission {
    MOONLIGHT_PROTOCOL_V1_ACL_PERMISSION_START_APPLICATION = 0x01,  ///< Creates an Application Instance.
    MOONLIGHT_PROTOCOL_V1_ACL_PERMISSION_ATTACH_APPLICATION = 0x02,  ///< Attaches to an ACL-visible instance.
    MOONLIGHT_PROTOCOL_V1_ACL_PERMISSION_STOP_APPLICATION = 0x04,  ///< Stops another Principal's instance.
    MOONLIGHT_PROTOCOL_V1_ACL_PERMISSION_SEND_INPUT = 0x08,  ///< Negotiates and sends Stream Session input.
    MOONLIGHT_PROTOCOL_V1_ACL_PERMISSION_VIEW_CATALOG = 0x10,  ///< Reads the visible application catalog.
    MOONLIGHT_PROTOCOL_V1_ACL_PERMISSION_VIEW_INSTANCES = 0x20,  ///< Lists ACL-visible Application Instances.
    MOONLIGHT_PROTOCOL_V1_ACL_PERMISSION_READ_ASSET = 0x40  ///< Reads an asset from the visible catalog.
  } MoonlightProtocolV1AclPermission;

  /**
   * @brief Defines protocol version 1 Application Instance visibility policies.
   */
  typedef enum MoonlightProtocolV1InstanceVisibility {
    MOONLIGHT_PROTOCOL_V1_INSTANCE_VISIBILITY_OWNER_ONLY = 1,  ///< Exposes only instances owned by the Principal.
    MOONLIGHT_PROTOCOL_V1_INSTANCE_VISIBILITY_SHARED = 2  ///< Also exposes explicitly shareable instances.
  } MoonlightProtocolV1InstanceVisibility;

  /**
   * @brief Holds one validated PAIR_REQUEST request payload.
   */
  typedef struct MoonlightProtocolV1PairRequest {
    uint8_t host_id[MOONLIGHT_PROTOCOL_V1_PAIR_UUID_SIZE];  ///< Invitation Host UUID in canonical wire bytes.
    uint8_t token_id[MOONLIGHT_PROTOCOL_V1_PAIR_TOKEN_ID_SIZE];  ///< Invitation token identifier.
    uint8_t invitation_secret[MOONLIGHT_PROTOCOL_V1_PAIR_INVITATION_SECRET_SIZE];  ///< Invitation secret; caller clears it after use.
    uint8_t client_name[MOONLIGHT_PROTOCOL_V1_PAIR_CLIENT_NAME_MAX];  ///< Canonical Client-name UTF-8 bytes.
    size_t client_name_size;  ///< Number of bytes in `client_name`.
  } MoonlightProtocolV1PairRequest;

  /**
   * @brief Holds one validated successful PAIR_REQUEST response payload.
   */
  typedef struct MoonlightProtocolV1PairResponse {
    uint8_t client_principal_id[MOONLIGHT_PROTOCOL_V1_PAIR_UUID_SIZE];  ///< Durable Client Principal UUID.
    uint8_t host_id[MOONLIGHT_PROTOCOL_V1_PAIR_UUID_SIZE];  ///< Host UUID in canonical wire bytes.
    uint64_t granted_acl_permission_bits;  ///< Subset of defined ACL permissions.
    uint64_t authorization_generation;  ///< Nonzero initial authorization generation.
    uint64_t credential_epoch;  ///< Initial Client Credential epoch, exactly one.
    MoonlightProtocolV1InstanceVisibility instance_visibility;  ///< Principal instance-visibility policy.
  } MoonlightProtocolV1PairResponse;

  /**
   * @brief Encodes one canonical PAIR_REQUEST request payload.
   *
   * The output and `encoded_size` are unchanged when validation fails.
   * The codec clears its internal secret copy; the caller remains responsible
   * for clearing `request` and the encoded payload after use.
   *
   * @param request Validated request values.
   * @param output Destination buffer.
   * @param output_size Available bytes in `output`.
   * @param encoded_size Receives the encoded payload size.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1EncodePairRequest(
    const MoonlightProtocolV1PairRequest *request,
    uint8_t *output,
    size_t output_size,
    size_t *encoded_size
  );

  /**
   * @brief Decodes one complete canonical PAIR_REQUEST request payload.
   *
   * Structural incompleteness is `MOONLIGHT_PROTOCOL_RESULT_MALFORMED` because
   * this API consumes an already delimited complete payload.
   * The codec clears its internal secret copy; the caller remains responsible
   * for clearing `input` and `request` after use.
   *
   * @param input Complete payload bytes.
   * @param input_size Number of bytes in `input`.
   * @param request Receives validated values only on success.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1DecodePairRequest(
    const uint8_t *input,
    size_t input_size,
    MoonlightProtocolV1PairRequest *request
  );

  /**
   * @brief Encodes one canonical successful PAIR_REQUEST response payload.
   *
   * The output and `encoded_size` are unchanged when validation fails.
   *
   * @param response Validated response values.
   * @param output Destination buffer.
   * @param output_size Available bytes in `output`.
   * @param encoded_size Receives the encoded payload size.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1EncodePairResponse(
    const MoonlightProtocolV1PairResponse *response,
    uint8_t *output,
    size_t output_size,
    size_t *encoded_size
  );

  /**
   * @brief Decodes one complete canonical successful PAIR_REQUEST response payload.
   *
   * Structural incompleteness is `MOONLIGHT_PROTOCOL_RESULT_MALFORMED` because
   * this API consumes an already delimited complete payload.
   *
   * @param input Complete payload bytes.
   * @param input_size Number of bytes in `input`.
   * @param response Receives validated values only on success.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1DecodePairResponse(
    const uint8_t *input,
    size_t input_size,
    MoonlightProtocolV1PairResponse *response
  );

#ifdef __cplusplus
}
#endif
