# Reode FAQ

## What is Reode?

A mod manager for Geometry Dash 2.2091 on Windows (Steam). It loads mods,
adds a Reode button to the main menu, and comes with its own SDK for mod
authors. It is written from scratch: it is not a fork of Geode and is not
made by the Geode team or RobTop Games.

## How do I install it?

Download `ReodeSetup.exe` from the
[latest release](../../releases/latest) and run it. It finds Geometry Dash
in your Steam library by itself; if it cannot, it asks for the folder that
contains `GeometryDash.exe`. Close the game before installing.

## How do I uninstall it?

Run `ReodeSetup.exe` again and choose **Uninstall Reode**. The game goes back
to how it was. You can keep or delete your mods and settings; the save
backups in `reode\backups` are always kept.

Without the installer: delete `XInput1_4.dll` and `Reode.dll` from the game
folder. If `XInput1_4.dll.reode-backup` exists, rename it back to
`XInput1_4.dll`.

## Is my progress safe?

Reode protects it in three ways:

- **Backups.** The first time the game starts with Reode, your save files
  (`CCGameManager*.dat`, `CCLocalLevels*.dat`) are copied to
  `reode\backups\first-launch`. That copy is never deleted. Every later start
  makes another backup; the newest 5 are kept (`save_backups` in
  `reode\config.json`).
- **Safe mode.** If the game crashed or was killed, the next start loads no
  mods. Restart once more to load them again. You can also hold **Shift**
  while the game starts.
- **Risky mods stay off.** Mods that could misbehave, such as Geode mods
  built for an older game version, are not loaded, and the mod list says why.

To restore a backup, close the game and copy the files from a folder in
`reode\backups` to `%LOCALAPPDATA%\GeometryDash`.

## My antivirus complains about XInput1_4.dll. Why?

Reode starts through a small file named `XInput1_4.dll` in the game folder.
Windows loads it in place of the system controller library, and it forwards
every controller call to the real one. This "DLL proxy" technique is common
in game mods, but it is also used by malware, so some antivirus programs flag
it by its shape alone.

What you can check:

- Reode is open source. Every release is built from the public code by
  GitHub Actions, and the release page links the build.
- Each release links its VirusTotal report.
- Download Reode only from its GitHub releases page.

## Can Reode run Geode mods?

No. Reode releases run Reode mods only. Reode has an experimental Geode
compatibility layer, but it is built from Geode's game bindings, which have
no license, so it is not part of any release. Developers can build it from
source; see the README.

It would not help much today anyway: most Geode mods are built for Geometry
Dash 2.2081, and version 2.2091 changed the game and its cocos2d library in
ways that make those mods call the wrong functions.

## How is Reode different from Geode?

Geode is the established mod loader with a large mod index. Reode is an
independent project that runs on 2.2091 today and has its own SDK. The two
are not compatible in general, and Reode does not use Geode's code, name or
artwork.

## Where are the logs?

In `reode\logs` in the game folder, one file per start. Attach the newest
one when you report a bug. Set `"show_console": true` in
`reode\config.json` to see the log live in a console window.

## How do I make a mod?

See [Your first Reode mod in 15 minutes](first-mod.md).
