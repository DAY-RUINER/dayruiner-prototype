#include "GranularEngine.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>

namespace
{
    float clamp01(float v)
    {
        return std::min(1.0f, std::max(0.0f, v));
    }

    // Log mapping: 0..1 -> lo..hi.
    float normalizedToLog(float n01, float lo, float hi)
    {
        const float n = clamp01(n01);
        return static_cast<float>(static_cast<double>(lo) * std::pow(static_cast<double>(hi) / static_cast<double>(lo),
                                                                     static_cast<double>(n)));
    }
}

void GranularEngine::prepare(double newSampleRate, int maxBlockSize)
{
    sampleRate = (newSampleRate > 0.0) ? newSampleRate : 44100.0;
    const int blockSize = std::max(1, maxBlockSize);

    topBuffer.setSize(2, blockSize);
    bottomBuffer.setSize(2, blockSize);

    topEngine.prepare(sampleRate);
    bottomEngine.prepare(sampleRate);
    applyTopGrainParams();
    applyBottomGrainParams();

    // GRAINFIELD monitor tap: start clean (atomics default-construct
    // uninitialized; prepare() is the single init point).
    for (auto& p : outputPeakHistory_)
        p.store(0.0f, std::memory_order_relaxed);
    peakWritePos_.store(0, std::memory_order_relaxed);
}

bool GranularEngine::loadTopSample(const juce::File& f)
{
    const bool ok = topSource.loadFromFile(f, sampleRate);
    if (ok)
        sampleChangedFlag.store(true);
    return ok;
}

bool GranularEngine::loadBottomSample(const juce::File& f)
{
    return bottomSource.loadFromFile(f, sampleRate);
}

void GranularEngine::loadDefaultSample(double targetSampleRate)
{
    const double sr = targetSampleRate > 0.0 ? targetSampleRate : 44100.0;
    const double durSeconds = 4.0;
    const int n = static_cast<int>(sr * durSeconds);
    if (n <= 0)
        return;

    juce::AudioBuffer<float> buf(2, n);

    // Harmonic stack (A1-ish root): smooth falloff, no fizzy top.
    struct Partial { double freq; double amp; double lfoHz; double lfoPhase; };
    const Partial partials[] = {
        { 55.0,  0.50, 0.07, 0.0 }, { 82.5,  0.34, 0.11, 1.3 },
        { 110.0, 0.40, 0.09, 2.1 }, { 146.8, 0.22, 0.13, 0.7 },
        { 165.0, 0.26, 0.08, 2.9 }, { 220.0, 0.30, 0.12, 1.8 },
        { 277.2, 0.16, 0.10, 0.4 }, { 330.0, 0.18, 0.14, 2.4 },
        { 440.0, 0.14, 0.09, 1.1 }, { 554.4, 0.08, 0.16, 3.3 },
    };

    juce::Random rng(0xD47AC3u);
    float noiseLp[2] = { 0.0f, 0.0f }; // soft noise bed, one-pole smoothed

    const double twoPi = juce::MathConstants<double>::twoPi;
    for (int ch = 0; ch < 2; ++ch)
    {
        float* d = buf.getWritePointer(ch);
        const double chPhase = (ch == 0) ? 0.0 : 0.35; // gentle stereo drift
        for (int i = 0; i < n; ++i)
        {
            const double t = static_cast<double>(i) / sr;
            double v = 0.0;
            for (const auto& p : partials)
            {
                const double lfo = 0.65 + 0.35 * std::sin(twoPi * p.lfoHz * t + p.lfoPhase + chPhase);
                v += p.amp * lfo * std::sin(twoPi * p.freq * t + chPhase * (p.freq / 55.0));
            }
            // Soft noise bed, heavily smoothed so it stays silky.
            const float nz = static_cast<float>(rng.nextDouble() * 2.0 - 1.0);
            noiseLp[ch] += 0.02f * (nz - noiseLp[ch]);
            v += 0.10 * noiseLp[ch] * (0.6 + 0.4 * std::sin(twoPi * 0.05 * t));

            // Gentle raised-cosine edges + one slow macro swell.
            const double edge = juce::jmin(1.0, juce::jmin(t, durSeconds - t) / 0.5);
            const double swell = 0.75 + 0.25 * std::sin(twoPi * 0.125 * t - 0.6);
            d[i] = static_cast<float>(v * edge * edge * swell);
        }
    }

    // Peak-normalize to a sane level for the grain engine.
    float peak = 0.0f;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < n; ++i)
            peak = juce::jmax(peak, std::abs(buf.getReadPointer(ch)[i]));
    if (peak > 0.0f)
        buf.applyGain(0.7f / peak);

    topSource.loadFromBuffer(buf, sr, sr);
    bottomSource.loadFromBuffer(buf, sr, sr);
    sampleChangedFlag.store(true);
}

