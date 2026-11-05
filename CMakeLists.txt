cmake_minimum_required(VERSION 3.22)
project(Omnichord VERSION 0.3.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
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
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/modules/foleys_gui_magic/modules/foleys_gui_magic/foleys_gui_magic.h")
    message(FATAL_ERROR "Initialise Foleys: git submodule update --init --recursive")
endif()
juce_add_module(modules/foleys_gui_magic/modules/foleys_gui_magic)
option(CHORDHARP_GUI_EDITOR "Include the Foleys visual design toolbox" ON)
juce_add_plugin(Omnichord
    COMPANY_NAME "Omnichord"
    BUNDLE_ID "audio.chordharp.instrument"
    PLUGIN_MANUFACTURER_CODE Chhp
    PLUGIN_CODE Ch01
    PRODUCT_NAME "Omnichord"
    FORMATS VST3 Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)
juce_generate_juce_header(Omnichord)
target_sources(Omnichord PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
if(APPLE)
    enable_language(OBJCXX)
    set_target_properties(Omnichord PROPERTIES OBJCXX_STANDARD 20 OBJCXX_STANDARD_REQUIRED ON)
    target_sources(Omnichord PRIVATE Source/MacKeyboard.mm)
endif()
# Bundle the original logo into both plugin formats, without a runtime file path.
set(CHORDHARP_LOGO "${CMAKE_CURRENT_SOURCE_DIR}/Assets/BlinblinLogo.png")
juce_add_binary_data(OmnichordBrand
    HEADER_NAME BinaryData.h
    NAMESPACE BinaryData
    SOURCES "${CHORDHARP_LOGO}" Assets/magic.xml)
target_link_libraries(Omnichord PRIVATE OmnichordBrand)
target_compile_definitions(Omnichord PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
    FOLEYS_ENABLE_BINARY_DATA=1 FOLEYS_ENABLE_OPEN_GL_CONTEXT=0
    FOLEYS_SHOW_GUI_EDITOR_PALLETTE=$<BOOL:${CHORDHARP_GUI_EDITOR}>)
target_link_libraries(Omnichord PRIVATE juce::juce_audio_utils foleys_gui_magic
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

option(CHORDHARP_PLUGIN_TESTS "Build JUCE processor integration checks" OFF)
if(CHORDHARP_PLUGIN_TESTS)
    juce_add_console_app(processor_tests PRODUCT_NAME "Omnichord processor tests")
    juce_generate_juce_header(processor_tests)
    target_compile_definitions(processor_tests PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)
    target_sources(processor_tests PRIVATE tests/processor_tests.cpp)
    target_link_libraries(processor_tests PRIVATE Omnichord OmnichordBrand foleys_gui_magic juce::juce_audio_utils)
    add_test(NAME processor COMMAND processor_tests)
endif()
