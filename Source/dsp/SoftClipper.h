#pragma once

// ============================================================================
// SoftClipper.h — transparent master-bus safety net.
//
// A tanh soft-knee clipper that sits pre-gain and engages ONLY on peaks above
// the threshold (default 0.95). Normal material passes through bit-identical.
// The engaged latch lets the UI raise a discreet "it happened" warning that
// appears only when clipping actually occurs.
//
// Adapted from team 3's contribution, unchanged in behaviour, cleaned up for
// JUCE 7 (minimal includes: no DSP module needed, only juce_core atomics and
// juce_audio_basics for the buffer).
// ============================================================================

#include <juce_audio_basics/juce_audio_basics.h>

#include <atomic>
#include <cmath>

class SoftClipper
{
public:
    SoftClipper() = default;

    // Knee hardness: higher = more transparent until it isn't.
    void setKnee (float k) noexcept      { knee_ = juce::jmax (0.5f, k); }

    // Clip threshold in linear amplitude (0.95 ~= -0.45 dBFS).
    void setThreshold (float t) noexcept { threshold_ = juce::jlimit (0.1f, 1.0f, t); }

    void reset() noexcept { engaged_.store (false, std::memory_order_relaxed); }

    // Process in place. Returns true if any sample was clipped this block.
    // Threading: call on the audio thread; isEngaged()/clearEngaged() are
    // safe to call from the UI thread.
    bool process (juce::AudioBuffer<float>& buffer) noexcept
    {
        const int nCh = buffer.getNumChannels();
        const int nSm = buffer.getNumSamples();
        bool clipped = false;

        for (int ch = 0; ch < nCh; ++ch)
        {
            float* d = buffer.getWritePointer (ch);
            for (int i = 0; i < nSm; ++i)
            {
                const float x = d[i];
                const float ax = std::abs (x);

                if (ax > threshold_)
                {
                    // tanh knee, rejoining the linear region smoothly at the
                    // threshold: no derivative discontinuity, no audible kink.
                    const float s = (x < 0.0f) ? -1.0f : 1.0f;
                    d[i] = s * (threshold_ + knee_ * std::tanh ((ax - threshold_) / knee_));
                    clipped = true;
                }
            }
        }

        if (clipped)
            engaged_.store (true, std::memory_order_relaxed);

        return clipped;
    }

    bool isEngaged() const noexcept  { return engaged_.load (std::memory_order_relaxed); }
    void clearEngaged() noexcept     { engaged_.store (false, std::memory_order_relaxed); }

private:
    float threshold_ = 0.95f;
    float knee_ = 1.5f;
    std::atomic<bool> engaged_ { false };
};
