#pragma once

#include <JuceHeader.h>

#include <map>

namespace DayRuiner
{

// Per-step parameter locks: overrides for named parameters (e.g. synth params)
// that apply only while this step's trig is sounding.
// Lives inside Step, so it is copied whenever steps/patterns are copied.
class ParameterLock
{
public:
    void setLock(const juce::String& id, float value);
    void clearLock(const juce::String& id);
    bool hasLock(const juce::String& id) const;
    float getValue(const juce::String& id, float fallback) const;
    const std::map<juce::String, float>& getAll() const;

    // Message thread only. Step objects (and their locks) are copied on the
    // audio thread at pattern-cycle boundaries, so serialization must never
    // run on the audio thread.
    juce::ValueTree toValueTree() const;   // "ParameterLock" with "Lock" children (id/value props)
    void fromValueTree(const juce::ValueTree& tree);

private:
    // std::map, not unordered_map: juce::String has no std::hash specialization.
    std::map<juce::String, float> locks;
};

} // namespace DayRuiner
