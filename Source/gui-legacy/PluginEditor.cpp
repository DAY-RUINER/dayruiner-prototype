#include "PluginEditor.h"

//==============================================================================
DayRuinerAudioProcessorEditor::DayRuinerAudioProcessorEditor (DayRuinerAudioProcessor& p)
    : AudioProcessorEditor (p),
      processor (p),
      seqGrid (p.getSequencer(), themeManager)
{
    // --- 17 knobs, one per float/int parameter (IDs must match the processor) ---
    grainTopLenKnob     = makeKnob ("GRAIN_TOP_LENGTH",    "Grain Top Len");
    grainTopFreqKnob    = makeKnob ("GRAIN_TOP_FREQ",      "Grain Top Frq");
    grainBottomLenKnob  = makeKnob ("GRAIN_BOTTOM_LENGTH", "Grain Bot Len");
    grainBottomFreqKnob = makeKnob ("GRAIN_BOTTOM_FREQ",   "Grain Bot Frq");
    blendKnob           = makeKnob ("GRAIN_BLEND",         "Grain Blend");
    decayKnob           = makeKnob ("SYNTH_DECAY",         "Decay");
    satDriveKnob        = makeKnob ("SAT_DRIVE",           "Drive");
    satMixKnob          = makeKnob ("SAT_MIX",             "Sat Mix");
    divNumKnob          = makeKnob ("DELAY_DIV_NUM",       "Div Num");
    divDenKnob          = makeKnob ("DELAY_DIV_DEN",       "Div Den");
    delayFbKnob         = makeKnob ("DELAY_FB",           "Delay FB");
    delayMixKnob        = makeKnob ("DELAY_MIX",           "Delay Mix");
    revRoomKnob         = makeKnob ("REV_ROOM",            "Room");
    revDampKnob         = makeKnob ("REV_DAMP",            "Damp");
    revMixKnob          = makeKnob ("REV_MIX",             "Rev Mix");
    seqLenKnob          = makeKnob ("SEQ_LENGTH",          "Seq Len");
    seqSwingKnob        = makeKnob ("SEQ_SWING",           "Swing");

    // --- Algorithm pickers (choice params; item order matches the processor) ---
    const juce::StringArray synthAlgos { "Analog Kick", "Analog Snare", "Analog Hat",
                                         "FM Kick", "FM Snare", "FM Pluck",
                                         "Wavetable Lead", "Wavetable Pad",
                                         "Additive Bell", "Additive Organ",
                                         "Modal Membrane", "Modal String",
                                         "Phase Distortion", "Vector Morph",
                                         "Granular Pulse", "Glitch Stutter",
                                         "Digital Crush", "Noise Sweep",
                                         "Metal Tine", "Sub Sine" };
    synthAlgoBox.addItemList (synthAlgos, 1);
    synthAlgoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
        (processor.apvts, "SYNTH_ALGO", synthAlgoBox);
    addAndMakeVisible (synthAlgoBox);

    const juce::StringArray satAlgos { "None", "Bitcrush", "Rate Crush", "Wavefold",
                                       "Phase", "Tape", "Tube" };
    satAlgoBox.addItemList (satAlgos, 1);
    satAlgoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
        (processor.apvts, "SAT_ALGO", satAlgoBox);
    addAndMakeVisible (satAlgoBox);

    // --- Bank / pattern (editor-thread request; the audio thread applies it) ---
    for (int b = 1; b <= DayRuiner::PatternManager::NUM_BANKS; ++b)
        bankBox.addItem ("Bank " + juce::String (b), b);
    bankBox.setSelectedId (processor.getPatternManager().getBankIndex() + 1, juce::dontSendNotification);
    bankBox.onChange = [this]
    {
        processor.getPatternManager().requestPattern (bankBox.getSelectedId() - 1,
                                                      patternBox.getSelectedId() - 1);
    };
    addAndMakeVisible (bankBox);

    for (int i = 1; i <= DayRuiner::PatternManager::PATTERNS_PER_BANK; ++i)
        patternBox.addItem ("Pattern " + juce::String (i), i);
    patternBox.setSelectedId (processor.getPatternManager().getPatternIndex() + 1, juce::dontSendNotification);
    patternBox.onChange = [this]
    {
        processor.getPatternManager().requestPattern (bankBox.getSelectedId() - 1,
                                                      patternBox.getSelectedId() - 1);
    };
    addAndMakeVisible (patternBox);

    // --- Theme picker ---
    for (int i = 0; i < themeManager.getNumThemes(); ++i)
        themeBox.addItem (themeManager.getThemeName (i), i + 1);
    themeBox.setSelectedId (themeManager.getCurrentThemeIndex() + 1, juce::dontSendNotification);
    themeBox.onChange = [this]
    {
        themeManager.setTheme (themeBox.getSelectedId() - 1);
        applyTheme();
    };
    addAndMakeVisible (themeBox);

    // --- Live-sampling record button ---
    recordButton.onClick = [this]
    {
        auto& ge = processor.getGranularEngine();
        if (ge.isLiveRecording())
            ge.stopLiveRecording();
        else
            ge.startLiveRecording();
    };
    addAndMakeVisible (recordButton);

    // --- Input monitor toggle ---
    monitorButton.onClick = [this] { processor.setMonitorInput (monitorButton.getToggleState()); };
    addAndMakeVisible (monitorButton);

    // --- Grain XY pad drives the top-layer grain params ---
    grainPanel.onChange = [this] (float x, float y)
    {
        if (auto* param = processor.apvts.getParameter ("GRAIN_TOP_LENGTH"))
            param->setValueNotifyingHost (x);
        if (auto* param = processor.apvts.getParameter ("GRAIN_TOP_FREQ"))
            param->setValueNotifyingHost (y);
    };
    grainPanel.setValues (0.3f, 0.5f); // matches the parameter defaults
    addAndMakeVisible (grainPanel);

    addAndMakeVisible (seqGrid);

    // --- Static labels ---
    bankLabel.setText ("Bank", juce::dontSendNotification);
    patternLabel.setText ("Pattern", juce::dontSendNotification);
    themeLabel.setText ("Theme", juce::dontSendNotification);
    granularTitle.setText ("GRANULAR", juce::dontSendNotification);
    synthTitle.setText ("SYNTH", juce::dontSendNotification);
    saturatorTitle.setText ("SATURATOR", juce::dontSendNotification);
    delayTitle.setText ("DELAY", juce::dontSendNotification);
    reverbTitle.setText ("REVERB", juce::dontSendNotification);
    sequencerTitle.setText ("SEQUENCER", juce::dontSendNotification);
    statusLabel.setJustificationType (juce::Justification::centredRight);

    for (auto* l : { &bankLabel, &patternLabel, &themeLabel,
                     &granularTitle, &synthTitle, &saturatorTitle,
                     &delayTitle, &reverbTitle, &sequencerTitle, &statusLabel })
        addAndMakeVisible (l);

    applyTheme();
    setSize (1280, 760);
    startTimerHz (10);
}

