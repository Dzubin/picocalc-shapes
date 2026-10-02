# Changelog

All notable changes to PicoCalc Shapes.
Format loosely follows [Keep a Changelog](https://keepachangelog.com/);
the version here matches the `VERSION` define in `picocalc-shapes.c`.

## [Unreleased]

### Added
- Vendored `drivers/fat32.c`: `get_next_free_cluster()` now advances its search
  hint past the cluster it hands out (a local fix, not in upstream), so a long
  sequential write to the SD card no longer re-scans every cluster already given
  out and looks hung.
- Desktop build: the build logic that was repeated in `desktop/CMakeLists.txt` now
  lives in `desktop/shim_desktop.cmake` (that file now only names the program and
  its sources), and the shim files were refreshed from a shared copy. The numeric
  keypad now works in the desktop build.
- Windows build: `desktop/` holds a small SDL2 shim that re-implements the
  picocalc-text-starter driver API (LCD, keyboard, audio) so the same
  `picocalc-shapes.c` runs on a PC; SDL2 is linked statically, giving one
  self-contained `picocalc-shapes-Windows.exe`. Build with
  `cmake -S desktop -B build-windows -G Ninja` then `ninja -C build-windows`
  from an MSYS2 UCRT64 shell.
- Linux build: the `desktop/` build (formerly `windows/`) now also builds on
  Linux and produces `picocalc-shapes-Linux`. SDL2 is linked dynamically there and the
  build copies `libSDL2-2.0.so.0` next to the executable (found through an
  `$ORIGIN` run-path); needs the SDL2 development package (e.g. `libsdl2-dev`).
- The firmware build now names every output with the chip (the `.uf2`, `.elf`, `.bin` and `.hex` all end in `-RP2040` / `-RP2350`) and copies the `.uf2` to the top-level folder as
  `picocalc-shapes-RP2040.uf2` / `picocalc-shapes-RP2350.uf2`, so the two
  chips' builds sit side by side.

## [V0.1] - 2026-09-12

### Removed
- The speech-synthesis demo (`picocalc_speech.c/.h` and the V/H/J/K/S keys)
  — split off into the new PicoCalc-Speech project.
- The sound-effects demo and the underlying audio engine
  (`picocalc_sound_effects.c/.h`, `picocalc_audio.c/.h`, the E/G/W/Z/X keys,
  and `pcaudio_init()`) — moved to PicoCalc-Speech, which is now the only
  place this engine is exercised. This project no longer produces any
  sound.
