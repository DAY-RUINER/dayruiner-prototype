# Lesson 00 — Overview: What You're About to Build

Welcome. Over the next 15 lessons you are going to build **DAY RUINER**, a real,
working audio plugin, from C++ source code, with your own hands, on your own
computer. Not a tutorial toy — a genuine VST3 instrument you can load in a DAW
and play.

This lesson is the map. Read it first, then work through the lessons in order.
Each one ends with a short recap. Nothing is skipped: every command is
copy-pasteable, and the *why* is explained every step of the way.

---

## What DAY RUINER is

DAY RUINER is a **JUCE 9.0.3 audio plugin** — a software instrument in the VST3
format (plus a standalone app version). You play it with MIDI notes or with its
built-in step sequencer. It makes sound with two engines at once:

1. **A granular engine** — chops loaded audio samples (or live-recorded input)
   into hundreds of tiny "grains" and sprays them back as evolving clouds of
   sound.
2. **A synth engine** — 20 drum and synth algorithms (analog kicks, FM plucks,
   wavetable leads, modal strings…) with 16-voice polyphony.

Those two engines are **blended** together, then run through an **FX chain**
(saturator → euclidean delay → reverb), and out to your speakers.

## The big picture: signal flow

Audio flows through the plugin in one direction. Memorize this diagram — every
lesson hangs off it:

```
 ┌──────────────┐   fires trigs    ┌──────────────────┐
 │  STEP        │ ───────────────▶ │  SYNTH ENGINE    │──┐
 │  SEQUENCER   │                  │  (20 algorithms,  │  │
 │  (8 tracks × │   parameter      │   16 voices)     │  │
 │   64 steps)  │   locks          └──────────────────┘  │  blend
 └──────────────┘                                   (GRAIN_BLEND)
                                                    ┌──┘
 ┌──────────────┐   samples / live  ┌──────────────────┐
 │  SAMPLE      │ ── input ───────▶ │  GRANULAR ENGINE  │──┘
 │  FILES       │                   │  (top + bottom    │
 └──────────────┘                   │   layers)         │
                                    └──────────────────┘
                                              │
                                              ▼
                                    ┌──────────────────┐
                                    │  SATURATOR       │  (None/Bitcrush/
                                    │                  │   Wavefold/Tape/…)
                                    └────────┬─────────┘
                                             ▼
                                    ┌──────────────────┐
                                    │  EUCLIDEAN DELAY │  (tempo-synced,
                                    │                  │   rhythm-gated)
                                    └────────┬─────────┘
                                             ▼
                                    ┌──────────────────┐
                                    │  LUSH REVERB     │  (Freeverb-style
                                    │                  │   + shimmer)
                                    └────────┬─────────┘
                                             ▼
                                        🔊 OUTPUT
                                        (× 0.7 safety gain)
```

The conductor of all this is the **PluginProcessor**
(`Source/PluginProcessor.cpp`). Every ~10 milliseconds your DAW calls its
`processBlock()` method, and the processor runs this whole chain for a few
hundred samples at a time. Lesson 05 walks that method line by line.

## What the plugin actually contains

| Piece | Files | One-line description |
|---|---|---|
| Plugin processor | `Source/PluginProcessor.{h,cpp}` | The host interface: audio callbacks, parameters, state save/load |
| Granular engine | `Source/dsp/Grain.h`, `GrainScheduler.{h,cpp}`, `GranularEngine.{h,cpp}`, `SampleBuffer.{h,cpp}` | Two-layer grain cloud renderer with live recording |
| Synth engine | `Source/dsp/SynthEngine.{h,cpp}` | 20 algorithms, 16 voices, sample-accurate triggering |
| Effects | `Source/dsp/Saturator`, `EuclideanDelay`, `LushReverb`, `FFTConvolver` | 6-mode saturator, rhythm-gated delay, shimmer reverb, (unwired) IR convolver |
| Sequencer | `Source/sequencer/StepSequencer.{h,cpp}`, `PatternManager.{h,cpp}`, `TrigCondition.h`, `ParameterLock.{h,cpp}` | Elektron-style 8×64 sequencer, 128 patterns, trig conditions, parameter locks |
| GUI | `Source/PluginEditor.{h,cpp}`, `Source/gui/*` | Functional editor: sequencer grid, grain XY pad, knobs, 8 themes |
| Parameters | `createParameterLayout()` in `PluginProcessor.cpp` | 19 host-automatable parameters |
| Smoke test | `Source/SmokeTest.cpp` | Headless DSP test: renders 1.5M samples, checks for NaN/inf/blowups |

That's **36 source files** total. It sounds like a lot, but you'll meet them one
family at a time.

## What you'll have at the end

By the end of lesson 13 you will have, built by you:

- `Day Ruiner.vst3` — the VST3 plugin bundle, installed where your DAW can find it
- `Day Ruiner` (standalone app) — double-clickable, no DAW needed
- `DayRuinerSmokeTest` — a console program that proves the DSP is clean
- `smoke_output.wav` — 5 seconds of the plugin's actual rendered output

And you'll understand what every one of those files does, because you read the
lessons that explain them.

## How to use this series

- **Platforms covered: Kali Linux and Windows only.** Each lesson that has
  platform-specific steps contains a **Kali Linux** section and a **Windows**
  section. Follow yours; skim the other only if you're curious.
- **Go in order.** Later lessons assume earlier ones (e.g. lesson 12 assumes you
  cloned JUCE in lesson 02).
- **Type the commands, don't just read them.** Muscle memory matters.
- **The `DEVIATIONS.md` file is the secret best lesson.** It documents 37 real
  bugs that were found and fixed while this code was built. Read it alongside
  lessons 05–09 — it's a masterclass in what actually goes wrong in audio code.
- **Each lesson ends with a "Check your understanding" recap.** These are
  recaps, not quizzes — read them and make sure they feel true before moving on.

## A note on difficulty (honest)

You are a beginner and this is a large C++ project. That's fine — it's designed
for you. But be aware of the shape of the work:

- Lessons 00–04 are **setup and orientation** — mostly reading and installing.
- Lessons 05–11 are **reading code** — the most valuable part. Go slowly.
- Lessons 12–13 are **compiling and running** — the payoff.
- Lesson 14 is **debugging** — you'll need it eventually; everyone does.

JUCE itself is a big library, and the first compile takes several minutes.
That's normal. Make tea.

---

## Check your understanding

- DAY RUINER is a **VST3 instrument plugin** built with **JUCE 9.0.3**.
- Sound flows: **sequencer → synth + granular engines → blend → saturator →
  delay → reverb → output**.
- The **processor** (`PluginProcessor.cpp`) conducts everything inside
  `processBlock()`.
- The project has **36 source files** in `Source/`, organized into
  `dsp/`, `sequencer/`, and `gui/` folders.
- Lessons cover **Kali Linux and Windows**; each has its own sections.
- `DEVIATIONS.md` is the real-world bug-fix log — treat it as a lesson.
