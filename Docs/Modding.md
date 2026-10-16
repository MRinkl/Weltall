# Modding

Open Options from the title screen or the in-game menu, then press L1 or R1 to reach the Assets page. One
export runs at a time. The page shows its progress, and you can cancel it. Everything you export stays on your
computer. Neither the game nor this repository includes any extracted game content.

## Voices

Put both discs of a supported English undub in `Games` to play with English text and Japanese voices. If
you also have the USA discs there, choose English, Japanese or Auto under **Options → Sound → Voices**. Auto
uses the voices of the version you're playing, which is the USA release when both are present. If the voices
you chose aren't available, you'll hear the ones from the version you're playing. A change applies from the
next battle or movie.

The game recognizes the undub by the SHA-1 hashes of its boot files, not by file names:

| Disc | Boot file | SHA-1 |
|---|---|---|
| 1 | `SLUS_006.64` | `1e21636bc067bd3873b78be3d72978c8038daca0` |
| 2 | `SLUS_006.69` | `2d9a51f9d6fc6855118fc217002edaef30dadec6` |

Japanese retail discs use a different text encoding, different scripts and different movie numbering. In this
version you can dump their files, but they can't supply Japanese text or voices.

## Dumping disc files

**Dump disc assets** copies every file in each recognized disc's index into an `Assets` folder next to
`Games`. That includes textures, sprites, models, animations, sound banks, music sequences, scripts and movies,
all in their original formats. Running it again skips files that are already there, so it never overwrites
your edits. Allow several GB if you have more than one version of the game. To edit textures as pictures, use
the texture export below instead.

Each disc gets its own folder: `usa.disc1`, `usa.disc2`, `usa.undub.disc1`, `usa.undub.disc2`, `jpn.disc1` or
`jpn.disc2`. Each folder has a `manifest.tsv` listing every file's number, disc sector, original size and path.
Keep it with that disc's files.

| Path inside a disc folder | Contents |
|---|---|
| `files/DD/NNNN.bin` | File NNNN in directory DD, padded to a whole sector |
| `records/NNNNNN.bin` | A file from the disc's index that isn't in a directory |
| `movies/NN.str` | Movie NN, with its video, audio and sector headers |

BIN/CUE images, and CHD images made from them, keep the movies' audio. An ISO doesn't contain it, so a dump
from an ISO can't recover it. The dump doesn't convert movies to MP4 or models to Unreal assets.

## Loading replacements

Turn on **External assets** to load your files from `Assets`. Anything you haven't replaced still comes from
the disc, and the discs are still needed either way. Put replacements in the folder for the version you're
playing. Voices come from the folder for the voice version you chose.

A changed file or song takes effect the next time the game loads it. Some data stays loaded the whole time, so
restart the game if a change doesn't show up. If the game can't read a replaced file, it shows its usual error
screen. Turn External assets off to go back to the disc's files.

A replacement for a dumped file can't be empty, its size must be a multiple of 2,048 bytes (a whole number of
sectors), and it can be up to 512 MB. The game reads some formats using the sizes in the disc's index, so a
bigger file doesn't always work. A replacement movie must keep the original's number of sectors and their
layout. These files are mainly useful with tools that already understand the game's formats.

## Music

**Export music WAVs** saves the game's 73 songs from Disc 1 as 44.1 kHz, 16-bit stereo WAV files in
`Assets/music/original`, ready to edit. It runs in the background, shows how many songs are done and can be
cancelled. Running it again keeps the files already there. It's separate from the disc dump because
uncompressed audio can take several GB.

Each WAV is named by its song number and a 16-digit key calculated from the song's data, such as
`song-07-3f2a9c41d0e8b6a1.wav`. A song has the same key everywhere it appears: on either disc, on the undub
and on the world map. To replace a song, copy its WAV from `original` to anywhere else under `Assets/music`,
then edit or replace the copy, keeping the key at the end of the name. Only the name matters. The key
identifies the original song, not your file, so the WAV can be any recording of any length. Turn on External
assets to use it. After editing, turn External assets off and on again to reload.

Replacements can be mono or stereo 16-bit PCM at 8,000–192,000 Hz. To loop part of the file, add a `smpl`
chunk with one forward loop. Without one, the whole file loops for as long as the original song plays. Each
export contains the song's first full loop, or its ending plus a short tail. If a song neither loops nor ends
within ten minutes, its export stops with an error.

The original song still decides when the music changes, its timing and its fades. A recording can't mute a
single instrument, so scenes that do that switch to the game's own synthesized music for the moment. Music
replacements don't affect sound effects or movie audio. When a scene changes a song's tempo or pitch, the
recording is resampled, which changes both together.

## Textures

**Export textures** saves every texture it finds on Disc 1 as a PNG in `Assets/textures/original`. It takes
under a minute, runs in the background and can be cancelled. Running it again keeps the files already there.
It covers:

