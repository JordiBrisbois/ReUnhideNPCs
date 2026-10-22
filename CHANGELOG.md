# Changelog

Releases are also published on the
[releases page](https://github.com/JordiBrisbois/ReUnhideNPCs/releases), which
GitHub generates from the commits since the previous tag. This file says what
changed for people using the add-on.

## v2026.10.22.1640 - 2026-10-22

### Performance

- The 50 ms character pass no longer asks the kernel whether every pointer it
  resolves is readable. In a crowded WvW map that was ~1400 `VirtualQuery`
  calls per pass, 4-5 ms of CPU taken from the thread that renders frames, and
  it showed up as a frame dropping below its deadline every third frame — the
  58-59 FPS oscillation. The pass measures a few microseconds now.
- Client memory is read directly and recovered from if the page has been
  released, so the add-on is not trading safety for that speed: a page the
  client unmaps while the add-on is looking at it can no longer be
  dereferenced.
- The mini/pet fallback, which walks every avatar in the zone, runs on every
  pass again instead of one pass in four. Cosmetic agents no longer react 200 ms
  late.

### Added

- `DebugLog` setting (off by default): writes the client pattern scan and the
  memory-read diagnostics to the log. Only needed when reporting a problem.

### Changed

- The default log no longer prints the client pattern names and the offsets
  they resolved to. Those lines are written only with `DebugLog` enabled.

## Earlier versions

See the [releases page](https://github.com/JordiBrisbois/ReUnhideNPCs/releases).
