#include "GrainPanel.h"

GrainPanel::GrainPanel()
{
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
}

void GrainPanel::setValues (float x01, float y01)
{
    xVal = juce::jlimit (0.0f, 1.0f, x01);
    yVal = juce::jlimit (0.0f, 1.0f, y01);
    repaint();
}

void GrainPanel::setColours (juce::Colour background, juce::Colour crosshair)
{
    bgColour = background;
    lineColour = crosshair;
    repaint();
}

void GrainPanel::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    g.setColour (bgColour);
    g.fillRoundedRectangle (bounds, 8.0f);

    const float px = bounds.getX() + xVal * bounds.getWidth();
    const float py = bounds.getY() + (1.0f - yVal) * bounds.getHeight();

    g.setColour (lineColour.withAlpha (0.55f));
    g.drawLine (px, bounds.getY() + 4.0f, px, bounds.getBottom() - 4.0f, 1.0f);
    g.drawLine (bounds.getX() + 4.0f, py, bounds.getRight() - 4.0f, py, 1.0f);

    g.setColour (lineColour);
    g.fillEllipse (px - 5.0f, py - 5.0f, 10.0f, 10.0f);

    g.setColour (lineColour.withAlpha (0.8f));
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText ("Length >", bounds.getX() + 8.0f, bounds.getBottom() - 22.0f, 80.0f, 16.0f,
                juce::Justification::left);
    g.drawText ("^ Freq", bounds.getX() + 8.0f, bounds.getY() + 6.0f, 80.0f, 16.0f,
                juce::Justification::left);
}

void GrainPanel::mouseDown (const juce::MouseEvent& e)
{
    updateFromMouse (e);
}

void GrainPanel::mouseDrag (const juce::MouseEvent& e)
{
    updateFromMouse (e);
}

void GrainPanel::updateFromMouse (const juce::MouseEvent& e)
{
    const float w = (float) getWidth();
    const float h = (float) getHeight();

    if (w <= 0.0f || h <= 0.0f)
        return;

    xVal = juce::jlimit (0.0f, 1.0f, (float) e.x / w);
    yVal = juce::jlimit (0.0f, 1.0f, 1.0f - (float) e.y / h);

    if (onChange)
        onChange (xVal, yVal);

    repaint();
}
