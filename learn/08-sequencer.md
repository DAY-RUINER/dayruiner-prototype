# Lesson 08 — The Sequencer: Musical Time

The sequencer answers one question, 44,100 times a second: *"is it time for
a note yet?"* It's Elektron-style — meaning per-step conditional triggers and
per-step parameter locks, the features that make Elektron hardware sequencers
legendary for generative patterns. Four files in `Source/sequencer/`:

| File | Role |
|---|---|
| `TrigCondition.h` | 9 per-step fire conditions |
| `ParameterLock.h` / `.cpp` | Per-step parameter overrides |
| `StepSequencer.h` / `.cpp` | 8 tracks × 64 steps, sample-accurate timing |
| `PatternManager.h` / `.cpp` | 128 patterns, chain & song modes, atomic switching |

## Timing: 16th notes, computed per sample

The sequencer is driven by `processSample()`, which the processor calls
**exactly once per sample** (lesson 05, step 7). Inside, a sample counter
accumulates until it reaches the current step's duration, then the playhead
advances. Step duration in samples:

```cpp
// Source/sequencer/StepSequencer.cpp — computeStepDurationSamples()
const double baseStepSamples = sr / ((b / 60.0) * 4.0); // 16th notes
const double swingFactor = (stepIndex % 2 == 0) ? (1.0 + (double) sw)
                                               : (1.0 - (double) sw);
return baseStepSamples * swingFactor;
```

Unpack the math: `b / 60.0` is beats per second; `× 4.0` because a 16th note
is a quarter of a beat — so `(b/60)·4` is 16th-notes per second, and
dividing the sample rate by that gives **samples per 16th note**. At 120 BPM
and 44100 Hz: `44100 / (2·4) = 5512.5` samples per step.

**Swing** delays every *odd* 16th note: even steps get longer
(`×(1+swing)`), odd steps get shorter (`×(1−swing)`), keeping the pair's
total length constant. Swing is clamped to 0–0.75 (beyond that, odd steps
would nearly vanish). Crucially, the duration is **recomputed for the
current step every time the playhead advances** — never cached across a step
change — so dragging the swing knob (or a tempo change) takes effect at the
very next step boundary.

`processSample()` returns a `FiredTrig` — at most one per call:

```cpp
struct FiredTrig
{
    bool fired = false;
    int trackIndex = -1;
    int stepIndex = -1;
    int midiNote = 36;
    float velocity = 1.0f;
    const Step* step = nullptr;  // valid only until the next pattern swap!
};
```

**Bug fix (DEVIATIONS.md item 17):** the sketch's `processSample`
signature and behavior were muddled — it wasn't clear whether it was
once-per-sample or once-per-block, and swing wasn't recomputed cleanly. The
final API is unambiguous: *call exactly once per sample; you get at most one
queued trig back.* When in doubt, make the contract brutally explicit in a
comment — the header does exactly that.

## Trig conditions: the 9 rules

Each step carries a `TrigConditionData`. When the playhead reaches an active
step, `evaluate()` decides whether it *actually* fires:

| # | Condition | Fires when… | Classic use |
|---|---|---|---|
| 0 | **Always** | Every cycle | The default: a normal sequencer step |
| 1 | **Probability** | Random chance < `probability` | Humanized hi-hats, generative variation |
| 2 | **A:B** | `((cycle % B) + 1) == A`, e.g. 1:4 | A fill that happens every 4th loop |
| 3 | **FirstOnly** | Only on cycle 0 | An intro hit that never repeats |
| 4 | **NotFirst** | Every cycle *except* 0 | Skip the downbeat the first time around |
| 5 | **Pre** | This track fired on an *earlier* step this cycle | Conditional chains along a track |
| 6 | **Nei** | A *neighboring* track fired earlier this cycle | Kick-follows-bass interplay |
| 7 | **Fill** | Only while fill mode is engaged | Performance fills on demand |
| 8 | **NotFill** | Only while fill mode is off | The "normal" version of a fill step |

Two implementation details worth studying:

1. **Pre/Nei flags reset at the cycle wrap** (item 18). The sketch let them
   go stale — a `Pre` condition could fire based on *last* cycle's activity.
   Now all 8 tracks are evaluated **in order** at each step boundary, and the
   "did it fire earlier this cycle" flags are cleared when the cycle wraps.
   The lesson: *state that means "this cycle" must be reset "every cycle"* —
   say it out loud when you write such code.
2. **Pending trig FIFO.** When several tracks fire on the same step,
   `fireStep()` evaluates all 8 tracks in order and queues the results; each
   `processSample()` call hands back at most one. The processor's per-sample
   loop (lesson 05, step 7) drains the queue with correct per-sample offsets.

