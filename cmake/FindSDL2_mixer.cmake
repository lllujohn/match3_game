# FindSDL2_mixer.cmake — fallback finder for SDL2_mixer
find_path(SDL2_MIXER_INCLUDE_DIR
    NAMES SDL_mixer.h
    PATH_SUFFIXES SDL2
    HINTS /opt/homebrew/include /usr/local/include /usr/include
)
find_library(SDL2_MIXER_LIBRARY
    NAMES SDL2_mixer
    HINTS /opt/homebrew/lib /usr/local/lib /usr/lib
)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SDL2_mixer
    REQUIRED_VARS SDL2_MIXER_LIBRARY SDL2_MIXER_INCLUDE_DIR
)
if(SDL2_mixer_FOUND AND NOT TARGET SDL2_mixer::SDL2_mixer)
    add_library(SDL2_mixer::SDL2_mixer UNKNOWN IMPORTED)
    set_target_properties(SDL2_mixer::SDL2_mixer PROPERTIES
        IMPORTED_LOCATION "${SDL2_MIXER_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${SDL2_MIXER_INCLUDE_DIR}"
    )
endif()
mark_as_advanced(SDL2_MIXER_INCLUDE_DIR SDL2_MIXER_LIBRARY)
