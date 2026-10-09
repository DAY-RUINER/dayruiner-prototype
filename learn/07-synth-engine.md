# Lesson 07 — The Synth Engine: 20 Algorithms, 16 Voices

If the granular engine is DAY RUINER's dreamy side, the synth engine is its
punchy side: drums, basses, leads, and digital weirdness, synthesized from
scratch — no samples needed. It lives in `Source/dsp/SynthEngine.h` (85
lines) and `Source/dsp/SynthEngine.cpp` (436 lines).

## The 20 algorithms

The `SYNTH_ALGO` parameter is a choice of 20, and **the order is a contract**:
the combo-box item order in `createParameterLayout()` (lesson 11) must match
`SynthEngine::Algorithm` index-for-index (0–19), or the GUI will trigger the
wrong sound. Both files carry warnings about this.

Grouped by family:

| # | Algorithm | Family | What it sounds like |
|---|---|---|---|
| 0 | Analog Kick | Analog drums | Pitch-swept sine thump |
| 1 | Analog Snare | Analog drums | Noisy burst + tonal body |
| 2 | Analog Hat | Analog drums | Highpassed noise tick |
| 3 | FM Kick | FM | Frequency-modulated drum punch |
| 4 | FM Snare | FM | FM snap with noise |
| 5 | FM Pluck | FM | Bright FM bass/key pluck |
| 6 | Wavetable Lead | Wavetable | Saw-ish lead from a 2048-sample table |
| 7 | Wavetable Pad | Wavetable | Detuned, evolving pad |
| 8 | Additive Bell | Additive/partial | Stacked sine partials, bell-like |
| 9 | Additive Organ | Additive/partial | Drawbar-ish harmonic stack |
| 10 | Modal Membrane | Modal/partial | Drum-head-like resonant modes |
| 11 | Modal String | Modal/partial | Plucked-string-like modes |
| 12 | Phase Distortion | Digital | Casio-style phase-warped timbres |
| 13 | Vector Morph | Digital | Crossfade between waveshapes |
| 14 | Granular Pulse | Digital | Grainy pulsed texture |
| 15 | Glitch Stutter | Digital | Repeating stuttered fragments |
| 16 | Digital Crush | Digital | Bitcrushed lo-fi |
| 17 | Noise Sweep | Digital | Filtered noise sweep |
| 18 | Metal Tine | Metallic | Inharmonic metallic ping |
| 19 | Sub Sine | Bass | Pure sub-bass sine |

**Design note for learners:** you don't need 20 algorithms to understand the
engine. Each `render*()` method is one self-contained recipe: take the
voice's phase/envelope state, produce one sample. The *machinery* around them
(voices, triggering, stealing) is identical for all 20 — learn the machinery
once, then read any two or three render methods to see how different
synthesis techniques plug into it.

## Voices: the machinery

```cpp
static constexpr int MAX_VOICES = 16;

struct Voice
{
    bool active = false;
    int noteNumber = -1;
    float velocity = 1.0f;
    double phase = 0.0, phaseInc = 0.0;
    double modPhase = 0.0, modPhaseInc = 0.0;
    float modIndex = 0.0f;
    float env = 0.0f, envDecay = 0.999f;
    int startOffsetSamples = 0;   // sample-accurate trigger offset within the block
    Algorithm algo = Algorithm::AnalogKick;
    static constexpr int MAX_PARTIALS = 16;
    float partialFreqs[MAX_PARTIALS] = {};
    float partialAmps[MAX_PARTIALS] = {};
    float partialPhases[MAX_PARTIALS] = {};
    float hatLastIn = 0.0f, hatLastOut = 0.0f;  // per-voice hat filter state (bug fix: no statics)
    ...
};
```

A **voice** is one currently-sounding note: its pitch (`phase`/`phaseInc`),
its loudness envelope (`env`), which algorithm renders it, and *when in the
block it starts* (`startOffsetSamples`).

### Triggering (with sample-accurate offsets)

```cpp
void trigger(int midiNote, float velocity, Algorithm algo,
             float decayTimeSeconds, int startOffsetSamples = 0);
```

`trigger()` finds a free voice — or **steals** one:

```cpp
// Find a free voice; otherwise steal the one with the smallest envelope.
int slot = -1;
float smallestEnv = 2.0f;
for (int i = 0; i < MAX_VOICES; ++i)
{
    if (!voices[i].active) { slot = i; break; }
    if (voices[i].env < smallestEnv) { smallestEnv = voices[i].env; slot = i; }
}
```

