#pragma once

// ============================================================================
// PrototypeAdapters — binds the DAY RUINER GUI's abstract view interfaces
// (see gui/EngineAdapter.h) to the real v1 engine classes.
//
//   SequencerAdapter   ISequencerView  -> DayRuiner::StepSequencer
//   GrainFieldAdapter  IGrainFieldSource -> GranularEngine
//   PresetStoreAdapter IPresetStore    -> kDrumPresets/kSynthPresets
//
// Owned by DayRuinerAudioProcessor (members, declared after the engines and
// apvts) so they outlive any editor instance.
// ============================================================================

#include <JuceHeader.h>

#include "EngineAdapter.h"
#include "../sequencer/StepSequencer.h"
#include "../dsp/GranularEngine.h"

// ----------------------------------------------------------------------------
// Sequencer
// ----------------------------------------------------------------------------
class SequencerAdapter : public ISequencerView
{
public:
    explicit SequencerAdapter(DayRuiner::StepSequencer& s) : seq(s) {}

    int getNumTracks() const override { return DayRuiner::StepSequencer::NUM_TRACKS; }
    int getNumSteps() const override { return DayRuiner::StepSequencer::NUM_STEPS; }
    int getPatternLength() const override { return seq.getPatternLength(); }
    int getCurrentStep() const override { return seq.getCurrentStep(); }

    StepView getStep(int track, int step) const override
    {
        StepView v;
        if (track < 0 || track >= getNumTracks() || step < 0 || step >= getNumSteps())
            return v;
        const auto& st = seq.getStep(track, step);
        v.active = st.active;
        // FX flag = the step carries an FX parameter lock (drives the
        // trig-grid blink). NOTE (prototype): v1 applies SYNTH_NOTE and
        // GRAIN_LENGTH locks in the audio path; DELAY_MIX/SAT_MIX/REV_MIX
        // locks currently drive the blink + persist in state, and per-step
        // FX application is staged engine work (see PROTOTYPE_NOTES.md).
        v.fx = st.locks.hasLock("DELAY_MIX")
            || st.locks.hasLock("SAT_MIX")
            || st.locks.hasLock("REV_MIX");
        return v;
    }

    void setStepActive(int track, int step, bool active) override
    {
        if (track < 0 || track >= getNumTracks() || step < 0 || step >= getNumSteps())
            return;
        seq.getStep(track, step).active = active;
    }

    void setStepFx(int track, int step, bool fx) override
    {
        if (track < 0 || track >= getNumTracks() || step < 0 || step >= getNumSteps())
            return;
        auto& locks = seq.getStep(track, step).locks;
        if (fx)
            locks.setLock("DELAY_MIX", 0.35f); // audible send; persisted + blinked
        else
        {
            locks.clearLock("DELAY_MIX");
            locks.clearLock("SAT_MIX");
            locks.clearLock("REV_MIX");
        }
    }

private:
    DayRuiner::StepSequencer& seq;
};

// ----------------------------------------------------------------------------
// Grain field
// ----------------------------------------------------------------------------
class GrainFieldAdapter : public IGrainFieldSource
{
public:
    explicit GrainFieldAdapter(GranularEngine& g) : grain(g) {}

    bool hasSample() const override
    {
        return grain.getTopSampleBuffer().getIsLoaded();
    }

    juce::String getSampleName() const override
    {
        const juce::String f = grain.getTopSampleBuffer().getFileName();
        if (f.isNotEmpty())
            return f;
        return hasSample() ? juce::String("FACTORY TEXTURE") : juce::String();
    }

    int getNumBins() const override { return 240; }

    void getSourcePeaks(float* minOut, float* maxOut, int numBins) const override
    {
        const auto& src = grain.getTopSampleBuffer();
        const int n = src.getNumSamples();
        if (minOut == nullptr || maxOut == nullptr || numBins <= 0 || n <= 0)
            return;

        constexpr int kSubSamples = 12;
        for (int b = 0; b < numBins; ++b)
        {
            float mn = 0.0f, mx = 0.0f;
            for (int k = 0; k < kSubSamples; ++k)
            {
                const double pos = (static_cast<double>(b)
                    + static_cast<double>(k) / kSubSamples)
                    / static_cast<double>(numBins) * static_cast<double>(n - 1);
                const float v = src.readSample(0, pos,
                    SampleBuffer::InterpolationMode::Hermite);
                mn = juce::jmin(mn, v);
                mx = juce::jmax(mx, v);
            }
            minOut[b] = mn;
            maxOut[b] = mx;
        }
    }

