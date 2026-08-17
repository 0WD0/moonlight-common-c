add_executable(test_moonlight_platform_crypto test_platform_crypto.c)

set_target_properties(
  test_moonlight_platform_crypto
  PROPERTIES
    C_EXTENSIONS OFF
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

target_link_libraries(
  test_moonlight_platform_crypto
  PRIVATE
    moonlight-common-c
)

if(MSVC)
  target_compile_options(test_moonlight_platform_crypto PRIVATE /W4 /WX)
elseif(CMAKE_C_COMPILER_ID STREQUAL "GNU"
    OR CMAKE_C_COMPILER_ID MATCHES "Clang")
  target_compile_options(
    test_moonlight_platform_crypto
    PRIVATE
      -Wall
      -Wextra
      -Wpedantic
      -Werror
  )
else()
  message(
    FATAL_ERROR
    "moonlight platform crypto tests require a compiler with configured warnings-as-errors"
  )
endif()

add_test(
  NAME moonlight-common.platform-crypto.native
  COMMAND test_moonlight_platform_crypto
)
