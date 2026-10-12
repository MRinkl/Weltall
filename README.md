# Weltall

Weltall is a native port of *Xenogears* (PlayStation, 1998), rebuilt in Unreal Engine 5.

Weltall contains no game data. It runs only with your own copy of the game: place both disc images in
the `Games` folder and they are detected automatically. It reads the USA release as BIN/CUE, or as CHD
images made from them (the best: the movies keep their sound), or as ISO/IMG images (the movies are
silent); for a PBP, ECM or other image, convert it to BIN/CUE first. Disc 2 can be added while the game
runs, when it asks for it.

**Status:** Weltall plays the whole game, both discs: the field, battles on foot and in Gears, the world map,
the menus, the Battling arena and the movies, every part checked against the original game's own code. So far it
has been played by hand for over ten hours without a single hang, and that playthrough is still going; automated
play covers the rest of the story: every map and every story battle run by machine, and long stretches of the story
played through from save to save. Expect to play it through from start to finish. Should anything go wrong, the game
says so on screen and names the log file to send, and reported issues are being fixed as they come in.

## What Weltall adds

- **Any resolution and widescreen** up to 32:9, the scenery drawn out to the sides where the original showed 4:3.
- **Smooth motion at any refresh rate**, 120 Hz and beyond, while the game itself keeps the original's pace.
- **A free camera** on the field and the world map: turn, tilt and zoom it with the mouse or the right stick.
- **Depth sorting by pixel**, so characters, Gears and scenery no longer cut into or flicker over one another.
- **Sharper or smoother pictures**: filters for the scenery and the sprites, anti-aliasing, and upscaled movies.
- **Gears as 3D models** on the field and the world map, built from their battle models, and the party drawn at
  full size in areas made for Gears.
- **Faster battles and text**: battles at 2x or 4x, text up to instant, with lines that time themselves still
  readable.
- **Chapter titles** on screen as each chapter begins.
- **Your own party order**: anyone can lead the party (see Playing).
- **Always run**, and doors that open as you walk into them.
- **Keyboard and mouse or a controller**, plugged in at any time, every button rebindable, and the game's
  button prompts showing the buttons you have bound.
- **Saves**: load from the menu anywhere, copy in saves from PlayStation memory-card images or emulators, and
  write cards back out.
- **Fixes for the original game's bugs**.

All of it is set in the Options menu, on the title screen or in the game's menu, where the original's way stays a
choice.

Weltall also carries the means to expand the second disc, telling its narrated stretches as play, with a few
scenes written as an example of them; packages leave the example out.

Weltall is not affiliated with or endorsed by the owners of *Xenogears*. All game content and
trademarks belong to their respective owners.

## Playing

You need Windows 10 or 11 (64-bit) and a DirectX 12 graphics card, and the USA release's two disc
images. Unpack the game anywhere, put the disc images in its `Games` folder (subfolders are fine), and
run `Weltall.exe`. Without disc 1 the game says where it looked and what it found; it starts as soon as
the disc is there. The Linux build (64-bit, with a Vulkan graphics driver) starts with `Weltall.sh` the same way.

The title screen's Options set the display, sound, camera, text speed, the fixes to the original's bugs
and the controls; Exit closes the game. In a battle, R1 and L1 step the battle's speed up to 2x and 4x
and back.

To put the party in another order, open the menu and press right from its commands. Pick a member with ×; the
cursor moves on among the others, and × again swaps the two. △ puts the party back in the story's order. The first
member leads when you walk, on the field and on the world map, and the party lines up in your order in the menus and
in ordinary battles. Your order belongs to the members you arranged: when the story has someone join or leave, the
party shows the story's order again, and the same members together again show yours. Member Change keeps it, a
newcomer taking the place of the one it replaces. The story's own scenes and battles keep the story's order, and so
does the field wherever the others are not walking behind the leader: when one of them sits or stands somewhere as
part of the place, or speaks, everyone looks as the story has them.

| PlayStation | Keyboard | Gamepad |
|---|---|---|
| Directions | W A S D, arrow keys | D-pad, left stick |
| × | E, Enter | A |
| ○ | Q, Escape | B |
| □ | F | X |
| △ | Space | Y |
| L1, R1 | Tab, R | LB, RB |
| L2, R2 | 1, 4 | LT, RT |
| Start, Select | P, Backspace | Menu, View |
| Turn the view | Mouse | Right stick |
| Zoom | Mouse wheel | Right stick up and down |
| Reset the view | Middle mouse button | Right stick click |

