cmake_minimum_required(VERSION 3.10)
project(match3_game)

set(CMAKE_C_STANDARD 11)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(SDL2 REQUIRED)
find_package(SDL2_ttf REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(SDL2_mixer REQUIRED)

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

add_executable(match3_game 
    src/main.c
    src/model.c
    src/view.c
    src/controller.c
)

target_include_directories(match3_game PRIVATE include)
target_link_libraries(match3_game PRIVATE 
    SDL2::SDL2main 
    SDL2::SDL2 
    SDL2_ttf::SDL2_ttf 
    SDL2_image::SDL2_image 
    SDL2_mixer::SDL2_mixer
)

if(APPLE)
    target_link_libraries(match3_game PRIVATE "-framework Cocoa")
else()
    target_link_libraries(match3_game PRIVATE m)
endif()

enable_testing()
add_subdirectory(tests)
