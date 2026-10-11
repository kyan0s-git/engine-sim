# Dependencies

Engine Simulator used to pull its dependencies in as Git submodules. Together
those submodules weighed more than 800 MB (most of it prebuilt Vulkan and
DirectX libraries and demo assets), and they only built on Windows. They are
now vendored here, trimmed to what the simulator actually uses, and adapted so
that the project builds with MSVC, GCC and Clang on Windows, Linux and macOS.

| Directory | Upstream | Base revision | License |
| --- | --- | --- | --- |
| `delta-studio/` | [ange-yaghi/delta-studio](https://github.com/ange-yaghi/delta-studio) | `b7d0a046` | MIT |
| `piranha/` | [ange-yaghi/piranha](https://github.com/ange-yaghi/piranha) | `ff2955da` | MIT |
| `simple-2d-constraint-solver/` | [ange-yaghi/simple-2d-constraint-solver](https://github.com/ange-yaghi/simple-2d-constraint-solver) | `3083ff9` | MIT |
| `csv-io/` | [ange-yaghi/csv-io](https://github.com/ange-yaghi/csv-io) | `2112c55` | MIT |
| `direct-to-video/` | [ange-yaghi/direct-to-video](https://github.com/ange-yaghi/direct-to-video) | `19f939c` | Same author as the rest of the project |
| `discord/` | [discord/discord-rpc](https://github.com/discord/discord-rpc) (prebuilt) | - | MIT |

## Local modifications

### delta-studio

* Removed the Direct3D 10/11, Vulkan, DirectSound and Win32 window/input
  backends along with the demos, tests, documentation and ~500 MB of prebuilt
  libraries.
* Added an SDL2 platform layer: window system (`yds_sdl_window*`), input
  (`yds_sdl_input_system`), monitor enumeration, an SDL audio backend
  (`yds_sdl_audio`) that emulates DirectSound-style ring buffers, and an SDL
  OpenGL context (`yds_opengl_sdl_context`).
* The OpenGL device rewrites the bundled GLSL 4.20 shaders to GLSL 4.10 at
  load time and applies the `layout(binding = N)` bindings through the API,
  so they also run on macOS's OpenGL 4.1 core profile.
* Added a portable WAV reader (`yds_audio_wave_file`) to replace the Win32
  multimedia API version.
* Portability shims (`yds_platform.h`, `yds_simd.h`) for MSVC-specific
  functions and a scalar fallback for SSE intrinsics on ARM (Apple Silicon).
* Replaced Boost.Filesystem with `std::filesystem` and the Win32 timer with
  `std::chrono`.
* Bug fixes: logger default constructor never initialized the object,
  `ysAudioDevice::DestroyAudioSource()` removed the source from the wrong
  list, registries with a zero initial capacity never grew, the window
  system singleton was never cleared, and two shader statements were
  invalid GLSL.

### piranha

* The Flex/Bison generated scanner and parser are checked in under
  `generated/` so that building does not require these tools (configure with
  `-DPIRANHA_REGENERATE_PARSER=ON` after editing `flex-bison/`).
* Replaced in-class explicit template specializations (an MSVC extension)
  with `if constexpr` so the library compiles with GCC.
* Removed the demos, tests and documentation.

### simple-2d-constraint-solver, csv-io, direct-to-video

* Build files reduced to the libraries themselves (no tests or demo
  executables, and no GoogleTest download).
* `scs_force_inline` now uses the GCC/Clang attribute on non-MSVC compilers.
