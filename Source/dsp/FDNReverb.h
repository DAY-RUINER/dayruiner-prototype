#pragma once

// ============================================================================
// FDNReverb.h — Householder feedback-delay-network reverb.
//
// A modern algorithmic reverb in the lineage of 2C-Aether / Valhalla /
// Blackhole: 8 modulated delay lines in a Householder feedback matrix,
// diffusion allpasses up front, damping + an AUTOMATABLE feedback cutoff +
// low-cut inside the feedback loop, input ducking, freeze, and an
// auto-cutoff that ends the tail shortly after notes stop sounding.
//
// Adapted from team 3's LushReverb contribution and cleaned up for JUCE 7:
//  - Diffusion allpasses fixed: team 3 never set their delay lengths (they
//    ran at 0 samples = no diffusion) and passed updateWritePointer=false
//    to pushSample, which froze the write head. Reimplemented on the owned
//    FracDelay with explicit pointer semantics.
//  - Feedback matrix corrected to the proper scaled-Householder form
//    y[d] = g[d] * (tap[d] - (2/N) * sum(taps)); team 3 folded the gain
//    into the matrix scaling.
//  - Per-delay-line RT60 gains instead of one rough mean-delay estimate.
//  - Mono delay lines + stereo output matrix (classic FDN) instead of
//    stereo lines; cheaper and better decorrelated.
//  - team 3's dead highCutHz param replaced with feedbackCutoffHz, a real
//    automatable low-pass in the feedback loop (the "kill the tail" knob).
//  - Stereo width now applies to the wet signal only (team 3 widened dry+wet).
//  - Auto-cutoff simplified: one setNoteActive(bool) call per block replaces
//    team 3's notify/begin/cancel/silenceCounter state machine.
//
// This REPLACES DayRuiner's original LushReverb (a juce::Reverb wrapper).
// See INTEGRATION.md for the parameter mapping.
//
// JUCE 7 compatible. No GUI modules required.
// ============================================================================

#include <juce_dsp/juce_dsp.h>

#include "DSPUtils.h"

#include <array>
#include <cmath>

class FDNReverb
{
public:
    struct Params
    {
        float decaySecs   = 4.0f;   // RT60, 0.2 .. 30
        float size        = 0.7f;   // 0..1, scales delay-line lengths
        float predelayMs  = 20.0f;  // 0..250 ms
        float damping     = 0.4f;   // 0..1, HF absorption in the feedback loop
        float diffusion   = 0.7f;   // 0..1, diffusion-allpass coefficient
        float modulationDepth = 0.3f; // 0..1, per-line delay modulation
        float lowCutHz    = 80.0f;  // feedback-loop high-pass
        float feedbackCutoffHz = 12000.0f; // feedback-loop low-pass (AUTOMATABLE)
        float width       = 1.0f;   // wet stereo width (mid/side)
        float mix         = 0.3f;   // dry/wet for series processing
        float duckAmount  = 0.0f;   // 0..1, ducks wet under loud dry input
        bool  freeze      = false;
        bool  autoCutoffOnNoteOff = false;
        float autoCutoffWindowSecs = 0.5f; // silence window before the tail kill
    };

    FDNReverb() = default;

