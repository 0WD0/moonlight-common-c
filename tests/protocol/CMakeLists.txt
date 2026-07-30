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

add_executable(test_moonlight_protocol_v1_control test_control.c)

set_target_properties(
  test_moonlight_protocol_v1_control
  PROPERTIES
    C_EXTENSIONS OFF
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

target_link_libraries(
  test_moonlight_protocol_v1_control
  PRIVATE
    moonlight::protocol-v1
)

moonlight_protocol_v1_apply_warnings(test_moonlight_protocol_v1_control)

add_test(
  NAME moonlight-protocol-v1.control.native
  COMMAND test_moonlight_protocol_v1_control
)

add_executable(test_moonlight_protocol_v1_host_info test_host_info.c)

set_target_properties(
  test_moonlight_protocol_v1_host_info
  PROPERTIES
    C_EXTENSIONS OFF
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

target_link_libraries(
  test_moonlight_protocol_v1_host_info
  PRIVATE
    moonlight::protocol-v1
)

target_compile_definitions(
  test_moonlight_protocol_v1_host_info
  PRIVATE
    MOONLIGHT_PROTOCOL_V1_TESTDATA_DIR="${MOONLIGHT_PROTOCOL_V1_TESTDATA_DIR}"
)

moonlight_protocol_v1_apply_warnings(test_moonlight_protocol_v1_host_info)

add_test(
  NAME moonlight-protocol-v1.host-info.native
  COMMAND test_moonlight_protocol_v1_host_info
)

add_executable(test_moonlight_protocol_v1_app_list test_app_list.c)

set_target_properties(
  test_moonlight_protocol_v1_app_list
  PROPERTIES
    C_EXTENSIONS OFF
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

target_link_libraries(
  test_moonlight_protocol_v1_app_list
  PRIVATE
    moonlight::protocol-v1
)

target_compile_definitions(
  test_moonlight_protocol_v1_app_list
  PRIVATE
    MOONLIGHT_PROTOCOL_V1_TESTDATA_DIR="${MOONLIGHT_PROTOCOL_V1_TESTDATA_DIR}"
)

moonlight_protocol_v1_apply_warnings(test_moonlight_protocol_v1_app_list)

add_test(
  NAME moonlight-protocol-v1.app-list.native
  COMMAND test_moonlight_protocol_v1_app_list
)

add_executable(
  test_moonlight_protocol_v1_display_list
  test_display_list.c
)

set_target_properties(
  test_moonlight_protocol_v1_display_list
  PROPERTIES
    C_EXTENSIONS OFF
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

target_link_libraries(
  test_moonlight_protocol_v1_display_list
  PRIVATE
    moonlight::protocol-v1
)

moonlight_protocol_v1_apply_warnings(
  test_moonlight_protocol_v1_display_list
)

add_test(
  NAME moonlight-protocol-v1.display-list.native
  COMMAND test_moonlight_protocol_v1_display_list
)

add_executable(test_moonlight_protocol_v1_session test_session.c)

set_target_properties(
  test_moonlight_protocol_v1_session
  PROPERTIES
    C_EXTENSIONS OFF
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

target_link_libraries(
  test_moonlight_protocol_v1_session
  PRIVATE
    moonlight::protocol-v1
)

moonlight_protocol_v1_apply_warnings(test_moonlight_protocol_v1_session)

add_test(
  NAME moonlight-protocol-v1.session.native
  COMMAND test_moonlight_protocol_v1_session
)

add_executable(test_moonlight_protocol_v1_input test_input.c)

set_target_properties(
  test_moonlight_protocol_v1_input
  PROPERTIES
    C_EXTENSIONS OFF
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

target_link_libraries(
  test_moonlight_protocol_v1_input
  PRIVATE
    moonlight::protocol-v1
)

moonlight_protocol_v1_apply_warnings(test_moonlight_protocol_v1_input)

add_test(
  NAME moonlight-protocol-v1.input.native
  COMMAND test_moonlight_protocol_v1_input
)

add_executable(test_moonlight_protocol_v1_client_proof test_client_proof.c)

set_target_properties(
  test_moonlight_protocol_v1_client_proof
  PROPERTIES
    C_EXTENSIONS OFF
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

target_link_libraries(
  test_moonlight_protocol_v1_client_proof
  PRIVATE
    moonlight::protocol-v1
)

target_compile_definitions(
  test_moonlight_protocol_v1_client_proof
  PRIVATE
    MOONLIGHT_PROTOCOL_V1_TESTDATA_DIR="${MOONLIGHT_PROTOCOL_V1_TESTDATA_DIR}"
)

moonlight_protocol_v1_apply_warnings(test_moonlight_protocol_v1_client_proof)

add_test(
  NAME moonlight-protocol-v1.client-proof.native
  COMMAND test_moonlight_protocol_v1_client_proof
)

add_executable(test_moonlight_protocol_v1_pairing test_pairing.c)

set_target_properties(
  test_moonlight_protocol_v1_pairing
  PROPERTIES
    C_EXTENSIONS OFF
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

target_link_libraries(
  test_moonlight_protocol_v1_pairing
  PRIVATE
    moonlight::protocol-v1
)

target_compile_definitions(
  test_moonlight_protocol_v1_pairing
  PRIVATE
    MOONLIGHT_PROTOCOL_V1_TESTDATA_DIR="${MOONLIGHT_PROTOCOL_V1_TESTDATA_DIR}"
)

moonlight_protocol_v1_apply_warnings(test_moonlight_protocol_v1_pairing)

add_test(
  NAME moonlight-protocol-v1.pairing.native
  COMMAND test_moonlight_protocol_v1_pairing
)
