# Lesson 14 — Debugging Tips: When Something Goes Wrong

Every audio developer spends half their life debugging. This lesson is your
field manual, organized by *where* things break: **build time** (it won't
compile), **launch time** (it won't run), and **DSP time** (it runs but
sounds wrong — or silent, or like a fax machine). Platform sections first,
then the DSP techniques that apply everywhere.

---

## Kali Linux: build and launch problems

### "fatal error: X11/extensions/XInput2.h: No such file or directory"

You missed `libxi-dev` from lesson 01. The general pattern for all of these:

| Error mentions | Missing package |
|---|---|
| `X11/...` | a `libx*-dev` package (`libxi-dev`, `libxrandr-dev`, …) |
| `alsa/asoundlib.h` | `libasound2-dev` |
| `jack/...` | `libjack-jackd2-dev` |
| `freetype/...` | `libfreetype6-dev` |
| `GL/...` | `libgl1-mesa-dev` |
| `webkit2gtk` | `libwebkit2gtk-4.1-dev` |

Fix: `sudo apt update && sudo apt install -y <package>`, then re-run the
build (`cmake --build …` — no need to reconfigure for a missing system
header, but it doesn't hurt).

### "error while loading shared libraries: lib….so: cannot open shared object file"

A *runtime* linker error — it compiled, but a system library is missing at
launch. Diagnose with `ldd`, which lists every shared library a binary
needs and whether each was found:

```bash
ldd ~/DayRuinerEngine/build/DayRuinerEngine_artefacts/Release/VST3/"Day Ruiner.vst3"/Contents/x86_64-linux/"Day Ruiner.so" | grep "not found"
```

Any line ending in `not found` names the missing library — `apt search` for
it, install the package, done. (Also useful: `ldd` on the standalone binary
if it refuses to start.)

### Headless machines: `xvfb-run`

On a server or VM with no display, GUI apps can't open windows. **Xvfb** (X
virtual framebuffer) fakes a display:

```bash
sudo apt install -y xvfb
xvfb-run -a ~/DayRuinerEngine/build/DayRuinerEngine_artefacts/Release/Standalone/"Day Ruiner"
```

The reference build was smoke-tested exactly this way — the standalone
launched under Xvfb with no JUCE assertion dialogs. If you get JUCE
assertion popups under Xvfb, *read them*: they're JUCE telling you about a
real bug (usually a threading violation), not noise.

### Blank window on Wayland

JUCE speaks X11; on a Wayland session it goes through XWayland, which is
usually automatic. If the standalone opens to a blank/gray window:

```bash
GDK_BACKEND=x11 ~/DayRuinerEngine/build/DayRuinerEngine_artefacts/Release/Standalone/"Day Ruiner"
```

or log into an X11/Xorg session at the login screen instead of Wayland.

### No audio from the standalone

1. `pavucontrol` → Playback tab → find the app → unmute, set the right
   output device.
2. Remember: the standalone uses ALSA, which on modern Kali routes through
   PipeWire's compat layer. If PipeWire itself is unhappy, everything
   ALSA-flavored is unhappy — check `systemctl --user status pipewire`.
3. And the Kali rule from lesson 01: **don't run as root** — audio and GUI
   both misbehave for root in subtle ways.

---

## Windows: build and launch problems

### "No CMAKE_CXX_COMPILER could be found" / MSVC toolset errors

- You're on the Ninja route in plain PowerShell → switch to the **x64
  Native Tools Command Prompt for VS 2022** (lesson 12, Route B), or use the
  Visual Studio generator route (Route A) instead.
- Or the workload is missing → Visual Studio Installer → Modify → check
  **"Desktop development with C++"** (needs MSVC v143 + Windows SDK).

### Cryptic `C1083` / "file not found" deep in the build tree

