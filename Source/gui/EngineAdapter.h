#pragma once

#include <JuceHeader.h>
#include "DayRuinerPresets.h"

// ============================================================================
// EngineAdapter — abstract view interfaces between the GUI and the engine.
//
// The editor is written against these interfaces, never against concrete
// engine classes, so the GUI drops straight into the real plugin:
//
//   ISequencerView  -> DayRuiner::StepSequencer
//       getNumTracks()  = StepSequencer::NUM_TRACKS (8)
//       getNumSteps()   = StepSequencer::NUM_STEPS (64)
//       getStep(t, s)   = Step{.active} from getStep(t, s).active
//       FX flag         = does the step carry FX parameter locks?
//                         (e.g. locks.hasLock("DELAY_MIX") ||
//                                locks.hasLock("SAT_MIX")  ||
//                                locks.hasLock("REV_MIX"))
//       getCurrentStep()= sequencer.getCurrentStep()
//       setStepActive() = getStep(t, s).active = value   (editor thread)
//       setStepFx()     = set or clear an FX lock, e.g. a "DELAY_MIX" lock
//                         at the step's current value (or clear it)
//
//   IGrainFieldSource -> GranularEngine
//       hasSample()     = topSource.getIsLoaded()
//       peaks           = min/max columns resampled from the SampleBuffer's
//                         channel 0 (use readSample at fractional positions)
//       playhead        = granular playhead position, normalised 0..1
//       grain windows   = live grains from the GrainSchedulers:
//                         { readPosition / numSamples, length / numSamples }
//       output peaks    = post-recombination monitor tap (engine team: add a
//                         small ring buffer on the granular mix bus)
//
//   IPresetStore -> preset manager
//       Wraps kDrumPresets[111] / kSynthPresets[111] from DayRuinerPresets.h.
//       The real implementation applies the preset to the APVTS on select
//       (see applyDayRuinerPresetToApvts below for the exact loader recipe).
//
// ---- Drum & mixer parameter contract (DRUMS / MIXER pages) -----------------
// The DRUMS page is track-centric: the 8 encoders edit the SELECTED track.
// The MIXER page shows all 8 track levels at once. Both pages are APVTS
// backed — no extra view interface is needed; the editor reads/writes these
// IDs directly (the demo layout implements them all; the real engine must
// add them per engine-fx/INTEGRATION.md §4b).
//
//   TRKn_ALGO      choice  14 drum algorithms (see kDrumAlgoChoices in
//                          DayRuinerEditor.cpp), default per track:
//                          1 AnalogKick, 2 AnalogSnare, 3 AnalogHat,
//                          4 Clap, 5 Tom, 6 Rimshot, 7 Crash, 8 NoiseSweep
//   TRKn_TUNE      float   -24..+24 semitones, default 0
//   TRKn_DECAY     float   0.01..4.0 s, default 0.5
//   TRKn_LEVEL     float   0..1, default 0.8   (shared by DRUMS enc 4 and
//                          the MIXER page encoders)
//   TRKn_PAN       float   -1..1, default 0
//   TRKn_HUMANIZE  float   0..1 timing+velocity slop, default 0.15
//   TRKn_CHOKE     choice  OFF/A/B/C/D, default OFF
//   TRKn_DRIVE     float   0..24 dB, default 0
//   TRKn_MUTE      bool    default false (false = audible)
//
// Track-button behaviour contract (page-dependent):
//   DRUMS (and GRANULAR/SYNTH/FX): tap SELECTS the track. The trig grid
//       shows that track's 16-step pattern; the DRUMS encoders edit it.
//   MIXER: tap TOGGLES TRKn_MUTE. Amber lit = audible, dim = muted.
//       The OLED shows the last touch ("TRK 4 LEVEL 0.80" / "TRK 4 MUTED").
//       The editor also shows the hint "MIXER · TAP = MUTE" until touched.
// ============================================================================

