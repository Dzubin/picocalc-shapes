# Changelog

All notable changes to PicoCalc Shapes.
Format loosely follows [Keep a Changelog](https://keepachangelog.com/);
the version here matches the `VERSION` define in `shapes_const.h`.

## [Unreleased]

## [V0.2] - 2026-10-15

- Vendored starter: `fat32.c` `fat32_write()` no longer leaks a cluster each time a write starts on a
  cluster boundary. The program itself is unchanged; not rebuilt.

- Vendored starter: `fat32.c` skips deleted directory entries before long-name parts (an
  out-of-bounds stack write) and `sdcard.c` waits for a busy card by the clock and before
  every command (failed reads and writes on a card that was still programming). The
  program itself is unchanged; not rebuilt.

### Changed
- All constants and data tables moved into `shapes_const.h`.
- The saturation and value bars are rebuilt only when their hue, wheel or other setting changes.
- In a hue mode the hue arrow is on the strip that has the outline.

### Added
- A help screen with the author, the version and the keys, shown when the program starts and
  when `?` is pressed; any key continues.
- `UP` and `DOWN` walk one vertical chain: top-row shape, the shape below it, the plain hue
  strip, the improved hue strip, the saturation bar, the value bar (and back up). `F1`, `F2`
  and `F3` still pick the pickers directly. Growing and shrinking a shape moved to `+` and `-`
 .
- The pickers are stacked hue (the two rainbow strips) on top, saturation in the middle and
  value at the bottom, with the RGB line under them. The box around the selected shape is
  white now, like the outline of the picker being edited.
- The bar the current mode edits (the plain or improved rainbow, the saturation bar or the value
  bar) gets a white outline. The arrows on the rainbow strip the shape is coloured from, the saturation bar
  and the value bar are always shown for the selected shape.
- Value (brightness): a bar above the saturation bar, black at the left end and the full colour
  at the right, chosen with `F3` and LEFT / RIGHT (0 to 100 percent in steps of 2, per shape).
  To fit everything the shapes are smaller, at most 49 pixels instead of 59.
- Saturation: a bar above the rainbow strips, white at the left end and the full colour at the
  right, chosen with `F2` and LEFT / RIGHT (0 to 100 percent in steps of 2, per shape). The
  strips moved up and the RGB565 hex line now sits under them.
- A line between the shapes and the rainbow strips shows the selected shape's colour as its
  RGB565 fields in hex (R and B 00 to 1F, G 00 to 3F).
- The two rows of shapes moved up to just below the top of the screen, leaving room above
  the highest point a rotated square can reach.
- A second, improved rainbow strip (each channel ramp remapped with sqrt(v * (510 - v)),
  so the in-between colours are not dimmer than the primaries) stacked flush under the
  first, which moved up. `F1` now steps shape-select, colour-select on the plain strip,
  colour-select on the improved strip, and back; each shape remembers which strip it was
  coloured from.
- Build outputs are named without the `picocalc-` prefix, since the chip or system
  at the end of the name already says what they are for: `shapes-RP2040.uf2`, `shapes-RP2350.uf2`, `shapes-Windows.exe` and `shapes-Linux`.
- Leaving: `ESC` or `Q` now leaves for the PicoCalc UF2 Loader menu (it asks the
  loader for its menu through the watchdog scratch registers, constants in the
  new `loader_const.h`; with no loader installed the program just restarts, and
  on a PC it closes). `~` still goes to BOOTSEL.
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
  self-contained `shapes-Windows.exe`. Build with
  `cmake -S desktop -B build-windows -G Ninja` then `ninja -C build-windows`
  from an MSYS2 UCRT64 shell.
- Linux build: the `desktop/` build (formerly `windows/`) now also builds on
  Linux and produces `shapes-Linux`. SDL2 is linked dynamically there and the
  build copies `libSDL2-2.0.so.0` next to the executable (found through an
  `$ORIGIN` run-path); needs the SDL2 development package (e.g. `libsdl2-dev`).
- The firmware build now names every output with the chip (the `.uf2`, `.elf`, `.bin` and `.hex` all end in `-RP2040` / `-RP2350`) and copies the `.uf2` to the top-level folder as
  `shapes-RP2040.uf2` / `shapes-RP2350.uf2`, so the two
  chips' builds sit side by side.

## [V0.1] - 2026-09-12

### Removed
- The speech-synthesis demo (`picocalc_speech.c/.h` and the V/H/J/K/S keys),
  split off into its own program.
- The sound-effects demo and the underlying audio engine
  (`picocalc_sound_effects.c/.h`, `picocalc_audio.c/.h`, the E/G/W/Z/X keys,
  and `pcaudio_init()`), moved out along with the speech demo. This
  project no longer produces any sound.
