#include "PatternManager.h"

namespace DayRuiner
{

juce::ValueTree Pattern::toValueTree() const
{
    juce::ValueTree tree("Pattern");
    tree.setProperty("name", name, nullptr);
    tree.setProperty("length", length, nullptr);
    tree.setProperty("swing", static_cast<double>(swing), nullptr);

    for (int t = 0; t < StepSequencer::NUM_TRACKS; ++t)
    {
        juce::ValueTree trackTree("Track");
        trackTree.setProperty("index", t, nullptr);

        for (int s = 0; s < StepSequencer::NUM_STEPS; ++s)
            trackTree.appendChild(stepToValueTree(tracks[static_cast<size_t>(t)][static_cast<size_t>(s)], s), nullptr);

        tree.appendChild(trackTree, nullptr);
    }

    return tree;
}

void Pattern::fromValueTree(const juce::ValueTree& tree)
{
    if (! tree.hasType("Pattern"))
        return;

    name = tree.getProperty("name", "Init").toString();
    length = juce::jlimit(1, StepSequencer::NUM_STEPS,
                          static_cast<int>(tree.getProperty("length", StepSequencer::NUM_STEPS)));
    swing = juce::jlimit(0.0f, 0.75f,
                         static_cast<float>(static_cast<double>(tree.getProperty("swing", 0.0))));

    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        const juce::ValueTree trackTree = tree.getChild(i);

        if (! trackTree.hasType("Track"))
            continue;

        const int t = static_cast<int>(trackTree.getProperty("index", -1));

        if (t < 0 || t >= StepSequencer::NUM_TRACKS)
            continue;

        for (int j = 0; j < trackTree.getNumChildren(); ++j)
        {
            const juce::ValueTree stepTree = trackTree.getChild(j);

            if (! stepTree.hasType("Step"))
                continue;

            const int s = static_cast<int>(stepTree.getProperty("index", -1));

            if (s < 0 || s >= StepSequencer::NUM_STEPS)
                continue;

            stepFromValueTree(stepTree, tracks[static_cast<size_t>(t)][static_cast<size_t>(s)]);
        }
    }
}

void PatternManager::prepare()
{
    for (int b = 0; b < NUM_BANKS; ++b)
    {
        for (int p = 0; p < PATTERNS_PER_BANK; ++p)
        {
            Pattern& pat = patterns[(size_t) indexFor(b, p)];
            pat = Pattern();
            pat.name = juce::String::formatted("%c%02d", 'A' + b, p + 1); // A01 .. H16
        }
    }

    currentIndex.store(0);
    pendingIndex.store(-1);
    clearChain();
    clearSong();
}

Pattern& PatternManager::getPattern(int bank, int pattern)
{
    const int b = juce::jlimit(0, NUM_BANKS - 1, bank);
    const int p = juce::jlimit(0, PATTERNS_PER_BANK - 1, pattern);
    return patterns[(size_t) indexFor(b, p)];
}

const Pattern& PatternManager::getPattern(int bank, int pattern) const
{
    const int b = juce::jlimit(0, NUM_BANKS - 1, bank);
    const int p = juce::jlimit(0, PATTERNS_PER_BANK - 1, pattern);
    return patterns[(size_t) indexFor(b, p)];
}

int PatternManager::getBankIndex() const
{
    return currentIndex.load() / PATTERNS_PER_BANK;
}

int PatternManager::getPatternIndex() const
{
    return currentIndex.load() % PATTERNS_PER_BANK;
}

void PatternManager::requestPattern(int bank, int pattern)
{
    const int b = juce::jlimit(0, NUM_BANKS - 1, bank);
    const int p = juce::jlimit(0, PATTERNS_PER_BANK - 1, pattern);
    pendingIndex.store(indexFor(b, p));
}

