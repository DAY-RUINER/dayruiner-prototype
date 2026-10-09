#pragma once

#include <JuceHeader.h>

#include "ThemeManager.h"
#include "../sequencer/StepSequencer.h"

// 8 rows x 64 columns step grid. Left-click toggles a step; right-click opens a
// popup menu to set the step's trigger condition. Repaints (at 30 Hz) only when
// the playing step changes.
class SequencerGrid : public juce::Component,
                      private juce::Timer
{
public:
    SequencerGrid (DayRuiner::StepSequencer& seq, ThemeManager& tm);

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;

private:
    void timerCallback() override;

    static juce::String conditionLabel (DayRuiner::TrigCondition type);

    DayRuiner::StepSequencer& sequencer;
    ThemeManager& themes;
    int lastStep = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SequencerGrid)
};
