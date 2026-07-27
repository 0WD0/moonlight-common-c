cmake_minimum_required(VERSION 3.13...4.0)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(moonlight-protocol-v1 LANGUAGES C)
endif()

get_filename_component(
  MOONLIGHT_PROTOCOL_V1_ROOT
  "${CMAKE_CURRENT_LIST_DIR}/../.."
  ABSOLUTE
)

option(
  MOONLIGHT_PROTOCOL_V1_BUILD_TESTS
  "Build the moonlight protocol version 1 native tests"
  OFF
)
option(
  MOONLIGHT_PROTOCOL_V1_BUILD_FUZZERS
  "Build the moonlight protocol version 1 libFuzzer targets"
  OFF
)
set(
  MOONLIGHT_PROTOCOL_V1_TESTDATA_DIR
  "${MOONLIGHT_PROTOCOL_V1_ROOT}/testdata/wire/v1"
  CACHE STRING
  "Runtime path containing the moonlight protocol version 1 test corpus"
)

function(moonlight_protocol_v1_apply_warnings target_name)
  if(MSVC)
    target_compile_options("${target_name}" PRIVATE /W4 /WX)
  elseif(CMAKE_C_COMPILER_ID STREQUAL "GNU"
      OR CMAKE_C_COMPILER_ID MATCHES "Clang")
    target_compile_options(
      "${target_name}"
      PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
  else()
    message(
      FATAL_ERROR
      "moonlight-protocol-v1 requires a compiler with configured warnings-as-errors"
    )
  endif()
endfunction()

add_library(moonlight-protocol-v1 STATIC control.c media.c wire.c)
add_library(moonlight::protocol-v1 ALIAS moonlight-protocol-v1)

set_target_properties(
  moonlight-protocol-v1
  PROPERTIES
    C_EXTENSIONS OFF
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

target_include_directories(
  moonlight-protocol-v1
  PUBLIC
    "$<BUILD_INTERFACE:${MOONLIGHT_PROTOCOL_V1_ROOT}/include>"
)

moonlight_protocol_v1_apply_warnings(moonlight-protocol-v1)

if(MOONLIGHT_PROTOCOL_V1_BUILD_TESTS)
  enable_testing()
  add_subdirectory(
    "${MOONLIGHT_PROTOCOL_V1_ROOT}/tests/protocol"
    "${CMAKE_CURRENT_BINARY_DIR}/tests"
  )
endif()

if(MOONLIGHT_PROTOCOL_V1_BUILD_FUZZERS)
  if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "moonlight-protocol-v1 fuzzers require Clang")
  endif()

  target_compile_options(
    moonlight-protocol-v1
    PRIVATE
      -fno-omit-frame-pointer
      -fsanitize=fuzzer-no-link,address,undefined
  )
  target_link_options(
    moonlight-protocol-v1
    INTERFACE
      -fno-omit-frame-pointer
      -fsanitize=address,undefined
  )

  add_subdirectory(
    "${MOONLIGHT_PROTOCOL_V1_ROOT}/fuzz"
    "${CMAKE_CURRENT_BINARY_DIR}/fuzz"
  )
endif()
