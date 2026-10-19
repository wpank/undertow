# Undertow spec tests: the acceptance tests of docs/specs/, written before the code. Change the code, never
# these tests.
#
# Each file pN/<name>_test.cpp builds its own executable, spec_pN_<name>, so a task's check compiles only
# the code it tests (two tasks working in parallel never break each other's check). tools/spec.sh builds
# and runs one of them by name.
#
# A test file lists the repository files it needs in "// spec-needs: <path>" lines. When they all exist,
# the executable joins the default build and ctest (so tools/check.sh runs every spec test whose code is
# in place); until then it is built only on request, and fails to compile with a missing-file error.
#
# A file with a "// spec-preset: plugin" line tests the JUCE plugin: it is defined only when the plugin
# target Undertow exists (the plugin preset), and links it.
#
# A file with a "// spec-target: <target>" line runs a program the build makes (the offline renderer): once that
# target exists, the test depends on it and gets its path as SPEC_TARGET_<TARGET> (upper case).
#
# Needs: the target Undertow::core (docs/specs/phase-0-foundation.md); for plugin tests, the target Undertow.

add_library(undertow_spec_support OBJECT
    support/spec_main.cpp
    support/spec_alloc.cpp
    support/spec_analysis.cpp
)
target_include_directories(undertow_spec_support PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/support"
    "${PROJECT_SOURCE_DIR}/third_party/doctest"
)
target_compile_features(undertow_spec_support PUBLIC cxx_std_17)

file(GLOB undertow_spec_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/p*/*_test.cpp")
list(SORT undertow_spec_sources)

foreach(source IN LISTS undertow_spec_sources)
    get_filename_component(file_name "${source}" NAME_WE)
    get_filename_component(phase_dir "${source}" DIRECTORY)
    get_filename_component(phase "${phase_dir}" NAME)
    string(REGEX REPLACE "_test$" "" test_name "${file_name}")
    set(target "spec_${phase}_${test_name}")

    file(STRINGS "${source}" preset_lines REGEX "^// spec-preset: ")
    set(is_plugin_test FALSE)
    if(preset_lines MATCHES "plugin")
        set(is_plugin_test TRUE)
        if(NOT TARGET Undertow)
            continue()
        endif()
    endif()

    set(present TRUE)
    file(STRINGS "${source}" needs_lines REGEX "^// spec-needs: ")
    foreach(line IN LISTS needs_lines)
        string(REGEX REPLACE "^// spec-needs: *" "" needed "${line}")
        string(STRIP "${needed}" needed)
        if(NOT EXISTS "${PROJECT_SOURCE_DIR}/${needed}")
            set(present FALSE)
        endif()
    endforeach()

    add_executable(${target} "${source}" $<TARGET_OBJECTS:undertow_spec_support>)
    target_include_directories(${target} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/support"
        "${PROJECT_SOURCE_DIR}/third_party/doctest"
    )
    target_compile_features(${target} PRIVATE cxx_std_17)
    target_compile_definitions(${target} PRIVATE
        UNDERTOW_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
        UNDERTOW_BINARY_DIR="${PROJECT_BINARY_DIR}"
    )
    target_link_libraries(${target} PRIVATE Undertow::core)
    if(is_plugin_test)
        # The plugin's shared code (createPluginFilter, UndertowProcessor, UndertowEditor) with the JUCE
        # modules already compiled into it: the test takes the plugin's include directories and definitions
        # instead of linking the JUCE module targets again, which would compile JUCE a second time. And the
        # paths of the three format bundles, which the test checks once they are built.
        target_include_directories(${target} PRIVATE
            "${PROJECT_SOURCE_DIR}/plugin"
            $<TARGET_PROPERTY:Undertow,INCLUDE_DIRECTORIES>
        )
        target_link_libraries(${target} PRIVATE Undertow)
        target_compile_definitions(${target} PRIVATE
            $<TARGET_PROPERTY:Undertow,COMPILE_DEFINITIONS>
            UNDERTOW_VST3_BUNDLE="$<TARGET_BUNDLE_DIR:Undertow_VST3>"
            UNDERTOW_AU_BUNDLE="$<TARGET_BUNDLE_DIR:Undertow_AU>"
            UNDERTOW_STANDALONE_BUNDLE="$<TARGET_BUNDLE_DIR:Undertow_Standalone>"
        )
        add_dependencies(${target} Undertow_VST3 Undertow_AU Undertow_Standalone)
    endif()
    file(STRINGS "${source}" target_lines REGEX "^// spec-target: ")
    foreach(line IN LISTS target_lines)
        string(REGEX REPLACE "^// spec-target: *" "" program "${line}")
        string(STRIP "${program}" program)
        string(TOUPPER "${program}" program_define)
        if(TARGET ${program})
            add_dependencies(${target} ${program})
            target_compile_definitions(${target} PRIVATE SPEC_TARGET_${program_define}="$<TARGET_FILE:${program}>")
        else()
            set(present FALSE)
            target_compile_definitions(${target} PRIVATE SPEC_TARGET_${program_define}="")
        endif()
    endforeach()
    if(present)
        add_test(NAME ${target} COMMAND ${target} --no-version)
    else()
        set_target_properties(${target} PROPERTIES EXCLUDE_FROM_ALL TRUE)
    endif()
endforeach()
