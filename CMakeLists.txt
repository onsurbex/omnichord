cmake_minimum_required(VERSION 3.22)
project(Omnichord VERSION 0.7.3 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(CHORDHARP_DSP_ONLY "Build the engine tests without JUCE" OFF)
enable_testing()
add_executable(engine_tests tests/engine_tests.cpp)
target_include_directories(engine_tests PRIVATE Source)
add_test(NAME engine COMMAND engine_tests)
add_executable(synthesis_tests tests/synthesis_tests.cpp)
target_include_directories(synthesis_tests PRIVATE Source)
add_test(NAME synthesis COMMAND synthesis_tests)
add_executable(mixer_tests tests/mixer_tests.cpp)
target_include_directories(mixer_tests PRIVATE Source)
add_test(NAME mixer COMMAND mixer_tests)
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
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Assets/omnichord.png"
    FORMATS VST3 Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(Omnichord_Standalone PRIVATE Source/StandaloneApp.cpp)
target_compile_definitions(Omnichord_Standalone PRIVATE JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1)
juce_generate_juce_header(Omnichord)
target_sources(Omnichord PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/Presets.cpp Source/SynthParameters.cpp Source/SampleLibrary.cpp Source/RhythmParameters.cpp Source/MixerParameters.cpp Source/StateMigration.cpp)
if(APPLE)
    enable_language(OBJCXX)
    set_target_properties(Omnichord PROPERTIES OBJCXX_STANDARD 20 OBJCXX_STANDARD_REQUIRED ON)
    target_sources(Omnichord PRIVATE Source/MacKeyboard.mm)
endif()
# Bundle the original logo into both plugin formats, without a runtime file path.
set(CHORDHARP_LOGO "${CMAKE_CURRENT_SOURCE_DIR}/Assets/BlinblinLogo.png")
# Embed the real factory pack when present. A placeholder is never treated as audio.
set(OMNICHORD_HAS_FACTORY_SAMPLES 0)
set(OMNICHORD_FACTORY_ASSETS "")
set(OMNICHORD_SAMPLE_ZIP "${CMAKE_CURRENT_SOURCE_DIR}/samples/omnichord_samples.zip")
if(EXISTS "${OMNICHORD_SAMPLE_ZIP}")
    file(SIZE "${OMNICHORD_SAMPLE_ZIP}" OMNICHORD_SAMPLE_BYTES)
    file(READ "${OMNICHORD_SAMPLE_ZIP}" OMNICHORD_SAMPLE_SIGNATURE LIMIT 4 HEX)
    if(OMNICHORD_SAMPLE_BYTES GREATER 22 AND OMNICHORD_SAMPLE_SIGNATURE STREQUAL "504b0304")
        configure_file("${OMNICHORD_SAMPLE_ZIP}" "${CMAKE_CURRENT_BINARY_DIR}/FactorySamples.zip" COPYONLY)
        set(OMNICHORD_FACTORY_ASSETS "${CMAKE_CURRENT_BINARY_DIR}/FactorySamples.zip")
        set(OMNICHORD_HAS_FACTORY_SAMPLES 1)
    endif()
endif()
target_compile_definitions(Omnichord PUBLIC OMNICHORD_HAS_FACTORY_SAMPLES=${OMNICHORD_HAS_FACTORY_SAMPLES})
juce_add_binary_data(OmnichordBrand
    HEADER_NAME BinaryData.h
    NAMESPACE BinaryData
    SOURCES "${CHORDHARP_LOGO}" Assets/BlinblinLogo_white.png Assets/magic.xml ${OMNICHORD_FACTORY_ASSETS})
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
    juce_add_console_app(ambience_tests PRODUCT_NAME "Omnichord ambience memory checks")
    juce_generate_juce_header(ambience_tests)
    target_sources(ambience_tests PRIVATE tests/ambience_tests.cpp)
    target_link_libraries(ambience_tests PRIVATE juce::juce_audio_basics)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(mixer_tests PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
        target_link_options(mixer_tests PRIVATE -fsanitize=address,undefined)
        target_compile_options(synthesis_tests PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
        target_link_options(synthesis_tests PRIVATE -fsanitize=address,undefined)
        target_compile_options(ambience_tests PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
        target_link_options(ambience_tests PRIVATE -fsanitize=address,undefined)
    endif()
    add_test(NAME ambience_memory COMMAND ambience_tests)
    set_tests_properties(ambience_memory synthesis mixer PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
endif()
