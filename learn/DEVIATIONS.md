# DEVIATIONS.md — What the Design Doc Got Wrong (and What It Taught Us)

This is the most honest document in the project. Before any code was
written, there was a design sketch — a plan for how each engine *should*
work. Then code review happened, and the sketch turned out to contain **37
real bugs**: wrong orders of operation, thread-safety violations, API drift,
uninitialized state, math that blew up. Every one was fixed, and every fix
is marked in the source with a `BUG FIX vs sketch:` comment.

This file collects them into one readable log in the form:

> **The sketch said X → reality needed Y, because Z.**

Read it as a catalog of *how audio code actually fails*. If you internalize
these 37, you'll avoid the most common species of real-time audio bugs.

---

## Granular engine (items 1–8)

**1. Double-ticked grains.**
The sketch had `process()` tick every grain, then `renderInto()` tick every
grain again — two samples of advance per sample of time. Grains ran at
double speed with double-speed envelopes.
→ *One* `processBlock()` fires *and* renders in a single pass
(`GrainScheduler.h`). **If two passes touch the same advancing state, one of
them is wrong.**

**2. Hardcoded 44100 Hz ring-mod phase.**
`Grain::tick()` advanced the ring-mod phase with a hardcoded sample rate.
At 48000 Hz (extremely common), every ring-mod pitch was wrong.
→ `tick()` takes the engine's actual `sampleRate` as a parameter (`Grain.h`).
**Never hardcode the sample rate. Anywhere. Ever.**

**3. Confused setSize/clear ordering per block.**
The sketch resized scratch buffers inside the per-block path — `setSize()`
can allocate, which is forbidden on the audio thread.
→ Scratch buffers are **members sized once in `prepare()`**; `process()`
only clears the active region (`GranularEngine.h`). **Allocate in prepare,
clear in process** — the fundamental real-time discipline.

**4. Unguarded divisions and int casts.**
Timer periods and ratios could hit zero → `inf`/`NaN` → poisoned mix (see
lesson 14: NaN is contagious).
→ Clamps on every division and cast. **Any value that enters a division
must be proven non-zero.**

**5. `setGrainLengthNormalized()` reset the grain frequency.**
Setting the length had the *side effect* of resetting frequency — spooky
action at a distance; the GUI couldn't change one without disturbing the
other.
→ Per-layer parameter caching: each layer stores its own length/frequency
and applies them independently. **Setters should do what their name says —
nothing more.**

**6. Ring-mod formula.**
Several candidate formulas; the chosen one crossfades dry→modulated:
`sample *= (1−depth) + depth·sin(phase)` (`GrainScheduler.cpp`). At depth 0
it's exactly 1.0 (bit-transparent); at 1.0 it's pure ring mod. **Design
parameters so their extremes are safe and meaningful.**

**7. Grain voice stealing.**
When all 199 slots are busy, steal the grain with the **largest
`elapsedSamples`** — the oldest, closest to finishing. Least audible choice.
**Steal the voice you'll miss least.**

**8. No-op on empty source.**
The scheduler now does nothing when no sample is loaded, instead of reading
an empty buffer. **Every reader must handle "no data yet."**

---

## Synth engine (items 9–16)

**9. Analog Snare never set `modPhaseInc`.**
The renderer used it; `trigger()` never initialized it — uninitialized
memory as a modulation rate (a dice roll per note).
→ `trigger()` initializes *all* per-voice state, every trigger
(`SynthEngine.cpp`). **If a renderer reads it, the trigger must write it.**

**10. Analog Hat used function-static filter locals.**
`static float lastIn/lastOut` inside the per-sample loop: shared across all
voices, both channels, never reset — voice A's filter bled into voice B.
→ Per-voice members `hatLastIn`/`hatLastOut` (`SynthEngine.h`). **Mutable
`static` locals have no place in per-sample DSP.** (The saturator had the
identical bug — item 23.)

**11. Four partial renderers merged.**
Modal Membrane, Modal String, Additive Bell, Additive Organ all render a
bank of sine partials — four near-duplicate functions.
→ One `renderPartials()`; the *difference* between a bell and a string is
just data (ratios/amplitudes loaded at trigger). **When four functions
differ only in data, you have one function and a table.**

**12. Stolen voices leaked partials.**
Voices are recycled; a voice that played Additive Bell kept its partial
arrays when retriggered as Analog Kick.
→ `trigger()` zeroes the partial arrays. **Ask of every recycled object:
"what did the previous occupant leave behind?"**

