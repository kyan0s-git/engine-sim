# Changelog

All notable changes to this project are documented in this file. Version
0.2.0 is the first release of this fork; earlier versions were released by
Ange Yaghi in the [original repository](https://github.com/ange-yaghi/engine-sim).

## [0.4.0] - 2026-10-15

### Turbochargers and superchargers
- Single, twin and sequential turbochargers and Roots, twin-screw and
  centrifugal superchargers. The compressor fills a charge-air volume that
  the throttle draws from; turbines are driven by the exhaust and spool up
  (turbo lag comes from the exhaust energy available); superchargers load
  the crankshaft with their drive power.
- Wastegate, blow-off valve, supercharger bypass valve and intercooler.
- Compressor whine and blow-off noise in the sound; turbines muffle the
  exhaust a little.
- Boost gauge, turbo speed, wastegate and charge air temperatures in the
  instruments; boost, drive ratio, intercooler and blow-off valve in the
  tuning window.
- Script nodes `turbocharger()`, `supercharger()` and
  `add_forced_induction()`; the dyno tool prints the boost, turbo speed and
  exhaust pressure at every point.

### Engine designer and library
- New layouts: W engines (two narrow-angle V's or three banks) and rotary
  (Wankel) engines with one to four rotors and port timing.
- Firing orders can be entered; the valvetrain type, the exhaust sound and
  forced induction are part of a design.
- Engine library (Ctrl+L): presets of the Honda K24, GM LS3, Toyota
  2JZ-GTE, BMW B58, the 2026 Formula 1 V6, Chevrolet small-block 350,
  Chrysler 426 Hemi, Mazda 13B-REW, Porsche M97.74 and a Volkswagen W12,
  to load as they are or customize; saved engines can be loaded, edited,
  duplicated and deleted. *Save as* in the designer.
- The script checker can validate the presets (`--presets`), which CI now
  does.

### Performance
- The simulation runs on its own thread, so the frame rate no longer drops
  with the cost of the physics. On a slow test machine the frame rate went
  from 8 to 18-22 fps and audio dropouts from 11 per run to none.
- The simulation speed no longer jitters from frame to frame (the audio
  queue was corrected by half of its error every frame).
- When the physics can't keep up the simulation catches up in short slices
  instead of freezing the interface for up to 200 ms.
- The Performance tab breaks the frame time down (interface, waiting for
  the simulation, engine view, presenting); `ENGINE_SIM_PROFILE=1` logs it.

### Engine view
- Realistic style (default): machined aluminium pistons, steel rods and
  crank with turned faces, polished pins, a honed cylinder bore, heat-tinted
  exhaust valves and an iron block, shaded per pixel from the flat 2D
  parts. The classic flat theme colors remain available (Settings > Display
  or View > Realistic engine view).
- The front rotor of a rotary engine was drawn behind the rear one.

### Fixes
- Ctrl+, (Cmd+, on macOS) opens the settings wherever the focus is; it only
  worked while an interface window had focus.
- Shader programs were destroyed after their shaders at exit (use after
  free).

## [0.3.1] - 2026-10-14

### Audio
- Fixed crackling. The synthesizer rendered at most once per frame and kept
  only 45 ms of audio ready, so any frame slower than about 22 fps ran the
  output dry; it now renders continuously as input arrives.
- When the output did run dry, the sound card replayed the last second of
  audio from its loop buffer. It now plays silence instead, which also
  removes the loop of old sound while paused, loading an engine or dragging
  the window.
- The simulation follows the measured frame time and keeps a steady amount
  of audio queued (it assumed at least 30 frames per second), and the
  output buffer grows with the frame time on slow computers.
- The level limiter no longer overshoots: about 3% of samples were hard
  clipped at full throttle. Peaks are now held below full scale.
- Fixed a race between the simulation and the audio thread that could drop
  a block of audio.
- Without an audio device the simulation keeps running in real time.

### Interface
- The rev limiter is shown in the engine view and the combustion readout,
  together with the share of cycles that fire. The instrument readouts are
  averaged over half a second instead of jumping between the firing and
  cut cycles.
- Fixed the Dear ImGui "conflicting ID" warning on the analysis panel (the
  cylinder selector and the cylinder plot shared an identifier) and made
  menu entries unique when user and bundled engines share a folder name.

## [0.3.0] - 2026-10-14

### Installers
- Windows setup program (per-user install, Start menu and desktop
  shortcuts, uninstaller), macOS disk image, Linux AppImage and Debian
  package, alongside the portable archives.
- Application icon on all platforms.
- The AppImage updates itself; the Windows installer's entry in the list of
  installed programs follows in-app updates.
- `engine-sim-dyno` finds the installation's data without `--data`.

### New user interface
- Rebuilt with Dear ImGui and ImPlot: menu bar, controls panel,
  instruments panel with the engineering outputs of the last cycle, and an
  analysis panel with cylinder pressure and valve lift against crank angle,
  p-V diagram, flows, dyno curve, history, audio and performance plots.
  Panels can be resized and Tab cycles the layouts.
- Settings page (File > Settings, Ctrl+,) stored in `settings.ini`:
  fullscreen, vertical sync, interface scale and theme (dark, light or the
  engine theme's colors), units, master volume, muting in the background,
  simulation frequency and update checks.
- Engine designer: describe an engine by its parameters (layout, cylinder
  count, bore, stroke, rod length, compression ratio, cams, intake and
  exhaust, ignition curve, fuel, mixture, vehicle mass) and run it. Designs
  are saved as engine scripts and can be reopened in the designer.
- Tuning window to change the timing, rev limiter, mixture, combustion
  efficiency, wall temperature, heat transfer, friction, sound and
  simulation frequency of the running engine.
- Console window for script errors and messages; menus to pick engines and
  themes.

### Fixes
- Gauge needles swung around with the engine stopped: the needle spring was
  integrated explicitly and became unstable at low or uneven frame rates.
  Needles and smoothed values now use an exact, frame-rate independent
  response.
- A frame without simulation steps divided the intake flow rate by zero,
  sending the flow gauges to infinity.
- Status messages are no longer repeated every frame while an audio or
  simulation key is held.

## [0.2.0] - 2026-10-13

### Platforms and packaging
- Native Windows, Linux and macOS (Apple Silicon and Intel) versions. The
  Win32/Direct3D/DirectSound code paths were replaced by an SDL2 + OpenGL 4.1
  backend that is shared by all platforms.
- The ~800 MB of Windows-only Git submodules were replaced with trimmed,
  vendored copies of the dependencies; Boost, Flex and Bison are no longer
  needed to build.
- GitHub Actions build and test every push on Linux (GCC and Clang), macOS
  and Windows, validate every bundled engine script and smoke test the
  application.
- Pushing a version tag builds self-contained packages (Windows zip, Linux
  tar.gz, macOS universal app bundle) and publishes a GitHub release.
- Command line options (`--engine`, `--theme`, `--data`, `--fullscreen`,
  `--windowed`, `--version`); the data folder is found automatically and
  per-user files are stored in the platform's application data folder.

### Engineering physics model
- New `physics_model: "engineering"` engine setting (the original model
  remains the default): real air, eight species with NASA polynomial
  properties, enthalpy-carrying flows, metered lambda, a fuel library
  (gasoline, iso-octane, n-heptane, ethanol, methanol, E10, E85, methane,
  propane, hydrogen) with evaporative charge cooling, heat release from
  formation enthalpies with water-gas shift for rich mixtures, a
  quasi-dimensional entrainment flame with the laminar flame speed updated
  every step, Woschni heat transfer and Patton-Nitschke-Heywood or
  Chen-Flynn friction. See `docs/engineering.md`.
- Per-cycle engine metrics: gross/net IMEP, PMEP, BMEP, FMEP, mechanical,
  indicated and brake efficiency, ISFC/BSFC, volumetric efficiency, AFR and
  lambda, spark advance, CA10/50/90, peak pressure and temperature, wall
  heat loss.
- `engine-sim-dyno`: headless dynamometer sweep that writes the metrics as
  CSV, including SAE J1349 corrected torque and power. Included in the
  release packages.
- Engineering reference engine (`engines/engineering/audi_i5_engineering.mr`).
- Research notes, report and roadmap in `research/`.

### New features
- **In-app updates**: the application checks GitHub for new releases at
  startup; F8 downloads and installs the update and restarts. Disable with
  `--no-update-check`.
- **Rotary (Wankel) engines** with `wankel_engine`, `wankel_rotor`,
  `wankel_peripheral_port` and `add_rotor`, using the same script names as
  the official 0.1.14a release. Includes a two-rotor demo engine.
- **Drivetrain flex**: optional axle/tire compliance on the vehicle and
  clutch damper springs that slip past the clutch's torque limit
  (`simulate_flex`, `stiffness`, `damping`, `max_flex`, ...).
- **Brakes**: hold `.` to brake (`max_brake_force` on the vehicle).
- **Console** (F5) showing the loaded scripts and script errors.
- **Engine and theme switching** in the application with Page Up/Page Down
  and `[`/`]`; the selection is remembered between sessions.
- **Pause** with `P` (Right Arrow steps one frame while paused).
- Simulation quality settings available to scripts
  (`max_sle_solver_steps`, `fluid_simulation_steps`) as well as
  `heat_transfer_coefficient` and `block_temperature`.
- Script library compatibility with the Community Edition: `run()`,
  `k_32inH2O`, `celsius`, `constants.pi`, `engine_sim_theme.mr` for themes,
  and the vehicle inputs of newer versions are accepted.
- New metric theme; themes and engine scripts can be loaded directly.
- `engine-sim-script-check`: a headless tool that compiles scripts and runs
  each engine, used by CI (`--require-running`, `--trace`, `--crank`).

### Performance
- Exhaust impulse responses are convolved with partitioned FFT convolution
  (about 32 times faster for full-length responses).
- The audio synthesizer renders whole blocks and uses a fast per-instance
  random number generator.

### Fixes
- The legacy combustion consumed oxygen without fuel once the cylinder ran
  out of fuel.
- Random cycle variation used the C library's global generator; each
  cylinder now has its own seeded generator.
- Engines started out with their intake and cylinders full of inert gas;
  large engines (2JZ, Audi I5, 9-cylinder radial, Merlin V12 and others)
  failed to start with a normal crank. Every bundled engine now starts.
- Ignition events just before the end of the cycle were skipped when a
  simulation step crossed the wrap-around point.
- Memory leaks every time an engine was loaded (synthesizer buffers,
  filters, functions, camshafts, valvetrains, impulse responses, cylinder
  head and connecting rod arrays).
- Undefined behaviour in the vendored libraries (memory released through
  `void *`, mismatched `new`/`delete[]`, wrong `_aligned_malloc` argument
  order, missing virtual destructors).
- Data races between the simulation and audio threads.
- All exhaust channels shared the first channel's air noise filter.
- `RingBuffer::overwrite` used a member function instead of the start index.
- Gauge ranges were truncated to integers (e.g. 0-1.1 bar).
- Cylinder heat transfer could overshoot the wall temperature.
- Exhaust backflow was discarded instead of mixed into the exhaust system.
- Missing impulse response files crashed the application.
- Scripts that failed to compile or didn't create a vehicle/transmission
  crashed the application; reloading reused freed objects.
- Quick key taps were lost at low frame rates.
- Transmissions used the vehicle's script channel type.
- The Merlin V12's starter couldn't turn the engine over.

### Removed
- The original author's YouTube handle and logo from the info panel (the
  original author is credited in the README, the console and `--version`),
  and the original project's Patreon supporter list.
- The unused SDL2_image dependency, the Win32-only backends and demos of the
  vendored libraries, and an outdated duplicate of the part library
  (`assets/part-library`).
