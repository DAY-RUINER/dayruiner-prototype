#pragma once

#include <JuceHeader.h>
#include "EngineAdapter.h"

// ============================================================================
// GrainFieldDisplay — the GRANULAR page centerpiece.
//
// Shows the loaded sample as a high-resolution waveform (source trace),
// the live grain windows being carved out of it (decomposition), the
// playhead, and a thinner trace of the recomposed granular output stream.
// Fed by IGrainFieldSource (see EngineAdapter.h); repaints at ~30 fps.
// ============================================================================
class GrainFieldDisplay : public juce::Component,
                           public juce::FileDragAndDropTarget,
                           private juce::Timer
{
public:
    explicit GrainFieldDisplay(IGrainFieldSource& source);
    ~GrainFieldDisplay() override;

    void paint(juce::Graphics& g) override;

    // Fired on click (loads via host file chooser) and on audio file drop.
    std::function<void()> onLoadRequested;
    std::function<void(const juce::StringArray&)> onFilesDropped;

    void mouseUp(const juce::MouseEvent& e) override;
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

private:
    void timerCallback() override { repaint(); }

    void drawTicks(juce::Graphics& g, juce::Rectangle<float> plot) const;
    void drawSourceTrace(juce::Graphics& g, juce::Rectangle<float> plot) const;
    void drawGrainWindows(juce::Graphics& g, juce::Rectangle<float> plot) const;
    void drawPlayhead(juce::Graphics& g, juce::Rectangle<float> plot) const;
    void drawOutputTrace(juce::Graphics& g, juce::Rectangle<float> plot) const;

    IGrainFieldSource& source;
    mutable std::vector<float> minBuf, maxBuf;
};
