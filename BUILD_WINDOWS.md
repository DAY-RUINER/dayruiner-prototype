# Building DAY RUINER on Windows

The prototype builds the **VST3** (`Day Ruiner.vst3`) and a **Standalone**
app with MSVC. The easy path is GitHub Actions (see
`.github/workflows/windows-vst3.yml`): push to `main` (or run it manually
from the Actions tab) and download the `DayRuiner-Windows-VST3` artifact.
It caches the JUCE clone and the CMake build dir, so the first run takes
~10–20 minutes and repeat runs are fast.

## Manual build

### Prerequisites

- **Visual Studio 2022 Community** (free) with the
  *Desktop development with C++* workload (MSVC v143, x64).
- **CMake** 3.22+ — install from https://cmake.org/download/
  (check "Add CMake to the system PATH").
- **Git** — https://git-scm.com/download/win

### Steps

```powershell
# 1. Clone this prototype tree, then from its root:
git clone --depth 1 --branch 9.0.3 https://github.com/juce-framework/JUCE.git _juce93
cd _juce93; git describe --tags   # must print: 9.0.3
cd ..

# 2. Configure (VS2022, 64-bit)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release

# 3. Build just the VST3 (or omit --target to build everything,
#    including the Standalone app and the DSP smoke test)
cmake --build build --target DayRuinerEngine_VST3 --config Release
```

### Where the VST3 lands

```
build\Source\DayRuinerEngine_artefacts\Release\VST3\Day Ruiner.vst3
```

### Installing it

Copy (or move) the whole `Day Ruiner.vst3` **folder** — not just the files
inside — to:

```
C:\Program Files\Common Files\VST3\
```

Then rescan plug-ins in your DAW (Ableton: Preferences → Plug-ins →
Rescan; Reaper: Options → Preferences → VST → Re-scan; FL Studio:
Manage plugins → Find installed plugins). It appears as
**Day Ruiner** (manufacturer "Day Ruiner", category Instrument/Synth).

The Standalone app (same build, no `--target` filter) lands at:

```
build\Source\DayRuinerEngine_artefacts\Release\Standalone\Day Ruiner.exe
```

It runs without a DAW — useful for a first smoke test (needs an audio
output device; ASIO4ALL works if you have no interface).

### Troubleshooting

- **`git describe --tags` doesn't print 9.0.3** — the `--branch 9.0.3`
  clone is required; JUCE 8/9 API drift breaks the GUI build otherwise.
- **CMake can't find the VS generator** — open the "x64 Native Tools
  Command Prompt for VS 2022" instead of plain PowerShell, or reinstall
  the C++ workload.
- **First build is slow** — that's JUCE compiling (~10–20 min); it's cached
  afterwards, including in CI.
