# FindSDL2_ttf.cmake — fallback finder for SDL2_ttf
find_path(SDL2_TTF_INCLUDE_DIR
    NAMES SDL_ttf.h
    PATH_SUFFIXES SDL2
    HINTS /opt/homebrew/include /usr/local/include /usr/include
)
find_library(SDL2_TTF_LIBRARY
    NAMES SDL2_ttf
    HINTS /opt/homebrew/lib /usr/local/lib /usr/lib
)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SDL2_ttf
    REQUIRED_VARS SDL2_TTF_LIBRARY SDL2_TTF_INCLUDE_DIR
)
if(SDL2_ttf_FOUND AND NOT TARGET SDL2_ttf::SDL2_ttf)
    add_library(SDL2_ttf::SDL2_ttf UNKNOWN IMPORTED)
    set_target_properties(SDL2_ttf::SDL2_ttf PROPERTIES
        IMPORTED_LOCATION "${SDL2_TTF_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${SDL2_TTF_INCLUDE_DIR}"
    )
endif()
mark_as_advanced(SDL2_TTF_INCLUDE_DIR SDL2_TTF_LIBRARY)
