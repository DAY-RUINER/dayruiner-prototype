#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>

// Thin wrapper around juce::dsp::Convolution for reverb-style IR processing.
// process() is a no-op bypass until an impulse response is loaded successfully.
class FFTConvolver
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec);
    bool loadImpulseResponse(const juce::File& irFile); // false = stays bypassed
    void setMix(float mix01);
    void process(juce::AudioBuffer<float>& buffer); // no-op bypass if no IR loaded
    void reset();

private:
    juce::dsp::Convolution convolution;
    juce::AudioBuffer<float> wetBuffer; // pre-sized scratch (no alloc on audio thread)
    juce::dsp::ProcessSpec storedSpec {};
    bool irLoaded = false;
    float mix = 1.0f;
};
