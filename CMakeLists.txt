cmake_minimum_required(VERSION 3.22)

project(Drizzle VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Steinberg VST2 SDK (optional). Set to SDK root containing pluginterfaces/vst2.x/aeffect.h
# Download: https://www.steinberg.net/vst2sdk (accept license, then point this path at the extracted folder)
set(DRIZZLE_VST2_SDK_PATH "" CACHE PATH "Steinberg VST2 SDK root directory")

add_subdirectory("C:/dev/JUCE" JUCE)

if(DRIZZLE_VST2_SDK_PATH AND EXISTS "${DRIZZLE_VST2_SDK_PATH}/pluginterfaces/vst2.x/aeffect.h")
    juce_set_vst2_sdk_path("${DRIZZLE_VST2_SDK_PATH}")
    set(DRIZZLE_HAS_VST2 TRUE)
    message(STATUS "Drizzle: VST2 hosting enabled (${DRIZZLE_VST2_SDK_PATH})")
else()
    set(DRIZZLE_HAS_VST2 FALSE)
    message(STATUS "Drizzle: VST2 hosting disabled (set DRIZZLE_VST2_SDK_PATH to Steinberg VST2 SDK)")
endif()

juce_add_gui_app(Drizzle
    PRODUCT_NAME "Drizzle"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/icon.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/icon.png"
)

target_sources(Drizzle
    PRIVATE
        Source/Main.cpp
        Source/MainComponent.cpp
        Source/Vst3HostIdentity.cpp
        Source/AudioEngine.cpp
        Source/DrizzleAudioProcessor.cpp
        Source/PluginChain.cpp
        Source/PluginScanPaths.cpp
        Source/TrackMixerProcessor.cpp
        Source/TrackPluginManager.cpp
        Source/SessionSettings.cpp
        Source/StreamEngine.cpp
        Source/YoutubeChatClient.cpp
        Source/ui/DrizzlePanels.cpp
        Source/ui/TrackPluginPanel.cpp
        Source/ui/SettingsPanels.cpp
)

target_compile_definitions(Drizzle
    PRIVATE
        JUCE_ASIO=1
        DRIZZLE_HAS_VST3_HOST_NAME_HOOK=1
        JUCE_PLUGINHOST_VST3=1
        JUCE_PLUGINHOST_VST=$<BOOL:${DRIZZLE_HAS_VST2}>
        DRIZZLE_HAS_VST2=$<BOOL:${DRIZZLE_HAS_VST2}>
        JUCE_VST3_HOST_CROSS_PLATFORM_UID=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

target_link_libraries(Drizzle
    PRIVATE
        juce::juce_gui_extra
        juce::juce_audio_utils
        juce::juce_audio_processors
)

# XLN / iZotope derive license ComputerId from the host process file name (e.g. reaper.exe).
# Ship a copy named reaper.exe for users who authorized plugins in Reaper.
add_custom_command(TARGET Drizzle POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Drizzle>/LicenseCompat"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:Drizzle>" "$<TARGET_FILE_DIR:Drizzle>/LicenseCompat/reaper.exe"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/LicenseCompat/README.txt"
            "$<TARGET_FILE_DIR:Drizzle>/LicenseCompat/README.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/LicenseCompat/install_to_reaper_folder.bat"
            "$<TARGET_FILE_DIR:Drizzle>/LicenseCompat/install_to_reaper_folder.bat"
    COMMENT "Creating LicenseCompat/reaper.exe for XLN/iZotope license compatibility"
)