Stealing the voice with the **smallest envelope** (the quietest, most
decayed note) is the least audible choice — same philosophy as the granular
scheduler stealing the oldest grain. Then it resets *all* reusable state for
the new note:

**Bug fix (DEVIATIONS.md item 12):** `trigger()` zeroes the partial
frequency/amplitude/phase arrays. Voices are *recycled* — without zeroing, a
stolen voice that previously played Additive Bell would leak its old
partials into (say) a new Analog Kick. Any state a voice reuses must be
re-initialized on trigger. When you write your own voice code, ask: *"what
did the previous occupant leave behind?"*

Other trigger details:

- `v.startOffsetSamples` is clamped to the block (`juce::jlimit(0,
  blockMax - 1, …)`) — a corrupt offset must never read past the buffer.
- `modPhaseInc` is always initialized (item 9: Analog Snare's renderer used
  it but the old trigger never set it — uninitialized state is a
  dice-roll bug).
- The sample rate falls back to 44100 only if it's somehow non-positive
  (item 16) — another "never trust, always guard" instance.

### Rendering: one pass per block

```cpp
// Renders ALL active voices, ADDING into output.
// Voices begin sounding at their startOffsetSamples.
void renderInto(juce::AudioBuffer<float>& output, int numSamples);
```

One call renders every active voice for the whole block, each voice starting
at its offset. This is the method lesson 05's step 8 calls — the fix that
replaced per-sample 1-sample rendering. The ADDING contract matches the
granular engine: the caller clears, the renderer accumulates.

### Per-voice state: the static-local bug (item 10)

The most instructive synth bug: Analog Hat's filter used **function-static**
locals inside the per-sample render:

```cpp
// THE OLD (BROKEN) WAY — never do this:
static float lastIn = 0.0f, lastOut = 0.0f;  // shared across ALL voices AND channels!
```

`static` locals persist across calls and are **shared by every voice, every
channel, every note** — voice A's filter state would bleed into voice B, and
two channels would corrupt each other. The fix: the filter state lives in
the `Voice` struct (`hatLastIn`/`hatLastOut`), so each voice has its own.
**Rule: on the audio thread, mutable state belongs to the voice (or the
instance), never to a `static` local.** The saturator had the identical bug
(lesson 09).

### Partial-based algorithms: `renderPartials()`

Four algorithms (Additive Bell, Additive Organ, Modal Membrane, Modal
String) all work the same way — a bank of up to 16 sine partials, each with
its own frequency ratio, amplitude, and phase — so their renderers were
**merged into one `renderPartials()`** (item 11) instead of four near-duplicate
functions. The *difference* between a bell and a string is just data: which
ratios and amplitudes `trigger()` loads into the partial arrays.

One subtle fix inside (item 13): partial phases wrap with `floor()`, not a
simple `if (phase > 1) phase -= 1`, because frequency ratios above 1.0 can
advance the phase by *more than a full cycle per sample* — a single
subtraction wouldn't wrap far enough.

### Wavetables

`buildWavetables()` (called once in `prepare()`) fills two 2048-sample
lookup tables — a saw and a "squareish" wave. `readWavetable()` reads them
with interpolation. Precomputing the tables once is the standard trick:
`std::sin()` per sample per voice is expensive; a table lookup is cheap.

### Pitch safety: the Analog Kick clamp (item 14)

```cpp
// pitch sweep clamped: juce::jlimit(0.5, 8) on the sweep ratio
```

The kick's pitch envelope sweeps downward from a multiple of the base
frequency. Without the clamp, extreme decay settings could push the sweep
ratio to absurd values (aliasing howl or `NaN`). **Any user-controlled value
that enters a division, a frequency, or an exponent gets clamped.** You'll
see `juce::jlimit` throughout the DSP code — it's the seatbelt.

## Check your understanding

- **20 algorithms** in 6 families; the GUI combo order must match the enum
  order 0–19 exactly.
- **16 voices**; `trigger()` steals the voice with the **smallest
  envelope** when all are busy.
- **Sample-accurate timing** via `startOffsetSamples`; one `renderInto()`
  per block renders all voices (never per-sample temp buffers).
- **Recycled voices must be fully reset** — `trigger()` zeroes partial
  arrays so stolen voices don't leak old state.
- **No `static` mutable locals on the audio thread** — filter state lives
  per-voice (the Analog Hat fix).
- Partial phases wrap with `floor()` (ratios > 1 can jump more than a cycle
  per sample); user-driven values get `juce::jlimit` clamps.
- Next: lesson 08 — the sequencer: musical time.
