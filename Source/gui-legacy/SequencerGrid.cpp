#include "SequencerGrid.h"

SequencerGrid::SequencerGrid (DayRuiner::StepSequencer& seq, ThemeManager& tm)
    : sequencer (seq), themes (tm)
{
    startTimerHz (30);
}

juce::String SequencerGrid::conditionLabel (DayRuiner::TrigCondition type)
{
    switch (type)
    {
        case DayRuiner::TrigCondition::Always:      return {};
        case DayRuiner::TrigCondition::Probability: return "P";
        case DayRuiner::TrigCondition::AB:          return "A:B";
        case DayRuiner::TrigCondition::FirstOnly:   return "1st";
        case DayRuiner::TrigCondition::NotFirst:    return "!1";
        case DayRuiner::TrigCondition::Pre:         return "Pre";
        case DayRuiner::TrigCondition::Nei:         return "Nei";
        case DayRuiner::TrigCondition::Fill:        return "F";
        case DayRuiner::TrigCondition::NotFill:     return "!F";
    }

    return {};
}

void SequencerGrid::paint (juce::Graphics& g)
{
    const auto& theme = themes.getCurrentTheme();
    g.fillAll (theme.background);

    constexpr int numRows = DayRuiner::StepSequencer::NUM_TRACKS;
    constexpr int numCols = DayRuiner::StepSequencer::NUM_STEPS;

    const float cellW = (float) getWidth() / (float) numCols;
    const float cellH = (float) getHeight() / (float) numRows;
    const int currentStep = sequencer.getCurrentStep();
    const int patternLength = sequencer.getPatternLength();

    for (int t = 0; t < numRows; ++t)
    {
        for (int s = 0; s < numCols; ++s)
        {
            const auto& step = sequencer.getStep (t, s);

            juce::Colour cell;
            if (s == currentStep)
                cell = theme.sequencerActive;
            else if (step.active)
                cell = theme.sequencerActive.withAlpha (0.6f);
            else
                cell = theme.sequencerInactive;

            // Steps past the pattern length are drawn dimmed (small addition,
            // not in the sketch: the grid always shows all 64 columns).
            if (s >= patternLength)
                cell = cell.withAlpha (cell.getAlpha() * 0.3f);

            g.setColour (cell);
            g.fillRect (s * cellW + 1.0f, t * cellH + 1.0f, cellW - 2.0f, cellH - 2.0f);

            const auto label = conditionLabel (step.condition.type);
            if (label.isNotEmpty())
            {
                g.setColour (theme.text.withAlpha (0.85f));
                g.setFont (juce::Font (juce::FontOptions (9.0f)));
                g.drawText (label, juce::Rectangle<float> (s * cellW, t * cellH, cellW, cellH),
                            juce::Justification::centred);
            }
        }
    }

    // Group separators every 4 steps.
    g.setColour (theme.text.withAlpha (0.18f));
    for (int s = 0; s <= numCols; s += 4)
    {
        const float x = s * cellW;
        g.drawLine (x, 0.0f, x, (float) getHeight(), 1.0f);
    }
}

void SequencerGrid::mouseDown (const juce::MouseEvent& e)
{
    constexpr int numRows = DayRuiner::StepSequencer::NUM_TRACKS;
    constexpr int numCols = DayRuiner::StepSequencer::NUM_STEPS;

    const float cellW = (float) getWidth() / (float) numCols;
    const float cellH = (float) getHeight() / (float) numRows;
    const int track = juce::jlimit (0, numRows - 1, (int) (e.y / cellH));
    const int step  = juce::jlimit (0, numCols - 1, (int) (e.x / cellW));

    if (e.mods.isRightButtonDown() || e.mods.isPopupMenu())
    {
        juce::PopupMenu menu;
        menu.addItem (1, "Condition: Always");
        menu.addItem (2, "Probability 50%");
        menu.addItem (3, "A:B 1:2");
        menu.addItem (4, "A:B 1:4");
        menu.addItem (5, "Fill only");
        menu.addItem (6, "Not in Fill");

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                            [this, track, step] (int result)
        {
            if (result <= 0)
                return;

            auto& st = sequencer.getStep (track, step);
            using TC = DayRuiner::TrigCondition;

            switch (result)
            {
                case 1: st.condition.type = TC::Always; break;
                case 2: st.condition.type = TC::Probability; st.condition.probability = 0.5f; break;
                case 3: st.condition.type = TC::AB; st.condition.aValue = 1; st.condition.bValue = 2; break;
                case 4: st.condition.type = TC::AB; st.condition.aValue = 1; st.condition.bValue = 4; break;
                case 5: st.condition.type = TC::Fill; break;
                case 6: st.condition.type = TC::NotFill; break;
                default: break;
            }

            repaint();
        });
    }
    else
    {
        auto& st = sequencer.getStep (track, step);
        st.active = ! st.active;
        repaint();
    }
}

void SequencerGrid::timerCallback()
{
    const int cs = sequencer.getCurrentStep();
    if (cs != lastStep)
    {
        lastStep = cs;
        repaint();
    }
}
