# Lesson 04 — The CMake Build System

Our build recipe is a single file: `CMakeLists.txt` in the project root (123
lines). This lesson walks it block by block. By the end you'll understand what
every line does — and you'll be able to adapt it for your own projects.

## CMake vs Projucer: why CMake?

JUCE ships with its own project-management GUI called the **Projucer**: you
click through settings, and it generates IDE project files (Visual Studio
`.sln`, Xcode, Makefiles…). It's friendly, and JUCE officially supports it.

We use **CMake** instead, for three reasons you should understand as a
developer:

1. **CMake is text.** The entire build configuration is one readable,
   diffable, version-controllable file. Projucer stores settings in a binary
   `.jucer` file that you can't meaningfully diff or review.
2. **CMake works headless.** This project was built on machines with no GUI
   at all — you can't click through the Projucer without a screen. CMake runs
   anywhere: servers, CI pipelines, your laptop.
3. **CMake is the industry default.** Most professional C++ audio teams use
   CMake. Learning it here pays off everywhere.

JUCE's CMake support (`juce_add_plugin`, `juce_add_console_app`, …) is
first-class and maintained by the JUCE team — we're not fighting the
framework by choosing it.

### The two-phase mental model

CMake works in two phases, and keeping them separate will save you confusion:

- **Configure phase** (`cmake -S <src> -B <build> …`): CMake *reads*
  `CMakeLists.txt`, finds JUCE, checks the compiler, and *generates* build
  files (for us: Ninja files) into the `build/` directory. No compiling
  happens yet.
- **Build phase** (`cmake --build <build> …`): the generated build files are
  executed — this is when the compiler actually runs.

You'll run both in lesson 12. Now, the file itself.

---

## Block 1 — Preamble

```cmake
cmake_minimum_required(VERSION 3.22)

project(DayRuinerEngine VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
```

- `cmake_minimum_required(VERSION 3.22)` — refuses to run on ancient CMake.
  3.22 is old enough to be in every distro's repo, new enough for everything
  JUCE needs.
- `project(DayRuinerEngine VERSION 0.1.0)` — names the project. Note this
  internal name (`DayRuinerEngine`) differs from the user-facing product name
  (`"Day Ruiner"`, set below). CMake target names can't contain spaces, so
  the internal name is spaceless.
- `set(CMAKE_CXX_STANDARD 17)` — compile as **C++17**. This is the language
  standard: it decides which language features you may use
  (`std::optional`, structured bindings, `if` with initializer like
  `if (auto* p = …; p == nullptr)` — you'll see that JUCE 9 idiom in
  `SmokeTest.cpp`). JUCE 9 requires at least C++17.

## Block 2 — Finding JUCE

```cmake
# JUCE location: uses the local clone on this machine. The learn/ lessons tell
# users how to point this at their own JUCE clone.
add_subdirectory($ENV{HOME}/workspace/JUCE JUCE)
```

As lesson 02 explained: this pulls JUCE's own CMake project into our build.
**Edit this path to match where you cloned JUCE** (e.g.
`$ENV{HOME}/JUCE`). Everything below depends on this line succeeding — if
CMake can't find JUCE here, nothing else works.

## Block 3 — The plugin target: `juce_add_plugin`

```cmake
juce_add_plugin(DayRuinerEngine
    PRODUCT_NAME "Day Ruiner"
    COMPANY_NAME "Day Ruiner"
    BUNDLE_ID com.dayruiner.engine
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    FORMATS VST3 Standalone
    VST3_CATEGORIES "Instrument" "Synth"
    PLUGIN_MANUFACTURER_CODE DayR
    PLUGIN_CODE DRu1
)
```

`juce_add_plugin` is JUCE's CMake function that declares "this target is an
audio plugin." Each option:

- `PRODUCT_NAME "Day Ruiner"` — the human-readable name. This becomes the
  VST3 bundle folder name (`Day Ruiner.vst3`) and the standalone app name.
- `COMPANY_NAME "Day Ruiner"` — shown in DAWs as the manufacturer.
- `BUNDLE_ID com.dayruiner.engine` — the unique reverse-DNS identifier for
  the plugin bundle. **This must be globally unique** — it's how hosts tell
  your plugin apart from every other plugin on earth. Convention is
  `com.yourname.product`.
- `IS_SYNTH TRUE` — tells hosts "this is an instrument, not an effect."
  (Note: JUCE 9's `AudioProcessor` base class has no `isSynth()` method, so
  our processor also defines its own plain `isSynth()` query — see lesson 05.)
- `NEEDS_MIDI_INPUT TRUE` — the plugin accepts MIDI (notes trigger the synth
  and sequencer).
- `FORMATS VST3 Standalone` — build two binaries from the same code: the
  **VST3** plugin for DAWs, and a **Standalone** app you can double-click.
  (JUCE can also do AU, AAX, LV2, Unity — we only ask for these two.)
- `VST3_CATEGORIES "Instrument" "Synth"` — how the plugin is filed in the
  host's browser.
- `PLUGIN_MANUFACTURER_CODE DayR` / `PLUGIN_CODE DRu1` — four-character codes
  identifying the plugin. `DayR`/`DRu1` are ours. (Steinberg-issued codes
  exist for commercial VST3s; for learning, any unique 4-char code is fine.)

```cmake
juce_generate_juce_header(DayRuinerEngine)
```

This generates the famous **`JuceHeader.h`** — a single header that
`#include`s every JUCE module we link. Our sources do `#include
<JuceHeader.h>` and get the whole framework. (It's generated into
`build/DayRuinerEngine_artefacts/JuceLibraryCode/` — you'll see it there
after configuring.)

## Block 4 — Our sources

```cmake
target_sources(DayRuinerEngine PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/dsp/SampleBuffer.cpp
    ... (all 17 .cpp files)
)
```