DayRuinerAudioProcessorEditor::~DayRuinerAudioProcessorEditor()
{
    stopTimer();

    // Detach the look-and-feel before it is destroyed.
    for (auto& s : knobSliders)
        s->setLookAndFeel (nullptr);
}

//==============================================================================
DayRuinerAudioProcessorEditor::KnobWidgets
DayRuinerAudioProcessorEditor::makeKnob (const juce::String& paramID, const juce::String& labelText)
{
    auto slider = std::make_unique<juce::Slider>();
    slider->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 20);
    slider->setLookAndFeel (&knobLF);
    addAndMakeVisible (slider.get());

    auto label = std::make_unique<juce::Label>();
    label->setText (labelText, juce::dontSendNotification);
    label->setJustificationType (juce::Justification::centred);
    addAndMakeVisible (label.get());

    auto attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
        (processor.apvts, paramID, *slider);

    KnobWidgets w { slider.get(), label.get() };
    knobSliders.push_back (std::move (slider));
    knobLabels.push_back (std::move (label));
    knobAttachments.push_back (std::move (attachment));
    return w;
}

void DayRuinerAudioProcessorEditor::layoutKnob (KnobWidgets& k, int x, int y, int w)
{
    k.label->setBounds (x, y, w, 18);
    k.slider->setBounds (x, y + 20, w, 108);
}

//==============================================================================
void DayRuinerAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (themeManager.getCurrentTheme().background);
}