    void getOutputPeaks(float* minOut, float* maxOut, int numBins) const override
    {
        // The engine's monitor tap records per-block peaks (positive only);
        // mirror them for the display's min/max columns.
        if (minOut == nullptr || maxOut == nullptr || numBins <= 0)
            return;
        juce::HeapBlock<float> peaks(static_cast<size_t>(numBins));
        grain.getOutputPeakHistory(peaks.get(), numBins);
        for (int b = 0; b < numBins; ++b)
        {
            maxOut[b] = peaks[b];
            minOut[b] = -peaks[b];
        }
    }

    int getNumGrains() const override
    {
        GrainScheduler::GrainWindow tmp[64];
        const int nTop = grain.getTopScheduler().getActiveGrainWindows(
            tmp, 64, static_cast<double>(grain.getTopSourceNumSamples()));
        const int nBot = grain.getBottomScheduler().getActiveGrainWindows(
            tmp, 64, static_cast<double>(grain.getBottomSourceNumSamples()));
        return juce::jmin(48, nTop + nBot);
    }

    GrainFieldGrain getGrain(int index) const override
    {
        GrainFieldGrain out;
        GrainScheduler::GrainWindow tmp[64];
        const int nTop = grain.getTopScheduler().getActiveGrainWindows(
            tmp, 64, static_cast<double>(grain.getTopSourceNumSamples()));
        if (index >= 0 && index < nTop)
        {
            out.start01 = static_cast<float>(tmp[index].start01);
            out.end01 = static_cast<float>(tmp[index].end01);
            out.intensity = tmp[index].intensity;
            return out;
        }
        const int botIndex = index - nTop;
        const int nBot = grain.getBottomScheduler().getActiveGrainWindows(
            tmp, 64, static_cast<double>(grain.getBottomSourceNumSamples()));
        if (botIndex >= 0 && botIndex < nBot)
        {
            out.start01 = static_cast<float>(tmp[botIndex].start01);
            out.end01 = static_cast<float>(tmp[botIndex].end01);
            out.intensity = tmp[botIndex].intensity * 0.8f; // bottom layer, slightly dimmer
        }
        return out;
    }

    float getPlayhead01() const override
    {
        if (! hasSample())
            return -1.0f;
        return static_cast<float>(grain.getTopScheduler().playheadPosition
            .load(std::memory_order_relaxed));
    }

private:
    GranularEngine& grain;
};

// ----------------------------------------------------------------------------
// Preset store (factory banks from DayRuinerPresets.h)
// ----------------------------------------------------------------------------
class PresetStoreAdapter : public IPresetStore
{
public:
    explicit PresetStoreAdapter(juce::AudioProcessorValueTreeState& a) : apvts(a) {}

    int getNumPresets() const override { return 222; }

    PresetInfo getPreset(int index) const override
    {
        PresetInfo info;
        if (const DayRuinerPreset* p = findPreset(index))
        {
            info.name = juce::String(p->name);
            info.engine = juce::String(p->engine);
            info.category = juce::String(p->category);
            info.description = juce::String(p->description);
            info.index = index;
        }
        return info;
    }

    int getCurrentIndex() const override { return currentIndex; }

    void selectPreset(int index) override
    {
        if (const DayRuinerPreset* p = findPreset(index))
        {
            applyDayRuinerPresetToApvts(apvts, *p);
            currentIndex = index;
        }
    }

private:
    const DayRuinerPreset* findPreset(int index) const
    {
        if (index >= 0 && index < 111)
            return &kDrumPresets[static_cast<size_t>(index)];
        if (index >= 111 && index < 222)
            return &kSynthPresets[static_cast<size_t>(index - 111)];
        return nullptr;
    }

    juce::AudioProcessorValueTreeState& apvts;
    int currentIndex = 0;
};
