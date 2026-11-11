cmake_minimum_required(VERSION 3.22)
# Pre-1.0 beta — each build appends an incrementing -beta.N via GenerateBuildInfo.
if(APPLE)
    project(MidiToolkit VERSION 0.1.0 LANGUAGES C CXX OBJCXX)
else()
    project(MidiToolkit VERSION 0.1.0 LANGUAGES C CXX)
endif()

set(PF_VERSION "${PROJECT_VERSION}")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/Generated")

# Fresh version stamp (0.1.0-beta.N · 12h time · git hash) on every build for testers.
set(PF_BUILDINFO_CPP "${CMAKE_BINARY_DIR}/Generated/BuildInfo.cpp")
set(PF_BUILDINFO_H   "${CMAKE_BINARY_DIR}/Generated/BuildInfo.h")
add_custom_target(MidiToolkitBuildInfo ALL
    COMMAND "${CMAKE_COMMAND}"
        -DPF_VERSION=${PF_VERSION}
        -DSRC_DIR=${CMAKE_SOURCE_DIR}
        -DOUT_DIR=${CMAKE_BINARY_DIR}/Generated
        -P "${CMAKE_SOURCE_DIR}/cmake/GenerateBuildInfo.cmake"
    BYPRODUCTS "${PF_BUILDINFO_CPP}" "${PF_BUILDINFO_H}"
    COMMENT "Updating BuildInfo stamp"
    VERBATIM)
# Seed so the first configure can resolve the source path before the first build.
# Skip incrementing the build counter on configure — only count real builds.
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -DPF_VERSION=${PF_VERSION}
        -DSRC_DIR=${CMAKE_SOURCE_DIR}
        -DOUT_DIR=${CMAKE_BINARY_DIR}/Generated
        -DPF_SEED_ONLY=1
        -P "${CMAKE_SOURCE_DIR}/cmake/GenerateBuildInfo.cmake")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum macOS version")

if(APPLE)
    # macOS 27 ar writes album archives. ld then refuses them unless the link
    # line passes -target. libtool still writes a classic static archive.
    set(CMAKE_C_ARCHIVE_CREATE "/usr/bin/libtool -static -o <TARGET> <OBJECTS>")
    set(CMAKE_CXX_ARCHIVE_CREATE "/usr/bin/libtool -static -o <TARGET> <OBJECTS>")
    set(CMAKE_C_ARCHIVE_FINISH "")
    set(CMAKE_CXX_ARCHIVE_FINISH "")
    set(_sdk_compat "-DMAC_OS_X_VERSION_MIN_REQUIRED=130000 -DMAC_OS_X_VERSION_MAX_ALLOWED=150400")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${_sdk_compat}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${_sdk_compat}")
    set(CMAKE_OBJC_FLAGS "${CMAKE_OBJC_FLAGS} ${_sdk_compat}")
    set(CMAKE_OBJCXX_FLAGS "${CMAKE_OBJCXX_FLAGS} ${_sdk_compat}")
endif()

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.12
)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
    set(_miditoolkit_formats AU VST3 Standalone)
else()
    set(_miditoolkit_formats VST3 Standalone)
endif()

# Instrument: MIDI output + silent stereo audio. Ableton lists this on its own track.
# IS_MIDI_EFFECT on this target breaks hosts that expect an instrument, so the
# insert that sits in front of a synth is the separate MidiToolkitFX target.
set(MIDITOOLKIT_PRODUCT_NAME "Midi Toolkit")
juce_add_plugin(MidiToolkit
    COMPANY_NAME            "MidiToolkit"
    BUNDLE_ID               com.miditoolkit.app
    PLUGIN_MANUFACTURER_CODE MdbR
    PLUGIN_CODE              MdbB
    FORMATS                  ${_miditoolkit_formats}
    PRODUCT_NAME             "${MIDITOOLKIT_PRODUCT_NAME}"
    IS_SYNTH                 TRUE
    NEEDS_MIDI_INPUT         TRUE
    NEEDS_MIDI_OUTPUT        TRUE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD  TRUE
)

