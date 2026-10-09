#pragma once

#include <JuceHeader.h>

// ============================================================================
// ChunkyKnob — one of the 8 hardware encoders.
//
// Binds 1:1 to a real APVTS parameter (slider attachment), or acts as a
// clearly-marked placeholder when the engine has no parameter for it yet.
// Placeholders are draggable and show a value, but are NEVER presented as
// real parameters: the label carries a "*" and the tooltip says so.
// ============================================================================
class ChunkyKnob : public juce::Component, public juce::SettableTooltipClient
{
public:
    ChunkyKnob();

    void bindToParameter(juce::AudioProcessorValueTreeState& apvts,
                         const juce::String& paramID,
                         const juce::String& shortLabel,
                         const juce::StringArray& choiceNames = {});
    void makePlaceholder(const juce::String& shortLabel, const juce::String& detail);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void refreshReadout();

    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    juce::String labelText;
    juce::String valueText;
    juce::StringArray choiceNames;
    juce::AudioProcessorValueTreeState* apvts = nullptr;
    juce::String paramID;
    bool placeholder = true;
};
