file(GLOB VEIL_THIRD_PARTY_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)
file(GLOB VEIL_THIRD_PARTY_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
)

if(EMSCRIPTEN)
    list(FILTER VEIL_THIRD_PARTY_SOURCES EXCLUDE REGEX "/flag\\.c$")
    list(FILTER VEIL_THIRD_PARTY_SOURCES EXCLUDE REGEX "/portable-file-dialogs\\.")
endif()

if(VEIL_THIRD_PARTY_SOURCES)
    add_library(veil_third_party STATIC
        ${VEIL_THIRD_PARTY_SOURCES}
        ${VEIL_THIRD_PARTY_HEADERS}
    )
    set(veil_third_party_scope PUBLIC)

    set_target_properties(veil_third_party PROPERTIES
        OUTPUT_NAME veil_vendor
        POSITION_INDEPENDENT_CODE ON
    )

    target_compile_options(veil_third_party PRIVATE
        $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-w>
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-w>
    )

    # raylib bundles its own copy of stb_image/stb_image_write, so anything
    # linking both it and veil::core sees every stb symbol defined twice.
    # The versions match, so emit ours weakly and let raylib's win when both
    # are present; on its own ours still resolves the reference.
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/stb.c"
            TARGET_DIRECTORY veil_third_party
            PROPERTIES COMPILE_DEFINITIONS
                "STBIDEF=__attribute__((weak));STBIWDEF=__attribute__((weak))")
    endif()
else()
    add_library(veil_third_party INTERFACE)
    set(veil_third_party_scope INTERFACE)
endif()
add_library(veil::third_party ALIAS veil_third_party)

target_include_directories(veil_third_party SYSTEM ${veil_third_party_scope}
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")

find_library(VEIL_MATH_LIBRARY m)
if(VEIL_MATH_LIBRARY)
    target_link_libraries(veil_third_party ${veil_third_party_scope} "${VEIL_MATH_LIBRARY}")
endif()
