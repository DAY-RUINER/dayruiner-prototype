# Lesson 06 — The Granular Engine: Clouds of Sound

**Granular synthesis** is one of the most beautiful ideas in electronic
music: take any sound, chop it into dozens or hundreds of tiny fragments
("grains", typically 1–100 ms long), and play them back overlapping,
scattered across time and pitch. The result is a shimmering cloud that still
faintly remembers its source — the signature sound of ambient and glitch
music.

DAY RUINER's granular engine lives in four files under `Source/dsp/`:

| File | Role |
|---|---|
| `Grain.h` | A single grain voice: playback state + envelope math |
| `GrainScheduler.{h,cpp}` | Fires grains on a timer; renders up to 199 at once |
| `GranularEngine.{h,cpp}` | Two layers (top/bottom samples) + live recording |
| `SampleBuffer.{h,cpp}` | Sample storage, resampling, interpolated playback |

## What a grain is (`Grain.h`)

A `Grain` is a small struct — no allocation, no virtual functions, all state
mutations on the audio thread:

```cpp
struct Grain
{
    double readPosition = 0.0;      // where in the source sample we're reading
    double playbackRate = 1.0;      // 1.0 = normal speed/pitch
    int totalLengthSamples = 0;     // grain duration
    int elapsedSamples = 0;         // how far through this grain we are
    float peakAmplitude = 1.0f;
    float currentEnvelope = 0.0f;
    ...
    bool active = false;
};
```

Each grain is a *read head* sweeping through the source sample. `tick()` —
called exactly once per sample — advances it:

```cpp
// Advances the grain by exactly one sample. sampleRate must be the
// engine's actual rate (bug fix: never hardcode 44100).
void tick(double sampleRate) noexcept
```

**Bug fix (DEVIATIONS.md item 2):** the sketch hardcoded 44100 Hz when
advancing the ring-modulation phase. At any other sample rate (48000 is
extremely common), the ring-mod pitch would be wrong — the phase increment
`2π·f/sr` depends on the *actual* rate. `tick()` now takes `sampleRate` as a
parameter. This is a classic audio-programming trap: **never assume 44100**.

### Envelopes: why grains don't click

If you just cut a chunk out of a waveform and play it, you hear a click at
each cut — the waveform jumps discontinuously. The fix is an **envelope**: a
smooth fade-in/fade-out window applied to each grain. `Grain::computeEnvelope()`
offers four window shapes:

- **Hann** (default) — the classic raised-cosine bell: `0.5·(1−cos(2πt))`.
  Smooth at both ends, zero at both ends. The safest choice.
- **Linear** — triangle ramp with a configurable attack fraction.
- **Tukey** — flat in the middle, cosine-tapered ends (a compromise between
  Hann and a rectangle).
- **Blackman** — a fancier bell with even lower sidelobes.

The envelope is why 199 overlapping grains sound like a cloud instead of
199 clicks.

### Ring modulation, per grain

Each grain can also ring-modulate its output — multiply the grain by a sine
wave at `ringModFrequency`. The formula chosen (after deliberation —
DEVIATIONS.md item 6):

```cpp
const float ring = (1.0f - g.ringModDepth)
                 + g.ringModDepth * static_cast<float>(std::sin(g.ringModPhase));
```

At depth 0 this is exactly 1.0 (no effect); at depth 1 it's a pure sine
multiplier (classic metallic ring-mod). In between it's a crossfade between
dry and modulated — so the parameter is always safe and musical.

## `SampleBuffer`: reading samples smoothly (`SampleBuffer.h`)

A grain's `readPosition` is a *fractional* sample index (e.g. 440.7) —
playback rates other than 1.0 land between samples. `readSample()` must
**interpolate**: estimate the value between the stored samples. Four modes:

| Mode | Quality | Cost | When to use |
|---|---|---|---|
| Linear | OK, slightly dull | Cheapest | Low CPU, preview |
| Cubic (Catmull-Rom) | Good | Moderate | General use |
| **Hermite** (default) | Very good | Moderate | The default — best quality-per-CPU |
| Sinc8 | Excellent | Expensive (8-tap windowed sinc) | Offline-quality rendering |

