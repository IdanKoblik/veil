if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "web/ needs the Emscripten toolchain; run web/build.sh")
endif()

add_executable(veil_wasm wasm/wrapper.c)

set_target_properties(veil_wasm PROPERTIES
    OUTPUT_NAME veil
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/dist"
    SUFFIX ".mjs"
)

target_link_libraries(veil_wasm PRIVATE veil::core)

target_link_options(veil_wasm PRIVATE
    "-sMODULARIZE=1"
    "-sEXPORT_ES6=1"
    "-sEXPORT_NAME=createVeil"
    "-sENVIRONMENT=web,node"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sINITIAL_MEMORY=64MB"
    "-sSTACK_SIZE=1MB"
    "-sEXIT_RUNTIME=0"
    "-sINVOKE_RUN=0"
    "-sFILESYSTEM=1"
    "-sEXPORTED_RUNTIME_METHODS=ccall,cwrap,FS,UTF8ToString"
    "-sEXPORTED_FUNCTIONS=_veil_init,_veil_set_verbose,_veil_free,_veil_encode,_veil_decode,_malloc,_free"
)

target_compile_options(veil_wasm PRIVATE "-O3")
target_link_options(veil_wasm PRIVATE "-O3")
