#pragma once

#include <JuceHeader.h>

#include <functional>

// XY pad for the granular top layer: X = grain length, Y = grain frequency.
// Dragging fires onChange(x01, y01); the editor routes that into the
// GRAIN_TOP_LENGTH / GRAIN_TOP_FREQ parameters.
class GrainPanel : public juce::Component
{
public:
    GrainPanel();

    std::function<void (float x01, float y01)> onChange;

    void setValues (float x01, float y01);
    void setColours (juce::Colour background, juce::Colour crosshair);

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;

private:
    void updateFromMouse (const juce::MouseEvent& e);

    float xVal = 0.3f;
    float yVal = 0.5f;
    juce::Colour bgColour { 0xff1e1b17 };
    juce::Colour lineColour { 0xff7fd4c1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrainPanel)
};
