#include "DayRuinerEditor.h"

// ---- encoder binding tables ------------------------------------------------
// label / APVTS id / placeholder? / placeholder note / choice names (if any)

struct EncoderBinding
{
    const char* label;
    const char* paramID;      // nullptr => placeholder
    const char* note;         // placeholder detail (ignored when bound)
    const char* const* choices;
};

static const char* const kSatAlgoChoices[] =
    { "None", "Bitcrush", "Rate Crush", "Wavefold", "Phase", "Tape", "Tube", nullptr };

static const char* const kSynthAlgoChoices[] =
    { "Analog Kick", "Analog Snare", "Analog Hat", "FM Kick", "FM Snare", "FM Pluck",
      "Wavetable Lead", "Wavetable Pad", "Additive Bell", "Additive Organ",
      "Modal Membrane", "Modal String", "Phase Distortion", "Vector Morph",
      "Granular Pulse", "Glitch Stutter", "Digital Crush", "Noise Sweep",
      "Metal Tine", "Sub Sine", nullptr };

// The 14 drum algorithms (TRKn_ALGO). Mirrors the demo layout and the
// engine contract in EngineAdapter.h.
static const char* const kDrumAlgoChoices[] =
    { "Analog Kick", "Analog Snare", "Analog Hat", "FM Kick", "FM Snare",
      "Clap", "Rimshot", "Tom", "Crash", "Modal Membrane", "Granular Pulse",
      "Glitch Stutter", "Digital Crush", "Noise Sweep", nullptr };

static const char* const kChokeChoices[] =
    { "OFF", "A", "B", "C", "D", nullptr };

static const char* const kDriveModeChoices[] =
    { "SoftClip", "HardClip", "Saturate", "Distort", nullptr };

static juce::StringArray toStringArray(const char* const* c)
{
    juce::StringArray out;
    for (int i = 0; c[i] != nullptr; ++i)
        out.add(c[i]);
    return out;
}

// FX page: 8 performance macros, ALL real parameters, zero placeholders.
// Deeper params (ratios, attacks, damping, diffusion…) stay preset-accessible.
// Note macro 7/8 use the existing REV_MIX / REV_ROOM IDs (REV_ROOM drives the
// FDN reverb's decaySecs) — see README "FX macro mapping".
static const EncoderBinding kFxBindings[8] = {
    { "SAT TYPE", "SAT_ALGO",       nullptr, kSatAlgoChoices },
    { "SAT DRV",  "SAT_DRIVE",      nullptr, nullptr },
    { "DRV MODE", "DRIVE_MODE",     nullptr, kDriveModeChoices },
    { "DRIVE",    "DRIVE_AMOUNT",   nullptr, nullptr },
    { "COMP",     "COMP_THRESHOLD", nullptr, nullptr },
    { "DLY MIX",  "DELAY_MIX",      nullptr, nullptr },
    { "VERB MIX", "REV_MIX",        nullptr, nullptr },
    { "VERB DCY", "REV_ROOM",       nullptr, nullptr },
};

// GRANULAR page: the 5 real grain parameters + 3 marked placeholders.
static const EncoderBinding kGranularBindings[8] = {
    { "TOP SIZE", "GRAIN_TOP_LENGTH",    nullptr, nullptr },
    { "TOP DENS", "GRAIN_TOP_FREQ",      nullptr, nullptr },
    { "BOT SIZE", "GRAIN_BOTTOM_LENGTH", nullptr, nullptr },
    { "BOT DENS", "GRAIN_BOTTOM_FREQ",   nullptr, nullptr },
    { "BLEND",    "GRAIN_BLEND",         nullptr, nullptr },
    { "ATTACK",   nullptr, "Grain attack — engine exposes no APVTS parameter yet", nullptr },
    { "POS",      nullptr, "Playhead position — engine exposes no APVTS parameter yet", nullptr },
    { "SPRAY",    nullptr, "Position randomisation — engine exposes no APVTS parameter yet", nullptr },
};

