# Changelog

All notable changes to this project are documented in this file. Version
0.2.0 is the first release of this fork; earlier versions were released by
Ange Yaghi in the [original repository](https://github.com/ange-yaghi/engine-sim).

## [0.2.0] - Unreleased

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

### New features
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
- The unused SDL2_image dependency, the Win32-only backends and demos of the
  vendored libraries, and an outdated duplicate of the part library
  (`assets/part-library`).
