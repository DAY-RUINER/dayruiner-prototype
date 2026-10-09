#pragma once

#include <JuceHeader.h>

#include <cstdint>

namespace DayRuiner
{

// Elektron-style trig conditions: per-step rules that decide whether an
// active step actually fires when the playhead reaches it.
enum class TrigCondition : uint8_t
{
    Always = 0,   // fires every cycle
    Probability,  // fires with probability `probability`
    AB,           // fires when ((currentCycle % bValue) + 1) == aValue, e.g. 1:4
    FirstOnly,    // fires only on cycle 0
    NotFirst,     // fires on every cycle except cycle 0
    Pre,          // fires if this track's trig fired on an earlier step this cycle
    Nei,          // fires if the neighbouring track fired on an earlier step this cycle
    Fill,         // fires only while fill mode is engaged
    NotFill       // fires only while fill mode is NOT engaged
};

struct TrigConditionData
{
    // Configured by the editor (serialized with the step).
    TrigCondition type = TrigCondition::Always;
    float probability = 1.0f; // used by Probability
    int aValue = 1;           // used by AB (the "A" in A:B)
    int bValue = 1;           // used by AB (the "B" in A:B)

    // Runtime state, maintained by StepSequencer on the audio thread.
    // Never edited directly by the editor; not serialized.
    int currentCycle = 0;
    bool lastTrigFired = false;
    bool neighborTrigFired = false;

    // NOTE: called on the audio thread. Uses juce::Random::getSystemRandom()
    // directly (never a stored reference) per JUCE's thread-affinity guidance.
    bool evaluate(bool fillModeActive) const noexcept
    {
        switch (type)
        {
            case TrigCondition::Always:      return true;
            case TrigCondition::Probability: return juce::Random::getSystemRandom().nextFloat() < probability;
            case TrigCondition::AB:
            {
                const int b = bValue >= 1 ? bValue : 1;
                return ((currentCycle % b) + 1) == aValue;
            }
            case TrigCondition::FirstOnly:   return currentCycle == 0;
            case TrigCondition::NotFirst:    return currentCycle != 0;
            case TrigCondition::Pre:         return lastTrigFired;
            case TrigCondition::Nei:         return neighborTrigFired;
            case TrigCondition::Fill:        return fillModeActive;
            case TrigCondition::NotFill:     return ! fillModeActive;
        }

        return true; // unreachable: every enumerator is handled above
    }
};

} // namespace DayRuiner
