#include "SynthEngine.h"

void SynthEngine::prepare(double newSampleRate, int newMaxBlockSize)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    maxBlockSize = juce::jmax(1, newMaxBlockSize);
    buildWavetables();
    reset();
}

void SynthEngine::reset()
{
    for (int i = 0; i < MAX_VOICES; ++i)
    {
        voices[i] = Voice{};
        voices[i].voiceIndex = i;
    }
}

void SynthEngine::buildWavetables()
{
    // Saw-ish table: 32-harmonic additive series, peak-normalised.
    float sawPeak = 0.0f;
    for (int n = 0; n < WAVETABLE_SIZE; ++n)
    {
        const double t = double(n) / double(WAVETABLE_SIZE);
        double s = 0.0;
        for (int k = 1; k <= 32; ++k)
            s += std::sin(twoPi * double(k) * t) / double(k);
        sawTable[size_t(n)] = float(s);
        sawPeak = juce::jmax(sawPeak, float(std::abs(s)));
    }
    if (sawPeak > 0.0f)
        for (auto& x : sawTable)
            x /= sawPeak;

    // Hollow square-ish table: odd harmonics only, fast 1/k^2 rolloff,
    // peak-normalised for a thinner, "hollow" body.
    float squarePeak = 0.0f;
    for (int n = 0; n < WAVETABLE_SIZE; ++n)
    {
        const double t = double(n) / double(WAVETABLE_SIZE);
        double s = 0.0;
        for (int k = 1; k <= 15; k += 2)
        {
            const double kd = double(k);
            s += std::sin(twoPi * kd * t) / (kd * kd);
        }
        squareishTable[size_t(n)] = float(s);
        squarePeak = juce::jmax(squarePeak, float(std::abs(s)));
    }
    if (squarePeak > 0.0f)
        for (auto& x : squareishTable)
            x /= squarePeak;
}

