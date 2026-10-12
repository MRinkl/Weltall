# A stretch of story

The `disc2` pack turns two narrated sections into playable sequences: the rematch with Ramsus above
the forest, and Elly's trip to the Mass Driver. Each branches from the original story, adds scenes
and maps, then rejoins it with the same game state. Here's how that works.

## Enabling the pack

The `disc2` pack is active only when the player chooses "Extended" at the start of Disc 2. Otherwise,
its patches, maps and world map locations aren't used. Release packages leave the pack out; build
with `--expansion` to include it (see Packaging in the main README).

## Story progress

Event variable 0 (`var[000]`) tracks story progress. Original scripts set it as the story advances,
and maps use it to decide which events to run. Some values are unused: the Mass Driver sequence uses
253, for example. A separate value lets patches distinguish your sequence from the original events
in the same maps, and identifies saves made during it.

## Branching from the original story

Patch the map where your sequence starts so it sends the player to your first map:

- `Content/Story/disc2/rematch-parting.txt` changes the Ministry scene to fade out and enter map 730
  instead of starting the rematch battle. It uses one `detour` at the battle instruction.
- `Content/Story/disc2/rematch-aftermath.txt` returns to the sky after the battle (map 731), then enters
  Taura's house with `ChangeMap #616 #1`, where the original would return to the chair narration.
- `Content/Story/disc2/mass-driver-setting-out.txt` starts the new sequence at Taura's house and sets
  story progress to 253.

## Returning to the original story

At the end, restore the state the original story expects: story progress, flags, map and entrance.

The rematch lead-in starts the original battle with the same formation, battle type and music.

The Mass Driver sequence returns to the original world map launch scene. In the original, the
narration on map 617, entrance 2, sets `SetVarBit var[248] #2` and leaves with
`ExitToWorldMap #1024 #4095 #-1 #17`. After the launch, the game returns to the narration for its
second pass. The new control room sets the same flag, restores story progress to 252 and starts the
same launch scene. This skips the first narration pass because the player has just played those events.

Read the original scripts at the point where you rejoin. Restore the values they set and perform
the transitions in the same order.

`Tests/WeltallCore/Story/StoryContentTests.cpp` verifies this by running the original and expanded
scenes from the same starting state to the point where they rejoin. It compares every saved variable
except those recording field position and elapsed time. Missing or extra flags fail the test.
`Tests/WeltallCore/Story/MassDriverContentTests.cpp` also walks the leader through every door using
normal movement input.

## Save compatibility

A save made during an expansion sequence needs a fallback for playing without the expansion,
including in the original game. `Content/Story/disc2/way-back.txt` maps each new story value to a
compatible point in the original story:

```
way back
story 253 to map 617 entrance 2 story 252
```

A save at story value 253 falls back to map 617, entrance 2, at story value 252: the Mass Driver
narration before the launch. The save also keeps the player's position in the expansion, so loading
it with the expansion enabled resumes there. The same table handles turning the expansion off in Options.

Add a row for every story value your sequence sets. Values without a row are treated as original
story values.

## Related guides

- [Changing a map](changing-a-map.md): branch from the story, open an exit or add a scene to an existing map.
- [A map of your own](a-map-of-your-own.md): reuse a scene's staging or create dungeon rooms.
- [The world map](the-world-map.md): add a destination and travel to it.
- [A battle's narration](a-battles-narration.md): continue narration from where your scene ends.
- [Subtitles](subtitles.md): add a title within an original chapter.