    void prepare (double sampleRate, int maxBlockSize, int numChannels)
    {
        sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
        numChannels_ = juce::jlimit (1, 2, numChannels);

        predelay_[0].prepare ((int) (0.30 * sampleRate_) + 8);
        predelay_[1].prepare ((int) (0.30 * sampleRate_) + 8);

        // Diffusion allpasses get their REAL delay lengths here (team 3 left
        // them at 0). Channel 1 is detuned slightly for stereo decorrelation.
        const float dScale = (float) (sampleRate_ / 48000.0);
        for (int a = 0; a < NumDiffusion; ++a)
        {
            const float base = (float) DiffusionLengths[(size_t) a] * dScale;
            for (int ch = 0; ch < 2; ++ch)
            {
                auto& dl = diffusion_[(size_t) (ch * NumDiffusion + a)];
                dl.prepare ((int) (0.05 * sampleRate_) + 8);
                dlDelay_[(size_t) (ch * NumDiffusion + a)] = base * (ch == 0 ? 1.0f : 1.007f);
            }
        }

        for (auto& line : lines_)
            line.prepare ((int) (0.25 * sampleRate_) + 16);

        wet_.setSize (numChannels_, juce::jmax (1, maxBlockSize));

        // Deterministic LFO phases (no juce::Random state in the DSP path).
        for (int d = 0; d < NumLines; ++d)
        {
            modPhase_[(size_t) d] = (float) d * 0.7853982f;
            modRateHz_[(size_t) d] = 0.13f + 0.037f * (float) d;
        }

        updateLineDelays();
        reset();
    }

    void reset()
    {
        for (auto& dl : predelay_)  dl.reset();
        for (auto& dl : diffusion_) dl.reset();
        for (auto& dl : lines_)     dl.reset();

        dampState_.fill (0.0f);
        fbCutState_.fill (0.0f);
        hpState_.fill (0.0f);

        duckEnv_ = 0.0f;
        tailKill_ = 1.0f;
        silenceSecs_ = 0.0f;
        noteActive_ = false;
    }

    // Cheap: call once per block before process().
    void setParams (const Params& p)
    {
        params_ = p;
        updateLineDelays();
    }

    // Call once per block from the processor: true if any voice/note is
    // currently sounding. Drives the auto-cutoff envelope.
    void setNoteActive (bool active) noexcept { noteActive_ = active; }

    // Series processing: dry/wet mixed internally per params.mix.
    void process (juce::dsp::AudioBlock<float>& block)      { processImpl (block, false); }

    // Fully-wet processing for Send routing (the caller blends the result).
    void processWet (juce::dsp::AudioBlock<float>& block)   { processImpl (block, true); }

private:
    static constexpr int NumLines = 8;
    static constexpr int NumDiffusion = 4;

    // Prime-ish line lengths (samples @ 48 kHz), no common factors so the
    // FDN modes don't stack up.
    static constexpr std::array<int, NumLines> BaseLineLengths
        { 1723, 2081, 2459, 2711, 3067, 3557, 4013, 4519 };
    // Prime-ish diffusion lengths (samples @ 48 kHz). Kept short enough
    // that transients survive the smear with an audible attack, while still
    // breaking up the FDN's metallic resonances.
    static constexpr std::array<int, NumDiffusion> DiffusionLengths
        { 89, 149, 227, 347 };

    void updateLineDelays()
    {
        const float scale = (float) (sampleRate_ / 48000.0)
                          * juce::jmap (params_.size, 0.0f, 1.0f, 0.35f, 1.0f);
        for (int d = 0; d < NumLines; ++d)
        {
            lineDelay_[(size_t) d] = (float) BaseLineLengths[(size_t) d] * scale;
            modInc_[(size_t) d] = juce::MathConstants<float>::twoPi
                                * modRateHz_[(size_t) d] / (float) sampleRate_;
        }
        predelaySamples_ = (float) (params_.predelayMs * 0.001 * sampleRate_);
    }

    static float allpass (DayRuinerDSP::FracDelay& dl, float x,
                          float delaySamples, float g) noexcept
    {
        const float w = dl.read (delaySamples);
        const float y = -g * x + w;
        dl.write (x + g * y);
        return y;
    }

    void updateDuckEnvelope (const juce::dsp::AudioBlock<float>& block,
                             int nCh, int nSm) noexcept
    {
        const float aC = 1.0f - std::exp (-1.0f / (0.005f * (float) sampleRate_));
        const float rC = 1.0f - std::exp (-1.0f / (0.250f * (float) sampleRate_));

        for (int i = 0; i < nSm; ++i)
        {
            float peak = 0.0f;
            for (int ch = 0; ch < nCh; ++ch)
            {
                const float v = std::abs (block.getChannelPointer ((size_t) ch)[i]);
                if (v > peak) peak = v;
            }
            const float c = (peak > duckEnv_) ? aC : rC;
            duckEnv_ += c * (peak - duckEnv_);
        }
    }

