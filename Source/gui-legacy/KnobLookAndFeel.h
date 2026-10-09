#pragma once

#include <JuceHeader.h>

// Simple rotary-knob look and feel for the paused-GUI phase: dark disc, accent
// value arc, pointer line. Colours come from the slider's findColour() so the
// editor can re-theme knobs without touching this class (dependency-free).
class KnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    KnobLookAndFeel();

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider& slider) override;
};
