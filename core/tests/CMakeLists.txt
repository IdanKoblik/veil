file(GLOB_RECURSE VEIL_CORE_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c")

add_executable(veil_core_tests ${VEIL_CORE_TEST_SOURCES})
target_link_libraries(veil_core_tests PRIVATE veil::core veil::testing)

add_test(NAME core COMMAND veil_core_tests)
