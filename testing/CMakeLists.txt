add_library(veil_testing INTERFACE)
add_library(veil::testing ALIAS veil_testing)

target_include_directories(veil_testing INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")
target_link_libraries(veil_testing INTERFACE veil::core)
