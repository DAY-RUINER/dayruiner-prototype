#pragma once

#include <JuceHeader.h>

// ============================================================================
// PageLEDs — the 1:4 2:4 3:4 4:4 sequencer page indicators above the trig keys.
// The 64-step pattern is edited 16 steps at a time; the lit LED is the page
// currently shown on the trig grid.
// ============================================================================
class PageLEDs : public juce::Component
{
public:
    static constexpr int kNumPages = 4;

    PageLEDs();

    void setActivePage(int page); // 0..3
    std::function<void(int page)> onPageClicked;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    int pageAt(juce::Point<float> p) const;

    int activePage = 0;
    float cellW = 64.0f;
};