void SynthEngine::trigger(int midiNote, float velocity, Algorithm algo, float decayTimeSeconds, int startOffsetSamples)
{
    // Find a free voice; otherwise steal the one with the smallest envelope.
    int slot = -1;
    float smallestEnv = 2.0f;
    for (int i = 0; i < MAX_VOICES; ++i)
    {
        if (!voices[i].active)
        {
            slot = i;
            break;
        }
        if (voices[i].env < smallestEnv)
        {
            smallestEnv = voices[i].env;
            slot = i;
        }
    }
    if (slot < 0)
        return;

    Voice& v = voices[slot];
    const double sr = sampleRate > 0.0 ? sampleRate : 44100.0;
    const double freq = juce::MidiMessage::getMidiNoteInHertz(midiNote);

    v.active = true;
    v.noteNumber = midiNote;
    v.velocity = juce::jlimit(0.0f, 1.0f, velocity);
    v.algo = algo;
    v.phase = 0.0;
    v.modPhase = 0.0;
    v.phaseInc = freq / sr;
    v.modPhaseInc = 0.0;
    v.modIndex = 0.0f;
    v.env = 1.0f;
    v.voiceIndex = slot;

    const int blockMax = juce::jmax(1, maxBlockSize);
    v.startOffsetSamples = juce::jlimit(0, blockMax - 1, startOffsetSamples);

    // Clear all reusable per-trigger state (voices are recycled).
    for (int i = 0; i < Voice::MAX_PARTIALS; ++i)
    {
        v.partialFreqs[i] = 0.0f;
        v.partialAmps[i] = 0.0f;
        v.partialPhases[i] = 0.0f;
    }
    v.hatLastIn = 0.0f;
    v.hatLastOut = 0.0f;
    v.noiseLpState = 0.0f;
    v.noiseLpCutoff = 0.5f;
    v.stutterCounter = 0;
    v.stutterHold = 0.0f;
    v.morphPos = 0.5f;

    float decaySeconds = juce::jmax(0.01f, decayTimeSeconds);

    switch (algo)
    {
        case Algorithm::AnalogKick:
            v.modIndex = 4.0f; // pitch-envelope amount
            break;
        case Algorithm::AnalogSnare:
            // BUG FIX: the sketch never initialised the second oscillator's rate.
            v.modPhaseInc = (freq * 1.5) / sr;
            break;
        case Algorithm::AnalogHat:
            decaySeconds = 0.05f;
            break;
        case Algorithm::FMKick:
            v.modPhaseInc = (freq * 2.0) / sr;
            v.modIndex = 8.0f;
            break;
        case Algorithm::FMSnare:
            v.modPhaseInc = (freq * 1.41) / sr;
            v.modIndex = 6.0f;
            break;
        case Algorithm::FMPluck:
            v.modPhaseInc = (freq * 3.0) / sr;
            v.modIndex = 5.0f;
            break;
        case Algorithm::MetalTine:
            v.modPhaseInc = (freq * 3.5) / sr;
            v.modIndex = 5.0f;
            decaySeconds *= 2.0f;
            break;
        case Algorithm::WavetablePad:
            decaySeconds *= 4.0f; // slow-attack feel via a longer decay
            break;
        case Algorithm::VectorMorph:
            v.morphPos = v.velocity;
            break;
        case Algorithm::ModalMembrane:
        {
            const float ratios[] = { 1.0f, 1.593f, 2.135f, 2.295f, 2.917f };
            const float amps[] = { 1.0f, 0.6f, 0.4f, 0.25f, 0.15f };
            for (int i = 0; i < 5; ++i)
            {
                v.partialFreqs[i] = ratios[i];
                v.partialAmps[i] = amps[i];
            }
            break;
        }
        case Algorithm::ModalString:
            for (int i = 0; i < 8; ++i)
            {
                v.partialFreqs[i] = float(i + 1);
                v.partialAmps[i] = 1.0f / float(i + 1);
            }
            break;
        case Algorithm::AdditiveBell:
        {
            const float ratios[] = { 1.0f, 2.76f, 5.40f, 8.93f, 13.34f };
            for (int i = 0; i < 5; ++i)
            {
                v.partialFreqs[i] = ratios[i];
                v.partialAmps[i] = 1.0f / float(i + 1);
            }
            break;
        }
        case Algorithm::AdditiveOrgan:
        {
            const float ratios[] = { 1.0f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f };
            for (int i = 0; i < 6; ++i)
            {
                v.partialFreqs[i] = ratios[i];
                v.partialAmps[i] = 1.0f / float(i + 1);
            }
            break;
        }
        case Algorithm::WavetableLead:
        case Algorithm::PhaseDistortion:
        case Algorithm::GranularPulse:
        case Algorithm::GlitchStutter:
        case Algorithm::DigitalCrush:
        case Algorithm::NoiseSweep:
        case Algorithm::SubSine:
            break; // need no extra trigger state
    }

    // Reaches ~5% amplitude after decaySeconds.
    v.envDecay = std::exp(-3.0f / (decaySeconds * float(sr)));
}

