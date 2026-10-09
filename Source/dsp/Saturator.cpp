#include "Saturator.h"

#include <cmath>

void Saturator::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
}

void Saturator::reset()
{
    phaseStateIn[0] = 0.0f;
    phaseStateIn[1] = 0.0f;
    phaseStateOut[0] = 0.0f;
    phaseStateOut[1] = 0.0f;
}

void Saturator::setAlgorithm (Algorithm a)
{
    algorithm = a;
}

void Saturator::setDrive (float driveDb_)
{
    driveDb = juce::jlimit (0.0f, 24.0f, driveDb_);
}

void Saturator::setMix (float mix01)
{
    mix = juce::jlimit (0.0f, 1.0f, mix01);
}

void Saturator::process (juce::AudioBuffer<float>& buffer)
{
    const float driveDbC = juce::jlimit (0.0f, 24.0f, driveDb);
    const float mixC = juce::jlimit (0.0f, 1.0f, mix);
    const float driveGain = juce::Decibels::decibelsToGain (driveDbC);
    const float dryGain = 1.0f - mixC;

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = buffer.getWritePointer (ch);

        switch (algorithm)
        {
            case Algorithm::None:
            {
                for (int i = 0; i < numSamples; ++i)
                {
                    const float dry = data[i];
                    const float wet = dry * driveGain;
                    data[i] = dry * dryGain + wet * mixC;
                }
                break;
            }

            case Algorithm::Bitcrush:
            {
                const float bits = juce::jmap (driveDbC, 0.0f, 24.0f, 16.0f, 2.0f);
                const float levels = std::pow (2.0f, bits);
                for (int i = 0; i < numSamples; ++i)
                {
                    const float dry = data[i];
                    const float driven = dry * driveGain;
                    const float wet = std::round (driven * levels) / levels;
                    data[i] = dry * dryGain + wet * mixC;
                }
                break;
            }

            case Algorithm::RateCrush:
            {
                const int holdSamples = juce::jmax (1, static_cast<int> (
                    juce::jmap (driveDbC, 0.0f, 24.0f, 1.0f, 64.0f)));
                // Per-channel hold state, declared inside the channel loop
                // (resets deterministically at the start of each channel).
                float lastHeld = 0.0f;
                int holdCounter = 0;
                for (int i = 0; i < numSamples; ++i)
                {
                    const float dry = data[i];
                    const float driven = dry * driveGain;
                    if (holdCounter == 0)
                        lastHeld = driven;
                    holdCounter = (holdCounter + 1) % holdSamples;
                    const float wet = lastHeld;
                    data[i] = dry * dryGain + wet * mixC;
                }
                break;
            }

            case Algorithm::Wavefold:
            {
                for (int i = 0; i < numSamples; ++i)
                {
                    const float dry = data[i];
                    const float driven = dry * driveGain;
                    const float wet = std::sin (driven * juce::MathConstants<float>::halfPi);
                    data[i] = dry * dryGain + wet * mixC;
                }
                break;
            }

            case Algorithm::Phase:
            {
                const float coeff = juce::jlimit (-0.99f, 0.99f, driveDbC / 24.0f);
                const int stateCh = ch % 2; // up to 2 channels of all-pass state
                for (int i = 0; i < numSamples; ++i)
                {
                    const float dry = data[i];
                    const float driven = dry * driveGain;
                    const float lastIn = phaseStateIn[stateCh];
                    const float lastOut = phaseStateOut[stateCh];
                    const float wet = coeff * driven + lastIn - coeff * lastOut;
                    phaseStateIn[stateCh] = driven;
                    phaseStateOut[stateCh] = wet;
                    data[i] = dry * dryGain + wet * mixC;
                }
                break;
            }

            case Algorithm::Tape:
            {
                for (int i = 0; i < numSamples; ++i)
                {
                    const float dry = data[i];
                    const float driven = dry * driveGain;
                    const float wet = std::tanh (driven) + 0.05f * std::sin (driven * 3.0f);
                    data[i] = dry * dryGain + wet * mixC;
                }
                break;
            }

            case Algorithm::Tube:
            {
                for (int i = 0; i < numSamples; ++i)
                {
                    const float dry = data[i];
                    const float driven = dry * driveGain;
                    const float wet = driven >= 0.0f
                        ? std::tanh (driven)
                        : std::tanh (driven * 1.5f) * 0.8f;
                    data[i] = dry * dryGain + wet * mixC;
                }
                break;
            }
        }
    }

    // Output trim compensates for the drive gain.
    buffer.applyGain (juce::Decibels::decibelsToGain (-driveDbC * 0.5f));
}
