/**
 * @file control.h
 * @brief Defines protocol version 1 Control Lane message schemas.
 */
#pragma once

#include <moonlight/protocol/wire.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Exact protocol minor accepted by version 1.
 */
#define MOONLIGHT_PROTOCOL_V1_MINOR 0u

/**
 * @brief Maximum encoded CLIENT_HELLO request payload.
 */
#define MOONLIGHT_PROTOCOL_V1_CLIENT_HELLO_REQUEST_PAYLOAD_MAX 98u

/**
 * @brief Exact encoded CLIENT_HELLO response payload size.
 */
#define MOONLIGHT_PROTOCOL_V1_CLIENT_HELLO_RESPONSE_PAYLOAD_SIZE 74u

/**
 * @brief Maximum complete stream-0 Early Hello byte count.
 */
#define MOONLIGHT_PROTOCOL_V1_EARLY_HELLO_SIZE_MAX \
  (MOONLIGHT_PROTOCOL_V1_LANE_PREFACE_SIZE + \
   MOONLIGHT_PROTOCOL_V1_MESSAGE_ENVELOPE_SIZE + \
   MOONLIGHT_PROTOCOL_V1_CLIENT_HELLO_REQUEST_PAYLOAD_MAX)

/**
 * @brief Maximum Client software-version UTF-8 byte count.
 */
#define MOONLIGHT_PROTOCOL_V1_SOFTWARE_VERSION_MAX 64u

/**
 * @brief Size of a protocol version 1 UUID value.
 */
#define MOONLIGHT_PROTOCOL_V1_UUID_SIZE 16u

/**
 * @brief Mask of every defined protocol version 1 capability bit.
 */
#define MOONLIGHT_PROTOCOL_V1_CAPABILITY_MASK UINT64_C(0x3f)

  /**
   * @brief Holds one validated CLIENT_HELLO request payload.
   */
  typedef struct MoonlightProtocolV1ClientHelloRequest {
    uint16_t protocol_minor;  ///< Exact protocol minor, currently zero.
    uint64_t capability_bits;  ///< Defined Client feature bits.
    uint8_t software_version[MOONLIGHT_PROTOCOL_V1_SOFTWARE_VERSION_MAX];  ///< Canonical UTF-8 bytes.
    size_t software_version_size;  ///< Number of bytes in `software_version`.
  } MoonlightProtocolV1ClientHelloRequest;

  /**
   * @brief Holds one validated CLIENT_HELLO successful-response payload.
   */
  typedef struct MoonlightProtocolV1ClientHelloResponse {
    uint16_t protocol_minor;  ///< Exact protocol minor, currently zero.
    uint64_t capability_bits;  ///< Listener-frozen Host feature bits.
    uint8_t host_id[MOONLIGHT_PROTOCOL_V1_UUID_SIZE];  ///< Host UUID in canonical wire bytes.
    uint32_t maximum_control_payload;  ///< Listener-frozen Control payload maximum.
    uint32_t maximum_bulk_payload;  ///< Listener-frozen Bulk payload maximum.
  } MoonlightProtocolV1ClientHelloResponse;

  /**
   * @brief Encodes one canonical CLIENT_HELLO request payload.
   *
   * The output and `encoded_size` are unchanged when validation fails.
   *
   * @param request Validated host-order request values.
   * @param output Destination buffer.
   * @param output_size Available bytes in `output`.
   * @param encoded_size Receives the encoded payload size.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1EncodeClientHelloRequest(
    const MoonlightProtocolV1ClientHelloRequest *request,
    uint8_t *output,
    size_t output_size,
    size_t *encoded_size
  );

  /**
   * @brief Decodes one complete canonical CLIENT_HELLO request payload.
   *
   * @param input Complete payload bytes.
   * @param input_size Number of bytes in `input`.
   * @param request Receives validated host-order values only on success.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1DecodeClientHelloRequest(
    const uint8_t *input,
    size_t input_size,
    MoonlightProtocolV1ClientHelloRequest *request
  );

  /**
   * @brief Encodes one canonical successful CLIENT_HELLO response payload.
   *
   * The output and `encoded_size` are unchanged when validation fails.
   *
   * @param response Validated host-order response values.
   * @param output Destination buffer.
   * @param output_size Available bytes in `output`.
   * @param encoded_size Receives the encoded payload size.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1EncodeClientHelloResponse(
    const MoonlightProtocolV1ClientHelloResponse *response,
    uint8_t *output,
    size_t output_size,
    size_t *encoded_size
  );

  /**
   * @brief Decodes one complete canonical successful CLIENT_HELLO response payload.
   *
   * @param input Complete payload bytes.
   * @param input_size Number of bytes in `input`.
   * @param response Receives validated host-order values only on success.
   * @return The codec result.
   */
  MoonlightProtocolResult MoonlightProtocolV1DecodeClientHelloResponse(
    const uint8_t *input,
    size_t input_size,
    MoonlightProtocolV1ClientHelloResponse *response
  );

#ifdef __cplusplus
}
#endif
