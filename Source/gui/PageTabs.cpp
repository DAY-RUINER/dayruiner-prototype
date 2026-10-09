#include "PageTabs.h"
#include "DayRuinerLookAndFeel.h"

PageTabs::PageTabs() {}

juce::String PageTabs::pageName(Page p)
{
    switch (p)
    {
        case Page::Drums:    return "DRUMS";
        case Page::Synth:    return "SYNTH";
        case Page::Granular: return "GRANULAR";
        case Page::Fx:       return "FX";
        case Page::Mixer:    return "MIXER";
    }
    return {};
}

void PageTabs::setSelectedPage(Page p)
{
    selectedPage = p;
    repaint();
}

void PageTabs::resized()
{
    tabW = (static_cast<float>(getWidth()) - static_cast<float>(kNumPages - 1) * gap)
           / static_cast<float>(kNumPages);
}

juce::Rectangle<float> PageTabs::tabBounds(int index) const
{
    return juce::Rectangle<float>(static_cast<float>(index) * (tabW + gap), 0.0f,
                                  tabW, static_cast<float>(getHeight()));
}

int PageTabs::tabAt(juce::Point<float> p) const
{
    for (int i = 0; i < kNumPages; ++i)
        if (tabBounds(i).contains(p))
            return i;
    return -1;
}

void PageTabs::mouseDown(const juce::MouseEvent& e)
{
    const int idx = tabAt(e.position);
    if (idx >= 0 && onPageSelected)
        onPageSelected(static_cast<Page>(idx));
}

void PageTabs::paint(juce::Graphics& g)
{
    using LAF = DayRuinerLookAndFeel;

    for (int i = 0; i < kNumPages; ++i)
    {
        const auto tb = tabBounds(i);
        const bool selected = (static_cast<Page>(i) == selectedPage);

        // Tab slab.
        g.setColour(selected ? juce::Colour(0xff2a2a2a) : juce::Colour(0xff1c1c1c));
        g.fillRoundedRectangle(tb, 8.0f);
        // Square off the bottom edge so tabs sit on the panel like hardware.
        g.fillRect(tb.withTrimmedTop(tb.getHeight() - 8.0f));

        g.setColour(juce::Colour(0xff000000).withAlpha(0.6f));
        g.drawRoundedRectangle(tb, 8.0f, 1.0f);

        LAF::drawEngravedText(g, pageName(static_cast<Page>(i)),
                              tb.toNearestInt(), LAF::labelFont(15.0f),
                              juce::Justification::centred,
                              selected ? LAF::amber : LAF::engravedDim);

        // Amber underline glow on the active tab.
        if (selected)
        {
            const float uy = tb.getBottom() - 3.0f;
            LAF::paintAmberGlow(g, { tb.getCentreX(), uy }, tb.getWidth() * 0.4f, 0.7f);
            g.setColour(LAF::amber);
            g.fillRect(tb.withTrimmedTop(tb.getHeight() - 3.0f));
        }
    }
}