| Folder inside `original` | Contents |
|---|---|
| `field/NNNN` | Field map NNNN's scenery and the background picture behind it, plus each character's sprite sheet in each palette |
| `sprites/04-NNNN` | Sprites the field loads as it needs them (the party, their Gears and extra animations), with all of a sprite's cells on one sheet per palette |
| `files/DD/NNNN` | Pictures stored whole in file NNNN of directory DD, in their own colors: the world map (`36`), the field's system images and full-screen pictures (`04`), the menus (`16`), the battle results screen (`18`), and some battle (`12`) and Battling arena (`48`) pictures |
| `battle/stages/NNN` | Battle stage NNN's textures, including its background picture and the clouds of its sky dome |
| `battle/enemies/NNN` | The enemies of battle scene NNN: textures for 3D enemies, and a sheet of all the cells for each sprite enemy |
| `battle/party/NN` | Character NN's battle sprite and attack animations, with all the cells on one sheet per palette |
| `battle/gears/GG-V` | Gear GG's battle textures, with variant V's parts or colors (0 for none) |
| `battle/special/N` | The textures of special battle model N |
| `battle/effects/NNN` | Battle effect NNN's sprites, with all the cells on one sheet per palette. `common` holds the effects every battle uses |
| `gears/NN` | The textures of Gear model NN, which field maps and the Gear shop show in 3D |
| `captured` | Textures saved by **Capture textures** while you play |

Each texture is named by a 16-digit key calculated from its pixels and colors, so it has the same name
wherever it appears. A map's folder holds every texture that map draws. A texture used by several maps is in
each of their folders, as a link rather than a copy on drives that support it. `original/index.tsv` lists every
folder each key appears in.

Pictures bigger than a 256 x 256 texture page, such as full-screen pictures and dungeon maps, are saved whole as
one PNG, even though the game draws them in pieces. When Gears are shown in 3D on the field and world map
(Options → Display → Gear sprites), they use their battle models, so their textures are the ones in
`battle/gears`.

For anything the export misses, such as the strips some battle effects cut out of a sprite, turn on **Capture
textures** and play through that part. A texture is saved once its pixels stay the same for a frame, and only if
there's no PNG with its key yet. If the game draws something that isn't a known picture, the whole 256 x 256
texture page is saved. Turn capture off when you're done.

To replace a texture, copy its PNG from `original` to anywhere else under `Assets/textures` (any subfolder
works) and edit the copy. You can add a description before the key, like
`village-door-0d381f34099245c6.png`, as long as the name ends with the key. Only the name matters. The key
identifies the original texture, not your file, so the PNG can be any picture. Don't edit the files in
`original`. They're your reference, and because shared textures are linked, an edit there changes every folder at
once. Turn on External assets to use your replacements. After editing, turn External assets off and on again to
reload them.

Each cell of a sprite is a separate texture. The export collects them on a sheet such as `cells-0.png`, with
`cells-0.tsv` next to it listing each cell's key and position. Copy both files and edit the PNG. The `.tsv` file
has to stay next to it with the same name. If a cell's key also has a PNG of its own, the game uses that PNG
instead of the sheet's cell.

A replacement can be up to 16 times the original's width and height, and it's stretched to cover exactly the
original's area. For example, a 64 x 48 texture can be replaced at 96 x 72, 128 x 96 or 1024 x 768. Keep the
same proportions, or the picture gets squeezed to fit. Whole multiples (2x, 3x, 4x and so on) line up exactly
with the original's pixels. A sheet scales the same way, as a whole. Pixels with an alpha of 128 or more are
drawn, and the rest are transparent. A replacement can draw where the original was transparent, for a smoother
outline, and leave holes where it was solid. If the original blends with what's behind it, the replacement
blends the same way.

Replacements load in the background the first time the game draws them. Small ones appear right away. A large
one may show the original for a moment first, usually while a map fades in. The replacements in one scene can
use up to 512 MB of video memory. Beyond that, the remaining textures keep their original look, and the log says
so. As a guide, replacing every texture of a large map at 4x takes under 100 MB, and at 8x up to about 350 MB.

When the game changes a texture's colors, as in fades, flashes and palette animations, the replacement changes
the same way. A texture drawn with a different palette, such as a character's color variant or the world map's
fog, has its own key. If it has no replacement of its own, it uses the replacement for the original colors and
applies the difference. When a 3D model (a battle stage, an enemy or a Gear) animates its own texture, as with the
fire behind Lahan in the first Gear battle, the replacement stays on but doesn't animate. Textures the game shifts
every frame, such as scrolling water, and pictures copied from the screen keep their original look.

## Scripts and models

You can change field scripts, messages and story behavior with plain text files in `Content/Story`.
[The story guides](Story/README.md) explain the instructions and formats. The settings file lists the game's
options as `key = value` lines, but it can't add new gameplay rules.

Models are only exported in the game's own formats, with their skeletons, animations and textures in the dumped
files. You can't replace a Gear with an Unreal skeletal mesh or import glTF or FBX models.

## Sharing mods

Only share material you have permission to share. Exported textures, music, models, scripts and movies, and edited
versions of them, can still contain copyrighted game content. Weltall doesn't download mod packs or upload your
exports. This repository ignores the `Games` and `Assets` folders, and the release downloads don't include their
contents. If you build packages yourself, rebuilding keeps your existing `Assets` folder.
