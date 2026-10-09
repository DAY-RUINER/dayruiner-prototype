#include "FFTConvolver.h"

#include <algorithm>
#include <cmath>

void FFTConvolver::prepare(const juce::dsp::ProcessSpec& spec)
{
    storedSpec = spec;
    convolution.prepare(spec);
    convolution.reset();

    const int numChannels = static_cast<int>(std::max<juce::uint32>(spec.numChannels, 1));
    const int maxBlock = static_cast<int>(std::max<juce::uint32>(spec.maximumBlockSize, 1));
    wetBuffer.setSize(numChannels, maxBlock);
    irLoaded = false;
}

bool FFTConvolver::loadImpulseResponse(const juce::File& irFile)
{
    irLoaded = false;

    try
    {
        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();

        std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(irFile));
        if (reader == nullptr)
            return false;

        if (reader->lengthInSamples <= 0 || reader->numChannels <= 0)
            return false;

        juce::AudioBuffer<float> irData(static_cast<int>(reader->numChannels),
                                        static_cast<int>(reader->lengthInSamples));
        reader->read(&irData, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);

        // JUCE 9's Convolution takes an AudioBuffer rvalue (or a File); the old
        // AudioBlock overload is gone.
        convolution.loadImpulseResponse(std::move(irData),
                                        storedSpec.sampleRate,
                                        juce::dsp::Convolution::Stereo::yes,
                                        juce::dsp::Convolution::Trim::no,
                                        juce::dsp::Convolution::Normalise::no);
        convolution.reset();
    }
    catch (...)
    {
        irLoaded = false;
        return false;
    }

    irLoaded = true;
    return true;
}

void FFTConvolver::setMix(float mix01)
{
    mix = std::min(1.0f, std::max(0.0f, mix01));
}

void FFTConvolver::process(juce::AudioBuffer<float>& buffer)
{
    if (! irLoaded)
        return; // bypass: no IR loaded

    const int numSamples = buffer.getNumSamples();
    const int numChannels = std::min(buffer.getNumChannels(), wetBuffer.getNumChannels());
    if (numSamples <= 0 || numChannels <= 0)
        return;
    if (numSamples > wetBuffer.getNumSamples())
        return; // safety: never process beyond the pre-sized scratch

    // Copy dry into scratch, convolve the scratch in place.
    for (int ch = 0; ch < numChannels; ++ch)
        wetBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    juce::dsp::AudioBlock<float> wetBlock(wetBuffer);
    wetBlock = wetBlock.getSubBlock(0, static_cast<size_t>(numSamples));
    juce::dsp::ProcessContextReplacing<float> context(wetBlock);
    convolution.process(context);

    // Manual wet/dry mix back into the buffer.
    const float dryGain = 1.0f - mix;
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* dry = buffer.getWritePointer(ch);
        const float* wet = wetBuffer.getReadPointer(ch);
        for (int i = 0; i < numSamples; ++i)
            dry[i] = dry[i] * dryGain + wet[i] * mix;
    }
}

void FFTConvolver::reset()
{
    convolution.reset();
    wetBuffer.clear();
}
