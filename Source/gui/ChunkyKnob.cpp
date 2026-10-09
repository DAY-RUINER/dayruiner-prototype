#include "ChunkyKnob.h"
#include "DayRuinerLookAndFeel.h"

ChunkyKnob::ChunkyKnob()
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters(-2.35f, 2.35f, true);
    slider.setRange(0.0, 1.0, 0.001);
    slider.onValueChange = [this] { refreshReadout(); };
    addAndMakeVisible(slider);
}

void ChunkyKnob::bindToParameter(juce::AudioProcessorValueTreeState& state,
                                 const juce::String& paramID_,
                                 const juce::String& shortLabel,
                                 const juce::StringArray& choiceNames_)
{
    placeholder = false;
    labelText = shortLabel;
    choiceNames = choiceNames_;
    apvts = &state;
    paramID = paramID_;

    // Drop the previous attachment FIRST: its slider listener must be gone
    // before the new attachment's initial update, otherwise the stale
    // listener hijacks the slider value mid-bind (and corrupts the old
    // parameter as a side effect).
    attachment.reset();

    if (auto* p = state.getParameter(paramID))
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(p))
            slider.setRange(0.0, static_cast<double>(choice->choices.size() - 1), 1.0);
        else
        {
            const auto nr = p->getNormalisableRange();
            slider.setNormalisableRange(juce::NormalisableRange<double>(
                static_cast<double>(nr.start), static_cast<double>(nr.end),
                static_cast<double>(nr.interval), static_cast<double>(nr.skew),
                nr.symmetricSkew));
        }
    }

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        state, paramID, slider);

    setTooltip(paramID + " — bound to engine parameter");
    refreshReadout();
}

void ChunkyKnob::makePlaceholder(const juce::String& shortLabel, const juce::String& detail)
{
    placeholder = true;
    labelText = shortLabel + " *";
    attachment.reset();
    apvts = nullptr;
    paramID = {};
    choiceNames.clear();
    slider.setRange(0.0, 1.0, 0.001);
    slider.setValue(0.5, juce::dontSendNotification);
    setTooltip("Placeholder — " + detail);
    refreshReadout();
}

void ChunkyKnob::refreshReadout()
{
    if (! choiceNames.isEmpty())
    {
        const int idx = juce::jlimit(0, choiceNames.size() - 1,
                                     static_cast<int>(std::round(slider.getValue())));
        valueText = choiceNames[idx];
    }
    else if (apvts != nullptr)
    {
        if (auto* p = apvts->getParameter(paramID))
        {
            // Trim the default float->text conversion ("0.3000000") to a
            // tidy 3-decimal readout.
            if (auto* fp = dynamic_cast<juce::AudioParameterFloat*>(p))
                valueText = juce::String(fp->get(), 3);
            else if (auto* ip = dynamic_cast<juce::AudioParameterInt*>(p))
                valueText = juce::String(ip->get());
            else
                valueText = p->getCurrentValueAsText();
        }
        else
        {
            valueText = juce::String(slider.getValue(), 3);
        }
    }
    else
    {
        valueText = juce::String(static_cast<int>(std::round(slider.getValue() * 100.0))) + "%";
    }
    repaint();
}

void ChunkyKnob::resized()
{
    const auto b = getLocalBounds();
    slider.setBounds(b.withTrimmedTop(22).withTrimmedBottom(18));
}

void ChunkyKnob::paint(juce::Graphics& g)
{
    const auto b = getLocalBounds();

    // Engraved label above the knob; placeholders render dimmer.
    const juce::Colour labelColour = placeholder ? DayRuinerLookAndFeel::engravedDim
                                                 : DayRuinerLookAndFeel::engraved;
    DayRuinerLookAndFeel::drawEngravedText(g, labelText,
                                           b.withHeight(22),
                                           DayRuinerLookAndFeel::labelFont(13.0f),
                                           juce::Justification::centred, labelColour);

    // Tiny value readout below the knob.
    DayRuinerLookAndFeel::drawEngravedText(g, valueText,
                                           juce::Rectangle<int>(b.getX(), b.getBottom() - 18,
                                                                b.getWidth(), 18),
                                           DayRuinerLookAndFeel::monoFont(10.0f),
                                           juce::Justification::centred,
                                           DayRuinerLookAndFeel::engravedDim);
}
