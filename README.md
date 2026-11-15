# Reode

Reode is a mod loader for Geometry Dash (Windows, 64-bit). It is written from
scratch in C++23 and follows the
[Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html).
It is not a fork of Geode and contains no Geode code. Release builds run
Reode mods only; an optional compatibility layer for Geode mods can be built
from source (see [Geode compatibility](#geode-compatibility)).

## Installing

Players run `ReodeSetup.exe` from a release. It finds Geometry Dash in the
Steam library (or asks for the folder), installs or updates Reode, and can
uninstall it, restoring the game exactly as it was. Answers to common
questions, including antivirus warnings, are in [docs/FAQ.md](docs/FAQ.md).

## Keeping progress safe

- **Save backups** (`loader/src/core/safety.cc`). Before anything else runs,
  Reode copies `CCGameManager*.dat` and `CCLocalLevels*.dat` from
  `%LOCALAPPDATA%\GeometryDash` into `reode/backups/`. The first backup,
  `first-launch`, is never deleted; of the timestamped ones made at every
  start, the newest `save_backups` (default 5) are kept. A copy is written to
  a `.partial` directory first, so an interrupted copy never looks complete.
- **Safe mode.** `reode/session.lock` exists while the game runs and is
  removed on a normal exit. If it is still there at the next start, the game
  crashed or was killed, and Reode loads no mods for that start
  (`safe_mode_after_crash`). Holding Shift while the game starts does the
  same. The mod list shows a banner saying why.
- **Reasons in the mod list.** For every mod that did not load, the list
  under the R button shows why (failed check, missing dependency, disabled in
  config.json, ...).

## How it works

1. **Bootstrap.** `libcocos2d.dll` imports `XInput1_4.dll`. Reode ships a
   proxy with that name. The proxy forwards every XInput call to the System32
   copy and redirects the game's entry point. Reode then starts on the main
   thread, outside the loader lock, before the game's own `main()` runs.
2. **Finding game code without hard-coded addresses.** Reode reads the
   game's MSVC RTTI to find class vtables (`TypeDescriptor` →
   `CompleteObjectLocator` → vtable). For example, `MenuLayer::init` is
   located like this:
   - find the slot that `cocos2d::CCLayer::init` occupies in
     `libcocos2d.dll`'s exported `CCLayer` vtable (slot 9);
   - read the same slot from `MenuLayer`'s vtable.

   For the current build, this resolves to `GeometryDash.exe+0x4720C0`.
3. **Hooks.** Reode has its own hooking engine (`loader/src/hook`):
   - an x86-64 instruction length decoder that handles legacy, REX, VEX and
     EVEX encodings;
   - trampolines that relocate RIP-relative operands and relative branches;
   - any number of hooks per function, ordered by priority, each of which can
     be enabled, disabled or removed at runtime.
4. **cocos2d.** cocos2d functions are resolved by their decorated export
   names. Virtual methods are called through the vtable slot found in the
   exported `CCNode` vtable, so subclass overrides are respected.
5. **Main menu.** Reode adds a Reode button to `MenuLayer`. Clicking it opens
   an overlay that lists every mod and its state.
6. **Debug console.** A colour console opens at startup. Quick-edit mode is
   off, so selecting text does not freeze the game, and the close button is
   disabled, so closing the console does not kill the game. Logs are also
   written to `reode/logs/`.

## Layout

| Path | Contents |
|---|---|
| `proxy/` | `XInput1_4.dll` proxy and entry-point bootstrap |
| `loader/include/reode/` | **SDK**: `api.h` (stable C ABI), `reode.h` (C++23 wrapper) |
| `loader/src/core/` | console, logger, config, paths |
| `loader/src/hook/` | x64 decoder, near allocator, thread freezer, hook manager |
| `loader/src/game/` | RTTI resolver, pattern scanner, cocos2d bindings, menu button |
| `loader/src/mods/` | `mod.json`, versions/dependencies, packages, loading, API table |
| `loader/src/geode_compat/` | Geode mod support: import analysis, shim loading |
| `geode_shim/` | Reode's own `Geode.dll` (Geode ABI implemented from scratch) |
| `installer/` | `ReodeSetup.exe`: one-click install, update and uninstall |
| `mods/` | Reode's own mods: Fast Transitions, Screenshot, Clock |
| `examples/hello_mod/` | example Reode mod |
| `templates/mod/` | project template for mod authors (shipped in `reode-sdk.zip`) |
| `docs/` | [FAQ](docs/FAQ.md), [first mod in 15 minutes](docs/first-mod.md), website (`docs/site/`) |
| `.github/workflows/` | CI: build, test, package, release on `v*` tags |
| `tests/` | GoogleTest unit tests |

## Building

Requirements: Visual Studio 2026 (MSVC v145) and CMake 3.28 or newer.
Dependencies (nlohmann/json, miniz, GoogleTest) are downloaded automatically.

```bash
cmake --preset msvc
```

```bash
cmake --build --preset debug
```

```bash
ctest --preset debug
```

Install into the game. Any existing `XInput1_4.dll`, such as Geode's, is
backed up first:

```bash
powershell -File tools/install.ps1 -GameDir "C:\Program Files (x86)\Steam\steamapps\common\Geometry Dash" -BuildDir build/loader/Debug
```

To restore the previous state, run the same script with `-Uninstall`.

`ReodeSetup.exe` is built to `build/installer/<config>/`. It embeds the
DLLs of the same configuration, so build `RelWithDebInfo` or `Release` for
players: a Debug installer needs the Visual Studio debug runtime.

## Game folder layout

```
Geometry Dash/
  XInput1_4.dll          Reode proxy
  Reode.dll              the loader
  reode/
    config.json          settings (console, log level, game version, ...)
    mods/                *.reode and *.geode packages
    unzipped/  saves/  logs/
    backups/             copies of the game's save files
    session.lock         present while the game runs (safe mode)
    compat/Geode.dll     Geode compatibility layer (optional builds only)
  geode/mods/            existing Geode packages (also scanned)
```

## Writing a mod

```cpp
#include <reode/reode.h>

REODE_MOD_MAIN(mod) {
  mod.Info("Hello from {}", mod.id());
  return {};
}
```

In `CMakeLists.txt`:

```cmake
reode_add_mod(my_mod ID "me.my_mod" MOD_JSON mod.json SOURCES main.cc)
```

This produces `me.my_mod.reode`. Put it in `reode/mods/`. See
`examples/hello_mod` for a mod that hooks `MenuLayer::init`, and
[docs/first-mod.md](docs/first-mod.md) for a step-by-step guide that builds a
mod outside this repository from `reode-sdk.zip`.

### Bundled mods

| Mod | What it does | Settings (`reode/saves/<id>/settings.ini`) |
|---|---|---|
| `reode.fast-transitions` | shortens every scene transition | `DurationPercent` (40) |
| `reode.screenshot` | F12 saves a PNG to Pictures\Geometry Dash and copies it | `Key`, `CopyToClipboard` |
| `reode.clock` | time of day and play time in the bottom-right corner; F9 toggles | `ShowClock`, `ShowPlayTime`, `SizePercent`, `VisibleAtStart`, `ToggleKey` |

They call cocos2d only through its exports (`mods/common/mod_util.h`) and
need no game addresses. One trap: exports that override a function of a
second base class (`CCLabelProtocol::setString(const char*)`,
`CCRGBAProtocol::setOpacity`) expect `this` to point at that base class and
crash when given the object pointer.

Mods only use the `ReodeApi` function table, which is versioned and only ever
extended at the end. Mods do not link against `Reode.dll`, so a mod built
against an older SDK keeps working with newer versions of Reode.

## Geode compatibility

**Not included in release builds.** `Geode.dll` is compiled against C++
headers generated from [geode-sdk/bindings](https://github.com/geode-sdk/bindings),
which states no license, so it is built only when you ask for it:

```bash
cmake --preset default -DREODE_GEODE_COMPAT=ON
```

Without it, Geode packages are still listed in the mod list, with the reason
"this build of Reode does not run Geode mods".

With `REODE_GEODE_COMPAT=ON`, Reode runs Geode mods built for **Geode 5.x** (the ABI of SDK 5.11.2)
through its own `Geode.dll`, found in `geode_shim/` and installed to
`reode/compat/`.

How a Geode mod works:

- The mod's game addresses are compiled into the mod itself. Geode's
  bindings are a static library, so `Geode.dll` exports no game classes.
  This means a mod only runs on the game version it was built for, or on any
  version if its `mod.json` says `"gd": {"win": "*"}`.
- The mod imports Geode's runtime (`Mod`, `Loader`, `Hook`, logging, events
  and settings) from `Geode.dll` by decorated C++ name.
- Event ports, settings and `Result` come from header templates that are
  inlined into the mod. The objects the loader creates must therefore match
  Geode's layout byte for byte.

How Reode handles this:

- **Official SDK headers as a dependency.** `cmake/ReodeGeodeSdk.cmake`
  fetches the Geode SDK v5.11.2 headers and their dependencies, all under
  BSL-1.0, MIT or BSD licences. They are used only for those binary layouts.
  It also builds Geode's bindings code generator, which produces the game
  class headers those SDK headers include. The implementation in
  `geode_shim/src/` is Reode's own code.
- **Exact export list.** SDK classes are compiled as plain classes (see
  `geode_abi_prelude.h`). `Geode.dll` exports exactly the symbols listed in
  `geode_shim/geode.def`.
- **Checked before loading.** A Geode mod is loaded only if it targets
  Geode 5.x and every one of its `Geode.dll` imports is implemented.
  Otherwise the missing functions are listed in the log.
- **Hooks.** A Geode detour calls the original function by calling the
  hooked address again. For every hooked address, `hook_dispatcher.cc`
  generates a handler with unwind data. The handler keeps a per-thread depth
  counter and walks the chain of Geode detours. It also converts the
  `Thiscall`/`Membercall` struct-return convention. The handler is installed
  as the first Reode hook on that address, so native Reode hooks on the
  same function run exactly once.
- **Load sequence.** For each mod, Reode runs
  `BeginMod` → `LoadLibrary` → `geodeImplicitEntry`. It then enables the
  mod's claimed hooks and sends the `DataLoaded` and `Loaded` events.

Tested in game (GD 2.2091):
- `starrydawn72.dark-windows-titlebar` loads and its `CCEGLView::setupWindow`
  hook runs; the window gets the dark title bar.
- `mat.draw-divide` loads.

Not implemented yet:
- the async runtime (`arc`);
- saving setting values and a settings UI;
- setting types other than `bool` and `int`;
- `Hook::createWrapper`, which mods use to call game functions that return
  structs.

### Mods built for 2.2081

Most Geode mods are built for GD 2.2081 and are not loaded on 2.2091. Their
game addresses are compiled in, and 2.2091 also changed cocos2d itself:
`cocos2d::CCNode` gained two virtual functions, `offsetPosition(float,
float)` and `applyScale(float)`, in slots 30 and 31, so every later slot moved
by 2 in `CCNode` and every class derived from it. A 2.2081 mod calling
`node->getParent()` (old slot 61) runs `getChildrenCount()` instead.

Reode used to translate the addresses of such mods (a "backport"). That
feature and its data were removed: the data was derived from the Geode
bindings, which state no license, and the vtable shift made it unsafe for
nearly every mod anyway.

`Geode.dll` itself is compiled against the 2.2081 cocos2d headers, so its
node code (layouts, `NineSlice`) calls the old slots too. Mods that declare
`"gd": "*"` load, but can hit the same problem.

### Mod resources

Reode adds each Geode mod's `resources/` directory to the cocos2d search
paths and loads the spritesheets declared in its `mod.json`. This currently
happens when the main menu is created.

## Game version

The executable has no version resource, so builds are recognised by their PE
timestamp, listed in `loader/src/game/game_version.cc`. The current build,
timestamp `1792203143`, is GD **2.2091**. You can override the detected
version with `"game_version"` in `reode/config.json`.

## License

Reode is released under the [MIT License](LICENSE). Third-party components
and their licenses are listed in [NOTICE.md](NOTICE.md). Release builds
contain no Geode code; the optional Geode compatibility layer is built from
the Geode bindings, which state no license, so do not distribute builds made
with `REODE_GEODE_COMPAT=ON`.
