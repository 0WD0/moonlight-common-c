/**
 * @file mic.h
 * @brief Encodes and decodes canonical protocol version 1 microphone datagrams.
 *
 * A MIC DATAGRAM carries one complete opus frame (48 kHz, mono, 20 ms) from a
 * client to the host. The transport (QUIC) already provides encryption and
 * loss detection, so the payload is the raw opus packet after the common
 * DATAGRAM header.
 */
#pragma once

#include <moonlight/protocol/media.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Maximum accepted MIC DATAGRAM opus payload in bytes.
 */
#define MOONLIGHT_PROTOCOL_V1_MIC_MAX_PAYLOAD_SIZE 1500u

/**
 * @brief Minimum accepted MIC DATAGRAM opus payload in bytes.
 */
#define MOONLIGHT_PROTOCOL_V1_MIC_MIN_PAYLOAD_SIZE 1u

/**
 * @brief Builds one canonical real-time MIC DATAGRAM.
 *
 * @param session_wire_id Exact nonzero active Stream Session wire ID.
 * @param sequence Per-channel nonzero monotonically increasing serial.
 * @param opus Complete opus packet bytes.
 * @param opus_size Number of opus bytes, in `[1, MOONLIGHT_PROTOCOL_V1_MIC_MAX_PAYLOAD_SIZE]`.
 * @param output Destination buffer.
 * @param output_size Available bytes in `output`.
 * @param encoded_size Receives the exact complete DATAGRAM size.
 * @return The codec result.
 */
MoonlightProtocolResult MoonlightProtocolV1EncodeMicDatagram(
  uint32_t session_wire_id,
  uint32_t sequence,
  const uint8_t *opus,
  size_t opus_size,
  uint8_t *output,
  size_t output_size,
  size_t *encoded_size
);

/**
 * @brief Decodes one complete canonical real-time MIC DATAGRAM.
 *
 * The decoder requires the exact active session and microphone channel, zero
 * flags and Media Epoch, and a nonzero sequence. The opus payload is copied
 * to `opus` only on success.
 *
 * @param expected_session_wire_id Exact nonzero active Stream Session wire ID.
 * @param input Complete DATAGRAM bytes.
 * @param input_size Number of bytes in `input`.
 * @param opus Receives the opus payload bytes only on success.
 * @param opus_capacity Available bytes in `opus`.
 * @param opus_size Receives the exact opus payload size.
 * @return The codec result.
 */
MoonlightProtocolResult MoonlightProtocolV1DecodeMicDatagram(
  uint32_t expected_session_wire_id,
  const uint8_t *input,
  size_t input_size,
  uint8_t *opus,
  size_t opus_capacity,
  size_t *opus_size
);

#ifdef __cplusplus
}
#endif
