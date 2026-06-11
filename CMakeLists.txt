# ==============================================================
#  CMakeLists.txt — Match-3 Game (Cross-Platform, Modern CMake)
#  Requires: CMake 3.15+, SDL2, SDL2_ttf, SDL2_image, SDL2_mixer
# ==============================================================
cmake_minimum_required(VERSION 3.15)

project(match3_game
    VERSION 1.0.0
    DESCRIPTION "A cross-platform Match-3 puzzle game"
    LANGUAGES C
)

# --------------------------------------------------------------
#  Global C11 standard — enforced, no extensions
# --------------------------------------------------------------
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Export compile_commands.json for clangd / clang-tidy
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --------------------------------------------------------------
#  Build type default
# --------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()

# --------------------------------------------------------------
#  Options
# --------------------------------------------------------------
option(MATCH3_ENABLE_TESTS     "Build unit tests"              ON)
option(MATCH3_ENABLE_ASAN      "Enable AddressSanitizer"       OFF)
option(MATCH3_ENABLE_UBSAN     "Enable UBSanitizer"            OFF)
option(MATCH3_ENABLE_CLANG_TIDY "Run clang-tidy during build"  OFF)

# --------------------------------------------------------------
#  Find SDL2 family — prefer config-mode (SDL2 >= 2.0.22),
#  fall back to our bundled FindSDL2.cmake modules
# --------------------------------------------------------------
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(SDL2  REQUIRED)
find_package(SDL2_ttf   REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(SDL2_mixer REQUIRED)

# Normalise target names: SDL2 package may expose SDL2::SDL2 or
# SDL2::SDL2-static depending on version; create aliases when needed.
if(NOT TARGET SDL2::SDL2)
    add_library(SDL2::SDL2 ALIAS SDL2)
endif()
if(NOT TARGET SDL2_ttf::SDL2_ttf AND TARGET SDL2_TTF)
    add_library(SDL2_ttf::SDL2_ttf ALIAS SDL2_TTF)
endif()
if(NOT TARGET SDL2_image::SDL2_image AND TARGET SDL2_IMAGE)
    add_library(SDL2_image::SDL2_image ALIAS SDL2_IMAGE)
endif()
if(NOT TARGET SDL2_mixer::SDL2_mixer AND TARGET SDL2_MIXER)
    add_library(SDL2_mixer::SDL2_mixer ALIAS SDL2_MIXER)
endif()

# --------------------------------------------------------------
#  Source files  (strict C11 — all *.c)
# --------------------------------------------------------------
set(MATCH3_SOURCES
    src/main.c
    src/model.c
    src/view.c
    src/controller.c
)

# --------------------------------------------------------------
#  Executable
#  WIN32 keyword gives us a Windows GUI subsystem on MSVC/MinGW;
#  it is silently ignored on macOS and Linux.
# --------------------------------------------------------------
add_executable(match3_game WIN32 ${MATCH3_SOURCES})

target_include_directories(match3_game PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# --------------------------------------------------------------
#  Link SDL2 family
# --------------------------------------------------------------
target_link_libraries(match3_game PRIVATE
    SDL2::SDL2main
    SDL2::SDL2
    SDL2_ttf::SDL2_ttf
    SDL2_image::SDL2_image
    SDL2_mixer::SDL2_mixer
)

# On macOS SDL2 needs the Cocoa framework
if(APPLE)
    target_link_libraries(match3_game PRIVATE "-framework Cocoa")
endif()

# On Linux we may also need pthreads and dl
if(UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
    target_link_libraries(match3_game PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
endif()

# --------------------------------------------------------------
#  Compiler warnings & hardening
# --------------------------------------------------------------
if(MSVC)
    target_compile_options(match3_game PRIVATE
        /W4
        /WX           # warnings as errors
        /utf-8        # source & execution charset UTF-8
        /wd4996       # disable "deprecated" POSIX name warnings
    )
else()
    # GCC / Clang
    target_compile_options(match3_game PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wformat=2
        -Wconversion
        -Wno-unused-parameter   # common in stubs / callbacks
        $<$<CONFIG:Release>:-O2>
        $<$<CONFIG:Debug>:-O0 -g3>
    )
endif()

# --------------------------------------------------------------
#  Sanitizers (Debug-only, Clang/GCC)
# --------------------------------------------------------------
if((MATCH3_ENABLE_ASAN OR MATCH3_ENABLE_UBSAN) AND NOT MSVC)
    if(MATCH3_ENABLE_ASAN)
        target_compile_options(match3_game PRIVATE -fsanitize=address -fno-omit-frame-pointer)
        target_link_options(match3_game PRIVATE -fsanitize=address)
    endif()
    if(MATCH3_ENABLE_UBSAN)
        target_compile_options(match3_game PRIVATE -fsanitize=undefined)
        target_link_options(match3_game PRIVATE -fsanitize=undefined)
    endif()
endif()

# --------------------------------------------------------------
#  clang-tidy (optional static analysis)
# --------------------------------------------------------------
if(MATCH3_ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy)
    if(CLANG_TIDY_EXE)
        set_target_properties(match3_game PROPERTIES
            C_CLANG_TIDY "${CLANG_TIDY_EXE};--config-file=${CMAKE_SOURCE_DIR}/.clang-tidy"
        )
    endif()
endif()

# --------------------------------------------------------------
#  Unit tests (model layer is pure C, zero SDL2 dependency)
# --------------------------------------------------------------
if(MATCH3_ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# --------------------------------------------------------------
#  Install rules (optional, for packaging)
# --------------------------------------------------------------
include(GNUInstallDirs)
install(TARGETS match3_game
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# --------------------------------------------------------------
#  Summary
# --------------------------------------------------------------
message(STATUS "=== match3_game build config ===")
message(STATUS "  Platform        : ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Compiler        : ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "  Build type      : ${CMAKE_BUILD_TYPE}")
message(STATUS "  C standard      : C${CMAKE_C_STANDARD}")
message(STATUS "  Tests           : ${MATCH3_ENABLE_TESTS}")
message(STATUS "  ASan            : ${MATCH3_ENABLE_ASAN}")
message(STATUS "================================")