struct StepView
{
    bool active = false;
    bool fx = false; // step has FX engaged (blinks on the trig grid)
};

class ISequencerView
{
public:
    virtual ~ISequencerView() = default;
    virtual int getNumTracks() const = 0;
    virtual int getNumSteps() const = 0;
    virtual int getPatternLength() const = 0; // <= getNumSteps()
    virtual int getCurrentStep() const = 0;   // audio-thread playhead
    virtual StepView getStep(int track, int step) const = 0;
    virtual void setStepActive(int track, int step, bool active) = 0;
    virtual void setStepFx(int track, int step, bool fx) = 0;
};

struct GrainFieldGrain
{
    float start01 = 0.0f;   // grain window start, normalised 0..1
    float end01 = 0.0f;     // grain window end,   normalised 0..1
    float intensity = 1.0f;  // 0..1 brightness
};

class IGrainFieldSource
{
public:
    virtual ~IGrainFieldSource() = default;
    virtual bool hasSample() const = 0;
    virtual int getNumBins() const = 0; // preferred peak-column count
    virtual void getSourcePeaks(float* minOut, float* maxOut, int numBins) const = 0;
    virtual void getOutputPeaks(float* minOut, float* maxOut, int numBins) const = 0;
    virtual int getNumGrains() const = 0;
    virtual GrainFieldGrain getGrain(int index) const = 0;
    virtual float getPlayhead01() const = 0; // -1 to hide
    virtual juce::String getSampleName() const { return {}; } // empty = none/factory
};

struct PresetInfo
{
    juce::String name;
    juce::String engine;     // "drums" | "synth"
    juce::String category;
    juce::String description;
    int index = -1;          // 0..221 across both banks
};

class IPresetStore
{
public:
    virtual ~IPresetStore() = default;
    virtual int getNumPresets() const = 0;
    virtual PresetInfo getPreset(int index) const = 0;
    virtual int getCurrentIndex() const = 0;
    virtual void selectPreset(int index) = 0; // real impl: applies to APVTS
};

// Applies a factory preset to the parameter tree, field by field.
// Member names map 1:1 to APVTS parameter IDs (see DayRuinerPresets.h).
inline void applyDayRuinerPresetToApvts(juce::AudioProcessorValueTreeState& apvts,
                                        const DayRuinerPreset& p)
{
    auto setFloat = [&](const char* id, float v)
    {
        if (auto* param = apvts.getParameter(juce::String(id)))
            param->setValueNotifyingHost(param->convertTo0to1(v));
    };
    auto setInt = [&](const char* id, int v) { setFloat(id, static_cast<float>(v)); };

    setInt  ("SYNTH_ALGO",          p.synthAlgo);
    setFloat("SYNTH_DECAY",         p.synthDecay);
    setFloat("GRAIN_TOP_LENGTH",    p.grainTopLength);
    setFloat("GRAIN_TOP_FREQ",      p.grainTopFreq);
    setFloat("GRAIN_BOTTOM_LENGTH", p.grainBottomLength);
    setFloat("GRAIN_BOTTOM_FREQ",   p.grainBottomFreq);
    setFloat("GRAIN_BLEND",         p.grainBlend);
    setInt  ("SAT_ALGO",            p.satAlgo);
    setFloat("SAT_DRIVE",           p.satDrive);
    setFloat("SAT_MIX",             p.satMix);
    setInt  ("DELAY_DIV_NUM",       p.delayDivNum);
    setInt  ("DELAY_DIV_DEN",       p.delayDivDen);
    setFloat("DELAY_FB",            p.delayFb);
    setFloat("DELAY_MIX",           p.delayMix);
    setFloat("REV_ROOM",            p.revRoom);
    setFloat("REV_DAMP",            p.revDamp);
    setFloat("REV_MIX",             p.revMix);
    setInt  ("SEQ_LENGTH",          p.seqLength);
    setFloat("SEQ_SWING",           p.seqSwing);
}