**Why Hermite is the default:** linear interpolation audibly dulls high
frequencies (it can't reconstruct sharp transients); 8-tap sinc is
near-perfect but costs 8 multiplies per read per grain per sample — with 199
grains that's significant. Hermite sits in the sweet spot.

`SampleBuffer` also **resamples on load**: if you load a 48000 Hz file into a
44100 Hz session, `resampleTo()` converts it once at load time, so the audio
thread never has to think about it. Loading happens on the message thread;
only `readSample()` — `noexcept`, allocation-free — runs on the audio thread.
That split (expensive setup on UI thread, cheap reads on audio thread) is the
fundamental pattern of real-time audio programming.

## The scheduler: fire + render in one pass (`GrainScheduler.h`)

This is where the most instructive bug lived. The sketch had **two**
per-sample passes: `process()` ticked every grain, then `renderInto()` ticked
every grain *again* — so each grain advanced **two samples per sample**
(double speed, double envelope speed, half the intended density).

The fix (`GrainScheduler::processBlock`, DEVIATIONS.md item 1): **one pass**
that fires new grains *and* renders all active grains, so each grain's
`tick()` runs exactly once per sample:

```cpp
// Fires grains AND renders them in ONE pass: each active grain advances
// exactly once per sample. ADDS into output (caller clears).
void processBlock(int numSamples, const SampleBuffer& source,
                  juce::AudioBuffer<float>& output,
                  SampleBuffer::InterpolationMode interpMode,
                  std::function<void(Grain&)> onGrainFired);
```

Other scheduler details worth studying:

- **Voice stealing:** 199 grain slots (`MAX_GRAINS`). When all are busy, the
  scheduler steals the grain with the **largest `elapsedSamples`** — the
  oldest grain, closest to finishing anyway. Stealing the *oldest* (not a
  random or newest one) is the least audible choice.
- **Fire rate:** grains fire on a timer (`samplesUntilNextFire`), with the
  period derived from the grain-frequency parameter (0.1–100 Hz, logarithmic).
- **No-op on empty source:** if no sample is loaded, the scheduler does
  nothing — it never reads from an empty buffer (DEVIATIONS.md item 8).
- **Parameter caching:** `setGrainLengthNormalized()` initially *reset the
  grain frequency* as a side effect (confusing coupling — item 5). Now each
  layer caches its own length/frequency and applies them independently.
- **Unguarded math:** every division and int cast in the scheduler is now
  guarded with clamps (item 4) — a division by a zero or near-zero timer
  period would produce `inf`/`NaN` and poison the whole mix.

## Two layers + blend (`GranularEngine.h`)

The engine holds **two complete sample layers** — `topSource`/`topEngine`
and `bottomSource`/`bottomEngine` — each with its own scheduler, length, and
frequency. `process()` renders both into pre-sized scratch buffers and mixes
them with an equal-power blend controlled by `GRAIN_BLEND`.

**Memory discipline** (DEVIATIONS.md item 3): the sketch had confused
`setSize`/`clear` ordering per block — resizing (potentially allocating) on
the audio thread. Now `topBuffer`/`bottomBuffer` are **members sized once in
`prepare()`**; `process()` only clears the active region per block. This is
the same "allocate in prepare, clear in process" pattern from lesson 05.

Parameter mapping is musical: `setGrainLengthNormalized(0..1)` maps
logarithmically to **1 ms–2000 ms**, and `setGrainFrequencyNormalized(0..1)`
maps logarithmically to **0.1 Hz–100 Hz**. Logarithmic because human hearing
is logarithmic — a linear knob would waste 90% of its travel on inaudibly
long grains.

## Live recording

```cpp
void startLiveRecording();   // allocates the 120 s record buffer — UI thread only
void captureLiveInput (const juce::AudioBuffer<float>& input, int numSamples); // audio thread
```

Press Record in the GUI and the engine captures the plugin's input (the
pre-clear snapshot from lesson 05's step 1!) into a 120-second stereo
buffer. Stop, and the recording becomes granulation source material. Note the
threading split again: `startLiveRecording()` allocates (UI thread is allowed
to), `captureLiveInput()` only writes into the pre-allocated buffer.

## Check your understanding

- A **grain** is a short read-head sweep through a sample; an **envelope**
  (Hann default) prevents clicks at grain boundaries.
- `SampleBuffer::readSample()` **interpolates** fractional positions;
  **Hermite** is the default quality/speed tradeoff; resampling happens once
  at load time.
- The scheduler's defining fix: **fire + render in a single per-sample
  pass** — the sketch double-ticked every grain.
- Full scheduler: 199 voices, steal-oldest policy, guarded math, no-op on
  empty source, per-layer parameter caching.
- The engine runs **two layers** blended by `GRAIN_BLEND`, with pre-sized
  scratch buffers (never resized on the audio thread).
- `Grain::tick()` takes the **actual sample rate** — never hardcode 44100.
- Live recording: allocate on the UI thread, capture on the audio thread.
- Next: lesson 07 — the synth engine and its 20 algorithms.
