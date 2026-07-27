add_executable(moonlight_protocol_v1_media_fuzzer media_fuzzer.c)

set_target_properties(
  moonlight_protocol_v1_media_fuzzer
  PROPERTIES
    C_EXTENSIONS OFF
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

target_link_libraries(
  moonlight_protocol_v1_media_fuzzer
  PRIVATE
    moonlight::protocol-v1
)

target_compile_options(
  moonlight_protocol_v1_media_fuzzer
  PRIVATE
    -fno-omit-frame-pointer
    -fsanitize=fuzzer,address,undefined
)
target_link_options(
  moonlight_protocol_v1_media_fuzzer
  PRIVATE
    -fno-omit-frame-pointer
    -fsanitize=fuzzer,address,undefined
)

moonlight_protocol_v1_apply_warnings(moonlight_protocol_v1_media_fuzzer)

add_executable(moonlight_protocol_v1_wire_fuzzer wire_fuzzer.c)

set_target_properties(
  moonlight_protocol_v1_wire_fuzzer
  PROPERTIES
    C_EXTENSIONS OFF
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

target_link_libraries(
  moonlight_protocol_v1_wire_fuzzer
  PRIVATE
    moonlight::protocol-v1
)

target_compile_options(
  moonlight_protocol_v1_wire_fuzzer
  PRIVATE
    -fno-omit-frame-pointer
    -fsanitize=fuzzer,address,undefined
)
target_link_options(
  moonlight_protocol_v1_wire_fuzzer
  PRIVATE
    -fno-omit-frame-pointer
    -fsanitize=fuzzer,address,undefined
)

moonlight_protocol_v1_apply_warnings(moonlight_protocol_v1_wire_fuzzer)
