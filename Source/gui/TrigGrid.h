#pragma once

#include <JuceHeader.h>

// ============================================================================
// TrigGrid — the 16 backlit step keys.
//
// Uniform warm amber (the multi-colour idea was dropped as needless
// complexity). Steps with FX engaged BLINK on/off to show FX kicking in and
// out. Left-click toggles the step, right-click toggles its FX flag.
// The playhead step gets a thin bright outline.
// ============================================================================
class TrigGrid : public juce::Component, private juce::Timer
{
public:
    static constexpr int kNumKeys = 16;

    TrigGrid();

    void setStep(int index, bool active, bool fx);
    void setPlayStep(int index); // -1 = none

    std::function<void(int index)> onStepClicked;    // toggle active
    std::function<void(int index)> onStepFxToggled;  // toggle FX flag

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    void timerCallback() override;

    juce::Rectangle<float> keyBounds(int index) const;
    int keyAt(juce::Point<float> p) const;

    bool stepActive[kNumKeys] = {};
    bool stepFx[kNumKeys] = {};
    int playStep = -1;
    bool blinkOn = true;

    float keyW = 68.0f, keyH = 68.0f, gap = 6.0f;
};
