# tests/CMakeLists.txt — Unit test runner for the pure Model layer
# The Model layer has ZERO dependency on SDL2, so these tests are
# completely standalone and run on every CI platform.

cmake_minimum_required(VERSION 3.15)

# ---- test_model executable ----
add_executable(test_model
    test_model.c
    # Pull in the model implementation directly (no SDL2 needed)
    "${CMAKE_SOURCE_DIR}/src/model.c"
)

target_include_directories(test_model PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
)

target_compile_options(test_model PRIVATE
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -g3>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /utf-8>
)

# Inherit sanitizer flags from parent if enabled
get_target_property(_asan_flags match3_game COMPILE_OPTIONS)
if(_asan_flags MATCHES "fsanitize=address")
    target_compile_options(test_model PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(test_model PRIVATE -fsanitize=address)
endif()

# Register with CTest
add_test(NAME ModelUnitTests COMMAND test_model)
set_tests_properties(ModelUnitTests PROPERTIES
    TIMEOUT 30
    LABELS  "unit;model"
)
