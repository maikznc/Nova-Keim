cmake_minimum_required(VERSION 3.22)

project(KEIM1 VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Läuft auf macOS 11 und neuer, auf Intel und Apple Silicon
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimale macOS-Version")

option(KEIM_COPY_AFTER_BUILD "Plugins nach dem Bauen in ~/Library/Audio/Plug-Ins kopieren" OFF)
option(KEIM_BUILD_TOOLS "Test- und Render-Werkzeug bauen" ON)
option(KEIM_LINUX_HEADLESS "Nur für Testbuilds auf Linux-Servern ohne Audio- und X11-Pakete" OFF)

# JUCE wird beim ersten Konfigurieren automatisch von GitHub geladen
set(KEIM_JUCE_TAG "8.0.16-security" CACHE STRING "JUCE-Version (Git-Tag)")
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        ${KEIM_JUCE_TAG}
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
    set(KEIM_FORMATS AU VST3 Standalone)
else()
    set(KEIM_FORMATS VST3 Standalone)
endif()

juce_add_plugin(KEIM1
    PRODUCT_NAME              "KEIM-1"
    COMPANY_NAME              "NOVA"
    BUNDLE_ID                 "com.nova.keim1"
    PLUGIN_MANUFACTURER_CODE  Nova
    PLUGIN_CODE               Kei1
    FORMATS                   ${KEIM_FORMATS}
    IS_SYNTH                  TRUE
    NEEDS_MIDI_INPUT          TRUE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD   ${KEIM_COPY_AFTER_BUILD}
    AU_MAIN_TYPE              kAudioUnitType_MusicDevice
    VST3_CATEGORIES           Instrument Synth
    VERSION                   ${PROJECT_VERSION})

target_sources(KEIM1 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(KEIM1 PRIVATE Source)

target_compile_definitions(KEIM1 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if(KEIM_LINUX_HEADLESS)
    target_compile_definitions(KEIM1 PUBLIC
        JUCE_ALSA=0 JUCE_JACK=0 JUCE_USE_XCURSOR=0 JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XRENDER=0)
endif()

target_link_libraries(KEIM1
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Rendert alle Sounds als WAV und prüft sie (ohne JUCE, läuft auch auf dem Daisy-Rechner)
if(KEIM_BUILD_TOOLS)
    add_executable(keim_render tools/keim_render.cpp)
    target_compile_features(keim_render PRIVATE cxx_std_17)
    enable_testing()
    add_test(NAME klangtest COMMAND keim_render ${CMAKE_BINARY_DIR}/render)
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/render)
endif()
