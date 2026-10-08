cmake_minimum_required(VERSION 3.21)
project(QPitch VERSION 2.0.0 LANGUAGES C CXX)

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

# Pinned Rubber Band 4.0.0 source, including its built-in FFT/resampler.
# Static linkage keeps VST3/CLAP releases self-contained on every platform.
add_library(qpitch_rubberband STATIC third_party/rubberband/single/RubberBandSingle.cpp)
target_include_directories(qpitch_rubberband PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/rubberband)
target_compile_definitions(qpitch_rubberband PRIVATE RUBBERBAND_STATIC)
set_target_properties(qpitch_rubberband PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES)
if(APPLE)
    target_link_libraries(qpitch_rubberband PUBLIC "-framework Accelerate")
endif()
if(WIN32)
    target_compile_definitions(qpitch_rubberband PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

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
)

if(WIN32)
    target_compile_definitions(QPitch PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

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
    qpitch_rubberband
    juce::juce_audio_utils
    juce::juce_audio_plugin_client
    juce::juce_gui_basics
    clap_juce_extensions
)

target_include_directories(QPitch PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

option(QPITCH_BUILD_TOOLS "Build the offline listening comparison tool" OFF)
if(QPITCH_BUILD_TOOLS AND CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    add_executable(qpitch_ab_listen tools/ab_listen_test.cpp src/dsp/PitchShifter.cpp)
    target_include_directories(qpitch_ab_listen PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(qpitch_ab_listen PRIVATE juce::juce_audio_formats qpitch_rubberband)
    target_compile_definitions(qpitch_ab_listen PRIVATE
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
    )
endif()

include(CTest)
if(BUILD_TESTING AND CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    add_executable(qpitch_shifter_test tools/shifter_test.cpp src/dsp/PitchShifter.cpp)
    target_include_directories(qpitch_shifter_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(qpitch_shifter_test PRIVATE qpitch_rubberband)
    add_test(NAME pitch_shifter COMMAND qpitch_shifter_test)
    add_executable(qpitch_detector_test tools/detector_test.cpp src/dsp/PitchDetector.cpp)
    target_include_directories(qpitch_detector_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME pitch_detector COMMAND qpitch_detector_test)
    add_executable(qpitch_editor_test tools/editor_test.cpp)
    target_include_directories(qpitch_editor_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    # Reuse the plugin's compiled JUCE modules rather than compiling them twice.
    target_include_directories(qpitch_editor_test PRIVATE $<TARGET_PROPERTY:QPitch,INCLUDE_DIRECTORIES>)
    target_compile_definitions(qpitch_editor_test PRIVATE $<TARGET_PROPERTY:QPitch,COMPILE_DEFINITIONS>)
    target_link_libraries(qpitch_editor_test PRIVATE QPitch)
    # GUI integration/render checks are run explicitly with a display available.
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_executable(qpitch_clap_load_test tools/clap_load_test.cpp)
        target_include_directories(qpitch_clap_load_test PRIVATE $<TARGET_PROPERTY:clap_juce_extensions,INTERFACE_INCLUDE_DIRECTORIES>)
        target_link_libraries(qpitch_clap_load_test PRIVATE ${CMAKE_DL_LIBS})
        add_test(NAME clap_load COMMAND qpitch_clap_load_test $<TARGET_FILE:QPitch_CLAP>)
    endif()

endif()
