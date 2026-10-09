#pragma once

#include <JuceHeader.h>

// ============================================================================
// LogoWordmark — the DAY RUINER wordmark.
//
// Monospace terminal/code-style typeface (computer-language aesthetic),
// etched into the metal, with thin sigil-like wire traces trailing off each
// letter — minimal and randomly placed (seeded RNG, so the sigil is stable
// across repaints).
// ============================================================================
class LogoWordmark : public juce::Component
{
public:
    LogoWordmark();
    void paint(juce::Graphics& g) override;

private:
    void drawSigilWires(juce::Graphics& g, float textX, float baselineY, float charW);
};
