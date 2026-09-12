file(GLOB_RECURSE VEIL_GUI_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_executable(veil_gui_tests ${VEIL_GUI_TEST_SOURCES})
target_link_libraries(veil_gui_tests PRIVATE veil::gui veil::testing)

# No window is opened here, so the suite runs on a headless machine.
add_test(NAME gui COMMAND veil_gui_tests)
