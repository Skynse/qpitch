cmake_minimum_required(VERSION 3.21)
project(QPitch VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(QPITCH_FETCH_DEPS "Fetch JUCE and clap-juce-extensions when local copies are not present" ON)
set(QPITCH_JUCE_GIT_TAG "8.0.8" CACHE STRING "JUCE git tag/branch used when fetching dependencies")
set(QPITCH_CLAP_JUCE_EXTENSIONS_GIT_TAG "main" CACHE STRING "clap-juce-extensions git tag/branch used when fetching dependencies")

include(FetchContent)

function(qpitch_add_or_fetch_dependency dep_name local_dir git_repository git_tag)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${local_dir}/CMakeLists.txt")
        add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/${local_dir}"
                         "${CMAKE_BINARY_DIR}/${local_dir}"
                         EXCLUDE_FROM_ALL)
        return()
    endif()

    if(NOT QPITCH_FETCH_DEPS)
        message(FATAL_ERROR
            "${dep_name} was not found at ${local_dir}. "
            "Either add that folder or reconfigure with -DQPITCH_FETCH_DEPS=ON.")
    endif()

    message(STATUS "Fetching ${dep_name} from ${git_repository} (${git_tag})")
    FetchContent_Declare(${dep_name}
        GIT_REPOSITORY "${git_repository}"
        GIT_TAG "${git_tag}"
        GIT_SHALLOW TRUE
        GIT_SUBMODULES_RECURSE TRUE)
    FetchContent_MakeAvailable(${dep_name})
endfunction()

qpitch_add_or_fetch_dependency(juce
    JUCE
    https://github.com/juce-framework/JUCE.git
    "${QPITCH_JUCE_GIT_TAG}")

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/clap-juce-extensions/CMakeLists.txt"
   AND (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/clap-juce-extensions/clap-libs/clap/CMakeLists.txt"
        OR NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/clap-juce-extensions/clap-libs/clap-helpers/CMakeLists.txt"))
    message(FATAL_ERROR
        "clap-juce-extensions exists, but its clap-libs submodules are missing. "
        "Run `git submodule update --init --recursive`, remove the local clap-juce-extensions folder so CMake can fetch it, "
        "or configure with a complete vendored copy.")
endif()

qpitch_add_or_fetch_dependency(clap_juce_extensions_project
    clap-juce-extensions
    https://github.com/free-audio/clap-juce-extensions.git
    "${QPITCH_CLAP_JUCE_EXTENSIONS_GIT_TAG}")

juce_add_plugin(QPitch
    COMPANY_NAME "QPitch"
    PLUGIN_MANUFACTURER_CODE "QPch"
    PLUGIN_CODE QPtc
    FORMATS VST3
    PRODUCT_NAME "QPitch"
    PLUGIN_IS_A_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_SYNTH FALSE
)

clap_juce_extensions_plugin(
    TARGET QPitch
    CLAP_ID "com.qpitch.autotune"
    CLAP_FEATURES audio-effect pitch-corrector
)

target_sources(QPitch PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/dsp/PitchDetector.cpp
    src/dsp/ScaleQuantizer.cpp
    src/dsp/PitchShifter.cpp
    src/dsp/FormantPreserver.cpp
)

target_compile_definitions(QPitch PUBLIC
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_JACK=0
    JUCE_ALSA=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(QPitch PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_plugin_client
    juce::juce_dsp
    juce::juce_gui_basics
    clap_juce_extensions
    ${CMAKE_DL_LIBS}
)

target_include_directories(QPitch PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