// SYNTH page: the 2 real synth parameters + 6 marked placeholders.
static const EncoderBinding kSynthBindings[8] = {
    { "ALGO",  "SYNTH_ALGO",  nullptr, kSynthAlgoChoices },
    { "DECAY", "SYNTH_DECAY", nullptr, nullptr },
    { "CUTOFF", nullptr, "Filter cutoff — engine exposes no APVTS parameter yet", nullptr },
    { "RESO",   nullptr, "Filter resonance — engine exposes no APVTS parameter yet", nullptr },
    { "ENV",    nullptr, "Amp envelope — engine exposes no APVTS parameter yet", nullptr },
    { "DETUNE", nullptr, "Voice detune — engine exposes no APVTS parameter yet", nullptr },
    { "LFO",    nullptr, "LFO depth — engine exposes no APVTS parameter yet", nullptr },
    { "WIDTH",  nullptr, "Stereo width — engine exposes no APVTS parameter yet", nullptr },
};

// DRUMS page: track-centric. The 8 encoders edit the SELECTED track's
// TRKn_* parameters — bound dynamically by bindDrumEncoders() (the param
// IDs depend on selectedTrack, so there is no static table here).
// Order: ALGO (14-way stepped), TUNE, DECAY, LEVEL, PAN, HUMANIZE,
// CHOKE (OFF/A/B/C/D stepped), DRIVE. All real, zero placeholders.

// MIXER page: 8 encoders = TRK1_LEVEL..TRK8_LEVEL, all live, zero
// placeholders. Track buttons toggle TRKn_MUTE (see TrackRow::Mode::Mute).
static const EncoderBinding kMixerBindings[8] = {
    { "T1", "TRK1_LEVEL", nullptr, nullptr },
    { "T2", "TRK2_LEVEL", nullptr, nullptr },
    { "T3", "TRK3_LEVEL", nullptr, nullptr },
    { "T4", "TRK4_LEVEL", nullptr, nullptr },
    { "T5", "TRK5_LEVEL", nullptr, nullptr },
    { "T6", "TRK6_LEVEL", nullptr, nullptr },
    { "T7", "TRK7_LEVEL", nullptr, nullptr },
    { "T8", "TRK8_LEVEL", nullptr, nullptr },
};

static const EncoderBinding* bindingsForPage(PageTabs::Page p)
{
    switch (p)
    {
        case PageTabs::Page::Drums:    return nullptr; // dynamic — see bindDrumEncoders()
        case PageTabs::Page::Synth:    return kSynthBindings;
        case PageTabs::Page::Granular: return kGranularBindings;
        case PageTabs::Page::Fx:       return kFxBindings;
        case PageTabs::Page::Mixer:    return kMixerBindings;
    }
    return kGranularBindings;
}

// ---- editor ----------------------------------------------------------------

