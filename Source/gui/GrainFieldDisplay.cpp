#include "GrainFieldDisplay.h"
#include "DayRuinerLookAndFeel.h"

GrainFieldDisplay::GrainFieldDisplay(IGrainFieldSource& src) : source(src)
{
    startTimerHz(30);
}

GrainFieldDisplay::~GrainFieldDisplay() = default;

void GrainFieldDisplay::mouseUp(const juce::MouseEvent& e)
{
    // Plain click (not a drag) opens the sample file chooser.
    if (! e.mouseWasDraggedSinceMouseDown() && onLoadRequested)
        onLoadRequested();
}

static bool isAudioFile(const juce::String& path)
{
    static const char* const exts[] = { ".wav", ".aif", ".aiff", ".flac",
                                        ".ogg", ".mp3", ".wma", nullptr };
    for (int i = 0; exts[i] != nullptr; ++i)
        if (path.endsWithIgnoreCase(exts[i]))
            return true;
    return false;
}

bool GrainFieldDisplay::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& f : files)
        if (isAudioFile(f))
            return true;
    return false;
}

void GrainFieldDisplay::filesDropped(const juce::StringArray& files, int, int)
{
    juce::StringArray audio;
    for (const auto& f : files)
        if (isAudioFile(f))
            audio.add(f);
    if (audio.size() > 0 && onFilesDropped)
        onFilesDropped(audio);
}

void GrainFieldDisplay::drawTicks(juce::Graphics& g, juce::Rectangle<float> plot) const
{
    g.setColour(DayRuinerLookAndFeel::engravedDim.withAlpha(0.35f));
    const float step = 26.0f;
    int i = 0;
    for (float x = plot.getX(); x <= plot.getRight() + 0.5f; x += step, ++i)
    {
        const float h = (i % 4 == 0) ? 7.0f : 4.0f;
        g.drawLine(x, plot.getY(), x, plot.getY() + h, 1.0f);
        g.drawLine(x, plot.getBottom() - h, x, plot.getBottom(), 1.0f);
    }
}

void GrainFieldDisplay::drawSourceTrace(juce::Graphics& g, juce::Rectangle<float> plot) const
{
    const int bins = juce::jmin(source.getNumBins(), static_cast<int>(plot.getWidth()));
    if (bins < 2)
        return;

    minBuf.assign(static_cast<size_t>(bins), 0.0f);
    maxBuf.assign(static_cast<size_t>(bins), 0.0f);
    source.getSourcePeaks(minBuf.data(), maxBuf.data(), bins);

    const float midY = plot.getY() + plot.getHeight() * 0.34f;
    const float amp = plot.getHeight() * 0.30f;

    juce::Path trace;
    bool started = false;
    for (int i = 0; i < bins; ++i)
    {
        const float x = plot.getX() + (static_cast<float>(i) / static_cast<float>(bins - 1)) * plot.getWidth();
        const float y = midY - juce::jlimit(-1.0f, 1.0f, maxBuf[static_cast<size_t>(i)]) * amp;
        if (! started) { trace.startNewSubPath(x, y); started = true; }
        else           { trace.lineTo(x, y); }
    }
    g.setColour(DayRuinerLookAndFeel::waveTrace);
    g.strokePath(trace, juce::PathStrokeType(1.2f));

    // Mirror (negative peaks) at low alpha for density.
    juce::Path mirror;
    started = false;
    for (int i = 0; i < bins; ++i)
    {
        const float x = plot.getX() + (static_cast<float>(i) / static_cast<float>(bins - 1)) * plot.getWidth();
        const float y = midY - juce::jlimit(-1.0f, 1.0f, minBuf[static_cast<size_t>(i)]) * amp;
        if (! started) { mirror.startNewSubPath(x, y); started = true; }
        else           { mirror.lineTo(x, y); }
    }
    g.setColour(DayRuinerLookAndFeel::waveTrace.withAlpha(0.45f));
    g.strokePath(mirror, juce::PathStrokeType(1.0f));
}

void GrainFieldDisplay::drawGrainWindows(juce::Graphics& g, juce::Rectangle<float> plot) const
{
    const int n = source.getNumGrains();
    const float topY = plot.getY() + plot.getHeight() * 0.04f;
    const float winH = plot.getHeight() * 0.60f;

    for (int i = 0; i < n; ++i)
    {
        const GrainFieldGrain grain = source.getGrain(i);
        const float x0 = plot.getX() + grain.start01 * plot.getWidth();
        const float x1 = plot.getX() + grain.end01 * plot.getWidth();
        if (x1 <= x0)
            continue;

        juce::Rectangle<float> win(x0, topY, x1 - x0, winH);
        g.setColour(DayRuinerLookAndFeel::amber.withAlpha(0.16f * grain.intensity));
        g.fillRoundedRectangle(win, 3.0f);
        g.setColour(DayRuinerLookAndFeel::amber.withAlpha(0.70f * grain.intensity));
        g.drawRoundedRectangle(win, 3.0f, 1.5f);
    }
}