# MIDI FX for Logic (AU aumi). Always passthrough in the editor.
# Ableton rejects true MIDI-effect VST3s ("effect category, no audio input bus"),
# so the Live-facing VST3 is a separate instrument-style target below.
set(MIDITOOLKIT_FX_PRODUCT_NAME "Midi Toolkit FX")
if(APPLE)
    juce_add_plugin(MidiToolkitFX
        COMPANY_NAME            "MidiToolkit"
        BUNDLE_ID               com.miditoolkit.app.fx
        PLUGIN_MANUFACTURER_CODE MdbR
        PLUGIN_CODE              MdbF
        FORMATS                  AU
        PRODUCT_NAME             "${MIDITOOLKIT_FX_PRODUCT_NAME}"
        IS_SYNTH                 FALSE
        NEEDS_MIDI_INPUT         TRUE
        NEEDS_MIDI_OUTPUT        TRUE
        IS_MIDI_EFFECT           TRUE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        COPY_PLUGIN_AFTER_BUILD  FALSE
    )
endif()

# Live VST3: not a synth (so it can sit ahead of an instrument), with stereo
# in and out so Live accepts the bus layout, and MIDI in/out so a keyboard
# passes through. Category stays Instrument rather than Fx.
# The Max device (Resources/Max) also hosts the private copy of this bundle by path.
juce_add_plugin(MidiToolkitFXVST3
    COMPANY_NAME            "MidiToolkit"
    BUNDLE_ID               com.miditoolkit.app.fx.vst3
    PLUGIN_MANUFACTURER_CODE MdbR
    PLUGIN_CODE              MdbF
    FORMATS                  VST3
    PRODUCT_NAME             "${MIDITOOLKIT_FX_PRODUCT_NAME}"
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         TRUE
    NEEDS_MIDI_OUTPUT        TRUE
    IS_MIDI_EFFECT           FALSE
    VST3_CATEGORIES          Instrument
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD  FALSE
)

