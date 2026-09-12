file(GLOB_RECURSE VEIL_CLI_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.c")
file(GLOB_RECURSE VEIL_CLI_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

list(FILTER VEIL_CLI_SOURCES EXCLUDE REGEX "/(tests|build)/")
list(FILTER VEIL_CLI_HEADERS EXCLUDE REGEX "/(tests|build)/")

# main() is kept out of the library so the test runner can bring its own.
set(VEIL_CLI_MAIN "")
foreach(source IN LISTS VEIL_CLI_SOURCES)
    get_filename_component(name "${source}" NAME)
    if(name STREQUAL "main.c")
        set(VEIL_CLI_MAIN "${source}")
    endif()
endforeach()

if(NOT VEIL_CLI_MAIN)
    message(FATAL_ERROR "No main.c under ${CMAKE_CURRENT_SOURCE_DIR}")
endif()

list(REMOVE_ITEM VEIL_CLI_SOURCES "${VEIL_CLI_MAIN}")

if(VEIL_CLI_SOURCES)
    add_library(veil_cli OBJECT ${VEIL_CLI_SOURCES} ${VEIL_CLI_HEADERS})
    set(veil_cli_scope PUBLIC)
else()
    add_library(veil_cli INTERFACE)
    set(veil_cli_scope INTERFACE)
endif()
add_library(veil::cli ALIAS veil_cli)

target_include_directories(veil_cli ${veil_cli_scope}
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")
target_link_libraries(veil_cli ${veil_cli_scope} veil::core veil::options)

add_executable(veil "${VEIL_CLI_MAIN}")
target_link_libraries(veil PRIVATE veil::cli)

foreach(candidate "${CMAKE_CURRENT_SOURCE_DIR}/linker.ld" "${PROJECT_SOURCE_DIR}/linker.ld")
    if(EXISTS "${candidate}")
        set(VEIL_LINKER_SCRIPT "${candidate}")
        break()
    endif()
endforeach()

if(VEIL_LINKER_SCRIPT)
    message(STATUS "Linking veil with ${VEIL_LINKER_SCRIPT}")
    target_link_options(veil_cli INTERFACE "LINKER:-T,${VEIL_LINKER_SCRIPT}")
    set_property(TARGET veil_cli APPEND
        PROPERTY INTERFACE_LINK_DEPENDS "${VEIL_LINKER_SCRIPT}")
endif()

install(TARGETS veil RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

if(VEIL_BUILD_TESTS)
    add_subdirectory(tests)
endif()
