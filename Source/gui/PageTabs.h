#pragma once

#include <JuceHeader.h>

// ============================================================================
// PageTabs — the full-width bottom tab bar: DRUMS / SYNTH / GRANULAR / FX / MIXER.
// Spread edge to edge, never crammed into a corner. The selected tab gets an
// amber underline glow; the tab also decides what the 8 encoders control.
// ============================================================================
class PageTabs : public juce::Component
{
public:
    enum class Page { Drums = 0, Synth, Granular, Fx, Mixer };

    static constexpr int kNumPages = 5;

    PageTabs();

    void setSelectedPage(Page p);
    Page getSelectedPage() const { return selectedPage; }
    static juce::String pageName(Page p);

    std::function<void(Page)> onPageSelected;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    juce::Rectangle<float> tabBounds(int index) const;
    int tabAt(juce::Point<float> p) const;

    Page selectedPage = Page::Granular;
    float tabW = 200.0f, gap = 8.0f;
};