# Always install into the user plug-in folders so DAWs see the latest build.
# (JUCE's COPY_PLUGIN_AFTER_BUILD can be left OFF in an old CMake cache.)
# Quote each -D so paths with spaces (e.g. "Midi Toolkit.vst3") stay intact.
# MidiToolkit_InstallPlugins is part of the default build (ALL) and of ./build.sh.
# It runs even when the bundles are already up to date, so the plugins folder
# is replaced at the end of every build.
if(APPLE)
    set(_fx_private_vst3 "$ENV{HOME}/Library/Application Support/Midi Toolkit/${MIDITOOLKIT_FX_PRODUCT_NAME}.vst3")
    foreach(_fmt IN ITEMS VST3 AU)
        if(TARGET MidiToolkit_${_fmt})
            add_custom_command(TARGET MidiToolkit_${_fmt} POST_BUILD
                COMMAND "${CMAKE_COMMAND}"
                    "-DPLUGIN_SRC=$<TARGET_BUNDLE_DIR:MidiToolkit_${_fmt}>"
                    "-DPLUGIN_KIND=${_fmt}"
                    "-DPRODUCT_NAME=${MIDITOOLKIT_PRODUCT_NAME}"
                    -P "${CMAKE_SOURCE_DIR}/cmake/InstallPlugins.cmake"
                COMMENT "Installing ${_fmt} -> ~/Library/Audio/Plug-Ins"
                VERBATIM)
        endif()
    endforeach()
    if(TARGET MidiToolkitFX_AU)
        add_custom_command(TARGET MidiToolkitFX_AU POST_BUILD
            COMMAND "${CMAKE_COMMAND}"
                "-DPLUGIN_SRC=$<TARGET_BUNDLE_DIR:MidiToolkitFX_AU>"
                "-DPLUGIN_KIND=AU"
                "-DPRODUCT_NAME=${MIDITOOLKIT_FX_PRODUCT_NAME}"
                -P "${CMAKE_SOURCE_DIR}/cmake/InstallPlugins.cmake"
            COMMENT "Installing FX AU -> ~/Library/Audio/Plug-Ins/Components"
            VERBATIM)
    endif()
    if(TARGET MidiToolkitFXVST3_VST3)
        # Scanned VST3 folder for Live's browser, plus the private copy the Max
        # device loads, plus the legacy build-tree path Live already scans.
        # Re-sign after copy: moduleinfo / ditto otherwise leave a broken ad-hoc
        # seal and Live silently skips the bundle.
        add_custom_command(TARGET MidiToolkitFXVST3_VST3 POST_BUILD
            COMMAND "${CMAKE_COMMAND}"
                "-DPLUGIN_SRC=$<TARGET_BUNDLE_DIR:MidiToolkitFXVST3_VST3>"
                "-DPLUGIN_KIND=VST3"
                "-DPRODUCT_NAME=${MIDITOOLKIT_FX_PRODUCT_NAME}"
                -P "${CMAKE_SOURCE_DIR}/cmake/InstallPlugins.cmake"
            COMMAND "${CMAKE_COMMAND}"
                "-DPLUGIN_SRC=$<TARGET_BUNDLE_DIR:MidiToolkitFXVST3_VST3>"
                "-DPLUGIN_KIND=VST3"
                "-DPRODUCT_NAME=${MIDITOOLKIT_FX_PRODUCT_NAME}"
                "-DPLUGIN_DEST=${_fx_private_vst3}"
                -P "${CMAKE_SOURCE_DIR}/cmake/InstallPlugins.cmake"
            COMMAND "${CMAKE_COMMAND}" -E make_directory
                "${CMAKE_BINARY_DIR}/MidiToolkitFX_artefacts/Release/VST3"
            COMMAND "${CMAKE_COMMAND}" -E rm -rf
                "${CMAKE_BINARY_DIR}/MidiToolkitFX_artefacts/Release/VST3/${MIDITOOLKIT_FX_PRODUCT_NAME}.vst3"
            COMMAND ditto
                "$<TARGET_BUNDLE_DIR:MidiToolkitFXVST3_VST3>"
                "${CMAKE_BINARY_DIR}/MidiToolkitFX_artefacts/Release/VST3/${MIDITOOLKIT_FX_PRODUCT_NAME}.vst3"
            COMMAND codesign --force --deep --sign -
                "${CMAKE_BINARY_DIR}/MidiToolkitFX_artefacts/Release/VST3/${MIDITOOLKIT_FX_PRODUCT_NAME}.vst3"
            COMMENT "Installing FX VST3 -> ~/Library/Audio/Plug-Ins/VST3 + private + build-tree copies (re-signed)"
            VERBATIM)
    endif()
    if(TARGET MidiToolkit_VST3 AND TARGET MidiToolkit_AU)
        add_custom_target(MidiToolkit_InstallPlugins ALL
            COMMAND "${CMAKE_COMMAND}"
                "-DPLUGIN_SRC=$<TARGET_BUNDLE_DIR:MidiToolkit_VST3>"
                "-DPLUGIN_KIND=VST3"
                "-DPRODUCT_NAME=${MIDITOOLKIT_PRODUCT_NAME}"
                -P "${CMAKE_SOURCE_DIR}/cmake/InstallPlugins.cmake"
            COMMAND "${CMAKE_COMMAND}"
                "-DPLUGIN_SRC=$<TARGET_BUNDLE_DIR:MidiToolkit_AU>"
                "-DPLUGIN_KIND=AU"
                "-DPRODUCT_NAME=${MIDITOOLKIT_PRODUCT_NAME}"
                -P "${CMAKE_SOURCE_DIR}/cmake/InstallPlugins.cmake"
            COMMAND "${CMAKE_COMMAND}"
                "-DPLUGIN_SRC=$<TARGET_BUNDLE_DIR:MidiToolkitFXVST3_VST3>"
                "-DPLUGIN_KIND=VST3"
                "-DPRODUCT_NAME=${MIDITOOLKIT_FX_PRODUCT_NAME}"
                -P "${CMAKE_SOURCE_DIR}/cmake/InstallPlugins.cmake"
            COMMAND "${CMAKE_COMMAND}"
                "-DPLUGIN_SRC=$<TARGET_BUNDLE_DIR:MidiToolkitFXVST3_VST3>"
                "-DPLUGIN_KIND=VST3"
                "-DPRODUCT_NAME=${MIDITOOLKIT_FX_PRODUCT_NAME}"
                "-DPLUGIN_DEST=${_fx_private_vst3}"
                -P "${CMAKE_SOURCE_DIR}/cmake/InstallPlugins.cmake"
            COMMAND "${CMAKE_COMMAND}"
                "-DPLUGIN_SRC=$<TARGET_BUNDLE_DIR:MidiToolkitFX_AU>"
                "-DPLUGIN_KIND=AU"
                "-DPRODUCT_NAME=${MIDITOOLKIT_FX_PRODUCT_NAME}"
                -P "${CMAKE_SOURCE_DIR}/cmake/InstallPlugins.cmake"
            DEPENDS MidiToolkit_VST3 MidiToolkit_AU MidiToolkitFXVST3_VST3 MidiToolkitFX_AU
            COMMENT "Replacing instrument and MIDI FX in ~/Library/Audio/Plug-Ins"
            VERBATIM)
    endif()
