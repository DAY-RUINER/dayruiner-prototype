# Lesson 11 — APVTS Parameters: The Plugin's Control Surface

**APVTS** = `AudioProcessorValueTreeState`. It's JUCE's system for plugin
parameters — the knobs your DAW can automate, the values saved in presets.
Every parameter in DAY RUINER is declared in one function,
`DayRuinerAudioProcessor::createParameterLayout()` in
`Source/PluginProcessor.cpp`, and there are **19** of them.

Why a central registry instead of loose variables? Because parameters have
*four* consumers that must all agree: the **GUI** (knobs), the **host**
(automation), **presets** (save/load), and the **audio thread** (DSP). APVTS
is the single source of truth they all share — with lock-free atomic reads
for the audio thread (lesson 05, step 2) and attachments for the GUI (lesson
10).

## The 19 parameters

Declared with `layout.add(std::make_unique<juce::AudioParameterFloat>(…))`
(and `…Int` / `…Choice` variants). Each takes: **ID** (the permanent,
code-facing name), **display name** (what the DAW shows), **range**, and
**default**.

### Granular engine (5)

| ID | Type | Range | Default | Maps to |
|---|---|---|---|---|
| `GRAIN_TOP_LENGTH` | float | 0–1 | 0.3 | Top grain length, log-mapped 1 ms–2000 ms |
| `GRAIN_TOP_FREQ` | float | 0–1 | 0.5 | Top grain fire rate, log-mapped 0.1–100 Hz |
| `GRAIN_BOTTOM_LENGTH` | float | 0–1 | 0.3 | Bottom layer grain length |
| `GRAIN_BOTTOM_FREQ` | float | 0–1 | 0.5 | Bottom layer grain fire rate |
| `GRAIN_BLEND` | float | 0–1 | 0.5 | Top/bottom layer blend |

### Synth engine (2)

| ID | Type | Range | Default | Notes |
|---|---|---|---|---|
| `SYNTH_ALGO` | choice (20) | 0–19 | 0 (Analog Kick) | **Item order must match `SynthEngine::Algorithm`** |
| `SYNTH_DECAY` | float | 0.01–4.0 s | 0.5 | Note decay time |

The 20 choice items, in order: Analog Kick, Analog Snare, Analog Hat, FM
Kick, FM Snare, FM Pluck, Wavetable Lead, Wavetable Pad, Additive Bell,
Additive Organ, Modal Membrane, Modal String, Phase Distortion, Vector
Morph, Granular Pulse, Glitch Stutter, Digital Crush, Noise Sweep, Metal
Tine, Sub Sine.

### Saturator (3)

| ID | Type | Range | Default | Notes |
|---|---|---|---|---|
| `SAT_ALGO` | choice (7) | 0–6 | 0 (None) | **Item order must match `Saturator::Algorithm`** |
| `SAT_DRIVE` | float | 0–24 dB | 0.0 | Drive into the shaper |
| `SAT_MIX` | float | 0–1 | 1.0 | Dry/wet mix |

Choice items: None, Bitcrush, Rate Crush, Wavefold, Phase, Tape, Tube.

### Delay (4)

| ID | Type | Range | Default | Notes |
|---|---|---|---|---|
| `DELAY_DIV_NUM` | int | 1–16 | 1 | Delay division numerator |
| `DELAY_DIV_DEN` | int | 1–16 | 4 | Delay division denominator (1/4 note default) |
| `DELAY_FB` | float | 0–0.95 | 0.4 | Feedback (capped below 1.0 — see lesson 09) |
| `DELAY_MIX` | float | 0–1 | 0.2 | Dry/wet mix |

### Reverb (3)

| ID | Type | Range | Default |
|---|---|---|---|
| `REV_ROOM` | float | 0–1 | 0.6 |
| `REV_DAMP` | float | 0–1 | 0.5 |
| `REV_MIX` | float | 0–1 | 0.3 |

### Sequencer (2)

| ID | Type | Range | Default | Notes |
|---|---|---|---|---|
| `SEQ_LENGTH` | int | 1–64 | 16 | Pattern length in steps |
| `SEQ_SWING` | float | 0–0.75 | 0.0 | Swing amount (lesson 08) |

