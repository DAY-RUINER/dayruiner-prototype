#pragma once

#include <JuceHeader.h>
#include "DayRuinerLookAndFeel.h"
#include "EngineAdapter.h"
#include "LogoWordmark.h"
#include "OLEDDisplay.h"
#include "ChunkyKnob.h"
#include "GrainFieldDisplay.h"
#include "TrigGrid.h"
#include "TrackRow.h"
#include "PageTabs.h"
#include "PageLEDs.h"
#include "PresetBrowser.h"

// ============================================================================
// DayRuinerEditor — the full DAY RUINER front panel.
//
// Layout follows the v8 mockup, top to bottom:
//   wordmark | OLED (click = preset browser) | master volume
//   8 chunky encoders (rebound per page tab)
//   GRAINFIELD waveform display
//   1:4-4:4 page LEDs
//   16 trig keys (amber; FX steps blink)
//   8 track buttons 1-8
//   full-width tab bar: DRUMS / SYNTH / GRANULAR / FX / MIXER
//
// The 8 encoders rebind per page:
//   FX       — 8 performance macros, all real APVTS params (saturator,
//              drive, compressor glue, delay mix, reverb mix/decay).
//   GRANULAR — the 5 real grain parameters + 3 marked placeholders.
//   SYNTH    — the 2 real synth parameters + 6 marked placeholders.
//   DRUMS    — track-centric editor: the encoders edit the SELECTED track's
//              TRKn_* parameters (ALGO/TUNE/DECAY/LEVEL/PAN/HUMANIZE/
//              CHOKE/DRIVE), all real. Track buttons select the track.
//   MIXER    — 8 encoders = TRK1_LEVEL..TRK8_LEVEL (live). Track buttons
//              toggle per-track MUTE (amber lit = audible, dim = muted),
//              unlike every other page where a tap SELECTS the track.
//
// Integration: construct with the real AudioProcessor, its APVTS, and
// adapters implementing ISequencerView / IGrainFieldSource / IPresetStore
// (see EngineAdapter.h for the mapping notes, including the drum & mixer
// parameter contract).
// ============================================================================
class DayRuinerEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    static constexpr int kWidth = 1280;
    static constexpr int kHeight = 800;

    DayRuinerEditor(juce::AudioProcessor& proc,
                    juce::AudioProcessorValueTreeState& apvts,
                    ISequencerView& sequencer,
                    IGrainFieldSource& grainSource,
                    IPresetStore& presetStore);
    ~DayRuinerEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Pull sequencer + preset state into the widgets (also called on a timer).
    void refreshFromEngine();

    // Switches page programmatically (used by the demo's --page flag and by
    // the tab bar alike).
    void selectPage(PageTabs::Page p);

    // Opens the preset browser overlay (also opened by clicking the OLED).
    void showPresetBrowser() { presetBrowser.showBrowser(); }

    // Sample loading: the processor installs a loader that suspends the audio
    // thread around GranularEngine::loadTopSample. Called with the chosen file.
    // Returns true on success.
    void setSampleLoader(std::function<bool(const juce::File&)> loader)
    { sampleLoader = std::move(loader); }

private:
    void timerCallback() override;
    void applyPage(PageTabs::Page p);
    void rebindEncoders();
    void bindDrumEncoders();   // encoders <- selected track's TRKn_* params
    void refreshTrigGrid();
    void refreshTrackRow();    // select highlight or mute states, per page
    void updateOledForPage();  // per-page OLED status line
    juce::String drumAlgoNameForTrack(int track) const; // 0-based
    void rebuildBackground();
    void openSampleChooser(); // file dialog rooted at the user Samples folder
    void loadSampleFile(const juce::File& f);
    static juce::File getUserSamplesFolder();

    juce::AudioProcessorValueTreeState& apvts;
    ISequencerView& sequencer;
    IPresetStore& presetStore;

    DayRuinerLookAndFeel lookAndFeel;

    LogoWordmark logo;
    OLEDDisplay oled;
    ChunkyKnob masterKnob;
    ChunkyKnob encoders[8];
    GrainFieldDisplay grainField;
    PageLEDs pageLeds;
    TrigGrid trigGrid;
    TrackRow trackRow;
    PageTabs pageTabs;
    PresetBrowser presetBrowser;

    PageTabs::Page currentPage = PageTabs::Page::Granular;
    int selectedTrack = 0;
    int trigPage = 0; // 0..3 -> steps 0-15, 16-31, 32-47, 48-63

    // MIXER page: last touched control, shown on the OLED, e.g.
    // "TRK 4 LEVEL 0.80" or "TRK 4 MUTED".
    juce::String mixerLastTouch;
    float mixerLevelCache[8] = { -1.0f, -1.0f, -1.0f, -1.0f,
                                 -1.0f, -1.0f, -1.0f, -1.0f };

    // Installed by the processor; loads a sample into the granular engine.
    std::function<bool(const juce::File&)> sampleLoader;
    std::unique_ptr<juce::FileChooser> sampleChooser; // kept alive during dialog

    juce::Image background;
};
