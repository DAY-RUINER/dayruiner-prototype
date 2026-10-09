#include "PageLEDs.h"
#include "DayRuinerLookAndFeel.h"

PageLEDs::PageLEDs() {}

void PageLEDs::setActivePage(int page)
{
    activePage = juce::jlimit(0, kNumPages - 1, page);
    repaint();
}

int PageLEDs::pageAt(juce::Point<float> p) const
{
    const float totalW = cellW * static_cast<float>(kNumPages);
    const float x0 = (static_cast<float>(getWidth()) - totalW) * 0.5f;
    for (int i = 0; i < kNumPages; ++i)
        if (juce::Rectangle<float>(x0 + i * cellW, 0.0f, cellW,
                                   static_cast<float>(getHeight())).contains(p))
            return i;
    return -1;
}

void PageLEDs::mouseDown(const juce::MouseEvent& e)
{
    const int idx = pageAt(e.position);
    if (idx >= 0 && onPageClicked)
        onPageClicked(idx);
}

void PageLEDs::paint(juce::Graphics& g)
{
    using LAF = DayRuinerLookAndFeel;
    const float totalW = cellW * static_cast<float>(kNumPages);
    const float x0 = (static_cast<float>(getWidth()) - totalW) * 0.5f;

    for (int i = 0; i < kNumPages; ++i)
    {
        const float cx = x0 + (static_cast<float>(i) + 0.5f) * cellW;
        const bool lit = (i == activePage);

        if (lit)
            LAF::paintAmberGlow(g, { cx, 8.0f }, 12.0f, 1.0f);

        g.setColour(lit ? LAF::amber : juce::Colour(0xff3a3a3a));
        g.fillEllipse(cx - 4.5f, 8.0f - 4.5f, 9.0f, 9.0f);

        LAF::drawEngravedText(g, juce::String(i + 1) + ":4",
                              juce::Rectangle<int>(static_cast<int>(x0 + i * cellW), 16,
                                                   static_cast<int>(cellW), 16),
                              LAF::labelFont(12.0f), juce::Justification::centred,
                              lit ? LAF::engraved : LAF::engravedDim);
    }
}