elseif(WIN32)
    if(TARGET MidiToolkit_VST3)
        add_custom_command(TARGET MidiToolkit_VST3 POST_BUILD
            COMMAND "${CMAKE_COMMAND}"
                "-DPLUGIN_SRC=$<TARGET_FILE_DIR:MidiToolkit_VST3>/../.."
                "-DPLUGIN_KIND=VST3"
                "-DPRODUCT_NAME=${MIDITOOLKIT_PRODUCT_NAME}"
                -P "${CMAKE_SOURCE_DIR}/cmake/InstallPlugins.cmake"
            COMMENT "Installing VST3 into Common Files"
            VERBATIM)
    endif()
    if(TARGET MidiToolkitFXVST3_VST3)
        add_custom_command(TARGET MidiToolkitFXVST3_VST3 POST_BUILD
            COMMAND "${CMAKE_COMMAND}"
                "-DPLUGIN_SRC=$<TARGET_FILE_DIR:MidiToolkitFXVST3_VST3>/../.."
                "-DPLUGIN_KIND=VST3"
                "-DPRODUCT_NAME=${MIDITOOLKIT_FX_PRODUCT_NAME}"
                -P "${CMAKE_SOURCE_DIR}/cmake/InstallPlugins.cmake"
            COMMENT "Installing Midi Toolkit FX VST3 into Common Files"
            VERBATIM)
    endif()
endif()

juce_add_binary_data(MidiToolkitAssets
    SOURCES
        Resources/Fonts/Archivo-Regular.ttf
        Resources/Fonts/Archivo-Medium.ttf
        Resources/Fonts/Archivo-SemiBold.ttf
        Resources/Fonts/Archivo-Bold.ttf
        Resources/Fonts/IBMPlexMono-Regular.ttf
        Resources/Fonts/IBMPlexMono-Medium.ttf
        Resources/Fonts/Inter-Regular.ttf
        Resources/Fonts/Inter-Medium.ttf
        Resources/Fonts/Inter-SemiBold.ttf
        Resources/Fonts/Inter-Bold.ttf
        Resources/Fonts/JetBrainsMono-Regular.ttf
        Resources/Fonts/JetBrainsMono-Medium.ttf
)

