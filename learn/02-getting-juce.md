# Lesson 02 — Getting JUCE 9.0.3

JUCE is the C++ framework DAY RUINER is built on. It provides *everything* the
plugin needs from the operating system: the audio device handling, the plugin
formats (VST3, standalone), the GUI toolkit, the DSP helpers, file I/O — all
of it. Without JUCE, you'd be writing thousands of lines of platform-specific
code yourself. With JUCE, `Source/PluginProcessor.cpp` is 350 lines.

This lesson: download exactly the right version of JUCE, put it in a known
place, and understand how our build finds it.

---

## Step 1 — Clone JUCE at the exact version

DAY RUINER was written against **JUCE 9.0.3**. JUCE's API drifts between
versions (lesson `DEVIATIONS.md` documents four places where JUCE 9 differs
from JUCE 7 — e.g. `juce::Reverb` lost its `setEnabled()` method). Building
against the wrong version *will* produce confusing compile errors. Pin the
version with the `--branch` flag:

**Kali Linux** (and Windows — Git commands are identical on both):

```bash
git clone --depth 1 --branch 9.0.3 https://github.com/juce-framework/JUCE.git ~/JUCE
```

Let's unpack that command, because every flag matters:

- `git clone <url>` — downloads a copy of JUCE's GitHub repository.
- `--branch 9.0.3` — checks out the `9.0.3` tag instead of the default
  `develop` branch. **This is the version pin.** Without it you'd get whatever
  JUCE's developers pushed this week, and the code might not compile.
- `--depth 1` — a **shallow clone**: downloads only the latest snapshot, not
  the entire 20-year history of the repository. JUCE's full history is
  gigabytes; a shallow clone is a few hundred megabytes and much faster. The
  tradeoff: you can't browse old history — which you don't need.
- `~/JUCE` — the destination folder: a folder called `JUCE` in your home
  directory. Keep the name exactly `JUCE`; later steps assume it.

> **Windows note:** run this in PowerShell or Git Bash — `~` means your home
> directory (`C:\Users\<you>`) in both. The command is byte-for-byte the same
> as on Kali.

This downloads ~300–500 MB. Wait for it to finish.

## Step 2 — Verify the clone

```bash
ls ~/JUCE/modules | head -30
```

You should see JUCE's module directories: `juce_audio_basics`,
`juce_audio_devices`, `juce_audio_formats`, `juce_audio_processors`,
`juce_core`, `juce_dsp`, `juce_gui_basics`, and so on. Our `CMakeLists.txt`
links 13 of these (lesson 04 lists them all).

Confirm the version:

```bash
git -C ~/JUCE describe --tags
```

This should print `9.0.3`. (If it prints something else, you cloned the wrong
branch — delete `~/JUCE` and re-run step 1.)

## Step 3 — Understand how the build finds JUCE

Open `CMakeLists.txt` in the project root and look at this line near the top:

```cmake
# JUCE location: uses the local clone on this machine. The learn/ lessons tell
# users how to point this at their own JUCE clone.
add_subdirectory($ENV{HOME}/workspace/JUCE JUCE)
```

`add_subdirectory()` tells CMake: "there is another CMake project at this
path — build it as part of mine." JUCE ships its own `CMakeLists.txt`, so
this one line pulls the entire framework into our build.

**You will almost certainly need to edit this line.** It currently points at
`$ENV{HOME}/workspace/JUCE` (the reference machine's location). If you cloned
JUCE to `~/JUCE` as this lesson instructed, change the line to:

```cmake
add_subdirectory($ENV{HOME}/JUCE JUCE)
```

> **Windows:** `$ENV{HOME}` works on Windows too — CMake sets `HOME` from
> your user profile. Alternatively use an absolute path with forward slashes,
> which CMake accepts on all platforms:
> ```cmake
> add_subdirectory(C:/Users/<you>/JUCE JUCE)
> ```
> (Use forward slashes even on Windows — CMake treats backslashes as escape
> characters, so `C:\Users\...` will break.)

The second argument (`JUCE`) is just the name CMake gives the sub-build's
binary directory inside `build/`. You don't need to change it.

**Why not just put JUCE inside the project folder?** You can — some people
keep it at `DayRuinerEngine/JUCE` and write `add_subdirectory(JUCE)`. The
reference build kept it *outside* the project so one JUCE clone can serve
multiple projects (and so the project folder stays small). Either way works;
what matters is that the path in `add_subdirectory()` is correct. If CMake
says something like "The source directory … does not contain a CMakeLists.txt
file" during configuration (lesson 12), this path is the first thing to
check.

## What you just got (a 30-second tour)

You don't need to read JUCE's source, but knowing what's in the box helps:

- `modules/juce_audio_processors/` — the `AudioProcessor` base class our
  processor inherits from, plus `AudioProcessorValueTreeState` (parameters).
- `modules/juce_dsp/` — DSP building blocks (`juce::dsp::Convolution`,
  `juce::Reverb`, …).
- `modules/juce_gui_basics/` + `juce_gui_extra/` — the entire GUI toolkit.
- `modules/juce_audio_plugin_client/` — the VST3/Standalone "wrappers" that
  turn our `AudioProcessor` into a real plugin binary.

JUCE is big — that's why the first build takes several minutes (lesson 12).
You're compiling a substantial framework, not just our 36 files.

---

## Check your understanding

- DAY RUINER needs **exactly JUCE 9.0.3** — API drift between versions breaks
  the build.
- Clone it shallow and pinned:
  `git clone --depth 1 --branch 9.0.3 https://github.com/juce-framework/JUCE.git ~/JUCE`
- `--depth 1` skips JUCE's multi-gigabyte history; `--branch 9.0.3` pins the
  version.
- Verify with `git -C ~/JUCE describe --tags` → `9.0.3`, and `ls ~/JUCE/modules`.
- Our build finds JUCE via `add_subdirectory(...)` in `CMakeLists.txt` —
  **edit that path** to match where you actually cloned JUCE.
- Next: lesson 03 tours the project's own source tree.
