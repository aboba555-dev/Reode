# Your first Reode mod in 15 minutes

This guide takes you from nothing to a mod that runs inside Geometry Dash.
You need Windows, Geometry Dash on Steam and Reode installed.

## 1. Install the tools (5 minutes)

- [Visual Studio](https://visualstudio.microsoft.com/) with the **Desktop
  development with C++** workload. The free Community edition is enough.
- [CMake](https://cmake.org/download/) 3.28 or newer (Visual Studio ships one).

## 2. Get the SDK and the template (1 minute)

From the [latest Reode release](../../releases/latest), download
`reode-sdk.zip` and unpack it, for example to `C:\reode-sdk`. It contains:

| Folder | What it is |
|---|---|
| `include/reode/` | the SDK headers: `reode.h` (C++) and `api.h` (C ABI) |
| `cmake/` | `ReodeMod.cmake`, which builds and packs a mod |
| `template/` | a ready-to-build mod project |

Copy `template/` somewhere and rename it after your mod.

## 3. Name your mod (1 minute)

Edit `mod.json`:

```json
{
  "reode": "1",
  "id": "yourname.my-mod",
  "name": "My Mod",
  "version": "1.0.0",
  "developers": ["Your Name"],
  "description": "What the mod does, in one sentence."
}
```

The `id` is lowercase letters, digits, `.`, `_` and `-`, and must be unique.
Use the same id in `CMakeLists.txt` (`ID "yourname.my-mod"`).

## 4. Build it (3 minutes)

Open a terminal in the project folder:

```bash
cmake -S . -B build -A x64 -DREODE_SDK_DIR=C:/reode-sdk
cmake --build build --config Release
```

The mod is now `build/Release/yourname.my-mod.reode`.

## 5. Run it (2 minutes)

Copy the `.reode` file into `reode\mods` in the Geometry Dash folder and
start the game. The debug console shows:

```
[INFO ] [yourname.my-mod] yourname.my-mod 1.0.0 is ready
[INFO ] [yourname.my-mod] The main menu is open
```

Click the **R** button on the left of the main menu: your mod is in the list
as *loaded*. If something goes wrong, the list and `reode\logs` say why.

## 6. Understand what it does (3 minutes)

`src/main.cc` hooks `MenuLayer::init`, the function that builds the main
menu:

- `reode::game::FindVirtual("MenuLayer", 9)` finds the function through the
  game's own type information, so the mod needs no hard-coded address.
- `mod.Hook(target, &Detour, &original)` makes the game call `Detour`
  instead. `original` calls the real function, or the next mod's hook.
- `REODE_MOD_MAIN(mod)` is the entry point. Returning an error tells Reode
  that the mod could not start; the reason appears in the mod list.

## Where to go next

- `reode.h` documents the whole C++ API: hooks, logging, game lookup
  (`FindVTable`, `FindVirtual`, `FindPattern`) and a per-mod save folder.
- Reode's own mods in [`mods/`](../mods) show real techniques: calling
  cocos2d through its exports (`mods/common/mod_util.h`), drawing an overlay
  on every screen (`mods/clock`), reading the screen (`mods/screenshot`) and
  settings in an `.ini` file.
- Ask questions in the Reode Discord, channel `#mod-dev`.

### A trap worth knowing

Some cocos2d exports are overrides of functions from a *second* base class,
such as `CCLabelProtocol::setString(const char*)` or
`CCRGBAProtocol::setOpacity`. They expect `this` to point at that base
class, not at the object, and crash when called with the object pointer.
Prefer functions the class declares itself, or overrides of `CCNode`
functions. `mods/clock` uses `CCLabelBMFont::setString(const char*, bool)`
for this reason.
