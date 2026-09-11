cmake_minimum_required(VERSION 3.22)
project(System1Bridge VERSION 0.7.1 LANGUAGES C CXX)
if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "SYSTEM-1 Audio Bridge requires Windows x64.")
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_compile_options(/utf-8)

set(SYSTEM1_JUCE_COMMIT 72782788ce18c2d4d760b28e0921d6ffc6431102)
set(JUCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/JUCE" CACHE PATH "Optional JUCE checkout at the pinned commit")
if(EXISTS "${JUCE_PATH}/CMakeLists.txt")
    add_subdirectory("${JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE")
else()
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG ${SYSTEM1_JUCE_COMMIT})
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_console_app(System1Capture PRODUCT_NAME "System1Capture")
target_sources(System1Capture PRIVATE Source/Helper.cpp)
target_compile_definitions(System1Capture PRIVATE JUCE_ASIO=0 JUCE_WASAPI_LOGGING=1 JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(System1Capture PRIVATE juce::juce_audio_devices juce::juce_recommended_config_flags)

juce_add_plugin(System1Bridge
    COMPANY_NAME "Local Audio Tools"
    BUNDLE_ID "local.audiotools.system1bridge"
    PLUGIN_MANUFACTURER_CODE Laot PLUGIN_CODE S1ab
    FORMATS VST3 PRODUCT_NAME "SYSTEM-1 Audio Bridge"
    IS_SYNTH TRUE NEEDS_MIDI_INPUT TRUE NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE VST3_CATEGORIES Instrument Tools)
target_sources(System1Bridge PRIVATE Source/Plugin.cpp)
target_compile_definitions(System1Bridge PUBLIC JUCE_ASIO=0 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_USE_WINRT_MIDI=0 JUCE_USE_WINDOWS_MIDI_SERVICES=0)
target_link_libraries(System1Bridge PRIVATE juce::juce_audio_processors juce::juce_audio_devices juce::juce_gui_basics juce::juce_recommended_config_flags)
add_dependencies(System1Bridge_VST3 System1Capture)
# Helper must sit beside the .vst3 bundle, not inside it.
add_custom_command(TARGET System1Bridge_VST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:System1Capture>" "$<TARGET_FILE_DIR:System1Bridge_VST3>/../../../System1Capture.exe")

include(CTest)
if(BUILD_TESTING)
    foreach(name ReaderTest ClockTest ScopeTest)
        add_executable(${name} Source/${name}.cpp)
        add_test(NAME ${name} COMMAND ${name})
    endforeach()
    foreach(name System1ControlTest System1EditorCheck)
        juce_add_console_app(${name} PRODUCT_NAME ${name})
        target_include_directories(${name} PRIVATE "${JUCE_MODULES_DIR}")
        target_compile_definitions(${name} PRIVATE JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1)
        target_link_libraries(${name} PRIVATE System1Bridge)
    endforeach()
    target_sources(System1ControlTest PRIVATE Source/ControlTest.cpp)
    target_sources(System1EditorCheck PRIVATE Source/EditorCheck.cpp)
    add_test(NAME MidiControlStateTest COMMAND System1ControlTest)
    add_test(NAME EditorCheck COMMAND System1EditorCheck)
    juce_add_console_app(System1LibraryTest PRODUCT_NAME "System1LibraryTest")
    target_sources(System1LibraryTest PRIVATE Source/LibraryTest.cpp)
    target_compile_definitions(System1LibraryTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(System1LibraryTest PRIVATE juce::juce_audio_processors juce::juce_recommended_config_flags)
    add_test(NAME PresetLibraryTest COMMAND System1LibraryTest)
endif()
