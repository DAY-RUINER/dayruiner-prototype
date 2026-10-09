#pragma once

#include <JuceHeader.h>
#include <cmath>

// Header-only grain voice for the DAY RUINER granular engine.
// All state mutations happen on the audio thread; no allocation here.
struct Grain
{
    double readPosition = 0.0;
    double playbackRate = 1.0;
    int totalLengthSamples = 0;
    int elapsedSamples = 0;
    float peakAmplitude = 1.0f;
    float currentEnvelope = 0.0f;

    enum class WindowShape { Hann, Linear, Tukey, Blackman };
    WindowShape windowShape = WindowShape::Hann;
    float attackFraction = 0.1f;

    float ringModDepth = 0.0f;
    double ringModPhase = 0.0;
    double ringModFrequency = 440.0;

    bool active = false;
    int voiceId = -1;
    int noteId = -1;

    float computeEnvelope() noexcept
    {
        if (totalLengthSamples <= 0)
            return 0.0f;

        double t = static_cast<double>(elapsedSamples) / static_cast<double>(totalLengthSamples);
        if (t < 0.0) t = 0.0;
        else if (t > 1.0) t = 1.0;

        constexpr double twoPi = 2.0 * juce::MathConstants<double>::pi;

        switch (windowShape)
        {
            case WindowShape::Hann:
                return static_cast<float>(0.5 * (1.0 - std::cos(twoPi * t)));

            case WindowShape::Linear:
            {
                double a = attackFraction;
                if (a <= 0.0) a = 0.001;
                else if (a >= 1.0) a = 0.999;
                if (t < a)
                    return static_cast<float>(t / a);
                return static_cast<float>(1.0 - (t - a) / (1.0 - a));
            }

            case WindowShape::Tukey:
            {
                double alpha = static_cast<double>(attackFraction) * 2.0;
                if (alpha <= 0.0)
                    return 1.0f;
                if (alpha > 1.0)
                    alpha = 1.0;
                const double half = alpha * 0.5;
                if (t < half)
                    return static_cast<float>(0.5 * (1.0 + std::cos(juce::MathConstants<double>::pi * (2.0 * t / alpha - 1.0))));
                if (t > 1.0 - half)
                    return static_cast<float>(0.5 * (1.0 + std::cos(juce::MathConstants<double>::pi * (2.0 * t / alpha - 2.0 / alpha + 1.0))));
                return 1.0f;
            }

            case WindowShape::Blackman:
                return static_cast<float>(0.42 - 0.5 * std::cos(twoPi * t) + 0.08 * std::cos(2.0 * twoPi * t));
        }

        return 0.0f;
    }

    // Advances the grain by exactly one sample. sampleRate must be the
    // engine's actual rate (bug fix: never hardcode 44100).
    void tick(double sampleRate) noexcept
    {
        currentEnvelope = computeEnvelope();
        readPosition += playbackRate;
        ++elapsedSamples;

        if (sampleRate > 0.0)
            ringModPhase += 2.0 * juce::MathConstants<double>::pi * ringModFrequency / sampleRate;

        if (elapsedSamples >= totalLengthSamples)
            active = false;
    }
};
