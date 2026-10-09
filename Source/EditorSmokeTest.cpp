//==============================================================================
// DAY RUINER temporary editor integration test (headless, xvfb).
// Constructs the REAL DayRuinerEditor against the REAL processor/APVTS and
// adapters, switches through every page, and snapshots each one to PNG.
// Run with: xvfb-run ./DayRuinerEditorTest
//==============================================================================

#include <JuceHeader.h>

#include <iostream>

#include "PluginProcessor.h"
#include "gui/DayRuinerEditor.h"

namespace
{
    bool savePng (const juce::Image& img, const juce::File& file)
    {
        juce::FileOutputStream stream (file);
        if (! stream.openedOk())
            return false;
        juce::PNGImageFormat png;
        return png.writeImageToStream (img, stream);
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI guiInit;
    std::cout << "=== DAY RUINER EDITOR TEST ===\n";

    DayRuinerAudioProcessor proc;
    proc.prepareToPlay (44100.0, 512);

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    if (ed == nullptr)
    {
        std::cout << "FAIL: createEditor() returned nullptr\n";
        return 1;
    }

    auto* druEditor = dynamic_cast<DayRuinerEditor*> (ed.get());
    if (druEditor == nullptr)
    {
        std::cout << "FAIL: editor is not a DayRuinerEditor\n";
        return 1;
    }

    std::cout << "editor size: " << ed->getWidth() << "x" << ed->getHeight() << "\n";
    if (ed->getWidth() != 1280 || ed->getHeight() != 800)
    {
        std::cout << "FAIL: unexpected editor size\n";
        return 1;
    }

    // Select a preset through the store path (index 0 = first drum preset),
    // then snapshot every page.
    const struct { PageTabs::Page page; const char* name; } pages[] =
    {
        { PageTabs::Page::Granular, "granular" },
        { PageTabs::Page::Drums,    "drums" },
        { PageTabs::Page::Synth,    "synth" },
        { PageTabs::Page::Fx,       "fx" },
        { PageTabs::Page::Mixer,    "mixer" },
    };

    const juce::File outDir = juce::File ("/tmp/dayruiner_editor_test");
    outDir.createDirectory();

    for (const auto& pg : pages)
    {
        druEditor->selectPage (pg.page);

        juce::Image snap = ed->createComponentSnapshot (ed->getLocalBounds(), false, 1.0f);
        if (! snap.isValid())
        {
            std::cout << "FAIL: snapshot invalid for page " << pg.name << "\n";
            return 1;
        }

        const juce::File out = outDir.getChildFile (juce::String ("page_") + pg.name + ".png");
        if (! savePng (snap, out))
        {
            std::cout << "FAIL: could not write " << out.getFullPathName().toStdString() << "\n";
            return 1;
        }
        std::cout << "page " << pg.name << " -> " << out.getFullPathName().toStdString() << "\n";
    }

    std::cout << "EDITOR TEST: PASS\n";
    return 0;
}
