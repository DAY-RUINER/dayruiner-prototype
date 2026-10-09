#pragma once

#include <JuceHeader.h>
#include "EngineAdapter.h"

// ============================================================================
// PresetBrowser — overlay panel with the REAL 222 factory presets.
//
// Two banks (DRUMS 111 / SYNTH 111) with live search. Single click loads the
// preset (applies it to the APVTS through IPresetStore::selectPreset).
// Opened by clicking the OLED display.
// ============================================================================
class PresetBrowser : public juce::Component
{
public:
    explicit PresetBrowser(IPresetStore& store);
    ~PresetBrowser() override;

    std::function<void(int presetIndex)> onPresetChosen;

    void showBrowser();
    void hideBrowser();
    bool isShowing() const { return isVisible(); }

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    class BankModel : public juce::ListBoxModel
    {
    public:
        BankModel(PresetBrowser& owner, bool drums);
        int getNumRows() override;
        void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) override;
        void listBoxItemClicked(int row, const juce::MouseEvent&) override;
        void refilter(const juce::String& query);
        int presetIndexForRow(int row) const;

    private:
        PresetBrowser& owner;
        bool drumsBank;
        std::vector<int> rows; // preset indices into the store
    };

    void refilter();

    IPresetStore& store;
    juce::TextEditor searchBox;
    juce::ListBox drumsList, synthList;
    BankModel drumsModel, synthModel;
    juce::TextButton closeButton { "CLOSE" };
};
