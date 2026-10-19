add_library(undertow_core STATIC
    src/params.cpp
    src/filter.cpp
    src/engine.cpp
)
add_library(Undertow::core ALIAS undertow_core)

target_include_directories(undertow_core PUBLIC include)
target_compile_features(undertow_core PUBLIC cxx_std_17)
set_target_properties(undertow_core PROPERTIES
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)
target_compile_options(undertow_core PRIVATE -Wall -Wextra -Wpedantic)
