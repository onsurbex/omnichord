cmake_minimum_required(VERSION 3.22)
project(ChordHarp VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(CHORDHARP_DSP_ONLY "Build the engine tests without JUCE" OFF)
enable_testing()
add_executable(engine_tests tests/engine_tests.cpp)
target_include_directories(engine_tests PRIVATE Source)
add_test(NAME engine COMMAND engine_tests)
if(CHORDHARP_DSP_ONLY)
    return()
endif()
set(JUCE_PATH "" CACHE PATH "Path to JUCE 8 source checkout")
if(NOT EXISTS "${JUCE_PATH}/CMakeLists.txt")
    message(FATAL_ERROR "Set -DJUCE_PATH=/path/to/JUCE (JUCE 8.0.6)")
endif()
add_subdirectory("${JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE")
juce_add_plugin(ChordHarp
    COMPANY_NAME "ChordHarp"
    BUNDLE_ID "audio.chordharp.instrument"
    PLUGIN_MANUFACTURER_CODE Chhp
    PLUGIN_CODE Ch01
    PRODUCT_NAME "ChordHarp"
    FORMATS VST3 Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)
juce_generate_juce_header(ChordHarp)
target_sources(ChordHarp PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_definitions(ChordHarp PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(ChordHarp PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

option(CHORDHARP_PLUGIN_TESTS "Build JUCE processor integration checks" OFF)
if(CHORDHARP_PLUGIN_TESTS)
    juce_add_console_app(processor_tests PRODUCT_NAME "ChordHarp processor tests")
    juce_generate_juce_header(processor_tests)
    target_sources(processor_tests PRIVATE tests/processor_tests.cpp)
    target_link_libraries(processor_tests PRIVATE ChordHarp juce::juce_audio_utils)
    add_test(NAME processor COMMAND processor_tests)
endif()
