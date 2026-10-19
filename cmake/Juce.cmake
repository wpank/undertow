# Discover JUCE: use UNDERTOW_JUCE_PREFIX if set, otherwise run tools/fetch-juce.sh --prefix.

set(UNDERTOW_JUCE_PREFIX "" CACHE PATH "Path to the JUCE install prefix (empty: auto-detect via tools/fetch-juce.sh)")

if(NOT UNDERTOW_JUCE_PREFIX)
    execute_process(
        COMMAND "${CMAKE_SOURCE_DIR}/tools/fetch-juce.sh" --prefix
        OUTPUT_VARIABLE _juce_prefix
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _juce_result
    )
    if(NOT _juce_result EQUAL 0)
        message(FATAL_ERROR "tools/fetch-juce.sh --prefix failed (exit code ${_juce_result})")
    endif()
    set(UNDERTOW_JUCE_PREFIX "${_juce_prefix}")
endif()

find_package(JUCE 8.0.12 EXACT CONFIG REQUIRED PATHS "${UNDERTOW_JUCE_PREFIX}" NO_DEFAULT_PATH)