DayRuinerEditor::DayRuinerEditor(juce::AudioProcessor& proc,
                                 juce::AudioProcessorValueTreeState& apvts_,
                                 ISequencerView& seq,
                                 IGrainFieldSource& grainSource,
                                 IPresetStore& presets)
    : juce::AudioProcessorEditor(proc),
      apvts(apvts_),
      sequencer(seq),
      presetStore(presets),
      grainField(grainSource),
      presetBrowser(presets)
{
    setLookAndFeel(&lookAndFeel);
    setSize(kWidth, kHeight);

    addAndMakeVisible(logo);
    addAndMakeVisible(oled);
    addAndMakeVisible(masterKnob);
    for (auto& k : encoders)
        addAndMakeVisible(k);
    addAndMakeVisible(grainField);
    addAndMakeVisible(pageLeds);
    addAndMakeVisible(trigGrid);
    addAndMakeVisible(trackRow);
    addAndMakeVisible(pageTabs);
    addChildComponent(presetBrowser); // overlay, hidden until opened

    masterKnob.makePlaceholder("MASTER", "Master trim — engine exposes no APVTS parameter yet");

    oled.onClick = [this] { presetBrowser.showBrowser(); };

    // GRAINFIELD: click opens the sample chooser, drops load directly.
    grainField.onLoadRequested = [this] { openSampleChooser(); };
    grainField.onFilesDropped = [this](const juce::StringArray& files)
    {
        if (files.size() > 0)
            loadSampleFile(juce::File(files[0]));
    };

    presetBrowser.onPresetChosen = [this](int idx)
    {
        presetStore.selectPreset(idx);
        oled.setPresetName(presetStore.getPreset(idx).name);
        presetBrowser.hideBrowser();
    };

    pageTabs.onPageSelected = [this](PageTabs::Page p) { selectPage(p); };

    pageLeds.onPageClicked = [this](int page)
    {
        trigPage = page;
        pageLeds.setActivePage(page);
        refreshTrigGrid();
    };

    trackRow.onTrackSelected = [this](int t)
    {
        if (currentPage == PageTabs::Page::Mixer)
        {
            // MIXER page: a tap toggles the track's mute. Amber lit = audible.
            const juce::String id = "TRK" + juce::String(t + 1) + "_MUTE";
            bool nowMuted = false;
            if (auto* p = apvts.getParameter(id))
            {
                nowMuted = ! (p->getValue() > 0.5f);
                p->setValueNotifyingHost(nowMuted ? 1.0f : 0.0f);
            }
            mixerLastTouch = "TRK " + juce::String(t + 1)
                           + (nowMuted ? " MUTED" : " AUDIBLE");
            updateOledForPage();
            refreshTrackRow();
        }
        else
        {
            // Every other page: a tap selects the track for editing.
            selectedTrack = t;
            trackRow.setSelectedTrack(t);
            if (currentPage == PageTabs::Page::Drums)
                bindDrumEncoders();
            updateOledForPage();
            refreshTrigGrid();
        }
    };

    trigGrid.onStepClicked = [this](int i)
    {
        const int step = trigPage * 16 + i;
        if (step < sequencer.getPatternLength())
        {
            const StepView sv = sequencer.getStep(selectedTrack, step);
            sequencer.setStepActive(selectedTrack, step, ! sv.active);
            refreshTrigGrid();
        }
    };

    trigGrid.onStepFxToggled = [this](int i)
    {
        const int step = trigPage * 16 + i;
        if (step < sequencer.getPatternLength())
        {
            const StepView sv = sequencer.getStep(selectedTrack, step);
            sequencer.setStepFx(selectedTrack, step, ! sv.fx);
            refreshTrigGrid();
        }
    };

    applyPage(currentPage);
    refreshFromEngine();
    startTimerHz(30);
}

DayRuinerEditor::~DayRuinerEditor()
{
    setLookAndFeel(nullptr);
}

void DayRuinerEditor::selectPage(PageTabs::Page p)
{
    applyPage(p);
}

void DayRuinerEditor::applyPage(PageTabs::Page p)
{
    currentPage = p;
    pageTabs.setSelectedPage(p);
    oled.setPageName(PageTabs::pageName(p));

    if (p == PageTabs::Page::Mixer)
    {
        // Snapshot levels so the first timer poll doesn't report a
        // phantom "last touch" before the user moves anything.
        for (int t = 0; t < 8; ++t)
            if (auto* q = apvts.getParameter("TRK" + juce::String(t + 1) + "_LEVEL"))
                mixerLevelCache[t] = q->getValue();
    }

    rebindEncoders();
    refreshTrackRow();
    updateOledForPage();
}

void DayRuinerEditor::rebindEncoders()
{
    if (currentPage == PageTabs::Page::Drums)
    {
        bindDrumEncoders();
        return;
    }

    const EncoderBinding* table = bindingsForPage(currentPage);
    for (int i = 0; i < 8; ++i)
    {
        if (table[i].paramID != nullptr)
            encoders[i].bindToParameter(apvts, table[i].paramID, table[i].label,
                                        table[i].choices != nullptr
                                            ? toStringArray(table[i].choices)
                                            : juce::StringArray());
        else
            encoders[i].makePlaceholder(table[i].label, table[i].note);
    }
}

