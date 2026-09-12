file(GLOB_RECURSE VEIL_GUI_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
file(GLOB_RECURSE VEIL_GUI_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp")

list(FILTER VEIL_GUI_SOURCES EXCLUDE REGEX "/(tests|build|_deps)/")
list(FILTER VEIL_GUI_HEADERS EXCLUDE REGEX "/(tests|build|_deps)/")

# main() is kept out of the library so a test runner can bring its own.
set(VEIL_GUI_MAIN "")
foreach(source IN LISTS VEIL_GUI_SOURCES)
    get_filename_component(name "${source}" NAME)
    if(name STREQUAL "main.cpp")
        set(VEIL_GUI_MAIN "${source}")
    endif()
endforeach()

if(NOT VEIL_GUI_MAIN)
    message(FATAL_ERROR "No main.cpp under ${CMAKE_CURRENT_SOURCE_DIR}")
endif()

list(REMOVE_ITEM VEIL_GUI_SOURCES "${VEIL_GUI_MAIN}")

set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL REQUIRED)

set(VEIL_IMGUI_TAG   "v1.92.9b" CACHE STRING "Dear ImGui version to build against")
set(VEIL_IMPLOT_TAG  "v1.0"     CACHE STRING "ImPlot version to build against")
set(VEIL_RAYLIB_TAG  "5.5"      CACHE STRING "raylib version to build when none is installed")
set(VEIL_RLIMGUI_TAG "main"     CACHE STRING "rlImGui revision to build against")

include(FetchContent)

# rlImGui drives imgui through raylib; prefer an installed raylib.
find_package(raylib 5.0 QUIET)
if(NOT TARGET raylib)
    FetchContent_Declare(raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG "${VEIL_RAYLIB_TAG}"
        GIT_SHALLOW TRUE
    )
    set(BUILD_EXAMPLES OFF)
    set(BUILD_SHARED_LIBS OFF)
    set(CUSTOMIZE_BUILD OFF)
    FetchContent_MakeAvailable(raylib)
endif()
set(VEIL_RAYLIB raylib)

FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG "${VEIL_IMGUI_TAG}"
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR do-not-configure
)

FetchContent_Declare(implot
    GIT_REPOSITORY https://github.com/epezent/implot.git
    GIT_TAG "${VEIL_IMPLOT_TAG}"
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR do-not-configure
)

# rlImGui ships loose sources with no CMake build of its own.
FetchContent_Declare(rlimgui
    GIT_REPOSITORY https://github.com/raylib-extras/rlImGui.git
    GIT_TAG "${VEIL_RLIMGUI_TAG}"
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR do-not-configure
)

FetchContent_MakeAvailable(imgui implot rlimgui)

# imgui_demo.cpp and implot_demo.cpp are left out.
add_library(veil_imgui STATIC
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${rlimgui_SOURCE_DIR}/rlImGui.cpp"
    "${implot_SOURCE_DIR}/implot.cpp"
    "${implot_SOURCE_DIR}/implot_items.cpp"
)

target_include_directories(veil_imgui SYSTEM PUBLIC
    "${imgui_SOURCE_DIR}"
    "${imgui_SOURCE_DIR}/backends"
    "${implot_SOURCE_DIR}"
    "${rlimgui_SOURCE_DIR}"
)

target_link_libraries(veil_imgui PUBLIC ${VEIL_RAYLIB} OpenGL::GL)

target_compile_features(veil_imgui PUBLIC cxx_std_17)

set_target_properties(veil_imgui PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(veil_imgui PRIVATE -w)
endif()

if(VEIL_GUI_SOURCES)
    add_library(veil_gui OBJECT ${VEIL_GUI_SOURCES} ${VEIL_GUI_HEADERS})
    set(veil_gui_scope PUBLIC)
else()
    add_library(veil_gui INTERFACE)
    set(veil_gui_scope INTERFACE)
endif()
add_library(veil::gui ALIAS veil_gui)

target_include_directories(veil_gui ${veil_gui_scope}
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>")
target_link_libraries(veil_gui ${veil_gui_scope} veil::core veil::options veil_imgui)
target_compile_features(veil_gui ${veil_gui_scope} cxx_std_17)

add_executable(veil-gui "${VEIL_GUI_MAIN}")
target_link_libraries(veil-gui PRIVATE veil::gui)

install(TARGETS veil-gui RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

if(VEIL_BUILD_TESTS)
    add_subdirectory(tests)
endif()