void SynthEngine::renderInto(juce::AudioBuffer<float>& output, int numSamples)
{
    if (numSamples <= 0)
        return;

    const int numChannels = output.getNumChannels();
    if (numChannels <= 0)
        return;

    for (int s = 0; s < numSamples; ++s)
    {
        float mixL = 0.0f;
        float mixR = 0.0f;

        for (int i = 0; i < MAX_VOICES; ++i)
        {
            Voice& v = voices[i];
            if (!v.active)
                continue;
            if (s < v.startOffsetSamples)
                continue;

            float sample = 0.0f;
            switch (v.algo)
            {
                case Algorithm::AnalogKick:      sample = renderAnalogKick(v); break;
                case Algorithm::AnalogSnare:     sample = renderAnalogSnare(v); break;
                case Algorithm::AnalogHat:       sample = renderAnalogHat(v); break;
                case Algorithm::FMKick:
                case Algorithm::FMSnare:
                case Algorithm::FMPluck:
                case Algorithm::MetalTine:       sample = renderFM(v); break;
                case Algorithm::ModalMembrane:
                case Algorithm::ModalString:
                case Algorithm::AdditiveBell:
                case Algorithm::AdditiveOrgan:  sample = renderPartials(v); break;
                case Algorithm::WavetableLead:   sample = renderWavetableLead(v); break;
                case Algorithm::WavetablePad:    sample = renderWavetablePad(v); break;
                case Algorithm::PhaseDistortion: sample = renderPhaseDistortion(v); break;
                case Algorithm::VectorMorph:     sample = renderVectorMorph(v); break;
                case Algorithm::GranularPulse:  sample = renderGranularPulse(v); break;
                case Algorithm::GlitchStutter:   sample = renderGlitchStutter(v); break;
                case Algorithm::DigitalCrush:    sample = renderDigitalCrush(v); break;
                case Algorithm::NoiseSweep:      sample = renderNoiseSweep(v); break;
                case Algorithm::SubSine:         sample = renderSine(v); break;
            }

            sample *= v.velocity * v.env;
            v.env *= v.envDecay;

            const float pan = (v.voiceIndex % 2 == 0) ? 0.3f : 0.7f;
            mixL += sample * (1.0f - pan);
            mixR += sample * pan;

            if (v.env < 1e-5f)
                v.active = false;
        }

        output.addSample(0, s, mixL);
        if (numChannels > 1)
            output.addSample(1, s, mixR);
    }
}

float SynthEngine::readWavetable(const std::array<float, WAVETABLE_SIZE>& table, double phase) const
{
    const double p = phase - std::floor(phase);
    const double pos = p * double(WAVETABLE_SIZE);
    const int i0 = int(pos) % WAVETABLE_SIZE;
    const int i1 = (i0 + 1) % WAVETABLE_SIZE;
    const float frac = float(pos - double(int(pos)));
    return table[size_t(i0)] + frac * (table[size_t(i1)] - table[size_t(i0)]);
}

float SynthEngine::renderAnalogKick(Voice& v)
{
    v.phase += v.phaseInc;
    if (v.phase >= 1.0)
        v.phase -= 1.0;

    const float pitchMod = juce::jlimit(0.5f, 8.0f, 1.0f + v.modIndex);
    v.modIndex *= 0.9995f;
    return float(std::sin(twoPi * v.phase * double(pitchMod)));
}

float SynthEngine::renderAnalogSnare(Voice& v)
{
    v.phase += v.phaseInc;
    if (v.phase >= 1.0)
        v.phase -= 1.0;
    v.modPhase += v.modPhaseInc;
    if (v.modPhase >= 1.0)
        v.modPhase -= 1.0;

    const float tone1 = float(std::sin(twoPi * v.phase));
    const float tone2 = float(std::sin(twoPi * v.modPhase));
    const float noise = random.nextFloat() * 2.0f - 1.0f;
    return tone1 * 0.4f + tone2 * 0.3f + noise * 0.3f;
}

float SynthEngine::renderAnalogHat(Voice& v)
{
    // One-pole highpass using per-voice state (thread-safe: no statics).
    const float noise = random.nextFloat() * 2.0f - 1.0f;
    const float hp = 0.9f * (v.hatLastOut + noise - v.hatLastIn);
    v.hatLastIn = noise;
    v.hatLastOut = hp;
    return hp;
}

float SynthEngine::renderFM(Voice& v)
{
    v.modPhase += v.modPhaseInc;
    if (v.modPhase >= 1.0)
        v.modPhase -= 1.0;
    const float modulator = float(std::sin(twoPi * v.modPhase));

    v.phase += v.phaseInc;
    if (v.phase >= 1.0)
        v.phase -= 1.0;

    const float carrier = float(std::sin(twoPi * v.phase + double(modulator) * double(v.modIndex)));
    v.modIndex *= v.envDecay;
    return carrier;
}

