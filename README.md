# ReUnhideNPCs

Ever been annoyed that enemies, bosses and NPCs disappear when many players are nearby?  
ReUnhideNPCs keeps them visible.

Using this, you could even put `Character Model Limit` to `Lowest` and still see all enemies/bosses/NPCs!

This is a maintained continuation of [UnhideNPCs](https://github.com/server-imp/UnhideNPCs)
by **server-imp**, rebranded and maintained by **Vox**
([JordiBrisbois](https://github.com/JordiBrisbois)). The original project is
GPLv3; this fork remains GPLv3.

## Notes

The add-on is built against the Guild Wars 2 client structures recovered from the
original release. A game update that changes those structures requires updating
`ReUnhideNPCs-RE`.

## In-Game Configuration

This add-on includes an in-game configuration interface.

> **Note:** You can CTRL+Click the sliders if you prefer typing the numbers.

Default overlay hotkey: `ALT+SHIFT+U`.

## Installation

### 1) Nexus (recommended)

1. Drop `ReUnhideNPCs.dll` into `<game folder>/addons/`.
2. Nexus will load it on the next launch.

### 2) ArcDPS

1. Download `ReUnhideNPCs_ArcDps.dll`.
2. Place it in your game folder (e.g. `C:\Program Files\Guild Wars 2`).
3. ArcDPS will load the plugin automatically on the next launch.

### 3) DLL Proxy

1. Download `ReUnhideNPCs.dll`.
2. Rename it to one of the following: `dxgi.dll` / `midimap.dll` / `d3d9.dll`.
   - Note: `midimap.dll` does not appear to work on Linux/Proton.
3. Place it in your game folder.

### 4) Manual injection (developers/testing)

1. Inject `ReUnhideNPCs.dll` using your injector of choice.
2. Press `END` to unload the plugin so it can be reloaded without restarting the game.

## Configuration

Hotkeys are saved at `<game folder>/addons/ReUnhideNPCs/hotkeys.json`.

Settings are saved at `<game folder>/addons/ReUnhideNPCs/settings.json`.

Logs are written to `<game folder>/addons/ReUnhideNPCs/reunhidenpcs.log`.

## Fixes over the original release

- **Frame cost**: the 50 ms character pass no longer issues a `VirtualQuery`
  for every pointer it resolves (~1400 syscalls per pass in a crowded WvW map,
  4-5 ms on the render thread). Client reads are fault-guarded instead, which
  costs one load per pointer and cannot dereference a released page. The pass
  measures a few microseconds with 220 characters. See
  [PERFORMANCE.md](https://github.com/JordiBrisbois/ReUnhideNPCs-RE/blob/main/PERFORMANCE.md).
- **Player-Owned / All** now also hides characters owned by the local player; `Mine` remains available when `All` is disabled.
- **Always Show My Owned** (enabled by default) makes your own pets, clones,
  minis and jade mech bypass every hide option: `All`, `Mine`, `Max
  Player-Owned`, `Non-Friends` and off-screen culling. It is evaluated before
  all other hide rules.
- **Hide off-screen crash**: the world-to-screen routine was called with a
  renderer pointer that could already have been released. ReUnhideNPCs installs
  a detour on the renderer's deleting destructor (vtable slot 0) and clears the
  saved pointer when the renderer is destroyed. `isOnScreen()` treats an
  unknown renderer as "on screen" (keeps characters visible), and a one-second
  freshness guard ignores a saved pointer that has not been refreshed by the
  render hook, so a freed renderer is never dereferenced. The options window
  shows live off-screen decision/confirmation counters next to the checkbox.
- **Always Show Group / Friends** keep party, squad and friend players visible
  even when `Max Players`, `Hide All` or off-screen culling would hide them.
- **Off-Screen Keep** and **Off-Screen Grace** expose the previous hardcoded
  12 m near-distance and 5 s grace period.
- **Nearest First** (enabled by default) makes `Max Players`, `Max
  Player-Owned` and `Max NPCs` keep the closest characters instead of the first
  entries in the game's LOD list. It has no effect when every maximum is
  unlimited, and always-shown entries still count toward the maximum.

## Verifying Hide Off-Screen

The option only applies to characters the game has already loaded. GW2 does not
instantiate most actors outside the camera, so zero hidden is normal in empty
areas.

- Next to `Hide Off-Screen`, `(N off-screen, M confirmed hidden)` shows the
  current pass: `N` = characters classified off-screen and hidden by the rule,
  `M` = those whose model is actually in the hidden LOD state (`dwLodState == 2`).
- Test in a crowded area with targets farther than the configured
  `Off-Screen Keep` distance (default 12 m): turn the camera away. After the
  `Off-Screen Grace` period (default 5 s), `N` and `M` rise; turn back and they
  drop.
- Toggle `Hide Off-Screen` off: the same characters reappear and the counters
  go to zero.
- `Always Show My Owned` entries never count: they bypass the rule.
- If the render hook is unavailable or the renderer is stale, the addon treats
  the character as visible instead of hiding it.

## Releases

Pushing a tag such as `v2026.10.20.1456` triggers the
[release workflow](.github/workflows/build-and-release.yml). It builds both
Nexus/proxy and ArcDPS variants and publishes:

- `ReUnhideNPCs.dll`
- `ReUnhideNPCs.pdb`
- `ReUnhideNPCs_ArcDps.dll`
- `ReUnhideNPCs_ArcDps.pdb`

The tag must match the version format `vYYYY.M.D.HHMM`; the workflow passes it
to CMake so the DLL metadata matches the release tag. A manual
`workflow_dispatch` run builds the current timestamped version and creates a
draft release.

Nexus reads `UpdateLink` from the addon metadata and uses the latest published
release, so keep the `ReUnhideNPCs.dll` asset name unchanged. ArcDPS users take
`ReUnhideNPCs_ArcDps.dll` from the same release.

## Credits

- Original [UnhideNPCs](https://github.com/server-imp/UnhideNPCs) by
  **server-imp**: addon design, in-game UI and GW2 structures.
- [fw](https://github.com/server-imp/fw) framework by **server-imp**.
- ReUnhideNPCs maintenance, GW2 structure recovery and fixes by **Vox**
  ([JordiBrisbois](https://github.com/JordiBrisbois)).
- Both projects are distributed under the GNU GPLv3.

## Building

Requirements: CMake 3.31+, a C++20 compiler, and the `fw` framework checked out
next to this repository:

```
GW2/
  ReUnhideNPCs/
  ReUnhideNPCs-RE/
  fw/
```

```sh
cmake -S ReUnhideNPCs -B ReUnhideNPCs/build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build ReUnhideNPCs/build
```

The output is `ReUnhideNPCs/build/ReUnhideNPCs.dll` and
`ReUnhideNPCs/build/ReUnhideNPCs_ArcDps.dll`.

On GitHub Actions the dependencies are fetched automatically
(`JordiBrisbois/ReUnhideNPCs-RE` and `server-imp/fw`).

## License

GNU General Public License v3.0. See [LICENSE](LICENSE).
