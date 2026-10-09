cmake_minimum_required(VERSION 3.22)

project(DayRuinerEngine VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)

# JUCE location: the prototype's own shallow clone of JUCE 9.0.3.
# (Kept out of distributable zips; CI clones it fresh — see BUILD_WINDOWS.md.)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../_juce93 JUCE)

#==============================================================================
# The plugin target
#==============================================================================
juce_add_plugin(DayRuinerEngine
    PRODUCT_NAME "Day Ruiner"
    COMPANY_NAME "Day Ruiner"
    BUNDLE_ID com.dayruiner.engine
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    FORMATS VST3 Standalone
    VST3_CATEGORIES "Instrument" "Synth"
    PLUGIN_MANUFACTURER_CODE DayR
    PLUGIN_CODE DRu1
)

juce_generate_juce_header(DayRuinerEngine)

target_sources(DayRuinerEngine PRIVATE
    PluginProcessor.cpp
    dsp/SampleBuffer.cpp
    dsp/GrainScheduler.cpp
    dsp/GranularEngine.cpp
    dsp/SynthEngine.cpp
    dsp/Saturator.cpp
    dsp/EuclideanDelay.cpp
    dsp/FFTConvolver.cpp
    sequencer/ParameterLock.cpp
    sequencer/StepSequencer.cpp
    sequencer/PatternManager.cpp
    # DAY RUINER GUI (prototype): the v8 panel. The old team-3 GUI lives in
    # Source/gui-legacy/ and is NOT compiled.
    gui/DayRuinerLookAndFeel.cpp
    gui/LogoWordmark.cpp
    gui/OLEDDisplay.cpp
    gui/ChunkyKnob.cpp
    gui/GrainFieldDisplay.cpp
    gui/TrigGrid.cpp
    gui/TrackRow.cpp
    gui/PageTabs.cpp
    gui/PageLEDs.cpp
    gui/PresetBrowser.cpp
    gui/DayRuinerEditor.cpp
)

# DayRuinerPresets.h (factory banks) for gui/EngineAdapter.h.
target_include_directories(DayRuinerEngine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/presets
)

target_link_libraries(DayRuinerEngine PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_plugin_client
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

target_compile_definitions(DayRuinerEngine PUBLIC
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

#==============================================================================
# Headless DSP smoke test (console app, no GUI).
# Links PluginProcessor + all non-GUI sources. gui/*.cpp are deliberately
# excluded; the DAYRUINER_HEADLESS_SMOKE_TEST guard in PluginProcessor.cpp
# keeps createEditor() from referencing the editor. gui/PrototypeAdapters.h
# IS compiled (processor members) — it only needs EngineAdapter.h, no widgets.
#==============================================================================
juce_add_console_app(DayRuinerSmokeTest)

juce_generate_juce_header(DayRuinerSmokeTest)

target_sources(DayRuinerSmokeTest PRIVATE
    SmokeTest.cpp
    PluginProcessor.cpp
    dsp/SampleBuffer.cpp
    dsp/GrainScheduler.cpp
    dsp/GranularEngine.cpp
    dsp/SynthEngine.cpp
    dsp/Saturator.cpp
    dsp/EuclideanDelay.cpp
    dsp/FFTConvolver.cpp
    sequencer/ParameterLock.cpp
    sequencer/StepSequencer.cpp
    sequencer/PatternManager.cpp
)

target_include_directories(DayRuinerSmokeTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/presets
)

#==============================================================================
# Editor integration test (headless under xvfb on Linux): constructs the real
# DayRuinerEditor against the real processor/APVTS/adapters and snapshots
# every page to /tmp/dayruiner_editor_test/. Dev utility — not wired into CI.
# Run: cmake --build build --target DayRuinerEditorTest && xvfb-run -a \
#        ./build/Source/DayRuinerEditorTest_artefacts/Release/DayRuinerEditorTest
#==============================================================================
juce_add_console_app(DayRuinerEditorTest)

juce_generate_juce_header(DayRuinerEditorTest)

target_sources(DayRuinerEditorTest PRIVATE
    EditorSmokeTest.cpp
    PluginProcessor.cpp
    dsp/SampleBuffer.cpp
    dsp/GrainScheduler.cpp
    dsp/GranularEngine.cpp
    dsp/SynthEngine.cpp
    dsp/Saturator.cpp
    dsp/EuclideanDelay.cpp
    dsp/FFTConvolver.cpp
    sequencer/ParameterLock.cpp
    sequencer/StepSequencer.cpp
    sequencer/PatternManager.cpp
    gui/DayRuinerLookAndFeel.cpp
    gui/LogoWordmark.cpp
    gui/OLEDDisplay.cpp
    gui/ChunkyKnob.cpp
    gui/GrainFieldDisplay.cpp
    gui/TrigGrid.cpp
    gui/TrackRow.cpp
    gui/PageTabs.cpp
    gui/PageLEDs.cpp
    gui/PresetBrowser.cpp
    gui/DayRuinerEditor.cpp
)

target_include_directories(DayRuinerEditorTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/presets
)

target_link_libraries(DayRuinerEditorTest PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
)

target_compile_definitions(DayRuinerEditorTest PRIVATE
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_link_libraries(DayRuinerSmokeTest PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
)

target_compile_definitions(DayRuinerSmokeTest PRIVATE
    DAYRUINER_HEADLESS_SMOKE_TEST=1
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)
