# Changelog

Releases are also published on the
[releases page](https://github.com/JordiBrisbois/ReUnhideNPCs/releases), which
GitHub generates from the commits since the previous tag. This file says what
changed for people using the add-on.

## v2026.10.23.1926 - 2026-10-23

### Added

- **Disable in Competitive** (enabled by default): the add-on does nothing in
  World vs World and PvP. Characters keep the game's own visibility there, so
  the add-on cannot be read as an advantage in modes where other players are
  affected. Untick it under `Misc` to use the add-on there anyway.

### Changed

- The options window now says why it is doing nothing: a competitive map and
  the `Instance Behaviour` setting both show a line above the settings, and the
  log records when the add-on pauses and resumes.

## v2026.10.22.1820 - 2026-10-22

### Added

- **Stats panel**: the options window reports what the character pass costs
  (microseconds, model count, FPS) and which rules hid how many characters on
  the current pass, so a profile can be read instead of guessed at.
- **Search and sections**: the options window is split into `Show`, `Models`,
  `Hide`, `Limits` and `Misc`, with a search box that filters the settings and
  opens the section that contains them.
- **Basic Beyond**: characters past a distance switch to basic models while
  staying visible, so a distant crowd costs less without disappearing.
- **Rank First**: with `Max NPCs` active, keep higher-ranked NPCs — bosses and
  champions before ordinary ones.
- **Always Show Attackable**: anything you can attack stays visible whatever
  the other hide rules say.

### Performance

- The character pass used to cost 4-5 ms of CPU every update in a crowded WvW
  map, which is enough to drop a frame; that showed up as a 58-59 FPS wobble.
  It measures a few microseconds now.

### Changed

- If a game update ever breaks the add-on's assumptions, it disables itself and
  says so in the log and in the options window, instead of hiding the wrong
  characters.
- The default log is quieter: detailed diagnostics only appear when `DebugLog`
  is enabled.

## v2026.10.22.1640 - 2026-10-22

### Performance

- Off-screen hiding, character limits and the "hide" rules no longer cost a
  measurable amount of frame time in a crowded WvW map.
- Characters the game has already released can no longer be read by mistake,
  which is what made the page cache of the previous attempt crash on a map
  change. The add-on kept the safety and dropped the cost.
- The mini/pet fallback, which walks every avatar in the zone, runs on every
  pass again instead of one pass in four. Cosmetic agents no longer react
  200 ms late.

### Added

- `DebugLog` setting (off by default): writes detailed diagnostics to the log.
  Only needed when reporting a problem.

### Changed

- The default log no longer prints internal diagnostics.

## Earlier versions

See the [releases page](https://github.com/JordiBrisbois/ReUnhideNPCs/releases).
