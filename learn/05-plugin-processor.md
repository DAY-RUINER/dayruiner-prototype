# Lesson 05 — The Plugin Processor: Heart of the Plugin

`Source/PluginProcessor.h` (97 lines) and `Source/PluginProcessor.cpp` (350
lines) are the most important files in the project. Everything else is an
engine the processor *owns*; this is the file the **host** (your DAW) talks
to. If you understand this file, you understand the plugin.

## The `AudioProcessor` lifecycle: how a host drives your plugin

JUCE's `AudioProcessor` base class defines a contract — a set of virtual
methods the host calls at specific moments. Our class
`DayRuinerAudioProcessor` implements them:

1. **Construction** — the host creates your processor once. Our constructor
   sets up the stereo-in/stereo-out buses and builds the `apvts` parameter
   tree (lesson 11).
2. **`prepareToPlay(sampleRate, samplesPerBlock)`** — called when playback is
   about to start (and again if the sample rate or block size changes). **This
   is where you allocate.** Every engine's `prepare()` runs here, exactly
   once, and all scratch buffers are sized here. After this returns, the
   audio thread is live and you must never allocate again.
3. **`processBlock(buffer, midiMessages)`** — called over and over, dozens of
   times per second, for as long as audio runs. Each call hands you a buffer
   of ~64–2048 samples to fill with sound. **This is the hot path**: no
   allocation, no locks, no waiting. Everything in lessons 06–09 lives to
   serve this method.
4. **`releaseResources()`** — called when playback stops for good. Ours is
   empty; we have nothing to tear down.
5. **`getStateInformation` / `setStateInformation`** — the host calls these
   to save/load presets and DAW project data. Ours serializes the parameters
   plus the sequencer and all 128 patterns.

There's also `createEditor()` (builds the GUI window on demand),
`isBusesLayoutSupported()` (we accept stereo-in/stereo-out only), and the
program/preset stubs (we report a single "Default" program).

One JUCE 9 detail worth knowing: the base class has **no `isSynth()`** method
(it was removed), so ours is a plain non-override query kept because project
code asks for it — the header says so explicitly:

```cpp
// NOTE: juce::AudioProcessor has no isSynth() in JUCE 9, so this cannot be an
// override. It is kept as a plain query because project code asks for it.
bool isSynth() const;
```

And because we override the single-precision `processBlock` but not the
double-precision overload, we un-hide the base version to silence
`-Woverloaded-virtual`:

```cpp
using juce::AudioProcessor::processBlock; // un-hide the double-precision overload
```

(`DEVIATIONS.md` item 35 — a one-line fix for a real warning.)

## `prepareToPlay`: allocate once, never on the audio thread

```cpp
void DayRuinerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    ...
    granularEngine.prepare (sampleRate, samplesPerBlock);
    synthEngine.prepare (sampleRate, samplesPerBlock);
    saturator.prepare (sampleRate);
    delay.prepare (sampleRate);
    delay.setEuclideanPattern (3, 8);   // fixed 3-in-8 rhythm until the UI exists
    ...
    reverb.prepare (spec);
    sequencer.prepare (sampleRate, 120.0);
    patternManager.prepare();
    sequencer.setPatternManager (&patternManager);

    // Pre-size scratch buffers to the maximum block size so processBlock never reallocates.
    synthBuffer.setSize (numOut, samplesPerBlock, false, false, false);
    grainBuffer.setSize (numOut, samplesPerBlock, false, false, false);
    inputCopy.setSize (numIn, samplesPerBlock, false, false, false);
}
```

Three things to internalize:

1. **Every engine is prepared here, exactly once.** The original sketch
   called `synthEngine.prepare()` *inside `processBlock`, every block* —
   reallocating wavetables 100+ times a second on the audio thread. That's
   bug fix (c) below: preparation is a setup-time job, full stop.
2. **Scratch buffers are member variables sized once.** `synthBuffer`,
   `grainBuffer`, and `inputCopy` are allocated here and merely *cleared* per
   block later. `juce::AudioBuffer::setSize()` can allocate — that's why it
   lives here and never in `processBlock`.
3. **`sequencer.prepare(sampleRate, 120.0)`** passes a dummy 120 BPM; the
   real tempo is pushed fresh every block from the host playhead (bug fix (b)
   below).

## `processBlock`: the 14 steps, in order

Here is the entire hot path, annotated. Read it slowly — this is the single
most important 100 lines in the project.

