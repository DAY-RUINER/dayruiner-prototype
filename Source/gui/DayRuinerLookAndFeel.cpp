#include "DayRuinerLookAndFeel.h"

const juce::Colour DayRuinerLookAndFeel::panelDark   { 0xff141414 };
const juce::Colour DayRuinerLookAndFeel::panelEdge   { 0xff2e2e2e };
const juce::Colour DayRuinerLookAndFeel::engraved    { 0xffc9c9c9 };
const juce::Colour DayRuinerLookAndFeel::engravedDim { 0xff7c7c7c };
const juce::Colour DayRuinerLookAndFeel::amber       { 0xfff5a623 };
const juce::Colour DayRuinerLookAndFeel::amberBright { 0xffffc964 };
const juce::Colour DayRuinerLookAndFeel::amberDim    { 0xff6e4c1e };
const juce::Colour DayRuinerLookAndFeel::keyOff      { 0xff232323 };
const juce::Colour DayRuinerLookAndFeel::oledBg      { 0xff050505 };
const juce::Colour DayRuinerLookAndFeel::waveTrace   { 0xffe9c893 };

DayRuinerLookAndFeel::DayRuinerLookAndFeel() = default;

juce::Font DayRuinerLookAndFeel::monoFont(float size, bool bold)
{
    // Prefer a real system monospace so the terminal aesthetic holds up
    // on every platform; fall back to JUCE's default monospace.
    static juce::StringArray available;
    if (available.isEmpty())
        available = juce::Font::findAllTypefaceNames();

    static const char* candidates[] = {
        "DejaVu Sans Mono", "Noto Sans Mono", "Liberation Mono",
        "Consolas", "Menlo", "Courier New", nullptr
    };

    const auto style = bold ? juce::Font::bold : juce::Font::plain;
    for (int i = 0; candidates[i] != nullptr; ++i)
        if (available.contains(candidates[i], true))
            return juce::Font(juce::FontOptions(candidates[i], size, style));

    return juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), size, style));
}

juce::Font DayRuinerLookAndFeel::labelFont(float size)
{
    // Clean grotesque for engraved panel labels (Digitakt-style).
    return juce::Font(juce::FontOptions(size, juce::Font::plain));
}

void DayRuinerLookAndFeel::paintBrushedMetal(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.fillAll(panelDark);

    // Deterministic brushed streaks: same seed -> identical grain every repaint.
    juce::Random rng(0x5EED4E7A);
    const int h = bounds.getHeight();
    for (int i = 0; i < h; i += 2)
    {
        const int y = bounds.getY() + static_cast<int>(rng.nextFloat() * static_cast<float>(h));
        const float a = 0.012f + rng.nextFloat() * 0.030f;
        g.setColour(rng.nextBool() ? juce::Colour(0xffffff).withAlpha(a)
                                   : juce::Colour(0x000000).withAlpha(a * 1.5f));
        g.drawHorizontalLine(y, static_cast<float>(bounds.getX()),
                             static_cast<float>(bounds.getRight()));
    }
}

void DayRuinerLookAndFeel::paintScrew(juce::Graphics& g, juce::Point<float> centre, float radius)
{
    // Bolted-hardware screw: dark recess, metallic rim, driver slot, top sheen.
    g.setColour(juce::Colour(0xff080808));
    g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);

    juce::ColourGradient rim(juce::Colour(0xff4d4d4d), centre.x - radius * 0.5f, centre.y - radius * 0.6f,
                             juce::Colour(0xff161616), centre.x, centre.y, true);
    g.setGradientFill(rim);
    g.drawEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 2.0f);

    // Driver slot at a fixed angle (hardware is torqued, not random).
    const float a = juce::MathConstants<float>::pi * 0.22f;
    g.setColour(juce::Colour(0xff050505));
    g.drawLine(centre.x - std::cos(a) * (radius - 2.5f), centre.y - std::sin(a) * (radius - 2.5f),
               centre.x + std::cos(a) * (radius - 2.5f), centre.y + std::sin(a) * (radius - 2.5f), 2.5f);

    g.setColour(juce::Colours::white.withAlpha(0.10f));
    g.drawEllipse(centre.x - radius + 1.5f, centre.y - radius + 1.0f,
                  (radius - 1.5f) * 2.0f, (radius - 1.5f) * 2.0f, 1.0f);
}

