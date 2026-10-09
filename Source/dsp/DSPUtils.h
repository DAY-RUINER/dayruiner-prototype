#pragma once

// ============================================================================
// DSPUtils.h — tiny shared helpers for the DayRuiner engine-fx modules.
//
// JUCE 7 compatible. Deliberately GUI-free so console test harnesses can
// include these headers without pulling in X11 on Linux.
// ============================================================================

#include <juce_dsp/juce_dsp.h>

#include <cmath>
#include <vector>

namespace DayRuinerDSP
{

// One-pole low-pass coefficient for a given cutoff (Hz) and sample rate.
// Usage: state += coeff * (input - state);
inline float onePoleLPCoeff (float cutoffHz, double sampleRate) noexcept
{
    const float c = juce::jlimit (20.0f, 20000.0f, cutoffHz);
    const double sr = sampleRate > 0.0 ? sampleRate : 44100.0;
    return 1.0f - std::exp (-juce::MathConstants<float>::twoPi * c / (float) sr);
}

// Fractional-delay circular buffer with linear interpolation.
//
// Owns its read/write pointer semantics explicitly, so callers never have to
// guess at juce::dsp::DelayLine pop/push pointer behaviour. Used for the FDN
// delay lines, diffusion allpasses and predelay in FDNReverb.
struct FracDelay
{
    std::vector<float> buf;
    int size = 0;
    int writePos = 0;

    void prepare (int maxSamples)
    {
        size = juce::jmax (8, maxSamples);
        buf.assign ((size_t) size, 0.0f);
        writePos = 0;
    }

    void reset()
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        writePos = 0;
    }

    // Read at a fractional delay (samples). Caller must ensure
    // 0 <= delaySamples < size.
    float read (float delaySamples) const noexcept
    {
        float rp = std::fmod ((float) writePos - delaySamples, (float) size);
        if (rp < 0.0f)
            rp += (float) size;

        const int i0 = (int) rp;
        const float frac = rp - (float) i0;
        int i1 = i0 + 1;
        if (i1 >= size)
            i1 = 0;

        return buf[(size_t) i0] + frac * (buf[(size_t) i1] - buf[(size_t) i0]);
    }

    void write (float x) noexcept
    {
        buf[(size_t) writePos] = x;
        if (++writePos >= size)
            writePos = 0;
    }
};

} // namespace DayRuinerDSP
