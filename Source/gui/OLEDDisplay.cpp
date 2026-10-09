#include "OLEDDisplay.h"
#include "DayRuinerLookAndFeel.h"

OLEDDisplay::OLEDDisplay()
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void OLEDDisplay::setPresetName(const juce::String& name) { presetName = name; repaint(); }
void OLEDDisplay::setPageName(const juce::String& name)   { pageName = name; repaint(); }
void OLEDDisplay::setBpm(double newBpm)                  { bpm = newBpm; repaint(); }

void OLEDDisplay::setLine1Override(const juce::String& line)
{
    if (line1Override != line) { line1Override = line; repaint(); }
}

void OLEDDisplay::clearLine1Override()
{
    if (line1Override.isNotEmpty()) { line1Override.clear(); repaint(); }
}

void OLEDDisplay::mouseDown(const juce::MouseEvent&)
{
    if (onClick)
        onClick();
}

void OLEDDisplay::drawDotGrid(juce::Graphics& g, juce::Rectangle<float> area) const
{
    // Little status glyph cluster, like the mockup's right-hand icons.
    g.setColour(DayRuinerLookAndFeel::amber.withAlpha(0.75f));
    const int cols = 4, rows = 3;
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c)
            g.fillEllipse(area.getX() + c * (area.getWidth() / cols),
                          area.getY() + r * (area.getHeight() / rows), 2.5f, 2.5f);
}

void OLEDDisplay::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    g.setColour(DayRuinerLookAndFeel::oledBg);
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(DayRuinerLookAndFeel::panelEdge);
    g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

    // Faint scanlines for the OLED feel.
    g.setColour(juce::Colours::white.withAlpha(0.025f));
    for (float y = bounds.getY() + 2.0f; y < bounds.getBottom() - 1.0f; y += 3.0f)
        g.drawHorizontalLine(static_cast<int>(y), bounds.getX() + 2.0f, bounds.getRight() - 2.0f);

    const juce::Font font = DayRuinerLookAndFeel::monoFont(16.0f, true);
    g.setFont(font);
    g.setColour(DayRuinerLookAndFeel::amber);

    const float pad = 12.0f;
    const float lineH = bounds.getHeight() * 0.5f;

    juce::String topLine = line1Override.isNotEmpty()
        ? line1Override.toUpperCase().substring(0, 22)
        : "Preset: " + presetName.toUpperCase().substring(0, 14);
    g.drawText(topLine,
               juce::Rectangle<float>(bounds.getX() + pad, bounds.getY() + 4.0f,
                                      bounds.getWidth() - pad * 2.0f, lineH),
               juce::Justification::centredLeft, true);

    juce::String bpmLine = juce::String(bpm, 1) + " BPM";
    g.drawText(bpmLine,
               juce::Rectangle<float>(bounds.getX() + pad, bounds.getY() + 4.0f,
                                      bounds.getWidth() - pad * 2.0f - 44.0f, lineH),
               juce::Justification::centredRight, true);

    g.drawText("PAGE: " + pageName.toUpperCase().substring(0, 12),
               juce::Rectangle<float>(bounds.getX() + pad, bounds.getY() + lineH,
                                      bounds.getWidth() - pad * 2.0f, lineH - 4.0f),
               juce::Justification::centredLeft, true);

    drawDotGrid(g, juce::Rectangle<float>(bounds.getRight() - 40.0f, bounds.getY() + lineH + 2.0f,
                                          28.0f, 18.0f));
}