    // Per-block auto-cutoff: after autoCutoffWindowSecs of silence the
    // feedback multiplier ramps to 0 (80 ms), ending the tail decisively.
    void updateAutoCutoff (int nSm) noexcept
    {
        const float blockSecs = (float) nSm / (float) sampleRate_;

        if (noteActive_)
        {
            silenceSecs_ = 0.0f;
            tailKill_ = juce::jmin (1.0f, tailKill_ + blockSecs / 0.02f);
        }
        else
        {
            silenceSecs_ += blockSecs;
            if (params_.autoCutoffOnNoteOff && silenceSecs_ >= params_.autoCutoffWindowSecs)
                tailKill_ = juce::jmax (0.0f, tailKill_ - blockSecs / 0.08f);
            else
                tailKill_ = juce::jmin (1.0f, tailKill_ + blockSecs / 0.02f);
        }
    }

    void processImpl (juce::dsp::AudioBlock<float>& block, bool wetOnly)
    {
        const int nCh = juce::jmin (numChannels_, (int) block.getNumChannels());
        const int nSm = (int) block.getNumSamples();
        if (nCh <= 0 || nSm <= 0)
            return;

        if (wet_.getNumSamples() < nSm || wet_.getNumChannels() < nCh)
            wet_.setSize (nCh, nSm, false, false, false);

        updateAutoCutoff (nSm);
        updateDuckEnvelope (block, nCh, nSm);

        const float apG   = juce::jlimit (0.0f, 0.85f, params_.diffusion * 0.85f);
        const float dampC = DayRuinerDSP::onePoleLPCoeff (
                                juce::jmap (params_.damping, 0.0f, 1.0f, 18000.0f, 800.0f),
                                sampleRate_);
        const float fbCutC = DayRuinerDSP::onePoleLPCoeff (params_.feedbackCutoffHz, sampleRate_);
        const float hpC    = DayRuinerDSP::onePoleLPCoeff (params_.lowCutHz, sampleRate_);

        // Per-line feedback gains from the requested RT60.
        const float rt = juce::jmax (0.1f, params_.decaySecs);
        float lineG[NumLines];
        for (int d = 0; d < NumLines; ++d)
        {
            const double g = std::pow (10.0, -3.0 * (double) lineDelay_[(size_t) d]
                                             / ((double) rt * sampleRate_));
            lineG[d] = params_.freeze ? 1.0f : juce::jlimit (0.0f, 0.9995f, (float) g);
        }

        const float modDepthSamps = params_.modulationDepth * 8.0f;
        constexpr float invN = 2.0f / (float) NumLines;
        const float ducked = 1.0f - juce::jlimit (0.0f, 1.0f, params_.duckAmount) * duckEnv_;
        const float width = params_.width;

        for (int i = 0; i < nSm; ++i)
        {
            float xL = nCh > 0 ? block.getChannelPointer (0)[i] : 0.0f;
            float xR = nCh > 1 ? block.getChannelPointer (1)[i] : xL;

            // Predelay.
            {
                const float pd = predelay_[0].read (predelaySamples_);
                predelay_[0].write (xL); xL = pd;
                if (nCh > 1)
                {
                    const float pd2 = predelay_[1].read (predelaySamples_);
                    predelay_[1].write (xR); xR = pd2;
                }
            }

            // Diffusion: 4 allpasses in series per channel.
            for (int ch = 0; ch < nCh; ++ch)
            {
                float x = (ch == 0) ? xL : xR;
                for (int a = 0; a < NumDiffusion; ++a)
                    x = allpass (diffusion_[(size_t) (ch * NumDiffusion + a)], x,
                                 dlDelay_[(size_t) (ch * NumDiffusion + a)], apG);
                if (ch == 0) xL = x; else xR = x;
            }

            // FDN taps: modulated fractional reads + feedback-path filtering.
            float tap[NumLines];
            for (int d = 0; d < NumLines; ++d)
            {
                const size_t dd = (size_t) d;
                modPhase_[dd] += modInc_[dd];
                if (modPhase_[dd] > juce::MathConstants<float>::twoPi)
                    modPhase_[dd] -= juce::MathConstants<float>::twoPi;

                const float md = lineDelay_[dd] + std::sin (modPhase_[dd]) * modDepthSamps;
                float t = lines_[dd].read (md);

                dampState_[dd]  += dampC  * (t - dampState_[dd]);  t = dampState_[dd];
                fbCutState_[dd] += fbCutC * (t - fbCutState_[dd]); t = fbCutState_[dd];
                hpState_[dd]    += hpC    * (t - hpState_[dd]);    t -= hpState_[dd];

                tap[d] = t;
            }

            // Householder feedback: y[d] = g[d] * (tap[d] - (2/N) * sum(taps)).
            float sum = 0.0f;
            for (int d = 0; d < NumLines; ++d) sum += tap[d];

            // Injection: each line gets +/- the (diffused) input sum.
            // Alternating signs keep the injection decorrelated with the
            // alternating-sign output taps below.
            const float inj = 1.0f * (xL + xR);
            const float kill = tailKill_;
            for (int d = 0; d < NumLines; ++d)
            {
                const float injD = inj * ((d % 2 == 0) ? 1.0f : -1.0f);
                lines_[(size_t) d].write (lineG[d] * kill * (tap[d] - invN * sum) + injD);
            }

            // Stereo output taps with opposing sign patterns (decorrelated).
            // Scale ~0.4: a full-scale transient reads back clearly without
            // clipping the tank on sustained material.
            float outL = 0.0f, outR = 0.0f;
            for (int d = 0; d < NumLines; ++d)
            {
                const float s = (d % 2 == 0) ? 1.0f : -1.0f;
                outL += tap[d] * s;
                outR += tap[d] * -s;
            }
            outL *= 0.4f;
            outR *= 0.4f;

            // Width on the wet signal only (mid/side).
            {
                const float mid  = 0.5f * (outL + outR);
                const float side = 0.5f * (outL - outR) * width;
                outL = mid + side;
                outR = mid - side;
            }

            wet_.getWritePointer (0)[i] = outL * ducked;
            if (nCh > 1)
                wet_.getWritePointer (1)[i] = outR * ducked;
        }

        // Mix.
        const float mix = wetOnly ? 1.0f : juce::jlimit (0.0f, 1.0f, params_.mix);
        for (int ch = 0; ch < nCh; ++ch)
        {
            float* dry = block.getChannelPointer ((size_t) ch);
            const float* w = wet_.getReadPointer (ch);
            for (int i = 0; i < nSm; ++i)
                dry[i] = dry[i] * (1.0f - mix) + w[i] * mix;
        }
    }

    double sampleRate_ = 44100.0;
    int numChannels_ = 2;
    Params params_;

    DayRuinerDSP::FracDelay predelay_[2];
    DayRuinerDSP::FracDelay diffusion_[NumDiffusion * 2];
    float dlDelay_[NumDiffusion * 2] = {};
    DayRuinerDSP::FracDelay lines_[NumLines];
    float lineDelay_[NumLines] = {};

    std::array<float, NumLines> dampState_ = {};
    std::array<float, NumLines> fbCutState_ = {};
    std::array<float, NumLines> hpState_ = {};

    float modPhase_[NumLines]  = {};
    float modRateHz_[NumLines] = {};
    float modInc_[NumLines]    = {};

    juce::AudioBuffer<float> wet_;

    float duckEnv_ = 0.0f;
    float tailKill_ = 1.0f;
    float silenceSecs_ = 0.0f;
    bool  noteActive_ = false;
    float predelaySamples_ = 0.0f;
};
