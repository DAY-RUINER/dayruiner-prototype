#pragma once

#include <JuceHeader.h>

// ============================================================================
// TrackRow — exactly 8 track select buttons, labelled 1-8.
// (The mockup renderer kept drawing 13; the real build gets 8.)
// ============================================================================
class TrackRow : public juce::Component
{
public:
    static constexpr int kNumTracks = 8;

    // Track-button behaviour is page-dependent (see README "Page behaviour"):
    //   Select — a tap selects the track for editing (DRUMS and all other
    //            pages). The selected button glows amber.
    //   Mute   — a tap toggles the track's mute (MIXER page only).
    //            Amber lit = audible, dim = muted.
    enum class Mode { Select, Mute };

    TrackRow();

    void setMode(Mode m);
    Mode getMode() const { return mode; }

    void setSelectedTrack(int index);
    int getSelectedTrack() const { return selectedTrack; }

    void setTrackMuted(int index, bool muted);
    bool isTrackMuted(int index) const;

    std::function<void(int index)> onTrackSelected;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    juce::Rectangle<float> buttonBounds(int index) const;
    int buttonAt(juce::Point<float> p) const;

    Mode mode = Mode::Select;
    int selectedTrack = 0;
    bool muted[kNumTracks] = {};
    float btnW = 64.0f, btnH = 44.0f, gap = 8.0f;
};
