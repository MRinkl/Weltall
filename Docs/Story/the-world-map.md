# The world map

A document that begins `world map E` adds to the world map of era E (0 to 8; the second disc's world is era 8's) as
the world map reads its scene: places the party can enter, their names, arrivals for maps that lead out onto it, dots
on the minimap, and story values at which the party's ship is elsewhere.

Example: `Content/Story/disc2/mass-driver-world-map.txt`, the Mass Driver's ruins on Taura's land.

```
world map 8

vehicle away at story 253

message MassDriver
{
Mass Driver
}

place on foot x 29640 z 16180 width 520 height 400 to map 732 entrance 0 name MassDriver
arrival 40 x 29700 z 16620

dot 21
```

## Places

`place on foot x X z Z width W height H to map M entrance E name N`: a rectangle of the world (whole units, its edges
included) that the party enters with the confirm button, leading to field map M by entrance E. `in gears` or `in
vehicle` instead of `on foot` puts it on the list for those ways of travelling; `on touch` before `to` enters it as
soon as the party stands there, on foot only.

The rectangle must hold ground the party can stand on. A town's or a ruin's own block is not walkable; the game's own
places take in the ground around it, and so should yours. Where two places overlap, the game's own wins.

The name is a message of the document (one line, 16 letters at most), shown as the party stands in the place, or one
of the scene's own names by number (`name #20`).

## Arrivals

`arrival N x X z Z`: where a field map's way out by `ExitToWorldMap #1024 #N` puts the party. Use a number the scene
does not have; put it just outside your place, as the game's own arrivals sit just outside theirs, so that leaving does
not stand the party where pressing the button would enter again.

## Dots on the minimap

`dot N`: the minimap (Select shows and hides it) marks one of its places with its yellow dot wherever the document is
added. The game has 24 places with a dot each, 0 to 23, at spots of their own, and shows each once a map's scripts have
set its bit (`SetVarBit var[1FC] #N`, or `var[1FE] #N-16` from 16). Four are never shown by the game: 14, 19, 21
and 22. Dot 21 lies on the Mass Driver's ruins, so the Mass Driver's document shows it.

Show a dot with this line rather than by setting its bit from a script: a bit set in the game would still be set
where your stretch leads back into the original's story, which must find the game as it left it.

A dot cannot yet be put anywhere else: a place of your own away from the game's dots has none.

## The ship away

`vehicle away at story S` (or `S-T`): while the game's story is at those values, the world map behaves as if the party's
ship were elsewhere. None is drawn or boarded, its dot is not on the map, and the party walks (or rides its gears)
wherever it would otherwise have started aboard. The ship's place and state in the game are left alone, so it is
where it was when the story brings it back.

The errand to the Mass Driver needs it: the first disc's end leaves the party aboard the flying ship by Taura's
forest, and the world map would start Elly and Emeralda in it.

## The walk

The land's own random battles go on as the scene has them for the story's band. Taura's land is one region, whose
battles at that point of the story are the first disc's weakest; a place of your own cannot yet choose others.