**13. Partial phase wrap.**
`if (phase > 1) phase -= 1` fails when a frequency ratio > 1 advances the
phase by *more than a full cycle per sample*.
→ Wrap with `floor()`. **Test your wraparounds against the largest possible
increment, not the typical one.**

**14. Analog Kick pitch sweep unclamped.**
Extreme decay settings could push the sweep ratio to absurd values
(aliasing howl or NaN).
→ `juce::jlimit(0.5, 8)` on the sweep ratio. **Any user-controlled value
entering a frequency, division, or exponent gets clamped.**

**15. VectorMorph index guards.**
Waveshape-morph indices could walk off the table ends.
→ Guarded. **Every table lookup needs a bounds proof.**

**16. Division-by-zero / 44100-fallback guards.**
Sample-rate-dependent divisions guard against non-positive rates, falling
back to 44100 only as a last resort. **Guard first, assume never.**

---

## Sequencer (items 17–22)

**17. Muddled `processSample()` contract.**
The sketch's once-per-sample function had an unclear signature and behavior
— was it per-sample or per-block? Did it return one trig or many?
→ A brutally explicit API: *call exactly once per sample; get at most one
queued trig back* (`StepSequencer.h`). **Ambiguous contracts produce
ambiguous bugs. Write the contract in a comment.**

