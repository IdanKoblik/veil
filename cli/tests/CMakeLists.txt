file(GLOB_RECURSE VEIL_CLI_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c")

add_executable(veil_cli_tests ${VEIL_CLI_TEST_SOURCES})
target_link_libraries(veil_cli_tests PRIVATE veil::cli veil::testing)

# e2e.c runs the real binary instead of linking it, so it needs the path and a
# guarantee that veil is built and up to date first.
target_compile_definitions(veil_cli_tests PRIVATE "VEIL_BINARY=\"$<TARGET_FILE:veil>\"")
add_dependencies(veil_cli_tests veil)

add_test(NAME cli COMMAND veil_cli_tests)
