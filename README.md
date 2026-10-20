# ReUnhideNPCs

Ever been annoyed that enemies, bosses and NPCs disappear when many players are nearby?  
ReUnhideNPCs keeps them visible.

Using this, you could even put `Character Model Limit` to `Lowest` and still see all enemies/bosses/NPCs!

This is a maintained continuation of [UnhideNPCs](https://github.com/server-imp/UnhideNPCs)
by **server-imp**, rebranded and maintained by **Vox**. The original project is
GPLv3; this fork remains GPLv3.

## Notes

The previous competitive-mode guard has been removed: the add-on also applies in PvP and WvW.

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

- **Player-Owned / All** now also hides characters owned by the local player; `Mine` remains available when `All` is disabled.
- **Always Show My Owned** (enabled by default) makes your own pets, clones and
  minis bypass `Max Player-Owned`, so a low limit cannot hide them. Explicit
  `Mine` / `All` / `Non-Friends` options still take precedence.
- **Hide off-screen crash**: the world-to-screen routine was called with a
  renderer pointer that could already have been released. ReUnhideNPCs installs
  a detour on the renderer's deleting destructor (vtable slot 0) and clears the
  saved pointer when the renderer is destroyed. `isOnScreen()` treats an
  unknown renderer as "on screen" (keeps characters visible), and a one-second
  freshness guard ignores a saved pointer that has not been refreshed by the
  render hook, so a freed renderer is never dereferenced.

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
