# Changing a map

A `map N` document patches field map N when it loads. You can add messages and code, replace an
actor's script, or redirect an existing instruction to new code. Additions go after the original
bytecode, so existing instruction offsets stay valid.

Examples:

- `Content/Story/additions/rps-helper.txt` adds a choice and three messages to a Lahan house, controlled
  by the RPS helper setting.
- `Content/Story/disc2/mass-driver-way-out.txt` changes a story-progress check to open an exit earlier.
- `Content/Story/disc2/mass-driver-setting-out.txt` replaces a scene at one story value, changes the
  party, removes characters from the room and adds dialogue.
- `Content/Story/disc2/rematch-parting.txt` branches from the original story into the expansion.

## Document structure

```
map 614
option fix.rps_helper            (apply only when this setting is on; omit to apply unconditionally)

message Hello                    a new message, referenced by name in scripts
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

actor 16 script 13:              replace this script slot
    SetPosition #-150 #-172
    End
```

- **`expect @offset instruction`** checks that the original instruction matches the listing from
  `WeltallMapText`. If the bytes differ, the document is rejected and the tests fail.
- **`detour @offset to Label instruction`** performs the same check, then replaces the instruction's
  first three bytes with a jump to `Label`. The instruction must be at least three bytes long. When
  returning to the original script, repeat the replaced instruction if needed, then jump to the next
  instruction. Here, `IfAccGreater #299 @0x0624` occupies five bytes at 0614, so execution resumes at 0619.
- **Messages** are appended after the original messages and referenced by the names you assign. Use
  `msg0`, `msg1`, etc. to reference existing messages, as in the listing.
- **Message windows** fit the longest line, up to 35 letters without a portrait or 26 with one.
  Portraits are selected by `SetMessageFace`, including for party members. Each page has four lines,
  counting the name line on the first page. Longer lines run off the screen; split the text across
  lines or pages (`{page}`). `Tests/WeltallCore/Story/StoryMessageWindowsTests.cpp` checks every added
  message in its actual window.
- **`actor A script S:`** replaces that script slot. Each actor has 32 slots, and unused slots are
  available for new scripts started with `StartScript` or related instructions.
- Labels, `define` and comments (`;`) use the standard script syntax.

Patches apply whenever their map loads with the pack active. Use conditions to limit changes to the
right story progress (`var[000]`), entrance (`var[002]`) or flag.

`mass-driver-setting-out.txt` shows four ways to do this in one room:

- Change the party during map initialization only at story value 252 and entrance 1.
- Remove characters by reusing the original code for a later story event.
- Keep a scene trigger frozen when its scene shouldn't run.
- Replace a scene's first instruction while preserving the original path at all other story values.

## Finding the script to change

Print the map and read the conditions around the behavior you want to change. A scene usually has an
actor whose init (`script 0`) freezes it unless the entrance or story value matches, with the scene
itself in `script 1`. Talking to an actor or pressing confirm at an invisible trigger runs `script 2`.
Walking into the trigger runs `script 3`.

## Making a patch optional

`option NAME` applies the document only when that player setting is enabled. Supported settings
include `fix.rps_helper` and `fix.wide_view_staging`. Adding another requires a corresponding option
and support in `FStoryPacks`.
