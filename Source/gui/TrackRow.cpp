#include "TrackRow.h"
#include "DayRuinerLookAndFeel.h"

TrackRow::TrackRow() {}

void TrackRow::setMode(Mode m)
{
    if (mode != m)
    {
        mode = m;
        repaint();
    }
}

void TrackRow::setSelectedTrack(int index)
{
    selectedTrack = juce::jlimit(0, kNumTracks - 1, index);
    repaint();
}

void TrackRow::setTrackMuted(int index, bool m)
{
    const int t = juce::jlimit(0, kNumTracks - 1, index);
    if (muted[t] != m)
    {
        muted[t] = m;
        repaint();
    }
}

bool TrackRow::isTrackMuted(int index) const
{
    return muted[juce::jlimit(0, kNumTracks - 1, index)];
}

void TrackRow::resized()
{
    const float totalW = static_cast<float>(getWidth());
    btnW = (totalW - static_cast<float>(kNumTracks - 1) * gap) / static_cast<float>(kNumTracks);
    btnH = static_cast<float>(getHeight()) - 20.0f; // room for the number labels
}

juce::Rectangle<float> TrackRow::buttonBounds(int index) const
{
    return juce::Rectangle<float>(static_cast<float>(index) * (btnW + gap), 0.0f, btnW, btnH);
}

int TrackRow::buttonAt(juce::Point<float> p) const
{
    for (int i = 0; i < kNumTracks; ++i)
        if (buttonBounds(i).contains(p))
            return i;
    return -1;
}

void TrackRow::mouseDown(const juce::MouseEvent& e)
{
    const int idx = buttonAt(e.position);
    if (idx >= 0 && onTrackSelected)
        onTrackSelected(idx);
}

void TrackRow::paint(juce::Graphics& g)
{
    using LAF = DayRuinerLookAndFeel;

    for (int i = 0; i < kNumTracks; ++i)
    {
        const auto bb = buttonBounds(i);

        // Mute mode: amber lit = audible, dim = muted.
        // Select mode: the selected track glows amber.
        const bool lit = (mode == Mode::Mute) ? ! muted[i] : (i == selectedTrack);

        g.setColour(lit ? LAF::amberDim.withAlpha(0.6f) : LAF::keyOff);
        g.fillRoundedRectangle(bb, 6.0f);

        if (lit)
        {
            LAF::paintAmberGlow(g, bb.getCentre(), btnW * 0.6f, 0.8f);
            g.setColour(LAF::amber);
            g.drawRoundedRectangle(bb.reduced(1.0f), 6.0f, 1.5f);
        }

        g.setColour(juce::Colour(0xff000000).withAlpha(0.6f));
        g.drawRoundedRectangle(bb, 6.0f, 1.0f);

        // Engraved number under the button.
        LAF::drawEngravedText(g, juce::String(i + 1),
                              juce::Rectangle<int>(static_cast<int>(bb.getX()),
                                                   static_cast<int>(bb.getBottom()) + 2,
                                                   static_cast<int>(bb.getWidth()), 16),
                              LAF::labelFont(12.0f), juce::Justification::centred,
                              lit ? LAF::amber : LAF::engravedDim);
    }
}
