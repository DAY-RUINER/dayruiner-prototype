#pragma once

#include <JuceHeader.h>

#include <array>
#include <atomic>

#include "TrigCondition.h"
#include "ParameterLock.h"

namespace DayRuiner
{

class PatternManager; // owned elsewhere; attached via setPatternManager()

// One sequencer step. Plain data: copied freely between the live tracks and
// stored patterns (copies happen on the audio thread, but only at pattern-
// cycle boundaries when a pattern switch was requested).
struct Step
{
    bool active = false;
    int midiNote = 36;
    float velocity = 1.0f;
    float probability = 1.0f; // legacy per-step probability (used unless the Probability condition owns it)
    TrigConditionData condition;
    ParameterLock locks;

    // Stored and serialized, but NOT applied in v1 (no micro-timing or
    // retrigger engine yet).
    int microTimingTicks = 0;
    int retrigCount = 0;
    int retrigRateDiv = 1;
};

// One trig that fired on a step boundary. The audio consumer must use it
// immediately: `step` points into the sequencer's live track storage and is
// only valid until the next pattern swap.
struct FiredTrig
{
    bool fired = false;
    int trackIndex = -1;
    int stepIndex = -1;
    int midiNote = 36;
    float velocity = 1.0f;
    const Step* step = nullptr;
};

// Message thread only. Shared by StepSequencer and Pattern serialization.
inline juce::ValueTree stepToValueTree(const Step& step, int index)
{
    juce::ValueTree tree("Step");
    tree.setProperty("index", index, nullptr);
    tree.setProperty("active", step.active, nullptr);
    tree.setProperty("midiNote", step.midiNote, nullptr);
    tree.setProperty("velocity", static_cast<double>(step.velocity), nullptr);
    tree.setProperty("probability", static_cast<double>(step.probability), nullptr);
    tree.setProperty("condType", static_cast<int>(step.condition.type), nullptr);
    tree.setProperty("condProbability", static_cast<double>(step.condition.probability), nullptr);
    tree.setProperty("condA", step.condition.aValue, nullptr);
    tree.setProperty("condB", step.condition.bValue, nullptr);
    tree.setProperty("microTimingTicks", step.microTimingTicks, nullptr);
    tree.setProperty("retrigCount", step.retrigCount, nullptr);
    tree.setProperty("retrigRateDiv", step.retrigRateDiv, nullptr);
    tree.appendChild(step.locks.toValueTree(), nullptr);
    return tree;
}

// Message thread only. Resets the step to defaults first, then applies the
// stored properties. Runtime condition state (currentCycle, fired flags) is
// deliberately not serialized; it is rebuilt by reset()/the cycle logic.
inline void stepFromValueTree(const juce::ValueTree& tree, Step& step)
{
    step = Step();

    step.active = static_cast<bool>(tree.getProperty("active", false));
    step.midiNote = static_cast<int>(tree.getProperty("midiNote", 36));
    step.velocity = static_cast<float>(static_cast<double>(tree.getProperty("velocity", 1.0)));
    step.probability = static_cast<float>(static_cast<double>(tree.getProperty("probability", 1.0)));

    const int condType = static_cast<int>(tree.getProperty("condType", 0));
    step.condition.type = (condType >= 0 && condType <= static_cast<int>(TrigCondition::NotFill))
                              ? static_cast<TrigCondition>(condType)
                              : TrigCondition::Always;

    step.condition.probability = static_cast<float>(static_cast<double>(tree.getProperty("condProbability", 1.0)));
    step.condition.aValue = static_cast<int>(tree.getProperty("condA", 1));
    step.condition.bValue = juce::jmax(1, static_cast<int>(tree.getProperty("condB", 1)));

    step.microTimingTicks = static_cast<int>(tree.getProperty("microTimingTicks", 0));
    step.retrigCount = static_cast<int>(tree.getProperty("retrigCount", 0));
    step.retrigRateDiv = juce::jmax(1, static_cast<int>(tree.getProperty("retrigRateDiv", 1)));

    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        const juce::ValueTree child = tree.getChild(i);

        if (child.hasType("ParameterLock"))
        {
            step.locks.fromValueTree(child);
            break;
        }
    }
}

