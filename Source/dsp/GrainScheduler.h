#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <functional>

#include "Grain.h"
#include "SampleBuffer.h"

// Fires and renders grains in a single per-sample pass so that every active
// grain advances exactly once per sample (no double-tick bug).
// processBlock() runs on the audio thread: no allocation, no locks.
class GrainScheduler
{
public:
    static constexpr int MAX_GRAINS = 199;

    void prepare(double sampleRate);
    void reset();

    void setGrainParameters(float grainLengthMs, float grainFrequencyHz, float attackFraction);
    void setSyncMode(bool shouldSync, double hostBpm);
    void setPositionModulation(double normalizedOffset); // [-1, +1]

    std::atomic<double> playheadPosition { 0.0 }; // normalized 0..1
    std::atomic<double> playbackRate { 1.0 };
    std::atomic<float> ringModDepth { 0.0f };

    // Fires grains AND renders them in ONE pass: each active grain advances
    // exactly once per sample. ADDS into output (caller clears).
    void processBlock(int numSamples, const SampleBuffer& source,
                      juce::AudioBuffer<float>& output,
                      SampleBuffer::InterpolationMode interpMode,
                      std::function<void(Grain&)> onGrainFired);

    // Live grain windows for the GRAINFIELD visualizer (UI thread). Fills
    // `out` with up to `maxOut` active grains; source positions are
    // normalised 0..1 by `sourceNumSamples`. Returns the count written.
    // Reads grain state without locking: benign for visualization, the audio
    // thread may advance a grain mid-read (values stay in range).
    struct GrainWindow
    {
        double start01 = 0.0; // window start in the source, normalised
        double end01 = 0.0;   // window end in the source, normalised
        float intensity = 0.0f; // current envelope 0..1
    };

    int getActiveGrainWindows(GrainWindow* out, int maxOut, double sourceNumSamples) const
    {
        if (out == nullptr || maxOut <= 0 || sourceNumSamples <= 0.0)
            return 0;

        int n = 0;
        for (const auto& g : grains)
        {
            if (! g.active || n >= maxOut)
                continue;

            const double startSrc = g.readPosition
                - static_cast<double>(g.elapsedSamples) * g.playbackRate;
            const double lenSrc = static_cast<double>(g.totalLengthSamples) * g.playbackRate;

            GrainWindow w;
            w.start01 = juce::jlimit(0.0, 1.0, startSrc / sourceNumSamples);
            w.end01 = juce::jlimit(0.0, 1.0, (startSrc + lenSrc) / sourceNumSamples);
            w.intensity = juce::jlimit(0.0f, 1.0f, g.currentEnvelope);
            out[n++] = w;
        }
        return n;
    }

private:
    void fireGrain(const SampleBuffer& source, std::function<void(Grain&)>& onGrainFired);

    std::array<Grain, MAX_GRAINS> grains;
    double sampleRate = 44100.0;
    float grainLengthMs = 100.0f;
    float grainFrequencyHz = 10.0f;
    float attackFraction = 0.1f;
    double firePeriodSamples = 4410.0;
    double samplesUntilNextFire = 0.0;
    bool syncMode = false;
    double syncBpm = 120.0;
    double positionModulation = 0.0;
};
