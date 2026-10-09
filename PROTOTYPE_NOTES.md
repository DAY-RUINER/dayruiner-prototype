# PROTOTYPE_NOTES.md — DAY RUINER Windows-prototype integration log

Built 2026-10-08/09 (UTC). Tree: `~/workspace/dayruiner-prototype/`
(a copy of `DayRuinerEngine-v1`; the original tree was not modified).

## 1. What was integrated

**GUI.** `Source/gui/*` is now the DAY RUINER panel (DayRuinerEditor +
all widgets). The old team-3 GUI (`SequencerGrid`, `GrainPanel`,
`ThemeManager`, `KnobLookAndFeel`, `PluginEditor.h/.cpp`) was moved to
`Source/gui-legacy/` and is NOT compiled. `createEditor()` constructs
`DayRuinerEditor(*this, apvts, seqAdapter, grainAdapter, presetStore)`.

**Adapters** (`Source/gui/PrototypeAdapters.h`, new):
- `SequencerAdapter` → `DayRuiner::StepSequencer`. FX-blink flag =
  `locks.hasLock("DELAY_MIX") || hasLock("SAT_MIX") || hasLock("REV_MIX")`.
  `setStepFx` sets a `DELAY_MIX` lock at 0.35 (or clears all three).
- `GrainFieldAdapter` → `GranularEngine`. Source peaks resampled from the
  top `SampleBuffer`; live grain windows via a new
  `GrainScheduler::getActiveGrainWindows()`; playhead from the scheduler's
  atomic; output peaks from a new 64-slot peak ring written in
  `GranularEngine::process()` (pure monitor tap — no DSP change).
- `PresetStoreAdapter` → `kDrumPresets[111]` / `kSynthPresets[111]` from
  `Source/presets/DayRuinerPresets.h` (copied from the CURRENT preset
  header; the rebalanced header is a drop-in swap later). Selection applies
  via the existing `applyDayRuinerPresetToApvts()` recipe.

**APVTS additions** (all pre-existing IDs/ranges untouched):
- `TRK1..8_ALGO` (14 drum algorithms, kit defaults 0,1,2,5,7,6,8,13),
  `TRKn_TUNE` (−24..+24 st), `TRKn_DECAY` (0.01..4 s), `TRKn_LEVEL` (0..1),
  `TRKn_PAN` (−1..1), `TRKn_HUMANIZE` (0..1), `TRKn_CHOKE` (OFF/A/B/C/D),
  `TRKn_DRIVE` (0..24 dB), `TRKn_MUTE` (bool) — 72 new params.
- `DRIVE_MODE` (SoftClip/HardClip/Saturate/Distort, default Saturate),
  `DRIVE_AMOUNT` (0..36 dB, default 6), `COMP_THRESHOLD` (−60..0 dB,
  default −18).

**Engine wiring** (`processBlock`, signal path preserved):
```
synth/grain render → blend → saturator → euclidean delay → [NEW] drive
→ [NEW] glue compressor (juce::dsp::Compressor, threshold from
COMP_THRESHOLD; ratio 4, attack 10 ms, release 100 ms) → [NEW] FDN reverb
→ [NEW] SoftClipper (always on; engage latch → clipEngagedFlag for the UI)
→ master gain 0.7 (always last, always once)
```
- `LushReverb` deleted; `FDNReverb` owned directly. Legacy `REV_ROOM` /
  `REV_DAMP` / `REV_MIX` IDs repointed per INTEGRATION.md §4b
  (`REV_ROOM` → RT60 via `0.2 * 150^v`, 0.2 s..30 s). `getTailLengthSeconds()`
  raised 4.0 → 8.0 s.
- Sequencer trigs now use the **per-track DRUMS voice**: ALGO (via
  `kTrackAlgoMap`; Clap/Rimshot/Tom/Crash map to nearest v1 voice until the
  engine grows those algorithms), TUNE (transpose ±24 st), DECAY, LEVEL
  (velocity scale), MUTE (trig skipped), HUMANIZE (lock-free LCG: velocity
  droop + ±10 ms timing slop via the trigger offset). MIDI-input trigs keep
  the global `SYNTH_ALGO`/`SYNTH_DECAY` (SYNTH page). Note activity
  (MIDI on/off count + fired trigs) feeds `FDNReverb::setNoteActive()`.

**Build system.** Root `CMakeLists.txt` added (the v1 tree's
`Source/CMakeLists.txt` was written for `cmake -S <root>` but shipped
without one). JUCE now comes from the prototype's own shallow 9.0.3 clone
(`_juce93/`, verified `git describe --tags` → `9.0.3`). `.gitignore`
covers build dirs, `_juce93/`, plugin binaries, CMake/IDE/OS junk.
`.github/workflows/windows-vst3.yml` builds the VST3 on `windows-latest`
(MSVC, VS2022 x64), caches the JUCE clone + CMake build dir, uploads
artifact `DayRuiner-Windows-VST3`; triggers on push to `main` and manual
dispatch.

