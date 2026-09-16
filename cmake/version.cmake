if(GIT_EXECUTABLE AND EXISTS "${VEIL_SOURCE_DIR}/.git")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" describe --tags --always --dirty
        WORKING_DIRECTORY "${VEIL_SOURCE_DIR}"
        OUTPUT_VARIABLE VEIL_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE veil_git_result
        ERROR_QUIET
    )
endif()

if(NOT veil_git_result EQUAL 0 OR NOT VEIL_VERSION)
    file(STRINGS "${VEIL_SOURCE_DIR}/VERSION" VEIL_VERSION LIMIT_COUNT 1)
endif()

configure_file("${VEIL_SOURCE_DIR}/core/version.h.in" "${VEIL_SOURCE_DIR}/core/version.h" @ONLY)
