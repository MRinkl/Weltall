# A stretch of story

The `disc2` pack tells two of the second disc's narrated stretches as play: the rematch with Ramsus above the forest,
and Elly's errand to the Mass Driver. Each leaves the original's story at one point, plays its own scenes and maps,
and leads back into the original's at another, so that the rest of the game goes on exactly as it would have. This
is how they are put together.

## The pack and the mode

The `disc2` pack is played only in a game whose player chose "Extended" as the second disc began; in any other game
its patches are not applied, its maps do not exist and its world map places are not there. A release leaves the pack
out (the main README's packaging), so a player sees none of it unless they build with `--expansion`.

## The story's progress

The game's progress is event variable 0 (`var[000]`). The original sets a value at each step of the story, and the
maps test it. Many values are never set by the original: the Mass Driver's errand runs at 253, which nothing of the
original uses. Running at a value of your own lets your patches tell your stretch from the original's moments in the
same maps, and lets a save know where it was made.

## Leaving the original

A patch of the map where the story should part: at the moment of parting, its course goes to your first map instead
of the original's next.

- `Content/Story/disc2/rematch-parting.txt`: the Ministry's scene, which in the original starts the rematch's battle,
  fades and changes to map 730 instead (one `detour` on the instruction where the battle begins).
- `Content/Story/disc2/rematch-aftermath.txt`: after the battle, the sky again (map 731), and from there Taura's house
  (`ChangeMap #616 #1`) where the original goes to the chair.
- `Content/Story/disc2/mass-driver-setting-out.txt`: the scene at Taura's house sets the story to 253 as it begins.

## Leading back

Where a stretch ends, the game must stand exactly where the original stands at that point: the same story value, the
same flags, the same map entered the same way. Then everything after is the original's.

- The rematch leads into the original's own battle: the lead-in starts the same formation with the same kind and
  song the original's scene does.
- The Mass Driver's errand leads into the original's scene of the launch on the world map. The original tells the
  errand as a narration (map 617 by entrance 2) that, at its end, sets a flag (`SetVarBit var[248] #2`) and leaves for
  that scene (`ExitToWorldMap #1024 #4095 #-1 #17`); the scene brings the game back to the narration, whose second pass
  tells of what follows. The errand's control room does the same three things (the flag, the story back to 252, the
  scene), so the original goes on from there. The narration's first pass, which tells what the errand has just played,
  is never seen.

Read the original's own scripts where you rejoin: what does it set just before, and how does it go on? Do the same,
in the same order.

**Prove it with a test.** `Tests/WeltallCore/Story/StoryContentTests.cpp` plays the original's scenes and the expanded
game's from the same point, each as far as the place they meet, and compares every saved variable of the two games,
apart from those that only say where the field stood and for how long. A stretch that leaves a flag set, or forgets one
the original sets, fails it. The walks in between are tested by walking them
(`Tests/WeltallCore/Story/MassDriverContentTests.cpp` steers the leader by the pad through every door).

## Saves: the way back

A player may save in the middle of your stretch and later load that save in a game that does not have it: the
original's second disc, or the original game itself. `Content/Story/disc2/way-back.txt` says, for each story value
your stretches run at, which point of the original matches what the player has seen:

```
way back
story 253 to map 617 entrance 2 story 252
```

A save made at 253 is, for any game but an expanded one, a save at map 617 by entrance 2 with the story at 252: the
narration of the errand, before the launch. The game's own place is kept beside it in the save, and Weltall, loading it
as an expanded game, goes on where the player was. The same table puts the game there when the player turns the
expansion off from the options.

Give every value your stretch sets a row; a value no row names is taken to be the original's own.

## The pieces

- [Changing a map](changing-a-map.md): parting, opening a way the original keeps shut, a scene in a map of the game.
- [A map of your own](a-map-of-your-own.md): scenes over a map's staging, rooms of a dungeon.
- [The world map](the-world-map.md): a place to walk to, and the walk itself.
- [A battle's narration](a-battles-narration.md): a battle told from where your scene left off.
- [Subtitles](subtitles.md): a title for your stretch inside the original's chapter.
