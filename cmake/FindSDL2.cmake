# FindSDL2.cmake — fallback finder for SDL2 (used when SDL2Config.cmake
# is not installed, e.g. system packages on older Ubuntu/Debian).
# Sets SDL2::SDL2 imported target.

find_path(SDL2_INCLUDE_DIR
    NAMES SDL2/SDL.h
    HINTS
        $ENV{SDL2DIR}
        /opt/homebrew/include
        /usr/local/include
        /usr/include
)

find_library(SDL2_LIBRARY
    NAMES SDL2
    HINTS
        $ENV{SDL2DIR}
        /opt/homebrew/lib
        /usr/local/lib
        /usr/lib
)

find_library(SDL2MAIN_LIBRARY
    NAMES SDL2main
    HINTS
        $ENV{SDL2DIR}
        /opt/homebrew/lib
        /usr/local/lib
        /usr/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SDL2
    REQUIRED_VARS SDL2_LIBRARY SDL2_INCLUDE_DIR
)

if(SDL2_FOUND AND NOT TARGET SDL2::SDL2)
    add_library(SDL2::SDL2 UNKNOWN IMPORTED)
    set_target_properties(SDL2::SDL2 PROPERTIES
        IMPORTED_LOCATION "${SDL2_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIR}"
    )
    if(SDL2MAIN_LIBRARY)
        add_library(SDL2::SDL2main UNKNOWN IMPORTED)
        set_target_properties(SDL2::SDL2main PROPERTIES
            IMPORTED_LOCATION "${SDL2MAIN_LIBRARY}"
        )
    endif()
endif()

mark_as_advanced(SDL2_INCLUDE_DIR SDL2_LIBRARY SDL2MAIN_LIBRARY)
