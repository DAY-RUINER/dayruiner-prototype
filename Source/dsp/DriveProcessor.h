#pragma once

// ============================================================================
// DriveProcessor.h — the distortion family: SoftClip / HardClip / Saturate /
// Distort in one class, one transfer function per mode.
//
// DECISION (see INTEGRATION.md): kept STANDALONE rather than merged into
// DayRuiner's existing Saturator. Rationale:
//  - Saturator is a per-sample creative-crush unit whose "drive" knob is
//    remapped per algorithm (bit depth, hold length, ...). DriveProcessor is
//    a proper musical drive stage: input gain -> waveshaper -> tone -> DC
//    block -> mix, with its own tone/bias/mix APVTS parameters.
//  - Merging would force two parameter models into one enum and bloat the
//    Saturator switch. The two units serve different musical roles and can
//    happily coexist (Saturator -> Drive -> ...).
//
// Improvements over team 3's version:
//  - process() takes juce::dsp::AudioBlock<float>& directly (team 3 flagged
//    their own AudioBuffer-wrapping mess; fixed).
//  - Tone is now a musical one-pole low-pass swept 800 Hz .. 20 kHz
//    (team 3's applyTone was a crude LP/HP blend with a dead zone).
//  - DC blocker fixed: team 3 prepared it with a hardcoded 512 max-block;
//    here it's a hand-rolled per-channel blocker, no spec juggling.
//  - Tone + DC block apply to the WET path only; dry stays untouched.
//
// JUCE 7 compatible. No GUI modules required.
// ============================================================================

#include <juce_dsp/juce_dsp.h>

#include "DSPUtils.h"

#include <array>
#include <cmath>

enum class DriveMode
{
    SoftClip = 0,
    HardClip = 1,
    Saturate = 2,
    Distort  = 3
};

struct DriveParams
{
    DriveMode mode = DriveMode::Saturate;
    float driveDb = 6.0f;  // 0..36 dB input gain
    float tone    = 0.5f;  // 0..1, wet low-pass 800 Hz .. 20 kHz
    float bias    = 0.0f;  // -0.5..0.5, asymmetry for even harmonics
    float mix     = 1.0f;  // 0..1 dry/wet
};

class DriveProcessor
{
public:
    DriveProcessor() = default;

    void prepare (double sampleRate, int /*maxBlockSize*/, int numChannels)
    {
        sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
        numChannels_ = juce::jlimit (1, 2, numChannels);
        reset();
    }

    void reset()
    {
        toneState_.fill (0.0f);
        dcX_.fill (0.0f);
        dcY_.fill (0.0f);
    }

    // Series processing with the params' own mix.
    void process (juce::dsp::AudioBlock<float>& block, const DriveParams& p)
    {
        processImpl (block, p, p.mix);
    }

    // Fully-wet processing for Send routing (the caller blends the result).
    void processWet (juce::dsp::AudioBlock<float>& block, const DriveParams& p)
    {
        processImpl (block, p, 1.0f);
    }

private:
    using ShapeFn = float (*) (float);

    static float shapeSoft (float x) noexcept
    {
        return std::tanh (x); // smooth, odd harmonics only
    }

    static float shapeHard (float x) noexcept
    {
        return juce::jlimit (-1.0f, 1.0f, x); // brickwall, aggressive
    }

    static float shapeSaturate (float x) noexcept
    {
        // Cubic soft-knee, gentle third-harmonic colour.
        if (x >  1.0f) return  2.0f / 3.0f;
        if (x < -1.0f) return -2.0f / 3.0f;
        return x - (x * x * x) / 3.0f;
    }

    static float shapeDistort (float x) noexcept
    {
        // Asymmetric exponential: even + odd harmonics, tube-ish.
        if (x >= 0.0f)
            return 1.0f - std::exp (-x);
        return -1.0f + std::exp (x * 0.7f); // softer on the negative side
    }

    void processImpl (juce::dsp::AudioBlock<float>& block,
                      const DriveParams& p, float mix)
    {
        const int nCh = juce::jmin (numChannels_, (int) block.getNumChannels());
        const int nSm = (int) block.getNumSamples();
        if (nCh <= 0 || nSm <= 0)
            return;

        ShapeFn fn = shapeSaturate;
        switch (p.mode)
        {
            case DriveMode::SoftClip: fn = shapeSoft;     break;
            case DriveMode::HardClip: fn = shapeHard;     break;
            case DriveMode::Saturate: fn = shapeSaturate; break;
            case DriveMode::Distort:  fn = shapeDistort;  break;
        }

        const float driveLin = juce::Decibels::decibelsToGain (juce::jlimit (0.0f, 36.0f, p.driveDb));
        const float bias = juce::jlimit (-0.5f, 0.5f, p.bias);
        const float m = juce::jlimit (0.0f, 1.0f, mix);
        const float toneC = DayRuinerDSP::onePoleLPCoeff (
                                800.0f * std::pow (25.0f, juce::jlimit (0.0f, 1.0f, p.tone)),
                                sampleRate_);

        for (int ch = 0; ch < nCh; ++ch)
        {
            const size_t c = (size_t) ch;
            float* data = block.getChannelPointer (c);
            float toneS = toneState_[c];
            float x1 = dcX_[c];
            float y1 = dcY_[c];

            for (int i = 0; i < nSm; ++i)
            {
                const float dry = data[i];

                float wet = fn (dry * driveLin + bias);

                toneS += toneC * (wet - toneS); // tone LP on wet
                wet = toneS;

                // DC blocker on wet (bias + asymmetric shapes create DC).
                const float y = wet - x1 + 0.995f * y1;
                x1 = wet;
                y1 = y;

                data[i] = dry * (1.0f - m) + y * m;
            }

            toneState_[c] = toneS;
            dcX_[c] = x1;
            dcY_[c] = y1;
        }
    }

    double sampleRate_ = 44100.0;
    int numChannels_ = 2;

    std::array<float, 2> toneState_ = {};
    std::array<float, 2> dcX_ = {};
    std::array<float, 2> dcY_ = {};
};
