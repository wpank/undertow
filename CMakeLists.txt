cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET 11.0)

project(Undertow VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

option(UNDERTOW_BUILD_PLUGIN "Build the JUCE plugin" OFF)

add_subdirectory(core)

if(UNDERTOW_BUILD_PLUGIN)
    include(cmake/Juce.cmake)
    add_subdirectory(plugin)
endif()

add_subdirectory(tools/render)

if(BUILD_TESTING)
    add_subdirectory(tests/spec)
endif()
