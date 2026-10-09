#pragma once

#include <JuceHeader.h>

#include <array>
#include <cmath>

// Battalion-inspired synth engine: 20 drum/synth algorithms, 16-voice
// polyphony, sample-accurate triggering. All render paths are allocation-,
// lock-, and static-free so they are safe to call on the audio thread.
class SynthEngine
{
public:
    enum class Algorithm
    {
        AnalogKick, AnalogSnare, AnalogHat,
        FMKick, FMSnare, FMPluck,
        WavetableLead, WavetablePad,
        AdditiveBell, AdditiveOrgan,
        ModalMembrane, ModalString,
        PhaseDistortion, VectorMorph,
        GranularPulse, GlitchStutter,
        DigitalCrush, NoiseSweep,
        MetalTine, SubSine
    }; // ORDER MATTERS: index must match the editor's combo box (0..19 in this order)

    struct Voice
    {
        bool active = false;
        int noteNumber = -1;
        float velocity = 1.0f;
        double phase = 0.0, phaseInc = 0.0;
        double modPhase = 0.0, modPhaseInc = 0.0;
        float modIndex = 0.0f;
        float env = 0.0f, envDecay = 0.999f;
        int startOffsetSamples = 0;   // sample-accurate trigger offset within the block
        Algorithm algo = Algorithm::AnalogKick;
        static constexpr int MAX_PARTIALS = 16;
        float partialFreqs[MAX_PARTIALS] = {};
        float partialAmps[MAX_PARTIALS] = {};
        float partialPhases[MAX_PARTIALS] = {};
        float hatLastIn = 0.0f, hatLastOut = 0.0f;  // per-voice hat filter state (bug fix: no statics)
        float noiseLpState = 0.0f;                   // per-voice lowpass state (NoiseSweep)
        float noiseLpCutoff = 0.5f;
        int stutterCounter = 0; float stutterHold = 0.0f; // GlitchStutter state
        float morphPos = 0.5f;                        // VectorMorph position
        int voiceIndex = 0;
    };

    void prepare(double sampleRate, int maxBlockSize);
    void reset(); // silence all voices
    void trigger(int midiNote, float velocity, Algorithm algo, float decayTimeSeconds, int startOffsetSamples = 0);
    // Renders ALL active voices, ADDING into output. Voices begin sounding at their startOffsetSamples.
    void renderInto(juce::AudioBuffer<float>& output, int numSamples);

private:
    static constexpr int MAX_VOICES = 16;
    static constexpr int WAVETABLE_SIZE = 2048;
    static constexpr double twoPi = 6.28318530717958647692;

    Voice voices[MAX_VOICES];
    double sampleRate = 44100.0;
    int maxBlockSize = 512;
    std::array<float, WAVETABLE_SIZE> sawTable{};
    std::array<float, WAVETABLE_SIZE> squareishTable{};
    juce::Random random;

    void buildWavetables();
    float readWavetable(const std::array<float, WAVETABLE_SIZE>& table, double phase) const;

    float renderAnalogKick(Voice& v);
    float renderAnalogSnare(Voice& v);
    float renderAnalogHat(Voice& v);
    float renderFM(Voice& v);
    float renderPartials(Voice& v); // ModalMembrane, ModalString, AdditiveBell, AdditiveOrgan
    float renderWavetableLead(Voice& v);
    float renderWavetablePad(Voice& v);
    float renderPhaseDistortion(Voice& v);
    float renderVectorMorph(Voice& v);
    float renderGranularPulse(Voice& v);
    float renderGlitchStutter(Voice& v);
    float renderDigitalCrush(Voice& v);
    float renderNoiseSweep(Voice& v);
    float renderSine(Voice& v); // SubSine
};
