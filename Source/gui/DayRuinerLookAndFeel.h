#pragma once

#include <JuceHeader.h>

// ============================================================================
// DayRuinerLookAndFeel — the whole visual language of the DAY RUINER hardware.
//
// Digitakt-inspired and built to last: brushed charcoal metal, engraved
// labels, chunky knurled knobs with white pointers. Amber is the ONLY accent
// colour anywhere in the UI — no neon, ever.
// ============================================================================
class DayRuinerLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DayRuinerLookAndFeel();

    // ---- palette ----
    static const juce::Colour panelDark;    // base brushed metal
    static const juce::Colour panelEdge;    // panel borders / dividers
    static const juce::Colour engraved;     // engraved label text
    static const juce::Colour engravedDim;  // secondary engraved text
    static const juce::Colour amber;        // the one accent colour
    static const juce::Colour amberBright;
    static const juce::Colour amberDim;
    static const juce::Colour keyOff;       // unlit key / button body
    static const juce::Colour oledBg;       // OLED display background
    static const juce::Colour waveTrace;    // grainfield source waveform

    // ---- type ----
    static juce::Font monoFont(float size, bool bold = false);
    static juce::Font labelFont(float size);

    // ---- texture helpers ----
    static void paintBrushedMetal(juce::Graphics& g, juce::Rectangle<int> bounds);
    static void paintScrew(juce::Graphics& g, juce::Point<float> centre, float radius = 10.0f);
    static void drawEngravedText(juce::Graphics& g, const juce::String& text,
                                 juce::Rectangle<int> area, const juce::Font& font,
                                 juce::Justification just = juce::Justification::centred,
                                 juce::Colour colour = engraved);
    static void paintAmberGlow(juce::Graphics& g, juce::Point<float> centre,
                               float radius, float intensity = 1.0f);

    // ---- widgets ----
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider& slider) override;
    void drawScrollbar(juce::Graphics& g, juce::ScrollBar&, int x, int y, int width, int height,
                       bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                       bool isMouseOver, bool isMouseDown) override;
};
