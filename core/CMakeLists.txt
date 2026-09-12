file(GLOB_RECURSE VEIL_CORE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.c")
file(GLOB_RECURSE VEIL_CORE_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

list(FILTER VEIL_CORE_SOURCES EXCLUDE REGEX "/(tests|build|third_party)/")
list(FILTER VEIL_CORE_HEADERS EXCLUDE REGEX "/(tests|build|third_party)/")

if(NOT VEIL_CORE_SOURCES)
    message(FATAL_ERROR "No .c files under ${CMAKE_CURRENT_SOURCE_DIR}")
endif()

find_package(PkgConfig QUIET)

get_filename_component(VEIL_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}" DIRECTORY)

set(VEIL_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/include")
file(MAKE_DIRECTORY "${VEIL_INCLUDE_DIR}")
file(CREATE_LINK "${CMAKE_CURRENT_SOURCE_DIR}" "${VEIL_INCLUDE_DIR}/veil"
    SYMBOLIC RESULT veil_include_alias)

if(NOT veil_include_alias EQUAL 0)
    message(STATUS "Could not link core/ as veil/ (${veil_include_alias}); copying headers")
    foreach(header IN LISTS VEIL_CORE_HEADERS)
        file(RELATIVE_PATH relative "${CMAKE_CURRENT_SOURCE_DIR}" "${header}")
        configure_file("${header}" "${VEIL_INCLUDE_DIR}/veil/${relative}" COPYONLY)
    endforeach()
endif()

if(NOT TARGET veil::third_party)
    set(VEIL_THIRD_PARTY_DIR "" CACHE PATH
        "Directory holding the vendored third-party sources")

    if(NOT VEIL_THIRD_PARTY_DIR)
        foreach(candidate
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party"
            "${VEIL_ROOT_DIR}/third_party"
            "${PROJECT_SOURCE_DIR}/third_party")
            if(EXISTS "${candidate}/CMakeLists.txt")
                set(VEIL_THIRD_PARTY_DIR "${candidate}")
                break()
            endif()
        endforeach()
    endif()

    if(NOT VEIL_THIRD_PARTY_DIR)
        message(FATAL_ERROR
            "Could not find a third_party directory next to ${CMAKE_CURRENT_SOURCE_DIR}. "
            "Point VEIL_THIRD_PARTY_DIR at one.")
    endif()

    message(STATUS "Resolved third_party to ${VEIL_THIRD_PARTY_DIR}")
    add_subdirectory("${VEIL_THIRD_PARTY_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/third_party")
endif()

if(NOT TARGET veil::sodium)
    set(VEIL_SODIUM_ROOT "" CACHE PATH "Location of a libsodium installation")

    add_library(veil_sodium INTERFACE)
    add_library(veil::sodium ALIAS veil_sodium)

    if(PKG_CONFIG_FOUND AND NOT VEIL_SODIUM_ROOT AND NOT EMSCRIPTEN)
        pkg_check_modules(VEIL_SODIUM_PC QUIET IMPORTED_TARGET libsodium)
    endif()

    if(TARGET PkgConfig::VEIL_SODIUM_PC)
        message(STATUS "Resolved libsodium ${VEIL_SODIUM_PC_VERSION} via pkg-config")
        target_link_libraries(veil_sodium INTERFACE PkgConfig::VEIL_SODIUM_PC)
    else()
        find_path(VEIL_SODIUM_INCLUDE_DIR sodium.h
            HINTS "${VEIL_SODIUM_ROOT}" PATH_SUFFIXES include)
        find_library(VEIL_SODIUM_LIBRARY NAMES sodium libsodium
            HINTS "${VEIL_SODIUM_ROOT}" PATH_SUFFIXES lib)

        if(NOT VEIL_SODIUM_INCLUDE_DIR OR NOT VEIL_SODIUM_LIBRARY)
            message(FATAL_ERROR
                "Could not find libsodium. Install it (Arch: libsodium, Debian: "
                "libsodium-dev, macOS: brew install libsodium) or point "
                "VEIL_SODIUM_ROOT at an installation.")
        endif()

        message(STATUS "Resolved libsodium to ${VEIL_SODIUM_LIBRARY}")
        target_include_directories(veil_sodium SYSTEM INTERFACE "${VEIL_SODIUM_INCLUDE_DIR}")
        target_link_libraries(veil_sodium INTERFACE "${VEIL_SODIUM_LIBRARY}")
    endif()
endif()

if(NOT TARGET veil::jpeg)
    find_package(JPEG QUIET)

    if(NOT JPEG_FOUND)
        message(FATAL_ERROR
            "Could not find libjpeg. Install it (Arch: libjpeg-turbo, Debian: "
            "libjpeg-dev, macOS: brew install jpeg-turbo) or point "
            "JPEG_ROOT at an installation.")
    endif()

    message(STATUS "Resolved libjpeg ${JPEG_VERSION} to ${JPEG_LIBRARIES}")
    add_library(veil_jpeg INTERFACE)
    add_library(veil::jpeg ALIAS veil_jpeg)
    target_link_libraries(veil_jpeg INTERFACE JPEG::JPEG)
endif()

add_library(veil_core STATIC
    ${VEIL_CORE_SOURCES}
    ${VEIL_CORE_HEADERS}
)
add_library(veil::core ALIAS veil_core)

set_target_properties(veil_core PROPERTIES
    OUTPUT_NAME veil
    POSITION_INDEPENDENT_CODE ON
)

target_include_directories(veil_core
    PUBLIC
        "$<BUILD_INTERFACE:${VEIL_INCLUDE_DIR}>"
)

target_link_libraries(veil_core PUBLIC veil::options veil::third_party veil::sodium veil::jpeg)

if(VEIL_BUILD_TESTS)
    add_subdirectory(tests)
endif()
