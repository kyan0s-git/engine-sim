# Engine Simulator
![Engine Simulator screenshot](docs/public/screenshots/screenshot_v03.png?raw=true)

A real-time internal combustion engine simulator that produces engine audio and simulates how an engine responds, now with an **engineering physics model** for power curves, efficiency and fuel consumption.

This is a maintained open source continuation of Engine Simulator. It runs natively on **Windows, Linux and macOS**, adds rotary engines and drivetrain features from the later releases, incorporates the script library improvements of the [Community Edition](https://github.com/Engine-Simulator/engine-sim-community-edition), and fixes a long list of bugs. See the [changelog](CHANGELOG.md) for details.

## Two physics models

By default engines use the original **legacy** model, which the bundled engines are tuned for: it sounds right but its numbers rely on compensating approximations, so don't use them for engineering decisions.

Engines can select the **engineering** model (`physics_model: "engineering"`) instead. It uses real air and fuel chemistry, temperature-dependent gas properties, a quasi-dimensional flame, Woschni heat transfer and a component friction model. The `engine-sim-dyno` tool measures torque, power, IMEP/BMEP/FMEP, efficiencies, BSFC, volumetric efficiency and burn angles over a speed sweep. See [docs/engineering.md](docs/engineering.md); the research behind it is in [research/](research/).

## Download

Installers are attached to each [release](https://github.com/kyan0s-git/engine-sim/releases):

| Platform | Installer | Installs to |
| :-- | :-- | :-- |
| Windows 10/11 (x64) | `engine-sim-<version>-windows-x64-setup.exe` | `%LOCALAPPDATA%\Programs\Engine Simulator` with Start menu (and optional desktop) shortcuts and an uninstaller. No administrator rights needed. |
| macOS 11+ (Apple Silicon and Intel) | `engine-sim-<version>-macos-universal.dmg` | Drag *Engine Simulator* to *Applications* |
| Linux (x86-64), any distribution | `engine-sim-<version>-linux-x86_64.AppImage` | Nothing to install: make it executable (`chmod +x`) and run it |
| Debian, Ubuntu and derivatives | `engine-sim_<version>_amd64.deb` | `/opt/engine-sim`, with an application menu entry and the `engine-sim` and `engine-sim-dyno` commands |

Portable archives (`-windows-x64.zip`, `-linux-x64.tar.gz`, `-macos-universal.zip`) that run without installing are attached as well; the in-app updater uses them.

A GPU driver supporting OpenGL 4.1 or newer is required. The installers aren't signed by a paid certificate: on Windows, SmartScreen may ask for confirmation (*More info* > *Run anyway*); on macOS, right-click the app and choose *Open* the first time. The Linux packages are built on Ubuntu 22.04 and work on distributions with glibc 2.35 or newer, on both X11 and Wayland (the AppImage needs FUSE, which most desktop distributions include).

## User interface

![Engine designer](docs/public/screenshots/designer_v03.png?raw=true)

- **Controls** (left): ignition, starter, throttle, gears, clutch, brake, dynamometer and slow motion. The keyboard shortcuts below work as well.
- **Instruments** (right): tachometer, speedometer, manifold pressure and air flow, and the engineering outputs of the last complete cycle (torque, power, mean effective pressures, efficiencies, BSFC, burn angles, peak pressure and temperature).
- **Analysis** (bottom): cylinder pressure and valve lift against crank angle, the p-V diagram, intake and exhaust flows, the dyno curve recorded while the dynamometer runs, speed/torque/manifold pressure history, the audio output and simulation performance.
- **Engine designer** (*File* > *Engine designer*): build an engine from its parameters (inline, V or flat layout, cylinder count, bore, stroke, rod length, compression ratio, cams, intake and exhaust, ignition curve, fuel and mixture) and run it. Designs are saved as ordinary engine scripts in the user data folder (`assets/engines/custom`) and can be reopened in the designer or edited by hand.
- **Tuning** (*Simulation* > *Tuning*): change the ignition timing, rev limiter, mixture, combustion efficiency, wall temperature, heat transfer, friction, sound and simulation frequency of the running engine.
- **Settings** (*File* > *Settings*, Ctrl+,): fullscreen, vertical sync, interface scale and theme, units, master volume, muting in the background, simulation frequency and update checks.
- **Console** (F5): script errors and messages.

Panels can be resized by dragging their inner edges, and Tab cycles between the full layout, the engine with its instruments, and the engine alone.

## Controls

| Key/Input | Action |
| :---: | :--- |
| A | Toggle ignition |
| S | Hold for starter |
| Q, W, E, R | Throttle presets |
| Space + Scroll | Fine throttle adjustment |
| Up / Down Arrow | Shift up / down |
| Shift | Hold to depress the clutch (hold Space as well to engage/disengage slowly) |
| T / U | Gradually release / engage the clutch (Y holds the current position) |
| . | Hold to apply the brakes |
| D | Toggle the dyno |
| H | Toggle RPM hold (see below) |
| G + Scroll | Change the hold speed |
| Z + Scroll | Volume |
| X + Scroll | Convolution level |
| C + Scroll | High frequency gain |
| V + Scroll | Low frequency noise |
| B + Scroll | High frequency noise |
| N + Scroll | Simulation frequency |
| M / , | Show more / fewer layers of the engine |
| F1 / F2 / F3 | Rotate the view / reset the rotation |
| 1 - 5 | Slow motion (1/10 to 1/1000 speed) |
| P | Pause; Right Arrow then advances one frame |
| Page Up / Page Down | Load the previous / next bundled engine |
| [ / ] | Previous / next theme |
| Enter | Reload the current scripts |
| F5 | Show the console (loaded scripts, script errors, version and update status) |
| F8 | Install an available update / restart into it |
| Tab | Cycle the panel layout |
| Ctrl + , | Settings |
| F | Toggle fullscreen |
| Escape | Exit |

The last engine and theme you selected are remembered between sessions.

### Using the RPM hold
The RPM hold holds the engine at a specific speed and measures its power and torque there. Press `H` to enable the hold, then **enable the dyno** with `D` for the hold to take effect. Hold `G` and scroll (or use the slider in the controls panel) to change the hold speed.

## Command line

```
engine-sim-app [options] [engine.mr]

  --engine <file>     Engine script to load (same as passing it positionally)
  --theme <file>      Theme script to load
  --data <dir>        Directory containing the assets/ and es/ folders
  --fullscreen        Start in fullscreen mode
  --windowed          Start in windowed mode
  --no-update-check   Don't check for new releases
  --version           Print the version and exit
```

The data directory is found automatically next to the executable (or inside the macOS app bundle). It can also be set with the `ENGINE_SIM_DATA_ROOT` environment variable.

## Updates

At startup the application asks GitHub whether a newer release exists (it sends no other information). When one does, the menu bar shows it; click it or press **F8** to download and install it, and again to restart into the new version. Installations from the Windows installer, the macOS disk image, the AppImage and the portable archives update themselves; the Debian package, development builds and read-only folders open the release page instead. The startup check is turned off in *Settings* > *Updates*, with `--no-update-check` or with the environment variable `ENGINE_SIM_NO_UPDATE_CHECK=1` (which also disables manual checks). Updating needs `curl`, which comes with Windows 10 and newer, macOS and most Linux distributions.

## Files and folders

| Location | Contents |
| :-- | :-- |
| `assets/main.mr` | Default script, loaded when no engine has been selected yet |
| `assets/engines/` | Bundled engines (piston and rotary) |
| `assets/themes/` | Color and unit themes |
| `es/` | The scripting library (`engine_sim.mr`) and the part library |
| User data folder | `settings.ini` (settings page), `session.ini` (last engine and theme), `layout.ini` (window positions), `error_log.log`, engines from the designer (`assets/engines/custom/`) and downloaded updates (`updates/`). On Windows `%APPDATA%\EngineSimulator\engine-sim`, on Linux `~/.local/share/EngineSimulator/engine-sim`, on macOS `~/Library/Application Support/EngineSimulator/engine-sim` |

Scripts placed in `assets/engines` or `assets/themes` inside the user data folder also show up when cycling engines and themes, and `import` statements search that `assets` folder too.

## Making your own engines

Engines, vehicles, transmissions and themes are described in a small scripting language. Start with [docs/scripting.md](docs/scripting.md), which covers the structure of an engine script, units, the most important parameters, rotary engines and themes. The bundled engines in `assets/engines` are good starting points.

An engine script ends by selecting what to simulate:

```
public node main {
    run(
        engine: my_engine(),
        transmission: my_transmission(),   // optional
        vehicle: my_vehicle()              // optional
    )
}

main()
```

Scripts can be loaded with `--engine`, by changing `assets/main.mr`, or by pressing Page Up / Page Down in the application. If a script has errors, press F5 to see them.

## Building from source

The project uses CMake (3.16+) and a C++17 compiler. All third-party code is included in `dependencies/` (see [dependencies/README.md](dependencies/README.md)); only SDL2 is needed from the system, or it can be downloaded and linked statically with `-DENGINE_SIM_BUNDLED_SDL=ON`.

### Linux

```
sudo apt install build-essential cmake ninja-build libsdl2-dev libgl-dev   # Debian/Ubuntu
sudo dnf install gcc-c++ cmake ninja-build SDL2-devel mesa-libGL-devel     # Fedora

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/bin/engine-sim-app
```

### macOS

```
brew install cmake ninja sdl2
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
open "build/bin/Engine Simulator.app"
```

### Windows

Install Visual Studio 2022 (with the C++ workload) and CMake, then from a developer command prompt:

```
cmake -S . -B build -DENGINE_SIM_BUNDLED_SDL=ON
cmake --build build --config Release
build\bin\engine-sim-app.exe
```

### Build options

| Option | Default | Description |
| :-- | :-- | :-- |
| `ENGINE_SIM_BUILD_APP` | `ON` | Build the graphical application (turn off for headless tools only) |
| `ENGINE_SIM_BUILD_TESTS` | `ON` | Build the unit tests (uses the system GoogleTest or downloads it) |
| `ENGINE_SIM_BUNDLED_SDL` | `OFF` | Download SDL2 and link it statically |
| `PIRANHA_ENABLED` | `ON` | Scripting support |
| `PIRANHA_REGENERATE_PARSER` | `OFF` | Regenerate the script parser with Flex and Bison |
| `DISCORD_ENABLED` | `OFF` | Discord rich presence (Windows only) |
| `DTV` | `OFF` | Video capture with the Insert key (requires FFmpeg) |

`cmake --install build --prefix <dir>` produces the same layout as the release packages.

### Tests and script validation

```
ctest --test-dir build --output-on-failure
./build/engine-sim-script-check --require-running assets/engines/rotary/two_rotor_13b.mr
```

`engine-sim-script-check` compiles scripts and runs each engine headlessly (starter, then throttle) without opening a window. It is used by CI to make sure every bundled engine still loads and runs. Use `--trace` to print the state of the first cylinder every frame while developing an engine, and `--crank <seconds>` to crank for longer.

## Releases

Pushing a tag such as `v0.3.0` runs the [Release workflow](.github/workflows/release.yml), which builds the installers and portable packages for Windows, Linux and macOS (the scripts are in [packaging/](packaging/)) and publishes a GitHub release using the matching section of `CHANGELOG.md` as release notes. Tags containing a hyphen (e.g. `v0.3.0-beta1`) are published as pre-releases. To cut a release:

1. Update `VERSION` in the `project()` call of `CMakeLists.txt` and add a section to `CHANGELOG.md`
2. Commit, then `git tag v<version>` and `git push origin v<version>`. Alternatively run the Release workflow from the Actions tab with the tag (e.g. `v0.3.0`) as its input; it creates the tag on the commit it ran on.

Every push is also built and tested on all three platforms by the [Build workflow](.github/workflows/build.yml).

## Credits

Engine Simulator was originally created by Ange Yaghi and released under the MIT license; this project continues that open source release. The original project was made possible by its Patreon supporters. The script library additions from the Community Edition are the work of its contributors. Third party components and their licenses are listed in [dependencies/README.md](dependencies/README.md); the interface font is Roboto (Apache License 2.0, see `assets/fonts`).