**Step 0 — Denormal protection.**
```cpp
juce::ScopedNoDenormals noDenormals;
```
Tiny ("denormal") floating-point numbers can make some CPUs 10–100× slower
per operation. This RAII guard flips the CPU into flush-to-zero mode for the
block. (Lesson 14 explains denormals properly.)

**Step 1 — Snapshot the live input BEFORE clearing.**
```cpp
const int snapCh = juce::jmin (inputCopy.getNumChannels(), numCh);
for (int ch = 0; ch < snapCh; ++ch)
    inputCopy.copyFrom (ch, 0, buffer, ch, 0, numSamples);

buffer.clear();
```
**Bug fix (a):** the sketch called `buffer.clear()` *first*, which erased the
incoming audio — and the granular engine's live sampler then recorded
silence. The input must be copied into `inputCopy` before the clear destroys
it. Order matters enormously on the audio thread: the incoming `buffer`
contains this block's live input, and `clear()` is destructive.

**Step 2 — Read every parameter once, via lock-free atomics.**
```cpp
const float grainTopLen = apvts.getRawParameterValue ("GRAIN_TOP_LENGTH")->load();
...
const int synthAlgoIdx  = juce::jlimit (0, 19, juce::roundToInt (...));
```
All 19 parameters are loaded once per block into locals.
`getRawParameterValue()` returns a pointer to a `std::atomic<float>`; the
`->load()` is a lock-free read, safe on the audio thread. Reading once per
block (not per sample) is both faster and *consistent*: the whole block uses
one coherent set of values even if the user drags a knob mid-block. The
`juce::jlimit` clamps keep choice-parameter indices in range even if
automation sends something odd.

**Step 3 — Get the host tempo (bug fix (b)).**
```cpp
double bpm = 120.0;
if (auto* playhead = getPlayHead())
    if (const auto pos = playhead->getPosition())
        bpm = pos->getBpm().orFallback (120.0);
```
The sketch hardcoded 120 BPM. Real plugins ask the host: `getPlayHead()`
may return null (no host transport, e.g. standalone with no playback), and
`getBpm()` returns an `Optional<double>` that may be empty — hence
`.orFallback(120.0)`. Defensive, layered, and correct: use the host tempo
when it exists, 120 otherwise. (Note the JUCE 9 `if`-with-initializer idiom.)

**Step 4 — Push parameters into the engines.**
Plain setters (`granularEngine.setBlend(blend)`, `saturator.setDrive(…)`,
`delay.setBpm(bpm)`, …). No allocation — just storing floats. Note the
documented simplifications: `delay.setEuclideanPattern(3, 8)` stays fixed
(no UI for the rhythm shape yet) and reverb width is hardcoded to `1.0f`.

**Step 5 — Feed live input to the recorder.**
```cpp
if (granularEngine.isLiveRecording())
    granularEngine.captureLiveInput (inputCopy, numSamples);
```
Uses the pre-clear snapshot from step 1. `isLiveRecording()` is an atomic
flag the GUI toggles.

**Step 6 — Handle MIDI input with sample-accurate offsets.**
```cpp
keyboardState.processNextMidiBuffer (midiMessages, 0, numSamples, true);

for (const auto meta : midiMessages)
{
    const auto msg = meta.getMessage();
    if (msg.isNoteOn())
        synthEngine.trigger (msg.getNoteNumber(),
                             msg.getVelocity() / 127.0f,
                             synthAlgo, synthDecay, meta.samplePosition);
}
```
Each MIDI event carries `meta.samplePosition` — *where in this block* the
note happened. `trigger()` stores it as the voice's `startOffsetSamples`, so
a note arriving at sample 300 of a 512-sample block starts sounding at
sample 300, not at the block edge. That's **sample-accurate timing**, and
it's why the synth renders in one pass (next steps) instead of per-sample.

**Step 7 — Run the sequencer, once per sample.**
```cpp
for (int s = 0; s < numSamples; ++s)
{
    const DayRuiner::FiredTrig ft = sequencer.processSample();

    if (ft.fired && ft.step != nullptr)
    {
        ...
        synthEngine.trigger (note, ft.velocity, synthAlgo, synthDecay, s);
        // grain parameter-locks apply immediately to the whole block
        // (documented timing slop — see DEVIATIONS.md)
        ...
    }
}
```
**Bug fix (bigger than it looks):** the sketch rendered the synth
*per-sample into a 1-sample temp buffer* — calling the full voice renderer
512 times per block with all its setup overhead. Instead: `trigger()` just
queues voices with their offset `s`, and rendering happens once per block
(step 8). Same sample accuracy, a fraction of the cost.

