#pragma once

#include <JuceHeader.h>

// ============================================================================
// OLEDDisplay — amber-on-black readout: preset name, page, BPM.
// Clicking it opens the preset browser.
// ============================================================================
class OLEDDisplay : public juce::Component
{
public:
    OLEDDisplay();

    void setPresetName(const juce::String& name);
    void setPageName(const juce::String& name);
    void setBpm(double bpm);

    // Per-page status line: when set, replaces the "Preset: …" text on the
    // top line (BPM readout stays). Used by the DRUMS page
    // ("TRACK 3 · FM SNARE") and the MIXER page ("TRK 4 LEVEL 0.80",
    // "TRK 4 MUTED"). Clear to go back to the preset name.
    void setLine1Override(const juce::String& line);
    void clearLine1Override();

    std::function<void()> onClick;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    juce::String presetName { "UMBER" };
    juce::String pageName { "GRANULAR" };
    juce::String line1Override; // when non-empty, replaces the preset line
    double bpm = 120.0;

    void drawDotGrid(juce::Graphics& g, juce::Rectangle<float> area) const;
};
