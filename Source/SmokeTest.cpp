//==============================================================================
// DAY RUINER headless DSP smoke test.
// Builds as a JUCE console app and links ONLY the non-GUI sources:
// PluginProcessor.cpp + dsp/*.cpp + sequencer/*.cpp.
// PluginEditor.cpp and gui/*.cpp are excluded; see the DAYRUINER_HEADLESS_SMOKE_TEST
// guard on DayRuinerAudioProcessor::createEditor() in PluginProcessor.cpp.
//==============================================================================

#include <JuceHeader.h>

#include <cmath>
#include <iostream>

#include "PluginProcessor.h"

namespace
{
    constexpr double kSampleRate = 44100.0;
    constexpr int kBlockSize = 512;
    constexpr int kNumBlocks = 3000;
    constexpr int kCaptureSamples = 220500; // first 5 s of output

    bool writeWavFile (const juce::File& file, const juce::AudioBuffer<float>& buffer,
                       double sampleRate, int bitsPerSample = 16)
    {
        if (buffer.getNumSamples() <= 0 || buffer.getNumChannels() <= 0)
            return false;

        std::unique_ptr<juce::OutputStream> stream =
            std::make_unique<juce::FileOutputStream> (file);

        if (auto* fos = static_cast<juce::FileOutputStream*> (stream.get());
            fos == nullptr || ! fos->openedOk())
            return false;

        juce::WavAudioFormat wav;
        const auto options = juce::AudioFormatWriterOptions{}
            .withSampleRate (sampleRate)
            .withNumChannels (buffer.getNumChannels())
            .withBitsPerSample (bitsPerSample);

        auto writer = wav.createWriterFor (stream, options);
        if (writer == nullptr)
            return false;

        return writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
    }

    std::atomic<float>* rawParam (DayRuinerAudioProcessor& proc, const char* id)
    {
        return proc.apvts.getRawParameterValue (id);
    }
}

