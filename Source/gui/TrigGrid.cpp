#include "TrigGrid.h"
#include "DayRuinerLookAndFeel.h"

TrigGrid::TrigGrid()
{
    startTimerHz(5); // FX blink rate
}

void TrigGrid::setStep(int index, bool active, bool fx)
{
    if (index < 0 || index >= kNumKeys)
        return;
    stepActive[index] = active;
    stepFx[index] = fx;
    repaint();
}

void TrigGrid::setPlayStep(int index)
{
    if (index != playStep)
    {
        playStep = index;
        repaint();
    }
}

void TrigGrid::timerCallback()
{
    // Only burn repaints while an FX step is actually blinking.
    bool anyFx = false;
    for (int i = 0; i < kNumKeys; ++i)
        anyFx = anyFx || (stepActive[i] && stepFx[i]);

    if (anyFx)
    {
        blinkOn = ! blinkOn;
        repaint();
    }
    else if (! blinkOn)
    {
        blinkOn = true;
        repaint();
    }
}

void TrigGrid::resized()
{
    const float totalW = static_cast<float>(getWidth());
    keyW = (totalW - static_cast<float>(kNumKeys - 1) * gap) / static_cast<float>(kNumKeys);
    keyH = static_cast<float>(getHeight());
}

juce::Rectangle<float> TrigGrid::keyBounds(int index) const
{
    return juce::Rectangle<float>(static_cast<float>(index) * (keyW + gap), 0.0f, keyW, keyH);
}

int TrigGrid::keyAt(juce::Point<float> p) const
{
    for (int i = 0; i < kNumKeys; ++i)
        if (keyBounds(i).contains(p))
            return i;
    return -1;
}

void TrigGrid::mouseDown(const juce::MouseEvent& e)
{
    const int idx = keyAt(e.position);
    if (idx < 0)
        return;

    if (e.mods.isRightButtonDown())
    {
        if (onStepFxToggled)
            onStepFxToggled(idx);
    }
    else
    {
        if (onStepClicked)
            onStepClicked(idx);
    }
}

void TrigGrid::paint(juce::Graphics& g)
{
    using LAF = DayRuinerLookAndFeel;

    for (int i = 0; i < kNumKeys; ++i)
    {
        const auto kb = keyBounds(i);
        const bool active = stepActive[i];
        const bool blinking = active && stepFx[i] && ! blinkOn;

        // Key body.
        g.setColour(active ? LAF::amberDim.withAlpha(0.55f) : LAF::keyOff);
        g.fillRoundedRectangle(kb, 7.0f);

        if (active)
        {
            // Amber backlight, dimmed on the blink's off-phase.
            const float glow = blinking ? 0.30f : 1.0f;
            LAF::paintAmberGlow(g, kb.getCentre(), keyW * 0.75f, glow);

            juce::ColourGradient fill(LAF::amberBright.withAlpha(0.95f * glow), kb.getCentreX(),
                                      kb.getY() + 6.0f,
                                      LAF::amber.withAlpha(0.85f * glow), kb.getCentreX(),
                                      kb.getBottom(), false);
            g.setGradientFill(fill);
            const auto inner = kb.reduced(3.0f);
            g.fillRoundedRectangle(inner, 5.0f);
        }

        // Bevel edge.
        g.setColour(juce::Colour(0xff000000).withAlpha(0.6f));
        g.drawRoundedRectangle(kb, 7.0f, 1.0f);
        g.setColour(juce::Colours::white.withAlpha(active ? 0.10f : 0.04f));
        g.drawRoundedRectangle(kb.reduced(1.0f), 6.0f, 1.0f);

        // Playhead: thin bright outline on the sounding step.
        if (i == playStep)
        {
            g.setColour(LAF::amberBright.withAlpha(0.95f));
            g.drawRoundedRectangle(kb.expanded(1.5f), 8.0f, 2.0f);
        }
    }
}