**18. Stale Pre/Nei flags.**
`Pre` ("this track fired earlier this cycle") and `Nei` ("neighbor fired
earlier this cycle") flags weren't reset at the cycle wrap — a condition
could fire based on *last* cycle's activity.
→ Flags reset at cycle wrap; all 8 tracks evaluated in order at each step
boundary; fired trigs go through a FIFO queue. **State that means "this
cycle" must be reset "every cycle" — say it out loud when you write it.**

**19. Torn two-atomic pattern request.**
Bank and pattern were two separate atomics; the audio thread could observe
bank-from-new + pattern-from-old mid-update.
→ A **single** `std::atomic<int> pendingIndex` holding `bank * 16 + pattern`
(`PatternManager.h`). **If two values must change together, put them in one
atomic.**

**20. `unordered_map` → `std::map`.**
`ParameterLock` used `std::unordered_map<juce::String, float>` — but nobody
wrote a `std::hash` for `juce::String`, so it couldn't compile portably.
→ `std::map` (needs only `<`). **Know what your containers require of your
key types.**

**21. microTiming/retrig: stored but not applied.**
`Step` carries `microTimingTicks`, `retrigCount`, `retrigRateDiv` — they're
serialized, but v1 has no micro-timing/retrigger engine.
→ Kept as honest, documented scope cut (`StepSequencer.h`). **An honest stub
with a comment beats silently dropping user data** — the values survive
save/load and will work when the engine exists.

**22. Default pattern names.**
128 patterns needed names; they default to `A01`–`H16` so the UI shows
something sensible. **Defaults are user interface too.**

---

## Effects (items 23–26)

**23. Saturator Phase used statics in the sample loop.**
Same disease as item 10: `static` all-pass state shared across channels.
→ Per-channel member arrays, cleared by `reset()` (`Saturator.h`). **The
rule bears repeating: no mutable statics in per-sample code.**

**24. Euclidean gate advanced per sample.**
The sketch stepped the rhythm pattern 44,100 times per second — through an
8-step pattern 5,500×/s. The "rhythm" was ultrasonic noise.
→ A `samplesIntoPeriod` counter; the step advances **once per delay period**
(`EuclideanDelay.cpp`). **Always ask: "per *what* should this advance?" —
per sample and per period differ by four orders of magnitude.**

**25. Delay buffer hardcoded to 96 kHz sizing.**
At 192 kHz sessions the buffer was too short for a 2-second delay.
→ Sized from the actual sample rate. **Buffer sizes derive from the real
rate, never a constant.**

**26. Reverb parameter redundancy.**
`juce::Reverb::Parameters` was rebuilt in multiple places.
→ Built once per block from stored members (`LushReverb.h`). **One source
of truth, one application point.**

---

## Processor (items 27–31)

**27. `synthEngine.prepare()` called every block.**
Preparation (wavetable building, allocation) ran 100+ times a second on the
audio thread.
→ Prepared **once** in `prepareToPlay()` (`PluginProcessor.cpp`). **Setup
is a setup-time job. Full stop.**

**28. `buffer.clear()` destroyed the live input.**
The sketch cleared the buffer before the granular sampler could read it —
live sampling recorded silence.
→ Snapshot input into `inputCopy` **before** clearing. **On the audio
thread, order is semantics: destructive operations go last.**

**29. Hardcoded 120 BPM.**
The sketch ignored the host tempo entirely.
→ Host playhead BPM with `.orFallback(120.0)` (`PluginProcessor.cpp`).
**A plugin that ignores the host tempo is a toy; layer your fallbacks
(host → default).**

**30. Per-sample 1-sample synth rendering.**
The sketch rendered the synth per sample into a 1-sample temp buffer —
all the per-block setup cost, 512× per block.
→ Triggers carry sample offsets; one `renderInto()` per block. **Same
sample accuracy, a fraction of the cost. Batching is performance.**

**31. State save dropped live grid edits.**
Edits live in the sequencer; they're only stored into the pattern on switch
— saving without stashing lost them.
→ `getStateInformation()` stashes live steps into the current pattern
first. **Save must capture *live* state, not just *stored* state.**

---

## Build fixes: JUCE 9 API drift (items 32–37)

The sketch was written against JUCE 7 idioms. JUCE 9 removed or changed
several APIs. The compiler caught each one ("no member named X") — **API
drift is normal; the compiler is your migration guide:**

**32.** `saveTopSampleToFile()` used the JUCE 7 writer API → rewritten with
`juce::AudioFormatWriterOptions` (the JUCE 9 builder-pattern API).

**33.** `juce::Reverb` has **no `setEnabled()`** in JUCE 9 → removed.

**34.** `Convolution::loadImpulseResponse(AudioBlock)` overload **removed**
in JUCE 9 → uses the `AudioBuffer&&` overload (`FFTConvolver.cpp`).

**35.** Added `using juce::AudioProcessor::processBlock;` to un-hide the
double-precision overload → silences `-Woverloaded-virtual`
(`PluginProcessor.h`).

**36.** `-Wshadow` / `-Wsign-conversion` cleanups across the DSP code —
variable shadowing and implicit signed/unsigned conversions fixed at the
source. **Strict warnings are a code-review robot that never sleeps.**

**37.** `DAYRUINER_HEADLESS_SMOKE_TEST` guard so the smoke test links
without GUI sources (`PluginProcessor.cpp`, `CMakeLists.txt`). **One source
file, two build configurations, decided at compile time.**

**The meta-lesson of items 32–37:** pin your dependency version (lesson 02:
`--branch 9.0.3`) and let the compiler tell you what drifted. Never "fix"
an API error by guessing — read the new API.

---

## Known simplifications and stubs (honest scope cuts)

Not bugs — deliberate v1 boundaries, documented so nobody mistakes them for
oversights:

- **FFTConvolver is compiled but unwired.** No IR path, no parameter, no UI
  — `process()` bypasses until an impulse response is loaded. The plumbing
  (including the JUCE 9 fix) is done; wiring waits for the IR-loading UI.
- **`saveTopSampleToFile()` has no UI button yet.** The function works; the
  GUI doesn't expose it.
- **Delay Euclidean pattern fixed at 3-in-8.** No rhythm-shape parameter
  until the GUI work resumes.
- **Reverb width fixed at 1.0.** Full stereo; no parameter allocated.
- **ThemeManager: 8 themes, not 22.** GUI design is paused.
- **Grain parameter-locks apply block-immediately.** A few milliseconds of
  timing slop vs. the exact trigger sample — documented in
  `processBlock()`.

---

## The patterns behind the bugs

If you read all 37 and felt a theme emerging, you're right. Nearly every
bug here is one of five species:

1. **Wrong rate** — per-sample vs per-block vs per-period (1, 24, 27, 30).
2. **Wrong lifetime** — statics shared across voices/channels; recycled
   state not reset (10, 12, 23).
3. **Unguarded math** — divisions, table indices, wraparounds, rates (2, 4,
   13, 14, 15, 16, 25).
4. **Torn communication** — two things that must change together, changed
   separately (19); live state not captured at save (31); input destroyed
   before use (28).
5. **Drift** — APIs change under you; pin versions and listen to the
   compiler (32–37).

Learn to *see* these five and you'll catch most audio bugs in review —
before they ever reach a speaker.
