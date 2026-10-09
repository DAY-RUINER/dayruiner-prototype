# Lesson 12 — Compiling: From Source to Plugin

This is the payoff lesson: you turn 36 source files into a real plugin. It
happens in the two CMake phases from lesson 04 — **configure**, then
**build** — with platform-specific commands below. Follow your platform's
section exactly; every command is copy-pasteable.

**Before you start:** lessons 01 (toolchain) and 02 (JUCE clone) must be
done, and the `add_subdirectory(...)` path in `CMakeLists.txt` must point at
your JUCE clone (lesson 02, step 3). This lesson assumes your project lives
at `~/DayRuinerEngine` on Kali or `C:\dev\DayRuinerEngine` on Windows —
adjust the paths if yours differs.

> **Keep the project path short**, especially on Windows (see
> Troubleshooting). `C:\dev\DayRuinerEngine` is ideal; avoid deep nesting
> like `C:\Users\<you>\Documents\Projects\Audio\Experiments\…`.

---

## Kali Linux

### Step 1 — Configure

```bash
cmake -S ~/DayRuinerEngine -B ~/DayRuinerEngine/build -G Ninja -DCMAKE_BUILD_TYPE=Release
```

What each flag does:

- `-S ~/DayRuinerEngine` — the **source** directory (where `CMakeLists.txt` lives).
- `-B ~/DayRuinerEngine/build` — the **build** directory (created for you; all generated files go here, never into `Source/`).
- `-G Ninja` — generate **Ninja** build files (fast parallel builder).
- `-DCMAKE_BUILD_TYPE=Release` — optimized build, no debug overhead. (Single-configuration generators like Ninja need this at configure time.)

**What success looks like.** The output ends with something like:

```
-- Configuring done
-- Generating done
-- Build files have been written to: /home/<you>/DayRuinerEngine/build
```

You'll also see JUCE's configuration scroll past (checking modules,
generating `JuceHeader.h` into
`build/DayRuinerEngine_artefacts/JuceLibraryCode/`). The key line is
**"Configuring done"** — if you see that, JUCE was found and every check
passed.

**If it fails here**, it's almost always one of: the JUCE path in
`add_subdirectory()` is wrong ("does not contain a CMakeLists.txt"), a
`-dev` package from lesson 01 is missing (a `Could NOT find …` or missing
header error), or CMake is too old. See lesson 14.

### Step 2 — Build

```bash
cmake --build ~/DayRuinerEngine/build --config Release
```

Now the compiler actually runs. **Expect this to take several minutes**
(5–15 on a typical laptop) — you're compiling all of JUCE plus our code.
Ninja parallelizes across your CPU cores; don't be alarmed by 100% CPU usage.

**What success looks like.** Hundreds of `[12/234] Building CXX object …`
lines, ending with link steps. Watch for the final lines:

```
[N/N] Linking CXX executable DayRuinerEngine_artefacts/Release/Standalone/Day Ruiner
[N/N] Linking CXX shared module DayRuinerEngine_artefacts/Release/VST3/Day Ruiner.vst3/Contents/x86_64-linux/Day Ruiner.so
```

(The exact `[N/N]` count varies by machine — what matters is that the last
lines are **Linking** steps with no `FAILED` lines.) Our own sources compile
with **zero warnings** under JUCE's strict warning flags (lesson 04) — you
may see warnings from inside JUCE itself on some compilers; those are
upstream's, not ours.

### Step 3 — Find your artifacts

On Linux, everything lands under `build/DayRuinerEngine_artefacts/Release/`:

| Artifact | Path (relative to project root) |
|---|---|
| VST3 bundle | `build/DayRuinerEngine_artefacts/Release/VST3/Day Ruiner.vst3` |
| VST3 binary module (inside the bundle) | `…/Day Ruiner.vst3/Contents/x86_64-linux/Day Ruiner.so` |
| Standalone app | `build/DayRuinerEngine_artefacts/Release/Standalone/Day Ruiner` |
| Smoke test | `build/DayRuinerSmokeTest_artefacts/Release/DayRuinerSmokeTest` |
| Smoke-test WAV (after running it) | `build/smoke_output.wav` |

Verify they exist:

```bash
ls -la ~/DayRuinerEngine/build/DayRuinerEngine_artefacts/Release/VST3/
ls -la ~/DayRuinerEngine/build/DayRuinerEngine_artefacts/Release/Standalone/
ls -la ~/DayRuinerEngine/build/DayRuinerSmokeTest_artefacts/Release/
file ~/DayRuinerEngine/build/DayRuinerEngine_artefacts/Release/VST3/"Day Ruiner.vst3"/Contents/x86_64-linux/"Day Ruiner.so"
```

The `file` command should report `ELF 64-bit LSB shared object, x86-64`.
If it says that, you built a real plugin. Lesson 13 runs it.

