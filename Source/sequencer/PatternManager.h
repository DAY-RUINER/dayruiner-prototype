#pragma once

#include <JuceHeader.h>

#include <array>
#include <atomic>
#include <utility>
#include <vector>

#include "StepSequencer.h"

namespace DayRuiner
{

// One stored pattern: name, length, swing, and a full 8x64 step grid.
struct Pattern
{
    juce::String name = "Init";
    int length = StepSequencer::NUM_STEPS;
    float swing = 0.0f;
    std::array<std::array<Step, StepSequencer::NUM_STEPS>, StepSequencer::NUM_TRACKS> tracks;

    // Message thread only.
    juce::ValueTree toValueTree() const;
    void fromValueTree(const juce::ValueTree& tree);
};

// One song-mode entry: play bank/pattern for `repeats` cycles, then move on.
struct SongSlot
{
    int bank = 0;
    int pattern = 0;
    int repeats = 1;
};

// Owns all 128 patterns (8 banks x 16) plus chain and song mode.
//
// Threading model:
//  - prepare(), getPattern(), toValueTree()/fromValueTree() and the
//    chain/song configuration calls run on the message thread. Configure
//    chain/song while the transport is stopped: advanceOnCycle() reads them
//    on the audio thread.
//  - requestPattern() is lock-free (single atomic) and may be called from
//    the editor at any time; the switch itself happens at the next cycle
//    boundary inside advanceOnCycle().
//  - applyPendingIfNeeded() / advanceOnCycle() run on the audio thread at a
//    cycle boundary. A pattern swap copies Step objects (whose ParameterLocks
//    own std::maps), which can allocate -- accepted in v1: it happens at most
//    once per cycle boundary and only when a switch was requested. A
//    lock-free double-buffered swap is future work.
class PatternManager
{
public:
    static constexpr int NUM_BANKS = 8;
    static constexpr int PATTERNS_PER_BANK = 16;
    static constexpr int TOTAL_PATTERNS = NUM_BANKS * PATTERNS_PER_BANK; // 128

    // Resets all 128 patterns to defaults (named A01..H16), clears any pending
    // switch, chain and song. Call once before use, on the message thread.
    void prepare();

    Pattern& getPattern(int bank, int pattern);             // indices clamped
    const Pattern& getPattern(int bank, int pattern) const; // indices clamped

    int getBankIndex() const;    // current bank, atomic snapshot
    int getPatternIndex() const; // current pattern, atomic snapshot

    // Editor thread: request a switch (bank 0..7, pattern 0..15, clamped).
    // Takes effect at the next cycle boundary.
    void requestPattern(int bank, int pattern);

    // Audio thread: call at a cycle boundary. First saves the sequencer's
    // live tracks into the current pattern (preserving editor edits), then
    // loads the pending pattern into the sequencer. Returns true if a swap
    // happened. No-op (returns false) when nothing was requested.
    bool applyPendingIfNeeded(StepSequencer& seq);

    void setChain(const std::vector<std::pair<int, int>>& bankPatternPairs);
    void clearChain();
    bool isChainActive() const;

    void setSongMode(const std::vector<SongSlot>& song);
    void clearSong();
    bool isSongActive() const;

    // Called by StepSequencer when a cycle wraps. Advances song/chain
    // positions (requesting the next pattern as needed) and then applies any
    // pending switch. Returns true if the pattern changed.
    // NOTE: assumes the chain/song starts on the currently playing pattern:
    // positions advance from slot 0, so slot 0's repeats are counted from the
    // moment song/chain mode was engaged.
    bool advanceOnCycle(StepSequencer& seq);

    juce::ValueTree toValueTree() const;         // message thread only (dumps all 128 patterns)
    void fromValueTree(const juce::ValueTree& tree); // message thread only

private:
    static constexpr int indexFor(int bank, int pattern) { return bank * PATTERNS_PER_BANK + pattern; }

    std::array<Pattern, TOTAL_PATTERNS> patterns;

    // Single atomics (bank * 16 + pattern): a switch request can never be
    // observed as a torn bank-from-new/pattern-from-old pair.
    std::atomic<int> currentIndex { 0 };
    std::atomic<int> pendingIndex { -1 }; // -1 = no switch requested

    // Chain / song state. Configured from the message thread while stopped;
    // only advanceOnCycle() touches the positions, on the audio thread.
    std::vector<std::pair<int, int>> chain;
    size_t chainPos = 0;
    std::atomic<bool> chainActive { false };

    std::vector<SongSlot> song;
    size_t songPos = 0;
    int songRepeatCount = 0;
    std::atomic<bool> songActive { false };
};

} // namespace DayRuiner
