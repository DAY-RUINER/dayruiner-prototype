#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>

// EuclideanDelay — tempo-synced delay whose feedback path is gated by a
// Bjorklund (Euclidean) rhythm pattern. The pattern step advances once per
// delay period, not per sample (the sketch's audio-rate stepping was a bug).
// process() does no allocation, takes no locks and uses no static locals.
class EuclideanDelay
{
public:
    void prepare (double sampleRate);
    void reset();
    void setBpm (double bpm);
    void setDivision (int num, int denom); // noteSeconds = (60/bpm) * (4.0*num/denom)
    void setFeedback (float fb01);         // clamped 0..0.95
    void setMix (float mix01);
    void setEuclideanPattern (int pulses, int steps); // Bjorklund; empty pattern = gate always open
    void process (juce::AudioBuffer<float>& buffer);  // in-place

private:
    void buildPattern (int pulses, int steps); // Bjorklund bucket algorithm

    double sampleRate = 44100.0;
    double bpm = 120.0;
    int divisionNum = 1;
    int divisionDenom = 4;
    float feedback = 0.4f;
    float mix = 0.3f;

    juce::AudioBuffer<float> delayBuffer; // 2 channels, 2 seconds + margin
    int bufSize = 0;
    int writePosition = 0;

    // Euclidean gating state: the step advances once per delay period.
    int samplesIntoPeriod = 0;
    int euclideanStep = 0;
    std::vector<bool> pattern; // empty = gate always open
};