bool PatternManager::applyPendingIfNeeded(StepSequencer& seq)
{
    const int pending = pendingIndex.load();

    if (pending < 0)
        return false;

    // FIRST: preserve the live sequencer state (including editor edits made
    // during playback) into the outgoing pattern...
    Pattern& current = patterns[(size_t) currentIndex.load()];
    current.tracks = seq.tracks; // friend access: PatternManager is a friend of StepSequencer
    current.length = seq.getPatternLength();
    current.swing = seq.getSwing();

    // ...THEN load the incoming pattern into the sequencer.
    const Pattern& next = patterns[(size_t) pending];
    seq.tracks = next.tracks;
    seq.setPatternLength(next.length);
    seq.setSwing(next.swing);

    currentIndex.store(pending);
    pendingIndex.store(-1);

    seq.reset(); // restart the new pattern at step 0, cycle 0
    return true;
}

void PatternManager::setChain(const std::vector<std::pair<int, int>>& bankPatternPairs)
{
    chain = bankPatternPairs;
    chainPos = 0;
    chainActive.store(! chain.empty());
}

void PatternManager::clearChain()
{
    chain.clear();
    chainPos = 0;
    chainActive.store(false);
}

bool PatternManager::isChainActive() const
{
    return chainActive.load();
}

void PatternManager::setSongMode(const std::vector<SongSlot>& slots)
{
    song = slots;

    for (auto& slot : song)
    {
        slot.bank = juce::jlimit(0, NUM_BANKS - 1, slot.bank);
        slot.pattern = juce::jlimit(0, PATTERNS_PER_BANK - 1, slot.pattern);
        slot.repeats = juce::jmax(1, slot.repeats);
    }

    songPos = 0;
    songRepeatCount = 0;
    songActive.store(! song.empty());
}

void PatternManager::clearSong()
{
    song.clear();
    songPos = 0;
    songRepeatCount = 0;
    songActive.store(false);
}

bool PatternManager::isSongActive() const
{
    return songActive.load();
}

bool PatternManager::advanceOnCycle(StepSequencer& seq)
{
    if (songActive.load() && ! song.empty())
    {
        ++songRepeatCount;

        if (songRepeatCount >= song[songPos % song.size()].repeats)
        {
            songRepeatCount = 0;
            songPos = (songPos + 1) % song.size();

            const SongSlot& next = song[songPos];
            requestPattern(next.bank, next.pattern);
        }
    }
    else if (chainActive.load() && ! chain.empty())
    {
        chainPos = (chainPos + 1) % chain.size();
        requestPattern(chain[chainPos].first, chain[chainPos].second);
    }

    return applyPendingIfNeeded(seq);
}

juce::ValueTree PatternManager::toValueTree() const
{
    juce::ValueTree tree("PatternManager");
    tree.setProperty("currentBank", getBankIndex(), nullptr);
    tree.setProperty("currentPattern", getPatternIndex(), nullptr);

    for (int i = 0; i < TOTAL_PATTERNS; ++i)
        tree.appendChild(patterns[(size_t) i].toValueTree(), nullptr);

    return tree;
}

void PatternManager::fromValueTree(const juce::ValueTree& tree)
{
    if (! tree.hasType("PatternManager"))
        return;

    int loaded = 0;

    for (int i = 0; i < tree.getNumChildren() && loaded < TOTAL_PATTERNS; ++i)
    {
        const juce::ValueTree child = tree.getChild(i);

        if (! child.hasType("Pattern"))
            continue;

        patterns[(size_t) loaded].fromValueTree(child);
        ++loaded;
    }

    const int b = juce::jlimit(0, NUM_BANKS - 1, static_cast<int>(tree.getProperty("currentBank", 0)));
    const int p = juce::jlimit(0, PATTERNS_PER_BANK - 1, static_cast<int>(tree.getProperty("currentPattern", 0)));
    currentIndex.store(indexFor(b, p));
    pendingIndex.store(-1);
}

} // namespace DayRuiner
