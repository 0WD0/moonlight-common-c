/**
 * @file wire.h
 * @brief Defines backend-independent Sunshine protocol version 1 wire results.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

  /**
   * @brief Describes the result of a Sunshine protocol version 1 codec operation.
   */
  typedef enum MoonlightProtocolResult {
    MOONLIGHT_PROTOCOL_RESULT_OK = 0,  ///< The complete value was encoded or decoded.
    MOONLIGHT_PROTOCOL_RESULT_INVALID_ARGUMENT,  ///< A pointer, size, or caller-supplied limit is invalid.
    MOONLIGHT_PROTOCOL_RESULT_BUFFER_TOO_SMALL,  ///< The output buffer cannot hold the complete canonical value.
    MOONLIGHT_PROTOCOL_RESULT_INSUFFICIENT_SHARDS,  ///< Too few valid FEC rows remain to recover missing data.
    MOONLIGHT_PROTOCOL_RESULT_INTERNAL_ERROR,  ///< A local invariant failed despite structurally valid inputs.
    MOONLIGHT_PROTOCOL_RESULT_TRUNCATED,  ///< The input ends before the complete declared value.
    MOONLIGHT_PROTOCOL_RESULT_TRAILING_DATA,  ///< Bytes remain after the one complete value.
    MOONLIGHT_PROTOCOL_RESULT_STALE_CONTEXT,  ///< A delayed value belongs to another valid session or epoch.
    MOONLIGHT_PROTOCOL_RESULT_MALFORMED,  ///< A known value violates a structural version 1 invariant.
    MOONLIGHT_PROTOCOL_RESULT_CONTEXT_MISMATCH,  ///< A known value is invalid for the supplied direction or channel role.
    MOONLIGHT_PROTOCOL_RESULT_UNSUPPORTED  ///< The value uses an unknown version 1 registry entry or flag.
  } MoonlightProtocolResult;

  /**
   * @brief Identifies the direction of an application payload.
   */
  typedef enum MoonlightProtocolDirection {
    MOONLIGHT_PROTOCOL_DIRECTION_CLIENT_TO_HOST = 1,  ///< The payload travels from Artemis to Sunshine.
    MOONLIGHT_PROTOCOL_DIRECTION_HOST_TO_CLIENT = 2  ///< The payload travels from Sunshine to Artemis.
  } MoonlightProtocolDirection;

#ifdef __cplusplus
}
#endif
