add_executable(undertow_render main.cpp)

target_link_libraries(undertow_render PRIVATE Undertow::core)

target_compile_features(undertow_render PRIVATE cxx_std_17)
set_target_properties(undertow_render PROPERTIES
    CXX_EXTENSIONS OFF
)