void GrainFieldDisplay::drawPlayhead(juce::Graphics& g, juce::Rectangle<float> plot) const
{
    const float ph = source.getPlayhead01();
    if (ph < 0.0f)
        return;

    const float x = plot.getX() + juce::jlimit(0.0f, 1.0f, ph) * plot.getWidth();
    g.setColour(DayRuinerLookAndFeel::amberBright.withAlpha(0.22f));
    g.drawLine(x, plot.getY(), x, plot.getBottom(), 7.0f);
    g.setColour(DayRuinerLookAndFeel::amberBright.withAlpha(0.55f));
    g.drawLine(x, plot.getY(), x, plot.getBottom(), 3.0f);
    g.setColour(DayRuinerLookAndFeel::amberBright);
    g.drawLine(x, plot.getY(), x, plot.getBottom(), 1.5f);
}

void GrainFieldDisplay::drawOutputTrace(juce::Graphics& g, juce::Rectangle<float> plot) const
{
    const int bins = juce::jmin(source.getNumBins(), static_cast<int>(plot.getWidth()));
    if (bins < 2)
        return;

    minBuf.assign(static_cast<size_t>(bins), 0.0f);
    maxBuf.assign(static_cast<size_t>(bins), 0.0f);
    source.getOutputPeaks(minBuf.data(), maxBuf.data(), bins);

    const float midY = plot.getY() + plot.getHeight() * 0.86f;
    const float amp = plot.getHeight() * 0.10f;

    juce::Path trace;
    bool started = false;
    for (int i = 0; i < bins; ++i)
    {
        const float x = plot.getX() + (static_cast<float>(i) / static_cast<float>(bins - 1)) * plot.getWidth();
        const float y = midY - juce::jlimit(-1.0f, 1.0f, maxBuf[static_cast<size_t>(i)]) * amp;
        if (! started) { trace.startNewSubPath(x, y); started = true; }
        else           { trace.lineTo(x, y); }
    }
    g.setColour(DayRuinerLookAndFeel::amber.withAlpha(0.55f));
    g.strokePath(trace, juce::PathStrokeType(1.0f));
}

void GrainFieldDisplay::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    g.setColour(juce::Colour(0xff0a0a0a));
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(DayRuinerLookAndFeel::panelEdge);
    g.drawRoundedRectangle(bounds, 8.0f, 1.0f);

    // Engraved title inside the top edge of the panel.
    DayRuinerLookAndFeel::drawEngravedText(
        g, juce::String::fromUTF8(" \xe2\x80\x94 GRAINFIELD \xe2\x80\x94 "),
        juce::Rectangle<int>(static_cast<int>(bounds.getX()),
                             static_cast<int>(bounds.getY()) + 3,
                             static_cast<int>(bounds.getWidth()), 18),
        DayRuinerLookAndFeel::monoFont(12.0f), juce::Justification::centred,
        DayRuinerLookAndFeel::engravedDim);

    const juce::Rectangle<float> plot = bounds.reduced(14.0f).withTrimmedTop(26.0f);

    if (! source.hasSample())
    {
        DayRuinerLookAndFeel::drawEngravedText(
            g, "NO SAMPLE LOADED", plot.toNearestInt(),
            DayRuinerLookAndFeel::monoFont(14.0f), juce::Justification::centred,
            DayRuinerLookAndFeel::engravedDim);
        auto hint = plot.toNearestInt().withTrimmedTop(24);
        DayRuinerLookAndFeel::drawEngravedText(
            g, "click or drop audio here", hint,
            DayRuinerLookAndFeel::monoFont(11.0f), juce::Justification::centred,
            DayRuinerLookAndFeel::engravedDim.withAlpha(0.7f));
        return;
    }

    const juce::String sname = source.getSampleName();
    if (sname.isNotEmpty())
    {
        auto nameArea = juce::Rectangle<int>(static_cast<int>(bounds.getX()),
                                            static_cast<int>(bounds.getBottom()) - 18,
                                            static_cast<int>(bounds.getWidth()), 16);
        DayRuinerLookAndFeel::drawEngravedText(
            g, sname.toUpperCase(), nameArea,
            DayRuinerLookAndFeel::monoFont(11.0f), juce::Justification::centredRight,
            DayRuinerLookAndFeel::engravedDim.withAlpha(0.8f));
    }

    drawTicks(g, plot);
    drawSourceTrace(g, plot);
    drawGrainWindows(g, plot);
    drawPlayhead(g, plot);
    drawOutputTrace(g, plot);
}
