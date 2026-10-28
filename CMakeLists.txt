cmake_minimum_required(VERSION 3.22)

project(VocalTune VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Prefer local JUCE if available, otherwise fetch it
set(DEFAULT_JUCE_PATH "F:/DEV/Audio VST Plugins/EQ Match/build-vs/_deps/juce-src")
if(EXISTS "${DEFAULT_JUCE_PATH}/CMakeLists.txt")
    set(VOCALTUNE_JUCE_PATH "${DEFAULT_JUCE_PATH}" CACHE PATH "Path to JUCE source")
else()
    set(VOCALTUNE_JUCE_PATH "" CACHE PATH "Path to JUCE source")
endif()

if(VOCALTUNE_JUCE_PATH AND EXISTS "${VOCALTUNE_JUCE_PATH}/CMakeLists.txt")
    message(STATUS "Using JUCE from local path: ${VOCALTUNE_JUCE_PATH}")
    add_subdirectory("${VOCALTUNE_JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
else()
    find_package(JUCE CONFIG QUIET)
    if(NOT JUCE_FOUND)
        message(STATUS "Fetching JUCE 8.0.6 from GitHub...")
        include(FetchContent)
        FetchContent_Declare(
            JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG 8.0.6
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(JUCE)
    endif()
endif()

juce_add_plugin(VocalTune
    COMPANY_NAME "VocalTune"
    BUNDLE_ID "com.vocaltune.audio"
    PLUGIN_MANUFACTURER_CODE VcTn
    PLUGIN_CODE VcTu
    FORMATS VST3 Standalone
    PRODUCT_NAME "Vocal Tune"
    DESCRIPTION "High Quality Real-Time Auto Tune Pitch Correction Plugin"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES "Fx|Pitch Shift" "Fx"
)

juce_generate_juce_header(VocalTune)

# Pinned, MIT-licensed headers; no network dependency for pitch processing.
add_library(vocaltune_pitch_backend INTERFACE)
target_include_directories(vocaltune_pitch_backend SYSTEM INTERFACE ThirdParty)

target_sources(VocalTune PRIVATE
    Source/Common/Parameters.h
    Source/Common/ScaleDefinition.h
    Source/DSP/PitchDetector.h
    Source/DSP/PitchDetector.cpp
    Source/DSP/PitchShifter.h
    Source/DSP/PitchShifter.cpp
    Source/DSP/FormantShifter.h
    Source/DSP/FormantShifter.cpp
    Source/DSP/VibratoGenerator.h
    Source/DSP/AutoTuneEngine.h
    Source/DSP/AutoTuneEngine.cpp
    Source/GUI/VocalTuneLookAndFeel.h
    Source/GUI/VocalTuneLookAndFeel.cpp
    Source/GUI/PitchDisplayComponent.h
    Source/GUI/PitchDisplayComponent.cpp
    Source/GUI/InteractiveKeyboardComponent.h
    Source/GUI/InteractiveKeyboardComponent.cpp
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp
)

target_compile_definitions(VocalTune PUBLIC
    JUCE_REPORT_APP_USAGE=0
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(VocalTune PRIVATE
    vocaltune_pitch_backend
    juce::juce_audio_processors
    juce::juce_audio_basics
    juce::juce_audio_formats
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# Ship the dependency licences with each binary distribution.
add_custom_target(VocalTuneNotices ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:VocalTune_VST3>/../Resources/ThirdParty"
        "$<TARGET_FILE_DIR:VocalTune_Standalone>/ThirdParty"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/signalsmith-stretch-LICENSE.txt"
        "$<TARGET_FILE_DIR:VocalTune_VST3>/../Resources/ThirdParty/signalsmith-stretch-LICENSE.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/signalsmith-linear/LICENSE.txt"
        "$<TARGET_FILE_DIR:VocalTune_VST3>/../Resources/ThirdParty/signalsmith-linear-LICENSE.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/signalsmith-stretch-LICENSE.txt"
        "$<TARGET_FILE_DIR:VocalTune_Standalone>/ThirdParty/signalsmith-stretch-LICENSE.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/signalsmith-linear/LICENSE.txt"
        "$<TARGET_FILE_DIR:VocalTune_Standalone>/ThirdParty/signalsmith-linear-LICENSE.txt"
    DEPENDS VocalTune_VST3 VocalTune_Standalone
    VERBATIM
)

include(CTest)
if(BUILD_TESTING)
    add_executable(VocalTuneDSPTests
        Tests/PitchShifterTests.cpp
        Source/DSP/PitchShifter.cpp
    )
    target_include_directories(VocalTuneDSPTests PRIVATE Source/DSP)
    target_compile_definitions(VocalTuneDSPTests PRIVATE
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        JUCE_REPORT_APP_USAGE=0
    )
    target_link_libraries(VocalTuneDSPTests PRIVATE
        vocaltune_pitch_backend
        juce::juce_audio_basics
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
    )
    add_test(NAME VocalTuneDSPTests COMMAND VocalTuneDSPTests)

    add_executable(VocalTuneEngineTests
        Tests/EngineTests.cpp
        Source/DSP/AutoTuneEngine.cpp
        Source/DSP/PitchDetector.cpp
        Source/DSP/PitchShifter.cpp
        Source/DSP/FormantShifter.cpp
    )
    target_include_directories(VocalTuneEngineTests PRIVATE Source/DSP)
    target_compile_definitions(VocalTuneEngineTests PRIVATE
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        JUCE_REPORT_APP_USAGE=0
    )
    target_link_libraries(VocalTuneEngineTests PRIVATE
        vocaltune_pitch_backend
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
    )
    add_test(NAME VocalTuneEngineTests COMMAND VocalTuneEngineTests)

    add_executable(VocalTuneRealtimeTests
        Tests/RealtimeTests.cpp
        Source/DSP/AutoTuneEngine.cpp
        Source/DSP/PitchDetector.cpp
        Source/DSP/PitchShifter.cpp
        Source/DSP/FormantShifter.cpp
    )
    target_include_directories(VocalTuneRealtimeTests PRIVATE Source/DSP)
    target_compile_definitions(VocalTuneRealtimeTests PRIVATE
        JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0 JUCE_REPORT_APP_USAGE=0)
    target_link_libraries(VocalTuneRealtimeTests PRIVATE
        vocaltune_pitch_backend
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
    add_test(NAME VocalTuneRealtimeTests COMMAND VocalTuneRealtimeTests)

    # Render the real editor offscreen and check its controls at resize extremes.
    add_executable(VocalTuneEditorTests Tests/EditorTests.cpp)
    target_link_libraries(VocalTuneEditorTests PRIVATE VocalTune)
    target_include_directories(VocalTuneEditorTests PRIVATE Source
        "$<TARGET_PROPERTY:VocalTune,INCLUDE_DIRECTORIES>")
    target_compile_definitions(VocalTuneEditorTests PRIVATE
        "$<TARGET_PROPERTY:VocalTune,COMPILE_DEFINITIONS>")
    add_test(NAME VocalTuneEditorTests COMMAND VocalTuneEditorTests "${CMAKE_BINARY_DIR}/ui-previews")
endif()