// DRUMS page: the 8 encoders edit the SELECTED track. Param IDs are built
// from the track number: TRK1_ALGO … TRK8_DRIVE (see EngineAdapter.h).
void DayRuinerEditor::bindDrumEncoders()
{
    static const char* const labels[8] =
        { "ALGO", "TUNE", "DECAY", "LEVEL", "PAN", "HUMAN", "CHOKE", "DRIVE" };
    static const char* const suffixes[8] =
        { "ALGO", "TUNE", "DECAY", "LEVEL", "PAN", "HUMANIZE", "CHOKE", "DRIVE" };

    const juce::String prefix = "TRK" + juce::String(selectedTrack + 1) + "_";

    for (int i = 0; i < 8; ++i)
    {
        juce::StringArray choices;
        if (i == 0)      choices = toStringArray(kDrumAlgoChoices);
        else if (i == 6) choices = toStringArray(kChokeChoices);
        encoders[i].bindToParameter(apvts, prefix + suffixes[i], labels[i], choices);
    }
}

juce::String DayRuinerEditor::drumAlgoNameForTrack(int track) const
{
    const juce::String id = "TRK" + juce::String(track + 1) + "_ALGO";
    if (auto* p = apvts.getParameter(id))
    {
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*>(p))
        {
            const int idx = juce::jlimit(0, c->choices.size() - 1, c->getIndex());
            return c->choices[idx];
        }
    }
    return {};
}

void DayRuinerEditor::refreshTrackRow()
{
    if (currentPage == PageTabs::Page::Mixer)
    {
        trackRow.setMode(TrackRow::Mode::Mute);
        for (int t = 0; t < TrackRow::kNumTracks; ++t)
        {
            bool muted = false;
            if (auto* p = apvts.getParameter("TRK" + juce::String(t + 1) + "_MUTE"))
                muted = p->getValue() > 0.5f;
            trackRow.setTrackMuted(t, muted);
        }
    }
    else
    {
        trackRow.setMode(TrackRow::Mode::Select);
        trackRow.setSelectedTrack(selectedTrack);
    }
}

void DayRuinerEditor::updateOledForPage()
{
    if (currentPage == PageTabs::Page::Drums)
    {
        oled.setLine1Override("TRACK " + juce::String(selectedTrack + 1)
                              + " - " + drumAlgoNameForTrack(selectedTrack));
    }
    else if (currentPage == PageTabs::Page::Mixer)
    {
        if (mixerLastTouch.isNotEmpty())
            oled.setLine1Override(mixerLastTouch);
        else
            oled.setLine1Override("MIXER - TAP = MUTE");
    }
    else
    {
        oled.clearLine1Override();
    }
}

void DayRuinerEditor::refreshTrigGrid()
{
    const int patternLen = sequencer.getPatternLength();
    for (int i = 0; i < TrigGrid::kNumKeys; ++i)
    {
        const int step = trigPage * 16 + i;
        if (step < patternLen && selectedTrack < sequencer.getNumTracks())
        {
            const StepView sv = sequencer.getStep(selectedTrack, step);
            trigGrid.setStep(i, sv.active, sv.fx);
        }
        else
        {
            trigGrid.setStep(i, false, false);
        }
    }
}

void DayRuinerEditor::refreshFromEngine()
{
    refreshTrigGrid();
    refreshTrackRow();
    updateOledForPage();
    pageLeds.setActivePage(trigPage);
    oled.setPresetName(presetStore.getPreset(presetStore.getCurrentIndex()).name);

    // Pull the playhead too, so the initial/static state shows it.
    const int step = sequencer.getCurrentStep();
    const int local = step - trigPage * 16;
    trigGrid.setPlayStep((local >= 0 && local < TrigGrid::kNumKeys) ? local : -1);
}