Explicit list, no globbing. (CMake *can* auto-glob with `file(GLOB …)`, but
explicit lists are preferred: if you add a file, you add it here
deliberately, and a stale build directory can never silently miss it.)

Note what's listed: only `.cpp` files. Headers are found automatically via
`#include`. And note `Source/SmokeTest.cpp` is **not** here — it belongs to
the second target below.

## Block 5 — Linking the JUCE modules

```cmake
target_link_libraries(DayRuinerEngine PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_plugin_client
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
```

13 JUCE modules, grouped by what they cover:

| Modules | Family |
|---|---|
| `juce_core`, `juce_events`, `juce_data_structures` | Foundation: strings, files, threads, messaging, ValueTrees |
| `juce_graphics`, `juce_gui_basics`, `juce_gui_extra` | GUI: drawing, widgets, windows, web views |
| `juce_audio_basics`, `juce_audio_devices`, `juce_audio_formats` | Audio I/O: buffers, sound cards, WAV/AIFF/MP3 reading |
| `juce_audio_processors`, `juce_audio_plugin_client` | Plugin hosting: the `AudioProcessor` API + the VST3/Standalone wrappers |
| `juce_audio_utils` | GUI helpers for audio apps (audio device selector, etc.) |
| `juce_dsp` | DSP toolkit (`juce::dsp::Convolution`, `juce::Reverb`, SIMD helpers) |

The three `juce_recommended_*_flags` targets are JUCE's blessed compiler
settings, applied `PUBLIC` (so they propagate):

- `juce_recommended_config_flags` — standard defines (debug vs release).
- `juce_recommended_lto_flags` — link-time optimization in Release builds
  (lets the optimizer see across translation units — meaningful for DSP).
- `juce_recommended_warning_flags` — the warning set our code compiles
  cleanly under: `-Wall -Wextra -Wpedantic -Wshadow -Wsign-conversion
  -Wswitch-enum -Woverloaded-virtual`. **Notably, this does NOT include
  `-Werror`** despite older docs suggesting otherwise — warnings don't fail
  our build, but the reference build produced **zero warnings** in our own
  sources anyway. (Getting to zero took real work — see `DEVIATIONS.md`
  items 35–36: the `using juce::AudioProcessor::processBlock;` line that
  silences `-Woverloaded-virtual`, and the `-Wshadow`/`-Wsign-conversion`
  cleanups.)

## Block 6 — Compile definitions

```cmake
target_compile_definitions(DayRuinerEngine PUBLIC
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)
```

These are `#define`s injected into every translation unit:

- `JUCE_VST3_CAN_REPLACE_VST2=0` — don't advertise VST2-replacement
  compatibility (we ship no VST2).
- `JUCE_DISPLAY_SPLASH_SCREEN=0` — skip JUCE's splash screen in the
  standalone app.
- `JUCE_WEB_BROWSER=0` / `JUCE_USE_CURL=0` — disable the web-browser
  component and cURL networking. We don't need them, and disabling them
  removes link dependencies (this is part of why the `libcurl4-openssl-dev`
  and `libwebkit2gtk` packages from lesson 01 are "headers JUCE wants"
  rather than features we use).

## Block 7 — The smoke-test target

```cmake
juce_add_console_app(DayRuinerSmokeTest)

juce_generate_juce_header(DayRuinerSmokeTest)

target_sources(DayRuinerSmokeTest PRIVATE
    Source/SmokeTest.cpp
    Source/PluginProcessor.cpp
    Source/dsp/SampleBuffer.cpp
    ... (dsp + sequencer sources, NO gui/, NO PluginEditor.cpp)
)
```

`juce_add_console_app` declares a plain command-line program (no windows, no
plugin wrappers). The smoke test links `PluginProcessor.cpp` and all the DSP
and sequencer sources — but deliberately **excludes** `PluginEditor.cpp` and
everything under `Source/gui/`, because a console app has no GUI.

That exclusion creates a problem: `PluginProcessor.cpp`'s `createEditor()`
references the editor class, which isn't compiled in. The fix is the
compile definition:

```cmake
target_compile_definitions(DayRuinerSmokeTest PRIVATE
    DAYRUINER_HEADLESS_SMOKE_TEST=1
    ...
)
```

and in `PluginProcessor.cpp`:

```cpp
juce::AudioProcessorEditor* DayRuinerAudioProcessor::createEditor()
{
#ifdef DAYRUINER_HEADLESS_SMOKE_TEST
    return nullptr; // headless smoke test links no GUI sources (no editor defined)
#else
    return new DayRuinerAudioProcessorEditor (*this);
#endif
}
```

A preprocessor guard swaps the editor-creating code for `return nullptr`
when building the smoke test. This is a standard technique: **one source
file, two build configurations, decided at compile time.** (`DEVIATIONS.md`
item 37.)

## Check your understanding

- **CMake over Projucer** because the build is text (diffable), headless
  (no GUI needed), and industry-standard. JUCE officially supports both.
- Configure phase *generates* build files; build phase *compiles*.
- `juce_add_plugin` declares the plugin and its metadata (`PRODUCT_NAME`,
  `BUNDLE_ID`, `FORMATS VST3 Standalone`, plugin codes…).
- 13 JUCE modules are linked, in foundation / GUI / audio / plugin / DSP
  families, plus JUCE's recommended config, LTO, and warning flags.
- Warning flags are strict (`-Wshadow`, `-Wsign-conversion`, …) but do
  **not** include `-Werror`; our sources still compile with zero warnings.
- The smoke test is a **separate console-app target** reusing the same DSP
  sources minus the GUI, via the `DAYRUINER_HEADLESS_SMOKE_TEST` guard.
- Next: lesson 05 — the processor, the heart of the plugin.
