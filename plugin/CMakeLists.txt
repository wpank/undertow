juce_add_binary_data(UndertowFonts
    HEADER_NAME UndertowFonts.h
    NAMESPACE UndertowFonts
    SOURCES
        ${PROJECT_SOURCE_DIR}/third_party/fonts/Michroma-Regular.ttf
        ${PROJECT_SOURCE_DIR}/third_party/fonts/GeistMono-Regular.ttf
        ${PROJECT_SOURCE_DIR}/third_party/fonts/GeistMono-Light.ttf
)

juce_add_plugin(Undertow
    VERSION 2.0.0
    COMPANY_NAME "Metakosmia"
    PLUGIN_MANUFACTURER_CODE Mtks
    PLUGIN_CODE MkU2
    BUNDLE_ID "audio.metakosmia.undertow2"
    PRODUCT_NAME "Metakosmia Undertow 2"
    FORMATS VST3 AU Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD FALSE
)

file(GLOB undertow_ui_sources CONFIGURE_DEPENDS ui/*.cpp)

target_sources(Undertow PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp
    ${undertow_ui_sources}
)

target_compile_definitions(Undertow PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Undertow PRIVATE
    Undertow::core
    UndertowFonts
    juce::juce_audio_utils
)

target_link_libraries(Undertow PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

target_include_directories(Undertow PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