void GranularEngine::applyTopGrainParams()
{
    topEngine.setGrainParameters(topGrainLengthMs, topGrainFreqHz, attackFraction);
}

void GranularEngine::applyBottomGrainParams()
{
    bottomEngine.setGrainParameters(bottomGrainLengthMs, bottomGrainFreqHz, attackFraction);
}

void GranularEngine::setGrainLengthNormalized(float top01, float bottom01)
{
    topGrainLengthMs = normalizedToLog(top01, 1.0f, 2000.0f);
    bottomGrainLengthMs = normalizedToLog(bottom01, 1.0f, 2000.0f);
    applyTopGrainParams();
    applyBottomGrainParams();
}

void GranularEngine::setGrainFrequencyNormalized(float top01, float bottom01)
{
    topGrainFreqHz = normalizedToLog(top01, 0.1f, 100.0f);
    bottomGrainFreqHz = normalizedToLog(bottom01, 0.1f, 100.0f);
    applyTopGrainParams();
    applyBottomGrainParams();
}

void GranularEngine::setBlend(float b01)
{
    blend = clamp01(b01);
}

void GranularEngine::setTopPlayhead(double n01)
{
    topEngine.playheadPosition.store(clamp01(static_cast<float>(n01)));
}

void GranularEngine::setBottomPlayhead(double n01)
{
    bottomEngine.playheadPosition.store(clamp01(static_cast<float>(n01)));
}

void GranularEngine::setTopPlaybackRate(double r)
{
    topEngine.playbackRate.store(r);
}

void GranularEngine::setBottomPlaybackRate(double r)
{
    bottomEngine.playbackRate.store(r);
}

void GranularEngine::process(juce::AudioBuffer<float>& output, int numSamples)
{
    if (numSamples <= 0)
        return;

    const int n = std::min(numSamples, topBuffer.getNumSamples());
    if (n <= 0)
        return;

    // Clear only the active region of the pre-sized scratch buffers.
    topBuffer.clear(0, n);
    topBuffer.clear(1, n);
    bottomBuffer.clear(0, n);
    bottomBuffer.clear(1, n);

    topEngine.processBlock(n, topSource, topBuffer,
                           SampleBuffer::InterpolationMode::Hermite, nullptr);
    bottomEngine.processBlock(n, bottomSource, bottomBuffer,
                              SampleBuffer::InterpolationMode::Hermite, nullptr);

    // Equal-power crossfade between layers.
    const float gainTop = static_cast<float>(std::cos(static_cast<double>(blend)
                                                        * juce::MathConstants<double>::pi * 0.5));
    const float gainBottom = static_cast<float>(std::sin(static_cast<double>(blend)
                                                           * juce::MathConstants<double>::pi * 0.5));

    const int numChannels = std::min(output.getNumChannels(), 2);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        output.addFrom(ch, 0, topBuffer, ch, 0, n, gainTop);
        output.addFrom(ch, 0, bottomBuffer, ch, 0, n, gainBottom);
    }

    // GRAINFIELD monitor tap: per-block peak of the recombined granular mix
    // into the 64-slot ring (audio thread writes, UI thread reads).
    float blockPeak = 0.0f;
    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* d = output.getReadPointer(ch);
        for (int s = 0; s < n; ++s)
        {
            const float a = std::fabs(d[s]);
            if (a > blockPeak)
                blockPeak = a;
        }
    }
    const int pos = peakWritePos_.load(std::memory_order_relaxed);
    outputPeakHistory_[static_cast<size_t>(pos)].store(blockPeak, std::memory_order_relaxed);
    peakWritePos_.store((pos + 1) % kPeakHistorySize, std::memory_order_relaxed);
}