---

## Windows

You have two build routes. **Use the Visual Studio generator** (primary) —
it works from plain PowerShell and avoids the vcvars/PowerShell awkwardness
from lesson 01. The Ninja route is documented as the alternative.

### Route A — Visual Studio generator (recommended)

In **PowerShell**, from the project directory:

```powershell
cd C:\dev\DayRuinerEngine
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
```

Flags: `-G "Visual Studio 17 2022"` selects the VS 2022 generator (the
quotes are required — the name contains spaces); `-A x64` targets 64-bit.
Note there's **no** `-DCMAKE_BUILD_TYPE` — Visual Studio is a
multi-configuration generator; you pick the configuration at *build* time
instead.

**What success looks like:**

```
-- Configuring done
-- Generating done
-- Build files have been written to: C:/dev/DayRuinerEngine/build
```

Then build:

```powershell
cmake --build build --config Release
```

`--config Release` is where you choose the optimized configuration. This
takes several minutes (JUCE is big — 10–20 minutes on a modest laptop is
normal). MSBuild parallelizes automatically.

**In cmd**, the commands are identical (only the prompt differs) — PowerShell
and cmd both handle the quoted generator name the same way.

### Route B — Ninja (alternative)

Open **x64 Native Tools Command Prompt for VS 2022** from the Start Menu
(this is required — it's what puts `cl.exe` on PATH), then:

```cmd
cd C:\dev\DayRuinerEngine
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Same two phases as Kali. Do **not** run the Ninja configure from plain
PowerShell — CMake won't find the compiler without the Native Tools
environment (that's the vcvars awkwardness from lesson 01).

### Find your artifacts (Windows)

Everything lands under `build\DayRuinerEngine_artefacts\Release\`:

| Artifact | Path (relative to project root) |
|---|---|
| VST3 bundle | `build\DayRuinerEngine_artefacts\Release\VST3\Day Ruiner.vst3` |
| Standalone app | `build\DayRuinerEngine_artefacts\Release\Standalone\Day Ruiner.exe` |
| Smoke test | `build\DayRuinerSmokeTest_artefacts\Release\DayRuinerSmokeTest.exe` |

Verify in PowerShell:

```powershell
Get-ChildItem build\DayRuinerEngine_artefacts\Release\VST3\
Get-ChildItem build\DayRuinerEngine_artefacts\Release\Standalone\
Get-ChildItem build\DayRuinerSmokeTest_artefacts\Release\
```

> **Quoting paths with spaces:** `"Day Ruiner.vst3"` must be quoted whenever
> a command sees it — PowerShell and cmd both accept double quotes.

### Troubleshooting (Windows)

- **"No CMAKE_CXX_COMPILER could be found" / MSVC toolset not found** —
  the "Desktop development with C++" workload didn't install (or you're on
  the Ninja route outside a Native Tools prompt). Open Visual Studio
  Installer → Modify → confirm the workload is checked; for Ninja, use the
  Native Tools prompt.
- **Path-length errors** (`C1083`, bizarre "file not found" deep in the
  tree) — Windows' legacy 260-character path limit. Keep the project shallow
  (`C:\dev\DayRuinerEngine`), and consider enabling long paths in Windows
  (Group Policy / registry `LongPathsEnabled`).
- **Antivirus locking build outputs** — real-time scanners sometimes lock
  freshly-linked `.exe`/`.dll` files, causing "access denied" link errors or
  mysteriously slow builds. Add an exclusion for your `build\` directory in
  Windows Security → Virus & threat protection → Manage settings →
  Exclusions.
- **"VST3 not showing in DAW"** — the number-one beginner issue, and it's
  never the build: the bundle must be in a folder your DAW scans
  (`C:\Program Files\Common Files\VST3\` — lesson 13), and you must
  **rescan plugins** in the DAW after copying it there.

---

## Check your understanding

- Two phases: **configure** (`cmake -S … -B … -G …`) → look for
  **"Configuring done"**; **build** (`cmake --build … --config Release`) →
  look for final **Linking** lines, no `FAILED`.
- **Kali:** Ninja generator, `-DCMAKE_BUILD_TYPE=Release` at configure
  time; artifacts under `build/DayRuinerEngine_artefacts/Release/`
  (`.vst3` bundle containing `Day Ruiner.so`; standalone `Day Ruiner`).
- **Windows:** prefer the **Visual Studio 17 2022** generator from plain
  PowerShell (`-A x64`, `--config Release` at build time); Ninja route
  requires the **x64 Native Tools prompt**.
- First build takes **several minutes** — JUCE is large; that's normal.
- Windows gotchas: missing C++ workload, path-length limits (keep it
  shallow), antivirus locking outputs, DAW rescan for the VST3.
- Next: lesson 13 — running the standalone, the smoke test, and the VST3 in
  a DAW.