Also note the honest simplification in the comment: sequencer parameter
locks for grain settings apply to the whole block rather than at the exact
trigger sample. A few milliseconds of timing slop, documented, acceptable
for v1.

**Steps 8–9 — Render both engines into cleared scratch buffers.**
```cpp
synthBuffer.clear (0, numSamples);
synthEngine.renderInto (synthBuffer, numSamples);

grainBuffer.clear (0, numSamples);
granularEngine.process (grainBuffer, numSamples);
```
Both render methods **ADD** into the buffer (that's their contract — "caller
clears"), so each scratch buffer is cleared first. Voices begin at their
stored offsets, so MIDI and sequencer timing from steps 6–7 lands exactly.

**Step 10 — Blend the two engines.**
```cpp
for (int ch = 0; ch < numCh; ++ch)
{
    auto* out = buffer.getWritePointer (ch);
    ...
    for (int s = 0; s < numSamples; ++s)
        out[s] = gIn[s] * grainGain + sIn[s] * blend;
}
```
A linear crossfade controlled by `GRAIN_BLEND`: 0 = all grains, 1 = all
synth. (Linear, not equal-power — there's a slight dip at 50/50. Simple and
predictable; documented in the code.)

**Step 11 — Optional input monitoring.**
```cpp
if (monitorInput)
    buffer.addFrom (ch, 0, inputCopy, ch, 0, numSamples, 0.5f);
```
Mixes the live input back in at half volume, post-blend but pre-FX. Toggled
by the GUI's "Monitor Input" button.

**Steps 12–13 — FX chain + safety gain.**
```cpp
saturator.process (buffer);
delay.process (buffer);
reverb.process (buffer);
buffer.applyGain (0.7f);
```
In-place, in series: saturator → euclidean delay → lush reverb (lessons 09),
then a 0.7 safety gain so stacked layers can't clip the host. **Order is
deliberate**: distortion before time-based effects is the classic chain —
you distort the dry sound, then smear it with delay and reverb.

## State save/load: don't lose the user's patterns

```cpp
void DayRuinerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Stash live sequencer edits into the current pattern first...
    auto& pat = patternManager.getPattern (...);
    for (int t = 0; t < ...; ++t)
        for (int s = 0; s < ...; ++s)
            pat.tracks[t][s] = sequencer.getStep (t, s);
    ...
    auto state = apvts.copyState();
    state.appendChild (sequencer.toValueTree(), nullptr);
    state.appendChild (patternManager.toValueTree(), nullptr);
    ...
}
```

**Bug fix worth studying:** pattern edits live in the *sequencer* (the grid
edits live steps), and are only stored into the *pattern* when you switch
patterns. Without the "stash" loop at the top, saving a DAW project would
silently drop every unsaved grid edit. `setStateInformation` does the
reverse: restores parameters, patterns, then reloads the current pattern's
steps back into the live sequencer. (`DEVIATIONS.md` item 31.)

## The three processor bugs, summarized

| # | The bug | The fix | Where |
|---|---|---|---|
| (a) | `buffer.clear()` erased live input before the sampler could record it | Snapshot input into `inputCopy` **before** clearing | `processBlock`, steps 1 |
| (b) | Tempo hardcoded to 120 BPM | Read host playhead BPM with `.orFallback(120.0)` | `processBlock`, step 3 |
| (c) | `synthEngine.prepare()` called every block (reallocating constantly) | Prepare **once** in `prepareToPlay` | `prepareToPlay` |

Plus the performance fix: per-sample 1-sample synth rendering → trigger
with offsets + one `renderInto` per block.

## Check your understanding

- The host lifecycle is: **construct → `prepareToPlay` (allocate) →
  `processBlock` (render, repeatedly) → `releaseResources`**; plus
  state save/load and `createEditor`.
- `prepareToPlay` prepares every engine **once** and sizes all scratch
  buffers; `processBlock` must never allocate.
- `processBlock` order: snapshot input → clear → read params (atomics, once)
  → host BPM → push params → live-sample → MIDI (offsets) → sequencer loop
  → render synth + grains → blend → monitor → saturator → delay → reverb →
  ×0.7.
- Parameters are read **once per block** via atomic loads — lock-free and
  consistent.
- Sample-accurate timing comes from **trigger offsets**, not per-sample
  rendering.
- `getStateInformation` stashes live grid edits into the pattern before
  serializing, or edits would be lost.
- Next: lesson 06 — the granular engine.