float SynthEngine::renderPartials(Voice& v)
{
    float sum = 0.0f;
    for (int i = 0; i < Voice::MAX_PARTIALS; ++i)
    {
        v.partialPhases[i] += v.partialFreqs[i] * float(v.phaseInc);
        // Floor-based wrap: ratio * phaseInc can exceed 1 per sample (e.g. AdditiveBell).
        if (v.partialPhases[i] >= 1.0f)
            v.partialPhases[i] -= std::floor(v.partialPhases[i]);
        sum += float(std::sin(twoPi * double(v.partialPhases[i]))) * v.partialAmps[i];
    }
    return sum;
}

float SynthEngine::renderWavetableLead(Voice& v)
{
    const float out = readWavetable(sawTable, v.phase);
    v.phase += v.phaseInc;
    if (v.phase >= 1.0)
        v.phase -= 1.0;
    return out;
}

float SynthEngine::renderWavetablePad(Voice& v)
{
    // Octave-down feel: the read head only traverses half the table per cycle.
    const float out = readWavetable(squareishTable, v.phase * 0.5);
    v.phase += v.phaseInc;
    if (v.phase >= 1.0)
        v.phase -= 1.0;
    return out;
}

float SynthEngine::renderPhaseDistortion(Voice& v)
{
    v.phase += v.phaseInc;
    if (v.phase >= 1.0)
        v.phase -= 1.0;

    const double warped = std::pow(v.phase, 1.0 + 2.0 * double(v.velocity));
    return float(std::sin(twoPi * warped));
}

float SynthEngine::renderVectorMorph(Voice& v)
{
    v.phase += v.phaseInc;
    if (v.phase >= 1.0)
        v.phase -= 1.0;

    const double p = v.phase;
    const float waves[4] = {
        float(std::sin(twoPi * p)),                    // sine
        float(2.0 * std::abs(2.0 * p - 1.0) - 1.0),    // triangle
        float(2.0 * p - 1.0),                          // saw
        p < 0.5 ? 1.0f : -1.0f                         // square
    };

    const float pos = juce::jlimit(0.0f, 3.0f, v.morphPos * 3.0f);
    const int seg = juce::jmin(2, int(pos));
    const float frac = pos - float(seg);
    return waves[seg] + frac * (waves[seg + 1] - waves[seg]);
}

float SynthEngine::renderGranularPulse(Voice& v)
{
    (void) v; // raw noise burst; the fast decay comes from the short trigger decay time
    return random.nextFloat() * 2.0f - 1.0f;
}

float SynthEngine::renderGlitchStutter(Voice& v)
{
    v.stutterCounter -= 1;
    if (v.stutterCounter <= 0)
    {
        v.stutterHold = float(std::sin(twoPi * v.phase));
        v.stutterCounter = int(sampleRate * 0.03); // 30 ms hold
    }

    v.phase += v.phaseInc;
    if (v.phase >= 1.0)
        v.phase -= 1.0;
    return v.stutterHold;
}

float SynthEngine::renderDigitalCrush(Voice& v)
{
    v.phase += v.phaseInc;
    if (v.phase >= 1.0)
        v.phase -= 1.0;

    const float s = float(std::sin(twoPi * v.phase));
    constexpr float levels = 16.0f;
    return std::round(s * levels) / levels;
}

float SynthEngine::renderNoiseSweep(Voice& v)
{
    const float noise = random.nextFloat() * 2.0f - 1.0f;
    v.noiseLpState += v.noiseLpCutoff * (noise - v.noiseLpState);
    v.noiseLpCutoff *= 0.9999f; // cutoff sweeps down over the voice's life
    return v.noiseLpState * 3.0f;
}

float SynthEngine::renderSine(Voice& v)
{
    v.phase += v.phaseInc;
    if (v.phase >= 1.0)
        v.phase -= 1.0;
    return float(std::sin(twoPi * v.phase));
}