void GranularEngine::getOutputPeakHistory(float* out, int numOut) const
{
    if (out == nullptr || numOut <= 0)
        return;

    // Chronological order: oldest -> newest. The next write position holds
    // the oldest entry.
    const int writePos = peakWritePos_.load(std::memory_order_relaxed);
    for (int i = 0; i < numOut; ++i)
    {
        const int slot = (writePos + (i * kPeakHistorySize) / numOut) % kPeakHistorySize;
        out[i] = outputPeakHistory_[static_cast<size_t>(slot)].load(std::memory_order_relaxed);
    }
}

void GranularEngine::startLiveRecording()
{
    // UI thread only: heap allocation is fine here.
    const double maxSamplesD = sampleRate * static_cast<double>(kMaxRecordSeconds);
    const int maxSamples = static_cast<int>(std::min(
        maxSamplesD, static_cast<double>(std::numeric_limits<int>::max())));

    recordBuffer.setSize(2, std::max(1, maxSamples));
    recordBuffer.clear();
    recordWritePos = 0;
    liveRecording.store(true);
}

void GranularEngine::stopLiveRecording()
{
    liveRecording.store(false);

    if (recordWritePos > 0)
    {
        juce::AudioBuffer<float> captured(2, recordWritePos);
        for (int ch = 0; ch < 2; ++ch)
            captured.copyFrom(ch, 0, recordBuffer, ch, 0, recordWritePos);

        topSource.loadFromBuffer(captured, sampleRate, sampleRate);
        sampleChangedFlag.store(true);
    }

    recordWritePos = 0;
}

void GranularEngine::captureLiveInput(const juce::AudioBuffer<float>& input, int numSamples)
{
    if (! liveRecording.load())
        return;

    const int capacity = recordBuffer.getNumSamples();
    if (capacity <= 0 || recordWritePos >= capacity)
    {
        liveRecording.store(false);
        return;
    }

    const int numInChannels = input.getNumChannels();
    int n = std::min({ numSamples, input.getNumSamples(), capacity - recordWritePos });
    if (n <= 0)
        return;

    for (int ch = 0; ch < 2; ++ch)
    {
        if (numInChannels > 0)
        {
            const int srcCh = std::min(ch, numInChannels - 1);
            recordBuffer.copyFrom(ch, recordWritePos, input, srcCh, 0, n);
        }
    }

    recordWritePos += n;

    if (recordWritePos >= capacity)
        liveRecording.store(false); // buffer full: stop capturing
}

bool GranularEngine::isLiveRecording() const
{
    return liveRecording.load();
}

bool GranularEngine::hasSampleChanged() const
{
    return sampleChangedFlag.load();
}

void GranularEngine::clearSampleChangedFlag()
{
    sampleChangedFlag.store(false);
}

const SampleBuffer& GranularEngine::getTopSampleBuffer() const
{
    return topSource;
}

bool GranularEngine::saveTopSampleToFile(const juce::File& file)
{
    const juce::AudioBuffer<float>& data = topSource.getAudioData();
    if (data.getNumSamples() <= 0 || data.getNumChannels() <= 0)
        return false;

    // unique_ptr<FileOutputStream> -> unique_ptr<OutputStream> (move-converts) so it
    // can bind to the AudioFormat::createWriterFor() lvalue reference parameter.
    auto fileStream = std::make_unique<juce::FileOutputStream>(file);
    if (! fileStream->openedOk())
        return false;
    std::unique_ptr<juce::OutputStream> stream = std::move(fileStream);

    juce::WavAudioFormat wavFormat;
    const juce::AudioFormatWriterOptions options = juce::AudioFormatWriterOptions{}
        .withSampleRate (topSource.getSampleRate())
        .withNumChannels (data.getNumChannels())
        .withBitsPerSample (16);

    std::unique_ptr<juce::AudioFormatWriter> writer = wavFormat.createWriterFor (stream, options);
    if (writer == nullptr)
        return false;

    // createWriterFor() takes ownership of the stream; no stream.release() needed.
    return writer->writeFromAudioSampleBuffer (data, 0, data.getNumSamples());
}
