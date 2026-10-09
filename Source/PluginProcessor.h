#pragma once

#include <JuceHeader.h>

#include <juce_dsp/juce_dsp.h>

#include "dsp/GranularEngine.h"
#include "dsp/SynthEngine.h"
#include "dsp/Saturator.h"
#include "dsp/EuclideanDelay.h"
#include "dsp/FDNReverb.h"      // prototype: replaces LushReverb (see PROTOTYPE_NOTES.md)
#include "dsp/DriveProcessor.h" // prototype: 4-mode drive stage
#include "dsp/SoftClipper.h"    // prototype: master safety clipper
#include "sequencer/StepSequencer.h"
#include "sequencer/PatternManager.h"
#include "gui/PrototypeAdapters.h"

// The editor's full type lives in gui/DayRuinerEditor.h
// (included by PluginProcessor.cpp).
class DayRuinerEditor;

//==============================================================================
class DayRuinerAudioProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    DayRuinerAudioProcessor();
    ~DayRuinerAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;
    using juce::AudioProcessor::processBlock; // un-hide the double-precision overload

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    // NOTE: juce::AudioProcessor has no isSynth() in JUCE 9, so this cannot be an
    // override. It is kept as a plain query because project code asks for it.
    bool isSynth() const;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    //==============================================================================
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    //==============================================================================
    // Engine access for the editor.
    GranularEngine& getGranularEngine()                  { return granularEngine; }
    SynthEngine& getSynthEngine()                        { return synthEngine; }
    Saturator& getSaturator()                            { return saturator; }
    EuclideanDelay& getDelay()                           { return delay; }
    FDNReverb& getReverb()                               { return fdnReverb; }
    DriveProcessor& getDriveProcessor()                  { return drive; }
    SoftClipper& getSoftClipper()                        { return softClipper; }
    DayRuiner::StepSequencer& getSequencer()             { return sequencer; }
    DayRuiner::PatternManager& getPatternManager()       { return patternManager; }

    // Clip indicator: latched on the audio thread when the safety clipper
    // engages; the editor polls and clears it.
    bool consumeClipFlag() { return clipEngagedFlag.exchange (false); }

    void setMonitorInput (bool shouldMonitor)            { monitorInput = shouldMonitor; }
    bool isMonitoringInput() const                       { return monitorInput; }
    bool isLiveRecording() const                         { return granularEngine.isLiveRecording(); }

    // Loads an audio file into the granular engine's top layer. Suspends audio
    // around the load so the message-thread file read can't race the audio
    // thread's sample reads. Called from the editor's sample loader.
    bool loadTopSampleFromFile (const juce::File& f);

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

private:
    //==============================================================================
    DayRuiner::StepSequencer sequencer;
    DayRuiner::PatternManager patternManager;
    GranularEngine granularEngine;
    SynthEngine synthEngine;
    Saturator saturator;
    EuclideanDelay delay;
    FDNReverb fdnReverb;        // prototype: Householder-FDN, replaces LushReverb
    DriveProcessor drive;       // prototype: 4-mode drive stage
    SoftClipper softClipper;    // prototype: master safety clipper (always on)
    juce::dsp::Compressor<float> compressor; // prototype: simple glue compressor
                                             // driven by COMP_THRESHOLD
                                             // (full EffectsChain routing deferred)

    // GUI adapters (own the editor's view of the engine; declared after the
    // engines they reference, constructed in the .cpp init list).
    SequencerAdapter seqAdapter;
    GrainFieldAdapter grainAdapter;
    PresetStoreAdapter presetStore;

    // Latched by the audio thread when the safety clipper engages.
    std::atomic<bool> clipEngagedFlag { false };

    // Prototype: per-track voice state + note activity for the FDN auto-cutoff.
    double prototypeSampleRate = 44100.0;
    int activeNoteCount = 0; // MIDI note-ons minus note-offs (audio thread)
    juce::uint32 humanizeSeed = 0x12345678u; // LCG: lock-free humanize jitter

    // Cached per-track APVTS pointers (atomic loads on the audio thread;
    // cached once in the constructor so processBlock never allocates).
    // TRKn_PAN / TRKn_CHOKE / TRKn_DRIVE are STAGED (see PROTOTYPE_NOTES.md).
    struct TrackParamSet
    {
        std::atomic<float>* algo = nullptr;
        std::atomic<float>* tune = nullptr;
        std::atomic<float>* decay = nullptr;
        std::atomic<float>* level = nullptr;
        std::atomic<float>* humanize = nullptr;
        std::atomic<float>* mute = nullptr;
    };
    TrackParamSet trackParams[8];

    bool monitorInput = false;

    // Scratch buffers, sized once in prepareToPlay — never resized on the audio thread.
    juce::AudioBuffer<float> synthBuffer;
    juce::AudioBuffer<float> grainBuffer;
    juce::AudioBuffer<float> inputCopy;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DayRuinerAudioProcessor)
};
