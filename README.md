# MarioParty123Recomp

Mario Party, Mario Party 2 and Mario Party 3 (N64, US) as native Windows programs, made with
[N64Recomp](https://github.com/N64Recomp/N64Recomp) static recompilation. A menu program
starts all three games, and they share their display, audio and control settings.

| Id | Game | Executable | Supported ROM (US, sha1) |
|---|---|---|---|
| `mp1` | Mario Party | `MarioPartyRecompiled.exe` | `1159bd56730094bfc71be30113e1cfc8bacf34f3` |
| `mp2` | Mario Party 2 | `MarioParty2Recompiled.exe` | `166eda1c05670d337e2c3f15a5db528ae1e5d6e3` |
| `mp3` | Mario Party 3 | `MarioParty3Recompiled.exe` | `6beb80ff822b96bcf85dcdb512e8b2b7969d8259` |

This is an unofficial fan project. It is not affiliated with or endorsed by Nintendo or Hudson Soft.
You need your own ROM of each game you want to play; no game data is included.

## Play

1. Download the latest release zip and unzip it anywhere.
2. Run `MarioParty123Recomp.exe`. It lists the three games over their box art, which it downloads
   from libretro-thumbnails the first time.
3. Press **Play** on a game. The first time, the game's launcher opens instead: choose your ROM
   there and start the game. After that, **Play** goes straight into the game.
4. **Settings** opens a game's launcher at any time, for its ROM, display, audio, controls and mods.

The menu works with the keyboard (arrows, Enter, Esc), the mouse, or a controller (D-pad, A, B). It
hides while a game runs and comes back when the game closes.

Default settings: fullscreen and 4:3 picture. The launcher's "Expand" aspect ratio does not work
correctly yet; see [Known issues](#known-issues).

### Controllers

Up to four players are supported, with no setup. The keyboard and the first controller are Player
1, and each further controller becomes the next player as it connects. Unplugging a controller
frees its port, and the next controller plugged in takes it. To choose a different order, use
**Assign players** on the launcher's Controls page.

## Status: 1.0

All three games boot and run from the menu, with shared settings and up to four players. What
has been checked so far:

- **Mario Party:** boot, intro, title and the Mushroom Village menu, with keyboard/mouse input and
  sound checked by hand. Boards, minigames and saves are not yet checked.
- **Mario Party 2:** boot, the intro cutscene and play by hand, with the frame-sync flicker fixed.
  Boards, minigames and saves are not yet checked systematically.
- **Mario Party 3:** boot through a Party Mode board turn and a minigame (scripted input, muted).
  Audio, manual input, Story and Duel modes and long sessions are not yet checked.

Please report problems on the
[issue tracker](https://github.com/jcoker85/MarioParty123Recomp/issues).

## Known issues

- **The Expand aspect ratio does not work correctly.** The games have no widescreen fixes yet, so
  stretching the picture past 4:3 shows glitches at the screen edges, and Mario Party 1 also gets a
  one-pixel black line along the bottom of the window. Use the default 4:3 aspect ratio.

## Settings and save data

All three games share one data folder: `%LOCALAPPDATA%\MarioParty123Recomp`, or the program's
folder when it contains a file named `portable.txt`.

```
MarioParty123Recomp/
  general.json graphics.json sound.json controls.json   shared by all three games
  mp1/  mp2/  mp3/                                      per game: stored ROM, saves/, mods/,
                                                        mods.json, mod_config/
```

Changing display, audio or controls in one game's launcher changes them for all three. Mods stay
per game, because each game rejects mods made for the others.

When a settings file is missing, it is copied from `default-settings/` next to the programs.
Default files must list every option, because the runtime crashes on a missing graphics key.

## Building

See [BUILDING.md](BUILDING.md). In short: build the three decomps in Docker, recompile them with
N64Recomp, then build everything with CMake and an llvm-mingw toolchain.
`tools/package_release.sh` turns a build into the release zip.

## How it fits together

- `lib/`: N64ModernRuntime, rt64 and RecompFrontend (shared by all three games, built once), and
  the three [mariopartyrd](https://github.com/mariopartyrd) decomps that provide function and
  overlay information.
- `common/`: the entry point, launcher config, input-script playback and display-list dumper
  shared by the games. Each game supplies its identity as `mp::game` (`games/<id>/src/game_info.cpp`).
- `games/<id>/`: the N64Recomp and RSPRecomp configs (`<id>.us.toml`, `aspMain.toml` /
  `n_aspMain.toml`) and the game's own runtime code:
  - `process_scheduler.cpp`: the Hudson HuPrc coroutine scheduler, which switches stacks with
    setjmp/longjmp, reimplemented with one Windows fiber per game process.
  - `overlay_loader.cpp`: tells the runtime which overlays are loaded (MP3 also has a second,
    "mode overlay" table).
  - `unknown_symbols.cpp`: stubs and forwards for 64DD probes and libultra functions.
- `hub/`: the menu program (`hub.c`, ported from TwistedMetal1234Recomp). CMake writes `games.txt`
  beside it, listing the games that were built.
- `icons/`: program icons, original artwork drawn by `tools/make_icons.ps1`.
- `tools/`: decomp build (Docker), code generation, submodule patches, release packaging.
- `patches/`: headers RecompFrontend includes by a path relative to the repository root
  (`ui_funcs.h` and friends); keep them there.

### Framework patches

`tools/apply_submodule_patches.sh` applies these to the pinned submodules:

- `N64ModernRuntime-frame-sync.patch`: signals RSP completion only after rt64 has parsed the
  display list, locks `update_vi()`, and presents the VI state that was just latched. Without it
  the Hudson graphics loop draws passes into the wrong framebuffer (black flashes in MP2).
- `N64ModernRuntime-shared-settings.patch`, `RecompFrontend-shared-settings.patch`: add
  `recomp::register_settings_path()` and load/save the base configs and `controls.json` from it,
  so settings can live outside the per-game config folder.
- `RecompFrontend-auto-players.patch`: assigns players automatically, because the frontend's own
  multiplayer mode starts with nobody assigned and has no prompt at launch. It also keeps the
  keyboard off players 2-4, and frees a port when its controller is unplugged.
- `rt64-mingw-bitscan.patch`: llvm-mingw build fix.

## Credits

- [N64Recomp](https://github.com/N64Recomp/N64Recomp), [N64ModernRuntime](https://github.com/N64Recomp/N64ModernRuntime)
  and [RecompFrontend](https://github.com/N64Recomp/RecompFrontend) by Wiseguy and contributors.
- [RT64](https://github.com/rt64/rt64) by Dario and contributors.
- The [mariopartyrd](https://github.com/mariopartyrd) decompilation projects for Mario Party 1, 2
  and 3.
- [HarvestMoon64Recomp](https://github.com/HarvestMoon64Recomp/HarvestMoon64Recomp) and
  [Zelda64Recomp](https://github.com/Zelda64Recomp/Zelda64Recomp), whose structure this project
  follows.
- Box art in the menu comes from [libretro-thumbnails](https://github.com/libretro-thumbnails/libretro-thumbnails)
  and is downloaded at run time, not distributed.

## License

GPL-3.0 (see [LICENSE](LICENSE)), as required by N64ModernRuntime and RecompFrontend, which every
program here links. Mario Party is a trademark of Nintendo. This repository contains no ROM data,
game assets or recompiled game code.

## AI disclosure

This project was built with Claude (Anthropic) working as a coding assistant
under the direction of the author.

- **Claude did:** wrote most of the code in this repository: each game's
  runtime pieces (the process scheduler, overlay loaders and stubs), the
  framework patches (frame sync and shared settings), the shared entry point
  and settings handling, the menu, the icons, and the build and release
  scripts. It also traced the game code through the decomps to find the
  addresses and functions those pieces replace, ran scripted test sessions,
  and drafted this README.
- **Claude did not:** write N64Recomp, N64ModernRuntime, RecompFrontend, RT64
  or the mariopartyrd decomps, which are other people's projects used here as
  submodules, or the games themselves, whose code the recompiler translates
  from your own ROMs. It did not decide what the project should be or when
  something was right: the author set the goals, played and tested the games
  by hand, and chose what shipped.
