# Lesson 13 — Running and Testing: Prove It Works

You compiled it — now *prove* it. This lesson runs all three binaries in
order of increasing complexity: the **standalone app** (no DAW needed), the
**smoke test** (automated DSP verification), and the **VST3 in a real DAW**.
On Windows you'll also validate with **pluginval**, the industry-standard
plugin tester.

---

## 1. The standalone app — hear it first

The standalone is the fastest way to hear your build: no DAW, no plugin
scanning, just double-click (or run) and play.

### Kali Linux

```bash
~/DayRuinerEngine/build/DayRuinerEngine_artefacts/Release/Standalone/"Day Ruiner"
```

(The quotes handle the space in the name.) A window opens with the editor
from lesson 10. Click **Options → Audio/MIDI Settings** (JUCE's standard
audio device selector) and choose your output device — on most Kali systems
the ALSA default through PipeWire just works (lesson 01). Play the on-screen
keyboard or enable the sequencer.

**No window? Blank window?** See lesson 14 (X11/Wayland, `GDK_BACKEND=x11`,
`xvfb-run` for headless machines). The reference build was launched under
`Xvfb` (a virtual display) with **no JUCE assertion dialogs** — that's the
"it starts clean" bar.

**No sound?** Open `pavucontrol`, find the app in the Playback tab, unmute
it and check its output device.

### Windows

Double-click:

```
C:\dev\DayRuinerEngine\build\DayRuinerEngine_artefacts\Release\Standalone\Day Ruiner.exe
```

In PowerShell:

```powershell
& "C:\dev\DayRuinerEngine\build\DayRuinerEngine_artefacts\Release\Standalone\Day Ruiner.exe"
```

(The `&` is PowerShell's "run this quoted path" operator; in cmd, just type
the quoted path directly.)

