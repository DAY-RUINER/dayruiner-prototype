#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>

// Saturator — 6 saturation algorithms with drive, dry/wet mix and output trim.
// All state is per-instance and per-channel; process() does no allocation,
// takes no locks and uses no static locals, so it is safe on the audio thread.
class Saturator
{
public:
    enum class Algorithm
    {
        None,       // 0
        Bitcrush,   // 1
        RateCrush,  // 2
        Wavefold,   // 3
        Phase,      // 4
        Tape,       // 5
        Tube        // 6
    };
    // Enum order matches the editor combo:
    // "None, Bitcrush, Rate Crush, Wavefold, Phase, Tape, Tube"

    void prepare (double sampleRate);
    void reset();
    void setAlgorithm (Algorithm a);
    void setDrive (float driveDb);   // 0..24
    void setMix (float mix01);       // 0..1
    void process (juce::AudioBuffer<float>& buffer); // in-place

private:
    Algorithm algorithm = Algorithm::None;
    double sampleRate = 44100.0;
    float driveDb = 0.0f;
    float mix = 1.0f;

    // All-pass state for the Phase algorithm.
    // BUG FIX vs sketch: the sketch used `static float lastIn/lastOut` inside
    // the per-sample loop — shared across channels, never reset, not
    // thread-safe. These member arrays are indexed per channel (modulo 2) and
    // cleared by reset().
    float phaseStateIn[2] = {};
    float phaseStateOut[2] = {};
};
