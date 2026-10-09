#include "SampleBuffer.h"

#include <algorithm>
#include <cmath>
#include <limits>

bool SampleBuffer::loadFromFile(const juce::File& file, double targetSampleRate, int maxDurationSeconds)
{
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader == nullptr)
        return false;

    if (maxDurationSeconds < 1)
        maxDurationSeconds = 1;
    if (targetSampleRate <= 0.0)
        targetSampleRate = 44100.0;

    const double maxSamplesD = targetSampleRate * static_cast<double>(maxDurationSeconds);
    const double clampedD = std::min(maxSamplesD, static_cast<double>(std::numeric_limits<int>::max()));
    const juce::int64 numSamplesToRead = static_cast<juce::int64>(std::min(
        static_cast<double>(reader->lengthInSamples), clampedD));

    if (numSamplesToRead <= 0)
        return false;

    juce::AudioBuffer<float> temp(static_cast<int>(reader->numChannels),
                                  static_cast<int>(numSamplesToRead));
    reader->read(&temp, 0, static_cast<int>(numSamplesToRead), 0, true, true);

    audioData.makeCopyOf(temp);
    sampleRate = reader->sampleRate;
    fileName = file.getFileName();
    isLoaded = true;

    resampleTo(targetSampleRate);
    return true;
}

void SampleBuffer::loadFromBuffer(const juce::AudioBuffer<float>& incoming,
                                  double sourceSampleRate,
                                  double targetSampleRate)
{
    if (incoming.getNumSamples() <= 0 || incoming.getNumChannels() <= 0)
    {
        isLoaded = false;
        return;
    }

    audioData.makeCopyOf(incoming);
    sampleRate = (sourceSampleRate > 0.0) ? sourceSampleRate : targetSampleRate;
    isLoaded = true;

    resampleTo(targetSampleRate);
}

void SampleBuffer::resampleTo(double targetSampleRate)
{
    if (! isLoaded || targetSampleRate <= 0.0)
        return;

    if (std::abs(sampleRate - targetSampleRate) < 1.0)
        return;

    const int numChannels = audioData.getNumChannels();
    const int numInput = audioData.getNumSamples();
    if (numChannels <= 0 || numInput <= 0)
        return;

    const double ratio = sampleRate / targetSampleRate; // >1 when downsampling
    const double outSamplesD = static_cast<double>(numInput) / ratio;
    const int numOutput = static_cast<int>(std::min(outSamplesD,
        static_cast<double>(std::numeric_limits<int>::max())));
    if (numOutput <= 0)
        return;

    juce::AudioBuffer<float> resampled(numChannels, numOutput);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        juce::LagrangeInterpolator interpolator;
        interpolator.process(ratio,
                             audioData.getReadPointer(ch),
                             resampled.getWritePointer(ch),
                             numOutput);
    }

    audioData.makeCopyOf(resampled);
    sampleRate = targetSampleRate;
}

float SampleBuffer::readSample(int channel, double position, InterpolationMode mode) const noexcept
{
    const int numSamples = audioData.getNumSamples();
    const int numChannels = audioData.getNumChannels();

    if (! isLoaded || numSamples <= 0 || numChannels <= 0)
        return 0.0f;

    int ch = channel;
    if (ch < 0) ch = 0;
    else if (ch >= numChannels) ch = numChannels - 1;

    const double n = static_cast<double>(numSamples);
    double pos = std::fmod(position, n);
    if (pos < 0.0)
        pos += n;

    switch (mode)
    {
        case InterpolationMode::Linear: return readLinear(ch, pos);
        case InterpolationMode::Cubic:  return readCubic(ch, pos);
        case InterpolationMode::Hermite:return readHermite(ch, pos);
        case InterpolationMode::Sinc8:  return readSinc8(ch, pos);
    }

    return readHermite(ch, pos);
}

