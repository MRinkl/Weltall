# A map of your own

Field maps 730 to 1023 are free. A document can make one of them out of a map of the game: its ground, walls,
scenery, cameras and textures, its walkmesh and the zones on it, its encounters if you want them. The game then
enters it, leaves it and saves in it as it would any map. There are two ways, by how much of the base map's scripts
you keep.

## `from`: the ground of a map, every script yours

```
map 733 from 384
encounters from 629

message Sealed
{
It won't open.
}

actors 19
unsigned 00A-00E 026-02A 030 036
entrances
entrance x -595 z -571 layer 0 camera keep facing keep
entrance x 441 z -590 layer 0 camera keep facing keep

actor 0 script 0:
    ...
```

The text is a whole script section: the actor count, the variables read unsigned, the entrance table, and every
actor's scripts. Its messages are numbered from 0 in their order. `encounters from K` takes map K's random battles
(its formations and their weights) instead of the base map's.

Examples: the Mass Driver's three rooms, `Content/Story/disc2/mass-driver-airlock.txt` (from map 382),
`mass-driver-corridor.txt` (from 384, with map 629's battles) and `mass-driver-control-room.txt` (from 369). Read them
beside the base maps' own listings (`WeltallMapText 382`): each room keeps the base's geometry and puts its own scripts
where the base had its own.

**The actors** are the base map's first actor definitions, by number: a map's archive defines its script actors first
and its scenery after, and a document may have as many as the base has script actors, or fewer (the rest then stand
as scenery, as the definitions say). A definition gives the actor its model, if it has one: the door models of a room
are actors whose scripts scroll the door's texture open. So give a room as many actors as the base needs for the
doors and people you keep, and look at what each did in the base.

**The pattern of a room**, as the Mass Driver's follow it:

- **Actor 0, the map's own init**: `SetHiddenModel`, the base's `CameraSet` (its values fit the room's geometry),
  `DisableTalk`, the room's song (`MusicLoad`), and `SetEncounterRate` if it has random battles (`#900 #2` is the rate
  the Zeboim facility's halls use).
- **Actors 1 to 11, the party**: actor N + 1 is character N (`SetPartyCharacter #N`, `SetMessageFace #N`); their
  `script 1` is `PlayerControlStep`, so that whoever is in the party walks and follows. A scene that wants the party
  still sets a scratch variable the party's `script 1` checks (`mass-driver-control-room.txt`: `If Scene == #0 else
  Held`).
- **Doors**: an invisible actor with a box at the door (`SetHiddenModel`, `SetPosition`, `SetStateFlags 0 0x2000`,
  `SetStateFlags 3 0x4`, `SetCollisionSize`, `SetCollisionFlags #1`), whose `script 2` (the confirm button at the box)
  or `script 3` (walking into it) plays the door's sound, starts the door model's opening (`ScrollModelUV 2 0`, a step
  a tick for fourteen ticks), walks the leader through (`WalkLeaderTo`) and changes the map (`ChangeMap`,
  `ExitToWorldMap`). Take the boxes' places from the base's own door actors; a door's box stops anyone a script walks
  too, so whoever comes in by a door should start inside it.
- **A base actor you do not want** (a guard of the original's story): `RemoveActor self` in its init.
- **Every actor's scripts 0 to 3 given**, an `End` for those it does not use. A script the text leaves out starts at
  the bytecode's first byte, which is the entrance table's mark, not an instruction.

Entering such a map: a `ChangeMap #733 #0` in any map's script (yours or a patched one of the game's), a place on the
world map ([The world map](the-world-map.md)), or a save made there.

## `over`: a map's own scripts, with yours added

```
map 730 over 613
encounters from 273

message Hurry
{
...
}

detour @0x00FD to Rebuilt SetGearModel #0
Rebuilt:
    SetGearModel #42
    Jump @0x0100

actor 8 script 1:
    ...
```

The map keeps the base map's scripts and messages whole, and the document is a patch of them ([Changing a
map](changing-a-map.md)): messages after the base's, `expect` and `detour` on its instructions, scripts given anew.
Use it where a scene wants the base map's staging: its actors' inits, its cameras, its effects, its people.

Examples: `Content/Story/disc2/rematch-lead-in.txt` and `rematch-aftermath.txt`, two scenes in the sky of the first
disc's last chase (map 613), with the base's cockpits and Gears and cameras, and their own words.

A map over its base runs everything the base would at the story's values you enter it at. Read the base's conditions
and keep what would be wrong (a scene of the original's story, a pose a Gear does not have) from starting: give that
actor's script anew, or never start it.

## Disc 1 and disc 2

The game powers on with the first disc and reads every file from it; the second disc's own copies serve its movies.
So any map of the first disc can be the base of a map played on the second.
