#include "PluginProcessor.h"
#include "gui/DayRuinerEditor.h"

#include <cmath>

// Maps the DRUMS page's 14-way TRKn_ALGO choice to the v1 SynthEngine.
// Clap/Rimshot/Tom/Crash are new-algorithm names from the Phase-3 build and
// have no v1 voice yet: they map to their nearest v1 equivalent until the
// engine grows those algorithms (documented in PROTOTYPE_NOTES.md as STAGED).
static constexpr SynthEngine::Algorithm kTrackAlgoMap[14] =
{
    SynthEngine::Algorithm::AnalogKick,    // 0 Analog Kick
    SynthEngine::Algorithm::AnalogSnare,   // 1 Analog Snare
    SynthEngine::Algorithm::AnalogHat,     // 2 Analog Hat
    SynthEngine::Algorithm::FMKick,        // 3 FM Kick
    SynthEngine::Algorithm::FMSnare,       // 4 FM Snare
    SynthEngine::Algorithm::AnalogSnare,   // 5 Clap -> nearest v1
    SynthEngine::Algorithm::AnalogSnare,   // 6 Rimshot -> nearest v1
    SynthEngine::Algorithm::AnalogKick,    // 7 Tom -> nearest v1
    SynthEngine::Algorithm::AnalogHat,     // 8 Crash -> nearest v1
    SynthEngine::Algorithm::ModalMembrane, // 9 Modal Membrane
    SynthEngine::Algorithm::GranularPulse, // 10 Granular Pulse
    SynthEngine::Algorithm::GlitchStutter, // 11 Glitch Stutter
    SynthEngine::Algorithm::DigitalCrush,  // 12 Digital Crush
    SynthEngine::Algorithm::NoiseSweep,    // 13 Noise Sweep
};

//==============================================================================
DayRuinerAudioProcessor::DayRuinerAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout()),
      seqAdapter (sequencer),
      grainAdapter (granularEngine),
      presetStore (apvts)
{
    // Cache per-track parameter pointers once (audio thread does atomic
    // loads only; no String allocation or map lookup per block).
    for (int t = 0; t < 8; ++t)
    {
        const juce::String p = "TRK" + juce::String (t + 1) + "_";
        trackParams[t].algo     = apvts.getRawParameterValue (p + "ALGO");
        trackParams[t].tune     = apvts.getRawParameterValue (p + "TUNE");
        trackParams[t].decay    = apvts.getRawParameterValue (p + "DECAY");
        trackParams[t].level    = apvts.getRawParameterValue (p + "LEVEL");
        trackParams[t].humanize = apvts.getRawParameterValue (p + "HUMANIZE");
        trackParams[t].mute     = apvts.getRawParameterValue (p + "MUTE");
        // TRKn_PAN / TRKn_CHOKE / TRKn_DRIVE are STAGED (PROTOTYPE_NOTES.md).
    }
}

