#pragma once

#include <JuceHeader.h>

#include <vector>

// A single colour theme for the (paused, intentionally plain) DAY RUINER GUI.
struct ColorTheme
{
    juce::Colour background;
    juce::Colour panel;
    juce::Colour accent;
    juce::Colour text;
    juce::Colour sequencerActive;
    juce::Colour sequencerInactive;
    juce::Colour grain;
    juce::Colour waveform;
    juce::String name;
};

//==============================================================================
// DEVIATION vs the design sketch: the sketch called for 22 themes. GUI design
// work is paused, so this ships 8 tasteful dark themes instead.
class ThemeManager
{
public:
    ThemeManager();

    void setTheme (int index);
    const ColorTheme& getCurrentTheme() const;
    int getNumThemes() const;
    int getCurrentThemeIndex() const;

    // Small addition (not in the sketch): lets the editor populate a theme
    // picker without disturbing the current theme.
    juce::String getThemeName (int index) const;

private:
    std::vector<ColorTheme> themes;
    int currentTheme = 0;
};
