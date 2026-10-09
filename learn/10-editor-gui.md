# Lesson 10 — The Editor GUI: Knobs, Grids, and Attachments

`Source/PluginEditor.h` (65 lines) and `Source/PluginEditor.cpp` (283 lines)
build the plugin's window, with four widget classes in `Source/gui/`. One
important framing note, stated plainly in the code:

> *Functional-but-plain editor (GUI design is paused): sequencer grid on
> top, grain XY pad + parameter knobs below, transport/utilities along the
> bottom.*

The GUI **works** — every parameter is controllable, the grid is clickable,
themes switch — but it's not *designed* yet. The fancy knob-rendering study
from Phase 1 is on hold. As a learner this is actually good news: the editor
is small enough to read in one sitting, and every widget demonstrates exactly
one JUCE technique.

## The layout

```
┌─────────────────────────────────────────────┐
│  SEQUENCER GRID (8 tracks × 64 steps)       │  ← SequencerGrid
├──────────────────────┬──────────────────────┤
│  GRAIN XY PAD        │  KNOBS               │  ← GrainPanel + Sliders
│  (length × freq)     │  (blend, decay,      │
│                      │   drive, delay…)     │
├─────────────────────────────────────────────┤
│  [Record] [Monitor]  algo boxes  bank/patt  │  ← transport + combos
│  theme picker        status label           │
└─────────────────────────────────────────────┘
```

## The magic: APVTS attachments

The single most important GUI concept in JUCE: **attachments**. Look at how
a knob is made (`PluginEditor.h`):

```cpp
KnobWidgets makeKnob (const juce::String& paramID, const juce::String& labelText);
```

and the members:

```cpp
std::vector<std::unique_ptr<juce::Slider>> knobSliders;
std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> knobAttachments;
```

A `SliderAttachment` binds a `Slider` to a parameter ID (e.g.
`"GRAIN_BLEND"`) **bidirectionally and thread-safely**:

- Drag the knob → the parameter updates → the host sees it (and can record
  it as automation).
- Host automation (or a preset load) changes the parameter → the knob moves
  to match.

You never write `onValueChange` handlers that poke the processor, and you
never poll parameters to update widgets. The attachment does it all,
including the thread-safety (GUI thread ↔ audio thread via the APVTS's
lock-free machinery). **This is the correct way to connect every widget to
every parameter** — if you find yourself manually syncing a widget and a
parameter, you're doing it wrong.

Combo boxes get the same treatment via `ComboBoxAttachment` (used for the
synth/synth-algorithm and saturator-algorithm pickers, bound to `SYNTH_ALGO`
and `SAT_ALGO`).

## The components

### `SequencerGrid` — the clickable step grid

Draws the 8×64 step grid and handles mouse clicks to toggle steps. It talks
to the sequencer through `getStep(track, step)` — the direct live-step
references from lesson 08 — so edits apply to the playing pattern in real
time. A `juce::Timer` (the editor inherits `private juce::Timer`) fires
`timerCallback()` ~30 times a second to repaint the playhead position
(`getCurrentStep()` atomic snapshot). Note the split: **audio thread moves
the playhead; the timer just repaints** — the GUI never touches audio timing.

### `GrainPanel` — the XY pad

A two-dimensional pad: X controls grain length, Y controls grain frequency
(wired to the top layer's parameters). XY pads are a staple of granular
instruments — one gesture morphs the cloud. It demonstrates custom mouse
handling + parameter setting without a `Slider`.

### `KnobLookAndFeel` — custom knob drawing

JUCE lets you override how widgets are drawn via `LookAndFeel` classes.
`KnobLookAndFeel` customizes the rotary slider rendering (the Phase 1 knob
study's descendant — currently simple, awaiting the design pass). The
technique to remember: **behavior (Slider) is separate from appearance
(LookAndFeel)** — you can reskin every knob in the plugin by changing one
class.

### `ThemeManager` — 8 color themes

```cpp
// DEVIATION vs the design sketch: the sketch called for 22 themes. GUI design
// work is paused, so this ships 8 tasteful dark themes instead.
```

`applyTheme()` walks the components and repaints them in the selected theme.
The theme picker is a plain combo box — functional, unglamorous, done.

## Transport and utilities

- **Record button** → `granularEngine.startLiveRecording()` /
  `stopLiveRecording()` (lesson 06). The button text/state reflects
  `isLiveRecording()`.
- **Monitor Input toggle** → `processor.setMonitorInput(bool)` (lesson 05,
  step 11).
- **Bank/pattern boxes** → `patternManager.requestPattern(bank, pattern)` —
  the atomic switch request from lesson 08.
- **Status label** — text feedback ("Recording…", sample names).

## GUI threading rules (the contract)

The editor lives on JUCE's **message thread** (the UI thread). Its rules
mirror the audio thread's, inverted:

- **Never block** the message thread: no heavy computation, no file I/O that
  could stall, no waiting on the audio thread. (Sample *loading* is the
  exception — it's brief and user-initiated.)
- **Talk to the audio thread only through thread-safe channels**:
  attachments, atomics (`getCurrentStep()`), and the async request pattern
  (`requestPattern()`).
- The editor may be **created and destroyed** at any time (the user closes
  the window; the plugin keeps playing). That's why the processor owns all
  state and the editor only *borrows* references — closing the GUI must never
  stop the audio.

## Check your understanding

- The editor is **functional, not finished** — GUI design is paused; every
  widget demonstrates one JUCE technique cleanly.
- **Attachments** (`SliderAttachment`, `ComboBoxAttachment`) bind widgets to
  parameters bidirectionally and thread-safely — never hand-sync widgets.
- `SequencerGrid` edits live steps; a `Timer` repaints the playhead from
  atomic snapshots (GUI never touches audio timing).
- **Behavior vs appearance are separate**: `Slider` + `KnobLookAndFeel`.
- `ThemeManager` ships **8 themes** (not 22 — paused design work).
- GUI rules: never block the message thread; talk to audio only via
  attachments/atomics; the editor borrows state, the processor owns it.
- Next: lesson 11 — the 19 parameters, and how to add your own.