DayRuinerAudioProcessor::~DayRuinerAudioProcessor() = default;

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout DayRuinerAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // --- Granular engine ---
    layout.add (std::make_unique<juce::AudioParameterFloat> ("GRAIN_TOP_LENGTH", "Grain Top Length",
                                                             0.0f, 1.0f, 0.3f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("GRAIN_TOP_FREQ", "Grain Top Freq",
                                                             0.0f, 1.0f, 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("GRAIN_BOTTOM_LENGTH", "Grain Bottom Length",
                                                             0.0f, 1.0f, 0.3f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("GRAIN_BOTTOM_FREQ", "Grain Bottom Freq",
                                                             0.0f, 1.0f, 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("GRAIN_BLEND", "Grain Blend",
                                                             0.0f, 1.0f, 0.5f));

    // --- Synth engine ---
    // NOTE: item order must match SynthEngine::Algorithm (index 0..19).
    layout.add (std::make_unique<juce::AudioParameterChoice> ("SYNTH_ALGO", "Synth Algorithm",
                 juce::StringArray { "Analog Kick", "Analog Snare", "Analog Hat",
                                     "FM Kick", "FM Snare", "FM Pluck",
                                     "Wavetable Lead", "Wavetable Pad",
                                     "Additive Bell", "Additive Organ",
                                     "Modal Membrane", "Modal String",
                                     "Phase Distortion", "Vector Morph",
                                     "Granular Pulse", "Glitch Stutter",
                                     "Digital Crush", "Noise Sweep",
                                     "Metal Tine", "Sub Sine" }, 0));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("SYNTH_DECAY", "Synth Decay",
                                                             0.01f, 4.0f, 0.5f));

    // --- Saturator ---
    // NOTE: item order must match Saturator::Algorithm (index 0..6).
    layout.add (std::make_unique<juce::AudioParameterChoice> ("SAT_ALGO", "Saturator Algorithm",
                 juce::StringArray { "None", "Bitcrush", "Rate Crush", "Wavefold",
                                     "Phase", "Tape", "Tube" }, 0));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("SAT_DRIVE", "Saturator Drive",
                                                             0.0f, 24.0f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("SAT_MIX", "Saturator Mix",
                                                             0.0f, 1.0f, 1.0f));

    // --- Delay ---
    layout.add (std::make_unique<juce::AudioParameterInt> ("DELAY_DIV_NUM", "Delay Div Num", 1, 16, 1));
    layout.add (std::make_unique<juce::AudioParameterInt> ("DELAY_DIV_DEN", "Delay Div Den", 1, 16, 4));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("DELAY_FB", "Delay Feedback",
                                                             0.0f, 0.95f, 0.4f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("DELAY_MIX", "Delay Mix",
                                                             0.0f, 1.0f, 0.2f));

    // --- Reverb ---
    layout.add (std::make_unique<juce::AudioParameterFloat> ("REV_ROOM", "Reverb Room",
                                                             0.0f, 1.0f, 0.6f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("REV_DAMP", "Reverb Damp",
                                                             0.0f, 1.0f, 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("REV_MIX", "Reverb Mix",
                                                             0.0f, 1.0f, 0.3f));

    // --- Sequencer ---
    layout.add (std::make_unique<juce::AudioParameterInt> ("SEQ_LENGTH", "Seq Length", 1, 64, 16));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("SEQ_SWING", "Seq Swing",
                                                             0.0f, 0.75f, 0.0f));

    // --- Prototype: per-track drum voice + mixer (DRUMS / MIXER pages) ---
    // Order of drumAlgos MUST match kTrackAlgoMap in PluginProcessor.cpp
    // and kDrumAlgoChoices in gui/DayRuinerEditor.cpp.
    const juce::StringArray drumAlgos { "Analog Kick", "Analog Snare", "Analog Hat",
                                       "FM Kick", "FM Snare", "Clap", "Rimshot", "Tom",
                                       "Crash", "Modal Membrane", "Granular Pulse",
                                       "Glitch Stutter", "Digital Crush", "Noise Sweep" };
    const juce::StringArray chokeGroups { "OFF", "A", "B", "C", "D" };
    // Musical kit defaults: kick / snare / hat / clap / tom / rimshot / crash / noisesweep
    const int algoDefaults[8] = { 0, 1, 2, 5, 7, 6, 8, 13 };
    for (int t = 1; t <= 8; ++t)
    {
        const juce::String p = "TRK" + juce::String (t) + "_";
        const juce::String n = "Track " + juce::String (t) + " ";
        layout.add (std::make_unique<juce::AudioParameterChoice> (p + "ALGO", n + "Algorithm",
                                                                 drumAlgos, algoDefaults[t - 1]));
        layout.add (std::make_unique<juce::AudioParameterFloat> (p + "TUNE", n + "Tune",
            juce::NormalisableRange<float> (-24.0f, 24.0f), 0.0f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (p + "DECAY", n + "Decay",
            juce::NormalisableRange<float> (0.01f, 4.0f), 0.5f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (p + "LEVEL", n + "Level",
            juce::NormalisableRange<float> (0.0f, 1.0f), 0.8f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (p + "PAN", n + "Pan",
            juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (p + "HUMANIZE", n + "Humanize",
            juce::NormalisableRange<float> (0.0f, 1.0f), 0.15f));
        layout.add (std::make_unique<juce::AudioParameterChoice> (p + "CHOKE", n + "Choke",
                                                                 chokeGroups, 0));
        layout.add (std::make_unique<juce::AudioParameterFloat> (p + "DRIVE", n + "Drive",
            juce::NormalisableRange<float> (0.0f, 24.0f), 0.0f));
        layout.add (std::make_unique<juce::AudioParameterBool> (p + "MUTE", n + "Mute", false));
    }

    // --- Prototype: drive stage + glue compressor (FX page macros) ---
    // NOTE: item order of DRIVE_MODE must match ::DriveMode (0..3).
    layout.add (std::make_unique<juce::AudioParameterChoice> ("DRIVE_MODE", "Drive Mode",
                 juce::StringArray { "SoftClip", "HardClip", "Saturate", "Distort" }, 2));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("DRIVE_AMOUNT", "Drive Amount",
        juce::NormalisableRange<float> (0.0f, 36.0f), 6.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("COMP_THRESHOLD", "Compressor Threshold",
        juce::NormalisableRange<float> (-60.0f, 0.0f), -18.0f));

    return layout;
}

//==============================================================================
void DayRuinerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int numIn  = juce::jmax (1, getTotalNumInputChannels());
    const int numOut = juce::jmax (2, getTotalNumOutputChannels());

    // BUG FIX vs sketch: every engine is prepared HERE, exactly once.
    // The sketch called synthEngine.prepare() inside processBlock every block.
    granularEngine.prepare (sampleRate, samplesPerBlock);
    synthEngine.prepare (sampleRate, samplesPerBlock);
    saturator.prepare (sampleRate);

    // The GRAINFIELD is never dead: if the user hasn't loaded a sample yet,
    // the engine synthesizes its factory texture (rich, evolving harmonics).
    if (! granularEngine.getTopSampleBuffer().getIsLoaded())
        granularEngine.loadDefaultSample (sampleRate);

    delay.prepare (sampleRate);
    // No params were allocated for the euclidean rhythm shape (GUI work is paused),
    // so the delay runs a fixed 3-pulses-in-8-steps pattern until that UI exists.
    delay.setEuclideanPattern (3, 8);

    // Prototype: new DSP stages (see PROTOTYPE_NOTES.md).
    prototypeSampleRate = sampleRate;
    drive.prepare (sampleRate, samplesPerBlock, numOut);
    {
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sampleRate;
        spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
        spec.numChannels = static_cast<juce::uint32> (numOut);
        fdnReverb.prepare (sampleRate, samplesPerBlock, numOut);
        compressor.prepare (spec);
    }
    compressor.setRatio (4.0f);
    compressor.setAttack (10.0f);   // ms
    compressor.setRelease (100.0f); // ms
    softClipper.reset();
    activeNoteCount = 0;

    sequencer.prepare (sampleRate, 120.0);
    patternManager.prepare();
    sequencer.setPatternManager (&patternManager);

    // Pre-size scratch buffers to the maximum block size so processBlock never reallocates.
    synthBuffer.setSize (numOut, samplesPerBlock, false, false, false);
    grainBuffer.setSize (numOut, samplesPerBlock, false, false, false);
    inputCopy.setSize (numIn, samplesPerBlock, false, false, false);

    keyboardState.reset();
}

void DayRuinerAudioProcessor::releaseResources()
{
}

//==============================================================================
void DayRuinerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();

    // BUG FIX vs sketch (live-input ordering): snapshot the input BEFORE buffer.clear()
    // destroys it. The sketch cleared first, which erased the live-sampling source.
    const int snapCh = juce::jmin (inputCopy.getNumChannels(), numCh);
    for (int ch = 0; ch < snapCh; ++ch)
        inputCopy.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    buffer.clear();

    // --- Read every parameter once per block via lock-free atomic loads. ---
    const float grainTopLen    = apvts.getRawParameterValue ("GRAIN_TOP_LENGTH")->load();
    const float grainTopFreq   = apvts.getRawParameterValue ("GRAIN_TOP_FREQ")->load();
    const float grainBottomLen = apvts.getRawParameterValue ("GRAIN_BOTTOM_LENGTH")->load();
    const float grainBottomFreq= apvts.getRawParameterValue ("GRAIN_BOTTOM_FREQ")->load();
    const float blend          = apvts.getRawParameterValue ("GRAIN_BLEND")->load();
    const int synthAlgoIdx     = juce::jlimit (0, 19, juce::roundToInt (apvts.getRawParameterValue ("SYNTH_ALGO")->load()));
    const float synthDecay     = apvts.getRawParameterValue ("SYNTH_DECAY")->load();
    const int satAlgoIdx       = juce::jlimit (0, 6, juce::roundToInt (apvts.getRawParameterValue ("SAT_ALGO")->load()));
    const float satDrive       = apvts.getRawParameterValue ("SAT_DRIVE")->load();
    const float satMix         = apvts.getRawParameterValue ("SAT_MIX")->load();
    const int divNum           = juce::jlimit (1, 16, juce::roundToInt (apvts.getRawParameterValue ("DELAY_DIV_NUM")->load()));
    const int divDen           = juce::jlimit (1, 16, juce::roundToInt (apvts.getRawParameterValue ("DELAY_DIV_DEN")->load()));
    const float delayFb        = apvts.getRawParameterValue ("DELAY_FB")->load();
    const float delayMix       = apvts.getRawParameterValue ("DELAY_MIX")->load();
    const float revRoom        = apvts.getRawParameterValue ("REV_ROOM")->load();
    const float revDamp        = apvts.getRawParameterValue ("REV_DAMP")->load();
    const float revMix         = apvts.getRawParameterValue ("REV_MIX")->load();
    const int seqLen           = juce::jlimit (1, 64, juce::roundToInt (apvts.getRawParameterValue ("SEQ_LENGTH")->load()));
    const float seqSwing       = apvts.getRawParameterValue ("SEQ_SWING")->load();

    // Prototype: new stage parameters.
    const int driveModeIdx     = juce::jlimit (0, 3, juce::roundToInt (apvts.getRawParameterValue ("DRIVE_MODE")->load()));
    const float driveAmount    = apvts.getRawParameterValue ("DRIVE_AMOUNT")->load();
    const float compThreshold  = apvts.getRawParameterValue ("COMP_THRESHOLD")->load();

    // BUG FIX vs sketch: use the host tempo when available instead of hardcoded 120.
    double bpm = 120.0;
    if (auto* playhead = getPlayHead())
        if (const auto pos = playhead->getPosition())
            bpm = pos->getBpm().orFallback (120.0);

    // --- Push parameters into the engines (plain setters, no allocation). ---
    granularEngine.setGrainLengthNormalized (grainTopLen, grainBottomLen);
    granularEngine.setGrainFrequencyNormalized (grainTopFreq, grainBottomFreq);
    granularEngine.setBlend (blend);

    saturator.setAlgorithm (static_cast<Saturator::Algorithm> (satAlgoIdx));
    saturator.setDrive (satDrive);
    saturator.setMix (satMix);

    delay.setBpm (bpm);
    delay.setDivision (divNum, divDen);
    delay.setFeedback (delayFb);
    delay.setMix (delayMix);

    // Prototype: FDN reverb takes the legacy REV_* IDs (INTEGRATION.md §4b).
    // REV_ROOM 0..1 -> RT60 0.2s..30s, REV_DAMP -> damping, REV_MIX -> mix.
    FDNReverb::Params revParams;
    revParams.decaySecs = 0.2f * std::pow (150.0f, juce::jlimit (0.0f, 1.0f, revRoom));
    revParams.damping   = juce::jlimit (0.0f, 1.0f, revDamp);
    revParams.mix       = juce::jlimit (0.0f, 1.0f, revMix);
    fdnReverb.setParams (revParams);

    sequencer.setBpm (bpm);
    sequencer.setPatternLength (seqLen);
    sequencer.setSwing (seqSwing);

    // Prototype: snapshot per-track voice params once per block (atomic loads).
    struct TrackVoice
    {
        SynthEngine::Algorithm algo = SynthEngine::Algorithm::AnalogKick;
        float tuneSt = 0.0f;
        float decay = 0.5f;
        float level = 0.8f;
        float humanize = 0.15f;
        bool mute = false;
    };
    TrackVoice trackVoice[8];
    for (int t = 0; t < 8; ++t)
    {
        const auto& tp = trackParams[t];
        const int algoChoice = tp.algo != nullptr
            ? juce::jlimit (0, 13, juce::roundToInt (tp.algo->load())) : 0;
        trackVoice[t].algo = kTrackAlgoMap[algoChoice];
        if (tp.tune     != nullptr) trackVoice[t].tuneSt   = tp.tune->load();
        if (tp.decay    != nullptr) trackVoice[t].decay    = tp.decay->load();
        if (tp.level    != nullptr) trackVoice[t].level    = tp.level->load();
        if (tp.humanize != nullptr) trackVoice[t].humanize = tp.humanize->load();
        if (tp.mute     != nullptr) trackVoice[t].mute     = tp.mute->load() > 0.5f;
    }

    // --- Live sampling: feed the pre-clear input snapshot into the recorder. ---
    if (granularEngine.isLiveRecording())
        granularEngine.captureLiveInput (inputCopy, numSamples);

    // --- MIDI input (host + on-screen keyboard): sample-accurate trigger offsets. ---
    // MIDI trigs use the GLOBAL synth algorithm/decay (SYNTH page); sequencer
    // trigs below use the per-track DRUMS page voice. Note activity is tracked
    // for the reverb's auto-cutoff.
    const auto synthAlgo = static_cast<SynthEngine::Algorithm> (synthAlgoIdx);
    keyboardState.processNextMidiBuffer (midiMessages, 0, numSamples, true);

    bool noteActivityThisBlock = false;

    for (const auto meta : midiMessages)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn())
        {
            ++activeNoteCount;
            noteActivityThisBlock = true;
            synthEngine.trigger (msg.getNoteNumber(),
                                 msg.getVelocity() / 127.0f,
                                 synthAlgo, synthDecay, meta.samplePosition);
        }
        else if (msg.isNoteOff())
        {
            activeNoteCount = juce::jmax (0, activeNoteCount - 1);
        }
    }

    // --- Sequencer: trigger with per-sample offsets, then render once per block. ---
    // BUG FIX vs sketch: the sketch rendered the synth per-sample into a 1-sample
    // temp buffer (absurdly slow). Triggering with offsets + one renderInto per
    // block is sample-accurate and cheap.
    //
    // Prototype: sequencer trigs use the per-track DRUMS voice (algo/tune/
    // decay/level/mute/humanize). TRKn_PAN/CHOKE/DRIVE are STAGED.
    for (int s = 0; s < numSamples; ++s)
    {
        const DayRuiner::FiredTrig ft = sequencer.processSample();

        if (ft.fired && ft.step != nullptr)
        {
            noteActivityThisBlock = true;

            const int tr = juce::jlimit (0, 7, ft.trackIndex);
            const auto& tv = trackVoice[tr];

            if (! tv.mute)
            {
                const auto& locks = ft.step->locks;
                int note = locks.hasLock ("SYNTH_NOTE")
                    ? static_cast<int> (locks.getValue ("SYNTH_NOTE", static_cast<float> (ft.midiNote)))
                    : ft.midiNote;
                note = juce::jlimit (0, 127, note + juce::roundToInt (tv.tuneSt));

                float vel = ft.velocity * tv.level;
                int offset = s;

                if (tv.humanize > 0.001f)
                {
                    // Lock-free LCG: velocity droop + ±10 ms timing slop.
                    humanizeSeed = humanizeSeed * 1664525u + 1013904223u;
                    const float r1 = static_cast<float> (humanizeSeed >> 8)
                        * (1.0f / 16777216.0f);
                    humanizeSeed = humanizeSeed * 1664525u + 1013904223u;
                    const float r2 = static_cast<float> (humanizeSeed >> 8)
                        * (1.0f / 16777216.0f);
                    vel *= 1.0f - tv.humanize * 0.35f * r1;
                    offset = juce::jlimit (0, numSamples - 1,
                        s + juce::roundToInt ((r2 * 2.0f - 1.0f) * tv.humanize
                            * 0.010f * static_cast<float> (prototypeSampleRate)));
                }

                synthEngine.trigger (note, vel, tv.algo, tv.decay, offset);

                // Documented simplification: grain parameter-locks apply immediately to
                // the whole block rather than at the exact trigger sample (slight timing slop).
                if (locks.hasLock ("GRAIN_LENGTH"))
                {
                    const float g = locks.getValue ("GRAIN_LENGTH", 0.5f);
                    granularEngine.setGrainLengthNormalized (g, g);
                }
            }
        }
    }

    // --- Render the two voices into scratch buffers (both ADD into cleared buffers). ---
    synthBuffer.clear (0, numSamples);
    synthEngine.renderInto (synthBuffer, numSamples);

    grainBuffer.clear (0, numSamples);
    granularEngine.process (grainBuffer, numSamples);

    // --- Blend: linear crossfade, grain*(1-blend) + synth*blend (not equal-power). ---
    const float grainGain = 1.0f - blend;
    const int synthCh = synthBuffer.getNumChannels();
    const int grainCh = grainBuffer.getNumChannels();

    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* out = buffer.getWritePointer (ch);
        const auto* sIn = synthBuffer.getReadPointer (juce::jmin (ch, synthCh - 1));
        const auto* gIn = grainBuffer.getReadPointer (juce::jmin (ch, grainCh - 1));

        for (int s = 0; s < numSamples; ++s)
            out[s] = gIn[s] * grainGain + sIn[s] * blend;
    }

    // --- Optional input monitoring (post-blend, pre-FX). ---
    if (monitorInput)
    {
        const int monCh = juce::jmin (numCh, inputCopy.getNumChannels());
        for (int ch = 0; ch < monCh; ++ch)
            buffer.addFrom (ch, 0, inputCopy, ch, 0, numSamples, 0.5f);
    }

    // --- FX chain + safety gain. ---
    // Prototype order: saturator -> euclidean delay -> drive -> glue
    // compressor -> FDN reverb -> safety clipper -> master gain.
    // The full EffectsChain (per-slot Send/Bus/Master routing, extra reverb
    // params) is deferred; see PROTOTYPE_NOTES.md.
    saturator.process (buffer);
    delay.process (buffer);

    juce::dsp::AudioBlock<float> block (buffer);

    DriveParams driveParams;
    driveParams.mode = static_cast<DriveMode> (driveModeIdx);
    driveParams.driveDb = driveAmount;
    driveParams.tone = 0.5f;
    driveParams.bias = 0.0f;
    driveParams.mix = 1.0f;
    drive.process (block, driveParams);

    compressor.setThreshold (compThreshold);
    compressor.process (juce::dsp::ProcessContextReplacing<float> (block));

    fdnReverb.setNoteActive (noteActivityThisBlock || activeNoteCount > 0);
    fdnReverb.process (block);

    // Master safety clipper: always engaged; latch the flag for the UI.
    if (softClipper.process (buffer))
        clipEngagedFlag.store (true, std::memory_order_relaxed);

    buffer.applyGain (0.7f); // master gain: ALWAYS last, ALWAYS once
}

//==============================================================================
void DayRuinerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Stash live sequencer edits into the current pattern first: pattern edits
    // live in the sequencer and are only stored into the pattern on switch,
    // so without this step unsaved grid edits would be lost on save.
    auto& pat = patternManager.getPattern (patternManager.getBankIndex(),
                                           patternManager.getPatternIndex());
    for (int t = 0; t < DayRuiner::StepSequencer::NUM_TRACKS; ++t)
        for (int s = 0; s < DayRuiner::StepSequencer::NUM_STEPS; ++s)
            pat.tracks[static_cast<size_t>(t)][static_cast<size_t>(s)] = sequencer.getStep (t, s);
    pat.length = sequencer.getPatternLength();

    auto state = apvts.copyState();
    state.appendChild (sequencer.toValueTree(), nullptr);
    state.appendChild (patternManager.toValueTree(), nullptr);

    juce::MemoryOutputStream stream (destData, true);
    state.writeToStream (stream);
}

void DayRuinerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (const auto tree = juce::ValueTree::readFromData (data, static_cast<size_t>(sizeInBytes)); tree.isValid())
    {
        apvts.replaceState (tree);

        // NOTE: assumes PatternManager::toValueTree() uses the tag "PatternManager".
        if (const auto pmTree = tree.getChildWithName ("PatternManager"); pmTree.isValid())
            patternManager.fromValueTree (pmTree);

        // Reload the current pattern's steps into the live sequencer.
        const auto& pat = patternManager.getPattern (patternManager.getBankIndex(),
                                                     patternManager.getPatternIndex());
        for (int t = 0; t < DayRuiner::StepSequencer::NUM_TRACKS; ++t)
            for (int s = 0; s < DayRuiner::StepSequencer::NUM_STEPS; ++s)
                sequencer.getStep (t, s) = pat.tracks[static_cast<size_t>(t)][static_cast<size_t>(s)];

        sequencer.setPatternLength (pat.length);
        if (const auto* swing = apvts.getRawParameterValue ("SEQ_SWING"))
            sequencer.setSwing (swing->load());
        sequencer.reset();
    }
}

