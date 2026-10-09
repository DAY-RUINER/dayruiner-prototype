#pragma once

#include <JuceHeader.h>
#include <atomic>

#include "GrainScheduler.h"
#include "SampleBuffer.h"

// Two-layer granular engine (top / bottom sample layers, equal-power blend).
// Temp buffers are members sized once in prepare(); process() only clears the
// active region per block. Recording allocates on the UI thread only.
class GranularEngine
{
public:
    void prepare(double sampleRate, int maxBlockSize);

    bool loadTopSample(const juce::File& f);
    bool loadBottomSample(const juce::File& f);

    // Synthesizes the factory default source texture (rich, evolving harmonics
    // with a soft noise bed) into both layers, so the GRAINFIELD is never dead
    // before the user loads their own sample. Called once from prepareToPlay
    // when nothing is loaded; no audio is running yet, so this is race-free.
    void loadDefaultSample(double sampleRate);

    void setGrainLengthNormalized(float top01, float bottom01);    // 0..1 -> 1ms..2000ms (log)
    void setGrainFrequencyNormalized(float top01, float bottom01); // 0..1 -> 0.1Hz..100Hz (log)
    void setBlend(float b01); // 0 = all top, 1 = all bottom

    void setTopPlayhead(double n01);
    void setBottomPlayhead(double n01);
    void setTopPlaybackRate(double r);
    void setBottomPlaybackRate(double r);

    // ADDS stereo output into `output` (caller clears).
    void process(juce::AudioBuffer<float>& output, int numSamples);

    void startLiveRecording();
    void stopLiveRecording();
    void captureLiveInput(const juce::AudioBuffer<float>& input, int numSamples);
    bool isLiveRecording() const;

    bool hasSampleChanged() const;
    void clearSampleChangedFlag();

    const SampleBuffer& getTopSampleBuffer() const;
    bool saveTopSampleToFile(const juce::File& file);

    // --- GRAINFIELD visualizer support (prototype addition) ---
    // Read-only access to the two grain schedulers for live grain windows.
    const GrainScheduler& getTopScheduler() const { return topEngine; }
    const GrainScheduler& getBottomScheduler() const { return bottomEngine; }
    int getTopSourceNumSamples() const { return topSource.getNumSamples(); }
    int getBottomSourceNumSamples() const { return bottomSource.getNumSamples(); }

    // Post-recombination monitor tap: per-block peak of the granular mix bus,
    // kept as a 64-slot ring (oldest -> newest). Written on the audio thread
    // in process(); read on the UI thread via getOutputPeakHistory().
    // This is a pure monitor tap: it does not alter the DSP in any way.
    static constexpr int kPeakHistorySize = 64;
    void getOutputPeakHistory(float* out, int numOut) const;

private:
    static constexpr int kMaxRecordSeconds = 120;

    void applyTopGrainParams();
    void applyBottomGrainParams();

    double sampleRate = 44100.0;
    float blend = 0.0f;

    float topGrainLengthMs = 100.0f;
    float topGrainFreqHz = 10.0f;
    float bottomGrainLengthMs = 100.0f;
    float bottomGrainFreqHz = 10.0f;
    float attackFraction = 0.1f;

    SampleBuffer topSource;
    SampleBuffer bottomSource;
    GrainScheduler topEngine;
    GrainScheduler bottomEngine;

    juce::AudioBuffer<float> topBuffer;    // pre-sized scratch
    juce::AudioBuffer<float> bottomBuffer; // pre-sized scratch

    // Live recording state (allocation happens in startLiveRecording, UI thread).
    juce::AudioBuffer<float> recordBuffer;
    int recordWritePos = 0;
    std::atomic<bool> liveRecording { false };
    std::atomic<bool> sampleChangedFlag { false };

    // GRAINFIELD monitor tap (see header): 64-slot peak ring, audio writes,
    // UI reads. Plain atomics, no locks.
    std::array<std::atomic<float>, kPeakHistorySize> outputPeakHistory_ {};
    std::atomic<int> peakWritePos_ { 0 };
};
