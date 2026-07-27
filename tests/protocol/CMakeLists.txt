add_executable(test_moonlight_protocol_v1 test_media.c)

set_target_properties(
  test_moonlight_protocol_v1
  PROPERTIES
    C_EXTENSIONS OFF
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

target_link_libraries(
  test_moonlight_protocol_v1
  PRIVATE
    moonlight::protocol-v1
)

target_compile_definitions(
  test_moonlight_protocol_v1
  PRIVATE
    MOONLIGHT_PROTOCOL_V1_TESTDATA_DIR="${MOONLIGHT_PROTOCOL_V1_TESTDATA_DIR}"
)

moonlight_protocol_v1_apply_warnings(test_moonlight_protocol_v1)

add_test(
  NAME moonlight-protocol-v1.native
  COMMAND test_moonlight_protocol_v1
)

add_executable(test_moonlight_protocol_v1_wire test_wire.c)

set_target_properties(
  test_moonlight_protocol_v1_wire
  PROPERTIES
    C_EXTENSIONS OFF
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

target_link_libraries(
  test_moonlight_protocol_v1_wire
  PRIVATE
    moonlight::protocol-v1
)

moonlight_protocol_v1_apply_warnings(test_moonlight_protocol_v1_wire)

add_test(
  NAME moonlight-protocol-v1.wire.native
  COMMAND test_moonlight_protocol_v1_wire
)
