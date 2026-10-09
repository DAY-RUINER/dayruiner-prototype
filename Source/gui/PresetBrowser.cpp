#include "PresetBrowser.h"
#include "DayRuinerLookAndFeel.h"

// ---- BankModel ----

PresetBrowser::BankModel::BankModel(PresetBrowser& o, bool drums)
    : owner(o), drumsBank(drums)
{
    refilter({});
}

int PresetBrowser::BankModel::getNumRows()
{
    return static_cast<int>(rows.size());
}

int PresetBrowser::BankModel::presetIndexForRow(int row) const
{
    if (row < 0 || row >= static_cast<int>(rows.size()))
        return -1;
    return rows[static_cast<size_t>(row)];
}

void PresetBrowser::BankModel::refilter(const juce::String& query)
{
    rows.clear();
    const juce::String q = query.trim().toLowerCase();
    const int n = owner.store.getNumPresets();

    for (int i = 0; i < n; ++i)
    {
        const PresetInfo info = owner.store.getPreset(i);
        const bool inBank = drumsBank ? (info.engine == "drums") : (info.engine == "synth");
        if (! inBank)
            continue;
        if (q.isNotEmpty())
        {
            const juce::String hay = (info.name + " " + info.category + " " + info.description).toLowerCase();
            if (! hay.contains(q))
                continue;
        }
        rows.push_back(i);
    }
}

void PresetBrowser::BankModel::paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected)
{
    using LAF = DayRuinerLookAndFeel;
    const int idx = presetIndexForRow(row);
    if (idx < 0)
        return;

    if (selected)
    {
        g.setColour(LAF::amber.withAlpha(0.16f));
        g.fillRect(0, 0, w, h);
        g.setColour(LAF::amber.withAlpha(0.6f));
        g.fillRect(0, 0, 2, h);
    }

    const PresetInfo info = owner.store.getPreset(idx);
    g.setColour(selected ? LAF::amberBright : LAF::engraved);
    g.setFont(LAF::labelFont(13.0f));
    g.drawText(info.name, 10, 0, w - 120, h, juce::Justification::centredLeft, true);
    g.setColour(LAF::engravedDim);
    g.setFont(LAF::labelFont(11.0f));
    g.drawText(info.category, w - 116, 0, 106, h, juce::Justification::centredRight, true);
}

void PresetBrowser::BankModel::listBoxItemClicked(int row, const juce::MouseEvent&)
{
    const int idx = presetIndexForRow(row);
    if (idx >= 0 && owner.onPresetChosen)
        owner.onPresetChosen(idx);
}

// ---- PresetBrowser ----

PresetBrowser::PresetBrowser(IPresetStore& s)
    : store(s), drumsModel(*this, true), synthModel(*this, false)
{
    setVisible(false);

    searchBox.setTextToShowWhenEmpty("search 222 presets...", juce::Colour(0xff5a5a5a));
    searchBox.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff0c0c0c));
    searchBox.setColour(juce::TextEditor::outlineColourId, DayRuinerLookAndFeel::panelEdge);
    searchBox.setColour(juce::TextEditor::focusedOutlineColourId, DayRuinerLookAndFeel::amberDim);
    searchBox.setColour(juce::TextEditor::textColourId, DayRuinerLookAndFeel::engraved);
    searchBox.setColour(juce::CaretComponent::caretColourId, DayRuinerLookAndFeel::amber);
    searchBox.setColour(juce::TextEditor::highlightColourId, DayRuinerLookAndFeel::amberDim);
    searchBox.setFont(DayRuinerLookAndFeel::monoFont(14.0f));
    searchBox.onTextChange = [this] { refilter(); };
    addAndMakeVisible(searchBox);

    for (auto* list : { &drumsList, &synthList })
    {
        list->setColour(juce::ListBox::backgroundColourId, juce::Colour(0x00000000));
        list->setColour(juce::ListBox::outlineColourId, juce::Colour(0x00000000));
        list->setRowHeight(26);
        addAndMakeVisible(list);
    }
    drumsList.setModel(&drumsModel);
    synthList.setModel(&synthModel);

    closeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e1e1e));
    closeButton.setColour(juce::TextButton::textColourOffId, DayRuinerLookAndFeel::engravedDim);
    closeButton.onClick = [this] { hideBrowser(); };
    addAndMakeVisible(closeButton);
}

PresetBrowser::~PresetBrowser()
{
    drumsList.setModel(nullptr);
    synthList.setModel(nullptr);
}

void PresetBrowser::refilter()
{
    drumsModel.refilter(searchBox.getText());
    synthModel.refilter(searchBox.getText());
    drumsList.updateContent();
    synthList.updateContent();
    drumsList.repaint();
    synthList.repaint();
}

void PresetBrowser::showBrowser()
{
    refilter();
    setVisible(true);
    toFront(true);
    searchBox.grabKeyboardFocus();
}

void PresetBrowser::hideBrowser()
{
    setVisible(false);
}

void PresetBrowser::resized()
{
    const int w = 720, h = 520;
    const int x0 = (getWidth() - w) / 2;
    const int y0 = (getHeight() - h) / 2;

    // Children are laid out in absolute editor coords; the backdrop is everything.
    searchBox.setBounds(x0 + 24, y0 + 48, w - 48 - 90, 28);
    closeButton.setBounds(x0 + w - 24 - 80, y0 + 48, 80, 28);

    const int listY = y0 + 100, listH = h - 100 - 24 - 30;
    const int listW = (w - 48 - 16) / 2;
    drumsList.setBounds(x0 + 24, listY, listW, listH);
    synthList.setBounds(x0 + 24 + listW + 16, listY, listW, listH);
}

void PresetBrowser::paint(juce::Graphics& g)
{
    using LAF = DayRuinerLookAndFeel;

    // Dim the hardware behind the browser.
    g.fillAll(juce::Colours::black.withAlpha(0.62f));

    const int w = 720, h = 520;
    const int x0 = (getWidth() - w) / 2;
    const int y0 = (getHeight() - h) / 2;
    const auto panel = juce::Rectangle<int>(x0, y0, w, h).toFloat();

    LAF::paintBrushedMetal(g, panel.toNearestInt());
    g.setColour(LAF::panelEdge);
    g.drawRoundedRectangle(panel, 10.0f, 1.5f);

    LAF::drawEngravedText(g, "PRESET BROWSER", { x0 + 24, y0 + 14, w - 48, 24 },
                          LAF::labelFont(15.0f), juce::Justification::centredLeft);

    LAF::drawEngravedText(g, "DRUMS", { x0 + 24, y0 + 100 - 20, 200, 18 },
                          LAF::labelFont(12.0f), juce::Justification::centredLeft,
                          LAF::engravedDim);
    LAF::drawEngravedText(g, "SYNTH", { x0 + 24 + (w - 48 - 16) / 2 + 16, y0 + 100 - 20, 200, 18 },
                          LAF::labelFont(12.0f), juce::Justification::centredLeft,
                          LAF::engravedDim);

    // Counters.
    LAF::drawEngravedText(g, juce::String(drumsModel.getNumRows()) + " kits",
                          { x0 + 24, y0 + h - 30, 200, 18 },
                          LAF::monoFont(11.0f), juce::Justification::centredLeft,
                          LAF::engravedDim);
    LAF::drawEngravedText(g, juce::String(synthModel.getNumRows()) + " patches",
                          { x0 + 24 + (w - 48 - 16) / 2 + 16, y0 + h - 30, 200, 18 },
                          LAF::monoFont(11.0f), juce::Justification::centredLeft,
                          LAF::engravedDim);
}
