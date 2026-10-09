#include "StepSequencer.h"

#include "PatternManager.h"

namespace DayRuiner
{

void StepSequencer::prepare(double newSampleRate, double hostBpm)
{
    this->sampleRate.store(newSampleRate > 0.0 ? newSampleRate : 44100.0);
    setBpm(hostBpm);
    reset();
}

void StepSequencer::setBpm(double newBpm)
{
    // Guarded: a non-positive bpm would divide by zero in the timing calc.
    bpm.store(newBpm > 0.0 ? newBpm : 120.0);
}

void StepSequencer::setPatternLength(int steps)
{
    patternLength.store(juce::jlimit(1, NUM_STEPS, steps));
}

void StepSequencer::setSwing(float amount)
{
    swing.store(juce::jlimit(0.0f, 0.75f, amount));
}

void StepSequencer::setFillMode(bool on)
{
    fillMode.store(on);
}

void StepSequencer::reset()
{
    currentStep.store(0);
    currentCycle.store(0);
    sampleCounter = 0.0;
    pendingCount = 0;
    pendingReadPos = 0;

    for (auto& track : tracks)
    {
        for (auto& s : track)
        {
            s.condition.lastTrigFired = false;
            s.condition.neighborTrigFired = false;
            s.condition.currentCycle = 0;
        }
    }

    currentStepDurationSamples = computeStepDurationSamples(0);
}

void StepSequencer::setSelectedTrack(int t)
{
    selectedTrack.store(juce::jlimit(0, NUM_TRACKS - 1, t));
}

int StepSequencer::getSelectedTrack() const
{
    return selectedTrack.load();
}

double StepSequencer::computeStepDurationSamples(int stepIndex) const
{
    const double sr = sampleRate.load();
    const double b = bpm.load();
    const float sw = swing.load();

    const double baseStepSamples = sr / ((b / 60.0) * 4.0); // 16th notes
    const double swingFactor = (stepIndex % 2 == 0) ? (1.0 + (double) sw)
                                                   : (1.0 - (double) sw);

    return baseStepSamples * swingFactor;
}

FiredTrig StepSequencer::processSample() noexcept
{
    sampleCounter += 1.0;

    if (currentStepDurationSamples > 0.0 && sampleCounter >= currentStepDurationSamples)
    {
        sampleCounter -= currentStepDurationSamples;
        advanceStep();
    }

    // Serve at most one queued trig per call so the caller sees a simple
    // one-trig-per-sample stream.
    if (pendingReadPos < pendingCount)
        return pending[pendingReadPos++];

    return FiredTrig();
}

void StepSequencer::advanceStep()
{
    const int length = juce::jmax(1, patternLength.load());
    const int newStep = (currentStep.load() + 1) % length;

    if (newStep == 0)
    {
        // Cycle wrapped: bump the cycle, clear per-cycle trig state, stamp the
        // new cycle onto every step, then let the pattern manager run song /
        // chain / requested switches at this boundary.
        const int newCycle = currentCycle.load() + 1;
        currentCycle.store(newCycle);

        for (auto& track : tracks)
        {
            for (auto& s : track)
            {
                s.condition.lastTrigFired = false;
                s.condition.neighborTrigFired = false;
                s.condition.currentCycle = newCycle;
            }
        }

        // NOTE: advanceOnCycle() may swap pattern contents here. A swap copies
        // Step objects (whose ParameterLocks own std::maps), which can
        // allocate -- accepted in v1 because it happens at most once per
        // cycle boundary and only when a switch was actually requested.
        // A lock-free double-buffered swap is future work.
        if (patternManager != nullptr)
            patternManager->advanceOnCycle(*this);
    }

    currentStep.store(newStep);

    // Recompute for the CURRENT step on every advance (never reuse a stale duration).
    currentStepDurationSamples = computeStepDurationSamples(newStep);

    fireStep(newStep);
}

void StepSequencer::fireStep(int stepIndex)
{
    // A step's trig queue is always fully served before the next boundary:
    // the shortest possible step is still far longer than 8 samples.
    pendingCount = 0;
    pendingReadPos = 0;

    const bool fill = fillMode.load();

    // All 8 tracks are evaluated here, in track order, at the step boundary.
    // Evaluating them together (rather than one track per sample) is what
    // keeps the Pre/Nei flags coherent.
    for (int t = 0; t < NUM_TRACKS; ++t)
    {
        Step& step = tracks[static_cast<size_t>(t)][static_cast<size_t>(stepIndex)];

        if (! step.active)
            continue;

        if (! step.condition.evaluate(fill))
            continue;

        // Legacy per-step probability; skipped when the Probability trig
        // condition already owns the dice roll.
        if (step.condition.type != TrigCondition::Probability
            && step.probability < 1.0f
            && juce::Random::getSystemRandom().nextFloat() >= step.probability)
            continue;

        if (pendingCount < NUM_TRACKS)
        {
            FiredTrig trig;
            trig.fired = true;
            trig.trackIndex = t;
            trig.stepIndex = stepIndex;
            trig.midiNote = step.midiNote;
            trig.velocity = step.velocity;
            trig.step = &step; // valid until the next pattern swap; use immediately
            pending[pendingCount++] = trig;
        }

        step.condition.lastTrigFired = true;

        // Mark the lower neighbour so its Nei condition can see this firing.
        if (t > 0)
            tracks[static_cast<size_t>(t - 1)][static_cast<size_t>(stepIndex)].condition.neighborTrigFired = true;
    }
}

Step& StepSequencer::getStep(int track, int step)
{
    jassert(track >= 0 && track < NUM_TRACKS && step >= 0 && step < NUM_STEPS);
    return tracks[static_cast<size_t>(juce::jlimit(0, NUM_TRACKS - 1, track))]
                 [static_cast<size_t>(juce::jlimit(0, NUM_STEPS - 1, step))];
}

const Step& StepSequencer::getStep(int track, int step) const
{
    jassert(track >= 0 && track < NUM_TRACKS && step >= 0 && step < NUM_STEPS);
    return tracks[static_cast<size_t>(juce::jlimit(0, NUM_TRACKS - 1, track))]
                 [static_cast<size_t>(juce::jlimit(0, NUM_STEPS - 1, step))];
}

int StepSequencer::getCurrentStep() const
{
    return currentStep.load();
}

int StepSequencer::getCurrentCycle() const
{
    return currentCycle.load();
}

int StepSequencer::getPatternLength() const
{
    return patternLength.load();
}

float StepSequencer::getSwing() const
{
    return swing.load();
}

bool StepSequencer::isFillMode() const
{
    return fillMode.load();
}

void StepSequencer::setPatternManager(PatternManager* pm)
{
    patternManager = pm;
}

juce::ValueTree StepSequencer::toValueTree() const
{
    juce::ValueTree tree("StepSequencer");
    tree.setProperty("patternLength", patternLength.load(), nullptr);
    tree.setProperty("swing", static_cast<double>(swing.load()), nullptr);
    tree.setProperty("bpm", bpm.load(), nullptr);
    tree.setProperty("selectedTrack", selectedTrack.load(), nullptr);

    for (int t = 0; t < NUM_TRACKS; ++t)
    {
        juce::ValueTree trackTree("Track");
        trackTree.setProperty("index", t, nullptr);

        for (int s = 0; s < NUM_STEPS; ++s)
            trackTree.appendChild(stepToValueTree(tracks[static_cast<size_t>(t)][static_cast<size_t>(s)], s), nullptr);

        tree.appendChild(trackTree, nullptr);
    }

    return tree;
}

void StepSequencer::fromValueTree(const juce::ValueTree& tree)
{
    if (! tree.hasType("StepSequencer"))
        return;

    setPatternLength(static_cast<int>(tree.getProperty("patternLength", NUM_STEPS)));
    setSwing(static_cast<float>(static_cast<double>(tree.getProperty("swing", 0.0))));
    setBpm(static_cast<double>(tree.getProperty("bpm", 120.0)));
    setSelectedTrack(static_cast<int>(tree.getProperty("selectedTrack", 0)));

    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        const juce::ValueTree trackTree = tree.getChild(i);

        if (! trackTree.hasType("Track"))
            continue;

        const int t = static_cast<int>(trackTree.getProperty("index", -1));

        if (t < 0 || t >= NUM_TRACKS)
            continue;

        for (int j = 0; j < trackTree.getNumChildren(); ++j)
        {
            const juce::ValueTree stepTree = trackTree.getChild(j);

            if (! stepTree.hasType("Step"))
                continue;

            const int s = static_cast<int>(stepTree.getProperty("index", -1));

            if (s < 0 || s >= NUM_STEPS)
                continue;

            stepFromValueTree(stepTree, tracks[static_cast<size_t>(t)][static_cast<size_t>(s)]);
        }
    }

    reset();
}

} // namespace DayRuiner