set(MIDITOOLKIT_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/MidiFileData.cpp
    Source/EditModel.cpp
    Source/GrooveEngine.cpp
    Source/GeneratorEngines.cpp
    Source/LiveMidiFx.cpp
    Source/UiAtoms.cpp
    Source/TransportBar.cpp
    Source/BrowserPanels.cpp
    Source/KnobPanel.cpp
    Source/EffectsInspector.cpp
    Source/PianoRollEditor.cpp
    Source/Theme.cpp
    Source/DesignSystem.cpp
    Source/LibraryStore.cpp
    Source/FolderIndex.cpp
    "${PF_BUILDINFO_CPP}"
)

set(_miditoolkit_plugin_targets MidiToolkit MidiToolkitFXVST3)
if(TARGET MidiToolkitFX)
    list(APPEND _miditoolkit_plugin_targets MidiToolkitFX)
endif()
foreach(_plug IN LISTS _miditoolkit_plugin_targets)
    target_include_directories(${_plug} PRIVATE
        "${CMAKE_BINARY_DIR}/Generated"
        "${CMAKE_SOURCE_DIR}/Source")
    target_sources(${_plug} PRIVATE ${MIDITOOLKIT_SOURCES})
    if(APPLE)
        target_sources(${_plug} PRIVATE Source/NativeWindowChrome.mm)
    endif()
    add_dependencies(${_plug} MidiToolkitBuildInfo)

    target_compile_definitions(${_plug} PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_ALSA=0
    )

    target_link_libraries(${_plug}
        PRIVATE
            MidiToolkitAssets
            juce::juce_audio_utils
            juce::juce_audio_processors
            juce::juce_gui_extra
            juce::juce_data_structures
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags
    )
endforeach()
if(TARGET MidiToolkitFX)
    target_compile_definitions(MidiToolkitFX PUBLIC MIDITOOLKIT_MIDI_FX=1)
endif()
target_compile_definitions(MidiToolkitFXVST3 PUBLIC MIDITOOLKIT_MIDI_FX=1 MIDITOOLKIT_FX_LIVE_VST3=1)

option(MIDITOOLKIT_BUILD_TESTS "Build unit tests" ON)
option(MIDITOOLKIT_BUILD_PROBE "Build the VST3 MIDI-out probe tool" ON)

if(MIDITOOLKIT_BUILD_PROBE)
    juce_add_console_app(MidiOutProbe PRODUCT_NAME "MidiOutProbe")
    target_sources(MidiOutProbe PRIVATE Tools/MidiOutProbe.cpp)
    target_compile_definitions(MidiOutProbe PRIVATE
        JUCE_PLUGINHOST_VST3=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
    )
    target_link_libraries(MidiOutProbe PRIVATE
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
    )
endif()

if(MIDITOOLKIT_BUILD_TESTS)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.5.4
    )
    FetchContent_MakeAvailable(Catch2)

    add_executable(MidiToolkitTests
        Tests/main.cpp
        Tests/MidiFileDataTests.cpp
        Tests/TrimTests.cpp
        Tests/PluginProcessorTests.cpp
        Tests/EditModelTests.cpp
        Tests/GrooveEngineTests.cpp
        Tests/EditorUiTests.cpp
        Tests/LibraryStoreTests.cpp
        Tests/LiveMidiFxTests.cpp
        Tests/FolderIndexTests.cpp
        Tests/ScanEfficiencyTests.cpp
        Tests/SnapshotTests.cpp
    )
    target_link_libraries(MidiToolkitTests PRIVATE
        MidiToolkit
        Catch2::Catch2WithMain
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_extra
        juce::juce_data_structures
    )
    target_include_directories(MidiToolkitTests PRIVATE ${CMAKE_SOURCE_DIR}/Source "${CMAKE_BINARY_DIR}/Generated")
    include(CTest)
    enable_testing()
    add_test(NAME MidiToolkitTests COMMAND MidiToolkitTests)
endif()