Also note `evaluate()` uses `juce::Random::getSystemRandom()` directly rather
than a stored `Random` — per JUCE's thread-affinity guidance, `Random`
instances shouldn't be shared across threads.

## Parameter locks: per-step automation

A **parameter lock** ("p-lock") overrides a parameter for exactly one step —
e.g. step 6 plays note 48 instead of the track default, or one grain hit is
twice as long. Implementation (`ParameterLock.h`):

```cpp
// std::map, not unordered_map: juce::String has no std::hash specialization.
std::map<juce::String, float> locks;
```

That comment is a small masterclass in itself: `std::unordered_map` needs a
hash function, and nobody wrote one for `juce::String` — so the ordered
`std::map` (which needs only `<`) is the correct choice. The processor reads
locks like this (lesson 05, step 7):

```cpp
const int note = locks.hasLock ("SYNTH_NOTE")
    ? static_cast<int> (locks.getValue ("SYNTH_NOTE", ...))
    : ft.midiNote;
```

**Honest scope cut (item 21):** each `Step` also stores `microTimingTicks`
and `retrigCount`/`retrigRateDiv` — and they're serialized — but v1 has no
micro-timing or retrigger engine, so they're **stored but not applied**.
The code says so plainly in the header. Shipping an honest stub with a
comment beats silently dropping user data (the values survive save/load and
will work when the engine exists).

## Pattern manager: 128 patterns, atomic switching

`PatternManager` owns **128 patterns: 8 banks × 16**, named `A01`–`H16`
(item 22 — default names so the bank/pattern boxes show something sensible).
Each pattern stores its name, length, swing, and a full 8×64 step grid.

Switching patterns *while playing* is a threading puzzle: the editor thread
requests the switch, but the audio thread must perform it at a safe moment
(the **cycle boundary** — the instant the pattern loops). The design:

```cpp
// Single atomics (bank * 16 + pattern): a switch request can never be
// observed as a torn bank-from-new/pattern-from-old pair.
std::atomic<int> currentIndex { 0 };
std::atomic<int> pendingIndex { -1 }; // -1 = no switch requested
```

**Bug fix (item 19):** the sketch used *two* atomics (bank + pattern), which
could be observed mid-update as a torn pair — bank from the new request,
pattern from the old. Packing both into a **single atomic int** (`bank * 16
+ pattern`) makes the request indivisible. This is a general lock-free
pattern: *if two values must change together, put them in one atomic.*

`applyPendingIfNeeded()` runs on the audio thread at the cycle boundary: it
first **saves the sequencer's live tracks into the current pattern**
(preserving grid edits — the same idea as lesson 05's state-save stash),
then loads the pending pattern in. A pattern swap copies `Step` objects
whose locks own `std::map`s — which can allocate — accepted in v1 because it
happens at most once per cycle and only when requested. (A lock-free
double-buffered swap is noted as future work — honest engineering.)

**Chain mode** plays a list of bank/pattern pairs in order; **song mode**
plays `SongSlot`s (bank + pattern + repeat count) in order. Both advance in
`advanceOnCycle()` at the cycle wrap.

## Threading model (read this twice)

The header documents it precisely, and it's worth memorizing as a template
for your own audio/UI code:

- `processSample()` — audio thread, once per sample, no allocation, no locks.
- Editor → sequencer communication — **atomics only** (bpm, swing, pattern
  length, fill mode, pattern requests).
- `getStep()` hands the editor *direct references* to live step data — the
  grid edits the playing pattern in real time. (Technically racy if the
  audio thread reads mid-edit; benign in practice, and the header says
  "prefer editing while the transport is stopped" for determinism.)
- `toValueTree()`/`fromValueTree()` — message thread only, transport
  stopped.

## Check your understanding

- Timing: **samples per 16th = `sr / ((bpm/60)·4)`**; swing stretches even
  steps and shrinks odd steps; duration is recomputed at every step
  boundary.
- `processSample()` is called **exactly once per sample** and returns at
  most one queued trig (FIFO across the 8 tracks).
- **9 trig conditions** (Always, Probability, A:B, First, NotFirst, Pre,
  Nei, Fill, NotFill); Pre/Nei flags reset every cycle.
- Parameter locks live in a **`std::map`** (no `std::hash` for
  `juce::String`); micro-timing/retrig are stored but not yet applied.
- **128 patterns (A01–H16)**, chain + song modes; pattern requests use a
  **single atomic** so bank+pattern can't tear.
- Live grid edits are stashed into the pattern on switch and on save —
  never silently dropped.
- Next: lesson 09 — the effects chain.