Every binding can be changed on the Controls page, and the game shows the keys and buttons you have bound
wherever it names a PlayStation button.

Settings and saves are kept in `%LOCALAPPDATA%\Weltall` (on Linux, the settings in `~/.config/Weltall` and the
saves in `~/.local/share/Weltall/Saves`). Saves from a PlayStation memory card image or a
save file (from an emulator, for example) can be copied in on the Options' Saves page, and a card can be
written back out as an `.mcr` image in `Saves\Cards`.

If the game stops, the screen says why and names its log file. Please include that file when you report
it, with a few words on what you were doing. If it closes instead with a message that it has crashed,
the newest folder in `%LOCALAPPDATA%\Weltall\Saved\Crashes` (on Linux, `~/.config/Epic/Weltall/Saved/Crashes`)
holds a copy of that log with a dump of the moment it crashed: please include the whole folder, zipped.

## Development

The engine core (`Source/WeltallCore`) has no Unreal dependencies and builds on its own for its
tests. With CMake 3.25+ and a C++20 compiler:

```sh
cd Tests
cmake --preset windows        # or: linux, linux-clang
cmake --build --preset windows
ctest --preset windows
```

Tests that read game data look for your disc images in `Games/` and report themselves as skipped when
an image is missing. The oracle tests compare the port with the original game's own code: each holds digests of what
that code leaves, run in an emulated CPU on cases drawn from the same generator as the test's.

What Weltall adds to the original is text in `Content/Story`, read by the game as it starts: each folder a pack,
each `.txt` file in it a document: a patch of a field map's scripts and messages, a map of its own made of a disc map's
parts, a change to a battle's narration, places on the world map, subtitles, written in the same instruction names
and message marks the original's own are read in. A document names what it builds on and holds only what it adds. The
`disc2` pack is an example of that machinery: two stretches of the second disc told as play (the rematch above the
forest, and Elly's errand to the Mass Driver with a short dungeon), each leading back into the original's story where
it ends; a game plays it only when its player chose it as the second disc began. [Docs/Story](Docs/Story/README.md)
explains how to write such documents, each task with the files that do it as its example. `WeltallMapText`, built
beside the tests, prints any map of your discs in that form (`WeltallMapText 613`), to read what a patch is written
against; what it prints is the game's own script and text, never to be committed. The tests put every document on its
map or its battle.

### Packaging

`Scripts/package.py` builds standalone packages from a commit, with an Unreal Engine 5.8 source
build named by `UE_ROOT` (in the environment, or a `local.env` file at the repository's root):

```sh
python Scripts/package.py                     # Development
python Scripts/package.py DebugGame Shipping  # any of DebugGame, Development, Shipping
python Scripts/package.py --platform Linux    # for Linux, with the engine's toolchain in LINUX_MULTIARCH_ROOT
```

Each package lands in `out/builds/<Configuration>/<Platform>`. Run `Weltall.exe` (`Weltall.sh` on Linux) there; the game reads the
disc images from the `Games` folder beside it, into which the script links those of the checkout's
`Games/`. Settings and saves live in the player's own folders (`%LOCALAPPDATA%\Weltall` on Windows), shared
by every build.

`python Scripts/package.py --archive Shipping` also writes the package to `out/releases` as a zip archive
for a release, without the disc images, and its debug symbols as a second archive beside it.

`python Scripts/release.py 0.8.0` makes a release of a pushed commit whose `ProjectVersion` is that version: both
Shipping packages archived under the version's name, and a draft release on GitHub with the players' archives and
notes, made with the GitHub CLI. The draft is published by hand from the release page.

A package leaves the `disc2` pack out, so it asks nothing as the second disc begins and plays the
original's. `--expansion` keeps the pack, to try the example; such an archive's name says so.

## License

Weltall's own code is under the MIT License (see `LICENSE`); the third-party code it includes keeps its own license
(`ThirdPartyNotices.md`). The license covers Weltall alone: it grants nothing in *Xenogears* itself, whose game
content and trademarks belong to their owners.

## Support

If you would like to support my open-source work, you can do so through
[GitHub Sponsors](https://github.com/sponsors/Roman215) or [Ko-fi](https://ko-fi.com/roman215).
