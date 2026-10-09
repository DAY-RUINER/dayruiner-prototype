#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "gui/ThemeManager.h"
#include "gui/KnobLookAndFeel.h"
#include "gui/SequencerGrid.h"
#include "gui/GrainPanel.h"

// Functional-but-plain editor (GUI design is paused): sequencer grid on top,
// grain XY pad + parameter knobs below, transport/utilities along the bottom.
class DayRuinerAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit DayRuinerAudioProcessorEditor (DayRuinerAudioProcessor&);
    ~DayRuinerAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct KnobWidgets
    {
        juce::Slider* slider = nullptr;
        juce::Label* label = nullptr;
    };

    KnobWidgets makeKnob (const juce::String& paramID, const juce::String& labelText);
    void layoutKnob (KnobWidgets& k, int x, int y, int w = 86);
    void timerCallback() override;
    void applyTheme();

    DayRuinerAudioProcessor& processor;

    ThemeManager themeManager;
    KnobLookAndFeel knobLF;
    SequencerGrid seqGrid;
    GrainPanel grainPanel;

    // Knob ownership; the named handles below are used for layout.
    std::vector<std::unique_ptr<juce::Slider>> knobSliders;
    std::vector<std::unique_ptr<juce::Label>> knobLabels;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> knobAttachments;

    KnobWidgets grainTopLenKnob, grainTopFreqKnob, grainBottomLenKnob, grainBottomFreqKnob, blendKnob;
    KnobWidgets decayKnob;
    KnobWidgets satDriveKnob, satMixKnob;
    KnobWidgets divNumKnob, divDenKnob, delayFbKnob, delayMixKnob;
    KnobWidgets revRoomKnob, revDampKnob, revMixKnob;
    KnobWidgets seqLenKnob, seqSwingKnob;

    juce::ComboBox synthAlgoBox, satAlgoBox, bankBox, patternBox, themeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> synthAlgoAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> satAlgoAttachment;

    juce::Label bankLabel, patternLabel, themeLabel;
    juce::Label granularTitle, synthTitle, saturatorTitle, delayTitle, reverbTitle, sequencerTitle;

    juce::TextButton recordButton { "Record" };
    juce::ToggleButton monitorButton { "Monitor Input" };
    juce::Label statusLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DayRuinerAudioProcessorEditor)
};
