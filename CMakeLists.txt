cmake_minimum_required(VERSION 3.22)

# macOS: one universal binary for Apple Silicon and Intel, running on macOS 10.13+
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
if (APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
endif()

project(Changes VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: use a local checkout when given (JUCE_DIR), otherwise download JUCE 7.0.12
set(JUCE_DIR "" CACHE PATH "Optional local JUCE checkout")
if (JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
    add_subdirectory(${JUCE_DIR} juce)
elseif (EXISTS "/home/claude/deps/JUCE/CMakeLists.txt")
    add_subdirectory("/home/claude/deps/JUCE" juce)
else()
    include(FetchContent)
    FetchContent_Declare(juce GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 7.0.12 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(juce)
endif()

if (APPLE)
    set(CHANGES_FORMATS AU VST3 Standalone)
else()
    set(CHANGES_FORMATS VST3 Standalone)
endif()

juce_add_binary_data(ChangesData SOURCES
    Resources/Poppins-Regular.ttf
    Resources/Poppins-Medium.ttf
    Resources/Poppins-Bold.ttf)

set(CHANGES_SOURCES
    Source/Theory.cpp Source/Palettes.cpp Source/SynthEngine.cpp
    Source/PluginProcessor.cpp Source/PluginEditor.cpp
    Source/LookAndFeel.cpp Source/Components.cpp)

set(CHANGES_DEFS
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_REPORT_APP_USAGE=0 JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_APPLICATION_NAME_STRING="Changes" JUCE_APPLICATION_VERSION_STRING="1.0.0")

juce_add_plugin(Changes
    COMPANY_NAME "JazzyT Audio"
    COMPANY_COPYRIGHT "JazzyT Audio"
    BUNDLE_ID com.jazzytaudio.changes
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    PLUGIN_MANUFACTURER_CODE Jzty
    PLUGIN_CODE Chg1
    FORMATS ${CHANGES_FORMATS}
    PRODUCT_NAME "Changes"
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    AU_SANDBOX_SAFE TRUE
    MICROPHONE_PERMISSION_ENABLED FALSE
    HARDENED_RUNTIME_ENABLED FALSE
    DESCRIPTION "Jazz harmony instrument"
    VST3_CATEGORIES Instrument Synth Generator
    VST3_AUTO_MANIFEST FALSE
    ICON_BIG Resources/icon.png
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(Changes PRIVATE ${CHANGES_SOURCES})
target_compile_definitions(Changes PUBLIC ${CHANGES_DEFS})
target_link_libraries(Changes
    PRIVATE ChangesData juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

if (MINGW)
    target_link_options(Changes PUBLIC -static)
    target_compile_options(Changes PUBLIC -Wno-unused-command-line-argument)
endif()

# ---- Test harness (audio render + GUI snapshot + theory unit tests) ----------
option(CHANGES_TESTS "Build the test harness" OFF)
if (CHANGES_TESTS)
    juce_add_console_app(TheoryTests PRODUCT_NAME "TheoryTests")
    target_sources(TheoryTests PRIVATE Tests/TheoryTests.cpp Source/Theory.cpp Source/Palettes.cpp)
    target_compile_definitions(TheoryTests PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
    target_link_libraries(TheoryTests PRIVATE juce::juce_core juce::juce_recommended_config_flags)

    juce_add_console_app(ChangesTests PRODUCT_NAME "ChangesTests")
    target_sources(ChangesTests PRIVATE ${CHANGES_SOURCES} Tests/RenderTest.cpp)
    target_compile_definitions(ChangesTests PRIVATE ${CHANGES_DEFS} JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_ALSA=0 JUCE_JACK=0 JucePlugin_Build_Standalone=0 CHANGES_TEST_BUILD=1)
    target_link_libraries(ChangesTests PRIVATE ChangesData juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)

    if (NOT APPLE)
    juce_add_console_app(SetupSnapshot PRODUCT_NAME "SetupSnapshot")
    target_sources(SetupSnapshot PRIVATE Installer/SetupMain.cpp Source/LookAndFeel.cpp)
    target_compile_definitions(SetupSnapshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 CHANGES_SETUP_SNAPSHOT=1)
    target_link_libraries(SetupSnapshot PRIVATE ChangesData juce::juce_gui_basics juce::juce_recommended_config_flags)
    endif()
endif()

# ---- Windows installer (setup stub; the payload is appended by Packaging/make_installer.py)
if (WIN32)
    juce_add_gui_app(ChangesSetup
        PRODUCT_NAME "ChangesSetup"
        COMPANY_NAME "JazzyT Audio"
        VERSION 1.0.0
        ICON_BIG Resources/icon.png)
    target_sources(ChangesSetup PRIVATE Installer/SetupMain.cpp Source/LookAndFeel.cpp Installer/setup.rc)
    target_compile_definitions(ChangesSetup PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
        JUCE_APPLICATION_NAME_STRING="Changes Setup" JUCE_APPLICATION_VERSION_STRING="1.0.0")
    target_link_libraries(ChangesSetup PRIVATE ChangesData juce::juce_gui_basics juce::juce_recommended_config_flags)
    target_link_options(ChangesSetup PRIVATE -static -mwindows)
endif()