If Windows asks about audio/microphone permissions: the standalone **needs
an audio output device, not a microphone** — allow what's needed for
playback. **ASIO is optional** — the default Windows Audio / WASAPI output
works fine for testing. (ASIO4ALL or a proper interface driver gives lower
latency later, but don't let driver setup block your first run.)

Click around the editor: drag the grain knobs, toggle sequencer steps, press
Record. If the window opens and knobs move, your GUI thread, parameter
system, and audio device are all working.

---

## 2. The smoke test — automated DSP verification

The smoke test (`Source/SmokeTest.cpp`, built as `DayRuinerSmokeTest`) is
the project's automated proof that the DSP is numerically sane. It:

1. Instantiates the real `DayRuinerAudioProcessor` and calls
   `prepareToPlay(44100, 512)`.
2. Synthesizes a 2-second test sample (a 100 Hz → 2000 Hz sine sweep with
   raised-cosine edges), writes it to a temp WAV, and loads it into the
   granular engine.
3. Programs the sequencer (10 active steps, one probability trig condition,
   one parameter lock), sets `SEQ_LENGTH` to 16 via the APVTS, and picks
   `SYNTH_ALGO` = FM Kick, `SAT_ALGO` = Tape.
4. Renders **3000 blocks × 512 samples = 1,536,000 samples** through the full
   `processBlock()` chain — including a MIDI note on/off pair and a
   mid-run algorithm switch (Additive Bell + Wavefold at block 1500).
5. Scans every sample for **NaN**, **infinity**, and **blowups** (block peak
   > 10.0), records the peak, and writes the first 5 seconds to
   `smoke_output.wav`.

### Kali Linux

```bash
~/DayRuinerEngine/build/DayRuinerSmokeTest_artefacts/Release/DayRuinerSmokeTest
```

### Windows

In PowerShell:

```powershell
& "C:\dev\DayRuinerEngine\build\DayRuinerSmokeTest_artefacts\Release\DayRuinerSmokeTest.exe"
```

In cmd:

```cmd
"C:\dev\DayRuinerEngine\build\DayRuinerSmokeTest_artefacts\Release\DayRuinerSmokeTest.exe"
```

### Reading the output

A passing run prints something like:

```
=== DAY RUINER SMOKE TEST ===
sampleLoaded: yes
blocks run: 3000
peak: 1.13
nanCount: 0
infCount: 0
blowupBlocks: 0
output wav: /home/<you>/workspace/DayRuinerEngine/build/smoke_output.wav
SMOKE TEST: PASS
```

What each line means:

- `sampleLoaded: yes` — the temp WAV wrote and loaded into the granular engine.
- `blocks run: 3000` — all blocks rendered without crashing.
- `peak: 1.13` — the loudest sample across 1.5M samples. Slightly above 1.0
  is fine (the ×0.7 safety gain keeps it sane); what would be alarming is
  10+, `inf`, or `NaN`.
- `nanCount: 0` / `infCount: 0` — no poisoned samples. A single NaN would
  spread through feedback paths (delay, reverb) and nuke the whole output —
  this is the test that catches the division-by-zero and uninitialized-state
  bugs from `DEVIATIONS.md`.
- `blowupBlocks: 0` — no block peaked above 10.0.
- `SMOKE TEST: PASS` — the line you want. (Exit code 0; any failure prints
  `FAIL` with a reason and exits 1.)

Then **listen** to `build/smoke_output.wav` — 5 seconds of your plugin's
actual output: sequenced FM kicks, tape saturation, the works. If it sounds
like music (rough, but musical), everything from lessons 05–09 is working.

> **Quirk to know:** `SmokeTest.cpp` writes the WAV to
> `juce::File::userHomeDirectory / "workspace/DayRuinerEngine/build/smoke_output.wav"` —
> a path baked in from the reference machine. If your project lives
> elsewhere, the write may fail with `output wav: WRITE FAILED` — **this
> does not fail the test** (the PASS verdict only depends on the DSP
> checks), but you won't get the WAV. Edit the path in `SmokeTest.cpp`
> (search for `smoke_output.wav`) to match your machine and rebuild the
> smoke target if you want the file.

**Reference results** (from the verified build): 3000 blocks × 512 samples,
1.536M samples scanned, 0 NaN, 0 inf, 0 blowups, peak 1.13 → **PASS**.

---

## 3. The VST3 in a DAW

### Installing the VST3

**Windows:** copy the *entire bundle folder* (not just the file inside) to
the system VST3 location:

```powershell
Copy-Item -Recurse "C:\dev\DayRuinerEngine\build\DayRuinerEngine_artefacts\Release\VST3\Day Ruiner.vst3" "C:\Program Files\Common Files\VST3\"
```

In cmd:

```cmd
xcopy /E /I "C:\dev\DayRuinerEngine\build\DayRuinerEngine_artefacts\Release\VST3\Day Ruiner.vst3" "C:\Program Files\Common Files\VST3\Day Ruiner.vst3\"
```

(`robocopy` works too, in both shells.) You'll need administrator rights to
write to `C:\Program Files` — approve the UAC prompt.

**Kali Linux:** copy the bundle to a scanned VST3 path:

```bash
mkdir -p ~/.vst3
cp -r ~/DayRuinerEngine/build/DayRuinerEngine_artefacts/Release/VST3/"Day Ruiner.vst3" ~/.vst3/
```

(`~/.vst3` is the per-user scan path; `/usr/lib/vst3/` is the system-wide
one.)

### Loading it in Reaper (example DAW)

[Reaper](https://www.reaper.fm) is free to evaluate (full-featured,
nag-screen license) and runs on both our platforms — ideal for testing your
own plugin:

1. Install Reaper, open it, and **rescan plugins**: Options → Preferences →
   Plug-ins → VST → "Re-scan" (Windows) / ensure `~/.vst3` is in the path
   list (Linux).
2. Insert a track, click FX, search **"Day Ruiner"** — it appears under
   Instruments (remember `VST3_CATEGORIES "Instrument" "Synth"` from lesson
   04).
3. Arm the track, play MIDI notes (or draw some in), and confirm sound. Try
   automating `GRAIN_BLEND` — draw an automation lane and watch the editor's
   knob follow it. That's the APVTS round trip from lesson 11, live.

**"VST3 not showing in DAW" checklist:** bundle in the right folder? (On
Windows: the *folder* `Day Ruiner.vst3`, not a loose file.) DAW rescanned
after copying? Correct architecture (x64 plugin, x64 DAW)?

---

## 4. pluginval — the industry validator (Windows)

**pluginval** ([github.com/Tracktion/pluginval](https://github.com/Tracktion/pluginval))
is the open-source tool the audio industry uses to shake down plugins: it
loads your VST3 in a test host and hammers it — valid and invalid parameter
values, state save/restore, all sample rates and block sizes, editor
open/close cycles — checking for crashes and spec violations.

1. Go to the [pluginval Releases page](https://github.com/Tracktion/pluginval),
   download the Windows build, and unzip it somewhere (e.g.
   `C:\tools\pluginval\`).
2. Run it against your installed VST3. In PowerShell:

```powershell
& "C:\tools\pluginval\pluginval.exe" --validate "C:\Program Files\Common Files\VST3\Day Ruiner.vst3" --strictness-level 5
```

In cmd:

```cmd
"C:\tools\pluginval\pluginval.exe" --validate "C:\Program Files\Common Files\VST3\Day Ruiner.vst3" --strictness-level 5
```

### What `--strictness-level` means

pluginval's tests are graded 1–10:

| Level | What it does |
|---|---|
| 1–3 | Quick sanity checks: does it load, do the basic calls work, does it crash |
| **5** | **The recommended minimum** (and the default): full host-compatibility suite |
| 6–10 | Longer, more punishing tests: parameter fuzzing (random values), repeated state save/restore, stress scenarios |

We validate at **5** — the level the pluginval authors recommend for host
compatibility. If it passes at 5, your plugin behaves correctly in real
hosts. Higher levels are for release hardening; feel free to try 10 once 5
is green, but expect it to take much longer.

Exit code **0** = all tests passed. Any failure prints which test failed —
paste that output into lesson 14's debugging workflow.

> pluginval also ships Linux builds, so Kali users can run the same command
> against `~/.vst3/Day Ruiner.vst3` — but the walkthrough above targets
> Windows, where most commercial DAWs (and their picky plugin scanners)
> live.

---

## Check your understanding

- **Standalone first**: run it directly (Kali) or double-click the `.exe`
  (Windows); it needs an audio *output* device, ASIO is optional.
- **Smoke test**: renders 1.5M samples through the real processor, scans
  for NaN/inf/blowups — you want `SMOKE TEST: PASS` and a musical
  `smoke_output.wav`.
- **VST3 install**: Windows → `C:\Program Files\Common Files\VST3\`
  (whole bundle folder); Kali → `~/.vst3/`; then **rescan** in the DAW.
- **Reaper** (free to evaluate) is the suggested test DAW on both
  platforms.
- **pluginval** at `--strictness-level 5` (recommended minimum; scale is
  1–10, higher = fuzzing and stress tests) validates host compatibility.
- Next: lesson 14 — when something goes wrong.