float SampleBuffer::readLinear(int ch, double pos) const noexcept
{
    const int numSamples = audioData.getNumSamples();
    const float* data = audioData.getReadPointer(ch);

    const int p0 = static_cast<int>(pos);
    const int p1 = (p0 + 1) % numSamples;
    const float frac = static_cast<float>(pos - static_cast<double>(p0));

    return data[p0] + frac * (data[p1] - data[p0]);
}

float SampleBuffer::readCubic(int ch, double pos) const noexcept
{
    // Catmull-Rom spline.
    const int numSamples = audioData.getNumSamples();
    const float* data = audioData.getReadPointer(ch);

    const int p1 = static_cast<int>(pos);
    const float frac = static_cast<float>(pos - static_cast<double>(p1));
    const int p0 = (p1 - 1 + numSamples) % numSamples;
    const int p2 = (p1 + 1) % numSamples;
    const int p3 = (p1 + 2) % numSamples;

    const float v0 = data[p0], v1 = data[p1], v2 = data[p2], v3 = data[p3];
    const float f2 = frac * frac;
    const float f3 = f2 * frac;

    return 0.5f * ((2.0f * v1)
                 + (-v0 + v2) * frac
                 + (2.0f * v0 - 5.0f * v1 + 4.0f * v2 - v3) * f2
                 + (-v0 + 3.0f * v1 - 3.0f * v2 + v3) * f3);
}

float SampleBuffer::readHermite(int ch, double pos) const noexcept
{
    // Cubic Hermite with Catmull-Rom tangents.
    const int numSamples = audioData.getNumSamples();
    const float* data = audioData.getReadPointer(ch);

    const int p1 = static_cast<int>(pos);
    const float frac = static_cast<float>(pos - static_cast<double>(p1));
    const int p0 = (p1 - 1 + numSamples) % numSamples;
    const int p2 = (p1 + 1) % numSamples;
    const int p3 = (p1 + 2) % numSamples;

    const float v0 = data[p0], v1 = data[p1], v2 = data[p2], v3 = data[p3];
    const float m1 = 0.5f * (v2 - v0);
    const float m2 = 0.5f * (v3 - v1);
    const float f2 = frac * frac;
    const float f3 = f2 * frac;

    const float h00 = 2.0f * f3 - 3.0f * f2 + 1.0f;
    const float h10 = f3 - 2.0f * f2 + frac;
    const float h01 = -2.0f * f3 + 3.0f * f2;
    const float h11 = f3 - f2;

    return h00 * v1 + h10 * m1 + h01 * v2 + h11 * m2;
}

float SampleBuffer::readSinc8(int ch, double pos) const noexcept
{
    // 8-tap windowed sinc, Blackman-Harris window, taps at offsets -3..4.
    const int numSamples = audioData.getNumSamples();
    const float* data = audioData.getReadPointer(ch);

    const int base = static_cast<int>(std::floor(pos));
    const double frac = pos - static_cast<double>(base);

    float sum = 0.0f;
    float norm = 0.0f;

    for (int k = -3; k <= 4; ++k)
    {
        const double x = static_cast<double>(k) - frac;

        double sinc;
        if (std::abs(x) < 1e-9)
            sinc = 1.0;
        else
            sinc = std::sin(juce::MathConstants<double>::pi * x) / (juce::MathConstants<double>::pi * x);

        // 4-term Blackman-Harris over the 8-tap span.
        const double w = 0.35875
                       - 0.48829 * std::cos(2.0 * juce::MathConstants<double>::pi * (x + 4.0) / 8.0)
                       + 0.14128 * std::cos(4.0 * juce::MathConstants<double>::pi * (x + 4.0) / 8.0)
                       - 0.01168 * std::cos(6.0 * juce::MathConstants<double>::pi * (x + 4.0) / 8.0);

        const float tap = static_cast<float>(sinc * w);
        const int idx = (base + k) % numSamples;
        const int wrapped = idx < 0 ? idx + numSamples : idx;

        sum += data[wrapped] * tap;
        norm += tap;
    }

    if (std::abs(norm) < 1e-6f)
        return 0.0f;

    return sum / norm;
}
