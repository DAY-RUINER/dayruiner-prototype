#pragma once

#include <JuceHeader.h>

// Sample storage + resampling + interpolated readout for the granular engine.
// Loading happens on the message thread; readSample() is the only method
// called on the audio thread (it is noexcept and allocation-free).
class SampleBuffer
{
public:
    enum class InterpolationMode { Linear, Cubic, Hermite, Sinc8 };

    // Loads an audio file, resampling to targetSampleRate when it differs.
    // Returns false if the file cannot be read; buffer is unchanged on failure.
    bool loadFromFile(const juce::File& file, double targetSampleRate, int maxDurationSeconds = 960);

    // Copies from an in-memory buffer, resampling from sourceSampleRate.
    void loadFromBuffer(const juce::AudioBuffer<float>& incoming, double sourceSampleRate, double targetSampleRate);

    // Interpolated sample read with wraparound. position is in samples.
    float readSample(int channel, double position, InterpolationMode mode = InterpolationMode::Hermite) const noexcept;

    bool getIsLoaded() const noexcept { return isLoaded; }
    int getNumSamples() const noexcept { return audioData.getNumSamples(); }
    int getNumChannels() const noexcept { return audioData.getNumChannels(); }
    double getSampleRate() const noexcept { return sampleRate; }
    juce::String getFileName() const { return fileName; }
    const juce::AudioBuffer<float>& getAudioData() const noexcept { return audioData; }

private:
    juce::AudioBuffer<float> audioData;
    double sampleRate = 44100.0;
    juce::String fileName;
    bool isLoaded = false;

    void resampleTo(double targetSampleRate);
    float readLinear(int ch, double pos) const noexcept;
    float readCubic(int ch, double pos) const noexcept;   // Catmull-Rom
    float readHermite(int ch, double pos) const noexcept;
    float readSinc8(int ch, double pos) const noexcept;   // 8-tap Blackman-Harris windowed sinc
};
