#include "ParameterLock.h"

namespace DayRuiner
{

void ParameterLock::setLock(const juce::String& id, float value)
{
    if (id.isNotEmpty())
        locks[id] = value;
}

void ParameterLock::clearLock(const juce::String& id)
{
    locks.erase(id);
}

bool ParameterLock::hasLock(const juce::String& id) const
{
    return locks.find(id) != locks.end();
}

float ParameterLock::getValue(const juce::String& id, float fallback) const
{
    const auto it = locks.find(id);

    if (it != locks.end())
        return it->second;

    return fallback;
}

const std::map<juce::String, float>& ParameterLock::getAll() const
{
    return locks;
}

juce::ValueTree ParameterLock::toValueTree() const
{
    juce::ValueTree tree("ParameterLock");

    for (const auto& kv : locks)
    {
        juce::ValueTree child("Lock");
        child.setProperty("id", kv.first, nullptr);
        child.setProperty("value", static_cast<double>(kv.second), nullptr);
        tree.appendChild(child, nullptr);
    }

    return tree;
}

void ParameterLock::fromValueTree(const juce::ValueTree& tree)
{
    locks.clear();

    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        const juce::ValueTree child = tree.getChild(i);

        if (! child.hasType("Lock"))
            continue;

        const juce::String id = child.getProperty("id", "").toString();

        if (id.isNotEmpty())
            locks[id] = static_cast<float>(static_cast<double>(child.getProperty("value", 0.0)));
    }
}

} // namespace DayRuiner
