#include "ThemeManager.h"

namespace
{
    ColorTheme makeTheme (juce::uint32 background, juce::uint32 panel, juce::uint32 accent,
                          juce::uint32 text, juce::uint32 seqActive, juce::uint32 seqInactive,
                          juce::uint32 grain, juce::uint32 waveform, const char* name)
    {
        ColorTheme t;
        t.background        = juce::Colour (background);
        t.panel             = juce::Colour (panel);
        t.accent            = juce::Colour (accent);
        t.text              = juce::Colour (text);
        t.sequencerActive   = juce::Colour (seqActive);
        t.sequencerInactive = juce::Colour (seqInactive);
        t.grain             = juce::Colour (grain);
        t.waveform          = juce::Colour (waveform);
        t.name              = juce::String (name);
        return t;
    }
}

ThemeManager::ThemeManager()
{
    // 8 dark themes. Index 0 ("Ember") is the default and matches the
    // DAY RUINER warm-amber/teal palette from the Phase 1 knob study.
    themes.push_back (makeTheme (0xff141210, 0xff1e1b17, 0xffff9a3c, 0xfff2ede6,
                                 0xffffb35c, 0xff3a332c, 0xff7fd4c1, 0xffff9a3c, "Ember"));
    themes.push_back (makeTheme (0xff0d1117, 0xff161b22, 0xff58a6ff, 0xffe6edf3,
                                 0xff79c0ff, 0xff2a3139, 0xff3fb950, 0xff58a6ff, "Midnight"));
    themes.push_back (makeTheme (0xff12101a, 0xff1c1830, 0xffb28dff, 0xffeae6f7,
                                 0xffc9a8ff, 0xff2c2745, 0xff7fe0d4, 0xffb28dff, "Violet Haze"));
    themes.push_back (makeTheme (0xff0e1410, 0xff16201a, 0xff7fc97f, 0xffe8f0e8,
                                 0xff9be09b, 0xff243428, 0xffd4c17f, 0xff7fc97f, "Forest"));
    themes.push_back (makeTheme (0xff160e0e, 0xff221415, 0xffff6b6b, 0xfff7e8e8,
                                 0xffff8585, 0xff382626, 0xff7fd4c1, 0xffff6b6b, "Crimson"));
    themes.push_back (makeTheme (0xff0a1218, 0xff10202c, 0xff4fd1c5, 0xffe2f2f0,
                                 0xff6fe3d8, 0xff1e3038, 0xffffb35c, 0xff4fd1c5, "Ocean Deep"));
    themes.push_back (makeTheme (0xff12100c, 0xff1e1811, 0xffd4a373, 0xfff0e4d0,
                                 0xffe8b878, 0xff33291c, 0xff9db4a0, 0xffd4a373, "Dune"));
    themes.push_back (makeTheme (0xff101010, 0xff1a1a1a, 0xffe0e0e0, 0xfff0f0f0,
                                 0xffffffff, 0xff2e2e2e, 0xffa0a0a0, 0xffe0e0e0, "Mono Slate"));
}

void ThemeManager::setTheme (int index)
{
    if (themes.empty())
        return;

    currentTheme = juce::jlimit (0, (int) themes.size() - 1, index);
}

const ColorTheme& ThemeManager::getCurrentTheme() const
{
    jassert (! themes.empty());
    return themes[(size_t) juce::jlimit (0, (int) themes.size() - 1, currentTheme)];
}

int ThemeManager::getNumThemes() const
{
    return (int) themes.size();
}

int ThemeManager::getCurrentThemeIndex() const
{
    return currentTheme;
}

juce::String ThemeManager::getThemeName (int index) const
{
    if (index < 0 || index >= (int) themes.size())
        return {};

    return themes[(size_t) index].name;
}