// Elektron-style 64-step, 8-track step sequencer.
//
// Threading model:
//  - processSample() runs on the audio thread, exactly once per sample.
//    It performs no allocation and takes no locks.
//  - The editor thread talks to it through atomics (bpm, swing, pattern
//    length, fill mode, selected track, pattern-switch requests).
//  - getStep() hands out direct references to live step data so the editor
//    can tweak steps during playback; concurrent audio-thread reads are
//    benign in practice but technically racy -- prefer editing while the
//    transport is stopped for fully deterministic behaviour.
//  - toValueTree()/fromValueTree() are message-thread only; call them while
//    the transport is stopped.
class StepSequencer
{
public:
    static constexpr int NUM_STEPS = 64;
    static constexpr int NUM_TRACKS = 8;

    void prepare(double sampleRate, double hostBpm);
    void setBpm(double bpm);
    void setPatternLength(int steps); // clamped 1..64
    void setSwing(float amount);      // clamped 0..0.75
    void setFillMode(bool on);

    // Restarts at step 0, cycle 0 and zeroes the phase counter. Called on the
    // audio thread by PatternManager after a pattern swap; the editor may
    // also call it while the transport is stopped.
    void reset();

    void setSelectedTrack(int t); // clamped 0..7
    int getSelectedTrack() const;

    // Call EXACTLY once per sample on the audio thread. Returns at most one
    // queued trig per call (fired = true) until the step's queue is empty,
    // then returns { fired = false }.
    FiredTrig processSample() noexcept;

    Step& getStep(int track, int step);
    const Step& getStep(int track, int step) const;

    int getCurrentStep() const;   // atomic snapshot for the GUI
    int getCurrentCycle() const;  // atomic snapshot for the GUI
    int getPatternLength() const;
    float getSwing() const;
    bool isFillMode() const;

    // Attaches the pattern manager (may be nullptr). Set it before prepare(),
    // ideally; do not change it while the transport is running.
    void setPatternManager(PatternManager* pm);

    juce::ValueTree toValueTree() const;        // message thread only
    void fromValueTree(const juce::ValueTree& tree); // message thread only

private:
    friend class PatternManager; // needs direct track access for cycle-boundary pattern swaps

    // Samples per step, with swing: even steps get base * (1 + swing),
    // odd steps get base * (1 - swing). Recomputed for the CURRENT step every
    // time the playhead advances (never cached across a step change).
    double computeStepDurationSamples(int stepIndex) const;
    void advanceStep();   // audio thread: timing advance + cycle wrap + trig evaluation
    void fireStep(int stepIndex); // audio thread: evaluates all 8 tracks, in order, for one step

    std::array<std::array<Step, NUM_STEPS>, NUM_TRACKS> tracks;

    // Editor-thread inputs (atomics; safe to change during playback).
    std::atomic<double> sampleRate { 44100.0 };
    std::atomic<double> bpm { 120.0 };
    std::atomic<int> patternLength { NUM_STEPS };
    std::atomic<float> swing { 0.0f };
    std::atomic<bool> fillMode { false };
    std::atomic<int> selectedTrack { 0 };

    // Playhead state (atomics so the GUI can snapshot them).
    std::atomic<int> currentStep { 0 };
    std::atomic<int> currentCycle { 0 };

    PatternManager* patternManager = nullptr;

    // Audio-thread-only state: touched solely inside processSample() and the
    // functions it calls (advanceStep/fireStep), plus reset() at a cycle
    // boundary. Never touched from the editor thread while running.
    double sampleCounter = 0.0;
    double currentStepDurationSamples = 0.0;
    FiredTrig pending[NUM_TRACKS];
    int pendingCount = 0;
    int pendingReadPos = 0;
};

} // namespace DayRuiner
