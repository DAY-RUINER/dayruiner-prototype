#include "GrainScheduler.h"

#include <algorithm>
#include <cmath>
#include <limits>

void GrainScheduler::prepare(double newSampleRate)
{
    sampleRate = (newSampleRate > 0.0) ? newSampleRate : 44100.0;
    setGrainParameters(grainLengthMs, grainFrequencyHz, attackFraction);
    setSyncMode(syncMode, syncBpm);
    reset();
}

void GrainScheduler::reset()
{
    for (auto& g : grains)
    {
        g.active = false;
        g.elapsedSamples = 0;
        g.currentEnvelope = 0.0f;
        g.readPosition = 0.0;
        g.ringModPhase = 0.0;
    }
    samplesUntilNextFire = 0.0;
}

void GrainScheduler::setGrainParameters(float lengthMs, float frequencyHz, float attack)
{
    grainLengthMs = std::max(0.1f, lengthMs);
    grainFrequencyHz = std::max(0.01f, frequencyHz);
    attackFraction = std::min(0.99f, std::max(0.0f, attack));

    if (! syncMode)
        firePeriodSamples = sampleRate / static_cast<double>(grainFrequencyHz);
}

void GrainScheduler::setSyncMode(bool shouldSync, double hostBpm)
{
    syncMode = shouldSync;

    if (syncMode)
    {
        syncBpm = (hostBpm > 0.0) ? hostBpm : 120.0;
        const double quarterHz = syncBpm / 60.0; // guarded: syncBpm > 0
        const int division = std::max(1, static_cast<int>(std::round(
            static_cast<double>(grainFrequencyHz) / quarterHz)));
        firePeriodSamples = sampleRate * 60.0 / (syncBpm * static_cast<double>(division));
    }
    else
    {
        firePeriodSamples = sampleRate / static_cast<double>(grainFrequencyHz);
    }
}

void GrainScheduler::setPositionModulation(double normalizedOffset)
{
    positionModulation = std::min(1.0, std::max(-1.0, normalizedOffset));
}

void GrainScheduler::processBlock(int numSamples, const SampleBuffer& source,
                                  juce::AudioBuffer<float>& output,
                                  SampleBuffer::InterpolationMode interpMode,
                                  std::function<void(Grain&)> onGrainFired)
{
    const int numOutChannels = output.getNumChannels();
    if (numSamples <= 0 || numOutChannels <= 0)
        return;

    const int numSourceSamples = source.getIsLoaded() ? source.getNumSamples() : 0;

    for (int s = 0; s < numSamples; ++s)
    {
        samplesUntilNextFire -= 1.0;

        if (samplesUntilNextFire <= 0.0 && numSourceSamples > 0)
        {
            fireGrain(source, onGrainFired);
            samplesUntilNextFire += firePeriodSamples;
        }

        for (auto& g : grains)
        {
            if (! g.active)
                continue;

            for (int ch = 0; ch < numOutChannels; ++ch)
            {
                float sample = source.readSample(ch, g.readPosition, interpMode);

                // Ring modulation: depth 0 -> dry, depth 1 -> full carrier multiply.
                const float ring = (1.0f - g.ringModDepth)
                                 + g.ringModDepth * static_cast<float>(std::sin(g.ringModPhase));
                sample *= ring;

                output.addSample(ch, s, sample * g.currentEnvelope * g.peakAmplitude);
            }

            g.tick(sampleRate); // single advance per sample
        }
    }
}

void GrainScheduler::fireGrain(const SampleBuffer& source,
                               std::function<void(Grain&)>& onGrainFired)
{
    const int numSourceSamples = source.getNumSamples();
    if (numSourceSamples <= 0)
        return;

    Grain* slot = nullptr;

    // Prefer a free slot; otherwise steal the oldest grain.
    int oldestElapsed = -1;
    for (auto& g : grains)
    {
        if (! g.active)
        {
            slot = &g;
            break;
        }
        if (g.elapsedSamples > oldestElapsed)
        {
            oldestElapsed = g.elapsedSamples;
            slot = &g;
        }
    }

    if (slot == nullptr)
        return;

    Grain& g = *slot;

    const double lengthSamplesD = static_cast<double>(grainLengthMs) * 0.001 * sampleRate;
    g.totalLengthSamples = std::max(1, static_cast<int>(
        std::min(lengthSamplesD, static_cast<double>(std::numeric_limits<int>::max()))));
    g.attackFraction = attackFraction;
    g.peakAmplitude = 1.0f;
    g.elapsedSamples = 0;
    g.currentEnvelope = 0.0f;
    g.windowShape = Grain::WindowShape::Hann;

    const double numSrc = static_cast<double>(numSourceSamples);
    double pos = playheadPosition.load(std::memory_order_relaxed) * numSrc
               + positionModulation * numSrc;
    pos = std::fmod(pos, numSrc);
    if (pos < 0.0)
        pos += numSrc;
    g.readPosition = pos;

    const double rate = playbackRate.load(std::memory_order_relaxed);
    g.playbackRate = rate;
    g.ringModDepth = ringModDepth.load(std::memory_order_relaxed);
    g.ringModFrequency = 440.0 * rate;
    g.ringModPhase = 0.0;

    g.active = true;

    if (onGrainFired)
        onGrainFired(g);
}