Count them: 5 + 2 + 3 + 4 + 3 + 2 = **19**.

## How automation works (the 30-second version)

When you drag a knob in the editor, the `SliderAttachment` calls
`parameter->setValueNotifyingHost()`. The host records that gesture as
**automation data**; on playback, the host calls `setValue()` on the
parameter, and the APVTS updates the underlying `std::atomic<float>`. The
audio thread picks it up on its next once-per-block `->load()` (lesson 05,
step 2). The whole round trip is lock-free — that's why dragging a knob
during playback never glitches.

## Reading parameters in DSP code: the rules

From `processBlock()`:

```cpp
const float blend = apvts.getRawParameterValue ("GRAIN_BLEND")->load();
const int synthAlgoIdx = juce::jlimit (0, 19,
    juce::roundToInt (apvts.getRawParameterValue ("SYNTH_ALGO")->load()));
```

Three rules:

1. **Read once per block, not per sample.** Cheaper, and the block uses one
   coherent value set.
2. **Clamp choice indices** with `juce::jlimit` — automation or a corrupt
   preset could theoretically hand you an out-of-range float; the cast to an
   enum must never produce an invalid enumerator.
3. **Never call `getRawParameterValue()` per sample and cache the pointer
   across blocks? Actually caching the pointer is fine** — the pointer is
   stable for the parameter's lifetime. What you must not do is the
   *string lookup* per sample (it's a map lookup — slow). Our code does the
   lookup once per block, which is simple and fast enough.

## Worked example: add a 20th parameter yourself

Let's add a real, useful parameter: **`MASTER_GAIN`**, replacing the
hardcoded `buffer.applyGain (0.7f)` at the end of `processBlock()` with
something the user (and the DAW) can control. Four edits:

**Edit 1 — declare it** in `createParameterLayout()`, at the end before
`return layout;`:

```cpp
// --- Master ---
layout.add (std::make_unique<juce::AudioParameterFloat> ("MASTER_GAIN", "Master Gain",
                                                         0.0f, 1.0f, 0.7f));
```

The default 0.7 preserves the current behavior exactly.

**Edit 2 — read it** in `processBlock()`, with the other parameter reads:

```cpp
const float masterGain = apvts.getRawParameterValue ("MASTER_GAIN")->load();
```

**Edit 3 — use it**, replacing the hardcoded gain:

```cpp
// was: buffer.applyGain (0.7f);
buffer.applyGain (masterGain);
```

**Edit 4 — add a knob** in the editor. In `PluginEditor.h`, add a member:

```cpp
KnobWidgets masterGainKnob;
```

and in `PluginEditor.cpp`'s constructor, create it the same way the other
knobs are created:

```cpp
masterGainKnob = makeKnob ("MASTER_GAIN", "Master");
```

then lay it out with `layoutKnob (masterGainKnob, x, y);` wherever there's
space. `makeKnob` wires the `SliderAttachment` for you — the knob, the host
automation, and presets all work immediately, with zero extra code.

Rebuild (lesson 12), and your DAW will list "Master Gain" as an automatable
parameter. That's the whole pipeline: **declare → read → use → attach**.

### Two warnings before you go wild with parameters

1. **Parameter IDs are permanent.** DAWs store automation and presets by ID
   string. If you rename `GRAIN_BLEND` to `GRAIN_MIX` in v2, every existing
   project loses that automation. Choose IDs carefully the first time.
2. **Choice order is a contract.** If you insert a new synth algorithm in
   the *middle* of the enum, every preset's stored index shifts. Append new
   choices at the end.

## Check your understanding

- **19 parameters** in 6 groups, all declared in `createParameterLayout()`;
  APVTS is the single source of truth shared by GUI, host, presets, and DSP.
- Audio thread reads via `getRawParameterValue(id)->load()` — **once per
  block**, lock-free; **clamp** choice indices.
- GUI connects via **attachments**; host automation flows through
  `setValueNotifyingHost()` → atomic → per-block load.
- Adding a parameter = **declare → read → use → attach** (the
  `MASTER_GAIN` example).
- IDs are permanent; append new choices at the end, never in the middle.
- Next: lesson 12 — compiling everything.
