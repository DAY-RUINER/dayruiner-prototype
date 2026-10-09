#include "EuclideanDelay.h"

void EuclideanDelay::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    const int size = static_cast<int> (sampleRate * 2.0) + 8; // 2 seconds, not a hardcoded rate
    delayBuffer.setSize (2, size, false, false, true);
    bufSize = size;
    reset();
}

void EuclideanDelay::reset()
{
    delayBuffer.clear();
    writePosition = 0;
    samplesIntoPeriod = 0;
    euclideanStep = 0;
}

void EuclideanDelay::setBpm (double newBpm)
{
    bpm = newBpm;
}

void EuclideanDelay::setDivision (int num, int denom)
{
    divisionNum = juce::jmax (1, num);
    divisionDenom = juce::jmax (1, denom); // guard denom >= 1
}

void EuclideanDelay::setFeedback (float fb01)
{
    feedback = juce::jlimit (0.0f, 0.95f, fb01);
}

void EuclideanDelay::setMix (float mix01)
{
    mix = juce::jlimit (0.0f, 1.0f, mix01);
}

void EuclideanDelay::setEuclideanPattern (int pulses, int steps)
{
    buildPattern (pulses, steps);
    euclideanStep = 0;
    samplesIntoPeriod = 0;
}

void EuclideanDelay::buildPattern (int pulses, int steps)
{
    pattern.clear();
    if (steps <= 0)
        return; // empty pattern: gate always open

    pattern.resize (static_cast<size_t> (steps));
    if (pulses <= 0)
    {
        std::fill (pattern.begin(), pattern.end(), false);
        return;
    }
    if (pulses >= steps)
    {
        std::fill (pattern.begin(), pattern.end(), true);
        return;
    }

    // Bjorklund via the bucket algorithm.
    int bucket = 0;
    for (int i = 0; i < steps; ++i)
    {
        bucket += pulses;
        if (bucket >= steps)
        {
            bucket -= steps;
            pattern[static_cast<size_t> (i)] = true;
        }
        else
        {
            pattern[static_cast<size_t> (i)] = false;
        }
    }
}

void EuclideanDelay::process (juce::AudioBuffer<float>& buffer)
{
    if (bufSize <= 1 || delayBuffer.getNumChannels() == 0)
        return;

    const double effectiveBpm = bpm >= 20.0 ? bpm : 20.0; // guard bpm >= 20
    const double noteSeconds = (60.0 / effectiveBpm)
        * (4.0 * static_cast<double> (divisionNum) / static_cast<double> (divisionDenom));
    const int delaySamples = juce::jlimit (1, bufSize - 1,
        static_cast<int> (noteSeconds * sampleRate));

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    const int delayChans = delayBuffer.getNumChannels();
    const float mixC = juce::jlimit (0.0f, 1.0f, mix);
    const float dryGain = 1.0f - mixC;

    for (int i = 0; i < numSamples; ++i)
    {
        // MANDATORY BUG FIX vs sketch: the sketch advanced euclideanStep once
        // per SAMPLE (gating feedback at audio rate — nonsense). The step
        // advances once per DELAY PERIOD.
        if (samplesIntoPeriod >= delaySamples)
        {
            samplesIntoPeriod = 0;
            if (! pattern.empty())
                euclideanStep = (euclideanStep + 1) % static_cast<int> (pattern.size());
        }
        ++samplesIntoPeriod;

        const bool gateOpen = pattern.empty() || pattern[static_cast<size_t> (euclideanStep)];
        const float gatedFeedback = gateOpen ? feedback : 0.0f;

        int readPos = writePosition - delaySamples;
        if (readPos < 0)
            readPos += bufSize; // delaySamples < bufSize, so one add is enough

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const int dch = juce::jmin (ch, delayChans - 1); // clamp extra channels
            const float in = buffer.getSample (ch, i);
            const float delayed = delayBuffer.getSample (dch, readPos);
            delayBuffer.setSample (dch, writePosition, in + delayed * gatedFeedback);
            buffer.setSample (ch, i, in * dryGain + delayed * mixC);
        }

        ++writePosition;
        if (writePosition >= bufSize)
            writePosition = 0;
    }
}