Almost always the **260-character path limit**. Move the project to
`C:\dev\DayRuinerEngine` and wipe `build\` (delete the folder, reconfigure
from scratch — stale CMake caches with old paths cause their own
confusion). Optionally enable long paths system-wide
(`LongPathsEnabled`), but shallow paths are the reliable fix.

### "Access denied" link errors, or the build is inexplicably slow

**Antivirus real-time scanning** locking fresh build outputs. Exclude the
build directory: Windows Security → Virus & threat protection → Manage
settings → Exclusions → add `C:\dev\DayRuinerEngine\build\`.

### VST3 builds fine but doesn't show in the DAW

1. Is the **whole `Day Ruiner.vst3` folder** in `C:\Program Files\Common
   Files\VST3\`? (Not the `.so`/binary alone — the bundle folder.)
2. Did you **rescan** plugins after copying? (Reaper: Preferences →
   Plug-ins → VST → Re-scan.)
3. 64-bit DAW + 64-bit plugin? (Our `-A x64` build is 64-bit; a 32-bit host
   won't see it.)
4. Still nothing → run **pluginval** (lesson 13): if pluginval can't load
   it either, the problem is the binary; if pluginval loads it fine, the
   problem is the DAW's scanner/cache.

---

## DSP debugging: it runs, but it's wrong

These techniques apply on both platforms. They're also *why* the smoke test
exists — most of these checks are automated there, and you should steal the
pattern for your own DSP code.

### 1. Hunt NaN and inf first, always

`NaN` (not-a-number) and `inf` are **contagious**: one NaN sample through a
feedback delay or reverb poisons every future sample, and NaN comparisons
are always false, so the corruption spreads silently. Symptoms: sudden
silence, full-scale white noise, or a "stuck" loud tone.

The smoke test's scan loop (`Source/SmokeTest.cpp`) is the template — copy
this shape into any DSP debugging session:

```cpp
for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
{
    const float* d = buffer.getReadPointer (ch);
    for (int s = 0; s < buffer.getNumSamples(); ++s)
    {
        if (std::isnan (d[s]))  { /* log block/sample, then break */ }
        if (std::isinf (d[s]))  { /* same */ }
    }
}
```

**Binary-search the chain:** when NaN appears, scan the buffer *between*
each stage (after synth render, after granular, after saturator, …). The
first stage whose output contains NaN is your culprit. Then look at that
stage for the usual suspects: **division by a value that can be zero**,
`std::sqrt`/`std::log` of a negative, uninitialized state (lesson 07's
`modPhaseInc`), or a feedback coefficient ≥ 1.0.

### 2. `DBG()` and `juce::Logger`

`DBG("bpm = " << bpm);` prints to the debugger console / stderr — the
quickest printf-debugging in JUCE. Two caveats:

- `DBG()` compiles to **nothing in Release builds** (it's guarded by
  `JUCE_DEBUG`), so it's safe to leave in — but it also means you can't use
  it to debug a Release-only problem. For that, use
  `juce::Logger::writeToLog()`, which works in all configurations.
- **Never `DBG()` per sample on the audio thread** — 44,100 log lines a
  second will hang or crash anything. Log per *block*, or gate it (e.g.
  only when a counter hits a condition).

### 3. Denormals: the invisible CPU fire

Floating-point numbers very close to zero (like `1e-38`) are called
**denormals**, and on many CPUs each denormal arithmetic operation runs
**10–100× slower** than normal. A reverb tail decaying toward zero can fill
your block with denormals and spike CPU usage for no audible reason.

That's why `processBlock()` opens with:

```cpp
juce::ScopedNoDenormals noDenormals;
```

This flips the CPU's flush-to-zero mode for the scope: denormals are treated
as zero, at full speed. If you ever profile a mysterious CPU spike in a
reverb/delay tail, denormals are suspect #1 — and `ScopedNoDenormals` is the
fix. (It's in `processBlock()` rather than each engine so the whole chain
is covered.)

### 4. Assertions are bug reports, not noise

JUCE is full of `jassert()`s — they fire in Debug builds when you violate a
contract (calling a message-thread-only method on the audio thread,
passing a null pointer, etc.). **Never "fix" an assertion by deleting it.**
Each one is JUCE telling you exactly what you did wrong and where. The
reference build launched under Xvfb with zero assertions — that's the bar.

### 5. When stuck: reduce, don't guess

The universal DSP debugging algorithm:

1. **Bypass stages** until the symptom disappears (comment out the reverb —
   still broken? comment out the delay…). The last stage you removed is the
   neighborhood.
2. **Feed it silence, then feed it a known signal** (the smoke test's sine
   sweep is ideal — deterministic input, deterministic output).
3. **Check the boring stuff**: sample rate actually set? Buffer sizes match?
   Reading from the right channel? Off-by-one in an offset?

Most audio bugs are not exotic. They're an uninitialized variable, a wrong
rate, or an operation in the wrong order — exactly the species documented in
`DEVIATIONS.md`.

---

## Check your understanding

- **Kali:** missing-header errors → install the matching `-dev` package;
  runtime link errors → `ldd … | grep "not found"`; headless GUI →
  `xvfb-run`; blank window → `GDK_BACKEND=x11` or an X11 session; no audio →
  `pavucontrol`; never build/run as root.
- **Windows:** no compiler → check the C++ workload / use the Native Tools
  prompt; deep-tree `C1083` → shorten the path; link "access denied" →
  antivirus exclusion; missing VST3 → bundle folder + DAW rescan, then
  pluginval to isolate.
- **DSP:** NaN/inf are contagious — scan per stage to localize; `DBG()` is
  Debug-only, `juce::Logger` works everywhere, never log per-sample;
  denormals cause mystery CPU spikes → `juce::ScopedNoDenormals`;
  assertions are bug reports; reduce the chain to isolate.
- Next: `DEVIATIONS.md` — the 37 real bugs, as a design document.