void DayRuinerEditor::timerCallback()
{
    // Playhead highlight follows the audio-thread step counter.
    const int step = sequencer.getCurrentStep();
    const int local = step - trigPage * 16;
    trigGrid.setPlayStep((local >= 0 && local < TrigGrid::kNumKeys) ? local : -1);

    if (currentPage == PageTabs::Page::Mixer)
    {
        // Live level changes (knobs or host automation) surface on the OLED.
        for (int t = 0; t < 8; ++t)
        {
            float v = 0.0f;
            if (auto* p = apvts.getParameter("TRK" + juce::String(t + 1) + "_LEVEL"))
                v = p->getValue();
            if (std::abs(v - mixerLevelCache[t]) > 0.0005f)
            {
                mixerLevelCache[t] = v;
                mixerLastTouch = "TRK " + juce::String(t + 1)
                               + " LEVEL " + juce::String(v, 2);
                updateOledForPage();
            }
        }
        refreshTrackRow(); // mutes may change via host automation
    }
    else if (currentPage == PageTabs::Page::Drums)
    {
        // The ALGO knob can rename the track under us — keep the OLED fresh.
        updateOledForPage();
    }
}

// ---- sample loading --------------------------------------------------------
// The canonical user location for granular source material. Created on
// demand; the file chooser opens rooted here.
juce::File DayRuinerEditor::getUserSamplesFolder()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                   .getChildFile("Day Ruiner")
                   .getChildFile("Samples");
    dir.createDirectory();
    return dir;
}

void DayRuinerEditor::openSampleChooser()
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();

    sampleChooser = std::make_unique<juce::FileChooser>(
        "Load granular source sample",
        getUserSamplesFolder(),
        fm.getWildcardForAllFormats());

    auto browserFlags = juce::FileBrowserComponent::openMode
                    | juce::FileBrowserComponent::canSelectFiles;

    sampleChooser->launchAsync(browserFlags, [this](const juce::FileChooser& fc)
    {
        const juce::File f = fc.getResult();
        if (f.existsAsFile())
            loadSampleFile(f);
        sampleChooser.reset();
    });
}

void DayRuinerEditor::loadSampleFile(const juce::File& f)
{
    if (sampleLoader)
    {
        const bool ok = sampleLoader(f);
        // The GRAINFIELD picks up the new sample on its next refresh via
        // IGrainFieldSource::hasSample(); surface failures on the OLED.
        if (! ok)
            oled.setPresetName("SAMPLE LOAD FAILED");
    }
}

void DayRuinerEditor::rebuildBackground()
{    background = juce::Image(juce::Image::RGB, kWidth, kHeight, false);
    juce::Graphics g(background);

    DayRuinerLookAndFeel::paintBrushedMetal(g, { 0, 0, kWidth, kHeight });

    // Divider under the top strip.
    g.setColour(DayRuinerLookAndFeel::panelEdge);
    g.drawHorizontalLine(112, 0.0f, static_cast<float>(kWidth));

    // Corner screws — bolted hardware.
    DayRuinerLookAndFeel::paintScrew(g, { 26.0f, 26.0f });
    DayRuinerLookAndFeel::paintScrew(g, { kWidth - 26.0f, 26.0f });
    DayRuinerLookAndFeel::paintScrew(g, { 26.0f, kHeight - 26.0f });
    DayRuinerLookAndFeel::paintScrew(g, { kWidth - 26.0f, kHeight - 26.0f });
}

void DayRuinerEditor::resized()
{
    rebuildBackground();

    logo.setBounds(30, 24, 340, 64);
    oled.setBounds(470, 24, 340, 64);
    masterKnob.setBounds(1128, 8, 96, 104);

    const int encY = 120, encH = 122, encW = 128;
    const int spanX0 = 36, spanX1 = kWidth - 36;
    for (int i = 0; i < 8; ++i)
    {
        const int x = spanX0 + (spanX1 - spanX0 - encW) * i / 7;
        encoders[i].setBounds(x, encY, encW, encH);
    }

    grainField.setBounds(36, 262, kWidth - 72, 178);
    pageLeds.setBounds(36, 446, kWidth - 72, 34);
    trigGrid.setBounds(36, 486, kWidth - 72, 72);
    trackRow.setBounds(36, 566, kWidth - 72, 66);
    pageTabs.setBounds(36, 640, kWidth - 72, 62);

    presetBrowser.setBounds(getLocalBounds());
}

void DayRuinerEditor::paint(juce::Graphics& g)
{
    g.drawImageAt(background, 0, 0);
}
