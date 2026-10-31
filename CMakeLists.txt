# OptiBob — plugin per KVR. Repository autonomo: JUCE si scarica da solo,
# gli header del motore stanno in Engine/ (copiati dagli originali con
# sincronizza.command, restano identici a quelli di OptiBob Live).
cmake_minimum_required(VERSION 3.22)
project(OptiBob VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "" FORCE)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "" FORCE)
endif()

# JUCE scaricato in automatico: nessuna copia locale da avere.
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(OptiBob
    COMPANY_NAME            "OptiBob"
    BUNDLE_ID               "com.optibob.plugin"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Obob
    PLUGIN_CODE             ObPl
    FORMATS                 AU VST3 Standalone
    PRODUCT_NAME            "OptiBob")


target_sources(OptiBob PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(OptiBob PRIVATE Source Engine)

target_compile_definitions(OptiBob PUBLIC
    _USE_MATH_DEFINES=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(OptiBob PRIVATE
    juce::juce_audio_utils
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

# --- collaudo del motore (non fa parte del prodotto) -----------------------
add_executable(prova-motore prova-motore.cpp)
target_include_directories(prova-motore PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(prova-motore PRIVATE _USE_MATH_DEFINES=1)
set_target_properties(prova-motore PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
