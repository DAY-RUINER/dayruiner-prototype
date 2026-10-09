# Lesson 01 — Prerequisites: Your Build Toolbox

Before you touch a single line of plugin code, you need a working C++
toolchain: a compiler, a build-system generator (CMake), a fast build tool
(Ninja), Git, and a set of operating-system libraries that JUCE needs to talk
to your audio hardware, your screen, and your GPU.

This lesson gets all of that installed and **verified**. The rule for this
whole series: *never assume an install worked — run the check command.*

Follow the section for your platform.

---

## Kali Linux

### Why each piece exists

| Tool | What it is | Why DAY RUINER needs it |
|---|---|---|
| `build-essential` | GCC/G++ compiler + make + standard headers | Compiles all the C++ |
| `cmake` | Build-system generator | Reads `CMakeLists.txt`, generates build files |
| `ninja-build` | Fast parallel builder | Actually runs the compile (much faster than make) |
| `git` | Version control | Clones JUCE in lesson 02 |

JUCE also needs **system libraries** (the `-dev` packages below) because it
talks directly to Linux audio and windowing APIs. They're grouped by purpose:

| Package group | Packages | Purpose |
|---|---|---|
| ALSA audio | `libasound2-dev` | Talking to sound cards (the standalone app's audio output) |
| JACK audio | `libjack-jackd2-dev` | Pro-audio server support (JUCE links against it) |
| LADSPA | `ladspa-sdk` | Plugin SDK headers JUCE references |
| Networking | `libcurl4-openssl-dev` | HTTP (JUCE's networking classes; we disable most of it, but JUCE still wants the headers) |
| Fonts | `libfreetype6-dev` | Text rendering in the GUI |
| X11 windowing | `libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev libxi-dev` | Creating windows, handling the mouse/keyboard. **JUCE on Linux is an X11 application.** `libxi-dev` in particular is required by `juce_gui_basics` (it needs `XInput2.h`) — if you skip it, the build fails with a missing-header error. |
| OpenGL | `libgl1-mesa-dev` | GPU-accelerated rendering path |
| WebKit | `libwebkit2gtk-4.1-dev` | Embedded web views for `juce_gui_extra` |

> **Kali is a rolling release.** Package names drift over time. Always run
> `sudo apt update` first so your package lists are current. If any `-dev`
> package 404s (not found), search for its current name with
> `apt search <name>` — e.g. `apt search webkit2gtk` — and install whatever the
> current equivalent is.

### Install (copy-paste)

```bash
sudo apt update && sudo apt install -y build-essential cmake ninja-build git \
  libasound2-dev libjack-jackd2-dev ladspa-sdk libcurl4-openssl-dev \
  libfreetype6-dev libx11-dev libxcomposite-dev libxcursor-dev libxext-dev \
  libxinerama-dev libxrandr-dev libxrender-dev libxi-dev libgl1-mesa-dev \
  libwebkit2gtk-4.1-dev
```

This downloads a few hundred megabytes. Let it finish.

### The root-user warning (Kali-specific, important)

Kali historically runs everything as **root**. **Do not build or run this
project as root.** Two reasons:

1. Building as root means every file in your project ends up owned by root,
   which causes permission headaches the moment you do anything as a normal
   user.
2. JUCE GUI applications running as root under Wayland can fail to open
   windows at all.

Create (or use) a normal user for development:

```bash
# As root: create a non-root user called "producer"
adduser producer
usermod -aG sudo,audio,video producer
```

Then log in as that user for everything in this series. If you already cloned
or created project files as root, fix ownership before building:

```bash
sudo chown -R producer:producer ~/DayRuinerEngine ~/JUCE
```

(replace the paths with wherever you actually put them — see lesson 02).

### X11 vs Wayland (Kali-specific)

JUCE on Linux speaks **X11**. If your Kali session runs on **Wayland**, JUCE
apps normally still work through **XWayland**, and this is usually automatic —
you don't need to do anything.

If the standalone app launches but shows a **blank window**, that's the
XWayland handshake failing. Try:

```bash
GDK_BACKEND=x11 ./build/DayRuinerEngine_artefacts/Release/Standalone/"Day Ruiner"
```

or simply log into an **X11/Xorg session** instead of Wayland at the login
screen. (Full details in lesson 14.)

### Audio on Kali

The standalone app uses **ALSA**. On modern Kali, PipeWire is usually the
actual audio server, but it provides an **ALSA compatibility layer**, so JUCE's
ALSA output normally "just works" through PipeWire.

If you get no sound from the standalone app: open `pavucontrol` (PulseAudio
Volume Control — install it with `sudo apt install pavucontrol` if missing)
and check that the app appears in the Playback tab and isn't muted or routed
nowhere.

### Verify (Kali)

Run each of these. Every one must succeed before you continue:

```bash
g++ --version        # should print a GCC version, e.g. 13.x or 14.x
cmake --version      # should print 3.22 or newer
ninja --version      # should print a version like 1.11.x
git --version        # should print a git version
whoami               # should NOT print "root"
```

Also verify the X11 headers JUCE needs most:

```bash
ls /usr/include/X11/extensions/XInput2.h   # must exist (this is the libxi-dev one)
ls /usr/include/alsa/asoundlib.h           # must exist (libasound2-dev)
```

If either `ls` fails, re-run the `apt install` line — you missed a package.

---

## Windows

### What you need

| Tool | Where to get it | Notes |
|---|---|---|
| **Visual Studio 2022 Community** (free) | [visualstudio.microsoft.com](https://visualstudio.microsoft.com) | During install, select the workload **"Desktop development with C++"**. This is the critical step — it installs the MSVC v143 compiler, the C++ standard library, and the Windows SDK. Without this workload, CMake will not find a compiler. |
| **CMake** | [cmake.org](https://cmake.org) (Download page → Windows x64 installer) | During install, check **"Add CMake to the system PATH"** so you can run `cmake` from any terminal. |
| **Ninja** | `winget install Ninja-build.Ninja` in PowerShell, or the release ZIP from the [Ninja GitHub releases](https://github.com/ninja-build/ninja/releases) | Make sure `ninja` ends up on your PATH. |
| **Git** | [git-scm.com](https://git-scm.com) | Needed to clone JUCE in lesson 02. |

> **Why Visual Studio and not MinGW?** JUCE's officially supported and tested
> Windows compiler is MSVC (Microsoft's compiler). MinGW builds of JUCE exist
> but fight you on details. As a beginner, stay on the supported path: MSVC.

### Install walkthrough

1. **Install Visual Studio 2022 Community.** Run the installer → on the
   Workloads tab check **"Desktop development with C++"** → Install. This
   takes a while (several GB). The two components you specifically need inside
   that workload are **MSVC v143 build tools** and the **Windows 10/11 SDK** —
   both are included by default, but if you customized the install, confirm
   they're there.
2. **Install CMake.** Run the installer, and on the options page choose **"Add
   CMake to the system PATH for all users"** (or "current user" — either is
   fine, just don't choose "Do not add").
3. **Install Ninja.** In PowerShell:
   ```powershell
   winget install Ninja-build.Ninja
   ```
   Then **close and reopen your terminal** so the new PATH takes effect.
4. **Install Git for Windows** with default options.

### The two terminals you need to know about

Windows gives you (at least) two command lines, and for this project it
matters which one you use:

- **PowerShell** — the modern default. This series shows PowerShell commands
  first.
- **cmd** — the old one. Where commands differ, both are shown.

There is also a special one: **"x64 Native Tools Command Prompt for VS 2022"**
(find it in the Start Menu under Visual Studio 2022). This is a `cmd` window
with all the compiler environment variables pre-set (`INCLUDE`, `LIB`,
`PATH` to `cl.exe`, …). **You only need it for the Ninja build path** (lesson
12 explains why). For the primary Visual Studio-generator path, plain
PowerShell is fine.

> **Why is vcvars awkward under PowerShell?** The compiler environment is set
> up by a batch script (`vcvars64.bat`). Batch scripts can't export
> environment variables into a PowerShell process — PowerShell can't "source"
> a `.bat` file the way `cmd` can. That's why the Ninja route (which needs
> the compiler on PATH) wants the Native Tools prompt, while the VS-generator
> route (where CMake talks to MSBuild, which finds the compiler itself)
> works from plain PowerShell. So: **in PowerShell, prefer the Visual Studio
> generator.**

### Verify (Windows)

In PowerShell:

```powershell
cmake --version   # 3.22+
ninja --version   # e.g. 1.11.x or 1.12.x
git --version
```

Then check the compiler. Open **x64 Native Tools Command Prompt for VS 2022**
from the Start Menu and run:

```cmd
cl
```

You should see something like `Microsoft (R) C/C++ Optimizing Compiler
Version 19.xx.xxxxx for x64`. (Bare `cl` with no file prints its version and
then complains about no input files — that's the success signal.)

If `cl` is "not recognized": you opened a regular terminal instead of the
Native Tools prompt, or the "Desktop development with C++" workload didn't
install. Re-check the workload in the Visual Studio Installer (Modify button).

---

## Check your understanding

- **Kali:** you installed a compiler (`build-essential`), `cmake`,
  `ninja-build`, `git`, plus `-dev` libraries for ALSA, JACK, X11, OpenGL,
  FreeType, cURL, and WebKit. You verified with `--version` checks and `ls`
  on `XInput2.h` and `asoundlib.h`.
- **Kali:** you do development as a **non-root user**; JUCE needs X11
  (XWayland is usually automatic); audio goes through ALSA (PipeWire's
  compat layer normally handles it).
- **Windows:** you installed VS 2022 Community with the **"Desktop
  development with C++"** workload, CMake (on PATH), Ninja, and Git. You
  verified `cmake`, `ninja`, `git`, and `cl` (in a Native Tools prompt).
- **Windows:** PowerShell is your main terminal; the **x64 Native Tools
  Command Prompt** is only needed for the Ninja build route.
- Next: lesson 02 clones the JUCE framework itself.