//==============================================================================
bool DayRuinerAudioProcessor::loadTopSampleFromFile (const juce::File& f)
{
    if (! f.existsAsFile())
        return false;

    // Structural change to the sample data: keep the audio thread parked while
    // the message thread reads + resamples the file. Brief dropout, no race.
    suspendProcessing (true);
    const bool ok = granularEngine.loadTopSample (f);
    suspendProcessing (false);
    return ok;
}

juce::AudioProcessorEditor* DayRuinerAudioProcessor::createEditor()
{
#ifdef DAYRUINER_HEADLESS_SMOKE_TEST
    return nullptr; // headless smoke test links no GUI sources (no editor defined)
#else
    auto* ed = new DayRuinerEditor (*this, apvts, seqAdapter, grainAdapter, presetStore);
    ed->setSampleLoader ([this] (const juce::File& f) { return loadTopSampleFromFile (f); });
    return ed;
#endif
}

bool DayRuinerAudioProcessor::hasEditor() const
{
    return true;
}

//==============================================================================
const juce::String DayRuinerAudioProcessor::getName() const          { return "DAY RUINER"; }
bool DayRuinerAudioProcessor::acceptsMidi() const                   { return true; }
bool DayRuinerAudioProcessor::producesMidi() const                  { return false; }
bool DayRuinerAudioProcessor::isMidiEffect() const                  { return false; }
bool DayRuinerAudioProcessor::isSynth() const                       { return true; }
double DayRuinerAudioProcessor::getTailLengthSeconds() const        { return 8.0; }

int DayRuinerAudioProcessor::getNumPrograms()                       { return 1; }
int DayRuinerAudioProcessor::getCurrentProgram()                    { return 0; }
void DayRuinerAudioProcessor::setCurrentProgram (int)               {}
const juce::String DayRuinerAudioProcessor::getProgramName (int)    { return "Default"; }
void DayRuinerAudioProcessor::changeProgramName (int, const juce::String&) {}

bool DayRuinerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Stereo in (live sampling) + stereo out only.
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

//==============================================================================
// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DayRuinerAudioProcessor();
}