//==============================================================================
int main()
{
    std::cout << "=== DAY RUINER SMOKE TEST ===\n";

    //--- 1. Instantiate and prepare ------------------------------------------------
    DayRuinerAudioProcessor proc;
    proc.prepareToPlay (kSampleRate, kBlockSize);

    //--- 2. Synthesize a 2 s stereo test sample, write it to a temp WAV, load it --
    const int testLen = static_cast<int> (2.0 * kSampleRate);
    juce::AudioBuffer<float> testBuf (2, testLen);

    double phase = 0.0;
    const double envT = 0.1; // 100 ms raised-cosine attack/release
    for (int s = 0; s < testLen; ++s)
    {
        const double t = static_cast<double> (s) / static_cast<double> (testLen - 1);
        const double freq = 100.0 * std::pow (20.0, t); // 100 Hz -> 2000 Hz sweep
        phase += juce::MathConstants<double>::twoPi * freq / kSampleRate;

        const double ts = static_cast<double> (s) / kSampleRate;
        double env = 1.0;
        if (ts < envT)
            env = 0.5 * (1.0 - std::cos (juce::MathConstants<double>::pi * ts / envT));
        else if (ts > 2.0 - envT)
            env = 0.5 * (1.0 - std::cos (juce::MathConstants<double>::pi * (2.0 - ts) / envT));

        const float v = static_cast<float> (0.5 * std::sin (phase) * env);
        testBuf.setSample (0, s, v);
        testBuf.setSample (1, s, v);
    }

    const juce::File tempWav = juce::File::getSpecialLocation (juce::File::tempDirectory)
        .getChildFile ("dayruiner_smoke_sample.wav");

    if (! writeWavFile (tempWav, testBuf, kSampleRate))
    {
        std::cout << "FAIL: could not write temp test WAV\n";
        return 1;
    }

    const bool sampleLoaded = proc.getGranularEngine().loadTopSample (tempWav);
    std::cout << "sampleLoaded: " << (sampleLoaded ? "yes" : "NO") << "\n";

    //--- 3. Program the sequencer --------------------------------------------------
    auto& seq = proc.getSequencer();

    const int activeSteps[][2] = { {0,0},{0,4},{0,8},{0,12},
                                   {1,2},{1,6},{1,10},{1,14},
                                   {2,0},{2,8} };
    for (const auto& st : activeSteps)
    {
        auto& step = seq.getStep (st[0], st[1]);
        step.active = true;
        step.midiNote = 36 + st[1];                 // varied notes across steps
        step.velocity = 0.7f + 0.1f * static_cast<float> (st[0]);
    }

    // One probability trig condition...
    seq.getStep (0, 8).condition.type = DayRuiner::TrigCondition::Probability;
    seq.getStep (0, 8).condition.probability = 0.5f;
    // ...and one parameter lock.
    seq.getStep (1, 6).locks.setLock ("SYNTH_NOTE", 48.0f);

    // Pattern length 16 via the APVTS (AudioParameterInt: raw value = step count).
    *rawParam (proc, "SEQ_LENGTH") = 16.0f;

    //--- 4. Set initial algorithms -------------------------------------------------
    *rawParam (proc, "SYNTH_ALGO") = 3.0f;  // 3 = FMKick
    *rawParam (proc, "SAT_ALGO")   = 5.0f;  // 5 = Tape

    //--- 5. Run 3000 blocks --------------------------------------------------------
    juce::AudioBuffer<float> buffer (2, kBlockSize);
    juce::AudioBuffer<float> capture (2, kCaptureSamples);
    juce::MidiBuffer midi;

    long long nanCount = 0, infCount = 0;
    int blowupBlocks = 0;
    double peak = 0.0;
    int captured = 0;

    for (int b = 0; b < kNumBlocks; ++b)
    {
        if (b == 100)
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
        if (b == 200)
            midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);

        // Halfway: switch algorithms mid-run.
        if (b == 1500)
        {
            *rawParam (proc, "SYNTH_ALGO") = 8.0f;  // 8 = AdditiveBell
            *rawParam (proc, "SAT_ALGO")   = 2.0f;  // 2 = Wavefold
        }

        buffer.clear();
        proc.processBlock (buffer, midi);
        midi.clear();

        double blockPeak = 0.0;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const float* d = buffer.getReadPointer (ch);
            for (int s = 0; s < buffer.getNumSamples(); ++s)
            {
                const float v = d[s];
                if (std::isnan (v))
                    ++nanCount;
                else if (std::isinf (v))
                    ++infCount;

                const double a = std::fabs (static_cast<double> (v));
                if (a > blockPeak)
                    blockPeak = a;
            }
        }

        if (blockPeak > 10.0)
            ++blowupBlocks;
        if (blockPeak > peak)
            peak = blockPeak;

        if (captured < kCaptureSamples)
        {
            const int toCopy = juce::jmin (kBlockSize, kCaptureSamples - captured);
            for (int ch = 0; ch < 2; ++ch)
                capture.copyFrom (ch, captured, buffer, ch, 0, toCopy);
            captured += toCopy;
        }
    }

    //--- 7. Write the captured output ----------------------------------------------
    const juce::File outWav = juce::File::getSpecialLocation (juce::File::userHomeDirectory)
        .getChildFile ("workspace/dayruiner-prototype/build/smoke_output.wav");
    const bool wavWritten = writeWavFile (outWav, capture, kSampleRate);

    //--- 8. Summary ------------------------------------------------------------------
    std::cout << "blocks run: " << kNumBlocks << "\n";
    std::cout << "peak: " << peak << "\n";
    std::cout << "nanCount: " << nanCount << "\n";
    std::cout << "infCount: " << infCount << "\n";
    std::cout << "blowupBlocks: " << blowupBlocks << "\n";
    std::cout << "output wav: " << (wavWritten ? outWav.getFullPathName().toStdString()
                                              : std::string ("WRITE FAILED")) << "\n";

    bool pass = true;
    if (! sampleLoaded) { std::cout << "FAIL reason: test sample did not load\n"; pass = false; }
    if (nanCount > 0)   { std::cout << "FAIL reason: NaN samples in output\n";   pass = false; }
    if (infCount > 0)   { std::cout << "FAIL reason: inf samples in output\n";   pass = false; }
    if (blowupBlocks > 0){ std::cout << "FAIL reason: blocks with peak > 10.0\n"; pass = false; }

    std::cout << (pass ? "SMOKE TEST: PASS" : "SMOKE TEST: FAIL") << "\n";
    return pass ? 0 : 1;
}
