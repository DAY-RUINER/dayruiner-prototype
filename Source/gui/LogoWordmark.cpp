#include "LogoWordmark.h"
#include "DayRuinerLookAndFeel.h"

LogoWordmark::LogoWordmark() {}

void LogoWordmark::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const juce::Font font = DayRuinerLookAndFeel::monoFont(34.0f, true);
    g.setFont(font);

    const juce::String text("DAY RUINER");
    const float textW = juce::GlyphArrangement::getStringWidth(font, text);
    const float charW = textW / static_cast<float>(text.length());
    const float textX = bounds.getX() + 6.0f;
    const float baselineY = bounds.getY() + bounds.getHeight() * 0.5f + 12.0f;

    // Sigil wires first (etched underneath the lettering).
    drawSigilWires(g, textX, baselineY, charW);

    // Etched lettering: dark cut, then pale face.
    g.setColour(juce::Colour(0xff000000).withAlpha(0.8f));
    g.drawText(text, juce::Rectangle<float>(textX, bounds.getY() + 2.0f,
                                            textW + 20.0f, bounds.getHeight()),
               juce::Justification::centredLeft, true);
    g.setColour(juce::Colour(0xffb5b5b5));
    g.drawText(text, juce::Rectangle<float>(textX, bounds.getY(),
                                            textW + 20.0f, bounds.getHeight()),
               juce::Justification::centredLeft, true);
}

void LogoWordmark::drawSigilWires(juce::Graphics& g, float textX, float baselineY, float charW)
{
    // Seeded: the same sigil every frame — this is a maker's mark, not noise.
    juce::Random rng(0xD4711E);

    const int numChars = 10; // "DAY RUINER"
    for (int c = 0; c < numChars; ++c)
    {
        // Minimal: most letters get nothing, some get one wire, rarely two.
        const float roll = rng.nextFloat();
        const int numWires = (roll < 0.45f) ? 0 : (roll < 0.85f ? 1 : 2);

        for (int w = 0; w < numWires; ++w)
        {
            const float sx = textX + (static_cast<float>(c) + 0.5f) * charW
                           + (rng.nextFloat() - 0.5f) * charW * 0.7f;
            // Start from a random edge of the glyph cell: top, bottom, left, right.
            const int edge = rng.nextInt(4);
            float px = sx, py = baselineY - 12.0f;
            if (edge == 0)      { py = baselineY - 26.0f; }
            else if (edge == 1) { py = baselineY + 2.0f; }
            else if (edge == 2) { px = sx - charW * 0.5f; py = baselineY - 13.0f; }
            else                { px = sx + charW * 0.5f; py = baselineY - 13.0f; }

            // 2–4 short segments wandering off at odd angles.
            const int segs = 2 + rng.nextInt(3);
            float angle = rng.nextFloat() * juce::MathConstants<float>::twoPi;

            juce::Path wire;
            wire.startNewSubPath(px, py);
            for (int s = 0; s < segs; ++s)
            {
                const float len = 8.0f + rng.nextFloat() * 22.0f;
                angle += (rng.nextFloat() - 0.5f) * 1.6f;
                px += std::cos(angle) * len;
                py += std::sin(angle) * len;
                wire.lineTo(px, py);
            }

            // Etched look: dark groove with a faint bright edge.
            g.setColour(juce::Colour(0xff000000).withAlpha(0.55f));
            g.strokePath(wire, juce::PathStrokeType(2.0f));
            g.setColour(juce::Colour(0xff9a9a9a).withAlpha(0.30f));
            g.strokePath(wire, juce::PathStrokeType(1.0f));
        }
    }
}