void DayRuinerAudioProcessorEditor::resized()
{
    seqGrid.setBounds (10, 10, getWidth() - 20, 300);
    grainPanel.setBounds (10, 330, 200, 200);

    // Row A: granular / synth / saturator
    granularTitle.setBounds (230, 316, 430, 16);
    int x = 230;
    layoutKnob (grainTopLenKnob, x, 336);     x += 88;
    layoutKnob (grainTopFreqKnob, x, 336);    x += 88;
    layoutKnob (grainBottomLenKnob, x, 336);  x += 88;
    layoutKnob (grainBottomFreqKnob, x, 336); x += 88;
    layoutKnob (blendKnob, x, 336);

    synthTitle.setBounds (690, 316, 190, 16);
    layoutKnob (decayKnob, 690, 336);
    synthAlgoBox.setBounds (690, 472, 190, 30);

    saturatorTitle.setBounds (900, 316, 280, 16);
    layoutKnob (satDriveKnob, 900, 336);
    layoutKnob (satMixKnob, 988, 336);
    satAlgoBox.setBounds (900, 472, 190, 30);

    // Row B: delay / reverb / sequencer
    delayTitle.setBounds (230, 524, 350, 16);
    x = 230;
    layoutKnob (delayFbKnob, x, 544);  x += 88;
    layoutKnob (delayMixKnob, x, 544); x += 88;
    layoutKnob (divNumKnob, x, 544);   x += 88;
    layoutKnob (divDenKnob, x, 544);

    reverbTitle.setBounds (610, 524, 270, 16);
    x = 610;
    layoutKnob (revRoomKnob, x, 544); x += 88;
    layoutKnob (revDampKnob, x, 544); x += 88;
    layoutKnob (revMixKnob, x, 544);

    sequencerTitle.setBounds (902, 524, 200, 16);
    layoutKnob (seqLenKnob, 902, 544);
    layoutKnob (seqSwingKnob, 990, 544);

    // Bottom row: bank / pattern / record / monitor / theme / status
    bankLabel.setBounds (10, 688, 100, 16);
    bankBox.setBounds (10, 706, 100, 32);
    patternLabel.setBounds (120, 688, 120, 16);
    patternBox.setBounds (120, 706, 120, 32);
    recordButton.setBounds (260, 706, 110, 32);
    monitorButton.setBounds (380, 706, 150, 32);
    themeLabel.setBounds (550, 688, 150, 16);
    themeBox.setBounds (550, 706, 150, 32);
    statusLabel.setBounds (720, 706, 550, 32);
}

//==============================================================================
void DayRuinerAudioProcessorEditor::applyTheme()
{
    const auto& theme = themeManager.getCurrentTheme();

    knobLF.setColour (juce::Slider::rotarySliderFillColourId,    theme.accent);
    knobLF.setColour (juce::Slider::rotarySliderOutlineColourId, theme.panel.brighter (0.12f));
    knobLF.setColour (juce::Slider::thumbColourId,               theme.text);
    knobLF.setColour (juce::Slider::textBoxTextColourId,         theme.text);
    knobLF.setColour (juce::Slider::textBoxOutlineColourId,      theme.panel);

    grainPanel.setColours (theme.panel, theme.grain);

    for (auto* l : { &bankLabel, &patternLabel, &themeLabel,
                     &granularTitle, &synthTitle, &saturatorTitle,
                     &delayTitle, &reverbTitle, &sequencerTitle, &statusLabel })
        l->setColour (juce::Label::textColourId, theme.text);

    for (auto& l : knobLabels)
        l->setColour (juce::Label::textColourId, theme.text); // knob name labels

    monitorButton.setColour (juce::ToggleButton::textColourId, theme.text);

    repaint();
}

//==============================================================================
void DayRuinerAudioProcessorEditor::timerCallback()
{
    // Keep the grain XY pad in sync with the top-layer parameters.
    if (const auto* pLen = processor.apvts.getRawParameterValue ("GRAIN_TOP_LENGTH"))
        if (const auto* pFreq = processor.apvts.getRawParameterValue ("GRAIN_TOP_FREQ"))
            grainPanel.setValues (pLen->load(), pFreq->load());

    // Status line: bank / pattern / playhead step / record state.
    const auto& pm = processor.getPatternManager();
    const int step = processor.getSequencer().getCurrentStep();
    juce::String text = "Bank " + juce::String (pm.getBankIndex() + 1)
                      + "  |  Pattern " + juce::String (pm.getPatternIndex() + 1)
                      + "  |  Step " + juce::String (step + 1);
    if (processor.isLiveRecording())
        text += "   |   REC";
    statusLabel.setText (text, juce::dontSendNotification);

    // Keep the record button honest if recording stops from anywhere else.
    const bool rec = processor.isLiveRecording();
    if ((recordButton.getButtonText() == "Stop") != rec)
        recordButton.setButtonText (rec ? "Stop" : "Record");

    recordButton.setColour (juce::TextButton::buttonColourId,
                            rec ? juce::Colour (0xff7a1f1f)
                                : themeManager.getCurrentTheme().panel);
}