## 2. Deferred / staged (deliberate, documented)

- **Full `EffectsChain` routing** (per-slot Send/Bus/Master, compressor
  attack/release/ratio/makeup/mix params, reverb size/predelay/diffusion/
  mod/lowcut/fbcut/width/duck/freeze/autocut params): deferred — would
  require invasive `processBlock` restructuring. The prototype wires the
  three stages directly with musical fixed defaults instead.
- **`TRKn_PAN`**: no per-voice pan stage in `SynthEngine`; param exists and
  is automation-safe, engine application staged.
- **`TRKn_CHOKE`**: no choke-group voice management in `SynthEngine`; staged.
- **`TRKn_DRIVE`**: no per-track drive stage (global `DRIVE_MODE`/`AMOUNT`
  exists instead); staged.
- **Per-step FX lock application**: `DELAY_MIX`/`SAT_MIX`/`REV_MIX` locks
  drive the trig-grid blink and persist in patterns/state, but v1's audio
  path only applies `SYNTH_NOTE`/`GRAIN_LENGTH` locks per step. Applying FX
  locks per step needs the FX stages to become step-aware — staged.
- **Clip badge UI**: `consumeClipFlag()` is latched and ready; the editor
  doesn't poll it yet (one-line GUI-team addition in the 30 Hz timer).
- **Drive oversampling** (2×/4×): noted in INTEGRATION.md §6; not in this
  prototype (CPU + Send-path complexity).

Nothing is a *silent* dead control: every staged param is listed here, and
the GUI marks engine-missing knobs with `*` + tooltip (SYNTH/GRANULAR pages,
MASTER) exactly as before.

## 3. JUCE 7 → 9 drift fixes (all in our GUI code)

- `juce::Font::getStringWidthFloat()` removed in JUCE 9 →
  `juce::GlyphArrangement::getStringWidth(font, text)` (`LogoWordmark.cpp`).
- Deprecated `juce::Font(String, float, int)` / `juce::Font(float, int)`
  constructors → `juce::Font(juce::FontOptions(...))`
  (`DayRuinerLookAndFeel.cpp`).
- `juce::MessageManager::runDispatchLoopUntil()` removed (test code only) —
  dropped; snapshots are synchronous.

## 4. Verification (Linux sandbox, JUCE 9.0.3, Release)

- `DayRuinerSmokeTest`: **PASS** — 3000 blocks, peak 0.35, 0 NaN, 0 inf,
  0 blowup blocks; exercises the new drive → compressor → FDN → clipper
  chain under the sequencer + MIDI.
- `DayRuinerEngine_VST3` + `DayRuinerEngine_Standalone`: **built**, zero
  errors, zero warnings in our code (`-Wall -Wextra` via JUCE's recommended
  flags; only JUCE's own splash-screen notice). VST3 bundle well-formed
  (`Day Ruiner.vst3/Contents/x86_64-linux/Day Ruiner.so`, no missing libs).
- `DayRuinerEditorTest` (dev utility, headless under xvfb): **PASS** —
  real editor + real processor/APVTS/adapters; all 5 pages render
  (`/tmp/dayruiner_editor_test/page_*.png`); DRUMS page shows
  `TRACK 1 - ANALOG KICK` with real defaults, FX page shows preset
  `808 CLAP CLASS` with macro defaults, GRAINFIELD honest
  `NO SAMPLE LOADED` state.
- Sandbox needed `libxrandr-dev`, `libxinerama-dev`, `libxcursor-dev`,
  `libxi-dev` for JUCE's X11 backend (headers, not a code patch — the tree
  builds against pristine JUCE 9.0.3).

## 5. Windows MSVC watch-list (not caught on Linux)

- `/W4`+`/WX`-style strictness was not tested: the Linux build used GCC
  `-Wall -Wextra` (JUCE recommended flags) with zero warnings in our code,
  but MSVC's warnings differ (C4244 narrowing, C4127, C4389 sign compare).
  The highest-risk spots: `std::pow` double→float in the reverb RT60 line,
  `juce::roundToInt` conversions, and the `static_cast<size_t>` on
  `numBins` in the adapter. None are errors at default `/W3`.
- `juce::dsp::Compressor<float>` and `juce::dsp::AudioBlock` usage is
  standard JUCE DSP — no platform-specific behavior expected.
- The workflow caches `build/` across runs; if MSVC/VS-version drift ever
  breaks the cache restore, the `restore-keys` fallback + hash-keyed
  invalidation recover automatically (worst case: one slow rebuild).
- First CI run compiles all of JUCE (~10–20 min on `windows-latest`);
  subsequent runs hit both caches.
