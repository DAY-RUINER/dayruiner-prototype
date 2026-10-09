#include "KnobLookAndFeel.h"

#include <cmath>

KnobLookAndFeel::KnobLookAndFeel()
{
    // Sensible defaults; the editor overrides them from the active theme.
    setColour (juce::Slider::rotarySliderFillColourId,    juce::Colour (0xffff9a3c));
    setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff2a2622));
    setColour (juce::Slider::thumbColourId,               juce::Colours::white);
    setColour (juce::Slider::textBoxTextColourId,         juce::Colours::white);
    setColour (juce::Slider::textBoxOutlineColourId,      juce::Colour (0xff2a2622));
}

void KnobLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPosProportional, float rotaryStartAngle,
                                        float rotaryEndAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (6.0f);
    const float radius  = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float centreX = bounds.getCentreX();
    const float centreY = bounds.getCentreY();
    const float angle   = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    // JUCE rotary angles use 0 = 12 o'clock, clockwise positive; Path arcs use
    // 0 = 3 o'clock, so shift by a quarter turn (same conversion JUCE uses internally).
    const float halfPi   = juce::MathConstants<float>::halfPi;
    const float startArc = rotaryStartAngle - halfPi;
    const float endArc   = rotaryEndAngle - halfPi;
    const float valueArc = angle - halfPi;

    // Body: dark disc.
    g.setColour (slider.findColour (juce::Slider::rotarySliderOutlineColourId));
    g.fillEllipse (centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f);

    const float arcRadius = radius - 5.0f;

    // Track arc (full travel).
    juce::Path track;
    track.addCentredArc (centreX, centreY, arcRadius, arcRadius, 0.0f, startArc, endArc, true);
    g.setColour (slider.findColour (juce::Slider::rotarySliderOutlineColourId).brighter (0.3f));
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    // Value arc (start -> current position).
    if (slider.isEnabled())
    {
        juce::Path value;
        value.addCentredArc (centreX, centreY, arcRadius, arcRadius, 0.0f, startArc, valueArc, true);
        g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId));
        g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
    }

    // Pointer line from the centre towards the current angle.
    const float pointerLen = radius - 11.0f;
    const juce::Point<float> tip (centreX + pointerLen * std::cos (valueArc),
                                  centreY + pointerLen * std::sin (valueArc));
    g.setColour (slider.findColour (juce::Slider::thumbColourId));
    g.drawLine (centreX, centreY, tip.x, tip.y, 2.5f);
}
