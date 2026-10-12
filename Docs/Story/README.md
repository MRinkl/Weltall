# Writing for the story

Weltall can add to the original game, and change parts of it, with plain text files the game reads as it starts.
Nothing is compiled: change a file, start the game, and the change is there. This folder explains how, one task at a
time, each pointing at files in `Content/Story` that already do it.

None of these files carries the game's own script or text. A file names what it builds on (a map's number, an
instruction at an offset, a message's number) and holds only what it adds; the game reads the rest from your disc.

## Packs and documents

`Content/Story` holds packs: each folder is a pack, each `.txt` file in it a document. The game reads every pack in
the order of the folders' names, and each pack's documents in the order of their file names. A document that cannot be
read stops the game's start with its name and line on the screen.

| Pack | What it is |
|---|---|
| `additions` | Small changes to the original, each under one of the player's settings (the RPS helper, a staging fix) |
| `disc2` | The expanded second disc: a game plays it only when its player chose "Extended" as the second disc began |

A document's first word says what it is:

| First word | What it does | How-to |
|---|---|---|
| `map 7` | Adds to a map of the game: new code, new messages, an instruction redirected | [Changing a map](changing-a-map.md) |
| `map 732 from 382` | A map of your own, made of a disc map's ground and scenery, every script yours | [A map of your own](a-map-of-your-own.md) |
| `map 730 over 613` | A map of your own that keeps a disc map's scripts and adds to them | [A map of your own](a-map-of-your-own.md) |
| `battle event 19` | Changes where a battle's narration begins and what is told before it | [A battle's narration](a-battles-narration.md) |
| `world map 8` | Places, their names and arrivals on the world map of an era; keeping the party's ship away | [The world map](the-world-map.md) |
| `way back` | Where a save of the expanded second disc puts the original game | [A stretch of story](a-stretch-of-story.md) |
| `subtitles` | Titles for stretches of your own inside the original's chapters | [Subtitles](subtitles.md) |

[A stretch of story](a-stretch-of-story.md) puts these together: leaving the original's story at a point, playing
something of your own, and leading back into it so that the rest of the game goes on as it would have.

## The language

Scripts are written in the same instruction names and operands that `WeltallMapText` prints for the game's own maps.
`Source/WeltallCore/Public/Field/EventAssembly.h` is the reference for the text (labels, `If`, operands, `define`,
`data`), and `Source/WeltallCore/Private/Field/EventInstructionRows*.cpp` list every instruction with its operands.
Messages are written as `Source/WeltallCore/Public/Text/MesText.h` describes: plain characters, line breaks, and marks
in braces such as `{page}`, `{wait 0A}` and `{name 00}`.

## Reading the game's own maps

`WeltallMapText`, built beside the tests (see the main README), prints any field map of your discs as a document of
this kind:

```sh
WeltallMapText 616        # map 616 from the first disc, as text
WeltallMapText 369 369 1  # map 369, numbered 369, from disc 1
```

Its listing shows every instruction with its offset, which is what a patch's `expect` and `detour` lines name, and the
map's messages as `msg0`, `msg1` ... as its scripts name them. What it prints is the game's own script and text: read
it, never commit it.

When placing people and doors, the listing's own numbers are the guide: the entrances' places, and where the map's
scripts put their actors (`SetPosition`, `WalkTo`). A point off the floor stops the game with the actor's number.

## Checking your work

The core's tests read every document against your discs:

- every patch fits the map it names, with the game's fixes and without;
- every map of your own is built and entered;
- the expanded second disc's stretches are played from the original's scenes and compared with the original where
  they lead back into it.

Run them as the main README shows; the story's tests carry the tag `[Story]`. A document that no longer fits fails them
by name and line.

To see a scene without playing up to it, the editor's `RenderScreen` commandlet plays the game headless from any map,
with the story at a value of your choosing, and saves pictures as it goes:

```sh
UnrealEditor-Cmd Weltall.uproject -run=RenderScreen -AllowCommandletRendering -RenderOffscreen -unattended
    -Screen=Field -Map=616 -FieldEntrance=1 -FieldStory=252 -StoryMode=Expanded
    -Reader=0.6,0.055 -Seconds=70 -Every=1 -Size=1280x720 -Out=Saved/Renders/shot.png
```

`-Reader` confirms each message as someone reading would, `-Route=x,z;x,z;!x,z` walks the party through points
(pressing the confirm button at those marked `!`, as at a door), `-SoundOut=sound.wav` keeps the sound, and
`-Every=0.016` keeps every frame for a video. Run it without options for the rest.