void DayRuinerLookAndFeel::drawEngravedText(juce::Graphics& g, const juce::String& text,
                                           juce::Rectangle<int> area, const juce::Font& font,
                                           juce::Justification just, juce::Colour colour)
{
    // Engraved = dark cut below, light face on top.
    g.setFont(font);
    g.setColour(juce::Colour(0xff000000).withAlpha(0.75f));
    g.drawText(text, area.translated(0, 1), just, true);
    g.setColour(colour);
    g.drawText(text, area, just, true);
}

void DayRuinerLookAndFeel::paintAmberGlow(juce::Graphics& g, juce::Point<float> centre,
                                         float radius, float intensity)
{
    juce::ColourGradient grad(amber.withAlpha(0.45f * intensity), centre.x, centre.y,
                              amber.withAlpha(0.0f), centre.x + radius, centre.y, true);
    g.setGradientFill(grad);
    g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
}

void DayRuinerLookAndFeel::drawScrollbar(juce::Graphics& g, juce::ScrollBar&,
                                           int x, int y, int width, int height,
                                           bool isScrollbarVertical, int thumbStartPosition,
                                           int thumbSize, bool isMouseOver, bool)
{
    // Hardware-style scrollbar: dark slot, amber-dim thumb. No blue anywhere.
    g.setColour(juce::Colour(0xff0c0c0c).withAlpha(0.6f));
    g.fillRect(x, y, width, height);

    juce::Rectangle<float> thumb;
    if (isScrollbarVertical)
        thumb = juce::Rectangle<float>(static_cast<float>(x) + 1.0f,
                                       static_cast<float>(y + thumbStartPosition),
                                       static_cast<float>(width) - 2.0f,
                                       static_cast<float>(thumbSize));
    else
        thumb = juce::Rectangle<float>(static_cast<float>(x + thumbStartPosition),
                                       static_cast<float>(y) + 1.0f,
                                       static_cast<float>(thumbSize),
                                       static_cast<float>(height) - 2.0f);

    g.setColour((isMouseOver ? amber : amberDim).withAlpha(0.85f));
    g.fillRoundedRectangle(thumb, 3.0f);
}

void DayRuinerLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                           juce::Slider&)
{
    const float cx = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
    const float cy = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
    const float radius = juce::jmin(static_cast<float>(width), static_cast<float>(height)) * 0.5f - 3.0f;

    // Drop shadow grounds the knob on the panel.
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.fillEllipse(cx - radius, cy - radius + 3.0f, radius * 2.0f, radius * 2.0f);

    // Chunky dark body.
    juce::ColourGradient body(juce::Colour(0xff353535), cx - radius * 0.4f, cy - radius * 0.5f,
                              juce::Colour(0xff0f0f0f), cx, cy, true);
    g.setGradientFill(body);
    g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

    // Knurled rim.
    g.setColour(juce::Colour(0xff4c4c4c));
    for (int i = 0; i < 48; ++i)
    {
        const float a = static_cast<float>(i) / 48.0f * juce::MathConstants<float>::twoPi;
        g.drawLine(cx + std::cos(a) * (radius - 1.0f), cy + std::sin(a) * (radius - 1.0f),
                   cx + std::cos(a) * (radius - 5.0f), cy + std::sin(a) * (radius - 5.0f), 1.0f);
    }

    // Top sheen arc — machined, not glossy.
    juce::Path sheen;
    sheen.addArc(cx - radius + 4.0f, cy - radius + 4.0f, (radius - 4.0f) * 2.0f, (radius - 4.0f) * 2.0f,
                 juce::MathConstants<float>::pi * 1.15f, juce::MathConstants<float>::pi * 1.85f, true);
    g.setColour(juce::Colours::white.withAlpha(0.06f));
    g.strokePath(sheen, juce::PathStrokeType(3.0f));

    // White pointer line, like the mockup.
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    g.setColour(juce::Colours::white.withAlpha(0.95f));
    g.drawLine(cx, cy,
               cx + std::sin(angle) * (radius - 8.0f),
               cy - std::cos(angle) * (radius - 8.0f), 3.0f);

    g.setColour(juce::Colour(0xff050505).withAlpha(0.8f));
    g.drawEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 1.0f);
}
