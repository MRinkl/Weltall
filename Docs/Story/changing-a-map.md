# Changing a map

A document that begins `map N` adds to field map N's own scripts and messages as the map loads. It can add messages,
add code, give one of an actor's scripts anew, and turn one of the map's instructions into a jump to code of its own.
Everything it adds goes after the map's own bytecode; the map's own instructions stay where they are.

Examples in the checkout:

- `Content/Story/additions/rps-helper.txt`: a choice and three messages added to a house in Lahan, under the player's
  setting for it.
- `Content/Story/disc2/mass-driver-way-out.txt`: one test of the story's progress changed, so that a way out opens at
  a point of the story where the original keeps it shut.
- `Content/Story/disc2/mass-driver-setting-out.txt`: a whole scene played in place of another at one point of the
  story, the party changed, people removed from the room, a person's line when talked to.
- `Content/Story/disc2/rematch-parting.txt`: where the expanded second disc leaves the original's story.

## The parts

```
map 614
option fix.rps_helper            (only under that setting of the player's; leave it out to apply always)

message Hello                    a message after the map's own, named for the scripts
{
Taura
"Hello."
}

expect @0x0619 ExitToWorldMap #1024 #20 #-1 #1
detour @0x0614 to Out IfAccGreater #299 @0x0624
Out:
    If var[000] == #253 else Shut
    Jump @0x0619
Shut:
    IfAccGreater #299 @0x0624
    Jump @0x0619

actor 16 script 13:              one of an actor's scripts given anew
    SetPosition #-150 #-172
    End
```

- **`expect @offset instruction`**: the map's own instruction at that offset, written as `WeltallMapText` prints it. If
  the map's bytes there are something else, the map is not the one the document was written for, and the whole
  document is left out (the tests fail on it).
- **`detour @offset to Label instruction`**: the same, and the instruction's first three bytes become a jump to
  `Label`. The instruction must be three bytes long or more. Your code then does what it wants and, where the map's
  own course should go on, repeats the instruction it replaced and jumps to the offset after it. In the example the
  replaced instruction is `IfAccGreater #299 @0x0624` (five bytes, at 0614); the code after it in the map starts at
  0619.
- **Messages** are numbered after the map's own; a script names them by the name you gave. The map's own messages are
  `msg0`, `msg1` ... as in the listing.
- **A message's window grows with its longest line**, up to 35 letters; beside a portrait (anyone whose scripts set
  `SetMessageFace`, the party's members among them) 26 letters, and four lines a page, the first page's name line
  among them. A wider window runs off the screen's edge, so break a long line over another page (`{page}`).
  `Tests/WeltallCore/Story/StoryMessageWindowsTests.cpp` checks every message of the documents in the window it
  opens in.
- **`actor A script S:`** gives that script slot anew. Slots a map leaves unused (an actor has 32) are free for your
  own scripts, which your code starts with `StartScript` and its relatives.
- Labels, `define` and comments (`;`) work as in any script text.

A patch applies to every load of its map (in a game that plays its pack), so make what you add conditional on what
should trigger it: the story's progress (`var[000]`), the entrance the map was entered by (`var[002]`), a flag.
`mass-driver-setting-out.txt` does that four times over in one room:

- the party changed in the map's init, but only when the story is at 252 and the entrance is 1;
- the people who should no longer be there removed by the map's own code for a later point of the story, which the
  detour reuses;
- a scene's trigger kept frozen at a point where its scene should not play;
- the scene itself started in place of another's first instruction, the other's course kept for every other value.

## Finding the place

Print the map, find the script that does what you want to change, and read its conditions. A scene is usually an
actor whose init (`script 0`) freezes it unless the map was entered by a given entrance or the story is at a given
value, and whose `script 1` is the scene. Talking to someone, or pressing the confirm button at an invisible box (a
door, a panel), runs its `script 2`; walking into it runs its `script 3`.

## Under a setting

`option NAME` adds the document only while the player's setting of that name is on. The settings a document may name
are the ones the game knows for this purpose (`fix.rps_helper`, `fix.wide_view_staging`); a new one needs its place in
the options and in `FStoryPacks`.
